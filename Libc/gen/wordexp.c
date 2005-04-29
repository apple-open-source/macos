/*
 *   Copyright 1994, University Corporation for Atmospheric Research
 *   See ../COPYRIGHT file for copying and redistribution conditions.
 */
/*
 * Reproduction of ../COPYRIGHT file:
 *
 *********************************************************************
 
Copyright 1995-2002 University Corporation for Atmospheric Research/Unidata

Portions of this software were developed by the Unidata Program at the 
University Corporation for Atmospheric Research.

Access and use of this software shall impose the following obligations
and understandings on the user. The user is granted the right, without
any fee or cost, to use, copy, modify, alter, enhance and distribute
this software, and any derivative works thereof, and its supporting
documentation for any purpose whatsoever, provided that this entire
notice appears in all copies of the software, derivative works and
supporting documentation.  Further, UCAR requests that the user credit
UCAR/Unidata in any publications that result from the use of this
software or in any product that includes this software. The names UCAR
and/or Unidata, however, may not be used in any advertising or publicity
to endorse or promote any products or commercial entity unless specific
written permission is obtained from UCAR/Unidata. The user also
understands that UCAR/Unidata is not obligated to provide the user with
any support, consulting, training or assistance of any kind with regard
to the use, operation and performance of this software nor to provide
the user with any updates, revisions, new versions or "bug fixes."

THIS SOFTWARE IS PROVIDED BY UCAR/UNIDATA "AS IS" AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL UCAR/UNIDATA BE LIABLE FOR ANY SPECIAL,
INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
WITH THE ACCESS, USE OR PERFORMANCE OF THIS SOFTWARE.

 *********************************************************************
 */

/* $Id: wordexp.c,v 1.13 2002/12/26 16:46:46 steve Exp $ */

#if 0
#include "ldmconfig.h"
#endif

/*
 * Hack to provide POSIX 1003.2-1992 _interface_.
 * NOT fully functional
 */

#include "xlocale_private.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "wordexp.h"



/*
 * Translate return value from wordexp() into a string
 */
const char *
s_wrde_err(int wrde_err)
{
	switch(wrde_err) {
	case 0:	return "No Error";
	case WRDE_BADCHAR: return "WRDE_BADCHAR";
	case WRDE_BADVAL: return "WRDE_BADVAL";
	case WRDE_CMDSUB: return "WRDE_CMDSUB";
	case WRDE_NOSPACE: return "WRDE_NOSPACE";
	case WRDE_SYNTAX: return "WRDE_SYNTAX";
	}
	/* default */
	return "Unknown Error";
}


/*ARGSUSED*/
int
wordexp(const char *words, wordexp_t *pwordexp, int flags)
{
	const char *ccp;
	char **argv;
	const char *buf;
	size_t argc;
        enum states {ARGSTART, IN_QUOTE, NOT_IN_QUOTE, DONE};
        enum classes {EOS, SPACE, QUOTE, OTHER};
        int state = ARGSTART;
        char *argbuf;
        const char *sp;
        char *dp;
        int status = 0;
	locale_t loc = __current_locale();
        
	/* devour leading white space */
	for(ccp = words; *ccp != 0 && isspace_l(*ccp, loc); )
	    ccp++;
	/* skip comments */
	if(*ccp == '#')
	{
		pwordexp->we_wordc = 0;
		pwordexp->we_wordv = NULL;
		return 0;
	}

/* If every other character was a space ... */
#define MAXNARGS(str) ((strlen(str) +1)/2 +1)
	argv = (char **)calloc(MAXNARGS(ccp), sizeof(char *));
	if(argv == NULL)
		return WRDE_NOSPACE;

	buf = ccp;

        argbuf = malloc(strlen(words)+1); /* where each arg is built */
        if (argbuf == NULL)
        {
            free(argv);
            return WRDE_NOSPACE;
        }

        sp = buf;
        dp = argbuf;
        argc = 0;
	while(state != DONE)
	{
            int class;

            if (*sp == 0)
                class = EOS;
            else if (isspace_l(*sp, loc))
                class = SPACE;
            else if (*sp == '"')
                class = QUOTE;
            else
                class = OTHER;
            switch (state) {
                case ARGSTART:
                    switch(class) {
                        case EOS:
                            state = DONE;
                            break;
                        case SPACE:
                            sp++;
                            break;
                        case QUOTE:
                            sp++;
                            state = IN_QUOTE;
                            break;
                        case OTHER:
                            *dp++ = *sp++;
                            state = NOT_IN_QUOTE;
                            break;
                    }
                    break;
                case IN_QUOTE:
                    switch(class) {
                        case EOS: /* unmatched quote */
                            state = DONE;
                            status = WRDE_SYNTAX;
                            break;
                        case QUOTE:
                            sp++;
                            state = NOT_IN_QUOTE;
                            break;
                        case SPACE:
                        case OTHER:
                            *dp++ = *sp++;
                            break;
                    }
                    break;
                case NOT_IN_QUOTE:
                    switch(class) {
                        case EOS:
                            *dp = 0;
                            dp = argbuf;
                            argv[argc++] = strdup(argbuf);
                            state = DONE;
                            break;
                        case SPACE:
                            *dp = 0;
                            dp = argbuf;
                            argv[argc++] = strdup(argbuf);
                            sp++;
                            state = ARGSTART;
                            break;
                        case QUOTE:
                            sp++;
                            state = IN_QUOTE;
                            break;
                        case OTHER:
                            *dp++ = *sp++;
                            break;
                    }
                    break;
            }
	}
	argv[argc] = NULL;

	pwordexp->we_wordc = argc;
	pwordexp->we_wordv = argv;
	
        free(argbuf);
        
	return status;
}


void
wordfree(wordexp_t *pwordexp)
{
	if(pwordexp == NULL || pwordexp->we_wordv == NULL)
		return;
	if(*pwordexp->we_wordv)
		free(*pwordexp->we_wordv);
	free(pwordexp->we_wordv);
}


#if TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main(int argc, char *argv[])
{
	char strbuf[1024];
	wordexp_t wrdexp;
	int status;
	char **cpp;

	while(fgets(strbuf, sizeof(strbuf), stdin) != NULL)
	{
		{
			char *cp = strrchr(strbuf,'\n');
			if(cp)
				*cp = 0;
		}
		fprintf(stdout, "\t%s\n", strbuf);
		status = wordexp(strbuf, &wrdexp, WRDE_SHOWERR);
		if(status)
		{
			fprintf(stderr, "wordexp: %s\n", s_wrde_err(status));
			continue;
		}
		/* else */
		fprintf(stdout, "\t%d:\n", wrdexp.we_wordc);
		for(cpp = wrdexp.we_wordv;
			cpp < &wrdexp.we_wordv[wrdexp.we_wordc]; cpp++)
		{
			fprintf(stdout, "\t\t%s\n", *cpp);
		}
		wordfree(&wrdexp);
		
	}
	exit(EXIT_SUCCESS);
}

#endif /* TEST */
