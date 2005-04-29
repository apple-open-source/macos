/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_video.h,v 1.3 2003/08/27 15:16:14 tsi Exp $ */
/*
 * Copyright 1998-2003 VIA Technologies, Inc. All Rights Reserved.
 * Copyright 2001-2003 S3 Graphics, Inc. All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sub license,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL
 * VIA, S3 GRAPHICS, AND/OR ITS SUPPLIERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef _VIA_VIDEO_H_
#define _VIA_VIDEO_H_ 1

/*
 * I N C L U D E S
 */

/* #define   XV_DEBUG      1 */   /* write log msg to /var/log/XFree86.0.log */
#define   COLOR_KEY       1    /* set color key value from driver layer*/

#ifdef XV_DEBUG
# define DBG_DD(x) (x)
#else
# define DBG_DD(x)
#endif

#define HW_3123

#define TRUE        1
#define FALSE       0

/* Definition for VideoStatus */
#define VIDEO_NULL              0x00000000
#define TV0SURFACE_CREATED      0x00000001
#define TV1SURFACE_CREATED      0x00000002
#define SWOV_SURFACE_CREATED    0x00000004
#define HW_MPEG_ON              0x00000010
#define TV0_VIDEO_ON            0x00000020
#define TV1_VIDEO_ON            0x00000040
#define SW_VIDEO_ON             0x00000080

typedef struct {
  unsigned long dwWidth;          /* On screen Width                  */
  unsigned long dwHeight;         /* On screen Height                 */
  unsigned long dwBPP;            /* Bits Per Pixel                   */
  unsigned long dwRefreshRate;    /* Refresh rate of the mode         */
}MODEINFO, * LPMODEINFO;

#define SDR100  1
#define SDR133  2
#define DDR100  3
#define DDR133  4


typedef struct{ 
      unsigned long interruptflag;         /* 200 */
      unsigned long ramtab;                /* 204 */
      unsigned long alphawin_hvstart;      /* 208 */
      unsigned long alphawin_size;         /* 20c */
      unsigned long alphawin_ctl;          /* 210 */
      unsigned long crt_startaddr;         /* 214 */
      unsigned long crt_startaddr_2;       /* 218 */
      unsigned long alphafb_stride ;       /* 21c */
      unsigned long color_key;             /* 220 */
      unsigned long alphafb_addr;          /* 224 */
      unsigned long chroma_low;            /* 228 */
      unsigned long chroma_up;             /* 22c */
      unsigned long video1_ctl;            /* 230 */
      unsigned long video1_fetch;          /* 234 */
      unsigned long video1y_addr1;         /* 238 */
      unsigned long video1_stride;         /* 23c */
      unsigned long video1_hvstart;        /* 240 */
      unsigned long video1_size;           /* 244 */
      unsigned long video1y_addr2;         /* 248 */
      unsigned long video1_zoom;           /* 24c */
      unsigned long video1_mictl;          /* 250 */
      unsigned long video1y_addr0;         /* 254 */
      unsigned long video1_fifo;           /* 258 */
      unsigned long video1y_addr3;         /* 25c */
      unsigned long hi_control;            /* 260 */
      unsigned long snd_color_key;         /* 264 */
      unsigned long v3alpha_prefifo;       /* 268 */
      unsigned long v1_source_w_h;         /* 26c */
      unsigned long hi_transparent_color;  /* 270 */
      unsigned long v_display_temp;        /* 274 :No use */
      unsigned long v3alpha_fifo;          /* 278 */
      unsigned long v3_source_width;       /* 27c */
      unsigned long dummy1;                /* 280 */
      unsigned long video1_CSC1;           /* 284 */
      unsigned long video1_CSC2;           /* 288 */
      unsigned long video1u_addr0;         /* 28c */
      unsigned long video1_opqctl;         /* 290 */
      unsigned long video3_opqctl;         /* 294 */
      unsigned long compose;               /* 298 */
      unsigned long dummy2;                /* 29c */
      unsigned long video3_ctl;            /* 2a0 */
      unsigned long video3_addr0;          /* 2a4 */
      unsigned long video3_addr1;          /* 2a8 */
      unsigned long video3_stribe;         /* 2ac */
      unsigned long video3_hvstart;        /* 2b0 */
      unsigned long video3_size;           /* 2b4 */
      unsigned long v3alpha_fetch;         /* 2b8 */
      unsigned long video3_zoom;           /* 2bc */
      unsigned long video3_mictl;          /* 2c0 */
      unsigned long video3_CSC1;           /* 2c4 */
      unsigned long video3_CSC2;           /* 2c8 */
      unsigned long v3_display_temp;       /* 2cc */
      unsigned long reserved[5];           /* 2d0 */
      unsigned long video1u_addr1;         /* 2e4 */
      unsigned long video1u_addr2;         /* 2e8 */
      unsigned long video1u_addr3;         /* 2ec */
      unsigned long video1v_addr0;         /* 2f0 */
      unsigned long video1v_addr1;         /* 2f4 */
      unsigned long video1v_addr2;         /* 2f8 */
      unsigned long video1v_addr3;         /* 2fc */
}  video_via_regs;     

#define vmmtr volatile video_via_regs *

#endif /* _VIA_VIDEO_H_ */
