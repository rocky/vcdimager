if BUILD_CLI_FE_POPT
POPT_SUBDIR = popt
POPT_INCLUDES = -I$(srcdir)/popt/
POPT_LIBS = popt/libpopt.a
else
POPT_LIBS = -lpopt
endif

bin_PROGRAMS = vcdimager vcddebug

SUBDIRS = $(POPT_SUBDIR)

INCLUDES = $(POPT_INCLUDES) -I$(top_srcdir) -I$(top_srcdir)/libvcd/ 

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

vcdimager_SOURCES = vcdimager.c
vcdimager_LDADD = $(VCD_LIBS) $(POPT_LIBS)

vcddebug_SOURCES = vcd_debug.c
vcddebug_LDADD = $(VCD_LIBS) $(POPT_LIBS)
