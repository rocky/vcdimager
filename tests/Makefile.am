noinst_PROGRAMS = mpegscan testassert testischar testimage

INCLUDES = $(POPT_INCLUDES) -I$(top_srcdir) -I$(top_srcdir)/libvcd/ 

VCD_LIBS = $(top_builddir)/libvcd/libvcd.a

mpegscan_LDADD = $(VCD_LIBS)

testassert_LDADD = $(VCD_LIBS)

testischar_LDADD = $(VCD_LIBS)

testimage_LDADD = $(VCD_LIBS)
