#!/bin/sh
#set -x
set -v

cd openssh
./configure --with-pam --with-audit=bsm --with-kerberos5=/usr \
 --disable-libutil \
 --disable-pututline \
 --with-default-path="/usr/bin:/bin:/usr/sbin:/sbin" \
 --with-cppflags="-I`xcodebuild -version -sdk macosx.internal Path`/usr/local/libressl/include" \
 --with-ldflags="-L`xcodebuild -version -sdk macosx.internal Path`/usr/local/libressl/lib"
