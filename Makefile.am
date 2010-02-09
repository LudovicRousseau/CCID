ACLOCAL_AMFLAGS = -I m4

SUBDIRS = m4 readers examples contrib src

EXTRA_DIST = bootstrap ChangeLog SCARDGETATTRIB.txt \
	$(AUX_DIST) \
	MacOSX/configure \
	MacOSX/convert_reader_h.pl \
	MacOSX/debuglog.h \
	MacOSX/ifdhandler.h \
	MacOSX/pcsclite.h \
	MacOSX/reader.h.in \
	MacOSX/winscard.h \
	MacOSX/wintypes.h

MAINTAINERCLEANFILES = $(AUX_DIST)
DISTCLEANFILES = ChangeLog

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ChangeLog:
	svn2cl --group-by-day --stdout --include-rev --reparagraph \
	| perl -pe 's/  rousseau/  Ludovic Rousseau/; \
	s+trunk/Drivers/ccid/++g;' > $@

