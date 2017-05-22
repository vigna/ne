#
# This Makefile has the basic commands for converting
# ne.texinfo into a plain text file (ne.txt), an
# info document set (ne.info*), a set of HTML files (html/) and a PDF
# document (ne.pdf).
#

DOCS=ne.info.gz ne.txt html/index.html

docs: $(DOCS)

pdf: ne.pdf

install: docs
	cp * ../../../doc

version.texinfo:
	( cd .. ; make version )

ne.txt: ne.texinfo version.texinfo
	(makeinfo --plaintext --no-headers ne.texinfo -o ne.txt || makeinfo --no-headers ne.texinfo -o ne.txt)
	sed -i -e "s/\`/'/g" ne.txt
	sed -i -e "s/''''/'\`''/g" ne.txt
	sed -i -e "s/'ne'/ne/g" ne.txt

ne.info.gz: ne.texinfo version.texinfo
	makeinfo ne.texinfo
	sed -i -e "s/\`/'/g" ne.info
	sed -i -e "s/''''/'\`''/g" ne.info
	sed -i -e "s/'ne'/ne/g" ne.info
	rm -f ne.info*gz
	gzip -9 ne.info*

ne.pdf: ne.texinfo version.texinfo
	pdftex ne.texinfo

html/index.html: ne.texinfo version.texinfo
	-rm -fr html
	makeinfo --html -o html ne.texinfo

clean:
	rm -f ne.txt ne.info* ne.ps ne.pdf ne.aux ne.cms ne.cps ne.fn ne.log ne.tp ne.cm ne.cp ne.dvi ne.ky ne.pg ne.toc ne.vr
	rm -rf html/


