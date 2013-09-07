#
# This Makefile has the basic commands for converting
# ne.texinfo into a plain text file (ne.txt), an
# info document set (ne.info*), a set of HTML files (html/) and a PDF
# document (ne.pdf).
#

INFODOCS=ne.info.gz
INFODOCSNUM=ne.info-1.gz ne.info-2.gz ne.info-3.gz ne.info-4.gz

DOCS=$(INFODOCS) ne.txt html/index.html ne.dvi ne.pdf

all: $(DOCS)

install: all
	cp * ../../../doc

version.texinfo:
	( cd .. ; make version )

ne.txt: ne.texinfo version.texinfo
	makeinfo --plaintext --no-headers ne.texinfo -o ne.txt
	sed -i -e "s/\`/'/g" ne.txt
	sed -i -e "s/''''/'\`''/g" ne.txt
	sed -i -e "s/'ne'/ne/g" ne.txt

$(INFODOCS): ne.texinfo version.texinfo
	makeinfo ne.texinfo
	sed -i -e "s/\`/'/g" ne.info
	sed -i -e "s/''''/'\`''/g" ne.info
	sed -i -e "s/'ne'/ne/g" ne.info
	rm -f ne.info*gz
	gzip -9 ne.info*

ne.dvi: ne.texinfo version.texinfo
	texi2dvi --clean ne.texinfo

ne.pdf: ne.dvi
	dvipdf ne.dvi

html/index.html: ne.texinfo version.texinfo
	-rm -fr html
	makeinfo --html -o html ne.texinfo
	for i in html/*html; do sed -i -e "s/\`/'/g" $$i; done
	for i in html/*html; do sed -i -e "s/''''/'\`''/g" $$i; done
	for i in html/*html; do sed -i -e "s/'ne'/ne/g" $$i; done

clean:
	rm -f ne.txt ne.info* ne.ps ne.pdf ne.dvi
	rm -rf html/
