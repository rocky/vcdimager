#!/bin/sh

#echo ${srcdir}
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
    rm -vf core videocd.{bin,cue}
    echo "$0: md5sum checksum matched :-)"
    exit 0
fi

#md5sum -b videocd.{bin,cue}

echo "$0: md5sum checksums didn't match :-("
rm -vf core videocd.{bin,cue}
exit 1