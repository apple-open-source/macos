
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

#include "plugin_common.h"

#ifdef macintosh
#include <sasl_digestmd5_plugin_decl.h>
#endif

SASL_CLIENT_PLUG_INIT( digestmd5 )
SASL_SERVER_PLUG_INIT( digestmd5 )

