/* $OpenLDAP: pkg/ldap/libraries/liblutil/memcmp.c,v 1.3 1999/09/08 17:06:31 kdz Exp $ */
#include "portable.h"

#include <ac/string.h>

/* 
 * Memory Compare
 */
int
(memcmp)(const void *v1, const void *v2, int n) 
{
    if (n != 0) {
		const unsigned char *s1=v1, *s2=v2;
        do {
            if (*s1++ != *s2++)
                return (*--s1 - *--s2);
        } while (--n != 0);
    }
    return (0);
} 
