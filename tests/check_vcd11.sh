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
    rm -vf core videocd.{bin,cue}
    echo "$0: md5sum checksum matched :-)"
    exit 0
fi

#md5sum -b videocd.{bin,cue}

echo "$0: md5sum checksums didn't match :-("
rm -vf core videocd.{bin,cue}
exit 1