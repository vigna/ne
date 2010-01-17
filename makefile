# Makefile for ne's distribution archive.

VERSION = 2.0.4-pre2

# If you change this prefix, you can call "make install" and ne will be compiled
# and installed under the $(PREFIX) hierarchy. You can even use "make install PREFIX=$HOME/<dir>"
# to install ne locally into the directory <dir>.

PREFIX=/usr/local

PROGRAM       = ne

source:
	( cd doc; make )
	( cd src; make clean; make )
	-rm -f ne-$(VERSION)
	ln -s . ne-$(VERSION)
	tar cvf ne-$(VERSION).tar ne-$(VERSION)/makefile ne-$(VERSION)/COPYING ne-$(VERSION)/README ne-$(VERSION)/CHANGES \
	ne-$(VERSION)/src/*.[hc] ne-$(VERSION)/src/*.c.in ne-$(VERSION)/src/*.pl \
	ne-$(VERSION)/syntax/*.jsf \
	ne-$(VERSION)/src/makefile ne-$(VERSION)/src/makefile.amiga ne-$(VERSION)/src/ne.texinfo ne-$(VERSION)/doc/ne.1 \
	ne-$(VERSION)/doc/makefile ne-$(VERSION)/doc/ne.texinfo ne-$(VERSION)/doc/ne.info*  \
	ne-$(VERSION)/doc/ne/*.html \
	ne-$(VERSION)/doc/ne.pdf ne-$(VERSION)/doc/ne.txt ne-$(VERSION)/doc/default*
	-rm -f ne-*.tar.gz
	gzip ne-$(VERSION).tar
	-rm -f ne-$(VERSION)

cygwin:
	( cd doc; make )
	( cd src; make clean; make NE_GLOBAL_DIR=/usr/lib/ne )
	make install PREFIX=/usr
	tar zcvf ne-cygwin-$(VERSION).tar.gz /usr/lib/ne /usr/bin/ne.exe /usr/share/doc/ne /usr/share/info/ne.info.gz /usr/share/man/man1/ne.1
	( cd src; make clean; make NE_GLOBAL_DIR=/usr/lib/ne NE_TERMCAP=1 NE_ANSI=1 )
	tar zcvf ne-cygwin-ansi-$(VERSION).tar.gz /usr/lib/ne /usr/bin/ne.exe /usr/share/doc/ne /usr/share/info/ne.info.gz /usr/share/man/man1/ne.1

install:
	(cd src; make NE_GLOBAL_DIR=$(PREFIX)/share/ne)
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/ne/syntax
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/ne
	mkdir -p $(DESTDIR)$(PREFIX)/share/info
	cp -p src/ne $(DESTDIR)$(PREFIX)/bin
	cp -p syntax/*.jsf $(DESTDIR)$(PREFIX)/share/ne/syntax
	cp -p doc/ne.1 $(DESTDIR)$(PREFIX)/share/man/man1
	cp -pr doc/ne.pdf doc/ne doc/ne.txt doc/default.* README COPYING CHANGES $(DESTDIR)$(PREFIX)/share/doc/ne
	cp -p doc/ne.info.gz $(DESTDIR)$(PREFIX)/share/info
	-install-info --dir-file=$(DESTDIR)$(PREFIX)/share/info/dir $(DESTDIR)$(PREFIX)/share/info/ne.info.gz

clean:
	-rm ne-*.tar*
