bin_PROGRAMS = vcdimager cdxa2mpeg vcd-info
man_MANS = vcdimager.1 cdxa2mpeg.1 vcd-info.1
MAINTAINERCLEANFILES = $(man_MANS)

INCLUDES = -I$(top_srcdir) $(LIBPOPT_CFLAGS) $(LIBVCD_CFLAGS) $(LIBCDIO_CFLAGS) $(LIBISO9660_CFLAGS)

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD =  $(LIBISO9660_LIBS) $(LIBVCD_LIBS) $(LIBPOPT_LIBS) $(LIBCDIO_LIBS)

cdxa2mpeg_SOURCES = cdxa2mpeg.c
cdxa2mpeg_LDADD =  $(LIBISO9660_LIBS) $(LIBVCD_LIBS) $(LIBPOPT_LIBS)

vcd_info_SOURCES = vcd-info.c
vcd_info_LDADD = $(LIBISO9660_LIBS) $(LIBVCDINFO_LIBS) $(LIBVCD_LIBS) $(LIBPOPT_LIBS) $(LIBCDIO_LIBS) $(LIBISO9660_LIBS)

if MAINTAINER_MODE
$(man_MANS): %.1: %
	-$(HELP2MAN) --output=$@ ./$<
endif

EXTRA_DIST = $(man_MANS)
