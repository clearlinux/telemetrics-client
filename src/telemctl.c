/*
 * This program is part of the Clear Linux Project
 *
 * Copyright 2019 Intel Corporation
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <limits.h>
#include <ftw.h>
#include <unistd.h>
#include <locale.h>
#include <pwd.h>
#include <grp.h>

#define TELEM_DIR       "/etc/telemetrics"
#define TM_OPT_OUT      TELEM_DIR"/opt-out"

#define TELEM_WRK_DIRS_CONF "/usr/lib/tmpfiles.d/telemetrics-dirs.conf"

#define cmd_create_tmp_files "systemd-tmpfiles --create " TELEM_WRK_DIRS_CONF
#define cmd_mk_telemdir      "mkdir -p " TELEM_DIR

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

static char *SPECIAL_UNITS[] = {
        "hprobe.timer",
        "telemprobd.socket",
        "telempostd.path",
        "klogscanner.service",
        "journal-probe-tail.service",
        "python-probe.path",
};

#define NUM_SPECIAL_UNITS ARRAY_SIZE(SPECIAL_UNITS)

static char *SERVICES[] = {
        "hprobe.service",
        "pstore-probe.service",
        "telemprobd.service",
        "telempostd.service",
        "journal-probe.service",
};

#define NUM_SERVICES ARRAY_SIZE(SERVICES)

static int telemctl_start(void);
static int telemctl_stop(void);
static int telemctl_restart(void);
static int telemctl_is_active(void);
static int telemctl_opt_out(void);
static int telemctl_opt_in(void);
static int telemctl_journal(char *);

struct telemcmd {
        char *cmd;
        union {
        int  (*f1)(void);
        int  (*f2)(char *);
        } f;
        char *doc;
};

static struct telemcmd commands[] = {
        {"stop",      {.f1=telemctl_stop},     "Stops all running telemetry services" },
        {"start",     {.f1=telemctl_start},    "Starts all telemetry services" },
        {"restart",   {.f1=telemctl_restart},  "Restarts all telemetry services" },
        {"is-active", {.f1=telemctl_is_active},"Checks if telemprobd and telempostd are active" },
        {"opt-in",    {.f1=telemctl_opt_in},   "Opts in to telemetry, and starts telemetry services" },
        {"opt-out",   {.f1=telemctl_opt_out},  "Opts out of telemetry, and stops telemetry services" },
        {"journal",   {.f2=telemctl_journal},  "Prints telemetry journal contents. Use -h argument with\n            command for more options"}
};

static int syscmd(char *cmd, char *buff, int bufflen)
{
        int status;
#ifdef DEBUG
        printf("[debug] [%s] %s\n", __func__, cmd);
#endif
        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
                perror("popen");
                return -1;
        }

        while (fgets(buff, bufflen, fp) != NULL) {
                ;
        }

        status = pclose(fp);
        if (status == -1) {
                perror("pclose");
        }

        return status;
}

/*
Script code:

  for_each_service() {
  local action=$1 && shift
  local -a array=($*)
  for service in "${array[@]}"; do
    systemctl $action $service
    [ $? -ne 0 ] && notice "Failed to $action ${service}. Continuing..."
  done
}
*/
static int for_each_service(const char *command, char* services[], int numservices)
{
        char buff[512];
        char cmd[256];
        int ret = 0, status;

        for (int i = 0; i < numservices; i++) {
                snprintf(cmd, sizeof(cmd), "systemctl %s %s", command, services[i]);
                memset(buff, 0, sizeof(buff));
                status = syscmd(cmd, buff, sizeof(buff));
                if (status != 0 || buff[0] != 0) {
                        fprintf(stderr, "%s", buff);
                        fprintf(stderr, "Failed to %s %s. Continuing...", command, services[i]);
                        ret = 1;
                }
         }

         return ret;
}

/*
Script code:

telem_start() {
  [ -f $OPT_OUT_FILE ] && exit_err "Opt out is enabled. Cannot start services."
  create_work_dirs
  for_each_service "start" ${SPECIAL_UNITS[@]}
}
*/
static int telemctl_start(void)
{
        char buff[512];
        int status, ret;

        if (access(TM_OPT_OUT, F_OK) == 0) {
                fprintf(stderr, "Opt out is enabled. Cannot start services.\n");
                return 1;
        }

        /* Creates dirs if missing, adjust ownership if exists */
        memset(buff, 0, sizeof(buff));

        /* Execute "systemd-tmpfiles --create ${TELEM_WRK_DIRS_CONF}" */
        status = syscmd(cmd_create_tmp_files, buff, sizeof(buff));
        fprintf(stderr, "%s", buff);

        if (status == -1) {
                return 1;
        }

        ret = for_each_service("start", SPECIAL_UNITS, NUM_SPECIAL_UNITS);
        return ret;
}


