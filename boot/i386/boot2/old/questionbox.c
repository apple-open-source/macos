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

#import "libsaio.h"
#import "io_inline.h"
#import "kernBootStruct.h"
#import "font.h"
#import "bitmap.h"
#import "console.h"
#import "graphics.h"
#import "ns_box.h"
#import "ns_logo.h"

int
popupQuestionBox(char *string)
{
    int ch, answer=0;
    int popup_x, popup_y, popup_w, popup_top_h, popup_h;
    int popup_t_x, popup_t_y, popup_t_w;
    int xpos;
    int blink = 0;
    int time, endtime;
    char buf[64];   

    clearActivityIndicator();
    if (kernBootStruct->graphicsMode == GRAPHICS_MODE) {
	popup_w = strwidth(string) + ns_logo_bitmap_WIDTH +
	    3 * POPUP_XMARGIN;
	popup_x = (SCREEN_W - popup_w) / 2;
	popup_t_x = (popup_x + (popup_w - POPUP_T_W) / 2);
	popup_top_h = max(strheight(string), ns_logo_bitmap.height)
		+ 2 * POPUP_YMARGIN;
	popup_h = popup_top_h + POPUP_YMARGIN + POPUP_T_H;
	popup_y = (SCREEN_H - popup_h) / 2;
	popup_t_y = popup_y + popup_h - (POPUP_T_H + POPUP_YMARGIN);
	popupBox(popup_x, popup_y, popup_w, popup_h, TEXT_BG, POPUP_OUT);

	copyImage(&ns_logo_bitmap, popup_x + POPUP_XMARGIN,
		popup_y + (popup_top_h - ns_logo_bitmap_HEIGHT) / 2);
	popupBox(popup_t_x, popup_t_y,
		POPUP_T_W, POPUP_T_H, COLOR_WHITE, POPUP_IN);
	blit_string(string, popup_x + ns_logo_bitmap_WIDTH + 2 * POPUP_XMARGIN,
		popup_y + (popup_top_h - strheight(string)) / 2
		+ fontp->bbx.height + fontp->bbx.yoff,
		TEXT_FG, 0);
	xpos = popup_t_x + POPUP_T_XMARGIN - fontp->bbx.xoff;
    
	for (;;) {
	    time = time18();
	    endtime = time + 12;
    
	    blink = ! blink;
	    clearRect(xpos, popup_t_y + POPUP_T_YMARGIN, 1,
		    POPUP_T_H - 2 * POPUP_T_YMARGIN, 
		    blink ? COLOR_BLACK : COLOR_WHITE);
	    while (time18() < endtime)
		    if (ch = readKeyboardStatus())
			break;
	    if (ch) {
		ch = getc();
		switch (ch) {
		case '\n':
		case '\r':
		    /* erase popup box */
		    clearBox(popup_x, popup_y, popup_w, popup_h);
		    return answer;
		case '\b':
		    clearRect(popup_t_x+3, popup_t_y+3,
			    POPUP_T_W-6, POPUP_T_H-6, COLOR_WHITE);
		    blink = answer = 0;
		    xpos = popup_t_x + POPUP_T_XMARGIN - fontp->bbx.xoff;
		    break;
		default:
		    if (answer)
			break;
		    buf[0] = answer = ch;
		    buf[1] = '\0';
		    clearRect(xpos, popup_t_y + POPUP_T_YMARGIN, 1,
				POPUP_T_H - 2 * POPUP_T_YMARGIN, COLOR_WHITE);
		    blink = 0;
		    xpos += blit_string(buf, xpos,
			popup_t_y + POPUP_T_YMARGIN + fontp->bbx.height
				+ fontp->bbx.yoff,
			COLOR_BLACK, 0);
		    break;
		}
	    }
	}
    } else {
	printf(string);
	printf("\n>> ");
	gets(buf,sizeof(buf));
	return buf[0];
    }
}
