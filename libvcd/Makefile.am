# $Id$

noinst_LIBRARIES = libvcd.a libvcdinfo.a

INCLUDES = -I$(top_srcdir)

libvcd_a_SOURCES = \
	vcd.c \
	vcd.h \
	vcd_assert.h \
	vcd_bitvec.h \
	vcd_bytesex.c \
	vcd_bytesex.h \
	vcd_bytesex_asm.h \
	vcd_cd_sector.c \
	vcd_cd_sector.h \
	vcd_cd_sector_private.h \
	vcd_data_structures.c \
	vcd_data_structures.h \
	vcd_dict.h \
	vcd_directory.c \
	vcd_directory.h \
	vcd_files.c \
	vcd_files.h \
	vcd_files_private.h \
	vcd_image.c \
	vcd_image.h \
	vcd_image_fs.c \
	vcd_image_fs.h \
	vcd_image_bincue.c \
	vcd_image_bincue.h \
	vcd_image_cdrdao.c \
	vcd_image_cdrdao.h \
	vcd_image_nrg.c \
	vcd_image_nrg.h \
	vcd_image_cd.h \
	vcd_iso9660.c \
	vcd_iso9660.h \
	vcd_iso9660_private.h \
	vcd_logging.c \
	vcd_logging.h \
	vcd_mpeg.c \
	vcd_mpeg.h \
	vcd_mpeg_stream.c \
	vcd_mpeg_stream.h \
	vcd_obj.h \
	vcd_pbc.h \
	vcd_pbc.c \
	vcd_salloc.c \
	vcd_salloc.h \
	vcd_stream.c \
	vcd_stream.h \
	vcd_stream_stdio.c \
	vcd_stream_stdio.h \
	vcd_util.c \
	vcd_util.h \
	vcd_types.h \
	vcd_xa.h

# vcd_recorder related stuff below isn't used yet
EXTRA_libvcd_a_SOURCES = \
	vcd_image_cd-bsdi.c \
	vcd_image_cd-generic.c \
	vcd_image_cd-linux.c \
	vcd_image_cd-sunos.c \
	vcd_image_vpkt.c \
	vcd_image_vpkt.h \
	vcd_mmc_linux.c \
	vcd_mmc_linux.h \
	vcd_mmc_private.h \
	vcd_recorder.c \
	vcd_recorder.h \
	vcd_recorder_mmc.c \
	vcd_recorder_mmc.h

libvcd_a_LIBADD = @LIBVCD_SYSDEP@
libvcd_a_DEPENDENCIES = @LIBVCD_SYSDEP@

libvcdinfo_a_SOURCES = vcdinf.h vcdinf.c vcdinfo.h vcdinfo.c
