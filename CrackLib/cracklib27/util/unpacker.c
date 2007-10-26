/*
 * This program is copyright Alec Muffett 1993. The author disclaims all 
 * responsibility or liability with respect to it's usage or its effect 
 * upon hardware or computer systems, and maintains copyright as set out 
 * in the "LICENCE" document which accompanies distributions of Crack v4.0 
 * and upwards.
 */

#include "packer.h"

int
main(argc, argv)
    int argc;
    char *argv[];
{
    int32 i;
    PWDICT *pwp;
    char buffer[STRINGSIZE];

    if (argc <= 1)
    {
	fprintf(stderr, "Usage:\t%s dbname\n", argv[0]);
	return (-1);
    }

    if (!(pwp = PWOpen (argv[1], "r")))
    {
	perror ("PWOpen");
	return (-1);
    }

    for (i=0; i < PW_WORDS(pwp); i++)
    {
    	register char *c;

	if (!(c = (char *) GetPW (pwp, i)))
	{
	    fprintf(stderr, "error: GetPW %d failed\n", i);
	    continue;
	}

	printf ("%s\n", c);
    }

    return (0);
}
