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

#if 0
/* The following functions implement expect's glob-style string matching */
/* Exp_StringMatch allow's implements the unanchored front (or conversely */
/* the '^') feature.  Exp_StringMatch2 does the rest of the work. */
int	/* returns # of chars that matched */
Exp_StringMatch(string, pattern,offset)
char *string;
char *pattern;
int *offset;	/* offset from beginning of string where pattern matches */
{
	char *s;
	int sm;	/* count of chars matched or -1 */
	int caret = FALSE;

	*offset = 0;

	if (pattern[0] == '^') {
		caret = TRUE;
		pattern++;
	}

	sm = Exp_StringMatch2(string,pattern);
	if (sm >= 0) return(sm);

	if (caret) return(-1);

	if (pattern[0] == '*') return(-1);

	for (s = string;*s;s++) {
 		sm = Exp_StringMatch2(s,pattern);
		if (sm != -1) {
			*offset = s-string;
			return(sm);
		}
	}
	return(-1);
}
#endif

/* The following functions implement expect's glob-style string matching */
/* Exp_StringMatch allow's implements the unanchored front (or conversely */
/* the '^') feature.  Exp_StringMatch2 does the rest of the work. */
int	/* returns # of chars that matched */
Exp_StringMatch(string, pattern,offset)
char *string;
char *pattern;
int *offset;	/* offset from beginning of string where pattern matches */
{
	char *s;
	int sm;	/* count of chars matched or -1 */
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

	sm = Exp_StringMatch2(string,pattern);
	if (sm >= 0) return(sm);

	if (caret) return -1;
	if (star) return -1;

	if (*string == '\0') return -1;

	for (s = string+1;*s;s++) {
 		sm = Exp_StringMatch2(s,pattern);
		if (sm != -1) {
			*offset = s-string;
			return(sm);
		}
	}
	return -1;
}

/* Exp_StringMatch2 --

Like Tcl_StringMatch except that
1) returns number of characters matched, -1 if failed.
	(Can return 0 on patterns like "" or "$")
2) does not require pattern to match to end of string
3) much of code is stolen from Tcl_StringMatch
4) front-anchor is assumed (Tcl_StringMatch retries for non-front-anchor)
*/

int Exp_StringMatch2(string,pattern)
    register char *string;	/* String. */
    register char *pattern;	/* Pattern, which may contain
				 * special characters. */
{
    char c2;
    int match = 0;	/* # of chars matched */

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
#if 1
	    int head_len;
	    char *tail;
#endif
	    pattern += 1;
	    if (*pattern == 0) {
		return(strlen(string)+match); /* DEL */
	    }
#if 1
	    /* find longest match - switched to this on 12/31/93 */
	    head_len = strlen(string);	/* length before tail */
	    tail = string + head_len;
	    while (head_len >= 0) {
		int rc;

		if (-1 != (rc = Exp_StringMatch2(tail, pattern))) {
		    return rc + match + head_len;	/* DEL */
		}
		tail--;
		head_len--;
	    }
#else
	    /* find shortest match */
	    while (*string != 0) {
		int rc;					/* DEL */

		if (-1 != (rc = Exp_StringMatch2(string, pattern))) {
		    return rc+match;		/* DEL */
		}
		string += 1;
		match++;				/* DEL */
	    }
	    if (*pattern == '$') return 0;	/* handle *$ */
#endif
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
	    goto thisCharOK;
	}

	/* Check for a "[" as the next pattern character.  It is followed
	 * by a list of characters that are acceptable, or by a range
	 * (two characters separated by "-").
	 */
	
	if (*pattern == '[') {
	    pattern += 1;
	    while (1) {
		if ((*pattern == ']') || (*pattern == 0)) {
		    return -1;			/* was 0; DEL */
		}
		if (*pattern == *string) {
		    break;
		}
		if (pattern[1] == '-') {
		    c2 = pattern[2];
		    if (c2 == 0) {
			return -1;		/* DEL */
		    }
		    if ((*pattern <= *string) && (c2 >= *string)) {
			break;
		    }
		    if ((*pattern >= *string) && (c2 <= *string)) {
			break;
		    }
		    pattern += 2;
		}
		pattern += 1;
	    }

/* OOPS! Found a bug in vanilla Tcl - have sent back to Ousterhout */
/* but he hasn't integrated it yet. - DEL */

#if 0
	    while ((*pattern != ']') && (*pattern != 0)) {
#else
	    while (*pattern != ']') {
		if (*pattern == 0) {
		    pattern--;
		    break;
	        }
#endif
		pattern += 1;
	    }
	    goto thisCharOK;
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
	
	if (*pattern != *string) {
	    return -1;
	}

	thisCharOK: pattern += 1;
	string += 1;
	match++;
    }
}

