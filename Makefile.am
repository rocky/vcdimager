SUBDIRS = libvcd frontends debug

EXTRA_DIST = BUGS FAQ cygwin-dist.sh debian HACKING

dist-hook: vcdimager.spec
	cp vcdimager.spec $(distdir)
	find $(distdir) -name 'CVS' -type d | xargs rm -vrf
	find $(distdir) -name '*~' -type f | xargs rm -vf
	find $(distdir) -name '.cvsignore' -type f | xargs rm -vf

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

cygwin-dist:
	./cygwin-dist.sh $(VERSION)
