#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([telemetrics-client], [1.11.2], [https://clearlinux.org/])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.14 -Wall -Werror -Wno-extra-portability foreign subdir-objects])
AM_SILENT_RULES([yes])
LT_INIT([disable-static])
AC_CONFIG_MACRO_DIRS([build-aux/m4])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_AWK
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET

# Checks for libraries.
PKG_CHECK_MODULES([CHECK], [check >= 0.9.12])
PKG_CHECK_MODULES([CURL], [libcurl])
PKG_CHECK_MODULES([GLIB], [glib-2.0])
AC_CHECK_LIB([elf], [elf_begin], [have_elflib=yes], [AC_MSG_ERROR([Unable to find libelf from elfutils])])
AC_CHECK_LIB([dw], [dwfl_begin], [have_dwlib=yes], [AC_MSG_ERROR([Unable to find libdw from elfutils])])
AS_IF([test "x$have_elflib" = "xyes" -a "x$have_dwlib" = "xyes"],
      [AC_SUBST(ELFUTILS_LIBS, "-lelf -ldw")])

# Special checks for systemd libs
TELEM_CHECK_SYSTEMD([journal],[JOURNAL])
TELEM_CHECK_SYSTEMD([daemon],[DAEMON])
TELEM_CHECK_SYSTEMD([id128],[ID128])

# Checks for header files.
AC_CHECK_HEADERS([curl/curl.h])
AC_CHECK_HEADERS([elfutils/libdwfl.h])
AC_CHECK_HEADERS([fcntl.h])
AC_CHECK_HEADERS([getopt.h])
AC_CHECK_HEADERS([inttypes.h])
AC_CHECK_HEADERS([libelf.h])
AC_CHECK_HEADERS([limits.h])
AC_CHECK_HEADERS([stdarg.h])
AC_CHECK_HEADERS([stdint.h])
AC_CHECK_HEADERS([stdlib.h])
AC_CHECK_HEADERS([string.h])
AC_CHECK_HEADERS([sys/fcntl.h])
AC_CHECK_HEADERS([sys/queue.h])
AC_CHECK_HEADERS([sys/prctl.h])
AC_CHECK_HEADERS([sys/socket.h])
AC_CHECK_HEADERS([sys/types.h])
AC_CHECK_HEADERS([syslog.h])
AC_CHECK_HEADERS([unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([memmove])
AC_CHECK_FUNCS([memset])
AC_CHECK_FUNCS([socket])
AC_CHECK_FUNCS([strdup])
AC_CHECK_FUNCS([strerror])
AC_CHECK_FUNCS([strndup])
AC_CHECK_FUNCS([strtol])
AC_CHECK_FUNCS([uname])

# Options

AC_ARG_WITH([systemdsystemunitdir], AS_HELP_STRING([--with-systemdsystemunitdir=DIR],
	    [path to systemd system service dir @<:@default=/usr/lib/systemd/system@:>@]), [unitpath=${withval}],
	    [unitpath="$($PKG_CONFIG --variable=systemdsystemunitdir systemd)"])
test -z "${unitpath}" && unitpath=/usr/lib/systemd/system
AC_SUBST(SYSTEMD_UNITDIR, [${unitpath}])

AC_ARG_WITH([systemdtmpfilesdir], AS_HELP_STRING([--with-systemdtmpfilesdir=DIR],
	    [path to systemd tmpfiles dir @<:@default=/usr/lib/tmpfiles.d@:>@]), [tmpfilespath=${withval}],
	    [tmpfilespath="$($PKG_CONFIG --variable=tmpfilesdir systemd)"])
test -z "${tmpfilespath}" && tmpfilespath=/usr/lib/tmpfiles.d
AC_SUBST(SYSTEMD_TMPFILESDIR, [${tmpfilespath}])

AC_ARG_WITH([systemdsysctldir], AS_HELP_STRING([--with-systemdsysctldir=DIR],
	    [path to systemd sysctl dir @<:@default=/usr/lib/sysctl.d@:>@]), [sysctlpath=${withval}],
	    [sysctlpath="$($PKG_CONFIG --variable=sysctldir systemd)"])
test -z "${sysctlpath}" && sysctlpath=/usr/lib/sysctl.d
AC_SUBST(SYSTEMD_SYSCTLDIR, [${sysctlpath}])

# systemd.pc gives the /etc conf dir, so we can't use PKG_CONFIG for this one
AC_ARG_WITH([systemdsystemconfdir], AS_HELP_STRING([--with-systemdsystemconfdir=DIR],
	    [path to systemd system conf dir @<:@default=/usr/lib/systemd/system.conf.d@:>@]), [confpath=${withval}],
	    [confpath="/usr/lib/systemd/system.conf.d"])
AC_SUBST(SYSTEMD_SYSTEMCONFDIR, [${confpath}])

AC_ARG_WITH([socketdir], AS_HELP_STRING([--with-socketdir=DIR],
	    [path to dir for telemetrics socket @<:@default=/run@:>@]), [socketpath=${withval}],
	    [socketpath="/run"])
AC_SUBST(SOCKETDIR, [${socketpath}])

AC_ARG_WITH([loglevel], AS_HELP_STRING([--with-loglevel=NUM],
	    [maximum log level for binaries @<:@default=5@:>@]), [loglevel=${withval}],
	    [loglevel=5])
AC_DEFINE_UNQUOTED([MAX_LOG_LEVEL], [${loglevel}], [Maximum log level for binaries])

AC_ARG_ENABLE([logtype], AS_HELP_STRING([--enable-logtype],
              [Vector for logging: stderr (default), syslog, systemd]),
			  [case ${enableval} in
				 systemd) logtype="systemd";;
                 syslog)  logtype="syslog" ;;
                 stderr)  logtype="stderr" ;;
                 *) AC_MSG_ERROR([bad value ${enableval} for --enable-logtype]);;
                 esac],
              [logtype="stderr"])

AM_CONDITIONAL([LOG_STDERR], [test x$logtype = xstderr])
AM_CONDITIONAL([LOG_SYSLOG], [test x$logtype = xsyslog])
AM_CONDITIONAL([LOG_SYSTEMD], [test x$logtype = xsystemd])

case x$logtype in
    xsystemd) AC_DEFINE([TM_LOG_SYSTEMD], [1], [Log through systemd apis]);;
    xsyslog)  AC_DEFINE([TM_LOG_SYSLOG],  [1], [Log through syslog apis]);;
    xstderr)  AC_DEFINE([TM_LOG_STDERR],  [1], [Log through fprintf(stder...)]);;
esac

AC_CONFIG_COMMANDS([config dir],[mkdir -p src/data; touch src/data/.dirstamp])
AC_CONFIG_FILES([Makefile])
AC_REQUIRE_AUX_FILE([tap-driver.sh])
AC_OUTPUT

AC_MSG_NOTICE([

telemetrics-client $VERSION
===========================

prefix:                 $prefix
exec_prefix:            $exec_prefix
libdir:                 $libdir
bindir:                 $bindir
datarootdir:            $datarootdir

compiler:               $CC
cflags:                 $CFLAGS
ldflags:                $LDFLAGS

unitdir:                $unitpath
tmpfilesdir:            $tmpfilespath
sysctldir:              $sysctlpath
systemconfdir:          $confpath
socketdir:              $socketpath
loglevel:               $loglevel
logtype:                $logtype
])
