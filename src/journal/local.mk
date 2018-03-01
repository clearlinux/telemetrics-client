bin_PROGRAMS += \
	%D%/telem_journal

%C%_telem_journal_SOURCES = %D%/cli.c \
	%D%/journal.c \
	src/util.c \
	src/common.c

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
