/*
 * Copyright 1993, 1995 Christopher Seiwald.
 *
 * This file is part of Jam - see jam.c for Copyright information.
 */

/*
 * hash.h - simple in-memory hashing routines 
 */

typedef struct hashdata HASHDATA;

struct hash *	hashinit();
int		hashitem();
void		hashdone();

# define	hashenter( hp, data ) !hashitem( hp, data, !0 )
# define	hashcheck( hp, data ) hashitem( hp, data, 0 )

#ifdef APPLE_EXTENSIONS

typedef void hashiterfunc( struct hash *hp, const void *item, const void *contextinfo );

void hashiterate( struct hash *hp, hashiterfunc *iterfunc, void *contextinfo );

#endif
