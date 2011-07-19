/*
 * This program is copyright Alec Muffett 1993. The author disclaims all 
 * responsibility or liability with respect to it's usage or its effect 
 * upon hardware or computer systems, and maintains copyright as set out 
 * in the "LICENCE" document which accompanies distributions of Crack v4.0 
 * and upwards.
 */

#include <string.h>
#include <stdarg.h>

#ifndef IN_CRACKLIB

#include "crack.h"

#else

#include "packer.h"

static char __unused vers_id[] = "rules.c : v5.0p3 Alec Muffett 20 May 1993";

static void
Debug(int __unused val, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
}

#endif

#define RULE_NOOP	':'
#define RULE_PREPEND	'^'
#define RULE_APPEND	'$'
#define RULE_REVERSE	'r'
#define RULE_UPPERCASE	'u'
#define RULE_LOWERCASE	'l'
#define RULE_PLURALISE	'p'
#define RULE_CAPITALISE	'c'
#define RULE_DUPLICATE	'd'
#define RULE_REFLECT	'f'
#define RULE_SUBSTITUTE	's'
#define RULE_MATCH	'/'
#define RULE_NOT	'!'
#define RULE_LT		'<'
#define RULE_GT		'>'
#define RULE_EXTRACT	'x'
#define RULE_OVERSTRIKE	'o'
#define RULE_INSERT	'i'
#define RULE_EQUALS	'='
#define RULE_PURGE	'@'
#define RULE_CLASS	'?'	/* class rule? socialist ethic in cracker? */

#define RULE_DFIRST	'['
#define RULE_DLAST	']'
#define RULE_MFIRST	'('
#define RULE_MLAST	')'

int
Suffix(myword, suffix)
    char *myword;
    char *suffix;
{
    register int i;
    register int j;
    i = strlen(myword);
    j = strlen(suffix);

    if (i > j)
    {
	return (STRCMP((myword + i - j), suffix));
    } else
    {
	return (-1);
    }
}

char *
Reverse(str)			/* return a pointer to a reversal */
    register char *str;
{
    register int i;
    register int j;
    static char area[STRINGSIZE];
    j = i = strlen(str);
    while (*str)
    {
	area[--i] = *str++;
    }
    area[j] = '\0';
    return (area);
}

char *
Uppercase(str)			/* return a pointer to an uppercase */
    register char *str;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;
    while (*str)
    {
	*(ptr++) = CRACK_TOUPPER(*str);
	str++;
    }
    *ptr = '\0';

    return (area);
}

char *
Lowercase(str)			/* return a pointer to an lowercase */
    register char *str;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;
    while (*str)
    {
	*(ptr++) = CRACK_TOLOWER(*str);
	str++;
    }
    *ptr = '\0';

    return (area);
}

char *
Capitalise(str)			/* return a pointer to an capitalised */
    register char *str;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;

    while (*str)
    {
	*(ptr++) = CRACK_TOLOWER(*str);
	str++;
    }

    *ptr = '\0';
    area[0] = CRACK_TOUPPER(area[0]);
    return (area);
}

char *
Pluralise(string)		/* returns a pointer to a plural */
    register char *string;
{
    register int length;
    static char area[STRINGSIZE];
    length = strlen(string);
    strlcpy(area, string, sizeof(area));

    if (!Suffix(string, "ch") ||
	!Suffix(string, "ex") ||
	!Suffix(string, "ix") ||
	!Suffix(string, "sh") ||
	!Suffix(string, "ss"))
    {
	/* bench -> benches */
	strlcat(area, "es", sizeof(area));
    } else if (length > 2 && string[length - 1] == 'y')
    {
	if (strchr("aeiou", string[length - 2]))
	{
	    /* alloy -> alloys */
	    strlcat(area, "s", sizeof(area));
	} else
	{
	    /* gully -> gullies */
	    strlcpy(area + length - 1, "ies", sizeof(area)-length+1);
	}
    } else if (string[length - 1] == 's')
    {
	/* bias -> biases */
	strlcat(area, "es", sizeof(area));
    } else
    {
	/* catchall */
	strlcat(area, "s", sizeof(area));
    }

    return (area);
}

char *
Substitute(string, old, new)	/* returns pointer to a swapped about copy */
    register char *string;
    register char old;
    register char new;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;
    while (*string)
    {
	*(ptr++) = (*string == old ? new : *string);
	string++;
    }
    *ptr = '\0';
    return (area);
}

