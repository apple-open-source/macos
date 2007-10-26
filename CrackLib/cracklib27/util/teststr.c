/*
 * This program is copyright Alec Muffett 1993. The author disclaims all 
 * responsibility or liability with respect to it's usage or its effect 
 * upon hardware or computer systems, and maintains copyright as set out 
 * in the "LICENCE" document which accompanies distributions of Crack v4.0 
 * and upwards.
 */

#include "packer.h"

int
main ()
{
    int32 i;
    PWDICT *pwp;
    char buffer[STRINGSIZE];

    if (!(pwp = PWOpen (CRACKLIB_DICTPATH, "r")))
    {
	perror ("PWOpen");
	return (-1);
    }

    printf("enter dictionary words, one per line...\n");

    while (fgets (buffer, STRINGSIZE, stdin))
    {
    	int32 i;

	Chop(buffer);
	i = FindPW(pwp, buffer);
	printf ("%s: %lu%s\n", buffer, i, (i==PW_WORDS(pwp) ? " **** NOT FOUND ****" : ""));
    }

    return (0);
}
