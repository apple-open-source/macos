/* Block-related functions for the GNU debugger, GDB.

   Copyright 2003 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "block.h"
#include "symtab.h"
#include "symfile.h"
#include "gdb_obstack.h"
#include "cp-support.h"
/* APPLE LOCAL cache lookup values for improved performance  */
#include "inferior.h"

/* This is used by struct block to store namespace-related info for
   C++ files, namely using declarations and the current namespace in
   scope.  */

struct block_namespace_info
{
  const char *scope;
  struct using_direct *using;
};

static void block_initialize_namespace (struct block *block,
					struct obstack *obstack);

/* APPLE LOCAL begin address ranges  */
/* Return the highest address accounted for by BL's scope or one of 
   BL's ranges.  
   Are we guaranteed that the RANGE_END values increase?  i.e. can this
   be a simple returning of the last range entry's endaddr?  */

CORE_ADDR
block_highest_pc (const struct block *bl)
{
  int i;
  CORE_ADDR highest = 0;
  if (BLOCK_RANGES (bl) == NULL)
    return BLOCK_END (bl);

  for (i = 0; i < BLOCK_RANGES (bl)->nelts; i++)
    {
      if (highest < BLOCK_RANGE_END (bl, i))
        highest = BLOCK_RANGE_END (bl, i);
    }
  return highest;
}
/* APPLE LOCAL end address ranges  */

/* Return Nonzero if block a is lexically nested within block b,
   or if a and b have the same pc range.
   Return zero otherwise. */

int
contained_in (const struct block *a, const struct block *b)
{
  int i, j;
  if (!a || !b)
    return 0;

  /* APPLE LOCAL begin address ranges  */
  if (BLOCK_RANGES (a) == NULL
      && BLOCK_RANGES (b) == NULL)
    {
 /* APPLE LOCAL end address ranges  */

      return BLOCK_START (a) >= BLOCK_START (b)
	&& BLOCK_END (a) <= BLOCK_END (b);

 /* APPLE LOCAL begin address ranges  */
    }
  else if (!BLOCK_RANGES (a))
    {
      /* Block A has a single contiguous address range, but block B
	 has multiple non-contiguous ranges.  A is contained in B
         if A's address range fits within ANY of B's address ranges. */

      for (i = 0; i < BLOCK_RANGES (b)->nelts; i++)
	if (BLOCK_START (a) >= BLOCK_RANGE_START (b, i)
	    && BLOCK_END (a) <= BLOCK_RANGE_END (b, i))
	  {
	    return 1;  /* A's scope fits within one of B's ranges */
	  }
      return 0; /* A's scope did not fit within any of B's ranges */
    }
  else if (!BLOCK_RANGES (b))
    {
      /* Block B has a single contiguous address range, but block A
	 has multiple non-contiguous ranges.  A is contained in B if
         ALL of A's address ranges fit within B's address range.  */

      for (i = 0; i < BLOCK_RANGES (a)->nelts; i++)
	if (BLOCK_RANGE_START (a, i) < BLOCK_START (b)
	    || BLOCK_RANGE_END (a, i) > BLOCK_END (b))
          {
	    return 0;  /* One of A's ranges is outside B's scope */
          }
      return 1; /* All of A's ranges are within B's scope */
    }
  else
    {
      /* Both block A and block B have non-contiguous address ranges.  
         A is contained in B if all of A's address ranges fit within at
         least one of B's address ranges.  */

      int fits;
      for (i = 0; i < BLOCK_RANGES (a)->nelts; i++)
	{
	  fits = 0;
	  for (j = 0; j < BLOCK_RANGES (b)->nelts && !fits; j++)
	    if (BLOCK_RANGE_START (a, i) >= BLOCK_RANGE_START (b, j)
		&& BLOCK_RANGE_END (a, i) <= BLOCK_RANGE_END (b, j))
              {
	        fits = 1;
              }
          if (fits == 0)
            {
              /* One of A's ranges is is not contained within any B range */ 
              return 0; 
            }
	}
      return 1;  /* All of A's ranges are contained within B's ranges */
    }
  /* APPLE LOCAL end address ranges  */
  return 0; /* notreached */
}


