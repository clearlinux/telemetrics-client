bin_PROGRAMS = \
	%D%/telemprobd \
	%D%/telempostd \
	%D%/telemctl

%C%_telemctl_SOURCES = \
	%D%/telemctl.c

%C%_telemctl_CFLAGS = \
	$(AM_CFLAGS)

%C%_telemctl_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

%C%_telemprobd_SOURCES = \
	%D%/probe.c \
	%D%/telemdaemon.c \
	%D%/telemdaemon.h \
	%D%/journal/journal.c \
	%D%/journal/journal.h

%C%_telemprobd_LDADD = $(CURL_LIBS) \
	%D%/libtelem-shared.la \
	%D%/libtelemetry.la

%C%_telemprobd_CFLAGS = \
	$(AM_CFLAGS)

%C%_telemprobd_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

if HAVE_SYSTEMD_DAEMON
%C%_telemprobd_CFLAGS += \
	$(SYSTEMD_DAEMON_CFLAGS)
%C%_telemprobd_LDADD += \
	$(SYSTEMD_DAEMON_LIBS)
endif

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_telemprobd_CFLAGS += \
	$(SYSTEMD_JOURNAL_CFLAGS)
%C%_telemprobd_LDADD += \
	$(SYSTEMD_JOURNAL_LIBS)
endif
endif

# Telemetry post daemon
%C%_telempostd_SOURCES = \
	%D%/post.c \
	%D%/telempostdaemon.c \
	%D%/telempostdaemon.h \
	%D%/journal/journal.c \
	%D%/journal/journal.h \
	%D%/spool.h \
	%D%/spool.c \
	%D%/retention.h \
	%D%/retention.c \
	%D%/iorecord.c \
	%D%/iorecord.h

%C%_telempostd_LDADD = $(CURL_LIBS) \
	%D%/libtelem-shared.la \
	%D%/libtelemetry.la

%C%_telempostd_CFLAGS = \
	$(AM_CFLAGS)

%C%_telempostd_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

if HAVE_SYSTEMD_DAEMON
%C%_telempostd_CFLAGS += \
	$(SYSTEMD_DAEMON_CFLAGS)
%C%_telempostd_LDADD += \
	$(SYSTEMD_DAEMON_LIBS)
endif

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_telempostd_CFLAGS += \
	$(SYSTEMD_JOURNAL_CFLAGS)
%C%_telempostd_LDADD += \
	$(SYSTEMD_JOURNAL_LIBS)
endif
endif

# set library version info
SHAREDLIB_CURRENT=4
SHAREDLIB_REVISION=1
SHAREDLIB_AGE=0

noinst_LTLIBRARIES = %D%/libtelem-shared.la

%C%_libtelem_shared_la_SOURCES = \
	%D%/util.c \
	%D%/util.h \
	%D%/configuration.c \
	%D%/nica/inifile.c \
	%D%/nica/hashmap.c \
	%D%/configuration.h \
	%D%/common.c \
	%D%/common.h

%C%_libtelem_shared_la_CFLAGS = \
	$(AM_CFLAGS)

%C%_libtelem_shared_la_LDFLAGS = \
	$(AM_LDFLAGS)

lib_LTLIBRARIES = \
	%D%/libtelemetry.la

%C%_libtelemetry_la_SOURCES = \
	%D%/telemetry.c

include_HEADERS = %D%/telemetry.h

noinst_HEADERS = %D%/log.h

%C%_libtelemetry_la_CFLAGS = \
	$(AM_CFLAGS)

%C%_libtelemetry_la_LDFLAGS = \
	$(AM_LDFLAGS) \
	-version-info $(SHAREDLIB_CURRENT):$(SHAREDLIB_REVISION):$(SHAREDLIB_AGE) \
	-Wl,--version-script=$(top_srcdir)/src/telemetry.sym

EXTRA_DIST += \
	%D%/telemetry.sym

%C%_libtelemetry_la_LIBADD = \
	%D%/libtelem-shared.la \
	-ldl

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
