#!/bin/sh

scriptdir=$(dirname $(realpath "$0"))

cfgflags="--with-shared --without-normal --without-debug"
cfgflags="$cfgflags --without-cxx-binding --without-cxx"

# Features that should always be present on macOS.
cfgflags="$cfgflags --enable-termcap --enable-widec --enable-ext-colors"

# ABI version should remain pinned to 5.4.  It won't affect much of anything
# useful in the build, we'll do our own magic to make new symbol versions
# apparent and functional while maintaining compatibility.
cfgflags="$cfgflags --with-abi-version=5.4"

# Finally, pertinent paths used for build/install.
cfgflags="$cfgflags --mandir=/usr/share/man --datarootdir=/usr/share"

cd ${scriptdir}/ncurses && sh configure $cfgflags
${scriptdir}/generate-syms.py
