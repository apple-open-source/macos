/* $Xorg: imLcPrs.c,v 1.3 2000/08/17 19:45:14 cpqbld Exp $ */
/******************************************************************

              Copyright 1992 by Oki Technosystems Laboratory, Inc.
              Copyright 1992 by Fuji Xerox Co., Ltd.

Permission to use, copy, modify, distribute, and sell this software
and its documentation for any purpose is hereby granted without fee,
provided that the above copyright notice appear in all copies and
that both that copyright notice and this permission notice appear
in supporting documentation, and that the name of Oki Technosystems
Laboratory and Fuji Xerox not be used in advertising or publicity
pertaining to distribution of the software without specific, written
prior permission.
Oki Technosystems Laboratory and Fuji Xerox make no representations
about the suitability of this software for any purpose.  It is provided
"as is" without express or implied warranty.

OKI TECHNOSYSTEMS LABORATORY AND FUJI XEROX DISCLAIM ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL OKI TECHNOSYSTEMS
LABORATORY AND FUJI XEROX BE LIABLE FOR ANY SPECIAL, INDIRECT OR
CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE
OR PERFORMANCE OF THIS SOFTWARE.

  Author: Yasuhiro Kawai	Oki Technosystems Laboratory
  Author: Kazunori Nishihara	Fuji Xerox

******************************************************************/

/* $XFree86: xc/lib/X11/imLcPrs.c,v 1.8 2003/01/15 02:59:33 dawes Exp $ */

#include <X11/Xlib.h>
#include <X11/Xmd.h>
#include <X11/Xos.h>
#include "Xlibint.h"
#include "Xlcint.h"
#include "Ximint.h"
#include <sys/stat.h>
#include <stdio.h>

extern int _Xmbstowcs(
#if NeedFunctionPrototypes
    wchar_t	*wstr,
    char	*str,
    int		len
#endif
);

extern int _Xmbstoutf8(
#if NeedFunctionPrototypes
    char	*ustr,
    const char	*str,
    int		len
#endif
);

/*
 *	Parsing File Format:
 *
 *	FILE          ::= { [PRODUCTION] [COMMENT] "\n"}
 *	PRODUCTION    ::= LHS ":" RHS [ COMMENT ]
 *	COMMENT       ::= "#" {<any character except null or newline>}
 *	LHS           ::= EVENT { EVENT }
 *	EVENT         ::= [MODIFIER_LIST] "<" keysym ">"
 *	MODIFIER_LIST ::= ("!" {MODIFIER} ) | "None"
 *	MODIFIER      ::= ["~"] modifier_name
 *	RHS           ::= ( STRING | keysym | STRING keysym )
 *	STRING        ::= '"' { CHAR } '"'
 *	CHAR          ::= GRAPHIC_CHAR | ESCAPED_CHAR
 *	GRAPHIC_CHAR  ::= locale (codeset) dependent code
 *	ESCAPED_CHAR  ::= ('\\' | '\"' | OCTAL | HEX )
 *	OCTAL         ::= '\' OCTAL_CHAR [OCTAL_CHAR [OCTAL_CHAR]]
 *	OCTAL_CHAR    ::= (0|1|2|3|4|5|6|7)
 *	HEX           ::= '\' (x|X) HEX_CHAR [HEX_CHAR]]
 *	HEX_CHAR      ::= (0|1|2|3|4|5|6|7|8|9|A|B|C|D|E|F|a|b|c|d|e|f)
 *
 */

static int
nextch(fp, lastch)
    FILE *fp;
    int *lastch;
{
    int c;

    if (*lastch != 0) {
	c = *lastch;
	*lastch = 0;
    } else {
	c = getc(fp);
	if (c == '\\') {
	    c = getc(fp);
	    if (c == '\n') {
		c = getc(fp);
	    } else {
		ungetc(c, fp);
		c = '\\';
	    }
	}
    }
    return(c);
}

static void
putbackch(c, lastch)
    int c;
    int *lastch;
{
    *lastch = c;
}

