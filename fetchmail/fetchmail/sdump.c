/* sdump.c -- library to allocate and format a printable version of a
 * string with embedded NUL */

/** \file sdump.c
 * \author Matthias Andree
 * \date 2009
 *
 * This file is available under the GNU Lesser General Public License
 * v2.1 or any later version of the GNU LGPL.
 */

#include <ctype.h>  /* for isprint() */
#include <stdio.h>  /* for sprintf() */
#include <stdlib.h> /* for size_t */
#include "xmalloc.h" /* for xmalloc() */

#include "sdump.h"   /* for prototype */

/** sdump converts a byte string \a in of size \a len into a printable
 * string and returns a pointer to the memory region.
 * \returns a pointer to a xmalloc()ed string that the caller must
 * free() after use. This function causes program abort on failure
 * through xmalloc. xmalloc is a function that calls malloc() and aborts
 * the program if malloc() returned NULL i. e. failure. */
char *sdump(const char *in, size_t len)
{
    size_t outlen = 0, i;
    char *out, *oi;

    for (i = 0; i < len; i++) {
	outlen += isprint((unsigned char)in[i]) ? 1 : 4;
    }

    oi = out = (char *)xmalloc(outlen + 1);
    for (i = 0; i < len; i++) {
	if (isprint((unsigned char)in[i])) {
	    *(oi++) = in[i];
	} else {
	    oi += sprintf(oi, "\\x%02X", (unsigned char)in[i]);
	}
    }
    *oi = '\0';
    return out;
}
