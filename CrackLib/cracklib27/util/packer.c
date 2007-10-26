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
    int32 readed;
    int32 wrote;
    PWDICT *pwp;
    char buffer[STRINGSIZE];

    if (argc <= 1)
    {
	fprintf(stderr, "Usage:\t%s dbname\n", argv[0]);
	return (-1);
    }

    if (!(pwp = PWOpen(argv[1], "w")))
    {
	perror(argv[1]);
	return (-1);
    }

    wrote = 0;

    for (readed = 0; fgets(buffer, STRINGSIZE, stdin); /* nothing */)
    {
    	readed++;

	buffer[MAXWORDLEN - 1] = '\0';

	Chop(buffer);

	if (!buffer[0])
	{
	    fprintf(stderr, "skipping line: %lu\n", readed);
	    continue;
	}

	if (PutPW(pwp, buffer))
	{
	    fprintf(stderr, "error: PutPW '%s' line %luy\n", buffer, readed);
	}

	wrote++;
    }

    PWClose(pwp);

    printf("%lu %lu\n", readed, wrote);

    return (0);
}
