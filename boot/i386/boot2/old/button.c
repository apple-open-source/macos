/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright 1993 NeXT, Inc.
 * All rights reserved.
 */
 
#import "libsa.h"
#import "console.h"
#import "fontio.h"
#import "graphics.h"
#import "button.h"
#import "return.h"

button_t *initButton(
    button_t *bp,
    int x,
    int y,
    char *message,
    int drawReturnSign
)
{
    bp->loc.x = x;
    bp->loc.y = y;
    bp->loc.h = strheight(message) + 2 * BUTTON_YMARGIN;
    bp->loc.w = strwidth(message) + 2 * BUTTON_XMARGIN;
    if (drawReturnSign)
	bp->loc.w += return_bitmap_WIDTH + BUTTON_XMARGIN;
    bp->message = message;
    bp->flags = drawReturnSign ? BUTTON_DRAW_RETURN : 0;
    return bp;
}

#if 0
void destroyButton(
    button_t *bp
)
{
    zfree((char *)bp);
}
#endif

void drawButton(
    button_t *bp
)
{
    popupBox(bp->loc.x, bp->loc.y, bp->loc.w, bp->loc.h, TEXT_BG, POPUP_OUT);
    blit_string(bp->message,
	    bp->loc.x + BUTTON_XMARGIN, bp->loc.y + bp->loc.h / 2,
	    TEXT_FG, CENTER_V);
    if (bp->flags & BUTTON_DRAW_RETURN) {
	copyImage(&return_bitmap,
	    bp->loc.x + bp->loc.w - BUTTON_XMARGIN - return_bitmap_WIDTH,
	    bp->loc.y + (bp->loc.h - return_bitmap_HEIGHT) / 2);
    }
}

#if 0
void
clearButton(
    button_t *bp,
    int color
)
{
    clearRect(bp->loc.x,bp->loc.y, bp->loc.w, bp->loc.h, color);
}
#endif
