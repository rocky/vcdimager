bin_PROGRAMS = vcdimager vcdrip

INCLUDES = -I $(top_srcdir)/libvcd/ $(GLIB_CFLAGS) $(CDI_CFLAGS)

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD = $(VCD_LIBS) $(POPT_LIBS)

vcdrip_SOURCES = vcdrip.c
vcdrip_LDADD = $(VCD_LIBS) $(POPT_LIBS)

info_TEXINFOS = vcdimager.texi

man_MANS = vcdimager.1

EXTRA_DIST = $(man_MANS)
