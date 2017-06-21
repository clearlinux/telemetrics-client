/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2015 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms and conditions of the GNU Lesser General Public License, as
 * published by the Free Software Foundation; either version 2.1 of the License,
 * or (at your option) any later version.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
 * details.
 */

#define _GNU_SOURCE
#include <assert.h>
#include <fcntl.h>
#include <getopt.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <pwd.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <libelf.h>
#include <elfutils/libdwfl.h>

#include "nica/nc-string.h"
#include "config.h"
#include "log.h"
#include "probe.h"
#include "telemetry.h"

static Dwfl *d_core = NULL;

/*
 * NOTE: if CRC32 validation should be enforced, change the first character to
 * '+' instead, which is the libdwfl default. Since the automatic debuginfo from
 * CLR usually reflects the latest release (within a 10-minute window), we will
 * certainly get crash reports from older releases, so try our best to get the
 * needed symbols or DWARF data from older packages instead of failing due to
 * validation problems...
 */
static char *debuginfo_path = "-/usr/lib/debug";

static unsigned int frame_counter = 0;
static char *proc_name = NULL;
static pid_t core_for_pid = 0;
static nc_string *header = NULL;
static char *errorstr = NULL;

static uint32_t unknown_severity = 2;
static uint32_t default_severity = 3;
static uint32_t error_severity = 4;
static uint32_t version = 1;
static char clr_class[30] = "org.clearlinux/crash/clr";
static char clr_build_class[40] = "org.clearlinux/crash/clr-build";
static char error_class[30] = "org.clearlinux/crash/error";
static char unknown_class[30] = "org.clearlinux/crash/unknown";

static char temp_core[] = "/tmp/corefile-XXXXXX";
static bool keep_core = false;


static const Dwfl_Callbacks cb =
{
        .find_elf = dwfl_build_id_find_elf,
        .find_debuginfo = dwfl_standard_find_debuginfo,
        .debuginfo_path = &debuginfo_path,
};

static inline void tm_elf_err(const char *msg)
{
        telem_log(LOG_ERR, "%s: %s\n", msg, elf_errmsg(-1));
}

static inline void tm_dwfl_err(const char *msg)
{
        telem_log(LOG_ERR, "%s: %s\n", msg, dwfl_errmsg(-1));
}

static char *replace_exclamations(char *str)
{
        char *c;
        while (1) {
                c = strchr(str, '!');
                if (!c) {
                        break;
                }
                *c = '/';
        }
        return str;
}

static void drop_privs(void)
{
        uid_t euid;

        euid = geteuid();
        if (euid != 0) {
                telem_log(LOG_DEBUG, "Not root; skipping privilege drop\n");
                return;
        }

        struct passwd *pw;

        pw = getpwnam("telemetry");
        if (!pw) {
                telem_log(LOG_ERR, "telemetry user not found\n");
                exit(EXIT_FAILURE);
        }

        if (chdir(LOCALSTATEDIR "/lib/telemetry") != 0) {
                telem_perror("Not able to change working directory");
                exit(EXIT_FAILURE);
        }

        // The order is important here:
        // change supplemental groups, our gid, and then our uid
        if (initgroups(pw->pw_name, pw->pw_gid) != 0) {
                telem_perror("Failed to set supplemental group list");
                exit(EXIT_FAILURE);
        }
        if (setgid(pw->pw_gid) != 0) {
                telem_perror("Failed to set GID");
                exit(EXIT_FAILURE);
        }
        if (setuid(pw->pw_uid) != 0) {
                telem_perror("Failed to set UID");
                exit(EXIT_FAILURE);
        }

        assert(getuid() == pw->pw_uid);
        assert(geteuid() == pw->pw_uid);
        assert(getgid() == pw->pw_gid);
        assert(getegid() == pw->pw_gid);
}

