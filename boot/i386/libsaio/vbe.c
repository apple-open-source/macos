/*
 * Copyright (c) 1999-2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 2.0 (the "License").  You may not use this file
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

#include "libsaio.h"
#include "vbe.h"

/* 
 * Various inline routines for video I/O
 */
static inline void
outi (int port, int index, int val)
{
    outw (port, (val << 8) | index);
}

static inline void
outib (int port, int index, int val)
{
    outb (port, index);
    outb (port + 1, val);
}

static inline int
ini (int port, int index)
{
    outb (port, index);
    return inb (port + 1);
}

static inline void
rmwi (int port, int index, int clear, int set)
{
    outb (port, index);
    outb (port + 1, (inb (port + 1) & ~clear) | set);
}

/*
 * Globals
 */
static biosBuf_t bb;

int getVBEInfo( void * infoBlock )
{
    bb.intno  = 0x10;
    bb.eax.rr = funcGetControllerInfo;
    bb.es     = SEG( infoBlock );
    bb.edi.rr = OFF( infoBlock );
    bios( &bb );
    return(bb.eax.r.h);
}

int getVBEModeInfo( int mode, void * minfo_p )
{
    bb.intno  = 0x10;
    bb.eax.rr = funcGetModeInfo;
    bb.ecx.rr = mode;
    bb.es     = SEG(minfo_p);
    bb.edi.rr = OFF(minfo_p);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBEDACFormat(unsigned char *format)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteFormat;
    bb.ebx.r.l = subfuncGet;
    bios(&bb);
    *format = bb.ebx.r.h;
    return(bb.eax.r.h);
}

int setVBEDACFormat(unsigned char format)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteFormat;
    bb.ebx.r.l = subfuncSet;
    bb.ebx.r.h = format;
    bios(&bb);
    return(bb.eax.r.h);
}

/*
 * Default GTF parameter values.
 */
#define kCellGranularity  8.0    // character cell granularity
#define kMinVSyncPlusBP   550.0  // min VSync + BP interval (us)
#define kMinFrontPorch    1.0    // minimum front porch in lines(V)/cells(H)
#define kVSyncLines       3.0    // width of VSync in lines
#define kHSyncWidth       8.0    // HSync as a percent of total line width
#define kC                30.0   // C = (C'-J) * (K/256) + J
#define kM                300.0  // M = K/256 * M'

static inline double Round( double f )
{
    asm volatile ("frndint" : "=t" (f) : "0" (f));
    return f;
}

static inline double Sqrt( double f )
{
    asm volatile ("fsqrt" : "=t" (f) : "0" (f));
    return f;
}

