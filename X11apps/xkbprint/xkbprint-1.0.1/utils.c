
  /*\
   * $Xorg: utils.c,v 1.5 2000/08/17 19:54:50 cpqbld Exp $
   *
   *		              COPYRIGHT 1990
   *		        DIGITAL EQUIPMENT CORPORATION
   *		           MAYNARD, MASSACHUSETTS
   *			    ALL RIGHTS RESERVED.
   *
   * THE INFORMATION IN THIS SOFTWARE IS SUBJECT TO CHANGE WITHOUT NOTICE AND
   * SHOULD NOT BE CONSTRUED AS A COMMITMENT BY DIGITAL EQUIPMENT CORPORATION.
   * DIGITAL MAKES NO REPRESENTATIONS ABOUT THE SUITABILITY OF THIS SOFTWARE 
   * FOR ANY PURPOSE.  IT IS SUPPLIED "AS IS" WITHOUT EXPRESS OR IMPLIED 
   * WARRANTY.
   *
   * IF THE SOFTWARE IS MODIFIED IN A MANNER CREATING DERIVATIVE COPYRIGHT
   * RIGHTS, APPROPRIATE LEGENDS MAY BE PLACED ON THE DERIVATIVE WORK IN
   * ADDITION TO THAT SET FORTH ABOVE.
   *
   * Permission to use, copy, modify, and distribute this software and its
   * documentation for any purpose and without fee is hereby granted, provided
   * that the above copyright notice appear in all copies and that both that
   * copyright notice and this permission notice appear in supporting
   * documentation, and that the name of Digital Equipment Corporation not be
   * used in advertising or publicity pertaining to distribution of the 
   * software without specific, written prior permission.
  \*/
/* $XFree86: xc/programs/xkbprint/utils.c,v 3.4 2001/01/17 23:46:11 dawes Exp $ */

#include 	"utils.h"
#include	<ctype.h>
#include	<stdlib.h>

/***====================================================================***/

Opaque
uAlloc(size)
    unsigned	size;
{
    return((Opaque)malloc(size));
}

/***====================================================================***/

Opaque
uCalloc(n,size)
    unsigned	n;
    unsigned	size;
{
    return((Opaque)calloc(n,size));
}

/***====================================================================***/

Opaque
uRealloc(old,newSize)
    Opaque	old;
    unsigned	newSize;
{
    if (old==NULL)
	 return((Opaque)malloc(newSize));
    else return((Opaque)realloc((char *)old,newSize));
}

/***====================================================================***/

Opaque
uRecalloc(old,nOld,nNew,itemSize)
    Opaque	old;
    unsigned	nOld;
    unsigned	nNew;
    unsigned	itemSize;
{
char *rtrn;

    if (old==NULL)
	 rtrn= (char *)calloc(nNew,itemSize);
    else {
	rtrn= (char *)realloc((char *)old,nNew*itemSize);
   	if ((rtrn)&&(nNew>nOld)) {
	    bzero(&rtrn[nOld*itemSize],(nNew-nOld)*itemSize);
	}
    }
    return (Opaque)rtrn;
}

/***====================================================================***/

void
uFree(ptr)
    Opaque ptr;
{
    if (ptr!=(Opaque)NULL)
	free((char *)ptr);
    return;
}

/***====================================================================***/
/***                  FUNCTION ENTRY TRACKING                           ***/
/***====================================================================***/

static	FILE	*entryFile=	NULL;
	int	 uEntryLevel;

Boolean
uSetEntryFile(name)
    char *name;
{
    if ((entryFile!=NULL)&&(entryFile!=stderr)) {
	fprintf(entryFile,"switching to %s\n",name?name:"stderr");
	fclose(entryFile);
    }
    if (name!=NullString)	entryFile=	fopen(name,"w");
    else			entryFile=	stderr;
    if (entryFile==NULL) {
	entryFile=	stderr;
	return(False);
    }
    return(True);
}

void
uEntry(int l, char *s,...)
{
int	i;
va_list ap;

    va_start(ap, s);
    for (i=0;i<uEntryLevel;i++) {
	putc(' ',entryFile);
    }
    vfprintf(entryFile,s,ap);
    uEntryLevel+= l;
    va_end(ap);
    return;
}

void
uExit(l,rtVal)
    int		l;
    char *	rtVal;
{
int	i;

    uEntryLevel-= l;
    if (uEntryLevel<0)	uEntryLevel=	0;
    for (i=0;i<uEntryLevel;i++) {
	putc(' ',entryFile);
    }
    fprintf(entryFile,"---> 0x%p\n",rtVal);
    return;
}

/***====================================================================***/
/***			PRINT FUNCTIONS					***/
/***====================================================================***/

	FILE	*uDebugFile=		NULL;
	int	 uDebugIndentLevel=	0;
	int	 uDebugIndentSize=	4;

