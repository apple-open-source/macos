/*
 * 
 * (c) Copyright 1989 OPEN SOFTWARE FOUNDATION, INC.
 * (c) Copyright 1989 HEWLETT-PACKARD COMPANY
 * (c) Copyright 1989 DIGITAL EQUIPMENT CORPORATION
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
**
**  NAME:
**
**      ndrold.h  
**
**  FACILITY:
**
**      Network Data Representation (NDR)
**
**  ABSTRACT:
**
**  This is a "hand-compiled" version of "ndrold.idl".  See the Abstract
**  in "ndrold.idl" for details.
**
**
*/

#ifndef ndrold_v0_included
#define ndrold_v0_included

/* 
 * Data representation descriptor type for NCS pre-v2.
 */
 
typedef struct {
    unsigned int int_rep: 4;
    unsigned int char_rep: 4;
    unsigned int float_rep: 8;
    unsigned int reserved: 16;
} ndr_old_format_t;

#endif