#define ENDOFFILE 0
#define ENDOFLINE 1
#define COLON 2
#define LESS 3
#define GREATER 4
#define EXCLAM 5
#define TILDE 6
#define STRING 7
#define KEY 8
#define ERROR 9

#ifndef isalnum
#define isalnum(c)      \
    (('0' <= (c) && (c) <= '9')  || \
     ('A' <= (c) && (c) <= 'Z')  || \
     ('a' <= (c) && (c) <= 'z'))
#endif

static int
nexttoken(fp, tokenbuf, lastch)
    FILE *fp;
    char *tokenbuf;
    int *lastch;
{
    int c;
    int token;
    char *p;
    int i, j;

    while ((c = nextch(fp, lastch)) == ' ' || c == '\t') {
    }
    switch (c) {
      case EOF:
	token = ENDOFFILE;
	break;
      case '\n':
	token = ENDOFLINE;
	break;
      case '<':
	token = LESS;
	break;
      case '>':
	token = GREATER;
	break;
      case ':':
	token = COLON;
	break;
      case '!':
	token = EXCLAM;
	break;
      case '~':
	token = TILDE;
	break;
      case '"':
	p = tokenbuf;
	while ((c = nextch(fp, lastch)) != '"') {
	    if (c == '\n' || c == EOF) {
		putbackch(c, lastch);
		token = ERROR;
		goto string_error;
	    } else if (c == '\\') {
		c = nextch(fp, lastch);
		switch (c) {
		  case '\\':
		  case '"':
		    *p++ = c;
		    break;
		  case '0':
		  case '1':
		  case '2':
		  case '3':
		  case '4':
		  case '5':
		  case '6':
		  case '7':
		    i = c - '0';
		    c = nextch(fp, lastch);
		    for (j = 0; j < 2 && c >= '0' && c <= '7'; j++) {
			i <<= 3;
			i += c - '0';
			c = nextch(fp, lastch);
		    }
		    putbackch(c, lastch);
		    *p++ = (char)i;
		    break;
		  case 'X':
		  case 'x':
		    i = 0;
		    c = nextch(fp, lastch);
#define ishexch(c) (((c) >= '0' && (c) <= '9') || \
		    ((c) >= 'A' && (c) <= 'F') || \
		    ((c) >= 'a' && (c) <= 'f'))
		    for (j = 0; j < 2 && ishexch(c); j++) {
			i <<= 4;
			if (c >= '0' && c <= '9') {
			    i += c - '0';
			} else if (c >= 'A' && c <= 'F') {
			    i += c - 'A' + 10;
			} else {
			    i += c - 'a' + 10;
			}
			c = nextch(fp, lastch);
		    }
		    if (j == 0) {
		        token = ERROR;
		        goto string_error;
		    }
		    putbackch(c, lastch);
		    *p++ = (char)i;
#undef ishexch
		    break;
		  case '\n':
		  case EOF:
		    putbackch(c, lastch);
		    token = ERROR;
		    goto string_error;
		  default:
		    *p++ = c;
		    break;
		}
	    } else {
		*p++ = c;
	    }
	}
	*p = '\0';
	token = STRING;
	break;
      case '#':
	while ((c = nextch(fp, lastch)) != '\n' && c != EOF) {
	}
	if (c == '\n') {
	    token = ENDOFLINE;
	} else {
	    token = ENDOFFILE;
	}
	break;
      default:
	if (isalnum(c) || c == '_' || c == '-') {
	    p = tokenbuf;
	    *p++ = c;
	    c = nextch(fp, lastch);
	    while (isalnum(c) || c == '_' || c == '-') {
		*p++ = c;
		c = nextch(fp, lastch);
	    }
	    *p = '\0';
	    putbackch(c, lastch);
	    token = KEY;
	} else {
	    token = ERROR;
	}
	break;
    }
string_error:
    return(token);
}

