if BUILD_CLI_FE_POPT
POPT_SUBDIR = popt
endif

bin_PROGRAMS = vcdimager vcddebug cdxa2mpeg
man_MANS = vcdimager.1 vcddebug.1
#man_MANS = $(patsubst %,%.1,$(bin_PROGRAMS))
DISTCLEANFILES = $(man_MANS)

SUBDIRS = $(POPT_SUBDIR)

INCLUDES = -I$(top_srcdir) $(POPT_CFLAGS) $(LIBVCD_CFLAGS)

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD = $(LIBVCD_LIBS) $(POPT_LIBS)

vcddebug_SOURCES = vcd_debug.c
vcddebug_LDADD = $(LIBVCD_LIBS) $(POPT_LIBS)

cdxa2mpeg_SOURCES = cdxa2mpeg.c
cdxa2mpeg_LDADD = $(LIBVCD_LIBS)

$(man_MANS): %.1: %
	-$(HELP2MAN) --output=$@ ./$<

EXTRA_DIST = $(man_MANS)
