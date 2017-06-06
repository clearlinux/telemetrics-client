EXTRA_DIST += %D%/taplib.sh

TEST_EXTENSIONS = .sh

tap_driver = env AM_TAP_AWK='$(AWK)' $(SHELL) \
	$(top_srcdir)/build-aux/tap-driver.sh

LOG_DRIVER = $(tap_driver)
SH_LOG_DRIVER = $(tap_driver)

TESTS = $(check_PROGRAMS) $(dist_check_SCRIPTS)

check_PROGRAMS = \
	%D%/check_config \
	%D%/check_daemon \
	%D%/check_probes \
	%D%/check_libtelemetry

dist_check_SCRIPTS = \
	%D%/create-core.sh

%C%_check_config_SOURCES = \
	%D%/check_config.c

%C%_check_config_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@

%C%_check_config_LDADD = \
	@CHECK_LIBS@ \
	$(top_builddir)/src/libtelem-shared.la

%C%_check_daemon_SOURCES = \
	%D%/check_daemon.c \
	src/telemdaemon.c \
	src/telemdaemon.h

%C%_check_daemon_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@ \
	@CURL_CFLAGS@
%C%_check_daemon_LDADD = \
	@CHECK_LIBS@ \
	@CURL_LIBS@ \
	$(top_builddir)/src/libtelem-shared.la

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_check_daemon_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_daemon_LDADD += \
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
        $(top_builddir)/src/libtelem-shared.la

if LOG_SYSTEMD
if HAVE_SYSTEMD_JOURNAL
%C%_check_probes_CFLAGS += \
        $(SYSTEMD_JOURNAL_CFLAGS)
%C%_check_probes_LDADD += \
        $(SYSTEMD_JOURNAL_LIBS)
endif
endif

%C%_check_libtelemetry_SOURCES = \
	%D%/check_libtelemetry.c \
	src/telemetry.c

%C%_check_libtelemetry_CFLAGS = \
	$(AM_CFLAGS) \
	@CHECK_CFLAGS@
%C%_check_libtelemetry_LDADD = \
	@CHECK_LIBS@ \
	$(top_builddir)/src/libtelemetry.la \
	$(top_builddir)/src/libtelem-shared.la

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
