# $Id$

CCID_BUNDLE = ifd-ccid.bundle
CCID_LIB = libccid.$(DYN_LIB_EXT)
CCIDTWIN_LIB = libccidtwin.$(DYN_LIB_EXT)

lib_LTLIBRARIES = libccid.la libccidtwin.la
bin_PROGRAMS = parse

COMMON = ccid.c \
	ccid.h \
	ccid_ifdhandler.h \
	commands.c \
	commands.h \
	debug.h \
	defs.h \
	ifdhandler.c \
	utils.c \
	utils.h
USB = ccid_usb.c ccid_usb.h
SERIAL = ccid_serial.c ccid_serial.h
T1 = towitoko/atr.c \
	towitoko/atr.h \
	towitoko/defines.h \
	towitoko/pps.c \
	towitoko/pps.h \
	openct/buffer.c \
	openct/buffer.h \
	openct/checksum.c \
	openct/checksum.h \
	openct/proto-t1.c \
	openct/proto-t1.h

# needed for MacOS X or when used without pcsc-lite (Solaris)
if NEED_PARSER
TOKEN_PARSER = tokenparser_fake.l
endif

libccid_la_SOURCES = $(COMMON) $(USB) $(TOKEN_PARSER) $(T1)
libccid_la_LIBADD = @LIBDL@ @LEXLIB@ @COREFOUNDATION@ @IOKIT@ @LIBUSB@

libccidtwin_la_SOURCES = $(COMMON) $(SERIAL) $(T1)
libccidtwin_la_CFLAGS = -DTWIN_SERIAL

parse_SOURCES = tokenparser.l parse.c parser.h debug.c
parse_LDADD = libccid.la

EXTRA_DIST = Info.plist create_Info_plist.pl reader.conf.in \
	towitoko/COPYING towitoko/README openct/LICENSE

tokenparser_fake.l: tokenparser.l
	cp -f $^ $@

# do not install the serial driver by default
# use explicitely 'make install_ccidtwin'
install: install_ccid

install_ccid: libccid.la
	$(mkinstalldirs) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/
	$(srcdir)/create_Info_plist.pl $(srcdir)/../readers/supported_readers.txt $(srcdir)/Info.plist | sed s/VERSION/$(VERSION)/ | sed s/TARGET/$(CCID_LIB)/ > $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/Info.plist
	cp .libs/$(CCID_LIB) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB).$(VERSION)

install_ccidtwin: libccidtwin.la
	$(mkinstalldirs) $(DESTDIR)$(ccidtwindir)
	cp .libs/$(CCIDTWIN_LIB) $(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB).$(VERSION)
	if [ -e $(DESTDIR)/etc/reader.conf ] ; \
	then \
		echo "Edit existing /etc/reader.conf" ; \
	else \
		$(mkinstalldirs) $(DESTDIR)/etc ; \
		perl -ne "s|TARGET|$(ccidtwindir)/$(CCIDTWIN_LIB).$(VERSION)| ; print" $(srcdir)/reader.conf.in > $(DESTDIR)/etc/reader.conf ; \
	fi

# do not uninstall the serial driver by default
# use explicitely 'make uninstall_ccidtwin'
uninstall: uninstall_ccid

uninstall_ccid:
	rm -rf $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)

uninstall_ccidtwin:
	rm -f $(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB).$(VERSION)

