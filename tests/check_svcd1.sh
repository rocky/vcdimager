#!/bin/sh
#$Id$

. ${srcdir}/check_common_fn
. ${srcdir}/check_vcddump_fn
. ${srcdir}/check_vcdxbuild_fn

BASE=`basename $0 .sh`
RC=0

if ! test_vcdxbuild ${srcdir}/$BASE.xml; then
    echo vcdxbuild failed 
    test_vcdxbuild_cleanup
    exit $RC
fi

if ! do_cksum <<EOF
4104676060 4059552 videocd.bin
669873986 424 videocd.cue
EOF
    then
    echo "$0: cksum(1) checksums didn't match :-("

    cksum videocd.bin videocd.cue

    test_vcdxbuild_cleanup
    exit 1
fi

echo "$0: cksum(1) checksums matched :-)"

if ! test_vcddump '--no-banner -i videocd.bin' \
    svcd1_test1.dump ${srcdir}/svcd1_test1.right ; then 
    echo "$0: vcddump test 1 failed "
    test_vcdxbuild_cleanup
    exit 1
fi

if ! test_vcddump '--no-banner --bin-file videocd.bin --show-info-all' \
    svcd1_test2.dump ${srcdir}/svcd1_test2.right ; then 
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
