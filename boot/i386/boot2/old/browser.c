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
#import "browser.h"
#import "scrollbar.h"
#import "ns_logo.h"
#import "dot.h"
#import "hdot.h"
#import "keys.h"

#define max(a,b) ((a) > (b) ? (a) : (b))
#define min(a,b) ((a) < (b) ? (a) : (b))

browser_t *
createBrowser(
    char **items,
    int nitems,
    char *top_message,
    char *bottom_message,
    char *button_string,
    int type
)
{
    browser_t *bp;
    register int i;
    
    bp = (browser_t *)zalloc(sizeof(browser_t));
    bp->items = items;
    bp->nitems = nitems;
    bp->bot_visible = min( MAX_ITEMS_VISIBLE - 1, nitems - 1 );
    /* bp->top_visible = 0; */
    
    bp->top_message = top_message;
    bp->top_h = max (strheight(bp->top_message), ns_logo_bitmap_HEIGHT);
    bp->top_w = strwidth(bp->top_message) + ns_logo_bitmap_WIDTH +
	    BROWSER_XMARGIN;
    bp->bottom_message = bottom_message;
    bp->bottom_h = bottom_message ?
	    strheight(bottom_message) :
	    -BROWSER_YMARGIN;
    bp->bottom_w = strwidth(bottom_message);
    if (nitems) {
	bp->selected = (char *)zalloc(nitems);
	if (type == BROWSER_CURRENT_IS_SELECTED)
	    bp->selected[0] = 1;
	bp->item_h = strheight(items[0]);
	for(i=0; i < nitems; i++)
	    bp->item_w = max(bp->item_w, strwidth(items[i]));
	bp->item_w += 2 * BROWSER_XMARGIN + dot_bitmap_WIDTH;
	bp->item_box_w = bp->item_w + 2 * BROWSER_XMARGIN;
	bp->item_box_h = bp->item_h * (bp->bot_visible - bp->top_visible + 1)
		+ 2 * BROWSER_YMARGIN;
	initScrollbar(&bp->scrollbar, 0, 0, SB_WIDTH,
		bp->item_box_h - 2 * POPUP_FRAME_MARGIN,
		0, (bp->bot_visible - bp->top_visible + 1) * 100 / bp->nitems);
	bp->item_box_w += SB_WIDTH + POPUP_FRAME_MARGIN;
    } else {
	bp->item_box_h = -BROWSER_YMARGIN;
    }
    initButton(&bp->button, 0, 0, button_string, BUTTON_DRAW_RETURN);
    bp->h = bp->top_h + bp->item_box_h + bp->bottom_h +
	    bp->button.loc.h + 5 * BROWSER_YMARGIN;
    bp->w = max(bp->top_w, bp->bottom_w);
    bp->w = max(bp->w, bp->item_box_w) + 2 * BROWSER_XMARGIN;
    bp->x = (SCREEN_W - bp->w) / 2;
    bp->y = (SCREEN_H - bp->h) / 2;
    if (nitems) {
	bp->item_box_x = bp->x + (bp->w - bp->item_box_w) / 2;
	bp->scrollbar.loc.x = bp->item_box_x + POPUP_FRAME_MARGIN;
	bp->item_box_y = bp->y + 2 * BROWSER_YMARGIN + bp->top_h;
	bp->scrollbar.loc.y = bp->item_box_y + POPUP_FRAME_MARGIN;
    }
    bp->bottom_y = bp->y + 3 * BROWSER_YMARGIN + bp->top_h + bp->item_box_h;
    bp->button.loc.x = bp->x + bp->w - bp->button.loc.w - BROWSER_XMARGIN;
    bp->button.loc.y = bp->y + bp->h - bp->button.loc.h - BROWSER_YMARGIN;
    
    bp->type = type;

    return bp;
}

inline void
destroyBrowser(
    browser_t *bp
)
{
/*    destroyButton(bp->button); */
/*    destroyScrollbar(bp->scrollbar); */
    zfree((char *)bp);
}

static int item_y(
    browser_t *bp,
    int item_num
)
{
    return ((bp)->item_box_y + BROWSER_YMARGIN +
	(item_num - bp->top_visible) * (bp)->item_h + (bp)->item_h / 2);
}

