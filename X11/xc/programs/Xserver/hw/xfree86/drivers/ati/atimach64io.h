/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atimach64io.h,v 1.14 2003/01/01 19:16:32 tsi Exp $ */
/*
 * Copyright 2000 through 2003 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
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

#ifndef ___ATIMACH64IO_H___

#if !defined(___ATI_H___) && defined(XFree86Module)
# error Missing #include "ati.h" before #include "atimach64io.h"
# undef XFree86Module
#endif

#define ___ATIMACH64IO_H___ 1

#include "atiio.h"
#include "atistruct.h"

/*
 * A few important notes on some of the I/O statements provided:
 *
 * inl/outl     32-bit R/W through PIO space.  The register is specified as the
 *              actual PIO address.  These are actually defined in compiler.h.
 *
 * inw/outw     16-bit counterparts to inl/outl.  Not used for Mach64 support.
 *
 * inb/outb     8-bit counterparts to inl/outl.
 *
 * inm/outm     32-bit R/W through MMIO space.  The register is specified as
 *              the actual MMIO offset (with Block 1 following Block 0), which,
 *              in this case, is equivalent to the register's IOPortTag from
 *              atiregs.h.  Can be used for those few non-FIFO'ed registers
 *              outside of Block 0's first 256 bytes.  inm() can also be used
 *              for FIFO'ed registers if, and only if, it can be guaranteed to
 *              not have been previously FIFO'ed (e.g. when the engine is
 *              idle).  pATI->pBlock array elements must have been previously
 *              set up by ATIMapApertures().
 *
 * outf         32-bit write through MMIO cache.  Identical to outm() but
 *              intended for FIFO'ed registers.  There is no inf() provided.
 *
 * inr/outr     32-bit R/W through PIO or MMIO.  Which one depends on the
 *              machine architecture.  The register is specified as a IOPortTag
 *              from atiregs.h.  Can only be used for registers in the first
 *              256 bytes of MMIO space (in Block 0).  Note that all of these
 *              registers are non-FIFO'ed.
 *
 * in8/out8     8-bit counterparts to inr/outr.
 *
 * For portability reasons, inr/outr/in8/out8 should be used in preference to
 * inl/outl/inb/outb to/from any register space starting with CRTC_H_TOTAL_DISP
 * but before DST_OFF_PITCH (in the order defined by atiregs.h).  None of
 * inm/outm/outf should ever be used for these registers.
 *
 * outf()'s should be grouped together as much as possible, while respecting
 * any ordering constraints the engine might impose.  Groups larger than 16
 * outf()'s should be split up into two or more groups as needed (but not
 * necessarily wanted).  The outf() groups that result should be immediately
 * preceeded by an ATIMach64WaitForFIFO(n) call, where "n" is the number of
 * outf()'s in the group with the exception that groups containing a single
 * outf() should not be thus preceeded.  This means "n" should not be less than
 * 2, nor larger than 16.
 */

/*
 * Cave canem (or it WILL bite you):  All Mach64 non-VGA registers are
 * ================================   little-endian, no matter how they are
 *                                    accessed (nor by what).
 */

#define inm(_Register)                                                   \
    MMIO_IN32(pATI->pBlock[GetBits(_Register, BLOCK_SELECT)],            \
              (_Register) & MM_IO_SELECT)
#define outm(_Register, _Value)                                          \
    MMIO_OUT32(pATI->pBlock[GetBits(_Register, BLOCK_SELECT)],           \
               (_Register) & MM_IO_SELECT, _Value)

#ifdef AVOID_CPIO

#   define inr(_Register) \
        MMIO_IN32(pATI->pBlock[0], (_Register) & MM_IO_SELECT)
#   define outr(_Register, _Value) \
        MMIO_OUT32(pATI->pBlock[0], (_Register) & MM_IO_SELECT, _Value)

#   define in8(_Register)                                                \
        MMIO_IN8(pATI->pBlock[0],                                        \
                 (_Register) & (MM_IO_SELECT | IO_BYTE_SELECT))
#   define out8(_Register, _Value)                                       \
        MMIO_OUT8(pATI->pBlock[0],                                       \
                  (_Register) & (MM_IO_SELECT | IO_BYTE_SELECT), _Value)

/* Cause a cpp syntax error if any of these are used */
#undef inb
#undef inw
#undef inl
#undef outb
#undef outw
#undef outl

#define inb()            /* Nothing */
#define inw()            /* Nothing */
#define inl()            /* Nothing */
#define outb()           /* Nothing */
#define outw()           /* Nothing */
#define outl()           /* Nothing */

#else /* AVOID_CPIO */

#   define ATIIOPort(_PortTag)                                 \
        (((pATI->CPIODecoding == SPARSE_IO) ?                  \
          ((_PortTag) & (SPARSE_IO_SELECT | IO_BYTE_SELECT)) : \
          ((_PortTag) & (BLOCK_IO_SELECT | IO_BYTE_SELECT))) | \
         pATI->CPIOBase)

#   define inr(_Register) \
        inl(ATIIOPort(_Register))
#   define outr(_Register, _Value) \
        outl(ATIIOPort(_Register), _Value)

#   define in8(_Register) \
        inb(ATIIOPort(_Register))
