#include <string.h>
#include <sys/types.h>
#include "Strn.h"

/*
 * Concatenate src on the end of dst.  The resulting string will have at most
 * n-1 characters, not counting the NUL terminator which is always appended
 * unlike strncat.  The other big difference is that strncpy uses n as the
 * max number of characters _appended_, while this routine uses n to limit
 * the overall length of dst.
 */
char *Strncat(char *dst, char *src, size_t n)
{
	register size_t i;
	register char *d, *s;

	if (n != 0 && ((i = strlen(dst)) < (n - 1))) {
		d = dst + i;
		s = src;
		/* If they specified a maximum of n characters, use n - 1 chars to
		 * hold the copy, and the last character in the array as a NUL.
		 * This is the difference between the regular strncpy routine.
		 * strncpy doesn't guarantee that your new string will have a
		 * NUL terminator, but this routine does.
		 */
		for (++i; i<n; i++) {
			if ((*d++ = *s++) == 0) {
				/* Pad with zeros. */
				for (; i<n; i++)
					*d++ = 0;
				return dst;
			}
		}
		/* If we get here, then we have a full string, with n - 1 characters,
		 * so now we NUL terminate it and go home.
		 */
		*d = 0;
	}
	return (dst);
}	/* Strncat */


/*
 * Copy src to dst, truncating or null-padding to always copy n-1 bytes.
 * Return dst.
 */
char *Strncpy(char *dst, char *src, size_t n)
{
	register char *d;
	register char *s;
	register size_t i;

	d = dst;
	*d = 0;
	if (n != 0) {
		s = src;
		/* If they specified a maximum of n characters, use n - 1 chars to
		 * hold the copy, and the last character in the array as a NUL.
		 * This is the difference between the regular strncpy routine.
		 * strncpy doesn't guarantee that your new string will have a
		 * NUL terminator, but this routine does.
		 */
		for (i=1; i<n; i++) {
			if ((*d++ = *s++) == 0) {
				/* Pad with zeros. */
				for (; i<n; i++)
					*d++ = 0;
				return dst;
			}
		}
		/* If we get here, then we have a full string, with n - 1 characters,
		 * so now we NUL terminate it and go home.
		 */
		*d = 0;
	}
	return (dst);
}	/* Strncpy */

/* eof Strn.c */