setItemHighlight(browser_t *bp, int item, int highlight)
{
    register int xpos = bp->item_box_x + POPUP_FRAME_MARGIN +
	SB_WIDTH + BROWSER_XMARGIN;
    if (currentMode() != GRAPHICS_MODE ||
	item < bp->top_visible ||
	item > bp->bot_visible)
	return;
    blit_clear(bp->item_w, xpos, item_y(bp, item),
    	CENTER_V, highlight ? COLOR_WHITE : TEXT_BG);
    if (bp->selected[item])
	copyImage( highlight ? &hdot_bitmap : &dot_bitmap,
	    xpos,
	    item_y(bp, item) - dot_bitmap_HEIGHT/2);
    xpos += BROWSER_XMARGIN + dot_bitmap_WIDTH;
    blit_string(bp->items[item], xpos, item_y(bp, item),
	TEXT_FG, CENTER_V);
}

static void drawItembox(browser_t *bp)
{
    register int i;
    bp->scrollbar.position = bp->top_visible * 100 / bp->nitems;
    drawScrollbar(&bp->scrollbar);
    for(i = bp->top_visible; i <= bp->bot_visible; i++)
	setItemHighlight(bp, i, i == bp->current);
}

static char *Separator = "----------------------------------------\n";

void drawBrowser(browser_t *bp)
{
    short item_y;
    register int i;
    
    clearActivityIndicator();
    if (currentMode() == TEXT_MODE) {
	printf("%s", Separator);
	if (bp->top_message) {
	    printf("%s\n%s",bp->top_message, Separator);
	}
	if (bp->nitems) {
	    for (i = bp->top_visible; i <= bp->bot_visible; i++) {
		printf("% 2d. %c %s\n",i+1,
		    bp->selected[i] ? '*' : ' ',
		    bp->items[i]);
	    }
	    if (bp->bot_visible != bp->nitems - 1)
		printf("......<More>\n");
	    printf(Separator);
	}
	if (bp->bottom_message) {
	    printf("%s\n",bp->bottom_message);
	}
	printf(">> ");
    } else { /* GRAPHICS_MODE */
	popupBox(bp->x, bp->y, bp->w, bp->h, TEXT_BG, POPUP_OUT);
	copyImage(&ns_logo_bitmap, bp->x + BROWSER_XMARGIN,
	    bp->y + BROWSER_YMARGIN + (bp->top_h - ns_logo_bitmap_HEIGHT)/2 );
	blit_string(bp->top_message,
	    bp->x + ns_logo_bitmap_WIDTH + 2 * BROWSER_XMARGIN,
	    bp->y + (bp->top_h / 2) + BROWSER_YMARGIN,
	    TEXT_FG, CENTER_V);
	if (bp->bottom_message)
	    blit_string(bp->bottom_message,
		bp->x + bp->w / 2, bp->bottom_y + bp->bottom_h / 2,
		TEXT_FG, CENTER_H | CENTER_V);
	drawButton(&bp->button);
	/* draw all items and highlight item 0 */
	if (bp->nitems) {
	    popupBox(bp->item_box_x, bp->item_box_y,
		    bp->item_box_w, bp->item_box_h,
		    TEXT_BG, POPUP_IN);
	    drawItembox(bp);
	}
    }
}

clearBrowser(browser_t *bp)
{
    if (currentMode() == GRAPHICS_MODE)
	clearBox(bp->x, bp->y, bp->w, bp->h);
}


selectItem(browser_t *bp, int item)
{
    int i;
    if (bp->selected[item] == 0 &&
	(bp->type == BROWSER_CURRENT_IS_SELECTED)) {
	for (i=0; i < bp->nitems; i++)
	    if (bp->selected[i]) {
		bp->selected[i] = 0;
		setItemHighlight(bp, i, i == bp->current);
	    }
    }
    bp->selected[item] = !bp->selected[item];
    setItemHighlight(bp, item, item == bp->current);
}

