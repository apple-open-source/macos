/*
 * Copyright (C) 1998 Arnaud LE HORS
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * Arnaud LE HORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of Arnaud LE HORS shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Arnaud LE HORS.
 */
/* $XFree86: xc/extras/Xpm/cxpm/cxpm.c,v 1.2 2001/08/01 00:44:34 tsi Exp $ */

/*****************************************************************************\
* cxpm.c:                                                                     *
*                                                                             *
*  Check XPM File program                                                     *
*                                                                             *
*  Developed by Arnaud Le Hors                                                *
\*****************************************************************************/

#define CXPMPROG

#include "../lib/XpmI.h"

#undef xpmGetC
#define xpmGetC(data) sGetc(data, data->stream.file)
#define Getc sGetc
#define Ungetc sUngetc


/*
 * special getc and ungetc counting read lines and characters
 * note that 's' could stand both for "special" and "slow" ;-)
 */
static int
sGetc(data)
    xpmData *data;
{
    int c = getc(data->stream.file);
    if (c == '\n') {
        data->lineNum++;
        data->charNum = 0;
    } else {
        data->charNum++;
    }
    return c;
}

static void
sUngetc(data, c)
    xpmData *data;
    int c;
{
    ungetc(c, data->stream.file);
    if (c == '\n') {
        data->lineNum--;
        data->charNum = 0;
    } else {
        data->charNum--;
    }
}

/* include all the code we need (yeah, I know, quite ugly...) */
#include "../lib/data.c"
#include "../lib/parse.c"
#include "../lib/RdFToI.c"	/* only for OpenReadFile and xpmDataClose */
#include "../lib/hashtab.c"
#include "../lib/misc.c"
#include "../lib/Attrib.c"
#include "../lib/Image.c"

void
ErrorMessage(ErrorStatus, data)
    int ErrorStatus;
    xpmData *data;

{
    char *error = NULL;

    switch (ErrorStatus) {
    case XpmSuccess:
	return;
    case XpmOpenFailed:
	error = "Cannot open file";
	break;
    case XpmFileInvalid:
	error = "Invalid XPM file";
	break;
    case XpmNoMemory:
	error = "Not enough memory";
	break;
    case XpmColorFailed:
	error = "Failed to parse color";
	break;
    }

    if (error) {
	fprintf(stderr, "Xpm Error: %s.\n", error);
	if (ErrorStatus == XpmFileInvalid && data)
	  fprintf(stderr, "Error found line %d near character %d\n",
		  data->lineNum + 1,
		  data->charNum + 1);
	exit(1);
    }
}

int
main(argc, argv)
    int argc;
    char **argv;
{
    XpmImage image;
    char *filename;
    int ErrorStatus;
    xpmData data;

    if (argc > 1) {
        if (!strcmp(argv[1], "-?") || !strncmp(argv[1], "-h", 2)) {
	    fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
	    exit(1);
	}
        filename = argv[1];
    } else {
        filename = NULL;
    }

    xpmInitXpmImage(&image);

    if ((ErrorStatus = OpenReadFile(filename, &data)) != XpmSuccess)
	ErrorMessage(ErrorStatus, NULL);

    ErrorStatus = xpmParseData(&data, &image, NULL);
    ErrorMessage(ErrorStatus, &data);

    xpmDataClose(&data);
    XpmFreeXpmImage(&image);

    exit(0);
}
