# $Id$

CCID_BUNDLE = ifd-ccid.bundle
CCID_LIB = libccid.$(DYN_LIB_EXT)
CCIDTWIN_LIB = libccidtwin.$(DYN_LIB_EXT)
CCIDFTWIN_PATH = $(prefix)/lib/pcsc/drivers/serial

lib_LTLIBRARIES = libccid.la libccidtwin.la
bin_PROGRAMS = parse

COMMON = ccid.c ccid.h \
	commands.c commands.h debug.c debug.h defs.h ifdhandler.c \
	ifdhandler.h pcscdefines.h utils.c utils.h
USB = ccid_usb.c ccid_usb.h
SERIAL = ccid_serial.c ccid_serial.h

# needed for MacOS X
if NEED_PARSER
CCID_PARSER = tokenparser.l
else
PARSE_PARSER = tokenparser.l
endif

libccid_la_SOURCES = $(COMMON) $(USB) $(CCID_PARSER)
libccid_la_LIBADD = @LIBDL@ @LEXLIB@ @COREFOUNDATION@ @IOKIT@ @LIBUSB@

libccidtwin_la_SOURCES = $(COMMON) $(SERIAL)
libccidtwin_la_CFLAGS = -DTWIN_SERIAL

parse_SOURCES = $(PARSE_PARSER) parse.c parser.h
parse_LDADD = libccid.la

EXTRA_DIST = Info.plist reader.conf.in

install: install_ccid install_ccidtwin

install_ccid: libccid.la
	mkdir -p $(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/
	cp .libs/$(CCID_LIB) $(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB).$(VERSION)
	sed s/VERSION/$(VERSION)/ $(srcdir)/Info.plist | sed s/TARGET/$(CCID_LIB)/ > $(CCID_BUNDLE)/Contents/Info.plist
	mkdir -p $(usbdropdir)
	cp -r $(CCID_BUNDLE) $(usbdropdir)

install_ccidtwin: libccidtwin.la
	perl -ne "s|TARGET|$(CCIDFTWIN_PATH)/$(CCIDTWIN_LIB).$(VERSION)| ; print" $(srcdir)/reader.conf.in > reader.conf
	mkdir -p $(CCIDFTWIN_PATH)
	cp .libs/$(CCIDTWIN_LIB) $(CCIDFTWIN_PATH)/$(CCIDTWIN_LIB).$(VERSION)
	@echo "copy src/reader.conf in /etc/ or edit /etc/reader.conf"

uninstall: uninstall_ccid uninstall_ccidtwin

uninstall_ccid:
	rm -f $(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB)
	rm -f $(usbdropdir)/$(CCID_BUNDLE)/Contents/Info.plist

uninstall_ccidtwin:
	rm -f $(CCIDFTWIN_PATH)/$(CCIDTWIN_LIB).$(VERSION)

