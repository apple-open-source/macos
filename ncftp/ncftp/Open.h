/* Open.h */

#ifndef _open_h_
#define _open_h_ 1

#ifndef _get_h_
#include "Get.h"
#endif

/* Variables for Open() that can be changed from the command line. */
typedef struct OpenOptions {
	int				openmode;
	int				ignoreRC;
	unsigned int	port;
	int				redialDelay;
	int				maxDials;
	int				ftpcat;
	int				loginVerbosity;
	char			hostname[128];
	char			cdpath[256];
	char			colonModePath[256];
	int				interactiveColonMode;
	GetOptions		gopt;
} OpenOptions;

/* Open modes. */
#define kOpenImplicitAnon 1
#define kOpenImplicitUser 4
#define kOpenExplicitAnon 3
#define kOpenExplicitUser 2

#define kRedialDelay 60

#define ISUSEROPEN(a) ((a==kOpenImplicitUser)||(a==kOpenExplicitUser))
#define ISANONOPEN(a) (!ISUSEROPEN(a))
#define ISEXPLICITOPEN(a) ((a==kOpenExplicitAnon)||(a==kOpenExplicitUser))
#define ISIMPLICITOPEN(a) (!ISEXPLICITOPEN(a))

/* ftpcat modes. */
#define kNoFTPCat 0
#define kFTPCat 1
#define kFTPMore 2

/* Protos: */
int LoginQuestion(char *, char *, size_t, char *, int);
int Login(char *, char *, char *);
void PostCloseStuff(void);
void DoClose(int);
int CloseCmd(void);
void InitOpenOptions(OpenOptions *);
int CheckForColonMode(OpenOptions *);
int GetOpenOptions(int, char **, OpenOptions *, int);
void CheckRemoteSystemType(int);
void ColonMode(OpenOptions *);
void PostLoginStuff(OpenOptions *);
int Open(OpenOptions *);
int OpenCmd(int, char **);

#endif 	/* _open_h_ */
