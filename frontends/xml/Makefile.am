if BUILD_CLI_FE_POPT
POPT_SUBDIR = popt
POPT_INCLUDES = -I$(srcdir)/../cli/popt/
POPT_LIBS = ../cli/popt/libpopt.a
else
POPT_LIBS = -lpopt
endif

bin_PROGRAMS = vcdxbuild vcdxgen vcdxrip vcdxminfo

INCLUDES = -I$(top_srcdir) $(POPT_INCLUDES) $(LIBVCD_CFLAGS) $(XML_CFLAGS)

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

EXTRA_DIST = videocd.dtd
BUILT_SOURCES = videocd_dtd.inc
MOSTLYCLEANFILES = videocd_dtd.inc

Makefile: $(BUILT_SOURCES)

vcdxbuild_LDADD = $(XML_LIBS) $(VCD_LIBS) $(POPT_LIBS)
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

vcdxrip_LDADD = $(XML_LIBS) $(VCD_LIBS) $(POPT_LIBS)
vcdxrip_SOURCES = \
	vcd_xml_rip.c \
	vcd_xml_dump.h \
	vcd_xml_dump.c \
	vcd_xml_dtd.h 

vcdxgen_LDADD = $(XML_LIBS) $(POPT_LIBS) $(VCD_LIBS)
vcdxgen_SOURCES = \
	vcd_xml_gen.c \
	vcd_xml_dump.h \
	vcd_xml_dump.c \
	vcd_xml_dtd.h 

vcdxminfo_LDADD = $(XML_LIBS) $(POPT_LIBS) $(VCD_LIBS)
vcdxminfo_SOURCES = \
	vcd_xml_minfo.c

#vcd_xml_dtd.c: videocd_dtd.inc

videocd_dtd.inc: $(srcdir)/videocd.dtd
	(sed 's/\"/\\"/g' | $(AWK) 'BEGIN { print "char videocd_dtd[] = \\" } { print " \"" $$0 "\\n\" \\" } END { print ";" }') < $< > $@

clean-local:
	-rm -f *.ps
