
/* Expect depends on these Tcl functions, which have been removed
   in the latest version of Tcl/Tk 8.3. */

/* 
 * tclParse.c --
 *
 *      This file contains a collection of procedures that are used
 *      to parse Tcl commands or parts of commands (like quoted
 *      strings or nested sub-commands).
 *
 * Copyright (c) 1987-1993 The Regents of the University of California.
 * Copyright (c) 1994-1997 Sun Microsystems, Inc.
 *
 * See the file "license.terms" for information on usage and redistribution
 * of this file, and for a DISCLAIMER OF ALL WARRANTIES.
 *
 * RCS: @(#) tclParse-compat.c,v 1.1 2001/09/08 06:26:30 irox Exp
 */

/* Only do this for Tcl8.3 and above. */
#include "tcl.h"
#if TCL_MAJOR_VERSION == 8 && TCL_MINOR_VERSION > 2
#include "tclInt.h"


static char *QuoteEnd(char *string, char *lastChar, int term);
static char *VarNameEnd(char *string, char *lastChar);
static char *ScriptEnd(char *p, char *lastChar, int nested);
/*
 *----------------------------------------------------------------------
 *
 * TclWordEnd --
 *
 *	Given a pointer into a Tcl command, find the end of the next
 *	word of the command.
 *
 * Results:
 *	The return value is a pointer to the last character that's part
 *	of the word pointed to by "start".  If the word doesn't end
 *	properly within the string then the return value is the address
 *	of the null character at the end of the string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

char *
TclWordEnd(start, lastChar, nested, semiPtr)
    char *start;		/* Beginning of a word of a Tcl command. */
    char *lastChar;		/* Terminating character in string. */
    int nested;			/* Zero means this is a top-level command.
				 * One means this is a nested command (close
				 * bracket is a word terminator). */
    int *semiPtr;		/* Set to 1 if word ends with a command-
				 * terminating semi-colon, zero otherwise.
				 * If NULL then ignored. */
{
    register char *p;
    int count;

    if (semiPtr != NULL) {
	*semiPtr = 0;
    }

    /*
     * Skip leading white space (backslash-newline must be treated like
     * white-space, except that it better not be the last thing in the
     * command).
     */

    for (p = start; ; p++) {
	if (isspace(UCHAR(*p))) {
	    continue;
	}
	if ((p[0] == '\\') && (p[1] == '\n')) {
	    if (p+2 == lastChar) {
		return p+2;
	    }
	    continue;
	}
	break;
    }

    /*
     * Handle words beginning with a double-quote or a brace.
     */

    if (*p == '"') {
	p = QuoteEnd(p+1, lastChar, '"');
	if (p == lastChar) {
	    return p;
	}
	p++;
    } else if (*p == '{') {
	int braces = 1;
	while (braces != 0) {
	    p++;
	    while (*p == '\\') {
		(void) Tcl_Backslash(p, &count);
		p += count;
	    }
	    if (*p == '}') {
		braces--;
	    } else if (*p == '{') {
		braces++;
	    } else if (p == lastChar) {
		return p;
	    }
	}
	p++;
    }

    /*
     * Handle words that don't start with a brace or double-quote.
     * This code is also invoked if the word starts with a brace or
     * double-quote and there is garbage after the closing brace or
     * quote.  This is an error as far as Tcl_Eval is concerned, but
     * for here the garbage is treated as part of the word.
     */

    while (1) {
	if (*p == '[') {
	    p = ScriptEnd(p+1, lastChar, 1);
	    if (p == lastChar) {
		return p;
	    }
	    p++;
	} else if (*p == '\\') {
	    if (p[1] == '\n') {
		/*
		 * Backslash-newline:  it maps to a space character
		 * that is a word separator, so the word ends just before
		 * the backslash.
		 */

		return p-1;
	    }
	    (void) Tcl_Backslash(p, &count);
	    p += count;
	} else if (*p == '$') {
	    p = VarNameEnd(p, lastChar);
	    if (p == lastChar) {
		return p;
	    }
	    p++;
	} else if (*p == ';') {
	    /*
	     * Include the semi-colon in the word that is returned.
	     */

	    if (semiPtr != NULL) {
		*semiPtr = 1;
	    }
	    return p;
	} else if (isspace(UCHAR(*p))) {
	    return p-1;
	} else if ((*p == ']') && nested) {
	    return p-1;
	} else if (p == lastChar) {
	    if (nested) {
		/*
		 * Nested commands can't end because of the end of the
		 * string.
		 */
		return p;
	    }
	    return p-1;
	} else {
	    p++;
	}
    }
}

