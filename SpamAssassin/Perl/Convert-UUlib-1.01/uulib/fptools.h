/*
 * fptools.c, some helper functions for getcgi.c and uu(en|de)view
 *
 * Distributed under the terms of the GNU General Public License.
 * Use and be happy.
 */

/*
 * Some handy, nonstandard functions. Note that the original may
 * be both faster and better. ``better'', if your compiler allows
 * cleaner use of such functions by proper use of ``const''.
 *
 * $Id: fptools.h,v 1.1 2004/04/19 17:50:27 dasenbro Exp $
 */

#ifndef FPTOOLS_H__
#define FPTOOLS_H__

typedef signed char schar;
typedef unsigned char uchar;

#ifndef TOOLEXPORT
#define TOOLEXPORT
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define _FP_free	FP_free
#define _FP_strdup	FP_strdup
#define _FP_strncpy	FP_strncpy
#define _FP_memdup	FP_memdup
#define _FP_stricmp	FP_stricmp
#define _FP_strnicmp	FP_strnicmp
#define _FP_strrstr	FP_strrstr
#define _FP_stoupper	FP_stoupper
#define _FP_stolower	FP_stolower
#define _FP_strmatch	FP_strmatch
#define _FP_strstr	FP_strstr
#define _FP_stristr	FP_stristr
#define _FP_strirstr	FP_strirstr
#define _FP_strrchr	FP_strrchr
#define _FP_fgets	FP_fgets
#define _FP_strpbrk	FP_strpbrk
#define _FP_strtok	FP_strtok
#define _FP_cutdir	FP_cutdir
#define _FP_strerror	FP_strerror
#define _FP_tempnam	FP_tempnam

void	TOOLEXPORT	_FP_free	(void *);
char *	TOOLEXPORT	_FP_strdup	(char *);
char *	TOOLEXPORT	_FP_strncpy	(char *, char *, int);
void *	TOOLEXPORT	_FP_memdup	(void *, int);
int 	TOOLEXPORT	_FP_stricmp	(char *, char *);
int 	TOOLEXPORT	_FP_strnicmp	(char *, char *, int);
char *	TOOLEXPORT	_FP_strrstr	(char *, char *);
char *	TOOLEXPORT	_FP_stoupper	(char *);
char *	TOOLEXPORT	_FP_stolower	(char *);
int 	TOOLEXPORT	_FP_strmatch	(char *, char *);
char *	TOOLEXPORT	_FP_strstr	(char *, char *);
char *	TOOLEXPORT	_FP_stristr	(char *, char *);
char *	TOOLEXPORT	_FP_strirstr	(char *, char *);
char *	TOOLEXPORT	_FP_strrchr	(char *, int);
char *	TOOLEXPORT	_FP_fgets	(char *, int, FILE *);
char *	TOOLEXPORT	_FP_strpbrk	(char *, char *);
char *	TOOLEXPORT	_FP_strtok	(char *, char *);
char *	TOOLEXPORT	_FP_cutdir	(char *);
char *	TOOLEXPORT	_FP_strerror	(int);
#ifndef HAVE_MKSTEMP
char *	TOOLEXPORT	_FP_tempnam	(char *, char *);
#endif /* HAVE_MKSTEMP */

#ifdef __cplusplus
}
#endif
#endif

