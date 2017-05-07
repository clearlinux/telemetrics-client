bin_PROGRAMS += \
	%D%/hprobe \
	%D%/crashprobe \
	%D%/telem-record-gen \
	%D%/klogscanner \
	%D%/pstoreprobe \
	%D%/oopsprobe \
	%D%/pstoreclean

%C%_hprobe_SOURCES = %D%/hello.c
%C%_hprobe_LDADD = $(top_builddir)/src/libtelemetry.la
%C%_hprobe_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

%C%_telem_record_gen_SOURCES = %D%/telem_record_gen.c
%C%_telem_record_gen_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)
%C%_telem_record_gen_LDADD = $(top_builddir)/src/libtelemetry.la \
	$(GLIB_LIBS)
%C%_telem_record_gen_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

%C%_pstoreclean_SOURCES = %D%/pstore_clean.c
%C%_pstoreclean_CFLAGS = \
        $(AM_CFLAGS)
%C%_pstoreclean_LDADD = \
	$(top_builddir)/src/libtelemetry.la \
	$(top_builddir)/src/libtelem-shared.la
%C%_pstoreclean_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_pstoreclean_CFLAGS += \
	$(SYSTEMD_JOURNAL_CFLAGS)
%C%_pstoreclean_LDADD += \
	$(SYSTEMD_JOURNAL_LIBS)
endif
endif


%C%_crashprobe_SOURCES = \
	%D%/crash_probe.c \
	%D%/../nica/nc-string.c \
	%D%/probe.h
%C%_crashprobe_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)
%C%_crashprobe_LDADD = \
	$(top_builddir)/src/libtelemetry.la \
	$(top_builddir)/src/libtelem-shared.la \
	@ELFUTILS_LIBS@ \
	$(GLIB_LIBS)
%C%_crashprobe_LDFLAGS = \
       $(AM_LDFLAGS) \
       -pie

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_crashprobe_CFLAGS += \
	$(SYSTEMD_JOURNAL_CFLAGS)
%C%_crashprobe_LDADD += \
	$(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_pstoreprobe_SOURCES = \
	%D%/pstore_probe.c \
	%D%/../nica/nc-string.c \
	%D%/oops_parser.c
%C%_pstoreprobe_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS)
%C%_pstoreprobe_LDADD = \
	$(top_builddir)/src/libtelemetry.la \
	$(top_builddir)/src/libtelem-shared.la \
	$(GLIB_LIBS)

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_pstoreprobe_CFLAGS += \
	$(SYSTEMD_JOURNAL_CFLAGS)
%C%_pstoreprobe_LDADD += \
	$(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_klogscanner_SOURCES = \
	%D%/klog_main.c \
        %D%/klog_scanner.c \
	%D%/klog_scanner.h \
	%D%/oops_parser.h \
	%D%/../nica/nc-string.c \
	%D%/oops_parser.c
%C%_klogscanner_CFLAGS = \
        $(AM_CFLAGS) \
	$(GLIB_CFLAGS)
%C%_klogscanner_LDADD = \
        $(top_builddir)/src/libtelemetry.la \
	$(GLIB_LIBS)
%C%_klogscanner_LDFLAGS = \
        $(AM_LDFLAGS) \
        -pie

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_klogscanner_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_klogscanner_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif

if HAVE_SYSTEMD_JOURNAL
%C%_klogscanner_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_klogscanner_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_oopsprobe_SOURCES = \
        %D%/oops_probe.c \
	%D%/oops_parser.c \
	%D%/../nica/nc-string.c \
	%D%/probe.h
%C%_oopsprobe_CFLAGS = \
        $(AM_CFLAGS) \
        $(GLIB_CFLAGS)
%C%_oopsprobe_LDADD = \
        $(top_builddir)/src/libtelemetry.la \
        $(top_builddir)/src/libtelem-shared.la \
        $(GLIB_LIBS)

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_oopsprobe_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_oopsprobe_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

if HAVE_SYSTEMD_JOURNAL
if HAVE_SYSTEMD_ID128
bin_PROGRAMS += \
	%D%/journalprobe

%C%_journalprobe_SOURCES = \
	%D%/journal.c
%C%_journalprobe_CFLAGS = \
	$(AM_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(SYSTEMD_ID128_CFLAGS) \
	$(SYSTEMD_JOURNAL_CFLAGS)
%C%_journalprobe_LDADD = \
	$(top_builddir)/src/libtelemetry.la \
	$(top_builddir)/src/libtelem-shared.la \
	$(GLIB_LIBS) \
	$(SYSTEMD_ID128_LIBS) \
	$(SYSTEMD_JOURNAL_LIBS)
%C%_journalprobe_LDFLAGS = \
	$(AM_LDFLAGS) \
	-pie
endif
endif

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
