SUBDIRS = libvcd frontends 

EXTRA_DIST = BUGS FAQ cygwin-dist.sh

dist-hook: vcdimager.spec
	cp vcdimager.spec $(distdir)

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

cygwin-dist:
	./cygwin-dist.sh $(VERSION)
