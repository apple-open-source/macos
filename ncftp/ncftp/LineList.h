/* LineList.h */

#ifndef _linelist_h_
#define _linelist_h_ 1

typedef struct Line *LinePtr;
typedef struct Line {
	LinePtr prev, next;
	char *line;
} Line;

typedef struct LineList {
	LinePtr first, last;
	int nLines;
} LineList, *LineListPtr;

#ifndef PTRZERO
#	define PTRZERO(p,siz)  memset(p, 0, (size_t) (siz))
#endif

char *StrDup(char *);
void DisposeLineListContents(LineListPtr);
void InitLineList(LineListPtr);
LinePtr RemoveLine(LineListPtr, LinePtr);
LinePtr AddLine(LineListPtr, char *);

#endif	/* _linelist_h_ */
