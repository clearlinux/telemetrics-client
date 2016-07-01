bin_PROGRAMS = %D%/telemd

%C%_telemd_SOURCES = \
	%D%/main.c \
	%D%/telemdaemon.c \
	%D%/telemdaemon.h \
	%D%/spool.c \
	%D%/spool.h

%C%_telemd_LDADD = $(CURL_LIBS) \
	%D%/libtelem-shared.la

%C%_telemd_CFLAGS = \
	$(AM_CFLAGS)

%C%_telemd_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

if HAVE_SYSTEMD_DAEMON
%C%_telemd_CFLAGS += \
        $(SYSTEMD_DAEMON_CFLAGS)
%C%_telemd_LDADD += \
        $(SYSTEMD_DAEMON_LIBS)
endif

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_telemd_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_telemd_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

# set library version info
SHAREDLIB_CURRENT=3
SHAREDLIB_REVISION=0
SHAREDLIB_AGE=0

noinst_LTLIBRARIES = %D%/libtelem-shared.la

%C%_libtelem_shared_la_SOURCES = \
	%D%/util.c \
	%D%/util.h \
	%D%/configuration.c \
	%D%/configuration.h \
	%D%/common.c \
	%D%/common.h

%C%_libtelem_shared_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

%C%_libtelem_shared_la_LDFLAGS = \
	$(AM_LDFLAGS)

%C%_libtelem_shared_la_LIBADD = \
	$(GLIB_LIBS)

lib_LTLIBRARIES = \
	%D%/libtelemetry.la

%C%_libtelemetry_la_SOURCES = \
	%D%/telemetry.c

include_HEADERS = %D%/telemetry.h

noinst_HEADERS = %D%/log.h

%C%_libtelemetry_la_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)

%C%_libtelemetry_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	$(GLIB_LIBS) \
	-version-info $(SHAREDLIB_CURRENT):$(SHAREDLIB_REVISION):$(SHAREDLIB_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/telemetry.sym

EXTRA_DIST += \
	%D%/telemetry.sym

%C%_libtelemetry_la_LIBADD = \
	%D%/libtelem-shared.la \
	-ldl

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
