
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

SASL_AUXPROP_PLUG_INIT( sasldb )