runBrowser(browser_t *bp)
{
    int c, i;
    char *s;
    
    if (bp->nitems == 0) {
	while ((c = getc() != '\r'));
	putc('\n');
	return;
    }
    
    if (currentMode() == TEXT_MODE) {
	s = zalloc(128);
	for(;;) {
	    gets(s, 128);
	    if ((*s == 'f') || (*s == 'F')) {
		if (currentMode() == TEXT_MODE)
		    if (bp->bot_visible != bp->nitems - 1) {
			bp->top_visible += MAX_ITEMS_VISIBLE;
			bp->bot_visible += MAX_ITEMS_VISIBLE;
			bp->bot_visible = min(bp->nitems - 1, bp->bot_visible);
			drawBrowser(bp);
			continue;
		    }
	    } else if ((*s == 'b') || (*s == 'B')) {
		if (currentMode() == TEXT_MODE)
		    if (bp->top_visible >= MAX_ITEMS_VISIBLE) {
			bp->top_visible -= MAX_ITEMS_VISIBLE;
			bp->bot_visible = bp->top_visible +
					MAX_ITEMS_VISIBLE - 1;
			drawBrowser(bp);
			continue;
		    }
	    }
	    if (*s == '\0')
		return;
	    i = atoi(s);
	    if (i < 1 || i > bp->nitems) {
		printf("Please enter a number between 1 and %d.\n"
			"Press <Return>:", 
			bp->nitems);
		while ((c = getc()) != '\r');
	    } else {
		selectItem(bp, i-1);
	    }
	    drawBrowser(bp);
	}
	zfree(s);
	return;
    }
    
    for(;;) {
	switch((c = getc())) {
	case 'j':
	case K_DOWN:
	    if (bp->current == bp->bot_visible &&
		bp->bot_visible != bp->nitems - 1) {
		bp->top_visible++;
		bp->bot_visible++;
		bp->current++;
		if (bp->type == BROWSER_CURRENT_IS_SELECTED)
		    selectItem(bp, bp->current);
		drawItembox(bp);
	    } else if (bp->current < bp->nitems - 1) {
		setItemHighlight(bp, bp->current, 0);
		bp->current++;
		if (bp->type == BROWSER_CURRENT_IS_SELECTED)
		    selectItem(bp, bp->current);
		else
		    setItemHighlight(bp, bp->current, 1);
	    }
	    break;
	case 'k':
	case K_UP:
	    if (bp->current == bp->top_visible &&
		bp->top_visible != 0) {
		bp->top_visible--;
		bp->bot_visible--;
		bp->current--;
		if (bp->type == BROWSER_CURRENT_IS_SELECTED)
		    selectItem(bp, bp->current);
		drawItembox(bp);
	    } else if (bp->current > 0) {
		setItemHighlight(bp, bp->current, 0);
		bp->current--;
		if (bp->type == BROWSER_CURRENT_IS_SELECTED)
		    selectItem(bp, bp->current);
		else
		    setItemHighlight(bp, bp->current, 1);
	    }
	    break;
	case ' ':
	    if (!(bp->type == BROWSER_CURRENT_IS_SELECTED))
		selectItem(bp, bp->current);
	    break;
	case '\r':
	case '\n':
	    return;
	}
    }
}

static browser_t *_doBrowser(
    char **items,
    int nitems,
    char *top_message,
    char *bottom_message,
    char *button_string,
    int type
)
{
    browser_t *bp;
    
    bp = createBrowser(items, nitems,
	top_message, bottom_message, button_string, type);
    drawBrowser(bp);
    runBrowser(bp);
    clearBrowser(bp);
    return bp;
}

/*
 * returns an array of characters, one per item,
 * that are set to 0 if the item was not selected,
 * 1 if the item was selected.
 */
char * popupBrowser(
    char **items,
    int nitems,
    char *top_message,
    char *bottom_message,
    char *button_string,
    int type
)
{
    browser_t *bp;
    register char *selected;
    
    bp = _doBrowser(items, nitems,
	top_message, bottom_message, button_string, type);
    selected = bp->selected;
    destroyBrowser(bp);
    return selected;
}

void popupPanel(
    char *message
)
{
    browser_t *bp;
    
    bp = _doBrowser(0, 0,
	message, 0, "Continue", 0);
    zfree(bp->selected);
    destroyBrowser(bp);
}


