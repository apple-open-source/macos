/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
 *
 * Portions Copyright (C) 2006 - 2007 Apple Inc. All rights reserved.
 *
 * To anyone who acknowledges that this file is provided "AS IS"
 * without any express or implied warranty:
 *                 permission to use, copy, modify, and distribute this
 * file for any purpose is hereby granted without fee, provided that
 * the above copyright notices and this notice appears in all source
 * code copies, and that none of the names of Open Software
 * Foundation, Inc., Hewlett-Packard Company, or Digital Equipment
 * Corporation be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.  Neither Open Software Foundation, Inc., Hewlett-
 * Packard Company, nor Digital Equipment Corporation makes any
 * representations about the suitability of this software for any
 * purpose.
 * 
 */
/*
 */
#ifndef _UUIDP_H
#define _UUIDP_H	1
/*
**
**  NAME:
**
**      uuidp.h
**
**  FACILITY:
**
**      UUID
**
**  ABSTRACT:
**
**      Interface Definitions for UUID internal subroutines used by
**      the uuid module
**
**
*/

/*
 * Max size of a uuid string: tttttttt-tttt-cccc-cccc-nnnnnnnnnnnn
 * Note: this includes the implied '\0'
 */
#define UUID_C_UUID_STRING_MAX          37

/*
 * defines for time calculations
 */
#ifndef UUID_C_100NS_PER_SEC
#define UUID_C_100NS_PER_SEC            10000000
#endif

#ifndef UUID_C_100NS_PER_USEC
#define UUID_C_100NS_PER_USEC           10
#endif



/*
 * UADD_UVLW_2_UVLW - macro to add two unsigned 64-bit long integers
 *                      (ie. add two unsigned 'very' long words)
 *
 * Important note: It is important that this macro accommodate (and it does)
 *                 invocations where one of the addends is also the sum.
 *
 * This macro was snarfed from the DTSS group and was originally:
 *
 * UTCadd - macro to add two UTC times
 *
 * add lo and high order longword separately, using sign bits of the low-order
 * longwords to determine carry.  sign bits are tested before addition in two
 * cases - where sign bits match. when the addend sign bits differ the sign of
 * the result is also tested:
 *
 *        sign            sign
 *      addend 1        addend 2        carry?
 *
 *          1               1            TRUE
 *          1               0            TRUE if sign of sum clear
 *          0               1            TRUE if sign of sum clear
 *          0               0            FALSE
 */
#define UADD_UVLW_2_UVLW(add1, add2, sum)                               \
    if (!(((add1)->lo&0x80000000UL) ^ ((add2)->lo&0x80000000UL)))           \
    {                                                                   \
        if (((add1)->lo&0x80000000UL))                                    \
        {                                                               \
            (sum)->lo = (add1)->lo + (add2)->lo ;                       \
            (sum)->hi = (add1)->hi + (add2)->hi+1 ;                     \
        }                                                               \
        else                                                            \
        {                                                               \
            (sum)->lo  = (add1)->lo + (add2)->lo ;                      \
            (sum)->hi = (add1)->hi + (add2)->hi ;                       \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (sum)->lo = (add1)->lo + (add2)->lo ;                           \
        (sum)->hi = (add1)->hi + (add2)->hi ;                           \
        if (!((sum)->lo&0x80000000UL))                                    \
            (sum)->hi++ ;                                               \
    }


/*
 * UADD_ULW_2_UVLW - macro to add a 32-bit unsigned integer to
 *                   a 64-bit unsigned integer
 *
 * Note: see the UADD_UVLW_2_UVLW() macro
 *
 */
#define UADD_ULW_2_UVLW(add1, add2, sum)                                \
{                                                                       \
    (sum)->hi = (add2)->hi;                                             \
    if ((*add1) & (add2)->lo & 0x80000000UL)                              \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
        (sum)->hi++;                                                    \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
        if (!((sum)->lo & 0x80000000UL))                                  \
        {                                                               \
            (sum)->hi++;                                                \
        }                                                               \
    }                                                                   \
}


/*
 * UADD_UW_2_UVLW - macro to add a 16-bit unsigned integer to
 *                   a 64-bit unsigned integer
 *
 * Note: see the UADD_UVLW_2_UVLW() macro
 *
 */
#define UADD_UW_2_UVLW(add1, add2, sum)                                 \
{                                                                       \
    (sum)->hi = (add2)->hi;                                             \
    if ((add2)->lo & 0x80000000UL)                                        \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
        if (!((sum)->lo & 0x80000000UL))                                  \
        {                                                               \
            (sum)->hi++;                                                \
        }                                                               \
    }                                                                   \
    else                                                                \
    {                                                                   \
        (sum)->lo = (*add1) + (add2)->lo;                               \
    }                                                                   \
}



/*
 * a macro to set *status uuid_s_coding_error
 */
#ifdef  CODING_ERROR
#undef  CODING_ERROR
#endif

#ifdef  DEBUG_DCE_RPC
#define CODING_ERROR(status)        *(status) = uuid_s_coding_error
#else
#define CODING_ERROR(status)
#endif


typedef struct
{
    char eaddr[6];      /* 6 bytes of ethernet hardware address */
} uuid_address_t, *uuid_address_p_t;


typedef struct
{
    unsigned32  lo;
    unsigned32  hi;
} uuid_time_t, *uuid_time_p_t;


typedef struct
{
    unsigned32  lo;
    unsigned32  hi;
} unsigned64_t, *unsigned64_p_t;


/*
 * U U I D _ _ G E N E R A T E
 *
 * Call the native UUID generator.
 */
PRIVATE void uuid__generate(
        void *              /*uuidp*/
    );

#endif /* _UUIDP_H */

