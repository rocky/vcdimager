bin_PROGRAMS = vcdxbuild vcdxgen vcdxrip vcdxminfo
man_MANS = vcdxbuild.1 vcdxgen.1 vcdxrip.1 vcdxminfo.1

INCLUDES = -I$(top_srcdir) $(POPT_CFLAGS) $(LIBVCD_CFLAGS) $(XML_CFLAGS)

BUILT_SOURCES = videocd_dtd.inc
MOSTLYCLEANFILES = videocd_dtd.inc

DISTCLEANFILES = $(man_MANS)
EXTRA_DIST = videocd.dtd $(man_MANS)

Makefile: $(BUILT_SOURCES)

vcdxbuild_LDADD = $(XML_LIBS) $(LIBVCD_LIBS) $(POPT_LIBS)
vcdxbuild_SOURCES = \
	vcdxml.h \
	vcd_xml_build.c \
	vcd_xml_common.c \
	vcd_xml_common.h \
	vcd_xml_dtd.c \
	vcd_xml_dtd.h \
	vcd_xml_master.c \
	vcd_xml_master.h \
	vcd_xml_parse.c \
	vcd_xml_parse.h 

vcdxrip_LDADD = $(XML_LIBS) $(LIBVCD_LIBS) $(POPT_LIBS)
vcdxrip_SOURCES = \
	vcd_xml_rip.c \
	vcd_xml_common.c \
	vcd_xml_common.h \
	vcd_xml_dump.h \
	vcd_xml_dump.c \
	vcd_xml_dtd.h 

vcdxgen_LDADD = $(XML_LIBS) $(POPT_LIBS) $(LIBVCD_LIBS)
vcdxgen_SOURCES = \
	vcd_xml_gen.c \
	vcd_xml_common.c \
	vcd_xml_common.h \
	vcd_xml_dump.h \
	vcd_xml_dump.c \
	vcd_xml_dtd.h 

vcdxminfo_LDADD = $(XML_LIBS) $(POPT_LIBS) $(LIBVCD_LIBS)
vcdxminfo_SOURCES = \
	vcd_xml_common.c \
	vcd_xml_common.h \
	vcd_xml_minfo.c

#vcd_xml_dtd.c: videocd_dtd.inc

videocd_dtd.inc: $(srcdir)/videocd.dtd
	(sed 's/\"/\\"/g' | $(AWK) 'BEGIN { print "char videocd_dtd[] = \\" } { print " \"" $$0 "\\n\" \\" } END { print ";" }') < $< > $@

$(man_MANS): %.1: %
	-$(HELP2MAN) --output=$@ ./$<


