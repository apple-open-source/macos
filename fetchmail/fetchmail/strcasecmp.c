/* 
 * scratch implementation of strcasecmp(), 
 * in case your C library doesn't have it 
 *
 * For license terms, see the file COPYING in this directory.
 */
#include <ctype.h>

strcasecmp(char *s1, char *s2)
{
    while (toupper(*s1) == toupper(*s2++))
	if (*s1++ == '\0')
	    return(0);
    return(toupper(*s1) - toupper(*--s2));
}

strncasecmp(char *s1, char *s2, register int n)
{
    while (--n >= 0 && toupper(*s1) == toupper(*s2++))
	if (toupper(*s1++) == '\0')
	    return(0);
    return(n < 0 ? 0 : toupper(*s1) - toupper(*--s2));
}
