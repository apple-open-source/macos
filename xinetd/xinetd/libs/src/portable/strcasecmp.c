typedef unsigned char u_char;

int
strcasecmp(s1, s2)
	        const char *s1, *s2;
{
	register const u_char
	*us1 = (const u_char *)s1,
	*us2 = (const u_char *)s2;

	while (tolower(*us1) == tolower(*us2++))
		if (*us1++ == '\0')
			return (0);
	return (tolower(*us1) - tolower(*--us2));
}