#   define out8(_Register, _Value) \
        outb(ATIIOPort(_Register), _Value)

#endif /* AVOID_CPIO */

extern void ATIMach64PollEngineStatus FunctionPrototype((ATIPtr));

/*
 * MMIO cache definitions.
 *
 * Many FIFO'ed registers can be cached by the driver.  Registers that qualify
 * for caching must not contain values that can change without driver
 * intervention.  Thus registers that contain hardware counters, strobe lines,
 * etc., cannot be cached.  This caching is intended to minimise FIFO use.
 * There is therefore not much point to enable it for non-FIFO'ed registers.
 *
 * The cache for a particular 32-bit register is enabled by coding a
 * CacheRegister() line for that register in the ATIMach64Set() function.  The
 * integrity of the cache for a particular register should be verified by the
 * ATIMach64Sync() function.  This code should be kept in register order, as
 * defined in atiregs.h.
 */
#define CacheByte(___Register) pATI->MMIOCached[CacheSlotOf(___Register) >> 3]
#define CacheBit(___Register)  (0x80U >> (CacheSlotOf(___Register) & 0x07U))

#define RegisterIsCached(__Register) \
    (CacheByte(__Register) & CacheBit(__Register))
#define CacheSlot(__Register) pATI->MMIOCache[CacheSlotOf(__Register)]

#define CacheRegister(__Register) \
    CacheByte(__Register) |= CacheBit(__Register)
#define UncacheRegister(__Register) \
    CacheByte(__Register) &= ~CacheBit(__Register)

/* This would be quite a bit slower as a function */
#define outf(_Register, _Value)                                        \
    do                                                                 \
    {                                                                  \
        CARD32 _IOValue = (_Value);                                    \
                                                                       \
        if (!RegisterIsCached(_Register) ||                            \
            (_IOValue != CacheSlot(_Register)))                        \
        {                                                              \
            while (!pATI->nAvailableFIFOEntries--)                     \
                ATIMach64PollEngineStatus(pATI);                       \
            MMIO_OUT32(pATI->pBlock[GetBits(_Register, BLOCK_SELECT)], \
                       (_Register) & MM_IO_SELECT, _IOValue);          \
            CacheSlot(_Register) = _IOValue;                           \
            pATI->EngineIsBusy = TRUE;                                 \
        }                                                              \
    } while (0)

/*
 * This is no longer as critical, especially for _n == 1.  However,
 * there is still a need to ensure _n <= pATI->nFIFOEntries.
 */
#define ATIMach64WaitForFIFO(_pATI, _n)        \
    while (pATI->nAvailableFIFOEntries < (_n)) \
        ATIMach64PollEngineStatus(pATI)

#define ATIMach64WaitForIdle(_pATI)      \
    while (pATI->EngineIsBusy)           \
        ATIMach64PollEngineStatus(pATI)

extern void ATIAccessMach64PLLReg FunctionPrototype((ATIPtr, const CARD8,
                                                     const Bool));

#define ATIGetMach64PLLReg(_Index)                  \
    (                                               \
        ATIAccessMach64PLLReg(pATI, _Index, FALSE), \
        in8(CLOCK_CNTL + 2)                         \
    )
#define ATIPutMach64PLLReg(_Index, _Value)          \
    do                                              \
    {                                               \
        ATIAccessMach64PLLReg(pATI, _Index, TRUE);  \
        out8(CLOCK_CNTL + 2, _Value);               \
    } while (0)

#define ATIGetMach64LCDReg(_Index)                       \
    (                                                    \
        out8(LCD_INDEX, SetBits(_Index, LCD_REG_INDEX)), \
        inr(LCD_DATA)                                    \
    )
#define ATIPutMach64LCDReg(_Index, _Value)               \
    do                                                   \
    {                                                    \
        out8(LCD_INDEX, SetBits(_Index, LCD_REG_INDEX)); \
        outr(LCD_DATA, _Value);                          \
    } while (0)

#define ATIGetMach64TVReg(_Index)                          \
    (                                                      \
        out8(TV_OUT_INDEX, SetBits(_Index, TV_REG_INDEX)), \
        inr(TV_OUT_DATA)                                   \
    )
#define ATIPutMach64TVReg(_Index, _Value)                  \
    do                                                     \
    {                                                      \
        out8(TV_OUT_INDEX, SetBits(_Index, TV_REG_INDEX)); \
        outr(TV_OUT_DATA, _Value);                         \
    } while (0)

/*
 * Block transfer definitions.
 */

#if defined(GCCUSESGAS) && \
    (defined(i386) || defined(__i386) || defined(__i386__))

#define ATIMove32(_pDst, _pSrc, _nCount) \
    do                                   \
    {                                    \
        long d0, d1, d2;                 \
        __asm__ __volatile__             \
        (                                \
            "cld\n\t"                    \
            "rep ; movsl"                \
            : "=&c" (d0),                \
              "=&D" (d1),                \
              "=&S" (d2)                 \
            : "0" (_nCount),             \
              "1" (_pDst),               \
              "2" (_pSrc)                \
            : "memory"                   \
        );                               \
    } while (0)

#endif

#endif /* ___ATIMACH64IO_H___ */
