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

test_vcdimager -t svcd ${srcdir}/avseq00.m1p
RC=$?

if test $RC -ne 0 ; then
  if test $RC -ne 77 ; then 
    echo vcdimager failed 
    exit $RC
  else
    echo vcdimager skipped
    test_vcdimager_cleanup
  fi
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

    exit 1
fi

echo "$0: vcdimager cksum(1) checksums matched :-)"

test_vcddump '--no-banner -i videocd.bin' \
    svcd1_test0.dump ${srcdir}/svcd1_test0.right
RC=$?
check_result $RC 'vcddump test 0'

test_vcdxbuild ${srcdir}/$BASE.xml
RC=$?
if test $RC -ne 0 ; then
  echo vcdxbuild failed 
  if test $RC -eq 77 ; then
    test_vcdxbuild_cleanup
  fi
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

test_vcdxrip '--norip -b videocd.bin --output-file svcd1_test1.xml' \
  svcd1_test1.xml ${srcdir}/svcd1_test1.xml-right
RC=$?
check_result $RC 'vcdxrip test 1'

test_vcddump '--no-banner -i videocd.bin' \
    svcd1_test1.dump ${srcdir}/svcd1_test1.right
RC=$?
check_result $RC 'vcddump test 1'

test_vcddump '--no-banner --bin-file videocd.bin --show-info-all' \
    svcd1_test2.dump ${srcdir}/svcd1_test2.right 
RC=$?
check_result $RC 'vcddump test 2'

# if we got this far, everything should be ok
test_vcdxbuild_cleanup
exit $RC

#;;; Local Variables: ***
#;;; mode:shell-script ***
#;;; eval: (sh-set-shell "bash") ***
#;;; End: ***
