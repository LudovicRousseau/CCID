ACLOCAL_AMFLAGS = -I m4

SUBDIRS = readers examples contrib src

EXTRA_DIST = bootstrap ChangeLog \
	SCARDCONTROL.md \
	SCARDGETATTRIB.md \
	README.md \
	$(AUX_DIST) \
	MacOSX/configure \
	MacOSX/debuglog.h \
	MacOSX/ifdhandler.h \
	MacOSX/pcsclite.h \
	MacOSX/reader.h \
	MacOSX/winscard.h \
	MacOSX/wintypes.h

MAINTAINERCLEANFILES = $(AUX_DIST)
DISTCLEANFILES = ChangeLog

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ChangeLog:
	git log --stat --decorate=short > $@

