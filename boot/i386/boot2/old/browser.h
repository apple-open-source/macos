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
#import "graphics.h"
#import "button.h"
#import "scrollbar.h"

typedef struct browser {
    rect_t loc;
    short x,y,w,h;
    char **items;
    char *selected;
    char *top_message;
    char *bottom_message;
    short nitems;
    short current;
    short top_visible;
    short bot_visible;
    short type;
    short item_box_x, item_box_y; /* item box location */
    short item_box_w, item_box_h;
    short item_h, item_w; /* individual item metrics */
    short top_h, top_w;	/* size of top text area */
    button_t button;
    scroll_t scrollbar;
    short bottom_y;
    short bottom_h, bottom_w; /* size of bottom text area */
} browser_t;

#define BROWSER_XMARGIN 8
#define BROWSER_YMARGIN 8

#define MAX_ITEMS_VISIBLE	8

#define BROWSER_SELECT_MULTIPLE		0
#define BROWSER_CURRENT_IS_SELECTED	1

#define BROWSER_NO_MESSAGE	((char *)0)
#define BROWSER_INSTRUCTIONS	((char *)1)

extern char * popupBrowser(
    char **items,
    int nitems,
    char *top_message,
    char *bottom_message,
    char *button_string,
    int type
);
