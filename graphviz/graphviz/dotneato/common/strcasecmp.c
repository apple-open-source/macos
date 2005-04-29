#include <utils.h>

#ifndef HAVE_STRCASECMP

#include <string.h>
#include <ctype.h>


int
strcasecmp(const char *s1, const char *s2)
{
	while ((*s1 != '\0') 
        && (tolower(*(unsigned char *)s1) == tolower(*(unsigned char *)s2)))
	{
		s1++;
		s2++;
	}

	return tolower(*(unsigned char *) s1) - tolower(*(unsigned char *) s2);
}

#endif /* HAVE_STRCASECMP */
