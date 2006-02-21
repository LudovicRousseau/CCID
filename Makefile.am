# $Id$

SUBDIRS = aclocal src readers examples

AUX_DIST = \
	$(ac_aux_dir)/aclocal.m4 \
	$(ac_aux_dir)/config.guess \
	$(ac_aux_dir)/config.sub \
	$(ac_aux_dir)/depcomp \
	$(ac_aux_dir)/install-sh \
	$(ac_aux_dir)/ltmain.sh \
	$(ac_aux_dir)/missing \
	$(ac_aux_dir)/ylwrap

EXTRA_DIST = reconf ChangeLog \
	$(AUX_DIST)

MAINTAINERCLEANFILES = $(AUX_DIST)
DISTCLEANFILES = ChangeLog

# Automatically update the libtool script if it becomes out-of-date.
LIBTOOL_DEPS = @LIBTOOL_DEPS@
libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck

ChangeLog:
	svn2cl.sh --group-by-day --stdout --include-rev \
	| perl -pe 's/rousseau /Ludovic Rousseau /; s+trunk/Drivers/ccid/++g;' > $@

