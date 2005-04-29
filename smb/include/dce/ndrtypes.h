/*
 * 
 * (c) Copyright 1991 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1991 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1991 DIGITAL EQUIPMENT CORPORATION
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
/*
**  NAME:
**
**      ndrtypes.h
**
**  FACILITY:
**
**      IDL Stub Support Include File
**
**  ABSTRACT:
**
**  This file is new for DCE 1.1. This is a platform specific file that
**  defines the base level ndr types. This file is indirectly included 
**  in all files via the idlbase.h file. 
**
*/

/*
 * This particular file defines the NDR types for the BSD platform
 * based on what's in <machine/endian.h>. This file also depends on
 * the presence of a ANSI C compiler, in that it uses the signed
 * keyword to create the ndr_small_int type.
 */

#ifndef _NDR_TYPES_H 
#define  _NDR_TYPES_H

#include <machine/endian.h>

typedef unsigned char 		ndr_boolean ;
#define ndr_false       false
#define ndr_true        true
typedef unsigned char 		ndr_byte ;

typedef unsigned char 		ndr_char ;

typedef signed char 		ndr_small_int ;

typedef unsigned char 		ndr_usmall_int ;

typedef short int 		ndr_short_int ;

typedef unsigned short int	ndr_ushort_int ;

/*
 * No, not "long int" - a "long", in DCE RPC, is 32 bits, but an int
 * is 32 bits and a long might be 32 or 64 bits in our environment.
 */
typedef int	 		ndr_long_int ;

typedef unsigned int	 	ndr_ulong_int ;

#if !defined(BYTE_ORDER)
#error "Sorry, you haven't defined the byte order"
#elif BYTE_ORDER == BIG_ENDIAN
struct ndr_hyper_int_rep_s_t   {
    ndr_long_int high; 
    ndr_ulong_int low;
};
#elif BYTE_ORDER == LITTLE_ENDIAN
struct ndr_hyper_int_rep_s_t {
    ndr_ulong_int low; 
    ndr_long_int high;
};
#else
#error "Sorry, we don't support the PDP-11"
#endif

typedef struct ndr_hyper_int_rep_s_t ndr_hyper_int ;

#if !defined(BYTE_ORDER)
#error "Sorry, you haven't defined the byte order"
#elif BYTE_ORDER == BIG_ENDIAN
struct ndr_uhyper_int_rep_s_t  {
    ndr_ulong_int high; 
    ndr_ulong_int low;
};
#elif BYTE_ORDER == LITTLE_ENDIAN
struct ndr_uhyper_int_rep_s_t  {
    ndr_ulong_int low; 
    ndr_ulong_int high;
};
#else
#error "Sorry, we don't support the PDP-11"
#endif

typedef struct ndr_uhyper_int_rep_s_t 		ndr_uhyper_int ;

typedef float 		        ndr_short_float ;
typedef double 			ndr_long_float ;


#endif /* _NDR_TYPES_H */

