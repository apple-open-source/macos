#!/bin/sh
. "${SRCROOT}/xcscripts/include.sh"

src="$(src generated-config.h)"
dst="${BUILT_PRODUCTS_DIR}/include/config.h"
install -d "${BUILT_PRODUCTS_DIR}/include"
if [ "${PLATFORM_NAME}" = "macosx" ]; then
    sed -E -e '1i\
#include <TargetConditionals.h>' "${src}" > "${dst}"
else
    undef="GSSAPI|HAVE_UTMP_H|KRB5|SANDBOX_DARWIN|USE_PAM"
    sed -E \
      -e '1i\
#include <TargetConditionals.h>' \
      -e 's;^#define[[:space:]]+('"$undef"')[[:space:]].*;#undef \1;' \
      -e '/SSH_PRIVSEP_USER/s;"sshd";"_sshd";' \
      -e '/USER_PATH/s;"[[:space:]]*$;:/usr/local/bin";' \
      -e '/SANDBOX_NULL/c\
#define SANDBOX_NULL 1' \
      "${src}" > "${dst}"
fi
