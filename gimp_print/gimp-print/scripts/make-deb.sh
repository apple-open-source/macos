#!/bin/sh

set -e

DEBVERSION=`cat /etc/debian_version`

case $DEBVERSION in
       testing/unstable|woody)
            #make ghost
            ./configure --with-ghost --without-cups --without-gimp
            make
            mkdir -p deb
            cd deb
            apt-get source gs libjpeg62
            mv libjpeg6b-6b libjpeg
            patch -p4 < ../src/ghost/debian-patch
            cd gs-5.10
            cp ../../src/ghost/debian-patch-stp debian/patches/stp
            mkdir -p contrib/stp
            cp ../../src/ghost/README contrib/stp/README.stp
            cp ../../src/ghost/gdevstp* contrib/stp
            cp ../../src/ghost/devs.mak.addon-5.10 contrib/stp
            fakeroot dpkg-buildpackage
            ;;
       *)
            echo "Debian release $DEBVERSION not yet supported."
            exit 1
            ;;
esac

exit 0