int generateCRTCTiming( unsigned short     width,
                        unsigned short     height,
                        unsigned long      paramValue,  
                        int                paramType,
                        VBECRTCInfoBlock * timing )
{
    double h_period_est, h_freq, h_period, h_total_pixels, h_sync_pixels;
    double h_active_pixels, h_ideal_duty_cycle, h_blank_pixels, pixel_freq = 0;
    double v_sync_plus_bp = 0, v_total_lines = 0, v_field_rate_est, v_frame_rate = 0;
    const double h_pixels = (double) width;
    const double v_lines  = (double) height;

    enum {
        left_margin_pixels  = 0,
        right_margin_pixels = 0,
        top_margin_lines    = 0,
        bot_margin_lines    = 0,
        interlace           = 0
    };

    // Total number of active pixels in image and both margins
    h_active_pixels = h_pixels + left_margin_pixels + right_margin_pixels;

    if (paramType == kCRTCParamPixelClock)
    {
        // Pixel clock provided in MHz
        pixel_freq = (double) paramValue / 1000000;

        // Ideal horizontal period from the blanking duty cycle equation
        h_period = ((kC - 100) + (Sqrt(((100 - kC) * (100 - kC)) + (0.4 * kM *
                    (h_active_pixels + right_margin_pixels + left_margin_pixels) /
                     pixel_freq)))) / 2.0 / kM * 1000;
    }
    else /* kCRTCParamRefreshRate */
    {
        double v_field_rate_in = (double) paramValue;

        // Estimate the horizontal period
        h_period_est = ((1 / v_field_rate_in) - kMinVSyncPlusBP / 1000000) /
                        (v_lines + (2 * top_margin_lines) + kMinFrontPorch + interlace) * 
                        1000000;

        // Number of lines in Vsync + back porch
        v_sync_plus_bp = Round(kMinVSyncPlusBP / h_period_est);

        // Total number of lines in Vetical field period
        v_total_lines = v_lines + top_margin_lines + bot_margin_lines +
                        v_sync_plus_bp + interlace + kMinFrontPorch;

        // Estimate the vertical field frequency
        v_field_rate_est = 1 / h_period_est / v_total_lines * 1000000;

        // Find the actual horizontal period
        h_period = h_period_est / (v_field_rate_in / v_field_rate_est);

        // Find the vertical frame rate (no interlace)
        v_frame_rate = 1 / h_period / v_total_lines * 1000000;
    }

    // Ideal blanking duty cycle from the blanking duty cycle equation
    h_ideal_duty_cycle = kC - (kM * h_period / 1000);

    // Number of pixels in the blanking time to the nearest double character cell
    h_blank_pixels = Round(h_active_pixels * h_ideal_duty_cycle /
                           (100 - h_ideal_duty_cycle) / (2 * kCellGranularity)) *
                     (2 * kCellGranularity);

    // Total number of horizontal pixels
    h_total_pixels = h_active_pixels + h_blank_pixels;

    if (paramType == kCRTCParamPixelClock)
    {
        // Horizontal frequency
        h_freq = pixel_freq / h_total_pixels * 1000;

        // Number of lines in V sync + back porch
        v_sync_plus_bp = Round(kMinVSyncPlusBP * h_freq / 1000);

        // Total number of lines in vertical field period
        v_total_lines = v_lines + top_margin_lines + bot_margin_lines +
                        interlace + v_sync_plus_bp + kMinFrontPorch;

        // Vertical frame frequency
        v_frame_rate = Round(h_freq / v_total_lines * 1000);
    }
    else
    {
        // Find pixel clock frequency
        pixel_freq = Round(h_total_pixels / h_period);
    }

    h_sync_pixels = Round(h_total_pixels * kHSyncWidth / 100 / kCellGranularity) *
                    kCellGranularity;

    timing->HTotal      = h_total_pixels;
    timing->HSyncStart  = h_active_pixels + (h_blank_pixels / 2) - h_sync_pixels;
    timing->HSyncEnd    = timing->HSyncStart + h_sync_pixels;
    timing->VTotal      = v_total_lines;
    timing->VSyncStart  = v_total_lines - v_sync_plus_bp;
    timing->VSyncEnd    = timing->VSyncStart + kVSyncLines;
    timing->Flags       = kCRTCNegativeHorizontalSync;
    timing->PixelClock  = pixel_freq * 1000000;
    timing->RefreshRate = v_frame_rate * 100;

    return 0;
}

int setVBEMode(unsigned short mode, const VBECRTCInfoBlock * timing)
{
    bb.intno  = 0x10;
    bb.eax.rr = funcSetMode;
    bb.ebx.rr = mode;
    if (timing) {
        bb.es     = SEG(timing);
        bb.edi.rr = OFF(timing);
    }
    bios(&bb);
    return(bb.eax.r.h);
}

int setVBEPalette(void *palette)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteData;
    bb.ebx.r.l = subfuncSet;
    bb.ecx.rr = 256;
    bb.edx.rr = 0;
    bb.es = SEG(palette);
    bb.edi.rr = OFF(palette);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBEPalette(void *palette)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetSetPaletteData;
    bb.ebx.r.l = subfuncGet;
    bb.ecx.rr = 256;
    bb.edx.rr = 0;
    bb.es = SEG(palette);
    bb.edi.rr = OFF(palette);
    bios(&bb);
    return(bb.eax.r.h);
}

int getVBECurrentMode(unsigned short *mode)
{
    bb.intno = 0x10;
    bb.eax.rr = funcGetCurrentMode;
    bios(&bb);
    *mode = bb.ebx.rr;
    return(bb.eax.r.h);
}

int getVBEPixelClock(unsigned short mode, unsigned long * pixelClock)
{
    bb.intno   = 0x10;
    bb.eax.rr  = funcGetSetPixelClock;
    bb.ebx.r.l = 0;
    bb.ecx.rx  = *pixelClock;
    bb.edx.rr  = mode;
    bios(&bb);
    *pixelClock = bb.ecx.rx;
    return(bb.eax.r.h);
}
