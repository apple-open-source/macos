/* Code dealing with blocks for GDB.

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

#ifndef BLOCK_H
#define BLOCK_H

/* Opaque declarations.  */

struct symbol;
struct symtab;
struct block_namespace_info;
struct using_direct;
struct obstack;
struct dictionary;

/* APPLE LOCAL begin address ranges  */
struct address_range
{
  CORE_ADDR startaddr;
  CORE_ADDR endaddr;
};

struct address_range_list
{
  int nelts;
  struct address_range *ranges;
};
/* APPLE LOCAL end address ranges  */

/* All of the name-scope contours of the program
   are represented by `struct block' objects.
   All of these objects are pointed to by the blockvector.

   Each block represents one name scope.
   Each lexical context has its own block.

   The blockvector begins with some special blocks.
   The GLOBAL_BLOCK contains all the symbols defined in this compilation
   whose scope is the entire program linked together.
   The STATIC_BLOCK contains all the symbols whose scope is the
   entire compilation excluding other separate compilations.
   Blocks starting with the FIRST_LOCAL_BLOCK are not special.

   Each block records a range of core addresses for the code that
   is in the scope of the block.  The STATIC_BLOCK and GLOBAL_BLOCK
   give, for the range of code, the entire range of code produced
   by the compilation that the symbol segment belongs to.

   The blocks appear in the blockvector
   in order of increasing starting-address,
   and, within that, in order of decreasing ending-address.

   This implies that within the body of one function
   the blocks appear in the order of a depth-first tree walk.  */

struct block
{

  /* Addresses in the executable code that are in this block.  */

  CORE_ADDR startaddr;
  CORE_ADDR endaddr;

  /* APPLE LOCAL begin address ranges  */
  /* Address ranges in the executable code that are in this block, if
     the block has non-contiguous address ranges in the executable code.
     If the block uses address ranges, then the startaddr and endaddr
     fields (above) should be 0 and -1, respectively.  */

  struct address_range_list *ranges;
  /* APPLE LOCAL end address ranges  */

  /* The symbol that names this block, if the block is the body of a
     function; otherwise, zero.  */

  struct symbol *function;

  /* The `struct block' for the containing block, or 0 if none.

     The superblock of a top-level local block (i.e. a function in the
     case of C) is the STATIC_BLOCK.  The superblock of the
     STATIC_BLOCK is the GLOBAL_BLOCK.  */

  struct block *superblock;

  /* This is used to store the symbols in the block.  */

  struct dictionary *dict;

  /* Used for language-specific info.  */

  union
  {
    struct
    {
      /* Contains information about namespace-related info relevant to
	 this block: using directives and the current namespace
	 scope.  */
      
      struct block_namespace_info *namespace;
    }
    cplus_specific;
  }
  language_specific;

  /* Version of GCC used to compile the function corresponding
     to this block, or 0 if not compiled with GCC.  When possible,
     GCC should be compatible with the native compiler, or if that
     is not feasible, the differences should be fixed during symbol
     reading.  As of 16 Apr 93, this flag is never used to distinguish
     between gcc2 and the native compiler.

     If there is no function corresponding to this block, this meaning
     of this flag is undefined.  */

  unsigned char gcc_compile_flag;
};

#define BLOCK_START(bl)		(bl)->startaddr
#define BLOCK_END(bl)		(bl)->endaddr
#define BLOCK_FUNCTION(bl)	(bl)->function
#define BLOCK_SUPERBLOCK(bl)	(bl)->superblock
#define BLOCK_GCC_COMPILED(bl)	(bl)->gcc_compile_flag
#define BLOCK_DICT(bl)		(bl)->dict
#define BLOCK_NAMESPACE(bl)   (bl)->language_specific.cplus_specific.namespace
/* APPLE LOCAL begin address ranges  */
#define BLOCK_RANGES(bl)        (bl)->ranges
#define BLOCK_RANGE_START(bl,i)  (bl)->ranges->ranges[i].startaddr
#define BLOCK_RANGE_END(bl,i)  (bl)->ranges->ranges[i].endaddr
#define BLOCK_LOWEST_PC(bl) \
  ( BLOCK_RANGES (bl) ? BLOCK_RANGE_START (bl, 0) : BLOCK_START (bl))
#define BLOCK_HIGHEST_PC(bl)  block_highest_pc ((bl))
/* APPLE LOCAL end address ranges  */

/* Macro to loop through all symbols in a block BL, in no particular
   order.  ITER helps keep track of the iteration, and should be a
   struct dict_iterator.  SYM points to the current symbol.  */

#define ALL_BLOCK_SYMBOLS(block, iter, sym)			\
	ALL_DICT_SYMBOLS (BLOCK_DICT (block), iter, sym)

struct blockvector
{
  /* Number of blocks in the list.  */
  int nblocks;
  /* The blocks themselves.  */
  struct block *block[1];
};

#define BLOCKVECTOR_NBLOCKS(blocklist) (blocklist)->nblocks
#define BLOCKVECTOR_BLOCK(blocklist,n) (blocklist)->block[n]

/* Special block numbers */

enum { GLOBAL_BLOCK = 0, STATIC_BLOCK = 1, FIRST_LOCAL_BLOCK = 2 };

extern struct symbol *block_function (const struct block *);

extern int contained_in (const struct block *, const struct block *);

extern struct blockvector *blockvector_for_pc (CORE_ADDR, int *);

extern struct blockvector *blockvector_for_pc_sect (CORE_ADDR, asection *,
						    int *, struct symtab *);

extern struct block *block_for_pc (CORE_ADDR);

extern struct block *block_for_pc_sect (CORE_ADDR, asection *);

extern const char *block_scope (const struct block *block);

extern void block_set_scope (struct block *block, const char *scope,
			     struct obstack *obstack);

extern struct using_direct *block_using (const struct block *block);

extern void block_set_using (struct block *block,
			     struct using_direct *using,
			     struct obstack *obstack);

extern const struct block *block_static_block (const struct block *block);

extern const struct block *block_global_block (const struct block *block);

extern struct block *allocate_block (struct obstack *obstack);

/* APPLE LOCAL begin address ranges  */
extern int block_contains_pc (const struct block *, CORE_ADDR);

extern int block_starts_and_ends (struct block *, CORE_ADDR, CORE_ADDR);

extern CORE_ADDR block_highest_pc (const struct block *bl);
/* APPLE LOCAL end address ranges  */

#endif /* BLOCK_H */