char *
Purge(string, target)		/* returns pointer to a purged copy */
    register char *string;
    register char target;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;
    while (*string)
    {
	if (*string != target)
	{
	    *(ptr++) = *string;
	}
	string++;
    }
    *ptr = '\0';
    return (area);
}
/* -------- CHARACTER CLASSES START HERE -------- */

/*
 * this function takes two inputs, a class identifier and a character, and
 * returns non-null if the given character is a member of the class, based
 * upon restrictions set out below
 */

int
MatchClass(class, input)
    register char class;
    register char input;
{
    register char c;
    register int retval;
    retval = 0;

    switch (class)
    {
	/* ESCAPE */

    case '?':			/* ?? -> ? */
	if (input == '?')
	{
	    retval = 1;
	}
	break;

	/* ILLOGICAL GROUPINGS (ie: not in ctype.h) */

    case 'V':
    case 'v':			/* vowels */
	c = CRACK_TOLOWER(input);
	if (strchr("aeiou", c))
	{
	    retval = 1;
	}
	break;

    case 'C':
    case 'c':			/* consonants */
	c = CRACK_TOLOWER(input);
	if (strchr("bcdfghjklmnpqrstvwxyz", c))
	{
	    retval = 1;
	}
	break;

    case 'W':
    case 'w':			/* whitespace */
	if (strchr("\t ", input))
	{
	    retval = 1;
	}
	break;

    case 'P':
    case 'p':			/* punctuation */
	if (strchr(".`,:;'!?\"", input))
	{
	    retval = 1;
	}
	break;

    case 'S':
    case 's':			/* symbols */
	if (strchr("$%%^&*()-_+=|\\[]{}#@/~", input))
	{
	    retval = 1;
	}
	break;

	/* LOGICAL GROUPINGS */

    case 'L':
    case 'l':			/* lowercase */
	if (islower(input))
	{
	    retval = 1;
	}
	break;

    case 'U':
    case 'u':			/* uppercase */
	if (isupper(input))
	{
	    retval = 1;
	}
	break;

    case 'A':
    case 'a':			/* alphabetic */
	if (isalpha(input))
	{
	    retval = 1;
	}
	break;

    case 'X':
    case 'x':			/* alphanumeric */
	if (isalnum(input))
	{
	    retval = 1;
	}
	break;

    case 'D':
    case 'd':			/* digits */
	if (isdigit(input))
	{
	    retval = 1;
	}
	break;

    default:
	Debug(1, "MatchClass: unknown class %c\n", class);
	return (0);
	break;
    }

    if (isupper(class))
    {
	return (!retval);
    }
    return (retval);
}

char *
PolyStrchr(string, class)
    register char *string;
    register char class;
{
    while (*string)
    {
	if (MatchClass(class, *string))
	{
	    return (string);
	}
	string++;
    }
    return ((char *) 0);
}

char *
PolySubst(string, class, new)	/* returns pointer to a swapped about copy */
    register char *string;
    register char class;
    register char new;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;
    while (*string)
    {
	*(ptr++) = (MatchClass(class, *string) ? new : *string);
	string++;
    }
    *ptr = '\0';
    return (area);
}

char *
PolyPurge(string, class)	/* returns pointer to a purged copy */
    register char *string;
    register char class;
{
    register char *ptr;
    static char area[STRINGSIZE];
    ptr = area;
    while (*string)
    {
	if (!MatchClass(class, *string))
	{
	    *(ptr++) = *string;
	}
	string++;
    }
    *ptr = '\0';
    return (area);
}
/* -------- BACK TO NORMALITY -------- */

int
Char2Int(character)
    char character;
{
    if (isdigit(character))
    {
	return (character - '0');
    } else if (islower(character))
    {
	return (character - 'a' + 10);
    } else if (isupper(character))
    {
	return (character - 'A' + 10);
    }
    return (-1);
}

