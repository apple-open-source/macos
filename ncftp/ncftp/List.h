/* List.h */

/* Parameter to DoList. */
#define kListLongMode			"-l"
#define kListShortMode			"-CF"
#define kListDirNamesOnlyMode	"-d"
#define kListNoFlags			NULL

/* Equates for ListCmd, to keep track whether we're paging the
 * output or dumping it to the screen.
 */
#define kNoPaging		0
#define kPageMode		1

#ifndef _xfer_h_
#include "Xfer.h"
#endif

/* List.c */
int ListReceiveProc(XferSpecPtr);
int ListToMemoryReceiveProc(XferSpecPtr);
void ListToMemory(LineListPtr, char *, char *, char *);
int FileListReceiveProc(XferSpecPtr);
void GetFileList(LineListPtr, char *);
int DoList(int, char **, char *);
int ListCmd(int, char **);
int LocalListCmd(int, char **);
int RedirCmd(int, char **);
