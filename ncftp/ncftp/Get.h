/* Get.h */

#ifndef _get_h_
#define _get_h_ 1

#ifndef _xfer_h_
#include "Xfer.h"
#endif

/* Parameter for DoGet. */
#define kSaveToDisk		0
#define kDumpToStdout	1

/* Value of a user-configurable variable which determines if
 * we try to restore the correct file time.
 */
#define kDontUTime		0
#define kDoUTime		1

typedef struct GetOptions {
	int recursive;
	int noGlob;
	int newer;
	int overwrite;
	int forceReget;
	int saveAs;
	int outputMode;	/* Dumping to the screen or saving to disk? */
	int doUTime;
	int doReports;
	char *rName;	/* This is required to be set. */
	char *lName;	/* This is optional.  If set, we use this name,
					 * otherwise we will make up a name, based on the rName.
					 */
} GetOptions, *GetOptionsPtr;

/* Get.c */
int BinaryGet(XferSpecPtr);
int AsciiGet(XferSpecPtr);
void SetLocalFileTimes(int, time_t, char *);
int TruncReOpenReceiveFile(XferSpecPtr);
int DoGet(GetOptionsPtr);
void InitGetOutputMode(GetOptionsPtr, int);
void InitGetOptions(GetOptionsPtr);
int SetGetOption(GetOptionsPtr, int, char *);
int GetGetOptions(int, char **, GetOptionsPtr);
int GetDir(GetOptionsPtr, char *, char *, char *);
int RemoteFileType(char *);
int DoGetWithGlobbingAndRecursion(GetOptionsPtr);
int GetCmd(int, char **);
int CatFileToScreenProc(XferSpecPtr);
int DoCat(char *);
int MakePageCmdLine(char *, size_t, char *);
int DoPage(char *);
int PageCmd(int, char **);
int CatCmd(int, char **);

#endif	/* _get_h_ */
