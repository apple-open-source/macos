/* rfc822valid.c -- validators for RFC-822 syntax
 * (C) Copyright 2007 Matthias Andree <matthias.andree@gmx.de>
 * GNU General Public License v2 */

/* This works only on ASCII-based computers. */

#include "fetchmail.h"
#include <string.h>

/* CHAR except specials, SPACE, CTLs */
static const char *atomchar = "!#$%&'*+-/0123456789=?ABCDEFGHIJKLMNOPQRSTUVWXYZ^_`abcdefghijklmnopqrstuvwxyz{|}~";

static int quotedpair(unsigned char const **x) {
    if (**x != '\\') return 0;
    ++ *x;
    if ((int)* *x > 127 || * *x == '\0')
	/* XXX FIXME: 0 is a legal CHAR, so the == '\0' is sort of bogus
	 * above, but fetchmail does not currently deal with NUL inputs
	 * so we don't need to make the distinction between
	 * end-of-string and quoted NUL. */
	return 0;
    ++ *x;
    return 1;
}


static int quotedstring(unsigned char const **x) {
    if (* *x != '"') return 0;
    ++ *x;
    for(;;) {
	switch (* *x) {
	    case '"':
		++ *x;
		return 1;
	    case '\\':
		if (quotedpair(x) == 0) return 0;
		continue;
	    case '\r':
	    case '\0':
		return 0;
	}
	if ((int)* *x >= 128) {
	    return 0;
	}
	++ *x;
    }
}

static int atom(unsigned char const **x) {
    /* atom */
    if (strchr(atomchar, (char)**x)) {
	*x += strspn((const char *)*x, atomchar);
	return 1;
    }
    /* invalid character */
    return 0;
}

static int word(unsigned char const **x) {
    if (**x == '"')
	return quotedstring(x);
    return atom(x);
}

static int domain_literal(unsigned char const **x) {
    if (**x != '[') return 0;
    ++ *x;
    for(;;) {
	switch (* *x) {
	    case '\0':
	    case '\r':
	    case '[':
		return 0;
	    case ']':
		++ *x;
		return 1;
	    case '\\':
		if (quotedpair(x) == 0) return 0;
		continue;
	}
	if ((int)* *x > 127) return 0;
	++ *x;
    }
}

static int subdomain(unsigned char const **x) {
    if (* *x == '[') return domain_literal(x);
    return atom(x);
}

int rfc822_valid_msgid(const unsigned char *x) {
    /* expect "<" */
    if (*x != '<') return 0;
    ++ x;

    /* expect local-part = word *("." word)
     * where
     * word = atom/quoted-string
     * atom = 1*ATOMCHAR
     * quoted-string = <"> *(qtext/quoted-pair) <">
     * qtext = CHAR except ", \, CR
     * quoted-pair = "\" CHAR
     */
    for(;;) {
	if (word(&x) == 0) return 0;
	if (*x == '.') { ++x; continue; }
	if (*x == '@') break;
	return 0;
    }

    /* expect "@" */
    if (*x != '@') return 0;
    ++ x;

    /* expect domain = sub-domain *("." sub-domain)
     * sub-domain = domain-ref/domain-literal
     * domain-ref = atom
     * domain-literal = "[" *(dtext/quoted-pair) "]" */
    for(;;) {
	if (subdomain(&x) == 0) return 0;
	if (*x == '.') { ++x; continue; }
	if (*x == '>') break;
	return 0;
    }

    if (*x != '>') return 0;
    return 1;
}

#ifdef TEST
#include <stdio.h>

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc; i++) {
	printf("%s: %s\n", argv[i], rfc822_valid_msgid((unsigned char *)argv[i]) ? "OK" : "INVALID");
    }
    return 0;
}
#endif
