/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/via/via_compose.h,v 1.2 2003/08/27 15:16:08 tsi Exp $ */
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

/* Definition of APs Distribution */
#ifndef _VIA_COMPOSE_H_
#define _VIA_COMPOSE_H_ 1

  /* Video Control definition */
#define VW_DVD_ONLY              0x00000001  /* ????????????????? */
#define VW_TV_ONLY               0x00000002  /* ????????????????? */
/*#define VW_INTERNAL_OPAQUE       0x00000008 */ /* ????????????????? */

#define VW_DVD_TOP               0x00000010  /* ATOM("XV_COMPOSE") */
#define VW_TV_TOP                0x00000020  /* ATOM("XV_COMPOSE") */
#define VW_TV0_TOP               0x00000100  /* ATOM("XV_COMPOSE") */
#define VW_TV1_TOP               0x00000200  /* ATOM("XV_COMPOSE") */
/*#define VW_TV_FULL               0x00000400 */ /*Not recommended*/

#define VW_BOB_TV0               0x00010000  /* ATOM("XV_BOB") */
#define VW_BOB_TV1               0x00020000  /* ATOM("XV_BOB") */
/*#define VW_WEAVE_TV1             0x00040000 */ /* ATOM("XV_BOB") */
/*#define VW_WEAVE_TV2             0x00080000 */ /* ATOM("XV_BOB") */

#define VW_HIGHQVDO_OFF          0x00000000  /* ATOM("XV_HIGHQVDO") */
#define VW_HIGHQVDO_DVD          0x00000001  /* ATOM("XV_HIGHQVDO") */
#define VW_HIGHQVDO_TV0          0x00000002  /* ATOM("XV_HIGHQVDO") */
#define VW_HIGHQVDO_TV1          0x00000004  /* ATOM("XV_HIGHQVDO") */

  /* Audio Control definition */
  /* Quality */
#define AUDIO_STEREO       0x00000001        /* ATOM("XV_AUDIOCTRL") */
#define AUDIO_SAP          0x00000002        /* ATOM("XV_AUDIOCTRL") */
#define AUDIO_DUAL         0x00000004        /* ATOM("XV_AUDIOCTRL") */

  /* Mute */
#define AUDIO_ON           0x00000010        /* ATOM("XV_AUDIOCTRL") */
#define AUDIO_OFF          0x00000020        /* ATOM("XV_AUDIOCTRL") */

  /* Source */  
#define AUDIO_TV0          0x00000100        /* ATOM("XV_AUDIOCTRL") */
#define AUDIO_TV1          0x00000200        /* ATOM("XV_AUDIOCTRL") */
#define AUDIO_SVIDEO       0x00000400        /* not available now    */
#define AUDIO_COMPOSE      0x00000800        /* 		               */	
                                                
#endif  /* _VIA_COMPOSE_H_ */
