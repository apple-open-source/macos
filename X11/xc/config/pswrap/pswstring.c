/*
 *  pswstring.c
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
/* $XFree86: xc/config/pswrap/pswstring.c,v 1.3 2000/09/26 15:56:28 tsi Exp $ */

#include <stdio.h>
#include <ctype.h>
#include <string.h>

#include "pswpriv.h"
#include "psw.h"

#define outfil stdout
#define MAX_PER_LINE 16

int PSWStringLength(char *s)
{
    register char *c = s;
    register int len = 0;

    while (*c != '\0') {	/* skip \\ and \ooo */
	if (*c++ == '\\') {
	    if (*c++ != '\\') c += 2;
	}
	len++;
    }
    return (len);
}

void PSWOutputStringChars(char *s)
{
    register char *c = s;
    register char b;
    register int perline = 0;
    
    while (*c != '\0') {
    putc('\'',outfil);
    switch (b = *c++) {
	    case '\\':
	        putc('\\',outfil);
		fputc(b = *c++,outfil);
	        if (b != '\\') {putc(*c++,outfil);putc(*c++,outfil);}
		break;
	    case '\'':
	        fprintf(outfil,"\\'");
		break;
	    case '\"':
	        fprintf(outfil,"\\\"");
		break;
	    case '\b':
	        fprintf(outfil,"\\b");
		break;
	    case '\f':
	        fprintf(outfil,"\\f");
		break;
/* avoid funny interpretations of \n, \r by MPW */
	    case '\012':
	        fprintf(outfil,"\\012"); perline++;
		break;
	    case '\015':
	        fprintf(outfil,"\\015"); perline++;
		break;
	    case '\t':
	        fprintf(outfil,"\\t");
		break;
	    default:
		putc(b,outfil); perline--;
		break;
	}
	putc('\'',outfil);
	if (*c != '\0') {
	    if (++perline >= MAX_PER_LINE) {
		fprintf(outfil,",\n     ");
		outlineno++;
	    }
	    else {putc(',',outfil);}
	    perline %= MAX_PER_LINE;
	}
    }
}


int PSWHexStringLength(char *s)
{
    return ((int) (strlen(s)+1)/2);
}

void PSWOutputHexStringChars(register char *s)
{
    register int perline = 0;
    char tmp[3];

    tmp[2] ='\0';
    while ((tmp[0] = *s++)!= '\0') {
	tmp[1] = *s ? *s++ : '\0';
	fprintf(outfil,"0x%s",tmp);
	if (*s != '\0') {
	    if (++perline >= MAX_PER_LINE) {
		fprintf(outfil,",\n     ");
		outlineno++;
	    }
	    else {putc(',',outfil);}
	    perline %= MAX_PER_LINE;
	}
    } /* while */
}
