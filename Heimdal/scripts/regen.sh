
find . -name \*-private.h -or -name \*-protos.h | grep -v lib/gssapi/netlogon | xargs rm
autoreconf -f -i || exit 1
rm -rf o || exit 1
mkdir o || exit 1
cd o || exit 1
../configure \
    --localstatedir=/var \
    --prefix=/usr \
    --disable-krb4 \
    --disable-digest \
    --disable-kx509 \
    --without-openssl > log || exit 1
make distdir || exit 1
cd .. || exit 1
( cat o/include/config.h
  echo '#include "config-apple.h"'
  echo '' ) > packages/mac/SnowLeopard10A/config.h
cp o/include/version.h packages/mac/SnowLeopard10A/version.h || exit 1
cp include/krb5-types.cross packages/mac/SnowLeopard10A/krb5-types.h || exit 1
perl cf/roken-h-process.pl \
    -c packages/mac/SnowLeopard10A/config.h  \
    -p lib/roken/roken.h.in \
    -o packages/mac/SnowLeopard10A/roken.h || exit 1
rm -rf doc/doxyout o
echo "git commit -m 'regen autotools/makefiles' -f configure config.sub config.guess aclocal.m4 include/config.h.in compile depcomp install-sh ltmain.sh missing"
