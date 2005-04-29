/* $XFree86: xc/programs/Xserver/hw/xfree86/parser/xf86tokens.h,v 1.21 2004/02/13 23:58:50 dawes Exp $ */
/* 
 * 
 * Copyright (c) 1997  Metro Link Incorporated
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"), 
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 * 
 * Except as contained in this notice, the name of the Metro Link shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from Metro Link.
 * 
 */
/*
 * Copyright (c) 1997-2003 by The XFree86 Project, Inc.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject
 * to the following conditions:
 *
 *   1.  Redistributions of source code must retain the above copyright
 *       notice, this list of conditions, and the following disclaimer.
 *
 *   2.  Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer
 *       in the documentation and/or other materials provided with the
 *       distribution, and in the same place and form as other copyright,
 *       license and disclaimer information.
 *
 *   3.  The end-user documentation included with the redistribution,
 *       if any, must include the following acknowledgment: "This product
 *       includes software developed by The XFree86 Project, Inc
 *       (http://www.xfree86.org/) and its contributors", in the same
 *       place and form as other third-party acknowledgments.  Alternately,
 *       this acknowledgment may appear in the software itself, in the
 *       same form and location as other such third-party acknowledgments.
 *
 *   4.  Except as contained in this notice, the name of The XFree86
 *       Project, Inc shall not be used in advertising or otherwise to
 *       promote the sale, use or other dealings in this Software without
 *       prior written authorization from The XFree86 Project, Inc.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESSED OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE XFREE86 PROJECT, INC OR ITS CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef _xf86_tokens_h
#define _xf86_tokens_h

/* Undefine symbols that some OSs might define */
#undef IOBASE

/* 
 * Each token should have a unique value regardless of the section
 * it is used in.
 */

typedef enum {
    /* errno-style tokens */
    EOF_TOKEN		= -4,
    LOCK_TOKEN		= -3,
    ERROR_TOKEN		= -2,

    /* value type tokens */
    NUMBER		= 1,
    STRING,

    /* Tokens that can appear in many sections */
    SECTION,
    SUBSECTION,
    ENDSECTION,
    ENDSUBSECTION,
    IDENTIFIER,
    VENDOR,
    DASH,
    COMMA,
    OPTION,
    COMMENT,

    /* Frequency units */
    HRZ,
    KHZ,
    MHZ,

    /* File tokens */
    FONTPATH,
    RGBPATH,
    MODULEPATH,
    INPUTDEVICES,
    LOGFILEPATH,

    /* Server Flag tokens.  These are deprecated in favour of generic Options */
    NOTRAPSIGNALS,
    DONTZAP,
    DONTZOOM,
    DISABLEVIDMODE,
    ALLOWNONLOCAL,
    DISABLEMODINDEV,
    MODINDEVALLOWNONLOCAL,
    ALLOWMOUSEOPENFAIL,
    BLANKTIME,
    STANDBYTIME,
    SUSPENDTIME,
    OFFTIME,
    DEFAULTLAYOUT,

    /* Monitor tokens */
    MODEL,
    MODELINE,
    DISPLAYSIZE,
    HORIZSYNC,
    VERTREFRESH,
    MODE,
    GAMMA,
    USEMODES,

    /* Modes tokens */
    /* no new ones */

    /* Mode tokens */
    DOTCLOCK,
    HTIMINGS,
    VTIMINGS,
    FLAGS,
    HSKEW,
    BCAST,
    VSCAN,
    ENDMODE,

    /* Screen tokens */
    OBSDRIVER,
    MDEVICE,
    MONITOR,
    SCREENNO,
    DEFAULTDEPTH,
    DEFAULTBPP,
    DEFAULTFBBPP,
    
    /* VideoAdaptor tokens */
    VIDEOADAPTOR,

    /* Mode timing tokens */
    TT_INTERLACE,
    TT_PHSYNC,
    TT_NHSYNC,
    TT_PVSYNC,
    TT_NVSYNC,
    TT_CSYNC,
    TT_PCSYNC,
    TT_NCSYNC,
    TT_DBLSCAN,
    TT_HSKEW,
    TT_BCAST,
    TT_VSCAN,
    TT_CUSTOM,

    /* Module tokens */
    LOAD,
    LOAD_DRIVER,
    
    /* Device tokens */
    DRIVER,
    CHIPSET,
    CLOCKS,
    VIDEORAM,
    BOARD,
    IOBASE,
    RAMDAC,
    DACSPEED,
    BIOSBASE,
    MEMBASE,
    CLOCKCHIP,
    CHIPID,
    CHIPREV,
    CARD,
    BUSID,
    TEXTCLOCKFRQ,
    IRQ,

    /* Keyboard tokens */
    AUTOREPEAT,
    XLEDS,
    KPROTOCOL,
    XKBKEYMAP,
    XKBCOMPAT,
    XKBTYPES,
    XKBKEYCODES,
    XKBGEOMETRY,
    XKBSYMBOLS,
    XKBDISABLE,
    PANIX106,
    XKBRULES,
    XKBMODEL,
    XKBLAYOUT,
    XKBVARIANT,
    XKBOPTIONS,
    /* The next two have become ServerFlags options */
    VTINIT,
    VTSYSREQ,
    /* Obsolete keyboard tokens */
    SERVERNUM,
    LEFTALT,
    RIGHTALT,
    SCROLLLOCK_TOK,
    RIGHTCTL,
    /* arguments for the above obsolete tokens */
    CONF_KM_META,
    CONF_KM_COMPOSE,
    CONF_KM_MODESHIFT,
    CONF_KM_MODELOCK,
    CONF_KM_SCROLLLOCK,
    CONF_KM_CONTROL,

    /* Pointer tokens */
    EMULATE3,
    BAUDRATE,
    SAMPLERATE,
    PRESOLUTION,
    CLEARDTR,
    CLEARRTS,
    CHORDMIDDLE,
    PROTOCOL,
    PDEVICE,
    EM3TIMEOUT,
    DEVICE_NAME,
    ALWAYSCORE,
    PBUTTONS,
    ZAXISMAPPING,

    /* Pointer Z axis mapping tokens */
    XAXIS,
    YAXIS,

    /* Display tokens */
    MODES,
    VIEWPORT,
    VIRTUAL,
    VISUAL,
    BLACK_TOK,
    WHITE_TOK,
    DEPTH,
    BPP,
    WEIGHT,
    
    /* Layout Tokens */
    SCREEN,
    INACTIVE,
    INPUTDEVICE,

    /* Adjaceny Tokens */
    RIGHTOF,
    LEFTOF,
    ABOVE,
    BELOW,
    RELATIVE,
    ABSOLUTE,

    /* Vendor Tokens */
    VENDORNAME,

    /* DRI Tokens */
    GROUP,
    BUFFERS
} ParserTokens;

#endif /* _xf86_tokens_h */
