/*
 * (c) Copyright 1992 by Panagiotis Tsirigotis
 * (c) Sections Copyright 1998-2001 by Rob Braun
 * All rights reserved.  The file named COPYRIGHT specifies the terms 
 * and conditions for redistribution.
 */

#ifndef MASK_H
#define MASK_H

/*
 * $Id: mask.h,v 1.1.1.2 2002/10/02 21:07:26 bbraun Exp $
 */

#include "config.h"
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

/*
 * Macros about masks - Note: 64 bits is needed because of attr.h
 */

typedef uint64_t mask_t;

#define MASK_NULL                   ((mask_t *)0)

#define XMASK( v )                  ( (mask_t)1 << ( (v)-1 ) )

#define M_CLEAR_ALL( mask )         (mask) = 0
#define M_ASSIGN( mask1, mask2 )    (mask1) = (mask2)
#define M_ARE_ALL_CLEAR( mask )     ( (mask) == 0 )
#define M_SET( mask, v )            (mask) |= XMASK(v)
#define M_CLEAR( mask, v )          (mask) &= ~XMASK(v)
#define M_IS_SET( mask, v )         ( (mask) & XMASK(v) )
#define M_IS_CLEAR( mask, v )       ( ! M_IS_SET( mask, v ) )

#define M_AND( mres, m1, m2 )       ( (mres) = (m1) & (m2) )
#define M_OR( mres, m1, m2 )        ( (mres) = (m1) | (m2) )
#define M_XOR( mres, m1, m2 )       ( (mres) = (m1) ^ (m2) )

#endif   /* MASK_H */
