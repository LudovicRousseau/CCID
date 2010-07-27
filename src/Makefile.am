# $Id$

CCID_BUNDLE = $(bundle)
CCID_LIB = libccid.$(DYN_LIB_EXT)
CCIDTWIN_LIB = libccidtwin.$(DYN_LIB_EXT)

CCID_VERSION=CCID_VERSION=`$(srcdir)/convert_version.pl $(PACKAGE_VERSION)`

lib_LTLIBRARIES =
LIBS_TO_INSTALL =
if WITH_LIBUSB
lib_LTLIBRARIES += libccid.la
LIBS_TO_INSTALL += install_ccid
bin_PROGRAMS = parse
endif
if WITH_TWIN_SERIAL
lib_LTLIBRARIES += libccidtwin.la
LIBS_TO_INSTALL += install_ccidtwin
endif

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
libccid_la_LIBADD = $(LEXLIB) $(LIBUSB_LIBS) $(PTHREAD_LIBS)
libccid_la_CFLAGS = $(PCSC_CFLAGS) $(LIBUSB_CFLAGS) $(PTHREAD_CFLAGS) \
	$(SYMBOL_VISIBILITY) -D$(CCID_VERSION)

libccidtwin_la_SOURCES = $(COMMON) $(SERIAL) $(TOKEN_PARSER) \
	$(PROVIDED_BY_PCSC) $(T1)
libccidtwin_la_CFLAGS = $(PCSC_CFLAGS) $(PTHREAD_CFLAGS) $(SYMBOL_VISIBILITY) \
	-DTWIN_SERIAL -D$(CCID_VERSION)
libccidtwin_la_LIBADD = $(PTHREAD_LIBS)

parse_SOURCES = parse.c debug.c
parse_LDADD = $(LIBUSB_LIBS)
parse_CFLAGS = $(PCSC_CFLAGS) $(LIBUSB_CFLAGS)

EXTRA_DIST = Info.plist.src create_Info_plist.pl reader.conf.in \
	towitoko/COPYING towitoko/README openct/LICENSE \
	convert_version.pl pcscd_ccid.rules

install: $(LIBS_TO_INSTALL)

if UDEV
ifdCapabilities=0x00000001
INSTALL_UDEV_RULE_FILE=@echo -e "\n\33[01;31m***************\n" ; echo "copy the src/pcscd_ccid.rules file in udev directory (/etc/udev/rules.d/)" ; echo -e "\n***************\n\33[0m"
else
ifdCapabilities=0x00000000
endif

Info.plist: Info.plist.src $(srcdir)/../readers/supported_readers.txt
	$(srcdir)/create_Info_plist.pl $(srcdir)/../readers/supported_readers.txt $(srcdir)/Info.plist.src --ifdCapabilities=$(ifdCapabilities) --target=$(CCID_LIB) --version=$(VERSION) --bundle=$(CCID_BUNDLE) $(NOCLASS) > Info.plist
	
DISTCLEANFILES = tokenparser.c Info.plist

install_ccid: libccid.la Info.plist
	$(mkinstalldirs) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/
	cp Info.plist $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/
	cp .libs/$(CCID_LIB) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB).$(VERSION)
	ln -fs $(CCID_LIB).$(VERSION) $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB)
	$(INSTALL_UDEV_RULE_FILE)

install_ccidtwin: libccidtwin.la
	$(mkinstalldirs) $(DESTDIR)$(ccidtwindir)
	cp .libs/$(CCIDTWIN_LIB) $(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB).$(VERSION)
	ln -fs $(CCIDTWIN_LIB).$(VERSION) $(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB)
	if [ -e $(DESTDIR)/etc/reader.conf ] ; \
	then \
		echo "Edit existing /etc/reader.conf" ; \
	else \
		$(mkinstalldirs) $(DESTDIR)/etc ; \
		perl -ne "s|TARGET|$(ccidtwindir)/$(CCIDTWIN_LIB)| ; print" $(srcdir)/reader.conf.in > $(DESTDIR)/etc/reader.conf ; \
	fi

# do not uninstall the serial driver by default
# use explicitely 'make uninstall_ccidtwin'
uninstall: uninstall_ccid

uninstall_ccid:
	rm -rf $(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)

uninstall_ccidtwin:
	rm -f $(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB).$(VERSION)

