/* $XFree86: xc/lib/font/stubs/stubs.h,v 1.3 1999/12/15 01:14:36 robin Exp $ */

/* This directory includes dummy entry for bdftopcf and mkfontdir */

#include <stdio.h>
#include "fntfilst.h"
#include "font.h"


#ifndef True
#define True (-1)
#endif
#ifndef False
#define False (0)
#endif

extern FontPtr find_old_font ( FSID id );
extern FontResolutionPtr GetClientResolutions ( int *num );
extern int GetDefaultPointSize ( void );
extern int set_font_authorizations ( char **authorizations, 
				     int *authlen, 
				     ClientPtr client );
extern Bool XpClientIsBitmapClient ( ClientPtr client );
extern Bool XpClientIsPrintClient ( ClientPtr client, 
				    FontPathElementPtr fpe );

extern unsigned long GetTimeInMillis (void);

/* end of file */
