noinst_PROGRAMS = mpegscan mpegscan2 testassert testischar testimage

INCLUDES = -I$(top_srcdir) $(POPT_CFLAGS) $(LIBVCD_CFLAGS)

mpegscan_LDADD = $(LIBVCD_LIBS)
mpegscan2_LDADD = $(LIBVCD_LIBS)
testassert_LDADD = $(LIBVCD_LIBS)
testischar_LDADD = $(LIBVCD_LIBS)
testimage_LDADD = $(LIBVCD_LIBS)
check_sizeof_LDADD = $(LIBVCD_LIBS)
check_bitfield_LDADD = $(LIBVCD_LIBS)

# make check targets

check_PROGRAMS = check_sizeof check_bitfield

check_SCRIPTS = check_vcd11.sh check_vcd20.sh check_svcd1.sh

check_DATA = avseq00.m1p item0000.m1p \
	check_vcd11.xml check_vcd20.xml check_svcd1.xml \
	vcd11_test0.right vcd20_test0.right svcd1_test0.right \
	vcd11_test1.right vcd20_test1.right svcd1_test1.right \
	vcd11_test2.right vcd20_test2.right svcd1_test2.right \
	vcd11_test1.xml-right vcd20_test1.xml-right svcd1_test1.xml-right

EXTRA_DIST = $(check_SCRIPTS) $(check_DATA) \
	check_common_fn check_vcddump_fn check_vcdimager_fn \
	check_vcdxbuild_fn check_vcdxrip_fn

TESTS = \
	check_sizeof \
	check_bitfield \
	check_vcd11.sh \
	check_vcd20.sh \
	check_svcd1.sh
