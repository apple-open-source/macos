#include "fetchmail.h"

#include <string.h>
#include <strings.h>

/** A picky certificate name check:
 * check if the pattern or string in s1 (from a certificate) matches the
 * hostname (in s2), returns true if matched.
 *
 * The only place where a wildcard is allowed is in the leftmost
 * position of p1. */
int name_match(const char *p1, const char *p2) {
    const char *const dom = "0123456789.";
    int wildcard_ok = 1;

    /* blank patterns never match */
    if (p1[0] == '\0')
	return 0;

    /* disallow wildcards in certificates for domain literals
     * (10.9.8.7-like) */
    if (strspn(p1+(*p1 == '*' ? 1 : 0), dom) == strlen(p1))
	wildcard_ok = 0;

    /* disallow wildcards for domain literals */
    if (strspn(p2, dom) == strlen(p2))
	wildcard_ok = 0;

    /* If we decided above that wildcarding is still OK,
     * try a wildcard match first - providing that
     * the wildcard is for a full component,
     * i. e. starts with "*." */
    if (wildcard_ok && p1[0] == '*' && p1[1] == '.') {
	size_t l1, l2;
	int number_dots = 0;
	const char *tmp;

	/* skip over the asterisk */
	++p1;

	/* make sure CAs don't wildcard top-level domains by requiring there
	 * are at least two dots in wildcarded X.509 CN/SANs */
	for(tmp = p1; *tmp; tmp += strcspn(tmp, ".")) {
	    if (*tmp == '.') {
		++number_dots;
		++tmp;
	    }
	}

	/* If there are at least 2 dots, do the wildcard match.
	 * Match from the end by incrementing the p2 pointer by the
	 * length difference between remainder of pattern and string to
	 * be matched. */
	if (number_dots >= 2) {
	    l1 = strlen(p1);
	    l2 = strlen(p2);
	    if (l2 > l1)
		p2 += l2 - l1;
	    /* FALLTHROUGH */
	}
    }

    /* Now to the match. Either wildcards are forbidden or not found,
     * then it's a case-insensitive full-string match, or wildcards are
     * permitted and found and we've bumped the start-string pointers
     * sufficiently. */
    return (0 == strcasecmp(p1, p2));

    /* XXX open issue: do we need to deal with trailing dots in patterns
     * or domains? A trailing dot is an anchor that prevents resolver
     * "search"es to DNS, so might cause false mismatches. */
}

#ifdef TEST
#include <stdlib.h>
#include <stdio.h>

static int verbose;

/* print test and return true on failure */
static int test(const char *p1, const char *p2, int expect) {
    int match = name_match(p1, p2);
    if (verbose)
	printf("name_match(\"%s\", \"%s\") == %d (%d expected)\n", p1, p2, match, expect);
    return expect != match;
}

int main(int argc, const char **argv) {
    int rc = 0;

    if (argc > 1 && 0 == strcmp(argv[1], "-v"))
	verbose = 1;

    rc |= test("example.org", "example.org", 1);
    rc |= test("*example.org", "foo.example.org", 0);
    rc |= test("*.example.org", "foo.example.org", 1);
    rc |= test("*.168.23.23", "192.168.23.23", 0);
    rc |= test("*.com", "example.com", 0);
    if (verbose) {
	printf("x509_name_match: ");
	puts(rc ? "FAIL" : "PASS");
    }
    return rc;
}
#endif
