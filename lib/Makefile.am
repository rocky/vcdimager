# $Id$

lib_LTLIBRARIES = libvcd.la libvcdinfo.la

INCLUDES = $(LIBVCD_CFLAGS) $(LIBCDIO_CFLAGS)

libvcd_la_LIBADD = $(LIBCDIO_LIBS) $(LIBISO9660_LIBS)
libvcd_la_SOURCES = \
	vcd_assert.h \
	bitvec.h \
	bytesex_asm.h \
	bytesex.h \
	data_structures.h \
	dict.h \
	directory.h \
	image_sink.h \
	mpeg.h \
	mpeg_stream.h \
	obj.h \
	pbc.h \
	salloc.h \
	sector_private.h \
	stream.h \
	stream_stdio.h \
	util.h \
	vcd.h \
	vcd.c \
	data_structures.c \
	directory.c \
	files.c \
	image.c \
	image_bincue.c \
	image_cdrdao.c \
	image_nrg.c \
	logging.c \
	mpeg.c \
	mpeg_stream.c \
	pbc.c \
	salloc.c \
	sector.c \
	stream.c \
	stream_stdio.c \
	util.c 

libvcdinfo_la_SOURCES = \
	info.c \
	inf.c \
	info_private.h \
	info_private.c \
	vcd_read.c \
	vcd_read.h 

libvcdinfo_la_LDFLAGS = $(LIBCDIO_LIBS) $(LIBISO9660_LIBS) -no-undefined -version-info 0:1:0

# EOF
