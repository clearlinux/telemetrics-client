bin_PROGRAMS += \
	%D%/telem_journal

%C%_telem_journal_SOURCES = %D%/cli.c \
	%D%/journal.c \
	src/util.c \
	src/common.c
%C%_telem_journal_CFLAGS = \
	$(AM_CFLAGS)

if LOG_SYSTEMD
%C%_telem_journal_CFLAGS += $(SYSTEMD_JOURNAL_CFLAGS)
%C%_telem_journal_LDADD = $(SYSTEMD_JOURNAL_LIBS)
endif
# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
