/* $XFree86: xc/lib/font/stubs/xpstubs.c,v 1.1 1999/01/11 05:13:22 dawes Exp $ */

/*
  stub for XpClient* functions.
*/

#include "stubs.h"

Bool
XpClientIsBitmapClient(ClientPtr client)
{
    return True;
}

Bool
XpClientIsPrintClient(ClientPtr client, FontPathElementPtr fpe)
{
    return False;
}

/* end of file */
