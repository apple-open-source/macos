#include <utils.h>

#ifndef HAVE_STRNCASECMP

#include <string.h>
#include <ctype.h>


int 
strncasecmp(const char *s1, const char *s2, unsigned int n)
{
	if (n == 0)
		return 0;

	while ((n-- != 0)
            && (tolower(*(unsigned char *)s1) == tolower(*(unsigned char *)s2)))
	{
		if (n == 0 || *s1 == '\0' || *s2 == '\0')
			return 0;
		s1++;
		s2++;
	}

	return tolower(*(unsigned char *) s1) - tolower(*(unsigned char *) s2);
}

#endif /* HAVE_STRNCASECMP */