/*
 *----------------------------------------------------------------------
 *
 * QuoteEnd --
 *
 *	Given a pointer to a string that obeys the parsing conventions
 *	for quoted things in Tcl, find the end of that quoted thing.
 *	The actual thing may be a quoted argument or a parenthesized
 *	index name.
 *
 * Results:
 *	The return value is a pointer to the last character that is
 *	part of the quoted string (i.e the character that's equal to
 *	term).  If the quoted string doesn't terminate properly then
 *	the return value is a pointer to the null character at the
 *	end of the string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
QuoteEnd(string, lastChar, term)
    char *string;		/* Pointer to character just after opening
				 * "quote". */
    char *lastChar;		/* Terminating character in string. */
    int term;			/* This character will terminate the
				 * quoted string (e.g. '"' or ')'). */
{
    register char *p = string;
    int count;

    while (*p != term) {
	if (*p == '\\') {
	    (void) Tcl_Backslash(p, &count);
	    p += count;
	} else if (*p == '[') {
	    for (p++; *p != ']'; p++) {
		p = TclWordEnd(p, lastChar, 1, (int *) NULL);
		if (*p == 0) {
		    return p;
		}
	    }
	    p++;
	} else if (*p == '$') {
	    p = VarNameEnd(p, lastChar);
	    if (*p == 0) {
		return p;
	    }
	    p++;
	} else if (p == lastChar) {
	    return p;
	} else {
	    p++;
	}
    }
    return p-1;
}

/*
 *----------------------------------------------------------------------
 *
 * VarNameEnd --
 *
 *	Given a pointer to a variable reference using $-notation, find
 *	the end of the variable name spec.
 *
 * Results:
 *	The return value is a pointer to the last character that
 *	is part of the variable name.  If the variable name doesn't
 *	terminate properly then the return value is a pointer to the
 *	null character at the end of the string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
VarNameEnd(string, lastChar)
    char *string;		/* Pointer to dollar-sign character. */
    char *lastChar;		/* Terminating character in string. */
{
    register char *p = string+1;

    if (*p == '{') {
	for (p++; (*p != '}') && (p != lastChar); p++) {
	    /* Empty loop body. */
	}
	return p;
    }
    while (isalnum(UCHAR(*p)) || (*p == '_')) {
	p++;
    }
    if ((*p == '(') && (p != string+1)) {
	return QuoteEnd(p+1, lastChar, ')');
    }
    return p-1;
}


/*
 *----------------------------------------------------------------------
 *
 * ScriptEnd --
 *
 *	Given a pointer to the beginning of a Tcl script, find the end of
 *	the script.
 *
 * Results:
 *	The return value is a pointer to the last character that's part
 *	of the script pointed to by "p".  If the command doesn't end
 *	properly within the string then the return value is the address
 *	of the null character at the end of the string.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */

static char *
ScriptEnd(p, lastChar, nested)
    char *p;			/* Script to check. */
    char *lastChar;		/* Terminating character in string. */
    int nested;			/* Zero means this is a top-level command.
				 * One means this is a nested command (the
				 * last character of the script must be
				 * an unquoted ]). */
{
    int commentOK = 1;
    int length;

    while (1) {
	while (isspace(UCHAR(*p))) {
	    if (*p == '\n') {
		commentOK = 1;
	    }
	    p++;
	}
	if ((*p == '#') && commentOK) {
	    do {
		if (*p == '\\') {
		    /*
		     * If the script ends with backslash-newline, then
		     * this command isn't complete.
		     */

		    if ((p[1] == '\n') && (p+2 == lastChar)) {
			return p+2;
		    }
		    Tcl_Backslash(p, &length);
		    p += length;
		} else {
		    p++;
		}
	    } while ((p != lastChar) && (*p != '\n'));
	    continue;
	}
	p = TclWordEnd(p, lastChar, nested, &commentOK);
	if (p == lastChar) {
	    return p;
	}
	p++;
	if (nested) {
	    if (*p == ']') {
		return p;
	    }
	} else {
	    if (p == lastChar) {
		return p-1;
	    }
	}
    }
}

#endif /* Tcl8.3 and above. */
