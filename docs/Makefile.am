info_TEXINFOS = vcdimager.texi
vcdimager_TEXINFOS = gpl.texi glossary.texi

vcdimager.ps.gz: vcdimager.ps
	gzip -9c $< > $@

vcdimager.pdf: vcdimager.texi
	texi2pdf $<

vcdimager.html: vcdimager.texi
	texi2html $<

man_MANS = vcdimager.1

EXTRA_DIST = $(man_MANS)

clean-local:
	-rm -f vcdimager.ps.gz vcdimager.pdf vcdimager.html vcdimager_toc.html vcdimager_foot.html
