# $Id$

all:
	$(MAKE) -C src $@

clean:
	$(MAKE) -C src $@

distclean:
	$(MAKE) -C src $@

install:
	$(MAKE) -C src $@

.PHONY: all clean distclean install

