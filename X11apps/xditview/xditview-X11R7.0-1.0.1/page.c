/* $XConsortium: page.c,v 1.5 91/07/26 00:40:20 keith Exp $ */
/* $XFree86: xc/programs/xditview/page.c,v 1.3 2001/08/01 00:45:03 tsi Exp $ */

/*
 * page.c
 *
 * map page numbers to file position
 */

#include <X11/IntrinsicP.h>
#include <X11/StringDefs.h>
#include <X11/Xos.h>
#include <stdio.h>
#include <ctype.h>
#include "DviP.h"

static DviFileMap *
MapPageNumberToFileMap (DviWidget dw, int number)
{
	DviFileMap	*m;

	for (m = dw->dvi.file_map; m; m=m->next)
		if (m->page_number == number)
			break;
	return m;
}

void
DestroyFileMap (m)
	DviFileMap	*m;
{
	DviFileMap	*next;

	for (; m; m = next) {
		next = m->next;
		XtFree ((char *) m);
	}
}

void
ForgetPagePositions (dw)
	DviWidget	dw;
{
	DestroyFileMap (dw->dvi.file_map);
	dw->dvi.file_map = 0;
}

void
RememberPagePosition(dw, number)
	DviWidget	dw;
	int		number;
{
	DviFileMap	*m;

	if (!(m = MapPageNumberToFileMap (dw, number))) {
		m = (DviFileMap *) XtMalloc (sizeof *m);
		m->page_number = number;
		m->next = dw->dvi.file_map;
		dw->dvi.file_map = m;
	}
	if (dw->dvi.tmpFile)
		m->position = ftell (dw->dvi.tmpFile);
	else
		m->position = ftell (dw->dvi.file);
}

long
SearchPagePosition (dw, number)
	DviWidget	dw;
	int		number;
{
	DviFileMap	*m;

	if (!(m = MapPageNumberToFileMap (dw, number)))
		return -1;
	return m->position;
}

void
FileSeek(dw, position)
DviWidget	dw;
long		position;
{
	if (dw->dvi.tmpFile) {
		dw->dvi.readingTmp = 1;
		fseek (dw->dvi.tmpFile, position, 0);
	} else
		fseek (dw->dvi.file, position, 0);
}

