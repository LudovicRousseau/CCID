# $Id$

SUBDIRS = src readers

AUX_DIST = \
	$(ac_aux_dir)/aclocal.m4 \
	$(ac_aux_dir)/config.guess \
	$(ac_aux_dir)/config.sub \
	$(ac_aux_dir)/depcomp \
	$(ac_aux_dir)/install-sh \
	$(ac_aux_dir)/ltmain.sh \
	$(ac_aux_dir)/missing \
	$(ac_aux_dir)/mkinstalldirs \
	$(ac_aux_dir)/ylwrap

EXTRA_DIST = bootstrap ChangeLog.cvs \
	$(AUX_DIST)

MAINTAINERCLEANFILES = $(AUX_DIST)
DISTCLEANFILES = ChangeLog.cvs

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ChangeLog.cvs:
	rcs2log | perl -e 'while (<>) { s/rousseau /Ludovic Rousseau /; \
	s+/cvsroot/pcsclite/PCSC/++g; print ; }' > $@

.PHONY: ChangeLog.cvs