static int temp_core_file(void)
{
        int tmp;
        ssize_t ret;

        /* mkstemp() opens the file with O_EXCL and 0600 permissions, so no need
         * to change umask or manipulate the fd to meet those requirements.
         */
        if ((tmp = mkstemp(temp_core)) < 0) {
                telem_perror("Failed to create temp core file");
                return -1;
        }

        while (true) {
                // Use Linux-specific splice(2) here;
                // simplifies copying data from pipe->file
                ret = splice(STDIN_FILENO, NULL, tmp, NULL, INT_MAX,
                             SPLICE_F_MORE | SPLICE_F_MOVE);

                if (ret > 0) {
                        // More data to read
                        continue;
                } else if (ret == 0) {
                        // End of data
                        break;
                } else if (ret < 0) {
                        telem_perror("Failed to splice data to core file");
                        return -1;
                }
        }

        return tmp;
}

/* There are a number of initialization routines required to initialize the Elf
 * and Dwfl objects used by libdwfl to later process a core file. The argument
 * e_core is the address of an Elf pointer declared by the caller, and core_fd
 * is the open file descriptor for the core file.
 */
static int prepare_corefile(Elf **e_core, int core_fd)
{
        // Cleanup previous corefile processing if needed
        if (d_core) {
                dwfl_end(d_core);
        }
        if (*e_core) {
                elf_end(*e_core);
        }

        // Boilerplate initialization for elfutils (libdwfl)
        if (!(*e_core = elf_begin(core_fd, ELF_C_READ, NULL))) {
                tm_elf_err("Failed to get file descriptor for ELF core file");
                goto fail;
        }

        if (!(d_core = dwfl_begin(&cb))) {
                tm_dwfl_err("Failed to start new libdwfl session");
                goto fail;
        }

        int core_modules = 0;
        core_modules = dwfl_core_file_report(d_core, *e_core, NULL);
        if (core_modules == -1) {
                tm_dwfl_err("Failed to report modules for ELF core file");
                goto fail;
        }

        if (dwfl_report_end(d_core, NULL, NULL) != 0) {
                tm_dwfl_err("Failed to finish reporting modules");
                goto fail;
        }

        if ((core_for_pid = dwfl_core_file_attach(d_core, *e_core)) < 0) {
                tm_dwfl_err("Failed to prepare libdwfl session for thread"
                            " iteration");
                goto fail;
        }

        return 0;
fail:
        return -1;
}

/* This callback is invoked for every frame in a thread. From a Dwfl_Frame, we
 * are able to extract the program counter (PC), and from that, the procedure
 * name via a Dwfl_Module.
 */
static int frame_cb(Dwfl_Frame *frame, void *userdata)
{
        Dwarf_Addr pc;
        Dwarf_Addr pc_adjusted;
        Dwfl_Module *module;
        Dwfl_Line *line;
        const char *procname;
        const char *modname;
        bool activation;
        nc_string **bt = (nc_string **)userdata;

        if (!dwfl_frame_pc(frame, &pc, &activation)) {
                int ret;
                ret = asprintf(&errorstr, "Failed to find program counter for"
                               " current frame: %s\n",
                               dwfl_errmsg(-1));
                if (ret < 0) {
                        return DWARF_CB_ABORT;
                }
                return DWARF_CB_ABORT;
        }

        // The return address may be beyond the calling address, putting the
        // current PC in a different context. Subtracting 1 from PC in this
        // case generally puts it back in the same context, thus fixing the
        // virtual unwind for this frame. See the DWARF standard for details.
        if (!activation) {
                pc_adjusted = pc - 1;
        } else {
                pc_adjusted = pc;
        }

        module = dwfl_addrmodule(d_core, pc_adjusted);

        if (!module) {
                errorstr = strdup("Failed to find module for current"
                                  " frame\n");
                return DWARF_CB_ABORT;
        }

        modname = dwfl_module_info(module, NULL, NULL, NULL, NULL, NULL, NULL,
                                   NULL);
        procname = dwfl_module_addrname(module, pc_adjusted);

        line = dwfl_module_getsrc(module, pc_adjusted);

        if (procname && modname) {
                nc_string_append_printf(*bt, "#%u %s() - [%s]",
                                        frame_counter++, procname, modname);
        } else if (modname) {
                nc_string_append_printf(*bt, "#%u ??? - [%s]",
                                        frame_counter++, modname);
        } else {
                // TODO: decide on "no symbol" representation
                nc_string_append_printf(*bt, "#%u (no symbols)",
                                        frame_counter++);
        }

        if (line) {
                const char *src;
                int lineno, linecol;
                src =  dwfl_lineinfo(line, &pc_adjusted, &lineno, &linecol, NULL, NULL);
                if (src) {
                        nc_string_append_printf(*bt, " - %s:%i", src, lineno);
                }
        }
        nc_string_append_printf(*bt, "\n");
        return DWARF_CB_OK;
}

