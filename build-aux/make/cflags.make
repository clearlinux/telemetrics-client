AM_CFLAGS = \
	-std=gnu99 \
	-pedantic \
	-Wall \
	-Wformat \
	-Wformat-security \
	-Wimplicit-function-declaration \
	-Wstrict-prototypes \
	-Wundef \
	-fno-common \
	-Wconversion \
	-Wunreachable-code \
	-funsigned-char \
	-fPIE \
	-fPIC \
	-fno-strict-overflow \
	-fno-delete-null-pointer-checks \
	-fwrapv

AM_CPPFLAGS = \
	-D_FORTIFY_SOURCE=2 \
	-I $(top_builddir) \
	-I $(top_srcdir) \
	-I $(top_builddir)/src \
	-I $(top_srcdir)/src \
	-DTOPSRCDIR=\"$(top_srcdir)\" \
	-DABSTOPSRCDIR=\"$(abs_top_srcdir)\" \
	-DDATADIR=\"$(datadir)\" \
	-DLOCALSTATEDIR=\"$(localstatedir)\" \
	-DPSTOREDIR=\"$(localstatedir)/cache/telemetry/pstore\" \
	-DTESTOOPSDIR=\"$(top_srcdir)/tests/oops_test_files\" \
	-DBACKEND_ADDR=\"$(BACKEND_ADDR)\"

# vim: filetype=automake tabstop=8 shiftwidth=8 noexpandtab
