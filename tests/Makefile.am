noinst_PROGRAMS = mpegscan mpegscan2 testassert testischar testimage

INCLUDES = -I$(top_srcdir) $(POPT_CFLAGS) $(LIBVCD_CFLAGS)

mpegscan_LDADD = $(LIBVCD_LIBS)

mpegscan2_LDADD = $(LIBVCD_LIBS)

testassert_LDADD = $(LIBVCD_LIBS)

testischar_LDADD = $(LIBVCD_LIBS)

testimage_LDADD = $(LIBVCD_LIBS)

# make check targets

check_PROGRAMS = check_sizeof check_bitfield

check_SCRIPTS = check_vcd11.sh check_vcd20.sh check_svcd1.sh

EXTRA_DIST = avseq00.m1p item0000.m1p check_vcd11.xml check_vcd20.xml check_svcd1.xml

TESTS = \
	check_sizeof \
	check_bitfield \
	check_vcd11.sh \
	check_vcd20.sh \
	check_svcd1.sh