Boolean
uSetDebugFile(name)
    char *name;
{
    if ((uDebugFile!=NULL)&&(uDebugFile!=stderr)) {
	fprintf(uDebugFile,"switching to %s\n",name?name:"stderr");
	fclose(uDebugFile);
    }
    if (name!=NullString)	uDebugFile=	fopen(name,"w");
    else			uDebugFile=	stderr;
    if (uDebugFile==NULL) {
	uDebugFile=	stderr;
	return(False);
    }
    return(True);
}

void
uDebug(char *s,...)
{
int	i;
va_list ap;

    va_start(ap, s);
    for (i=(uDebugIndentLevel*uDebugIndentSize);i>0;i--) {
	putc(' ',uDebugFile);
    }
    vfprintf(uDebugFile,s,ap);
    fflush(uDebugFile);
    va_end(ap);
    return;
}

void
uDebugNOI(char *s,...)
{
va_list ap;

    va_start(ap, s);
    vfprintf(uDebugFile,s,ap);
    fflush(uDebugFile);
    va_end(ap);
    return;
}

/***====================================================================***/

static	FILE	*errorFile=	NULL;

Boolean
uSetErrorFile(char *name)
{
    if ((errorFile!=NULL)&&(errorFile!=stderr)) {
	fprintf(errorFile,"switching to %s\n",name?name:"stderr");
	fclose(errorFile);
    }
    if (name!=NullString)	errorFile=	fopen(name,"w");
    else			errorFile=	stderr;
    if (errorFile==NULL) {
	errorFile=	stderr;
	return(False);
    }
    return(True);
}

void
uInformation(char *s,...)
{
    va_list ap;

    va_start(ap, s);
    vfprintf(errorFile,s,ap);
    fflush(errorFile);
    va_end(ap);
    return;
}

/***====================================================================***/

void
uAction(char *s,...)
{
    va_list ap;

    va_start(ap, s);
    fprintf(errorFile,"                  ");
    vfprintf(errorFile,s,ap);
    fflush(errorFile);
    va_end(ap);
    return;
}

/***====================================================================***/

void
uWarning(char *s,...)
{
    va_list ap;

    va_start(ap, s);
    fprintf(errorFile,"Warning:          ");
    vfprintf(errorFile,s,ap);
    fflush(errorFile);
    va_end(ap);
    return;
}

/***====================================================================***/

void
uError(char *s,...)
{
    va_list ap;

    va_start(ap, s);
    fprintf(errorFile,"Error:            ");
    vfprintf(errorFile,s,ap);
    fflush(errorFile);
    va_end(ap);
    return;
}

/***====================================================================***/

void
uFatalError(char *s,...)
{
    va_list ap;

    va_start(ap, s);
    fprintf(errorFile,"Fatal Error:      ");
    vfprintf(errorFile,s,ap);
    fprintf(errorFile,"                  Exiting\n");
    fflush(errorFile);
    va_end(ap);
    exit(1);
    /* NOTREACHED */
}

/***====================================================================***/

void
uInternalError(char *s,...)
{
    va_list ap;

    va_start(ap, s);
    fprintf(errorFile,"Internal error:   ");
    vfprintf(errorFile,s,ap);
    fflush(errorFile);
    va_end(ap);
    return;
}

/***====================================================================***/

#ifndef HAVE_STRDUP
char *
uStringDup(str)
    char *str;
{
char *rtrn;

    if (str==NULL)
	return NULL;
    rtrn= (char *)uAlloc(strlen(str)+1);
    strcpy(rtrn,str);
    return rtrn;
}
#endif

#ifndef HAVE_STRCASECMP
int
uStrCaseCmp(str1, str2)
    char *str1, *str2;
{
    char buf1[512],buf2[512];
    char c, *s;
    register int n;

    for (n=0, s = buf1; (c = *str1++); n++) {
	if (isupper(c))
	    c = tolower(c);
	if (n>510)
	    break;
	*s++ = c;
    }
    *s = '\0';
    for (n=0, s = buf2; (c = *str2++); n++) {
	if (isupper(c))
	    c = tolower(c);
	if (n>510)
	    break;
	*s++ = c;
    }
    *s = '\0';
    return (strcmp(buf1, buf2));
}

int
uStrCasePrefix(prefix, str)
    char *prefix, *str;
{
    char c1;
    char c2;
    while (((c1=*prefix)!='\0')&&((c2=*str)!='\0')) {
	if (isupper(c1))	c1= tolower(c1);
	if (isupper(c2))	c2= tolower(c2);
	if (c1!=c2)
	    return 0;
	prefix++; str++;
    }
    if (c1!='\0')
	return 0;
    return 1;
}

#endif
