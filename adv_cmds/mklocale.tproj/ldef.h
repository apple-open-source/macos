/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Paul Borman at Krystal Technologies.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *
 *	@(#)ldef.h	8.1 (Berkeley) 6/6/93
 */

/*
 * This should look a LOT like a _RuneEntry
 */
typedef struct rune_list {
    rune_t		min;
    rune_t 		max;
    rune_t 		map;
    u_long		*types;
    struct rune_list	*next;
} rune_list;

typedef struct rune_map {
    u_long		map[_CACHED_RUNES];
    rune_list		*root;
} rune_map;
