SUBDIRS = libvcd frontends tests amiga docs

EXTRA_DIST = BUGS FAQ cygwin-dist.sh HACKING libpopt.m4 \
	debian/changelog debian/control debian/copyright debian/docs debian/info debian/rules debian/watch

dist-hook: vcdimager.spec
	cp vcdimager.spec $(distdir)

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

bintar-dist:
	here=`pwd`; \
	ipath="$$here/install-tmp/"; \
	rm -rf "$$ipath"; \
	$(MAKE) install-strip prefix="$$ipath/usr/local/"; \
	$(AMTAR) -C "$$ipath" -cvzf $(PACKAGE)-$(VERSION)-$(host_triplet).tar.gz usr

if CYGWIN
cygwin-dist:
	./cygwin-dist.sh $(VERSION)
endif

# cvs2cl
MAINTAINERCLEANFILES = ChangeLog

if MAINTAINER_MODE
ChangeLog:
	$(srcdir)/cvs2cl.pl -W 450 --header $(srcdir)/cvs2cl_header --utc -w -I ChangeLog --usermap $(srcdir)/cvs2cl_usermap
endif

