/* Dummy header for libintl.h */

#undef _
#ifdef ENABLE_NLS
#undef __OPTIMIZE__
#include <libintl.h>
#define _(String) gettext((String))
#define N_(String) (String)
#else
#define _(String) (String)
#define N_(String) (String)
#endif
