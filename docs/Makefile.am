info_TEXINFOS = vcdimager.texi vcddump.texi
vcdimager_TEXINFOS = fdl.texi gpl.texi glossary.texi

pdf: vcdimager.pdf vcddump.pdf

txt: vcdimager.txt vcddump.txt

ps: vcdimager.ps vcddump.ps

html: vcdimager.html vcddump.html 

%.ps.gz: %.ps
	gzip -9c $< > $@

.texi.pdf:
	texi2pdf $<

.texi.html:
	texi2html $<

.texi.txt:
	makeinfo --no-headers $< > $@

all-formats: pdf dvi txt ps html

MOSTLYCLEANFILES = vcdimager.tgs vcdimager.ps.gz vcdimager.pdf vcdimager.html vcdimager_toc.html vcdimager_foot.html vcddump.cps vcddump.kys vcddump.html vcddump.fns vcddump.ps.gz vcddump.pdf
