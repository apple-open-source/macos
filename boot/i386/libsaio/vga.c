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
#include "io_inline.h"
#include "libsaio.h"
#include "vga.h"

#define	VGA_SEQ_CNT     5
#define	VGA_CRT_CNT     25
#define	VGA_ATR_CNT     21
#define	VGA_GFX_CNT     9

typedef struct VGAState {
    /* Miscellaneous output register. */
    unsigned char miscOutput;
    /* Sequencer registers. */
    unsigned char sequencerData[VGA_SEQ_CNT];
    /* CRT Controller registers. */
    unsigned char crtcData[VGA_CRT_CNT];
    /* Graphics controller registers. */
    unsigned char graphicsData[VGA_GFX_CNT];
    /* Attribute controller registers. */
    unsigned char attrData[VGA_ATR_CNT];
} VGAState;

#if 0
/* VGA Mode 0x03. */

static const VGAState VGAMode3 = {
    /* Miscellaneous output register. */
    0x67,
    /* Sequencer registers. */
    { 0x01, 0x00, 0x03, 0x00, 0x02 },
    /* CRT controller registers. */
    {
	0x5f, 0x4f, 0x50, 0x82, 0x55, 0x81, 0xbf, 0x1f, 0x00, 0x4f,
	0x0d, 0x0e, 0x00, 0x00, 0x00, 0x00, 0x9c, 0x8e, 0x8f, 0x28,
	0x1f, 0x96, 0xb9, 0xa3, 0xff,
    },
    /* Graphics controller registers. */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0e, 0x00, 0xff },
    /* Attribute controller registers. */
    {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07, 0x38, 0x39,
	0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f, 0x0c, 0x00, 0x0f, 0x08,
	0x00,
    },
};
#endif

/* VGA Mode 0x12. */

static const VGAState VGAMode12 = {
    /* Miscellaneous output register. */
    0xe3,
    /* Sequencer registers. */
    {0x03, 0x21, 0x0f, 0x00, 0x06},
    /* CRT controller registers. */
    {
	0x5f, 0x4f, 0x50, 0x82, 0x54, 0x80, 0x0b, 0x3e, 0x00,
	0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0xea, 0x8c,
	0xdf, 0x28, 0x00, 0xe7, 0x04, 0xe3, 0xff,
    },
    /* Graphics controller registers. */
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x05, 0x0f, 0xff },
    /* Attribute controller registers. */
    {
	0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,
	0x00, 0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x03,
	0x01, 0x00,
	0x03, 0x00,
	0x00,
    },
};

#define palette_entry(r,g,b) (((r) << 4)|((g) << 2)|(b))

static const unsigned char colorData[4] = {
    0x00, 0x15, 0x2a, 0x3f,
};

#ifdef NOTYET
static const unsigned char mode_03_palette[] = {
    palette_entry(0, 0, 0),
    palette_entry(0, 0, 2),
    palette_entry(0, 2, 0),
    palette_entry(0, 2, 2),
    palette_entry(2, 0, 0),
    palette_entry(2, 0, 2),
    palette_entry(2, 2, 0),
    palette_entry(2, 2, 2),
    palette_entry(0, 0, 1),
    palette_entry(0, 0, 3),
    palette_entry(0, 2, 1),
    palette_entry(0, 2, 3),
    palette_entry(2, 0, 1),
    palette_entry(2, 0, 3),
    palette_entry(2, 2, 1),
    palette_entry(2, 2, 3),
    palette_entry(0, 1, 0),
    palette_entry(0, 1, 2),
    palette_entry(0, 3, 0),
    palette_entry(0, 3, 2),
    palette_entry(2, 1, 0),
    palette_entry(2, 1, 2),
    palette_entry(2, 3, 0),
    palette_entry(2, 3, 2),
    palette_entry(0, 1, 1),
    palette_entry(0, 1, 3),
    palette_entry(0, 3, 1),
    palette_entry(0, 3, 3),
    palette_entry(2, 1, 1),
    palette_entry(2, 1, 3),
    palette_entry(2, 3, 1),
    palette_entry(2, 3, 3),
    palette_entry(1, 0, 0),
    palette_entry(1, 0, 2),
    palette_entry(1, 2, 0),
    palette_entry(1, 2, 2),
    palette_entry(3, 0, 0),
    palette_entry(3, 0, 2),
    palette_entry(3, 2, 0),
    palette_entry(3, 2, 2),
    palette_entry(1, 0, 1),
    palette_entry(1, 0, 3),
    palette_entry(1, 2, 1),
    palette_entry(1, 2, 3),
    palette_entry(3, 0, 1),
    palette_entry(3, 0, 3),
    palette_entry(3, 2, 1),
    palette_entry(3, 2, 3),
    palette_entry(1, 1, 0),
    palette_entry(1, 1, 2),
    palette_entry(1, 3, 0),
    palette_entry(1, 3, 2),
    palette_entry(3, 1, 0),
    palette_entry(3, 1, 2),
    palette_entry(3, 3, 0),
    palette_entry(3, 3, 2),
    palette_entry(1, 1, 1),
    palette_entry(1, 1, 3),
    palette_entry(1, 3, 1),
    palette_entry(1, 3, 3),
    palette_entry(3, 1, 1),
    palette_entry(3, 1, 3),
    palette_entry(3, 3, 1),
    palette_entry(3, 3, 3),
};
#endif NOTYET