static int thread_cb(Dwfl_Thread *thread, void *userdata)
{
        int ret;
        nc_string **bt = (nc_string **)userdata;
        pid_t tid;

        tid = dwfl_thread_tid(thread);

        nc_string_append_printf(*bt, "\nBacktrace (TID %u):\n",
                                (unsigned int)tid);

        ret = dwfl_thread_getframes(thread, frame_cb, userdata);

        switch (ret) {
                case -1:
                        if (asprintf(&errorstr, "Error while iterating"
                                     " through frames for thread"
                                     " %u: %s\n",
                                     (unsigned int)tid,
                                     dwfl_errmsg(-1)) < 0) {
                                errorstr = NULL;
                        }
                        return DWARF_CB_ABORT;
                case DWARF_CB_ABORT:
                        /* already set the error string in frame_cb */
                        return DWARF_CB_ABORT;
                case DWARF_CB_OK:
                        break;
                default:
                        telem_log(LOG_ERR, "Unrecognized return code\n");
                        return DWARF_CB_ABORT;
        }

        // New threads (if any), will require a fresh frame counter
        frame_counter = 0;

#if 0
        nc_string_append_printf(*bt, "\nRegisters (TID %u):\nTODO\n", current,
                                (unsigned int)tid);
#endif

        return DWARF_CB_OK;
}

static bool send_data(nc_string **backtrace, uint32_t severity, char *class)
{
        struct telem_ref *handle = NULL;
        int ret;

        if ((ret = tm_create_record(&handle, severity, class, version)) < 0) {
                telem_log(LOG_ERR, "Failed to create record: %s",
                          strerror(-ret));
                goto fail;
        }

        if ((ret = tm_set_payload(handle, (char *)(*backtrace)->str)) < 0) {
                telem_log(LOG_ERR, "Failed to set payload: %s", strerror(-ret));
                tm_free_record(handle);
                goto fail;
        }

        if ((ret = tm_send_record(handle)) < 0) {
                telem_log(LOG_ERR, "Failed to send record: %s", strerror(-ret));
                tm_free_record(handle);
                goto fail;
        }

        tm_free_record(handle);
        return true;
fail:
        return false;
}

/* This is the entry point for libdwfl to unwind the backtrace from the core
 * file. All data necessary for processing is referenced by d_core (a *Dwfl),
 * which must be initialized prior to calling this function.  The callback
 * function for processing each core thread is passed as the second argument to
 * dwfl_getthreads(). The backtrace argument is the address of a pointer to a
 * nc_string object declared by the caller which stores the backtrace data as a
 * string.
 */
static int process_corefile(nc_string **backtrace)
{
        if (*backtrace) {
                nc_string_free(*backtrace);
        }
        *backtrace = nc_string_dup("");

        if (dwfl_getthreads(d_core, thread_cb, backtrace) != DWARF_CB_OK) {
                /* When errors occur during the unwinding, we reach this point.
                 * If an error string is set for the particular error, send an
                 * "error" record to capture at least a partial backtrace if
                 * some frames were processed.
                 */
                if (errorstr != NULL) {
                        telem_log(LOG_ERR, "%s", errorstr);
                        nc_string_append_printf(header, "Error: %s", errorstr);
                        nc_string_prepend(*backtrace, header->str);
                        send_data(backtrace, error_severity, error_class);
                }
                goto fail;
        }

        return 0;
fail:
        return -1;
}