static long
modmask(name)
    char *name;
{
    long mask;

    struct _modtbl {
	char *name;
	long mask;
    };
    struct _modtbl *p;

    static struct _modtbl tbl[] = {
	{ "Ctrl",	ControlMask	},
        { "Lock",	LockMask	},
        { "Caps",	LockMask	},
        { "Shift",	ShiftMask	},
        { "Alt",	Mod1Mask	},
        { "Meta",	Mod1Mask	},
        { NULL,		0		}};

    p = tbl;
    mask = 0;
    for (p = tbl; p->name != NULL; p++) {
	if (strcmp(name, p->name) == 0) {
	    mask = p->mask;
	    break;
	}
    }
    return(mask);
}

#define AllMask (ShiftMask | LockMask | ControlMask | Mod1Mask) 
#define LOCAL_WC_BUFSIZE 128
#define LOCAL_UTF8_BUFSIZE 256
#define SEQUENCE_MAX	10

static int
parseline(fp, top, tokenbuf)
    FILE *fp;
    DefTree **top;
    char* tokenbuf;
{
    int token;
    unsigned modifier_mask;
    unsigned modifier;
    unsigned tmp;
    KeySym keysym = NoSymbol;
    DefTree *p = NULL;
    Bool exclam, tilde;
    KeySym rhs_keysym = 0;
    char *rhs_string_mb;
    int l;
    int lastch = 0;
    wchar_t local_wc_buf[LOCAL_WC_BUFSIZE], *rhs_string_wc;
    char local_utf8_buf[LOCAL_UTF8_BUFSIZE], *rhs_string_utf8;

    struct DefBuffer {
	unsigned modifier_mask;
	unsigned modifier;
	KeySym keysym;
    };

    struct DefBuffer buf[SEQUENCE_MAX];
    int i, n;

    do {
	token = nexttoken(fp, tokenbuf, &lastch);
    } while (token == ENDOFLINE);
    
    if (token == ENDOFFILE) {
	return(-1);
    }

    n = 0;
    do {
	if ((token == KEY) && (strcmp("None", tokenbuf) == 0)) {
	    modifier = 0;
	    modifier_mask = AllMask;
	    token = nexttoken(fp, tokenbuf, &lastch);
	} else {
	    modifier_mask = modifier = 0;
	    exclam = False;
	    if (token == EXCLAM) {
		exclam = True;
		token = nexttoken(fp, tokenbuf, &lastch);
	    }
	    while (token == TILDE || token == KEY) {
		tilde = False;
		if (token == TILDE) {
		    token = nexttoken(fp, tokenbuf, &lastch);
		    tilde = True;
		    if (token != KEY)
			goto error;
		}
		token = nexttoken(fp, tokenbuf, &lastch);
		tmp = modmask(tokenbuf);
		if (!tmp) {
		    goto error;
		}
		modifier_mask |= tmp;
		if (tilde) {
		    modifier &= ~tmp;
		} else {
		    modifier |= tmp;
		}
	    }
	    if (exclam) {
		modifier_mask = AllMask;
	    }
	}

	if (token != LESS) {
	    goto error;
	}

	token = nexttoken(fp, tokenbuf, &lastch);
	if (token != KEY) {
	    goto error;
	}

	token = nexttoken(fp, tokenbuf, &lastch);
	if (token != GREATER) {
	    goto error;
	}

	keysym = XStringToKeysym(tokenbuf);
	if (keysym == NoSymbol) {
	    goto error;
	}

	buf[n].keysym = keysym;
	buf[n].modifier = modifier;
	buf[n].modifier_mask = modifier_mask;
	n++;
	if( n >= SEQUENCE_MAX )
	    goto error;
	token = nexttoken(fp, tokenbuf, &lastch);
    } while (token != COLON);

    token = nexttoken(fp, tokenbuf, &lastch);
    if (token == STRING) {
	if( (rhs_string_mb = Xmalloc(strlen(tokenbuf) + 1)) == NULL )
	    goto error;
	strcpy(rhs_string_mb, tokenbuf);
	token = nexttoken(fp, tokenbuf, &lastch);
	if (token == KEY) {
	    rhs_keysym = XStringToKeysym(tokenbuf);
	    if (rhs_keysym == NoSymbol) {
		Xfree(rhs_string_mb);
		goto error;
	    }
	    token = nexttoken(fp, tokenbuf, &lastch);
	}
	if (token != ENDOFLINE && token != ENDOFFILE) {
	    Xfree(rhs_string_mb);
	    goto error;
	}
    } else if (token == KEY) {
	rhs_keysym = XStringToKeysym(tokenbuf);
	if (rhs_keysym == NoSymbol) {
	    goto error;
	}
	token = nexttoken(fp, tokenbuf, &lastch);
	if (token != ENDOFLINE && token != ENDOFFILE) {
	    goto error;
	}
	if( (rhs_string_mb = Xmalloc(1)) == NULL ) {
	    Xfree( rhs_string_mb );
	    goto error;
	}
	rhs_string_mb[0] = '\0';
    } else {
	goto error;
    }

    l = _Xmbstowcs(local_wc_buf, rhs_string_mb, LOCAL_WC_BUFSIZE - 1);
    if (l == LOCAL_WC_BUFSIZE - 1) {
	local_wc_buf[l] = (wchar_t)'\0';
    }
    if( (rhs_string_wc = (wchar_t *)Xmalloc((l + 1) * sizeof(wchar_t))) == NULL ) {
	Xfree( rhs_string_mb );
	return( 0 );
    }
    memcpy((char *)rhs_string_wc, (char *)local_wc_buf, (l + 1) * sizeof(wchar_t) );

    l = _Xmbstoutf8(local_utf8_buf, rhs_string_mb, LOCAL_UTF8_BUFSIZE - 1);
    if (l == LOCAL_UTF8_BUFSIZE - 1) {
	local_wc_buf[l] = '\0';
    }
    if( (rhs_string_utf8 = (char *)Xmalloc(l + 1)) == NULL ) {
	Xfree( rhs_string_wc );
	Xfree( rhs_string_mb );
	return( 0 );
    }
    memcpy(rhs_string_utf8, local_utf8_buf, l + 1);

    for (i = 0; i < n; i++) {
	for (p = *top; p; p = p->next) {
	    if (buf[i].keysym        == p->keysym &&
		buf[i].modifier      == p->modifier &&
		buf[i].modifier_mask == p->modifier_mask) {
		break;
	    }
	}
	if (p) {
	    top = &p->succession;
	} else {
	    if( (p = (DefTree*)Xmalloc(sizeof(DefTree))) == NULL ) {
		Xfree( rhs_string_mb );
		goto error;
	    }
	    p->keysym        = buf[i].keysym;
	    p->modifier      = buf[i].modifier;
	    p->modifier_mask = buf[i].modifier_mask;
	    p->succession    = NULL;
	    p->next          = *top;
	    p->mb            = NULL;
	    p->wc            = NULL;
	    p->utf8          = NULL;
	    p->ks            = NoSymbol;
	    *top = p;
	    top = &p->succession;
	}
    }

    if( p->mb != NULL )
	Xfree( p->mb );
    p->mb = rhs_string_mb;
    if( p->wc != NULL )
	Xfree( p->wc );
    p->wc = rhs_string_wc;
    if( p->utf8 != NULL )
	Xfree( p->utf8 );
    p->utf8 = rhs_string_utf8;
    p->ks = rhs_keysym;
    return(n);
error:
    while (token != ENDOFLINE && token != ENDOFFILE) {
	token = nexttoken(fp, tokenbuf, &lastch);
    }
    return(0);
}

void
_XimParseStringFile(fp, ptop)
    FILE *fp;
    DefTree **ptop;
{
    char tb[8192];
    char* tbp;
    struct stat st;

    if (fstat (fileno (fp), &st) != -1) {
	unsigned long size = (unsigned long) st.st_size;
	if (size <= sizeof tb) tbp = tb;
	else tbp = malloc (size);

	if (tbp != NULL) {
	    while (parseline(fp, ptop, tbp) >= 0) {}
	    if (tbp != tb) free (tbp);
	}
    }
}
