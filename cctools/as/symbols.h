/* symbols.h -
   Copyright (C) 1987 Free Software Foundation, Inc.

This file is part of GAS, the GNU Assembler.

GAS is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GAS is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#import "struc-symbol.h"
#import "hash.h"

extern struct hash_control *sy_hash;
extern struct obstack notes;
extern symbolS *symbol_rootP;
extern symbolS *symbol_lastP;
extern symbolS abs_symbol;

extern void symbol_begin(
    void);
extern char *local_label_name(
    int n,
    int augend);
extern void
    local_colon(
    int n);
extern symbolS *symbol_new(
    char *name,
    unsigned char type,
    char other,
    short desc,
    valueT value,
    struct frag *frag);
extern void colon(
    char *sym_name);
extern void symbol_table_insert(
    struct symbol *symbolP);
extern symbolS *symbol_find_or_make(
    char *name);
extern symbolS *
    symbol_find(
    char *name);
extern symbolS *
    symbol_table_lookup(
    char *name);
extern isymbolS *indirect_symbol_new(
    char *name,
    struct frag *frag,
    unsigned long offset);
