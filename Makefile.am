SUBDIRS = libvcd frontends 

dist-hook: vcdimager.spec
	cp vcdimager.spec $(distdir)

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`
