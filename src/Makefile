#
# $Id$
#

# name of bundle
BUNDLE=ifd-ccid.bundle

# by default install in /usr/local
ifeq ($(DESTDIR),)
DESTDIR=/usr/local
endif

# by default use /usr/local/pcsc/drivers/
ifeq ($(PCSCLITE_HP_DROPDIR),)
PCSCLITE_HP_DROPDIR=/usr/local/pcsc/drivers/
endif

# use a correct default CFLAGS
ifeq ($(CFLAGS),)
CFLAGS = -O2 -g -Wall
endif

CFLAGS += -fPIC -I. -I/usr/local/include -DPCSCLITE_HP_DROPDIR=\"$(PCSCLITE_HP_DROPDIR)$(BUNDLE)\"
LDFLAGS += -L/usr/local/lib -lusb

# needed to correctly generate the lexer on MacOS X
YLWRAP = ylwrap
CPPFLAGS = -fno-common

ifeq ($(OSTYPE),darwin)
TARGET_LIB = libccid.dylib
PARSE =
BUNDLE_HOST = MacOS
else
TARGET_LIB = libccid.so
PARSE = parse
BUNDLE_HOST = Linux
endif

# /home/rousseau/sc/pcsc/ccid-0.5.10 -> 0.5.10
version=$(shell expr `pwd` : '.*-\([0-9.]*\)')

objects = capabilities.o commands.o debug.o ifdhandler.o ccid_usb.o utils.o
parse_objects = parse.o
sources = $(objects:%.o=%.c) $(parse_objects:%.o=%.c)

all: check expert

check: check_stamp
check_stamp:
	./check
	touch check_stamp

expert: $(PARSE) $(TARGET_LIB)

parse: $(objects) tokenparser.o parse.o
	$(CC) $(LDFLAGS) -o $@ $^

libccid.so: $(objects)
	$(CC) -shared $(LDFLAGS) -o $@ $(objects)
	#strip $@

libccid.dylib: $(objects) tokenparser.o
	$(CC) -dynamiclib -flat_namespace -undefined suppress $(LDFLAGS) -o $@ $(objects) tokenparser.o

tokenparser.c: tokenparser.l
	$(SHELL) $(YLWRAP) $< lex.ccid.c $@ -- "$(LEX)" -Pccid

clean:
	rm -f $(objects)
	rm -f libccid.so
	rm -rf $(BUNDLE)
	rm -f ifd-ccid
	rm -f tokenparser.o tokenparser.c parse.o parse
	rm -f Makefile.bak
	rm -f config.log

distclean: clean
	rm -f .dependencies
	touch .dependencies
	rm -f tags
	rm -f tokenparser.c
	rm -f check_stamp dep_stamp

install: all
	mkdir -p $(BUNDLE)/Contents/$(BUNDLE_HOST)/
	cp libccid.so $(BUNDLE)/Contents/$(BUNDLE_HOST)/$(TARGET_LIB).$(version)
	sed s/VERSION/$(version)/ Info.plist | sed s/TARGET/$(TARGET_LIB)/ > $(BUNDLE)/Contents/Info.plist
	mkdir -p $(DESTDIR)/pcsc/drivers/
	cp -r $(BUNDLE) $(DESTDIR)/pcsc/drivers/

dep: dep_stamp
dep_stamp:
	@echo "Making dependencies..."
	makedepend -f - -I. -DPCSCLITE_MAX_CHANNELS=16 $(sources) > .dependencies || true
	touch dep_stamp

.dependencies: dep

ctags:
	ctags-exuberant *.h *.c

.PHONY: common clean distclean dep ctags

include .dependencies