/* Return the symbol for the function which contains a specified
   lexical block, described by a struct block BL.  */

struct symbol *
block_function (const struct block *bl)
{
  while (BLOCK_FUNCTION (bl) == 0 && BLOCK_SUPERBLOCK (bl) != 0)
    bl = BLOCK_SUPERBLOCK (bl);

  return BLOCK_FUNCTION (bl);
}

/* APPLE LOCAL begin address ranges  */
/* Return a 1 if any of the address ranges for block BL begins with START
   and any of the address ranges for BL ends with END; return a 0 otherwise.  */
int
block_starts_and_ends (struct block *bl, CORE_ADDR start, CORE_ADDR end)
{
  int retval;
  int start_found = 0;
  int end_found = 0;

  if (!BLOCK_RANGES (bl))
    retval = BLOCK_START (bl) == start && BLOCK_END (bl) == end;
  else
    {
      int i;
      for (i = 0; 
           i < BLOCK_RANGES (bl)->nelts && !start_found && !end_found; 
           i++)
	{
	  if (BLOCK_RANGE_START (bl, i) == start)
	    start_found = 1;
	  if (BLOCK_RANGE_END (bl, i) == end)
	    end_found = 1;
	}
      retval = start_found && end_found;
    }
  
  return retval;
}
/* APPLE LOCAL end address ranges  */

/* APPLE LOCAL begin address ranges  */
/* Return a 1 if PC is contained within any of the address ranges in BL;
   otherwise return a 0.  */

int
block_contains_pc (const struct block *bl, CORE_ADDR pc)
{
  int i;
  int contains_pc = 0;

  if (! BLOCK_RANGES (bl))
    /* No range list; just a low & high address  */
    contains_pc = BLOCK_START (bl) <= pc && BLOCK_END (bl) > pc;
  else
    for (i = 0; i < BLOCK_RANGES (bl)->nelts && !contains_pc; i++)
      if (BLOCK_RANGE_START (bl, i) <= pc && BLOCK_RANGE_END (bl, i) > pc)
	contains_pc = 1;

  return contains_pc;
}
/* APPLE LOCAL end address ranges  */

/* Return the blockvector immediately containing the innermost lexical block
   containing the specified pc value and section, or 0 if there is none.
   PINDEX is a pointer to the index value of the block.  If PINDEX
   is NULL, we don't pass this information back to the caller.  */