/*
Script code:

telem_stop() {
  for_each_service "stop" ${SPECIAL_UNITS[@]}
  for_each_service "stop" ${SERVICES[@]}
}
*/
static int telemctl_stop(void)
{
        int ret;

        /* the special units must be stopped first so that activation no longer happens */
        ret =  for_each_service("stop", SPECIAL_UNITS, NUM_SPECIAL_UNITS);
        ret |= for_each_service("stop", SERVICES, NUM_SERVICES);
        return ret;
}

/*
Script code:
*
telem_restart() {
  telem_stop
  telem_start
}
*/
static int telemctl_restart(void)
{
        if (telemctl_stop() == 0) {
                return telemctl_start();
        }
        return 1;
}

/*
Script code:

telem_is_active() {
  echo "telemprobd :" $(systemctl is-active telemprobd.socket)
  echo "telempostd :" $(systemctl is-active telempostd.path)
}
*/

static int telemctl_is_active(void)
{
        char buff[256];

        memset(buff, 0, sizeof(buff));
        if (syscmd("systemctl is-active telemprobd.socket", buff, sizeof(buff)) != -1) {
                printf("telemprobd : %s", buff);
                memset(buff, 0, sizeof(buff));
                if (syscmd("systemctl is-active telempostd.path",buff, sizeof(buff)) != -1) {
                        printf("telempostd : %s", buff);
                        return 0;
                }
        }
        return 1;
}

/*
 * We run as root, so be extra careful when deleting folders/files.
 * We only delete those owned by telemetry:telemetry
 */
static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
        struct stat info;
        if (stat(fpath, &info) == 0) {
                struct passwd *pw = getpwuid(info.st_uid);
                if (pw == NULL) {
                        perror("getpwuid");
                        return -1;
                }
                if (strcmp(pw->pw_name, "telemetry") != 0) {
                        fprintf(stderr, "Not removing \"%s\": Incorrect file owner \"%s\"\n",
                                fpath, pw->pw_name);
                        return -1;
                }
                struct group  *gr = getgrgid(info.st_gid);
                if (gr == NULL) {
                        perror("getgrgid");
                        return -1;
                }
                if (strcmp(gr->gr_name, "telemetry") != 0) {
                        fprintf(stderr, "Not removing \"%s\": Incorrect file group \"%s\"\n",
                        fpath, gr->gr_name);
                        return -1;
                }

                int rv = remove(fpath);
        #ifdef DEBUG
                printf("[debug] [%s] remove fpath:%s rv:%d\n", __func__, fpath, rv);
        #endif
                if (rv) {
                        perror(fpath);
                }

                return rv;
        } else {
                perror("stat");
                return -1;
        }
}

