POPT_LIBS = -lpopt

bin_PROGRAMS = vcdxml

INCLUDES = $(POPT_INCLUDES) $(LIBVCD_CFLAGS) $(LIBXML2_CFLAGS)

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

vcdxml_SOURCES = vcdxml.c
vcdxml_LDADD = $(LIBXML2_LIBS) $(VCD_LIBS) $(POPT_LIBS)

videocd_dtd.h: videocd.dtd
	(sed 's/\"/\\"/g' | gawk 'BEGIN { print "static char videocd_dtd[] = \\" } { print " \"" $$0 "\\n\" \\" } END { print ";" }') < videocd.dtd > videocd_dtd.h

clean-local:
	-rm -f *.ps
