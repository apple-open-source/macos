/*
 * strstr -- locate first occurence of a substring 
 *
 * Locates the first occurrence in the string pointed to by S1 of the string
 * pointed to by S2.  Returns a pointer to the substring found, or a NULL
 * pointer if not found.  If S2 points to a string with zero length, the
 * function returns S1. 
 *
 * For license terms, see the file COPYING in this directory.
 */

char *strstr(register char *buf, register char *sub)
{
    register char *bp;

    if (!*sub)
	return buf;
    for (;;)
    {
	if (!*buf)
	    break;
	bp = buf;
	for (;;)
	{
	    if (!*sub)
		return buf;
	    if (*bp++ != *sub++)
		break;
	}
	sub -= (unsigned long) bp;
	sub += (unsigned long) buf;
	buf += 1;
    }
    return 0;
}

