
sbin_PROGRAMS = hcidump

noinst_PROGRAMS = bpasniff csrsniff

LDADD = @BLUEZ_LIBS@ $(top_builddir)/parser/libparser.a

man_MANS = hcidump.8

AM_CFLAGS = @BLUEZ_CFLAGS@

INCLUDES = -I$(top_srcdir)

EXTRA_DIST = $(man_MANS) magic.btsnoop

MAINTAINERCLEANFILES = Makefile.in
