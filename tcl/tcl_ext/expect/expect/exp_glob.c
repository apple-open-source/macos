/* exp_glob.c - expect functions for doing glob

Based on Tcl's glob functions but modified to support anchors and to
return information about the possibility of future matches

Modifications by: Don Libes, NIST, 2/6/90

Design and implementation of this program was paid for by U.S. tax
dollars.  Therefore it is public domain.  However, the author and NIST
would appreciate credit if this program or parts of it are used.

*/

#include "expect_cf.h"
#include "tcl.h"
#include "exp_int.h"

/* The following functions implement expect's glob-style string matching */
/* Exp_StringMatch allow's implements the unanchored front (or conversely */
/* the '^') feature.  Exp_StringMatch2 does the rest of the work. */
int	/* returns # of BYTES that matched */
Exp_StringCaseMatch(string, pattern, nocase, offset)		/* INTL */
char *string;
char *pattern;
int nocase;
int *offset;	/* offset in bytes from beginning of string where pattern matches */
{
	CONST char *s;
	int sm;	/* count of bytes matched or -1 */
	int caret = FALSE;
	int star = FALSE;

	*offset = 0;

	if (pattern[0] == '^') {
		caret = TRUE;
		pattern++;
	} else if (pattern[0] == '*') {
		star = TRUE;
	}

	/*
	 * test if pattern matches in initial position.
	 * This handles front-anchor and 1st iteration of non-front-anchor.
	 * Note that 1st iteration must be tried even if string is empty.
	 */

	sm = Exp_StringCaseMatch2(string,pattern, nocase);
	if (sm >= 0) return(sm);

	if (caret) return -1;
	if (star) return -1;

	if (*string == '\0') return -1;

	for (s = Tcl_UtfNext(string);*s;s = Tcl_UtfNext(s)) {
 		sm = Exp_StringCaseMatch2(s,pattern, nocase);
		if (sm != -1) {
			*offset = s-string;
			return(sm);
		}
	}
	return -1;
}

/* Exp_StringCaseMatch2 --

Like Tcl_StringCaseMatch except that
1) returns number of characters matched, -1 if failed.
	(Can return 0 on patterns like "" or "$")
2) does not require pattern to match to end of string
3) much of code is stolen from Tcl_StringMatch
4) front-anchor is assumed (Tcl_StringMatch retries for non-front-anchor)
*/

int Exp_StringCaseMatch2(string,pattern, nocase)	/* INTL */
    register CONST char *string;	/* String. */
    register CONST char *pattern;	/* Pattern, which may contain
				 * special characters. */
    int nocase;
{
    Tcl_UniChar ch1, ch2;
    int match = 0;	/* # of bytes matched */
    CONST char *oldString;

    CONST char *pstart = pattern;

    while (1) {
	/* If at end of pattern, success! */
	if (*pattern == 0) {
		return match;
	}

	/* If last pattern character is '$', verify that entire
	 * string has been matched.
	 */
	if ((*pattern == '$') && (pattern[1] == 0)) {
		if (*string == 0) return(match);
		else return(-1);		
	}

	/* Check for a "*" as the next pattern character.  It matches
	 * any substring.  We handle this by calling ourselves
	 * recursively for each postfix of string, until either we
	 * match or we reach the end of the string.
	 */
	
	if (*pattern == '*') {
	    CONST char *tail;

	    pattern += 1;
	    if (*pattern == 0) {
		return(strlen(string)+match); /* DEL */
	    }

	    /* find LONGEST match */
	    tail = string + strlen(string);
	    while (1) {
		int rc;

		if (-1 != (rc = Exp_StringCaseMatch2(tail, pattern, nocase))) {
		    return match + (tail - string) + rc;
		    /* match = # of bytes we've skipped before this */
		    /* (...) = # of bytes we've skipped due to "*" */
		    /* rc    = # of bytes we've matched after "*" */
		}

		/* if we've backed up to beginning of string, give up */
		if (tail == string) break;
		tail = Tcl_UtfPrev(tail,string);
	    }
	    return -1;					/* DEL */
	}
    
	/*
	 * after this point, all patterns must match at least one
	 * character, so check this
	 */

	if (*string == 0) return -1;

	/* Check for a "?" as the next pattern character.  It matches
	 * any single character.
	 */

	if (*pattern == '?') {
	    pattern++;
	    oldString = string;
	    string = Tcl_UtfNext(string);
	    match += (string - oldString); /* incr by # of bytes in char */
	    continue;
	}

	/* Check for a "[" as the next pattern character.  It is followed
	 * by a list of characters that are acceptable, or by a range
	 * (two characters separated by "-").
	 */
	
	if (*pattern == '[') {
	    Tcl_UniChar ch, startChar, endChar;

	    pattern++;
	    oldString = string;
	    string += Tcl_UtfToUniChar(string, &ch);

	    while (1) {
		if ((*pattern == ']') || (*pattern == '\0')) {
		    return -1;			/* was 0; DEL */
		}
		pattern += Tcl_UtfToUniChar(pattern, &startChar);
		if (nocase) {
		    startChar = Tcl_UniCharToLower(startChar);
		}
		if (*pattern == '-') {
		    pattern++;
		    if (*pattern == '\0') {
			return -1;		/* DEL */
		    }
		    pattern += Tcl_UtfToUniChar(pattern, &endChar);
		    if (nocase) {
			endChar = Tcl_UniCharToLower(endChar);
		    }
		    if (((startChar <= ch) && (ch <= endChar))
			    || ((endChar <= ch) && (ch <= startChar))) {
			/*
			 * Matches ranges of form [a-z] or [z-a].
			 */

			break;
		    }
		} else if (startChar == ch) {
		    break;
		}
	    }
	    while (*pattern != ']') {
		if (*pattern == '\0') {
		    pattern = Tcl_UtfPrev(pattern, pstart);
		    break;
		}
		pattern = Tcl_UtfNext(pattern);
	    }
	    pattern++;
	    match += (string - oldString); /* incr by # of bytes in char */
	    continue;
	}
 
	/* If the next pattern character is backslash, strip it off
	 * so we do exact matching on the character that follows.
	 */
	
	if (*pattern == '\\') {
	    pattern += 1;
	    if (*pattern == 0) {
		return -1;
	    }
	}

	/* There's no special character.  Just make sure that the next
	 * characters of each string match.
	 */
	
	oldString = string;
	string  += Tcl_UtfToUniChar(string, &ch1);
	pattern += Tcl_UtfToUniChar(pattern, &ch2);
	if (nocase) {
	    if (Tcl_UniCharToLower(ch1) != Tcl_UniCharToLower(ch2)) {
		return -1;
	    }
	} else if (ch1 != ch2) {
	    return -1;
	}
	match += (string - oldString);  /* incr by # of bytes in char */
    }
}
