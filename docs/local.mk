MANPAGES = \
	docs/man/telemctl.1 \
	docs/man/telemd.1 \
	docs/man/telem-record-gen.1 \
	docs/man/telemetry.3 \
	docs/man/telemetrics.conf.5

MANLINKS = \
	docs/man/tm_create_record.3 \
	docs/man/tm_send_record.3 \
	docs/man/tm_set_config_file.3 \
	docs/man/tm_set_payload.3

manpages:
	for MANPAGE in $(MANPAGES); do \
		ronn --roff < $${MANPAGE}.md > $${MANPAGE}; \
		ronn --html < $${MANPAGE}.md > $${MANPAGE}.html; \
	done

dist_man_MANS = \
	$(MANPAGES) $(MANLINKS)

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
