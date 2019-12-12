pathfix = @sed \
	-e 's|@prefix[@]|$(prefix)|g' \
	-e 's|@bindir[@]|$(bindir)|g' \
	-e 's|@libdir[@]|$(libdir)|g' \
	-e 's|@localstatedir[@]|$(localstatedir)|g' \
	-e 's|@PACKAGE_VERSION[@]|$(PACKAGE_VERSION)|g' \
	-e 's|@SOCKETDIR[@]|$(SOCKETDIR)|g' \
	-e 's|@systemctldir[@]|$(SYSTEMD_SYSTEMCTLDIR)|g' \
	-e 's|@BACKEND_ADDR[@]|$(BACKEND_ADDR)|g'

EXTRA_DIST += \
	%D%/40-core-ulimit.conf \
	%D%/40-crash-probe.conf.in \
	%D%/example.conf \
	%D%/example.1.conf \
	%D%/example.2.conf \
	%D%/hprobe.service.in \
	%D%/hprobe.timer \
	%D%/journal-probe.service.in \
	%D%/journal-probe-tail.service.in \
	%D%/pstore-probe.service.in \
	%D%/klogscanner.service.in \
	%D%/pstore-clean.service.in \
	%D%/libtelemetry.pc.in \
	%D%/telempostd.service.in \
	%D%/telempostd.path.in \
	%D%/telemprobd.service.in \
	%D%/telemprobd.socket.in \
	%D%/telemprobd-update-trigger.service.in \
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
	%D%/journal-probe-tail.service \
	%D%/pstore-probe.service \
	%D%/klogscanner.service \
	%D%/pstore-clean.service \
	%D%/telemprobd.service \
	%D%/telemprobd.socket \
	%D%/telemprobd-update-trigger.service \
	%D%/telempostd.service \
	%D%/telempostd.path

%D%/hprobe.service: %D%/hprobe.service.in
	$(pathfix) < $< > $@

%D%/journal-probe.service: %D%/journal-probe.service.in
	$(pathfix) < $< > $@

%D%/journal-probe-tail.service: %D%/journal-probe-tail.service.in
	$(pathfix) < $< > $@

%D%/pstore-probe.service: %D%/pstore-probe.service.in
	$(pathfix) < $< > $@

%D%/klogscanner.service: %D%/klogscanner.service.in
	$(pathfix) < $< > $@

%D%/pstore-clean.service: %D%/pstore-clean.service.in
	$(pathfix) < $< > $@

%D%/telempostd.path: %D%/telempostd.path.in
	$(pathfix) < $< > $@

%D%/telempostd.service: %D%/telempostd.service.in
	$(pathfix) < $< > $@

%D%/telemprobd.service: %D%/telemprobd.service.in
	$(pathfix) < $< > $@

%D%/telemprobd.socket: %D%/telemprobd.socket.in
	$(pathfix) < $< > $@

%D%/telemprobd-update-trigger.service: %D%/telemprobd-update-trigger.service.in
	$(pathfix) < $< > $@

sysctldir = @SYSTEMD_SYSCTLDIR@
sysctl_DATA = %D%/40-crash-probe.conf

%D%/40-crash-probe.conf: %D%/40-crash-probe.conf.in
	$(pathfix) < $< > $@

systemconfdir = @SYSTEMD_SYSTEMCONFDIR@
systemconf_DATA = %D%/40-core-ulimit.conf

clean-local:
	-rm -f  %D%/telemprobd.service \
		%D%/telemprobd.socket \
		%D%/telempostd.service \
		%D%/telempostd.path \
		%D%/telemprobd-update-trigger.service \
		%D%/telemetrics.conf \
		%D%/telemetrics-dirs.conf \
		%D%/libtelemetry.pc \
		%D%/40-crash-probe.conf \
		%D%/journal-probe.service \
		%D%/journal-probe-tail.service \
		%D%/pstore-probe.service \
		%D%/klogscanner.service \
		%D%/pstore-clean.service \
		%D%/hprobe.service \
		%D%/.dirstamp