struct blockvector *
blockvector_for_pc_sect (CORE_ADDR pc, struct bfd_section *section,
			 int *pindex, struct symtab *symtab)
{
  struct block *b;
  struct block *static_block;
  int bot, top, half;
  struct blockvector *bl;

  if (pindex)
    *pindex = 0;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_blockvector_lookup_pc
	   && pc == last_mapped_section_lookup_pc
	   && section == cached_mapped_section
	   && cached_blockvector)
    {
      *pindex = cached_blockvector_index;
      return cached_blockvector;
    }

  last_blockvector_lookup_pc = pc;
  /* APPLE LOCAL end cache lookup values for improved performance  */

  if (symtab == 0)		/* if no symtab specified by caller */
    {
      /* First search all symtabs for one whose file contains our pc */
      symtab = find_pc_sect_symtab (pc, section);
      if (symtab == 0)
	/* APPLE LOCAL begin cache lookup values for improved performance  */
	{
	  cached_blockvector_index = -1;
	  cached_blockvector = NULL;
	  return 0;
	}
        /* APPLE LOCAL end cache lookup values for improved performance  */
    }

  bl = BLOCKVECTOR (symtab);
  static_block = BLOCKVECTOR_BLOCK (bl, STATIC_BLOCK);
  b = BLOCKVECTOR_BLOCK (bl, 0);

  /* Then search that symtab for the smallest block that wins.  */
  /* Use binary search to find the last block that starts before PC.  */

  bot = 0;
  top = BLOCKVECTOR_NBLOCKS (bl);

  while (top - bot > 1)
    {
      half = (top - bot + 1) >> 1;
      b = BLOCKVECTOR_BLOCK (bl, bot + half);
      /* APPLE LOCAL begin address ranges  */
      if (BLOCK_LOWEST_PC (b) <= pc)
      /* APPLE LOCAL end address ranges  */
	bot += half;
      else
	top = bot + half;
    }

  /* APPLE LOCAL We start with the block whose start/end address
     is higher than PC.  */

  /* Now search backward for a block that ends after PC.  */

  /* APPLE LOCAL: Stop at the first local block; i.e. don't iterate down
     to the global/static blocks.  */

  while (bot >= FIRST_LOCAL_BLOCK)
    {
      b = BLOCKVECTOR_BLOCK (bl, bot);
      /* APPLE LOCAL begin address ranges  */

      /* This condition is a little tricky.
         Given a function like
            func () { 
               { subblock}
                // pc here
            }
	 BOT may be pointing to "subblock" and so the BOT block
	 start/end addrs are less than PC.  But we don't want to
	 terminate the search in this case - we need to keep iterating
         backwards to find "func"'s block.  
         So I'm trying to restrict this to only quit searching if
         we're looking at a function's overall scope and both its
         highest/lowest addresses are lower than PC.  */
      if (BLOCK_SUPERBLOCK (b) == static_block 
          && BLOCK_LOWEST_PC (b) < pc && BLOCK_HIGHEST_PC (b) < pc)
	/* APPLE LOCAL begin cache lookup values for improved performance  */
	{
	  cached_blockvector_index = -1;
	  cached_blockvector = NULL;
	  return 0;
	}
        /* APPLE LOCAL end cache lookup values for improved performance  */

      if (block_contains_pc (b, pc))
      /* APPLE LOCAL end address ranges  */
	{
	  if (pindex)
	    *pindex = bot;
	  /* APPLE LOCAL begom cache lookup values for improved 
	     performance  */	
	  cached_blockvector_index = bot;
	  cached_blockvector = bl;
	  /* APPLE LOCAL end cache lookup values for improved performance  */
	  return bl;
	}
      bot--;
    }
  /* APPLE LOCAL begin cache lookup values for improved performance  */
  cached_blockvector_index = -1;
  cached_blockvector = NULL;
  /* APPLE LOCAL end cache lookup values for improved performance  */
  return 0;
}

/* Return the blockvector immediately containing the innermost lexical block
   containing the specified pc value, or 0 if there is none.
   Backward compatibility, no section.  */

struct blockvector *
blockvector_for_pc (CORE_ADDR pc, int *pindex)
{
  return blockvector_for_pc_sect (pc, find_pc_mapped_section (pc),
				  pindex, NULL);
}

/* Return the innermost lexical block containing the specified pc value
   in the specified section, or 0 if there is none.  */

struct block *
block_for_pc_sect (CORE_ADDR pc, struct bfd_section *section)
{
  struct blockvector *bl;
  int index;

  /* APPLE LOCAL begin cache lookup values for improved performance  */
  if (pc == last_block_lookup_pc
	   && pc == last_mapped_section_lookup_pc
	   && section == cached_mapped_section
	   && cached_block)
    return cached_block;

  last_block_lookup_pc = pc;

  bl = blockvector_for_pc_sect (pc, section, &index, NULL);
  if (bl)
    {
      cached_block = BLOCKVECTOR_BLOCK (bl, index);
      return BLOCKVECTOR_BLOCK (bl, index);
    }
  cached_block = NULL;
  return 0;
  /* APPLE LOCAL end cache lookup values for improved performance  */
}

/* Return the innermost lexical block containing the specified pc value,
   or 0 if there is none.  Backward compatibility, no section.  */

struct block *
block_for_pc (CORE_ADDR pc)
{
  return block_for_pc_sect (pc, find_pc_mapped_section (pc));
}

/* Now come some functions designed to deal with C++ namespace issues.
   The accessors are safe to use even in the non-C++ case.  */

/* This returns the namespace that BLOCK is enclosed in, or "" if it
   isn't enclosed in a namespace at all.  This travels the chain of
   superblocks looking for a scope, if necessary.  */

