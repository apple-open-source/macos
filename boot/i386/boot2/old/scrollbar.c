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
#import "scrollbar.h"


scroll_t *initScrollbar(
    scroll_t *sp,
    int x,
    int y,
    int w,
    int h,
    int position,		/* 0 - 100 */
    int percent			/* 0 - 100 */
)
{
    sp->loc.x = x;
    sp->loc.y = y;
    sp->loc.w = w;
    sp->loc.h = h;
    sp->position = position;
    sp->percent = percent;
    return sp;
}

#if 0
void destroyScrollbar(
    scroll_t *sp
)
{
    (void)zfree((char *)sp);
}
#endif

void drawScrollbar(
    scroll_t *sp
)
{
    clearRect(sp->loc.x, sp->loc.y, sp->loc.w, sp->loc.h, SB_BG);
//    clearRect(sp->loc.x, sp->loc.y, sp->loc.w, 1, COLOR_BLACK);
//    clearRect(sp->loc.x, sp->loc.y + sp->loc.h - 1, sp->loc.w, 1, COLOR_BLACK);
//    clearRect(sp->loc.x, sp->loc.y, 1, sp->loc.h, COLOR_BLACK);
    clearRect(sp->loc.x + sp->loc.w - 1, sp->loc.y, 1, sp->loc.h, COLOR_BLACK);
    if (sp->percent < 100)
	popupBox(sp->loc.x + SB_MARGIN,
	    sp->loc.y + (sp->position * (sp->loc.h - 2 * SB_MARGIN)) / 100,
	    sp->loc.w - 3 * SB_MARGIN,
	    (sp->loc.h - 2 * SB_MARGIN) * sp->percent / 100,
	    TEXT_BG, POPUP_OUT);
}

#if 0
void clearScrollbar(
    scroll_t *sp,
    int color
)
{
    clearRect(sp->loc.x, sp->loc.y, sp->loc.w, sp->loc.h, color);
}
#endif
