/* Util.h */

#ifndef _util_h_
#define _util_h_ 1

#include "Strn.h"

typedef char string[256], str16[16], str32[32], str64[64];
typedef char longstring[512];
typedef char pathname[512];

/* Exit status. */
#define kExitNoErr			0
#define kExitUsageErr		2
#define kExitOutOfMemory	3
#define kExitBadHostName	4
#define kExitWinFail1		5
#define kExitWinFail2		6
#define kExitColonModeFail	7
#define kExitPanic			8
#define kExitSignal			9

#define ZERO(a)	PTRZERO(&(a), sizeof(a))
#define STREQ(a,b) (strcmp(a,b) == 0)
#define STRNEQ(a,b,s) (strncmp(a,b,(size_t)(s)) == 0)

#ifndef ISTRCMP
#	ifdef HAVE_STRCASECMP
#		define ISTRCMP strcasecmp
#		define ISTRNCMP strncasecmp
#	else
#		define ISTRCMP strcmp
#		define ISTRNCMP strncmp
#	endif
#endif

#define ISTREQ(a,b) (ISTRCMP(a,b) == 0)
#define ISTRNEQ(a,b,s) (ISTRNCMP(a,b,(size_t)(s)) == 0)

typedef int (*cmp_t)(const void *, const void *);
#define QSORT(base,n,s,cmp) \
	qsort(base, (size_t)(n), (size_t)(s), (cmp_t)(cmp))

#define BSEARCH(key,base,n,s,cmp) \
	bsearch(key, base, (size_t)(n), (size_t)(s), (cmp_t)(cmp))

/* For Error(): */
#define kDoPerror		1
#define kDontPerror		0

/* Used by SetArraySize(). */
#define kArrayIncrement 8

#define kClosedFileDescriptor (-1)

#define SZ(a) ((size_t) (a))

typedef void (*Sig_t)(int);
typedef volatile Sig_t VSig_t;

#define SIGNAL(a,proc) signal((a), (Sig_t)(proc))
#define kNoSignalHandler ((Sig_t) -96)

#ifndef kDebugStream
#	define kDebugStream stdout
#endif

#ifndef F_OK
#	define F_OK 0
#endif

#ifndef HAVE_MEMMOVE
void *MemMove(void *, void *, size_t);
#	define MEMMOVE MemMove
#else
#	define MEMMOVE memmove
#endif

#ifdef HAVE_REMOVE
#	define UNLINK remove
#else
#	define UNLINK unlink
#endif

#ifndef SEEK_SET
#	define SEEK_SET    0
#	define SEEK_CUR    1
#	define SEEK_END    2
#endif  /* SEEK_SET */

#ifdef SETVBUF_REVERSED
#	define SETVBUF(a,b,c,d) setvbuf(a,c,b,d)
#else
#	define SETVBUF setvbuf
#endif

#ifdef ultrix
#	ifndef NO_FGTEST
#		define NO_FGTEST 1
#	endif
#endif

char *FGets(char *, size_t, FILE *);
void OutOfMemory(void);
char *PtrCat(char *, char *);
char *PtrCatSlash(char *, char *);
int SetArraySize(int **, int, int *, size_t);
void StrLCase(char *);
char *GetCWD(char *, size_t);
char *Path(char *, size_t, char *, char *);
char *OurDirectoryPath(char *, size_t, char *);
int MkDirs(char *);
int CloseFile(FILE **);
int InForeGround(void);
int UserLoggedIn(void);
int CheckNewMail(void);
size_t FlagStrCopy(char *, size_t, char *);
void *Realloc(void *, size_t);
void OverflowAdd(long *dst, long plus);
void AbbrevStr(char *dst, char *src, size_t max, int mode);
void MakeStringPrintable(char *dst, unsigned char *src, size_t siz);
FILE *POpen(char *, char *, int);

#include "LineList.h"
#include "Win.h"

#endif	/* _util_h_ */
