#!/bin/sh
#set -x
set -v

cd openssh
./configure --with-pam --with-audit=bsm --with-kerberos5=/usr \
 --disable-libutil \
 --disable-pututline \
 --with-xauth="xauth" \
 --with-default-path="/usr/bin:/bin:/usr/sbin:/sbin" \
 --with-cppflags="-I`xcodebuild -version -sdk macosx.internal Path`/usr/local/include/sshcrypto" \
 --with-ldflags="-L`xcodebuild -version -sdk macosx.internal Path`/usr/local/lib/sshcrypto"
