POPT_LIBS = -lpopt

bin_PROGRAMS = vcdxml 
#vcdrestore

INCLUDES = -I$(top_srcdir) $(POPT_INCLUDES) $(LIBVCD_CFLAGS) $(XML_CFLAGS)

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

EXTRA_DIST = videocd.dtd
BUILT_SOURCES = videocd_dtd.h
#DISTCLEANFILES = videocd_dtd.h

vcdxml_LDADD = $(XML_LIBS) $(VCD_LIBS) $(POPT_LIBS)
vcdxml_SOURCES = \
	vcdxml.c \
	vcdxml.h \
	vcd_xml_parse.h \
	vcd_xml_parse.c \
	vcd_xml_master.c \
	vcd_xml_master.h

#vcdrestore_LDADD = $(XML_LIBS) $(VCD_LIBS) $(POPT_LIBS)
#vcdrestore_SOURCES = \
#	vcdrestore.c \
#	vcdxml.h 

videocd_dtd.h: $(srcdir)/videocd.dtd
	(sed 's/\"/\\"/g' | gawk 'BEGIN { print "static char videocd_dtd[] = \\" } { print " \"" $$0 "\\n\" \\" } END { print ";" }') < $< > $@

clean-local:
	-rm -f *.ps
