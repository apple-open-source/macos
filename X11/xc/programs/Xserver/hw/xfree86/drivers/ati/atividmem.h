/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atividmem.h,v 1.9 2003/01/01 19:16:35 tsi Exp $ */
/*
 * Copyright 1997 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ___ATIVIDMEM_H___
#define ___ATIVIDMEM_H___ 1

#include "atiproto.h"
#include "atipriv.h"

/* Memory types for 68800's and 88800GX's */
typedef enum
{
    MEM_MACH_DRAMx4,
    MEM_MACH_VRAM,
    MEM_MACH_VRAMssr,
    MEM_MACH_DRAMx16,
    MEM_MACH_GDRAM,
    MEM_MACH_EVRAM,
    MEM_MACH_EVRAMssr,
    MEM_MACH_TYPE_7
} ATIMachMemoryType;
extern const char *ATIMemoryTypeNames_Mach[];

/* Memory types for 88800CX's */
typedef enum
{
    MEM_CX_DRAM,
    MEM_CX_EDO,
    MEM_CX_TYPE_2,
    MEM_CX_DRAM_A,
    MEM_CX_TYPE_4,
    MEM_CX_TYPE_5,
    MEM_CX_TYPE_6,
    MEM_CX_TYPE_7
} ATICXMemoryType;
extern const char *ATIMemoryTypeNames_88800CX[];

/* Memory types for 264xT's */
typedef enum
{
    MEM_264_NONE,
    MEM_264_DRAM,
    MEM_264_EDO,
    MEM_264_PSEUDO_EDO,
    MEM_264_SDRAM,
    MEM_264_SGRAM,
    MEM_264_SGRAM32,
    MEM_264_TYPE_7
} ATI264MemoryType;
extern const char *ATIMemoryTypeNames_264xT[];

extern Bool ATIMapApertures   FunctionPrototype((int, ATIPtr));
extern void ATIUnmapApertures FunctionPrototype((int, ATIPtr));

#endif /* ___ATIVIDMEM_H___ */
