if BUILD_CLI_FE_POPT
POPT_SUBDIR = popt
POPT_INCLUDES = -I$(srcdir)/popt/
POPT_LIBS = popt/libpopt.a
else
POPT_LIBS = -lpopt
endif

bin_PROGRAMS = vcdimager vcdrip

SUBDIRS = $(POPT_SUBDIR)

INCLUDES = $(POPT_INCLUDES) -I $(top_srcdir)/libvcd/ 

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD = $(VCD_LIBS) $(POPT_LIBS)

vcdrip_SOURCES = vcdrip.c
vcdrip_LDADD = $(VCD_LIBS) $(POPT_LIBS)

info_TEXINFOS = vcdimager.texi

man_MANS = vcdimager.1 vcdrip.1

EXTRA_DIST = $(man_MANS)
