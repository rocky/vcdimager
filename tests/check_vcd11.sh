#!/bin/sh
#$Id$

#echo ${srcdir}

. ${srcdir}/check_vcddump_fn

VCDXBUILD="../frontends/xml/vcdxbuild"

if ! md5sum < /dev/null > /dev/null; then
    echo "$0: md5sum not found, check not possible";
    exit 77
fi

if [ ! -x "${VCDXBUILD}" ]; then
    echo "$0: ${VCDXBUILD} missing, check not possible"
    exit 77
fi

if ! ${VCDXBUILD} --check --file-prefix "${srcdir}/" ${srcdir}/check_vcd11.xml; then
    echo "$0: execution failed"
    rm -vf core videocd.{bin,cue}
    exit 1
fi

if md5sum -c <<EOF
98fa8b9348f94cd8676ccfcb411af5d1 *videocd.bin
55e5ab512c130f06ea525e267626f4d4 *videocd.cue
EOF
 then
    echo "$0: md5sum checksum matched :-)"

    if ! test_vcddump '-B -i videocd.bin -I --show-filesystem' \
      vcd11_test1.dump ${srcdir}/vcd11_test1.right ; then 
      echo "$0: vcddump test 1 failed "
      rm -vf core videocd.{bin,cue}
      exit 1
    fi
    
    if ! test_vcddump '-B --bin-file videocd.bin -X -S --show-pvd vol' \
      vcd11_test2.dump ${srcdir}/vcd11_test2.right ; then 
      echo "$0: vcddump test 2 failed "
      rm -vf core videocd.{bin,cue}
      exit 1
    fi
    
    rm -vf core videocd.{bin,cue}
    exit 0
fi

#md5sum -b videocd.{bin,cue}

echo "$0: md5sum checksums didn't match :-("
rm -vf core videocd.{bin,cue}
exit 1

#;;; Local Variables: ***
#;;; mode:shell-script ***
#;;; eval: (sh-set-shell "bash") ***
#;;; End: ***
