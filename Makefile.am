SUBDIRS = libvcd frontends tests docs

EXTRA_DIST = BUGS FAQ cygwin-dist.sh debian amiga HACKING

dist-hook: vcdimager.spec
	cp vcdimager.spec $(distdir)
	find $(distdir) -name 'CVS' -type d | xargs rm -vrf
	find $(distdir) -name '*~' -type f | xargs rm -vf
	find $(distdir) -name '.cvsignore' -type f | xargs rm -vf
	find $(distdir) -name '.nfs*' -type f | xargs rm -vf

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

tar-dist:
	here=`pwd`; \
	ipath="$$here/install-tmp/"; \
	rm -rf "$$ipath"; \
	$(MAKE) install-strip prefix="$$ipath/usr/local/"; \
	$(AMTAR) -C "$$ipath" -cvzf $(PACKAGE)-$(VERSION)-$(host_triplet).tar.gz usr

if CYGWIN
cygwin-dist:
	./cygwin-dist.sh $(VERSION)
endif
