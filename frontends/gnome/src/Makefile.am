INCLUDES = $(GNOMEMM_CFLAGS) $(LIBGLADE_CFLAGS) $(LIBVCD_CFLAGS) -I$(top_srcdir)

noinst_PROGRAMS = gvcdimager

gvcdimager_SOURCES = \
	main.cc \
	GVCDImager.hh \
	GVCDImagerApp.cc \
	GVCDImagerApp.hh \
	GladeHelper.hh \
	LibvcdWrapper.hh

gvcdimager_LDADD = $(GNOMEMM_LIBS) $(LIBGLADE_LIBS) $(LIBVCD_LIBS)
