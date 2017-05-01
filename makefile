# Makefile for ne's distribution archive.

.PHONY: install

VERSION=3.1.0

# If you change this prefix, you can invoke "make build install" and ne will be compiled
# and installed under the $(PREFIX) hierarchy. You can even use "make install PREFIX=$HOME/<dir>"
# to install ne locally into the directory <dir>.

PREFIX=/usr/local

PROGRAM       = ne

ifeq ($(OS), Windows_NT)
	OS := Windows
else
	OS := $(shell uname -s)
endif


build: docs
	( cd src; make clean; make NE_GLOBAL_DIR=$(PREFIX)/share/ne; strip ne )

docs:
	( cd doc; make )

alldocs: docs
	( cd doc; make pdf )

version:
	./version.pl VERSION=$(VERSION)

source: version alldocs
	( cd src; make clean; make )
	-rm -f ne-$(VERSION)
	ln -s . ne-$(VERSION)
	tar cvf ne-$(VERSION).tar ne-$(VERSION)/version.pl ne-$(VERSION)/makefile ne-$(VERSION)/COPYING ne-$(VERSION)/INSTALL.md ne-$(VERSION)/README.md ne-$(VERSION)/NEWS ne-$(VERSION)/CHANGES \
	ne-$(VERSION)/src/*.[hc] ne-$(VERSION)/src/*.c.in ne-$(VERSION)/src/*.pl \
	ne-$(VERSION)/extensions \
	ne-$(VERSION)/macros/* \
	ne-$(VERSION)/syntax/*.jsf \
	ne-$(VERSION)/src/makefile ne-$(VERSION)/src/ne.texinfo ne-$(VERSION)/doc/ne.1 \
	ne-$(VERSION)/doc/makefile ne-$(VERSION)/doc/ne.texinfo ne-$(VERSION)/doc/ne.info* ne-$(VERSION)/doc/version.*  \
	ne-$(VERSION)/doc/html/*.html \
	ne-$(VERSION)/doc/ne.pdf ne-$(VERSION)/doc/ne.txt ne-$(VERSION)/doc/default*
	-rm -f ne-*.tar.gz
	gzip ne-$(VERSION).tar
	-rm -f ne-$(VERSION)

install:
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	mkdir -p $(DESTDIR)$(PREFIX)/share/ne/syntax
	mkdir -p $(DESTDIR)$(PREFIX)/share/ne/macros
	mkdir -p $(DESTDIR)$(PREFIX)/share/man/man1
	mkdir -p $(DESTDIR)$(PREFIX)/share/doc/ne
	mkdir -p $(DESTDIR)$(PREFIX)/share/info
	cp -pf src/ne$(CMDSUFFIX) $(DESTDIR)$(PREFIX)/bin
	cp -p extensions $(DESTDIR)$(PREFIX)/share/ne
	cp -p syntax/*.jsf $(DESTDIR)$(PREFIX)/share/ne/syntax
	cp -p macros/*     $(DESTDIR)$(PREFIX)/share/ne/macros
	cp -p doc/ne.1 $(DESTDIR)$(PREFIX)/share/man/man1
	cp -pR doc/html doc/ne.txt doc/default.* README.md COPYING NEWS CHANGES $(DESTDIR)$(PREFIX)/share/doc/ne
	if [ -f doc/ne.pdf ]; then cp -p doc/ne.pdf $(DESTDIR)$(PREFIX)/share/doc/ne ; fi
	cp -p doc/ne.info.gz $(DESTDIR)$(PREFIX)/share/info
	-install-info --dir-file=$(DESTDIR)$(PREFIX)/share/info/dir $(DESTDIR)$(PREFIX)/share/info/ne.info.gz

# Creates cygwin package on Windows from source tar.

cygwin:
ifneq ($(OS), Windows)
	$(error This target can only be run under Windows)
endif
	( cd src; make clean; make NE_GLOBAL_DIR=/usr/share/ne NE_TERMCAP=1 NE_ANSI=1 OPTS=-U__STRICT_ANSI__ )
	make install PREFIX=/usr CMDSUFFIX=.exe
	tar zcvf ne-cygwin-ansi-$(VERSION)-$(shell uname -m).tar.gz /usr/share/ne /usr/bin/ne.exe /usr/share/doc/ne /usr/share/info/ne.info.gz /usr/share/man/man1/ne.1
	( cd src; make clean; make NE_GLOBAL_DIR=/usr/share/ne OPTS=-U__STRICT_ANSI__ )
	make install PREFIX=/usr CMDSUFFIX=.exe
	tar zcvf ne-cygwin-$(VERSION)-$(shell uname -m).tar.gz /usr/share/ne /usr/bin/ne.exe /usr/share/doc/ne /usr/share/info/ne.info.gz /usr/share/man/man1/ne.1


# Creates Mac OS X .dmg from source tar.

macosx:
ifneq ($(OS), Darwin)
	$(error This target can only be run under Mac OS X)
endif
	( cd src; make clean; make NE_GLOBAL_DIR=/usr/share/ne; strip ne )
	-rm -fr /tmp/package-ne-$(VERSION)
	make install DESTDIR=/tmp/package-ne-$(VERSION)
	pkgbuild --root /tmp/package-ne-$(VERSION) --install-location "/" --version $(VERSION) --identifier ne-$(VERSION) ne-$(VERSION).pkg
	-rm -f ne-$(VERSION).dmg
	hdiutil create -fs HFS+ -srcfolder ne-$(VERSION).pkg -volname ne-$(VERSION) ne-$(VERSION).dmg


clean:
	-rm -f ne-*.tar*

really-clean: clean
	(cd src; make clean)
	(cd doc; make clean)
