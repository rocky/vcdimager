#!/bin/sh
#$Id$

if test -z $srcdir ; then
  srcdir=`pwd`
fi

. ${srcdir}/check_common_fn
. ${srcdir}/check_vcddump_fn
. ${srcdir}/check_vcdimager_fn
. ${srcdir}/check_vcdxbuild_fn
. ${srcdir}/check_vcdxrip_fn

BASE=`basename $0 .sh`
RC=0

if test_vcdimager --type=vcd20 ${srcdir}/avseq00.m1p ; then
    :
else
    echo vcdimager failed 
    test_vcdimager_cleanup
    exit $RC
fi

if do_cksum <<EOF
2927361135 1764000 videocd.bin
3699460731 172 videocd.cue
EOF
    then
    :
else
    echo "$0: cksum(1) checksums didn't match :-("

    cksum videocd.bin videocd.cue

    test_vcdimager_cleanup
    exit 1
fi

echo "$0: vcdimager cksum(1) checksums matched :-)"

if test_vcddump '-B -i videocd.bin ' \
    vcd20_test0.dump ${srcdir}/vcd20_test0.right ; then 
    :
else
    echo "$0: vcddump test 0 failed "
    test_vcdxbuild_cleanup
    exit 1
fi

if test_vcdxbuild ${srcdir}/${BASE}.xml; then
 :
else 
    echo vcdxbuild failed 
    test_vcdxbuild_cleanup
    exit $RC
fi

if do_cksum <<EOF
1209563022 4840416 videocd.bin
2350689551 447 videocd.cue
EOF
    then
    :
else
    echo "$0: cksum(1) checksums didn't match :-("

    cksum videocd.bin videocd.cue

    test_vcdxbuild_cleanup
    exit 1
fi

echo "$0: vcdxbuild cksum(1) checksums matched :-)"

test_vcdxrip '--norip --bin-file videocd.bin -o vcd20_test1.xml' \
  vcd20_test1.xml ${srcdir}/vcd20_test1.xml-right
RC=$?
check_result $RC 'vcdxrip test 1'

test_vcddump '-B --bin-file videocd.bin ' \
    vcd20_test1.dump ${srcdir}/vcd20_test1.right
RC=$?
check_result $RC 'vcddump test 1'

test_vcddump '-B --bin-file videocd.bin -v -f -L -S --show-pvd vol' \
    vcd20_test2.dump ${srcdir}/vcd20_test2.right
RC=$?
check_result $RC 'vcddump test 2'

# if we got this far, everything should be ok
test_vcdxbuild_cleanup
exit 0

#;;; Local Variables: ***
#;;; mode:shell-script ***
#;;; eval: (sh-set-shell "bash") ***
#;;; End: ***
