/* 
 * scratch implementation of strcasecmp(), 
 * in case your C library doesn't have it 
 *
 * For license terms, see the file COPYING in this directory.
 */
#include <ctype.h>

int strcasecmp(char *s1, char *s2)
{
    while (toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
	if (*s1++ == '\0')
	    return 0;
    return(toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
}

int strncasecmp(char *s1, char *s2, register int n)
{
    while (--n >= 0 && toupper((unsigned char)*s1) == toupper((unsigned char)*s2++))
	if (*s1++ == '\0')
	    return 0;
    return(n < 0 ? 0 : toupper((unsigned char)*s1) - toupper((unsigned char)*--s2));
}
