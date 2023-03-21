EXTRA_DIST += \
	%D%/taplib.sh \
	%D%/telemetrics-client.supp

TEST_EXTENSIONS = .sh

tap_driver = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/build-aux/tap-driver.sh

LOG_DRIVER = $(tap_driver)
SH_LOG_DRIVER = $(tap_driver)

TESTS = $(check_PROGRAMS) $(dist_check_SCRIPTS)

check_PROGRAMS = \
	%D%/check_config \
	%D%/check_probd \
	%D%/check_postd \
	%D%/check_probes \
	%D%/check_journal \
	%D%/check_libtelemetry

dist_check_SCRIPTS = \
	%D%/create-core.sh

%C%_check_config_SOURCES = \
	%D%/configuration_check.h \
	%D%/check_config.c

%C%_check_config_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@

%C%_check_config_LDADD = \
	@CHECK_LIBS@ \
	$(top_builddir)/src/libtelem-shared.la

if HAVE_SYSTEMD_JOURNAL
if LOG_SYSTEMD
%C%_check_config_CFLAGS += $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_config_LDADD += $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_check_probd_SOURCES = \
	%D%/configuration_check.h \
	%D%/check_probd.c \
	src/telemdaemon.c \
	src/telemdaemon.h \
	src/iorecord.h \
	src/iorecord.c \
	src/journal/journal.c \
	src/journal/journal.h

%C%_check_probd_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@ \
	@CURL_CFLAGS@
%C%_check_probd_LDADD = \
	@CHECK_LIBS@ \
	@CURL_LIBS@ \
	$(top_builddir)/src/libtelem-shared.la

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_check_probd_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_probd_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_check_postd_SOURCES = \
	%D%/check_postd.c \
	src/spool.c \
	src/iorecord.c \
	src/retention.c \
        src/telempostdaemon.c \
        src/telempostdaemon.h \
        src/journal/journal.c \
        src/journal/journal.h

EXTRA_DIST += \
	%D%/telempostd/correct_message \
	%D%/telempostd/incorrect_headers \
	%D%/telempostd/empty_message \
	%D%/telempostd/incorrect_message

%C%_check_postd_CFLAGS = \
        $(AM_CFLAGS) \
        @CHECK_CFLAGS@ \
        @CURL_CFLAGS@
%C%_check_postd_LDADD = \
        @CHECK_LIBS@ \
        @CURL_LIBS@ \
        @JSON_C_LIBS@ \
        $(top_builddir)/src/libtelem-shared.la

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_check_postd_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_postd_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_check_probes_SOURCES = \
        %D%/read_oopsfile.h \
        %D%/read_oopsfile.c \
        %D%/check_probes.c \
	src/nica/nc-string.c \
	src/probes/klog_scanner.c \
	src/probes/klog_scanner.h \
	src/probes/oops_parser.c \
	src/probes/oops_parser.h 

EXTRA_DIST += \
	%D%/oops_test_files/2or3_digit_loglevel.txt \
	%D%/oops_test_files/ALSA.txt \
	%D%/oops_test_files/badness.txt \
	%D%/oops_test_files/bad_page_map.txt \
	%D%/oops_test_files/bug_kernel_handle.txt \
	%D%/oops_test_files/bug_kernel_handle_new.txt \
	%D%/oops_test_files/double_fault_DONE.txt \
	%D%/oops_test_files/double_fault.txt \
	%D%/oops_test_files/general_protection_fault.txt \
	%D%/oops_test_files/irq.txt \
	%D%/oops_test_files/kernel_bug_at_DONE.txt \
	%D%/oops_test_files/kernel_bug_at.txt \
	%D%/oops_test_files/kernel_null_pointer.txt \
	%D%/oops_test_files/loglevel_notimestamp.txt \
	%D%/oops_test_files/nologlevel_notimestamp.txt \
	%D%/oops_test_files/RTNL.txt \
	%D%/oops_test_files/soft_lockup.txt \
	%D%/oops_test_files/sysctl_check1 \
	%D%/oops_test_files/sysctl_check1.txt \
	%D%/oops_test_files/sysctl_check2 \
	%D%/oops_test_files/sysctl_check2.txt \
	%D%/oops_test_files/two_warnings.txt \
	%D%/oops_test_files/warning.txt \
	%D%/oops_test_files/warn_on.txt \
	%D%/oops_test_files/watchdog.txt

%C%_check_probes_CFLAGS = \
        $(AM_CFLAGS) \
        @CHECK_CFLAGS@
%C%_check_probes_LDADD = \
        @CHECK_LIBS@ \
        $(top_builddir)/src/libtelemetry.la \
        $(top_builddir)/src/libtelem-shared.la

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_check_probes_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_probes_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_check_journal_SOURCES = \
	%D%/check_journal.c \
	src/journal/journal.c \
	src/util.h \
	src/util.c

%C%_check_journal_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@

%C%_check_journal_LDADD = \
	@CHECK_LIBS@

if HAVE_SYSTEMD_JOURNAL
if LOG_SYSTEMD
%C%_check_journal_CFLAGS += $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_journal_LDADD += $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_check_libtelemetry_SOURCES = \
	%D%/check_libtelemetry.c \
	src/configuration.h \
	src/util.h \
	src/nica/hashmap.h \
	src/nica/inifile.h

%C%_check_libtelemetry_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@
%C%_check_libtelemetry_LDADD = \
	@CHECK_LIBS@ \
	$(top_builddir)/src/libtelemetry.la \
	$(top_builddir)/src/libtelem-shared.la

if HAVE_SYSTEMD_JOURNAL
if LOG_SYSTEMD
%C%_check_libtelemetry_CFLAGS += $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_libtelemetry_LDADD += $(SYSTEMD_JOURNAL_LIBS)
endif
endif

@VALGRIND_CHECK_RULES@
VALGRIND_SUPPRESSIONS_FILES = %D%/telemetrics-client.supp
VALGRIND_FLAGS = \
	--error-exitcode=1 \
	--track-origins=yes \
	--leak-resolution=low \
	--verbose \
	--leak-check=full \
	--show-possibly-lost=no

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
