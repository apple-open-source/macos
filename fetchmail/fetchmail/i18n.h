/* Dummy header for libintl.h
 *
 * For license terms, see the file COPYING in this directory.
 */
#undef _
#ifdef ENABLE_NLS
#undef __OPTIMIZE__
#include <libintl.h>
#define GT_(String) gettext((String))
#define NGT_(String) (String)
#else
#define GT_(String) (String)
#define NGT_(String) (String)
#endif
