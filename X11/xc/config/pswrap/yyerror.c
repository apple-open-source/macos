/*
 *  yyerror.c
 *
 * (c) Copyright 1988-1994 Adobe Systems Incorporated.
 * All rights reserved.
 * 
 * Permission to use, copy, modify, distribute, and sublicense this software
 * and its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notices appear in all copies and that
 * both those copyright notices and this permission notice appear in
 * supporting documentation and that the name of Adobe Systems Incorporated
 * not be used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  No trademark license
 * to use the Adobe trademarks is hereby granted.  If the Adobe trademark
 * "Display PostScript"(tm) is used to describe this software, its
 * functionality or for any other purpose, such use shall be limited to a
 * statement that this software works in conjunction with the Display
 * PostScript system.  Proper trademark attribution to reflect Adobe's
 * ownership of the trademark shall be given whenever any such reference to
 * the Display PostScript system is made.
 * 
 * ADOBE MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THE SOFTWARE FOR
 * ANY PURPOSE.  IT IS PROVIDED "AS IS" WITHOUT EXPRESS OR IMPLIED WARRANTY.
 * ADOBE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NON- INFRINGEMENT OF THIRD PARTY RIGHTS.  IN NO EVENT SHALL ADOBE BE LIABLE
 * TO YOU OR ANY OTHER PARTY FOR ANY SPECIAL, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE, STRICT LIABILITY OR ANY OTHER ACTION ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.  ADOBE WILL NOT
 * PROVIDE ANY TRAINING OR OTHER SUPPORT FOR THE SOFTWARE.
 * 
 * Adobe, PostScript, and Display PostScript are trademarks of Adobe Systems
 * Incorporated which may be registered in certain jurisdictions
 * 
 * Author:  Adobe Systems Incorporated
 */

#include <stdio.h>
#include <ctype.h>

#include "pswpriv.h"

/* ErrIntro prints a standard intro for error messages;
 * change it if your system uses something else.  We have many options:
 *
 * to match Macintosh:  #define FMT "File \"%s\"; Line %d # "
 * to match BSD cc:	#define FMT "\"%s\", line %d: "
 * to match gcc:	#define FMT "%s:%d: "
 * to match Mips cc:	#define FMT "pswrap: Error: %s, line %d: "
 */


#define INTRO	"# In function %s -\n"

#ifdef macintosh
#define FMT "File \"%s\"; Line %d # "
#else /* macintosh */
#define FMT "\"%s\", line %d: "
#endif /* macintosh */

void ErrIntro(int line)
{
    if (! reportedPSWName && currentPSWName) {
		reportedPSWName = 1;
		fprintf(stderr,INTRO,currentPSWName);
    }
    fprintf(stderr,FMT,ifile,line);
    errorCount++;
}


void yyerror(char *errmsg)
{
    ErrIntro(yylineno);
    fprintf(stderr,"%s near text \"%s\"\n",errmsg,yytext);
}
