#!/bin/sh

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

if ! ${VCDXBUILD} --check --file-prefix "${srcdir}/" ${srcdir}/check_vcd20.xml; then
    echo "$0: execution failed"
    rm -vf core videocd.{bin,cue}
    exit 1
fi

if md5sum -c <<EOF
82b5be9bd5005d349299c745d43c389d *videocd.bin
cc0d5303f3e58ef1ef708e705bb50559 *videocd.cue
EOF
 then
    echo "$0: md5sum checksum matched :-)"

    if ! test_vcddump '-B -i videocd.bin -p --show-entries prof ' \
      vcd20_test1.dump vcd20_test1.right ; then 
      echo "$0: vcddump test 1 failed "
      rm -vf core videocd.{bin,cue}
      exit 1
    fi
    
    if ! test_vcddump '-B --bin-file videocd.bin -v -f -L -S --show-pvd vol' \
      vcd20_test2.dump vcd20_test2.right ; then 
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