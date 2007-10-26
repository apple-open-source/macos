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
    char buffer[STRINGSIZE];

    printf("enter potential passwords, one per line...\n");

    while (fgets (buffer, STRINGSIZE, stdin))
    {
    	char *val;
 
	Chop(buffer);

	val = FascistCheck(buffer, CRACKLIB_DICTPATH);

	if (!val)
	{
		val = "ok";
	}	
	printf ("%s: %s\n", buffer, val);
    }

    return (0);
}
