/* $Xorg: PRead.c,v 1.4 2001/02/09 02:05:58 xorgcvs Exp $ */
/*

Copyright 1996, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABIL-
ITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT
SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABIL-
ITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization from
The Open Group.

*/

#include "RxI.h"
#include <ctype.h>


/* Not null terminated string */
typedef struct {
    char *ptr;			/* beginning of string */
    int length;			/* length of string */
} NString;


#ifdef NETSCAPE_PLUGIN
/* utility function for netscape plugin where realloc is not available
 * it's too bad we need both the old size and the new one...
 */
void *
_RxRealloc(void *p, size_t olds, size_t s)
{
    void *np = Malloc(s);
    if (np) {
	memcpy(np, p, olds);
	Free(p);
    }
    return np;
}
#endif


/* get next word and return trailing stream */
static char *
NextWord(char *stream, char *limit, NString *word)
{
    /* skip leading whitespace */
    while (isspace((int) *stream) && *stream && stream != limit)
	stream++;
    word->ptr = stream;
    /* go to first whitespace */
    while (!isspace((int) *stream) && *stream && stream != limit)
	stream++;
    word->length = stream - word->ptr;
    return stream;
}


/* get next chunk of text, with possible quoted sections,
   and return trailing stream */
static char *
NextChunk(char *stream, char *limit, NString *word)
{
    /* skip leading whitespace */
    while (isspace((int) *stream) && *stream && stream != limit)
	stream++;
    word->ptr = stream;

    /* go to first whitespace or end of string */
    while (!isspace((int) *stream) && *stream && stream != limit) {
	if (*stream == '"' || *stream == '\'') {
	    char quote = *stream;
	    do
		stream++;
	    while (*stream != quote && *stream && stream != limit);
	    if (*stream && stream != limit)
		stream++;
	    break;
	} else
	    stream++;
    }
    word->length = stream - word->ptr;
    return stream;
}


/* get next SGML element "< ... >" and return next trailing stream */
static char *
NextElement(char *stream, NString *element)
{

    /* look for opening bracket */
    while (*stream != '<' && *stream)
	stream++;

    element->ptr = stream;

    /* look for closing bracket */
    while (*stream != '>' && *stream)
	stream++;

    element->length = stream - element->ptr;

    return *stream ? stream + 1 : stream;
}


/* seek next PARAM element content: "<PARAM ... >" (delimiters excluded)
   and return next trailing stream */
static char *
NextParam(char *stream, NString *param)
{
    NString element, word;

    do {
	stream = NextElement(stream, &element);

	if (element.length)
	    /* get element name */
	    (void) NextWord(element.ptr + 1, element.ptr + element.length - 1,
			    &word);
	else {
	    /* no more elements stop here */
	    param->ptr = 0;
	    param->length = 0;

	    return stream;
	}

	/* check if it's a PARAM element */
    } while(word.length != 5 && memcmp("PARAM", word.ptr, 5) != 0 && *stream);

    param->ptr = word.ptr + word.length;
    param->length = element.length - word.length - 1; /* delimiters excluded */

    return stream;
}


/* return literal value as a Null terminated string,
   removing possible quotes and extra whitespace */
static char *
GetLiteralValue(NString *literal)
{
    char *ptr, *limit, *value, *vptr;
    char quote;
    int skip;

    value = vptr = (char *)Malloc(literal->length + 1);
    if (!value)
	return 0;

    ptr = literal->ptr;
    limit = ptr + literal->length;
    quote = (*ptr == '"' || *ptr == '\'') ? *ptr++ : '\0';
    skip = 0;
    do
	if (isspace((int) *ptr)) {
	    if (!skip) {
		*vptr++ = ' ';
		skip = 1;
	    }
	    ptr++;
	} else {
	    skip = 0;
	    *vptr++ = *ptr++;
	}
    while (*ptr != quote && ptr != limit);
    *vptr = '\0';

    return value;
}


/* parse PARAM tag content: " NAME=... VALUE=... " */
static int
ParseParam(NString *param, char **name_ret, char **value_ret)
{
    NString word, name, value;
    char *stream = param->ptr;
    char *limit = param->ptr + param->length;

    /* look for the name part */
    do
	stream = NextChunk(stream, limit, &word);
    while (word.length < 5 && memcmp("NAME=", word.ptr, 5 != 0) && *stream);
    if (stream == limit)
	return 1;
    name.ptr = word.ptr + 5;
    name.length = word.length - 5;
    *name_ret = GetLiteralValue(&name);

    /* look for the value part */
    do
	stream = NextChunk(stream, limit, &word);
    while (word.length < 6 && memcmp("VALUE=", word.ptr, 6) != 0 && *stream);
    value.ptr = word.ptr + 6;
    value.length = word.length - 6;
    *value_ret = GetLiteralValue(&value);

    return 0;
}

/* how much we make a params array bigger every time we reach the limit */
#define PARAMSINC 10

/* read an RX document stream and augment the given list of arguments */
int
RxReadParams(char *stream,
	     char **argn_ret[], char **argv_ret[], int *argc_ret)
{
    char **argv, **argn;
    int argc, n;
    NString param;
    char *name, *value;
    int status;

    status = 0;
    argc = n = 0;
    argn = argv = NULL;
    if (stream != NULL) {
	do {
	    stream = NextParam(stream, &param);
	    if (param.length != 0 && ParseParam(&param, &name, &value) == 0) {
		argc++;
		if (n == 0) {	/* alloc first block */
		    n = PARAMSINC;
		    argn = (char **)Malloc(sizeof(char *) * n);
		    if (!argn)
			return 1;
		    argv = (char **)Malloc(sizeof(char *) * n);
		    if (!argv) {
			Free(argn);
			return 1;
		    }
		}
		if (argc % PARAMSINC == 0) { /* we need to add a block */
		    n += PARAMSINC;
		    argn = (char **)
		      Realloc(argn, sizeof(char *) * argc, sizeof(char *) * n);
		    argv = (char **)
		      Realloc(argv, sizeof(char *) * argc, sizeof(char *) * n);
		    if (!argn || !argv) {
			argc--;
			status = 1;
			break;
		    }
		}
		argn[argc - 1] = name;
		argv[argc - 1] = value;
	    }
	} while (*stream);
    }
    *argn_ret = argn;
    *argv_ret = argv;
    *argc_ret = argc;

    return status;
}
