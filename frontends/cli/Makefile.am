bin_PROGRAMS = vcdimager vcddebug cdxa2mpeg
man_MANS = vcdimager.1 vcddebug.1
#man_MANS = $(patsubst %,%.1,$(bin_PROGRAMS))
DISTCLEANFILES = $(man_MANS)

INCLUDES = -I$(top_srcdir) $(LIBPOPT_CFLAGS) $(LIBVCD_CFLAGS)

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD = $(LIBVCD_LIBS) $(LIBPOPT_LIBS)

vcddebug_SOURCES = vcd_debug.c
vcddebug_LDADD = $(LIBVCD_LIBS) $(LIBPOPT_LIBS)

cdxa2mpeg_SOURCES = cdxa2mpeg.c
cdxa2mpeg_LDADD = $(LIBVCD_LIBS)

$(man_MANS): %.1: %
	-$(HELP2MAN) --output=$@ ./$<

EXTRA_DIST = $(man_MANS)