static bool in_clr_build(char *fullpath)
{
        // Global override for privacy filters
        if (access(TM_PRIVACY_FILTERS_OVERRIDE, F_OK) == 0) {
                return false;
        }

        /*
         * The build environment for Clear Linux packages is set up by 'mock',
         * and the chroot in which rpmbuild builds the packages has this prefix.
         */
        if ((strstr(fullpath, "/builddir/build/BUILD/")) ||
            (strstr(fullpath, "!builddir!build!BUILD!"))) {
                return true;
        }

        return false;
}

static bool is_banned_path(char *fullpath)
{
        // Global override for privacy filters
        if (access(TM_PRIVACY_FILTERS_OVERRIDE, F_OK) == 0) {
                return false;
        }

        // Anything outside of /usr/, or in /usr/local/, we consider third-party

        if ((strncmp(fullpath, "/usr/", 5) != 0) &&
            (strncmp(fullpath, "!usr!", 5) != 0)) {
                return true;
        }

        if ((strncmp(fullpath, "/usr/local/", 11) == 0) ||
            (strncmp(fullpath, "!usr!local!", 11) == 0)) {
                return true;
        }

        return false;
}

static char *config_file = NULL;
static char *core_file = NULL;
static char *proc_path = NULL;
static long int signal_num = -1;
static bool verbose = false;

static const struct option prog_opts[] = {
        { "help", no_argument, 0, 'h' },
        { "config-file", required_argument, 0, 'f' },
        { "core-file", required_argument, 0, 'c' },
        { "process-name", required_argument, 0, 'p' },
        { "process-path", required_argument, 0, 'E' },
        { "signal", required_argument, 0, 's' },
        { "version", no_argument, 0, 'V' },
        { "verbose", no_argument, 0, 'v' },
        { 0, 0, 0, 0 }
};

static void print_help(void)
{
        printf("Usage:\n");
        printf("  crashprobe [OPTIONS] - collect data from core files\n");
        printf("\n");
        printf("Help Options:\n");
        printf("  -h, --help            Show help options\n");
        printf("\n");
        printf("Application Options:\n");
        printf("  -f, --config-file     Path to configuration file (not implemented yet)\n");
        printf("  -c, --core-file       Path to core file to process\n");
        printf("  -p, --process-name    Name of process for crash report (required)\n");
        printf("  -E, --process-path    Absolute path of crashed process, with ! or / delimiters\n");
        printf("  -s, --signal          Signal number that crashed the process\n");
        printf("  -V, --version         Print the program version\n");
        printf("  -v, --verbose         Print the crash payload to stdout\n");
        printf("\n");
}

