/* LineList.c */

#include "Sys.h"
#include "LineList.h"


/* Dynamically make a copy of a string. */
char *StrDup(char *buf)
{
	char *cp;
	
	cp = (char *) malloc(strlen(buf) + 1);
	if (cp != NULL)
		strcpy(cp, buf);
	return (cp);
}	/* StrDup */




/* Disposes each node of a LineList.  Does a few extra things
 * so the disposed memory won't be very useful after it is freed.
 */
void DisposeLineListContents(LineListPtr list)
{
	LinePtr lp, lp2;
	
	for (lp = list->first; lp != NULL; ) {
		lp2 = lp;
		lp = lp->next;
		lp2->line[0] = '\0';
		free(lp2->line);
		free(lp2);
	}
	/* Same as InitLineList. */
	PTRZERO(list, sizeof(LineList));
}	/* DisposeLineListContents */




void InitLineList(LineListPtr list)
{
	PTRZERO(list, sizeof(LineList));
}	/* InitLineList */




LinePtr RemoveLine(LineListPtr list, LinePtr killMe)
{
	LinePtr nextLine, prevLine;
	
	nextLine = killMe->next;	
	prevLine = killMe->prev;	
	killMe->line[0] = '\0';		/* Make it useless just in case. */
	
	if (list->first == killMe)
		list->first = nextLine;
	if (list->last == killMe)
		list->last = prevLine;

	if (nextLine != NULL)
		nextLine->prev = prevLine;
	if (prevLine != NULL)
		prevLine->next = nextLine;

	free(killMe->line);
	free(killMe);	
	list->nLines--;
	return (nextLine);
}	/* RemoveLine */




/* Adds a string to the LineList specified. */
LinePtr AddLine(LineListPtr list, char *buf)
{
	LinePtr lp;
	
	lp = (LinePtr) malloc(sizeof(Line));
	if (lp != NULL) {
		buf = StrDup(buf);
		if (buf == NULL) {
			free(lp);
			lp = NULL;
		} else {
			lp->line = buf;
			lp->next = NULL;
			if (list->first == NULL) {
				list->first = list->last = lp;
				lp->prev = NULL;
				list->nLines = 1;
			} else {
				lp->prev = list->last;
				list->last->next = lp;
				list->last = lp;
				list->nLines++;
			}
		}
	}
	return lp;
}	/* AddLine */

/* eof LineList.c */
