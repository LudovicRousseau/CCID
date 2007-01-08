# $Id$

CCID_BUNDLE = ifd-ccid.bundle
CCID_LIB = libccid.$(DYN_LIB_EXT)
CCIDTWIN_LIB = libccidtwin.$(DYN_LIB_EXT)

CCID_VERSION=CCID_VERSION=`$(srcdir)/convert_version.pl $(PACKAGE_VERSION)`

lib_LTLIBRARIES = libccid.la
LIBS_TO_INSTALL = install_ccid
if WITH_TWIN_SERIAL
lib_LTLIBRARIES += libccidtwin.la
LIBS_TO_INSTALL += install_ccidtwin
endif
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
TOKEN_PARSER = tokenparser.l parser.h \
	strlcpy.c \
	misc.h \
	strlcpycat.h

if WITHOUT_PCSC
PROVIDED_BY_PCSC = debug.c
endif

libccid_la_SOURCES = $(COMMON) $(USB) $(TOKEN_PARSER) $(PROVIDED_BY_PCSC) $(T1)
libccid_la_LIBADD = $(LEXLIB) $(COREFOUNDATION) $(IOKIT) \
	$(LIBUSB_LIBS) $(PTHREAD_LIBS)
libccid_la_CFLAGS = $(PCSC_CFLAGS) $(LIBUSB_CFLAGS) $(PTHREAD_CFLAGS) \
	$(SYMBOL_VISIBILITY) -D$(CCID_VERSION)

libccidtwin_la_SOURCES = $(COMMON) $(SERIAL) $(TOKEN_PARSER) \
	$(PROVIDED_BY_PCSC) $(T1)
libccidtwin_la_CFLAGS = $(PCSC_CFLAGS) $(PTHREAD_CFLAGS) $(SYMBOL_VISIBILITY) \
	-DTWIN_SERIAL -D$(CCID_VERSION)
libccidtwin_la_LIBADD = $(PTHREAD_LIBS)

parse_SOURCES = parse.c debug.c
parse_LDADD = libccid.la
parse_CFLAGS = $(PCSC_CFLAGS) $(LIBUSB_CFLAGS)

EXTRA_DIST = Info.plist create_Info_plist.pl reader.conf.in \
	towitoko/COPYING towitoko/README openct/LICENSE \
	convert_version.pl pcscd_ccid.rules

install: $(LIBS_TO_INSTALL)

if UDEV
ifdCapabilities=0x00000001
INSTALL_UDEV_RULE_FILE=$(mkinstalldirs) $(DESTDIR)/etc/udev/rules.d ; cp pcscd_ccid.rules $(DESTDIR)/etc/udev/ ; cd $(DESTDIR)/etc/udev/rules.d ; ln -s ../pcscd_ccid.rules 50-pcscd_ccid.rules
else
ifdCapabilities=0x00000000
endif

install_ccid: libccid.la
	$(mkinstalldirs) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/
	$(srcdir)/create_Info_plist.pl $(srcdir)/../readers/supported_readers.txt $(srcdir)/Info.plist $(ifdCapabilities) | sed s/VERSION/$(VERSION)/ | sed s/TARGET/$(CCID_LIB)/ > $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/Info.plist
	cp .libs/$(CCID_LIB) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB).$(VERSION)
	$(INSTALL_UDEV_RULE_FILE)

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