int main(int argc, char **argv)
{
        Elf *e_core = NULL;
        int ret = EXIT_FAILURE;
        int core_fd = STDIN_FILENO;
        nc_string *backtrace = NULL;

        if (fcntl(STDERR_FILENO, F_GETFL) < 0) {
                // redirect stderr to avoid bad things to happen with
                // fd 2 when launched from kernel core pattern
                if (freopen("/dev/null", "w", stderr) == NULL) {
                        exit(EXIT_FAILURE);
                }
        }

        // Since this program handles core files, make sure it does not produce
        // core files itself, or else it will be invoked endlessly...
        prctl(PR_SET_DUMPABLE, 0);

        drop_privs();

        int opt;

        while ((opt = getopt_long(argc, argv, "hf:c:p:E:s:Vv", prog_opts, NULL)) != -1) {
                switch (opt) {
                        case 'h':
                                print_help();
                                goto success;
                        case 'V':
                                printf(PACKAGE_VERSION "\n");
                                goto success;
                        case 'f':
                                config_file = strdup(optarg);
                                break;
                        case 'c':
                                core_file = strdup(optarg);
                                break;
                        case 'p':
                                proc_name = strdup(optarg);
                                break;
                        case 'E':
                                proc_path = strdup(optarg);
                                break;
                        case 's':
                                errno = 0;
                                char *endptr = NULL;

                                signal_num = strtol(optarg, &endptr, 10);

                                if (errno != 0) {
                                        telem_perror("Failed to convert signal number");
                                        goto fail;
                                }
                                if (endptr && *endptr != '\0') {
                                        telem_log(LOG_ERR, "Invalid signal number. Must be an integer\n");
                                        goto fail;
                                }
                                break;
                        case 'v':
                                verbose = true;
                                break;
                }
        }

        if (!proc_name) {
                printf("Missing required -p option. See --help output\n");
                exit(EXIT_FAILURE);
        }

        if (core_file) {
                core_fd = open(core_file, O_RDONLY);
                if (core_fd == -1) {
                        telem_perror("Failed to open input core file");
                        goto fail;
                }
        } else {
                struct stat sb;

                if (fstat(STDIN_FILENO, &sb) < 0) {
                        telem_perror("Failed to stat stdin");
                        goto fail;
                }

                /* Support core files on the filesystem, or over a pipe */
                if (S_ISREG(sb.st_mode)) {
                        // Can read STDIN_FILENO directly, so fall through
                } else if (S_ISFIFO(sb.st_mode)) {
                        // elf_begin() requires a seekable file, so dump core contents
                        // to a temporary file and use it instead of stdin.
                        core_fd = temp_core_file();

                        if (core_fd == -1) {
                                goto fail;
                        }
                } else {
                        printf("Cannot process core file. Use the -c option,"
                               " or pass the core file on stdin.\n");
                        goto fail;
                }
        }

        if (proc_path && in_clr_build(proc_path)) {
                telem_log(LOG_NOTICE, "Ignoring core (from mock build)\n");

                backtrace = nc_string_dup("Crash from Clear package build\n");

                keep_core = true;

                if (!send_data(&backtrace, unknown_severity, clr_build_class)) {
                        goto fail;
                }
                goto success;
        }

        if (proc_path && is_banned_path(proc_path)) {
                telem_log(LOG_NOTICE, "Ignoring core (third-party binary)\n");

                backtrace = nc_string_dup("Crash from third party\n");

                keep_core = true;

                if (!send_data(&backtrace, unknown_severity, unknown_class)) {
                        goto fail;
                }
                goto success;
        }

        elf_version(EV_CURRENT);

        if (prepare_corefile(&e_core, core_fd) < 0) {
                goto fail;
        }

        header = nc_string_dup_printf("Process: %s\nPID: %u\n",
                                      proc_path ? replace_exclamations(proc_path) : proc_name,
                                      (unsigned int)core_for_pid);

        if (signal_num >= 0) {
                nc_string_append_printf(header, "Signal: %ld\n", signal_num);
        }

        if (process_corefile(&backtrace) < 0) {
                goto fail;
        }

        /* On Clear Linux OS, missing symbols may appear if automatic debuginfo
         * downloads are still in flight. So if any missing symbols appear on
         * the first run (indicated by the presence of "??? - ["), wait 10
         * seconds and try again.
         */
        if (strstr(backtrace->str, "??? - [")) {
                sleep(10);

                if (prepare_corefile(&e_core, core_fd) < 0) {
                        goto fail;
                }

                if (process_corefile(&backtrace) < 0) {
                        goto fail;
                }
        }

        nc_string_prepend(backtrace, header->str);

        if (!send_data(&backtrace, default_severity, clr_class)) {
                goto fail;
        }

success:
        if (verbose) {
                if (backtrace != NULL) {
                        printf("%s\n", (char *)backtrace->str);
                }
        }

        ret = EXIT_SUCCESS;
fail:
        // Do not remove the core file if any errors occur
        if (ret == EXIT_FAILURE) {
                keep_core = true;
        }

        free(core_file);
        free(proc_name);
        free(proc_path);

        if (header) {
                nc_string_free(header);
        }

        if (backtrace) {
                nc_string_free(backtrace);
        }

        if (errorstr) {
                free(errorstr);
        }

        if (d_core) {
                dwfl_end(d_core);
        }

        if (e_core) {
                elf_end(e_core);
        }

        if (core_fd >= 0 && core_fd != STDIN_FILENO) {
                close(core_fd);
        }

        // Remove the core file by default, except when the --core-file option
        // is specified, or when keep_core is overridden to true.
        if (!core_file && !keep_core) {
                unlink(temp_core);
        }

        return ret;
}

/* vi: set ts=8 sw=8 sts=4 et tw=80 cino=(0: */
