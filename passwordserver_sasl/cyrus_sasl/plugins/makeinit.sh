for mech in anonymous crammd5 digestmd5 gssapiv2 kerberos4 login plain srp otp; do

echo "
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <time.h>
#ifndef macintosh
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <assert.h>

#include <sasl.h>
#include <saslplug.h>
#include <saslutil.h>

#include \"plugin_common.h\"

#ifdef macintosh
#include <sasl_${mech}_plugin_decl.h>
#endif

SASL_CLIENT_PLUG_INIT( $mech )
SASL_SERVER_PLUG_INIT( $mech )
" > ${mech}_init.c
done

for mech in sasldb; do

echo "
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <config.h>
#include <time.h>
#ifndef macintosh
#include <sys/stat.h>
#endif
#include <fcntl.h>
#include <assert.h>

#include <sasl.h>
#include <saslplug.h>
#include <saslutil.h>

#include \"plugin_common.h\"

SASL_AUXPROP_PLUG_INIT( $mech )
" > ${mech}_init.c
done