static void
spin(volatile int count)
{
    while (count--)
	count = count;
}

void
set_video_mode(unsigned int mode)
{
    register unsigned int j, k;
    const VGAState *      state;
    extern unsigned short screen_width;
    extern unsigned short screen_height;

    screen_width  = 640;
    screen_height = 480;

    switch (mode) {
        case 0x02:
        case 0x03:
            video_mode(mode);
            return;
            /*
            state = &VGAMode3;
            break;
            */
        case 0x12:
            state = &VGAMode12;
            break;
        default:
            return;
    }

    /* Turn the video off while we are doing this.... */
    outb(VGA_SEQ_INDEX, 1);
    outb(VGA_SEQ_DATA, state->sequencerData[1]);

    /* Set the attribute flip-flop to "index" */
    inb(VGA_INPUT_STATUS_1);

    /* Give palette to CPU, turns off video */
    outb(VGA_WRITE_ATTR_INDEX, 0x00);

    /* Set the general registers */
    outb(VGA_WRITE_MISC_PORT, state->miscOutput);
    outb(VGA_WRITE_FEATURE_PORT, 0x00);

    /* Load the sequencer registers */
    for (k = 0; k < VGA_SEQ_CNT; k++) {
        outb(VGA_SEQ_INDEX, k);
        outb(VGA_SEQ_DATA, state->sequencerData[k]);
    }
    outb(VGA_SEQ_INDEX, 0x00);
    outb(VGA_SEQ_DATA, 0x03);	/* Low order two bits are reset bits */
    
    /* Load the CRTC registers.
     * CRTC registers 0-7 are locked by a bit in register 0x11. We need
     * to unlock these registers before we can start setting them.
     */
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, 0x00);		/* Unlocks registers 0-7 */
    for (k = 0; k < VGA_CRT_CNT; k++) {
        outb(VGA_CRTC_INDEX, k);
        outb(VGA_CRTC_DATA, state->crtcData[k]);
    }

    /* Load the attribute registers */
    inb(VGA_INPUT_STATUS_1);  /* Set the attribute flip-flop to "index" */
    for (k = 0; k < VGA_ATR_CNT; k++) {
        outb(VGA_WRITE_ATTR_INDEX, k);
        outb(VGA_WRITE_ATTR_DATA, state->attrData[k]);
    }
    
    /* Load graphics registers */
    for (k = 0; k < VGA_GFX_CNT; k++) {
        outb(VGA_GFX_INDEX, k);
        outb(VGA_GFX_DATA, state->graphicsData[k]);
    }    

    /* Set up the palette. */

    outb(VGA_PALETTE_WRITE, 0);
    for (k = 0; k < 256; k++) {
#ifdef NOTYET
        if (mode == 0x12) {
#endif NOTYET
            j = colorData[k % 4];
            outb(VGA_PALETTE_DATA, j); spin(1000);
            outb(VGA_PALETTE_DATA, j); spin(1000);
            outb(VGA_PALETTE_DATA, j); spin(1000);
#ifdef NOTYET
        } else {
            j = mode_03_palette[(k - 64) % 64];
            outb(VGA_PALETTE_DATA, colorData[(j >> 4) & 3]);
            outb(VGA_PALETTE_DATA, colorData[(j >> 2) & 3]);
            outb(VGA_PALETTE_DATA, colorData[j & 3]);
        }
#endif NOTYET
    }

    /* Re-enable video */
    /* First, clear memory to zeros */
    bzero((void *) VGA_BUF_ADDR, VGA_BUF_LENGTH);

    /* Set the attribute flip-flop to "index" */
    inb(VGA_INPUT_STATUS_1);

    /* Give the palette back to the VGA */
    outb(VGA_WRITE_ATTR_INDEX, 0x20);

    // Really re-enable video.
    outb(VGA_SEQ_INDEX, 1);
    outb(VGA_SEQ_DATA, (state->sequencerData[1] & ~0x20));
}