const char *
block_scope (const struct block *block)
{
  for (; block != NULL; block = BLOCK_SUPERBLOCK (block))
    {
      if (BLOCK_NAMESPACE (block) != NULL
	  && BLOCK_NAMESPACE (block)->scope != NULL)
	return BLOCK_NAMESPACE (block)->scope;
    }

  return "";
}

/* Set BLOCK's scope member to SCOPE; if needed, allocate memory via
   OBSTACK.  (It won't make a copy of SCOPE, however, so that already
   has to be allocated correctly.)  */

void
block_set_scope (struct block *block, const char *scope,
		 struct obstack *obstack)
{
  block_initialize_namespace (block, obstack);

  BLOCK_NAMESPACE (block)->scope = scope;
}

/* This returns the first using directives associated to BLOCK, if
   any.  */

/* FIXME: carlton/2003-04-23: This uses the fact that we currently
   only have using directives in static blocks, because we only
   generate using directives from anonymous namespaces.  Eventually,
   when we support using directives everywhere, we'll want to replace
   this by some iterator functions.  */

struct using_direct *
block_using (const struct block *block)
{
  const struct block *static_block = block_static_block (block);

  if (static_block == NULL
      || BLOCK_NAMESPACE (static_block) == NULL)
    return NULL;
  else
    return BLOCK_NAMESPACE (static_block)->using;
}

/* Set BLOCK's using member to USING; if needed, allocate memory via
   OBSTACK.  (It won't make a copy of USING, however, so that already
   has to be allocated correctly.)  */

void
block_set_using (struct block *block,
		 struct using_direct *using,
		 struct obstack *obstack)
{
  block_initialize_namespace (block, obstack);

  BLOCK_NAMESPACE (block)->using = using;
}

/* If BLOCK_NAMESPACE (block) is NULL, allocate it via OBSTACK and
   ititialize its members to zero.  */

static void
block_initialize_namespace (struct block *block, struct obstack *obstack)
{
  if (BLOCK_NAMESPACE (block) == NULL)
    {
      BLOCK_NAMESPACE (block)
	= obstack_alloc (obstack, sizeof (struct block_namespace_info));
      BLOCK_NAMESPACE (block)->scope = NULL;
      BLOCK_NAMESPACE (block)->using = NULL;
    }
}

/* Return the static block associated to BLOCK.  Return NULL if block
   is NULL or if block is a global block.  */

const struct block *
block_static_block (const struct block *block)
{
  if (block == NULL || BLOCK_SUPERBLOCK (block) == NULL)
    return NULL;

  while (BLOCK_SUPERBLOCK (BLOCK_SUPERBLOCK (block)) != NULL)
    block = BLOCK_SUPERBLOCK (block);

  return block;
}

/* Return the static block associated to BLOCK.  Return NULL if block
   is NULL.  */

const struct block *
block_global_block (const struct block *block)
{
  if (block == NULL)
    return NULL;

  while (BLOCK_SUPERBLOCK (block) != NULL)
    block = BLOCK_SUPERBLOCK (block);

  return block;
}

/* Allocate a block on OBSTACK, and initialize its elements to
   zero/NULL.  This is useful for creating "dummy" blocks that don't
   correspond to actual source files.

   Warning: it sets the block's BLOCK_DICT to NULL, which isn't a
   valid value.  If you really don't want the block to have a
   dictionary, then you should subsequently set its BLOCK_DICT to
   dict_create_linear (obstack, NULL).  */

struct block *
allocate_block (struct obstack *obstack)
{
  struct block *bl = obstack_alloc (obstack, sizeof (struct block));

  BLOCK_START (bl) = 0;
  BLOCK_END (bl) = 0;
  BLOCK_FUNCTION (bl) = NULL;
  BLOCK_SUPERBLOCK (bl) = NULL;
  BLOCK_DICT (bl) = NULL;
  BLOCK_NAMESPACE (bl) = NULL;
  BLOCK_GCC_COMPILED (bl) = 0;
  /* APPLE LOCAL begin address ranges  */
  BLOCK_RANGES (bl) = NULL;
  /* APPLE LOCAL end address ranges  */

  return bl;
}
