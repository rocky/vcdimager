#!/bin/sh
#
# $Id$
# cygwin-dist.sh script, by hvr
#

if [ ! -f /bin/cygwin1.dll ]
 then
   echo "this script must be run from a cygwin environment"
   exit 1
 fi

if [ -z "$1" ]
 then
   echo "no version given"
   exit 1
 fi

if [ ! -f configure.in ]
 then
   echo "wrong dir"
   exit 1
 fi

if [ ! -f frontends/cli/vcdimager.exe -o ! -f frontends/cli/vcdrip.exe ]
 then
   echo "executable(s) not found"
   exit 2
 fi

VERSION="$1"
TMPDIR="cygwin-$VERSION-tmp"
DISTZIP="vcdimager-$VERSION.win32.zip"

rm -rf $TMPDIR
mkdir $TMPDIR

for DOCFILE in BUGS TODO README NEWS ChangeLog THANKS AUTHORS COPYING FAQ
 do
   cp $DOCFILE $TMPDIR/$DOCFILE.txt
 done

(cd frontends/cli/; makeinfo --no-headers -o ../../$TMPDIR/manual.txt vcdimager.texi)

unix2dos $TMPDIR/*.txt

cp -v frontends/cli/vcdimager.exe frontends/cli/vcdrip.exe /bin/cygwin1.dll $TMPDIR/
strip -v $TMPDIR/*.exe

rm -fv "$DISTZIP"
zip -9v "$DISTZIP" -j $TMPDIR/*

rm -rf $TMPDIR

exit 0
