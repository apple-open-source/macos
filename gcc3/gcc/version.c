#include "ansidecl.h"
#include "version.h"

const char *const version_string = "3.1 20020420 (prerelease)";

/* APPLE LOCAL begin Apple version */
/* Note that we can't say "apple_v*rs**n_str*ng" because of a cheesy
   grep in configure that will get very confused if we do.  */
const char *const apple_version_str = "1161";
/* APPLE LOCAL end Apple version */
