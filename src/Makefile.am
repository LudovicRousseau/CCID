CCID_BUNDLE = $(bundle)
CCID_LIB = libccid.$(DYN_LIB_EXT)
CCIDTWIN_LIB = libccidtwin.$(DYN_LIB_EXT)

CCID_VERSION=CCID_VERSION=`$(srcdir)/convert_version.pl $(PACKAGE_VERSION)`

lib_LTLIBRARIES =
LIBS_TO_INSTALL =
LIBS_TO_UNINSTALL =
if WITH_LIBUSB
lib_LTLIBRARIES += libccid.la
LIBS_TO_INSTALL += install_ccid
LIBS_TO_UNINSTALL += uninstall_ccid
noinst_PROGRAMS = parse
endif
if WITH_TWIN_SERIAL
lib_LTLIBRARIES += libccidtwin.la
LIBS_TO_INSTALL += install_ccidtwin
LIBS_TO_UNINSTALL += uninstall_ccidtwin
endif

COMMON = ccid.c \
	ccid.h \
	ccid_ifdhandler.h \
	commands.c \
	commands.h \
	debug.h \
	defs.h \
	ifdhandler.c \
	sys_generic.h \
	sys_unix.c \
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
	strlcpycat.h \
	simclist.c \
	simclist.h

if WITHOUT_PCSC
PROVIDED_BY_PCSC = debug.c
endif

libccid_la_SOURCES = $(COMMON) $(USB) $(TOKEN_PARSER) $(PROVIDED_BY_PCSC) $(T1)
libccid_la_LIBADD = $(LIBUSB_LIBS) $(PTHREAD_LIBS)
libccid_la_CFLAGS = $(PCSC_CFLAGS) $(LIBUSB_CFLAGS) $(PTHREAD_CFLAGS) \
	$(SYMBOL_VISIBILITY) -D$(CCID_VERSION) -DSIMCLIST_NO_DUMPRESTORE
libccid_la_LDFLAGS = -avoid-version

libccidtwin_la_SOURCES = $(COMMON) $(SERIAL) $(TOKEN_PARSER) \
	$(PROVIDED_BY_PCSC) $(T1)
libccidtwin_la_CFLAGS = $(PCSC_CFLAGS) $(PTHREAD_CFLAGS) $(SYMBOL_VISIBILITY) \
	-DTWIN_SERIAL -D$(CCID_VERSION) -DSIMCLIST_NO_DUMPRESTORE
libccidtwin_la_LIBADD = $(PTHREAD_LIBS)
libccidtwin_la_LDFLAGS = -avoid-version

parse_SOURCES = parse.c debug.c ccid_usb.c sys_unix.c $(TOKEN_PARSER)
parse_LDADD = $(LIBUSB_LIBS)
parse_CFLAGS = $(PCSC_CFLAGS) $(LIBUSB_CFLAGS) -DSIMCLIST_NO_DUMPRESTORE

EXTRA_DIST = Info.plist.src create_Info_plist.pl reader.conf.in \
	towitoko/COPYING towitoko/README openct/LICENSE openct/README \
	convert_version.pl 92_pcscd_ccid.rules

# We can't use install-exec-local since we want to overwrite the install
# rule. We do not want to _add_ files to install
install: $(LIBS_TO_INSTALL)

INSTALL_UDEV_RULE_FILE=@/bin/echo "***************" ; echo "copy the src/92_pcscd_ccid.rules file in udev directory (/etc/udev/rules.d/)" ; /bin/echo "***************"

Info.plist: Info.plist.src $(srcdir)/../readers/supported_readers.txt
	$(srcdir)/create_Info_plist.pl $(srcdir)/../readers/supported_readers.txt $(srcdir)/Info.plist.src --target=$(CCID_LIB) --version=$(VERSION) $(NOCLASS) > Info.plist

DISTCLEANFILES = tokenparser.c Info.plist

install_ccid: libccid.la Info.plist
	$(mkinstalldirs) "$(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/"
	cp Info.plist "$(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/"
	cp .libs/$(CCID_LIB) "$(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)/Contents/$(BUNDLE_HOST)/$(CCID_LIB)"
	$(INSTALL_UDEV_RULE_FILE)

install_ccidtwin: libccidtwin.la
	$(mkinstalldirs) "$(DESTDIR)$(ccidtwindir)"
	cp .libs/$(CCIDTWIN_LIB) "$(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB)"
	$(mkinstalldirs) "$(DESTDIR)/$(serialconfdir)" ; \
		perl -ne "s|TARGET|$(ccidtwindir)/$(CCIDTWIN_LIB)| ; print" $(srcdir)/reader.conf.in > "$(DESTDIR)/$(serialconfdir)/libccidtwin"

# do not uninstall the serial driver by default
# use explicitely 'make uninstall_ccidtwin'
uninstall: $(LIBS_TO_UNINSTALL)

uninstall_ccid:
	rm -rf "$(DESTDIR)$(usbdropdir)/$(CCID_BUNDLE)"

uninstall_ccidtwin:
	rm -f "$(DESTDIR)$(ccidtwindir)/$(CCIDTWIN_LIB)"
	rm -f "$(DESTDIR)/$(serialconfdir)/libccidtwin"

