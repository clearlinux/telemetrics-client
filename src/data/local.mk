pathfix = @sed \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@libdir[@]|$(libdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	-e 's|@SOCKETDIR[@]|$(SOCKETDIR)|g' \
	-e 's|@systemctldir[@]|$(SYSTEMD_SYSTEMCTLDIR)|g'

EXTRA_DIST += \
	%D%/40-core-ulimit.conf \
	%D%/40-crash-probe.conf.in \
	%D%/example.conf \
	%D%/hprobe.service.in \
	%D%/hprobe.timer \
	%D%/journal-probe.service.in \
	%D%/oops-probe.service.in \
	%D%/pstore-probe.service.in \
	%D%/klogscanner.service.in \
	%D%/pstore-clean.service.in \
	%D%/libtelemetry.pc.in \
	%D%/telemd.path.in \
	%D%/telemd.service.in \
	%D%/telemd.socket.in \
	%D%/telemd-update-trigger.service.in \
	%D%/telemetrics-dirs.conf.in \
	%D%/telemetrics.conf.in

configdefaultdir = $(datadir)/defaults/telemetrics
configdefault_DATA = %D%/telemetrics.conf

%D%/telemetrics.conf: %D%/telemetrics.conf.in
	$(pathfix) < $< > $@

tmpfilesdir = @SYSTEMD_TMPFILESDIR@
tmpfiles_DATA = %D%/telemetrics-dirs.conf

%D%/telemetrics-dirs.conf: %D%/telemetrics-dirs.conf.in
	$(pathfix) < $< > $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = %D%/libtelemetry.pc

%D%/libtelemetry.pc: %D%/libtelemetry.pc.in
	$(pathfix) < $< > $@

systemdunitdir = @SYSTEMD_UNITDIR@
systemdunit_DATA = \
	%D%/hprobe.service \
	%D%/hprobe.timer \
	%D%/journal-probe.service \
	%D%/oops-probe.service \
	%D%/pstore-probe.service \
	%D%/klogscanner.service \
	%D%/pstore-clean.service \
	%D%/telemd.service \
	%D%/telemd.socket \
	%D%/telemd-update-trigger.service \
	%D%/telemd.path

%D%/hprobe.service: %D%/hprobe.service.in
	$(pathfix) < $< > $@

%D%/journal-probe.service: %D%/journal-probe.service.in
	$(pathfix) < $< > $@

%D%/oops-probe.service: %D%/oops-probe.service.in
	$(pathfix) < $< > $@

%D%/pstore-probe.service: %D%/pstore-probe.service.in
	$(pathfix) < $< > $@

%D%/klogscanner.service: %D%/klogscanner.service.in
	$(pathfix) < $< > $@

%D%/pstore-clean.service: %D%/pstore-clean.service.in
	$(pathfix) < $< > $@

%D%/telemd.service: %D%/telemd.service.in
	$(pathfix) < $< > $@

%D%/telemd.socket: %D%/telemd.socket.in
	$(pathfix) < $< > $@

%D%/telemd.path: %D%/telemd.path.in
	$(pathfix) < $< > $@

%D%/telemd-update-trigger.service: %D%/telemd-update-trigger.service.in
	$(pathfix) < $< > $@

sysctldir = @SYSTEMD_SYSCTLDIR@
sysctl_DATA = %D%/40-crash-probe.conf

%D%/40-crash-probe.conf: %D%/40-crash-probe.conf.in
	$(pathfix) < $< > $@

systemconfdir = @SYSTEMD_SYSTEMCONFDIR@
systemconf_DATA = %D%/40-core-ulimit.conf

clean-local:
	-rm -f  %D%/telemd.service \
		%D%/telemd.socket \
		%D%/telemd.path \
		%D%/telemd-update-trigger.service \
		%D%/telemetrics.conf \
		%D%/telemetrics-dirs.conf \
		%D%/libtelemetry.pc \
		%D%/40-crash-probe.conf \
		%D%/journal-probe.service \
		%D%/oops-probe.service \
		%D%/pstore-probe.service \
		%D%/klogscanner.service \
		%D%/pstore-clean.service \
		%D%/hprobe.service \
		%D%/.dirstamp
