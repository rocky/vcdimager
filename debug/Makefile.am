noinst_PROGRAMS = mpegscan

INCLUDES = $(POPT_INCLUDES) -I$(top_srcdir) -I$(top_srcdir)/libvcd/ 

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

mpegscan_LDADD = $(VCD_LIBS)
mpegscan_SOURCES = mpegscan.c