/* rm -rf path */
static int rm_rf(char *path)
{
        return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

/*
 Script code:

 telem_remove_work_dirs() {
  # Remove dirs
  awk '/^d/{print $2}' ${TELEM_WRK_DIRS_CONF} | xargs rm -rf;
}

Notes:
  Parse /usr/lib/tmpfiles.d/telemetrics-dirs.conf
  This file was used to create the folders needed by telemetry.
  Now we want to delete them. The file contents is as:

   d /usr/local/var/lib/telemetry 0755 telemetry telemetry -
   d /usr/local/var/spool/telemetry 0750 telemetry telemetry -
   d /usr/local/var/log/telemetry 0750 telemetry telemetry -
   d /usr/local/var/log/telemetry/records 0750 telemetry telemetry -
   d /usr/local/var/cache/telemetry 0750 telemetry telemetry -
   d /usr/local/var/cache/telemetry/pstore 0750 telemetry telemetry -
   d /var/lib/telemetry/python 01777 telemetry telemetry -
   d /var/tmp/telemetry 01777 telemetry telemetry -
*/
static int telemctl_remove_work_dirs(void)
{
        FILE *fp = fopen(TELEM_WRK_DIRS_CONF, "r");
        if (fp == NULL){
                fprintf(stderr, "Could not open file %s", TELEM_WRK_DIRS_CONF);
                return 1;
        }

        while (true) {
                char folder[PATH_MAX+1];
                int ret = fscanf(fp, "d %s %*s telemetry telemetry - \n", folder);
                if (ret == 1) {
                        /* Delete the folder. We may get an error trying to delete
                         * folders already deleted. We ignore them, so don't bother
                         * checking rmrtf return value */
                        rm_rf(folder);
                }
                if (ret == EOF) {
                        break;
                }
        }

        fclose(fp);
        return 0;
}

static int mk_telem_dir(void)
{
        char buff[512];
        int status;
        struct stat sb;

        if (stat(TELEM_DIR, &sb) == 0 && S_ISDIR(sb.st_mode)) {
#ifdef DEBUG
                printf("[debug] [%s] Folder %s exists\n", __func__, TELEM_DIR);
#endif
                return 0;
        }

        memset(buff, 0, sizeof(buff));

        /* Execute "mkdir -p TM_DIR" */
        status = syscmd(cmd_mk_telemdir, buff, sizeof(buff));
        fprintf(stderr, "%s", buff);

        if (status == -1 || buff[0] != 0) {
                return 1;
        }

        return 0;
}

/*
Script code:

telem_opt_out() {
  [ -f $OPT_OUT_FILE ] && exit_ok "Already opted out. Nothing to do."
  mkdir -p $TELEM_DIR || exit_err "Failed to create ${TELEM_DIR}."
  touch $OPT_OUT_FILE || exit_err "Failed to create ${OPT_OUT_FILE}."
  telem_stop
  telem_remove_work_dirs
}
*/
static int telemctl_opt_out(void)
{
        if (access(TM_OPT_OUT, F_OK) == 0) {
                fprintf(stderr, "Already opted out. Nothing to do.\n");
                return 0;
        } else {
                /* Ensure TELEM_DIR exists */
                if (mk_telem_dir() != 0) {
                        fprintf(stderr, "Failed to create %s\n", TELEM_DIR);
                        return 1;
                }
                /* create TM_OPT_OUT file in TELEM_DIR*/
                if ((creat(TM_OPT_OUT, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH)) < 0) {
                        fprintf(stderr, "Failed to create %s.\n", TM_OPT_OUT);
                        return 1;
                }
                telemctl_stop();
                return telemctl_remove_work_dirs();
        }

        return 1;
}

/*
Script code:

telem_opt_in() {
  [ ! -f $OPT_OUT_FILE ] && exit_ok "Already opted in. Nothing to do."
  rm -f $OPT_OUT_FILE || exit_err "Failed to remove ${OPT_OUT_FILE}."
  telem_start
}
*/
static int telemctl_opt_in(void)
{
        if (access(TM_OPT_OUT, F_OK) != 0) {
                fprintf(stderr, "Already opted in. Nothing to do.\n");
                return 0;
        } else {
                if (unlink(TM_OPT_OUT) == -1) {
                        fprintf(stderr, "Failed to remove %s.\n", TM_OPT_OUT);
                        return 1;
                }
                return telemctl_start();
        }
}


static int journal_cmd(char *cmd)
{
        int status;
        char buff[256];
#ifdef DEBUG
        printf("[debug] [%s] %s\n", __func__, cmd);
#endif
        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
                perror("popen");
                return -1;
        }

        while (fgets(buff, sizeof(buff), fp) != NULL) {
                printf("%s", buff);
        }

        status = pclose(fp);
        if (status == -1) {
                perror("pclose");
        }

        return status;
}

static char* concatargs(int argc, char** argv)
{
        size_t len = strlen("telem_journal") + 1;
        for (int i = 2; i < argc; i++) {
                len += strlen(argv[i]) + 1;
        }

        char *buff = malloc(len*sizeof(char));

        if (buff != NULL) {
                buff[0] = '\0';
                strcat(buff,"telem_journal ");

                for (int i = 2; i < argc; i++) {
                        strcat(buff, argv[i]);
                        strcat(buff, " ");
                }
        }

        return buff;
}

static int telemctl_journal(char * x)
{
        journal_cmd(x);
        return 1;
}

static void print_usage(char *str)
{
        printf("%s - Control actions for telemetry services\n\n", str);

        for (int i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
                printf("  %-9s %s\n", commands[i].cmd, commands[i].doc);
        }
}

int main(int argc, char **argv)
{
        int i, ret = EXIT_SUCCESS;
        bool is_root;

        setlocale(LC_ALL, "");

        if (argc == 1) {
                print_usage(argv[0]);
                exit(2);
        }

        is_root = getuid()?false:true;

        for (i = 0; i < sizeof(commands)/sizeof(commands[0]); i++) {
                if (strcmp(commands[i].cmd, argv[1]) == 0) {
                        if (!is_root) {
                                fprintf(stderr, "Must be root to run this command. Exiting...\n");
                                exit(1);
                        }
                        /* Special treatment for "journal", it can have cmd line arguments */
                        if (strcmp(argv[1],"journal") != 0) {
                                if (argc != 2) {
                                        print_usage(argv[0]);
                                        exit(2);
                                }
                                ret = commands[i].f.f1();
                        } else {
                                char *buff = concatargs(argc, argv);
                                if (buff != NULL) {
                                        ret = commands[i].f.f2(buff);
                                        free(buff);
                                }
                        }

                        break;
                }
        }

        if (i == sizeof(commands)/sizeof(commands[0])) {
                printf("Unknown command passed to %s\n\n", argv[0]);
                print_usage(argv[0]);
                exit(2);
        }

        exit(ret);
}
