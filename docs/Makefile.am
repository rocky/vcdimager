info_TEXINFOS = vcdimager.texi
vcdimager_TEXINFOS = fdl.texi gpl.texi glossary.texi

vcdimager.ps.gz: vcdimager.ps
	gzip -9c $< > $@

vcdimager.pdf: vcdimager.texi
	texi2pdf $<

pdf: vcdimager.pdf

vcdimager.html: vcdimager.texi
	texi2html $<

vcdimager.txt: vcdimager.texi
	makeinfo --no-headers $< > $@

all-formats: vcdimager.txt vcdimager.html vcdimager.dvi vcdimager.ps vcdimager.pdf

MOSTLYCLEANFILES = vcdimager.tgs vcdimager.ps.gz vcdimager.pdf vcdimager.html vcdimager_toc.html vcdimager_foot.html
