#!/bin/sh
#$Id$

if test -z $srcdir ; then
  srcdir=`pwd`
fi

. ${srcdir}/check_common_fn
. ${srcdir}/check_vcddump_fn
. ${srcdir}/check_vcdimager_fn
. ${srcdir}/check_vcdxbuild_fn

BASE=`basename $0 .sh`
RC=0

if test_vcdimager -t svcd ${srcdir}/avseq00.m1p; then
    :
else
    echo vcdimager failed 
    test_vcdimager_cleanup
    exit $RC
fi

if do_cksum <<EOF
2858408396 1587600 videocd.bin
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

if test_vcddump '--no-banner -i videocd.bin' \
    svcd1_test0.dump ${srcdir}/svcd1_test0.right ; then 
    :
else
    echo "$0: vcddump test 0 failed "
    test_vcdxbuild_cleanup
    exit 1
fi

if test_vcdxbuild ${srcdir}/$BASE.xml; then
    :
else
    echo vcdxbuild failed 
    test_vcdxbuild_cleanup
    exit $RC
fi

if do_cksum <<EOF
4104676060 4059552 videocd.bin
669873986 424 videocd.cue
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

if test_vcddump '--no-banner -i videocd.bin' \
    svcd1_test1.dump ${srcdir}/svcd1_test1.right ; then 
    :
else
    echo "$0: vcddump test 1 failed "
    test_vcdxbuild_cleanup
    exit 1
fi

if test_vcddump '--no-banner --bin-file videocd.bin --show-info-all' \
    svcd1_test2.dump ${srcdir}/svcd1_test2.right ; then 
    :
else
    echo "$0: vcddump test 2 failed "
    test_vcdxbuild_cleanup
    exit 1
fi

# if we got this far, everything should be ok
test_vcdxbuild_cleanup
exit 0

#;;; Local Variables: ***
#;;; mode:shell-script ***
#;;; eval: (sh-set-shell "bash") ***
#;;; End: ***
