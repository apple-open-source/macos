/*
 * support.h - Useful definitions and macros. 
 *
 * Copyright (c) 2000-2004 Anton Altaparmakov
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_SUPPORT_H
#define _NTFS_SUPPORT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDDEF_H
#include <stddef.h>
#endif

/*
 * Our mailing list. Use this define to prevent typos in email address.
 */
#define NTFS_DEV_LIST	"ntfs-support@tuxera.com"

/*
 * Generic macro to convert pointers to values for comparison purposes.
 */
#ifndef p2n
#define p2n(p)		((ptrdiff_t)((ptrdiff_t*)(p)))
#endif

/*
 * The classic min and max macros.
 */
#ifndef min
#define min(a,b)	((a) <= (b) ? (a) : (b))
#endif

#ifndef max
#define max(a,b)	((a) >= (b) ? (a) : (b))
#endif

/*
 * Useful macro for determining the offset of a struct member.
 */
#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

/*
 * Simple bit operation macros. NOTE: These are NOT atomic.
 */
#define test_bit(bit, var)	      ((var) & (1 << (bit)))
#define set_bit(bit, var)	      (var) |= 1 << (bit)
#define clear_bit(bit, var)	      (var) &= ~(1 << (bit))

#define test_and_set_bit(bit, var)			\
({							\
	const BOOL old_state = test_bit(bit, var);	\
	set_bit(bit, var);				\
	old_state;					\
})

#define test_and_clear_bit(bit, var)			\
({							\
	const BOOL old_state = test_bit(bit, var);	\
	clear_bit(bit, var);				\
	old_state;					\
})

#endif /* defined _NTFS_SUPPORT_H */
