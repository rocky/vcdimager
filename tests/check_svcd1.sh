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

if ! ${VCDXBUILD} --check --file-prefix "${srcdir}/" ${srcdir}/check_svcd1.xml; then
    echo "$0: execution failed"
    rm -vf core videocd.{bin,cue}
    exit 1
fi

if md5sum -c <<EOF
7ea58abea20fe1daa68b85ebb830e013 *videocd.bin
a66be7dca7cbd2ed03b782cf4dcd66ee *videocd.cue
EOF
 then
    echo "$0: md5sum checksum matched :-)"

    if ! test_vcddump '--no-banner -i videocd.bin' \
      svcd1_test1.dump svcd1_test1.right ; then 
      echo "$0: vcddump test 1 failed "
      exit 1
    fi
    
    if ! test_vcddump '--no-banner --bin-file videocd.bin --show-info-all' \
      svcd1_test2.dump svcd1_test2.right ; then 
      echo "$0: vcddump test 2 failed "
      exit 1
    fi
    
    if ! ${VCDXBUILD} --check --file-prefix "${srcdir}/" ${srcdir}/check_svcd1.xml; then
      echo "$0: execution failed"
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