char *
Mangle(input, control)		/* returns a pointer to a controlled Mangle */
    char *input;
    char *control;
{
    int limit;
    register char *ptr;
    static char area[STRINGSIZE];
    char area2[STRINGSIZE];
    area[0] = '\0';
    strlcpy(area, input, sizeof(area));

    for (ptr = control; *ptr; ptr++)
    {
	switch (*ptr)
	{
	case RULE_NOOP:
	    break;
	case RULE_REVERSE:
	    strlcpy(area, Reverse(area), sizeof(area));
	    break;
	case RULE_UPPERCASE:
	    strlcpy(area, Uppercase(area), sizeof(area));
	    break;
	case RULE_LOWERCASE:
	    strlcpy(area, Lowercase(area), sizeof(area));
	    break;
	case RULE_CAPITALISE:
	    strlcpy(area, Capitalise(area), sizeof(area));
	    break;
	case RULE_PLURALISE:
	    strlcpy(area, Pluralise(area), sizeof(area));
	    break;
	case RULE_REFLECT:
	    strlcat(area, Reverse(area), sizeof(area));
	    break;
	case RULE_DUPLICATE:
	    strlcpy(area2, area, sizeof(area2));
	    strlcat(area, area2, sizeof(area));
	    break;
	case RULE_GT:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: '>' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		limit = Char2Int(*(++ptr));
		if (limit < 0)
		{
		    Debug(1, "Mangle: '>' weird argument in '%s'\n", control);
		    return ((char *) 0);
		}
		if (strlen(area) <= limit)
		{
		    return ((char *) 0);
		}
	    }
	    break;
	case RULE_LT:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: '<' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		limit = Char2Int(*(++ptr));
		if (limit < 0)
		{
		    Debug(1, "Mangle: '<' weird argument in '%s'\n", control);
		    return ((char *) 0);
		}
		if (strlen(area) >= limit)
		{
		    return ((char *) 0);
		}
	    }
	    break;
	case RULE_PREPEND:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: prepend missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		area2[0] = *(++ptr);
		strlcpy(area2 + 1, area, sizeof(area2)-1);
		strlcpy(area, area2, sizeof(area));
	    }
	    break;
	case RULE_APPEND:
	    if (!ptr[1])
	    {
		Debug(1, "Mangle: append missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		register char *string;
		string = area;
		while (*(string++));
		string[-1] = *(++ptr);
		*string = '\0';
	    }
	    break;
	case RULE_EXTRACT:
	    if (!ptr[1] || !ptr[2])
	    {
		Debug(1, "Mangle: extract missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		register int i;
		int start;
		int length;
		start = Char2Int(*(++ptr));
		length = Char2Int(*(++ptr));
		if (start < 0 || length < 0)
		{
		    Debug(1, "Mangle: extract: weird argument in '%s'\n", control);
		    return ((char *) 0);
		}
		strlcpy(area2, area, sizeof(area2));
		for (i = 0; length-- && area2[start + i]; i++)
		{
		    area[i] = area2[start + i];
		}
		/* cant use strncpy() - no trailing NUL */
		area[i] = '\0';
	    }
	    break;
	case RULE_OVERSTRIKE:
	    if (!ptr[1] || !ptr[2])
	    {
		Debug(1, "Mangle: overstrike missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		register int i;
		i = Char2Int(*(++ptr));
		if (i < 0)
		{
		    Debug(1, "Mangle: overstrike weird argument in '%s'\n",
			  control);
		    return ((char *) 0);
		} else
		{
		    ++ptr;
		    if (area[i])
		    {
			area[i] = *ptr;
		    }
		}
	    }
	    break;
	case RULE_INSERT:
	    if (!ptr[1] || !ptr[2])
	    {
		Debug(1, "Mangle: insert missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		register int i;
		register char *p1;
		register char *p2;
		i = Char2Int(*(++ptr));
		if (i < 0)
		{
		    Debug(1, "Mangle: insert weird argument in '%s'\n",
			  control);
		    return ((char *) 0);
		}
		p1 = area;
		p2 = area2;
		while (i && *p1)
		{
		    i--;
		    *(p2++) = *(p1++);
		}
		*(p2++) = *(++ptr);
		strlcpy(p2, p1, STRINGSIZE);
		strlcpy(area, area2, sizeof(area));
	    }
	    break;
	    /* THE FOLLOWING RULES REQUIRE CLASS MATCHING */

	case RULE_PURGE:	/* @x or @?c */
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: delete missing arguments in '%s'\n", control);
		return ((char *) 0);
	    } else if (ptr[1] != RULE_CLASS)
	    {
		strlcpy(area, Purge(area, *(++ptr)), sizeof(area));
	    } else
	    {
		strlcpy(area, PolyPurge(area, ptr[2]), sizeof(area));
		ptr += 2;
	    }
	    break;
	case RULE_SUBSTITUTE:	/* sxy || s?cy */
	    if (!ptr[1] || !ptr[2] || (ptr[1] == RULE_CLASS && !ptr[3]))
	    {
		Debug(1, "Mangle: subst missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else if (ptr[1] != RULE_CLASS)
	    {
		strlcpy(area, Substitute(area, ptr[1], ptr[2]), sizeof(area));
		ptr += 2;
	    } else
	    {
		strlcpy(area, PolySubst(area, ptr[2], ptr[3]), sizeof(area));
		ptr += 3;
	    }
	    break;
	case RULE_MATCH:	/* /x || /?c */
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: '/' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else if (ptr[1] != RULE_CLASS)
	    {
		if (!strchr(area, *(++ptr)))
		{
		    return ((char *) 0);
		}
	    } else
	    {
		if (!PolyStrchr(area, ptr[2]))
		{
		    return ((char *) 0);
		}
		ptr += 2;
	    }
	    break;
	case RULE_NOT:		/* !x || !?c */
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: '!' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else if (ptr[1] != RULE_CLASS)
	    {
		if (strchr(area, *(++ptr)))
		{
		    return ((char *) 0);
		}
	    } else
	    {
		if (PolyStrchr(area, ptr[2]))
		{
		    return ((char *) 0);
		}
		ptr += 2;
	    }
	    break;
	    /*
	     * alternative use for a boomerang, number 1: a standard throwing
	     * boomerang is an ideal thing to use to tuck the sheets under
	     * the mattress when making your bed.  The streamlined shape of
	     * the boomerang allows it to slip easily 'twixt mattress and
	     * bedframe, and it's curve makes it very easy to hook sheets
	     * into the gap.
	     */

	case RULE_EQUALS:	/* =nx || =n?c */
	    if (!ptr[1] || !ptr[2] || (ptr[2] == RULE_CLASS && !ptr[3]))
	    {
		Debug(1, "Mangle: '=' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		register int i;
		if ((i = Char2Int(ptr[1])) < 0)
		{
		    Debug(1, "Mangle: '=' weird argument in '%s'\n", control);
		    return ((char *) 0);
		}
		if (ptr[2] != RULE_CLASS)
		{
		    ptr += 2;
		    if (area[i] != *ptr)
		    {
			return ((char *) 0);
		    }
		} else
		{
		    ptr += 3;
		    if (!MatchClass(*ptr, area[i]))
		    {
			return ((char *) 0);
		    }
		}
	    }
	    break;

	case RULE_DFIRST:
	    if (area[0])
	    {
		register int i;
		for (i = 1; area[i]; i++)
		{
		    area[i - 1] = area[i];
		}
		area[i - 1] = '\0';
	    }
	    break;

	case RULE_DLAST:
	    if (area[0])
	    {
		register int i;
		for (i = 1; area[i]; i++);
		area[i - 1] = '\0';
	    }
	    break;

	case RULE_MFIRST:
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: '(' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		if (ptr[1] != RULE_CLASS)
		{
		    ptr++;
		    if (area[0] != *ptr)
		    {
			return ((char *) 0);
		    }
		} else
		{
		    ptr += 2;
		    if (!MatchClass(*ptr, area[0]))
		    {
			return ((char *) 0);
		    }
		}
	    }
	case RULE_MLAST:
	    if (!ptr[1] || (ptr[1] == RULE_CLASS && !ptr[2]))
	    {
		Debug(1, "Mangle: ')' missing argument in '%s'\n", control);
		return ((char *) 0);
	    } else
	    {
		register int i;

		for (i = 0; area[i]; i++);

		if (i > 0)
		{
		    i--;
		} else
		{
		    return ((char *) 0);
		}

		if (ptr[1] != RULE_CLASS)
		{
		    ptr++;
		    if (area[i] != *ptr)
		    {
			return ((char *) 0);
		    }
		} else
		{
		    ptr += 2;
		    if (!MatchClass(*ptr, area[i]))
		    {
			return ((char *) 0);
		    }
		}
	    }

	default:
	    Debug(1, "Mangle: unknown command %c in %s\n", *ptr, control);
	    return ((char *) 0);
	    break;
	}
    }
    if (!area[0])		/* have we deweted de poor widdle fing away? */
    {
	return ((char *) 0);
    }
    return (area);
}

int
PMatch(control, string)
register char *control;
register char *string;
{
    while (*string && *control)
    {
    	if (!MatchClass(*control, *string))
    	{
    	    return(0);
    	}

    	string++;
    	control++;
    }

    if (*string || *control)
    {
    	return(0);
    }

    return(1);
}
