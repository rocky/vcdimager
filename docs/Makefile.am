info_TEXINFOS = vcdimager.texi vcddump.texi
vcdimager_TEXINFOS = fdl.texi gpl.texi glossary.texi

vcdimager.ps.gz: vcdimager.ps
	gzip -9c $< > $@

vcdimager.pdf: vcdimager.texi
	texi2pdf $<

pdf: vcdimager.pdf

.texi.html:
	texi2html $<

.texi.txt:
	makeinfo --no-headers $< > $@

all-formats: vcdimager.txt vcdimager.html vcdimager.dvi vcdimager.ps vcdimager.pdf vcddump.dvi vcddump.html 

MOSTLYCLEANFILES = vcdimager.tgs vcdimager.ps.gz vcdimager.pdf vcdimager.html vcdimager_toc.html vcdimager_foot.html vcddump.cps vcddump.kys vcddump.html vcddump.fns
