#if	!defined(lint) && !defined(DOS)
static char rcsid[] = "$Id: window.c,v 1.2 2002/01/03 22:16:43 jevans Exp $";
#endif
/*
 * Program:	Window management routines
 *
 *
 * Michael Seibel
 * Networks and Distributed Computing
 * Computing and Communications
 * University of Washington
 * Administration Builiding, AG-44
 * Seattle, Washington, 98195, USA
 * Internet: mikes@cac.washington.edu
 *
 * Please address all bugs and comments to "pine-bugs@cac.washington.edu"
 *
 * Copyright 1991-1994  University of Washington
 *
 *  Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee to the University of
 * Washington is hereby granted, provided that the above copyright notice
 * appears in all copies and that both the above copyright notice and this
 * permission notice appear in supporting documentation, and that the name
 * of the University of Washington not be used in advertising or publicity
 * pertaining to distribution of the software without specific, written
 * prior permission.  This software is made available "as is", and
 * THE UNIVERSITY OF WASHINGTON DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED,
 * WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT LIMITATION ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, AND IN
 * NO EVENT SHALL THE UNIVERSITY OF WASHINGTON BE LIABLE FOR ANY SPECIAL,
 * INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, TORT
 * (INCLUDING NEGLIGENCE) OR STRICT LIABILITY, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Pine and Pico are trademarks of the University of Washington.
 * No commercial use of these trademarks may be made without prior
 * written permission of the University of Washington.
 *
 */
/*
 * Window management. Some of the functions are internal, and some are
 * attached to keys that the user actually types.
 */

#include        <stdio.h>
#include        "estruct.h"
#include        "pico.h"
#include	"edef.h"
#include	"osdep.h"


/*
 * Refresh the screen. With no argument, it just does the refresh. With an
 * argument it recenters "." in the current window. Bound to "C-L".
 */
refresh(f, n)
  int f, n;
{
    /*
     * since pine mode isn't using the traditional mode line, sgarbf isn't
     * enough.
     */
    if(Pmaster)
        curwp->w_flag |= WFMODE;

    if (f == FALSE)
        sgarbf = TRUE;
    else{
        curwp->w_force = 0;             /* Center dot. */
        curwp->w_flag |= WFFORCE;
    }

    return (TRUE);
}
