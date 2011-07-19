/*
 * This program is copyright Alec Muffett 1993. The author disclaims all 
 * responsibility or liability with respect to it's usage or its effect 
 * upon hardware or computer systems, and maintains copyright as set out 
 * in the "LICENCE" document which accompanies distributions of Crack v4.0 
 * and upwards.
 */

#include "packer.h"
#include <string.h>
#include <stdlib.h>

static char __unused vers_id[] = "stringlib.c : v2.3p2 Alec Muffett 18 May 1993";

char
Chop(string)
    register char *string;
{
    register char c;
    register char *ptr;
    c = '\0';

    for (ptr = string; *ptr; ptr++);
    if (ptr != string)
    {
	c = *(--ptr);
	*ptr = '\0';
    }
    return (c);
}

char *
Trim(string)
    register char *string;
{
    register char *ptr;
    for (ptr = string; *ptr; ptr++);

    while ((--ptr >= string) && isspace(*ptr));

    *(++ptr) = '\0';

    return (ptr);
}

char *
Clone(string)
    char *string;
{
    register char *retval;
    int len = strlen(string)+1;
    retval = (char *) malloc(len);
    if (retval)
    {
	strlcpy(retval, string, len);
    }
    return (retval);
}
