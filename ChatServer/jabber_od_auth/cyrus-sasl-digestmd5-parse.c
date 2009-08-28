/* 
 * Copyright (c) 1998-2003 Carnegie Mellon University.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. The name "Carnegie Mellon University" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For permission or any other legal
 *    details, please contact  
 *      Office of Technology Transfer
 *      Carnegie Mellon University
 *      5000 Forbes Avenue
 *      Pittsburgh, PA  15213-3890
 *      (412) 268-4387, fax: (412) 268-7395
 *      tech-transfer@andrew.cmu.edu
 *
 * 4. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by Computing Services
 *     at Carnegie Mellon University (http://www.cmu.edu/computing/)."
 *
 * CARNEGIE MELLON UNIVERSITY DISCLAIMS ALL WARRANTIES WITH REGARD TO
 * THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL CARNEGIE MELLON UNIVERSITY BE LIABLE
 * FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <string.h>
#include "cyrus-sasl-digestmd5-parse.h"

/* this code taken from cyrus-sasl */

#define HT	(9)
#define CR	(13)
#define LF	(10)
#define SP	(32)
#define DEL	(127)

static char *skip_lws (char *s)
{
    if (!s) return NULL;
    
    /* skipping spaces: */
    while (s[0] == ' ' || s[0] == HT || s[0] == CR || s[0] == LF) {
	if (s[0] == '\0') break;
	s++;
    }  
    
    return s;
}

static char *skip_token (char *s, int caseinsensitive)
{
    if(!s) return NULL;
    
    while (s[0]>SP) {
	if (s[0]==DEL || s[0]=='(' || s[0]==')' || s[0]=='<' || s[0]=='>' ||
	    s[0]=='@' || s[0]==',' || s[0]==';' || s[0]==':' || s[0]=='\\' ||
	    s[0]=='\'' || s[0]=='/' || s[0]=='[' || s[0]==']' || s[0]== '?' ||
	    s[0]=='=' || s[0]== '{' || s[0]== '}') {
	    if (caseinsensitive == 1) {
		if (!isupper((unsigned char) s[0]))
		    break;
	    } else {
		break;
	    }
	}
	s++;
    }  
    return s;
}

/* NULL - error (unbalanced quotes), 
   otherwise pointer to the first character after the value.
   The function performs work in place. */
static char *unquote (char *qstr)
{
    char *endvalue;
    int   escaped = 0;
    char *outptr;
    
    if(!qstr) return NULL;
    
    if (qstr[0] == '"') {
	qstr++;
	outptr = qstr;
	
	for (endvalue = qstr; endvalue[0] != '\0'; endvalue++, outptr++) {
	    if (escaped) {
		outptr[0] = endvalue[0];
		escaped = 0;
	    }
	    else if (endvalue[0] == '\\') {
		escaped = 1;
		outptr--; /* Will be incremented at the end of the loop */
	    }
	    else if (endvalue[0] == '"') {
		break;
	    }      
	    else {
		outptr[0] = endvalue[0];      
	    }
	}
	
	if (endvalue[0] != '"') {
	    return NULL;
	}
	
	while (outptr <= endvalue) {
	    outptr[0] = '\0';
	    outptr++;
	}
	endvalue++;
    }
    else { /* not qouted value (token) */
	/* qstr already contains output */
	endvalue = skip_token(qstr,0);
    };
    
    return endvalue;  
}

static void get_pair(char **in, char **name, char **value)
{
    char  *endpair;
    char  *curp = *in;
    *name = NULL;
    *value = NULL;

    if (curp == NULL) return;
    if (curp[0] == '\0') return;

    /* skipping spaces: */
    curp = skip_lws(curp);

    *name = curp;

    curp = skip_token(curp,1);

    /* strip wierd chars */
    if (curp[0] != '=' && curp[0] != '\0') {
    *curp++ = '\0';
    };

    curp = skip_lws(curp);

    if (curp[0] != '=') { /* No '=' sign */
    *name = NULL;
    return;
    }

    curp[0] = '\0';
    curp++;

    curp = skip_lws(curp);

    *value = (curp[0] == '"') ? curp+1 : curp;

    endpair = unquote (curp);
    if (endpair == NULL) { /* Unbalanced quotes */
    *name = NULL;
    return;
    }
    if (endpair[0] != ',') {
    if (endpair[0]!='\0') {
        *endpair++ = '\0';
    }
    }

    endpair = skip_lws(endpair);

    /* syntax check: MUST be '\0' or ',' */
    if (endpair[0] == ',') {
    endpair[0] = '\0';
    endpair++; /* skipping <,> */
    } else if (endpair[0] != '\0') {
    *name = NULL;
    return;
    }

    *in = endpair;
}

void
ODKGetPair(char **in, char **name, char **value)
{
    return get_pair(in, name, value);
}

