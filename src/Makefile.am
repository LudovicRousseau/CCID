# $Id$

CCID_BUNDLE = ifd-ccid.bundle
CCID_LIB = libccid.$(DYN_LIB_EXT)
CCIDTWIN_LIB = libccidtwin.$(DYN_LIB_EXT)
CCIDFTWIN_PATH = $(prefix)/pcsc/drivers/serial

lib_LTLIBRARIES = libccid.la libccidtwin.la
bin_PROGRAMS = parse

COMMON = ccid.c ccid.h \
	commands.c commands.h debug.c debug.h defs.h ifdhandler.c \
	ifdhandler.h pcscdefines.h utils.c utils.h
USB = ccid_usb.c ccid_usb.h
SERIAL = ccid_serial.c ccid_serial.h

# needed for MacOS X
if NEED_PARSER
TOKEN_PARSER = tokenparser_macosx.l
endif

libccid_la_SOURCES = $(COMMON) $(USB) $(TOKEN_PARSER)
libccid_la_LIBADD = @LIBDL@ @LEXLIB@ @COREFOUNDATION@ @IOKIT@ @LIBUSB@

libccidtwin_la_SOURCES = $(COMMON) $(SERIAL)
libccidtwin_la_CFLAGS = -DTWIN_SERIAL

parse_SOURCES = tokenparser.l parse.c parser.h
parse_LDADD = libccid.la

EXTRA_DIST = Info.plist reader.conf.in

tokenparser_macosx.l: tokenparser.l
	cp -f $^ $@

install: install_ccid install_ccidtwin

install_ccid: libccid.la
	$(mkinstalldirs) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/
	cp .libs/$(CCID_LIB) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB).$(VERSION)
	sed s/VERSION/$(VERSION)/ $(srcdir)/Info.plist | sed s/TARGET/$(CCID_LIB)/ > $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/Info.plist

install_ccidtwin: libccidtwin.la
	$(mkinstalldirs) $(DESTDIR)$(CCIDFTWIN_PATH)
	cp .libs/$(CCIDTWIN_LIB) $(DESTDIR)$(CCIDFTWIN_PATH)/$(CCIDTWIN_LIB).$(VERSION)
	if [ -e $(DESTDIR)/etc/reader.conf ] ; \
	then \
		echo "Edit existing /etc/reader.conf" ; \
	else \
		$(mkinstalldirs) $(DESTDIR)/etc ; \
		perl -ne "s|TARGET|$(CCIDFTWIN_PATH)/$(CCIDTWIN_LIB).$(VERSION)| ; print" $(srcdir)/reader.conf.in > $(DESTDIR)/etc/reader.conf ; \
	fi

uninstall: uninstall_ccid uninstall_ccidtwin

uninstall_ccid:
	rm -rf $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)

uninstall_ccidtwin:
	rm -f $(DESTDIR)$(CCIDFTWIN_PATH)/$(CCIDTWIN_LIB).$(VERSION)

