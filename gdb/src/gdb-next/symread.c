/* SYM symbol-table support for Mach, for GDB.
   Copyright 1997  Free Software Foundation, Inc.
   Written by Klee Dienes.  Contributed by Apple Computer, Inc.

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "breakpoint.h"
#include "bfd.h"
#include "symfile.h"
#include "objfiles.h"
#include "buildsym.h"
#include "obstack.h"
#include "gdb-stabs.h"
#include "stabsread.h"
#include "gdbcmd.h"
#include "complaints.h"

#include "sym.h"

#include <string.h>
#include <stdio.h>

#ifndef __i386__

struct type *sym_builtin_type_pstr;
struct type *sym_builtin_type_cstr;

struct complaint sym_complaint = 
  {"error parsing SYM-format symbol table", 0, 0};

struct complaint sym_unknown_typecode_complaint = 
  {"unknown SYM typecode %d", 0, 0};

struct sym_symfile_info {
  asymbol **syms;
  long nsyms;
};

static void
sym_new_init (struct objfile *objfile)
{
}

static struct type *sym_lookup_builtin_type (unsigned int num)
{
  switch (num) {
  case 0: return builtin_type_void;
  case 1: return sym_builtin_type_pstr;
  case 2: return builtin_type_unsigned_long;
  case 3: return builtin_type_long;
  case 4: return builtin_type_error;
  case 5: return builtin_type_bool;
  case 6: return builtin_type_uint8;
  case 7: return builtin_type_int8;
  case 8: return builtin_type_char;
  case 9: return builtin_type_short;
  case 10: return builtin_type_unsigned_short;
  case 11: return builtin_type_short;
  case 12: return builtin_type_error;
  case 13: return builtin_type_error;
  case 14: return builtin_type_error;
  case 15: return builtin_type_error;
  case 16: return sym_builtin_type_cstr;
  case 17: return builtin_type_error;
  default:
    return NULL;
  }
}

int sym_parse_type
(struct objfile *objfile, struct type **typevec, size_t ntypes, 
 unsigned char *buf, size_t len, size_t offset, size_t *offsetptr,
 struct type **tptr, char **nptr, unsigned long *vptr)
{
  unsigned int typecode = 0;
  struct type *type = NULL;
  char *typename = NULL;
  struct type *target = NULL;
  char *targname = NULL;
  unsigned long value = 0;
  int retval = 0;
  int ret;

  if (offset >= len) {
    type = builtin_type_error;
    retval = -1;
    goto end;
  }
  
  typecode = buf[offset];
  offset++;

  if (! (typecode & 0x80)) {
    type = sym_lookup_builtin_type (typecode);
    goto end;
  }
  
  switch (typecode & 0x3f) {

  case 1: {

    long value;

    ret = bfd_sym_fetch_long (buf, len, offset, &offset, &value); 
    if (ret < 0) { complain (&sym_complaint); break; }
    if (value < 0) { complain (&sym_complaint); break; } 
    if (value >= ntypes) { complain (&sym_complaint); break; }
    if (typevec[value] == NULL) { complain (&sym_complaint); break; }
    type = typevec[value]; 

    break;
  }

  case 2: {
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &target, NULL, NULL);
    if ((ret < 0) || (target == NULL)) { complain (&sym_complaint); break; }
    if (target == NULL) { complain (&sym_complaint); break; }
    type = make_pointer_type (target, NULL);
    break;
  }

  case 3: {
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &type, NULL, NULL);
    if ((ret < 0) || (type == NULL)) { complain (&sym_complaint); break; }
    ret = bfd_sym_fetch_long (buf, len, offset, &offset, &value); 
    if (ret < 0) { complain (&sym_complaint); break; }
    break;
  }

  case 5: {

    long lower, upper, nelem;
    unsigned long i;

    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &target, NULL, NULL);
    bfd_sym_fetch_long (buf, len, offset, &offset, &lower); 
    bfd_sym_fetch_long (buf, len, offset, &offset, &upper); 
    bfd_sym_fetch_long (buf, len, offset, &offset, &nelem); 

    type = alloc_type (objfile);

    TYPE_LENGTH (type) = TARGET_INT_BIT / HOST_CHAR_BIT;
    TYPE_CODE (type) = TYPE_CODE_ENUM;
    TYPE_NFIELDS (type) = nelem;
    TYPE_FIELDS (type) = (struct field *) TYPE_ALLOC (type, sizeof (struct field) * nelem);
    memset (TYPE_FIELDS (type), 0, sizeof (struct field) * nelem);

    for (i = 0; i < nelem; i++) {

      ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &target, &targname, &value);
      if ((ret < 0) || (target == NULL)) { complain (&sym_complaint); break; }

      if (targname == NULL) { complain (&sym_complaint); targname = "";  }
      
      TYPE_FIELD_TYPE (type, i) = target;
      TYPE_FIELD_NAME (type, i) = targname;
      TYPE_FIELD_BITPOS (type, i) = value;
      TYPE_FIELD_BITSIZE (type, i) = 0;
    }

    break;
  }

  case 6: {
    struct type *index;
    
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &index, NULL, NULL);
    if ((ret < 0) || (index == NULL)) { complain (&sym_complaint); break; }
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &target, NULL, NULL);
    if ((ret < 0) || (target == NULL)) { complain (&sym_complaint); break; }
    
    type = create_array_type (NULL, target, index);
    break;
  }

  case 7:
  case 8: {

    long nrec, eloff, i;

    ret = bfd_sym_fetch_long (buf, len, offset, &offset, &nrec); 
    if (ret < 0) { complain (&sym_complaint); break; }
    if (nrec < 0) { complain (&sym_complaint); break; }

    type = alloc_type (objfile);

    if ((typecode & 0x3f) == 7) {
      TYPE_CODE (type) = TYPE_CODE_STRUCT;
    } else {
      TYPE_CODE (type) = TYPE_CODE_UNION;
    }

    INIT_CPLUS_SPECIFIC (type);

    TYPE_FIELDS (type) = (struct field *) TYPE_ALLOC (type, nrec * sizeof (struct field));
    TYPE_NFIELDS (type) = nrec;
    TYPE_LENGTH (type) = 0;

    for (i = 0; i < nrec; i++) {
      ret = bfd_sym_fetch_long (buf, len, offset, &offset, &eloff); 
      if (ret < 0) { complain (&sym_complaint); break; }
      ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &target, &targname, NULL);
      if ((ret < 0) || (target == NULL)) { complain (&sym_complaint); break; }

      if ((eloff < 0) || (targname == NULL)) {
	type = NULL; 
	break;
      }

      TYPE_FIELD_TYPE (type, i) = target;
      TYPE_FIELD_NAME (type, i) = targname;
      TYPE_FIELD_BITPOS (type, i) = TARGET_CHAR_BIT * eloff;
      TYPE_FIELD_BITSIZE (type, i) = 0;
      TYPE_LENGTH (type) = eloff + TYPE_LENGTH (target);
    }
    
    break;
  }

  case 9: {
    struct type *lower, *upper;
    unsigned long lval, uval;

    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &target, NULL, NULL);
    if ((ret < 0) || (target == NULL)) { complain (&sym_complaint); break; }
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &lower, NULL, &lval);
    if ((ret < 0) || (lower == NULL)) { complain (&sym_complaint); break; }
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &upper, NULL, &uval);
    if ((ret < 0) || (upper == NULL)) { complain (&sym_complaint); break; }
    
    type = create_range_type (NULL, target, lval, uval);

    break;
  }

  case 11: {
    
    long value;
    const unsigned char *name;

    ret = bfd_sym_fetch_long (buf, len, offset, &offset, &value); 
    if (ret < 0) { complain (&sym_complaint); break; }
    ret = sym_parse_type (objfile, typevec, ntypes, buf, len, offset, &offset, &type, NULL, NULL);
    if ((ret < 0) || (type == NULL)) { complain (&sym_complaint); break; }

    name = bfd_sym_symbol_name (objfile->obfd, value);
    typename = (char *) obstack_alloc (&objfile->symbol_obstack, name[0] + 1);

    sprintf (typename, "%.*s", name[0], name + 1);
    break;
  }

  default:
    complain (&sym_unknown_typecode_complaint, typecode);
    type = builtin_type_error;
    break;
  }

  if (typecode == (0x40 | 0x6)) {
    /* vector */
    long n, width, m;
    long l;
    long i;
    bfd_sym_fetch_long (buf, len, offset, &offset, &n); 
    bfd_sym_fetch_long (buf, len, offset, &offset, &width); 
    bfd_sym_fetch_long (buf, len, offset, &offset, &m); 
    for (i = 0; i < m; i++) {
      bfd_sym_fetch_long (buf, len, offset, &offset, &l); 
    }
  } else if (typecode & 0x40) {
    /* other packed type */
    long msb, lsb;
    bfd_sym_fetch_long (buf, len, offset, &offset, &msb); 
    bfd_sym_fetch_long (buf, len, offset, &offset, &lsb); 
  }

 end:
  if (offsetptr != NULL) { *offsetptr = offset; }
  if (tptr != NULL) { *tptr = type; } 
  if (nptr != NULL) { *nptr = typename; } 
  if (vptr != NULL) { *vptr = value; } 
  return 0;
}

static void sym_read_type
(struct objfile *objfile,
 struct type **typevec, unsigned long ntypes,
 struct type *itype, struct symbol **psymbol,
 unsigned long i)
{
  bfd *abfd = NULL;
  bfd_sym_data_struct *sdata = NULL;

  bfd_sym_type_table_entry index;
  bfd_sym_type_information_table_entry entry;
  unsigned char buf[4096];

  struct type *type = NULL;
  struct symbol *symbol = NULL;

  const unsigned char *typename = NULL;
  char *ntypename = NULL;

  int ret = 0;

  CHECK_FATAL (objfile != NULL);
  abfd = objfile->obfd;
  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  CHECK_FATAL (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;
      
  /* *ptype = NULL; */
  *psymbol = NULL;

  if (i <= 99) {
    struct type *builtin = sym_lookup_builtin_type (i);
    if (builtin != NULL) {
      *itype = *builtin;
    }
    return;
  }

  if (bfd_sym_fetch_type_table_entry (abfd, &index, i - 100) < 0) { return; }
  if (bfd_sym_fetch_type_information_table_entry (abfd, &entry, index) < 0) { return; }

  if (entry.physical_size > 4096) { return; }

  if (bfd_seek (abfd, entry.offset, SEEK_SET) < 0) { return; }

  if (bfd_read (buf, 1, entry.physical_size, abfd) != entry.physical_size) { return; }

  typename = bfd_sym_symbol_name (objfile->obfd, entry.nte_index);

  if (typename[0] > 0) {
    ntypename = (char *) obstack_alloc (&objfile->symbol_obstack, typename[0] + 1);
    sprintf (ntypename, "%.*s", typename[0], typename + 1);
  } else {
    if (0) {
      ntypename = (char *) obstack_alloc (&objfile->symbol_obstack, 16);
      sprintf (ntypename, "type%lu", i);
    }
  }
      
  ret = sym_parse_type (objfile, typevec, ntypes, buf, entry.physical_size, 0, NULL, &type, NULL, NULL);
  if ((ret != 0) || (type == NULL)) { return; }
      
  if (ntypename != NULL ) {

    if ((TYPE_CODE (type) == TYPE_CODE_STRUCT) || (TYPE_CODE (type) == TYPE_CODE_UNION)) {
      TYPE_TAG_NAME (type) = ntypename;
    }

    symbol = (struct symbol *) obstack_alloc (&objfile->symbol_obstack, sizeof (struct symbol));
      
    SYMBOL_TYPE (symbol) = type;
    SYMBOL_NAME (symbol) = ntypename;
    SYMBOL_VALUE (symbol) = 0;
    SYMBOL_LANGUAGE (symbol) = 0;
    SYMBOL_SECTION (symbol) = 0;
    SYMBOL_BFD_SECTION (symbol) = 0;
    SYMBOL_NAMESPACE (symbol) = VAR_NAMESPACE;
    SYMBOL_CLASS (symbol) = LOC_TYPEDEF;
    SYMBOL_LINE (symbol) = 0;
    SYMBOL_BASEREG (symbol) = 0;
    SYMBOL_ALIASES (symbol) = NULL; 
    SYMBOL_RANGES (symbol) = NULL;
  }

  *itype = *type;
  *psymbol = symbol;
}

static void sym_read_types
(struct objfile *objfile, 
 unsigned long *pmaxtypes,
 struct type ***ptypevec, unsigned long *pntypes,
 struct symbol ***ptypedefvec, unsigned long *pntypedefs) 
{
  bfd *abfd = NULL;
  bfd_sym_data_struct *sdata = NULL;

  struct type **typevec = NULL;
  struct symbol **typedefvec = NULL;
  unsigned long ntypes = 0;
  unsigned long ntypedefs = 0;

  unsigned long maxtypes = 0;
  unsigned long i;

  CHECK_FATAL (objfile != NULL);
  abfd = objfile->obfd;
  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  CHECK_FATAL (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  maxtypes = sdata->header.dshb_tte.dti_object_count + 1;

  typevec = obstack_alloc (&objfile->type_obstack, maxtypes * sizeof (struct type *));
  typedefvec = obstack_alloc (&objfile->type_obstack, maxtypes * sizeof (struct symbol *));

  for (i = 0; i < maxtypes; i++) {
    typevec[i] = alloc_type (objfile);
    typedefvec[i] = 0;
  }
  
  for (i = 0; i < maxtypes; i++) {
    sym_read_type (objfile, typevec, maxtypes, typevec[i], &typedefvec[i], i);
    ntypes++;
    if (typedefvec[i] != NULL) {
      ntypedefs++;
    }
  }

  *ptypevec = typevec;
  *ptypedefvec = typedefvec;

  *pmaxtypes = maxtypes;
  *pntypes = ntypes;
  *pntypedefs = ntypedefs;
}

static void sym_read_contained_variables
(struct objfile *objfile, 
 struct bfd_sym_modules_table_entry *entry,
 struct type **typevec, unsigned long ntypes,
 struct field *argvec, unsigned long *pnargs,
 struct symbol **localvec, unsigned long *pnlocals,
 struct symbol **globalvec, unsigned long *pnglobals)
{
  bfd *abfd = NULL;
  bfd_sym_data_struct *sdata = NULL;

  bfd_sym_contained_variables_table_entry cventry;
  unsigned long nargs = 0;
  unsigned long nlocals = 0;
  unsigned long nglobals = 0;

  int ret = 0;
  unsigned long i = 0;
  
  CHECK_FATAL (objfile != NULL);
  abfd = objfile->obfd;
  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  CHECK_FATAL (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  *pnargs = 0;
  *pnlocals = 0;
  *pnglobals = 0;

  i = entry->mte_cvte_index;

  if (i == 0) { return; }
  if (i > sdata->header.dshb_cvte.dti_object_count) { complain (&sym_complaint); goto end; }
  ret = bfd_sym_fetch_contained_variables_table_entry (abfd, &cventry, i);
  if (ret < 0) { complain (&sym_complaint); goto end; }
  if (cventry.generic.type != BFD_SYM_SOURCE_FILE_CHANGE) { complain (&sym_complaint); goto end; }
  i++;
      
  for (;;) {

    const unsigned char *nname = NULL;

    if (i > sdata->header.dshb_cvte.dti_object_count) { complain (&sym_complaint); goto end; }
    ret = bfd_sym_fetch_contained_variables_table_entry (abfd, &cventry, i);
    i++;

    if (ret < 0) { complain (&sym_complaint); goto end; }
    if (cventry.generic.type == BFD_SYM_END_OF_LIST) { break; }
    if (cventry.generic.type == BFD_SYM_SOURCE_FILE_CHANGE) { goto end; }

    if (cventry.entry.tte_index > ntypes) { complain (&sym_complaint); goto end; }
    if (typevec[cventry.entry.tte_index] == NULL) { complain (&sym_complaint); goto end; }

    if (cventry.entry.la_size != BFD_SYM_CVTE_SCA) { complain (&sym_complaint); goto end; }

    nname = bfd_sym_symbol_name (abfd, cventry.entry.nte_index);

    if (cventry.entry.address.scstruct.sca_kind == BFD_SYM_STORAGE_KIND_VALUE) {

      if (argvec != NULL) {
	FIELD_TYPE (argvec[nargs]) = typevec[cventry.entry.tte_index];
	FIELD_NAME (argvec[nargs]) = (char *) obstack_alloc (&objfile->symbol_obstack, nname[0] + 1);
	sprintf (FIELD_NAME (argvec[nargs]), "%.*s", nname[0], nname + 1);
	FIELD_BITPOS (argvec[nargs]) = 0;
	FIELD_BITSIZE (argvec[nargs]) = 0;
      }

      nargs++;
    }

    if ((localvec != NULL) && (globalvec != NULL)) {

      struct symbol *lsym = NULL;
      lsym = (struct symbol *) obstack_alloc (&objfile->symbol_obstack, sizeof (struct symbol));

      SYMBOL_TYPE (lsym) = typevec[cventry.entry.tte_index];
      SYMBOL_NAME (lsym) = obstack_alloc (&objfile->symbol_obstack, nname[0] + 1);
      sprintf (SYMBOL_NAME (lsym), "%.*s", nname[0], nname + 1);
      SYMBOL_LANGUAGE (lsym) = language_cplus;
      SYMBOL_SECTION (lsym) = 0;
      SYMBOL_BFD_SECTION (lsym) = 0;
      SYMBOL_LINE (lsym) = 0;
      SYMBOL_BASEREG (lsym) = 0;
      SYMBOL_ALIASES (lsym) = NULL; 
      SYMBOL_RANGES (lsym) = NULL;

      switch (cventry.entry.address.scstruct.sca_kind) {

      case BFD_SYM_STORAGE_KIND_VALUE:

	switch (cventry.entry.address.scstruct.sca_class) {

	case BFD_SYM_STORAGE_CLASS_REGISTER:
	  SYMBOL_NAMESPACE (lsym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (lsym) = LOC_REGPARM;
	  SYMBOL_VALUE (lsym) = cventry.entry.address.scstruct.sca_offset;
	  break;

	case BFD_SYM_STORAGE_CLASS_STACK_RELATIVE:
	case BFD_SYM_STORAGE_CLASS_FRAME_RELATIVE:
	  SYMBOL_NAMESPACE (lsym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (lsym) = LOC_ARG;
	  SYMBOL_VALUE (lsym) = cventry.entry.address.scstruct.sca_offset;
	  break;

	default:
	  complain (&sym_complaint);
	  goto end;
	}

	break;

      case BFD_SYM_STORAGE_KIND_LOCAL:

	switch (cventry.entry.address.scstruct.sca_class) {

	case BFD_SYM_STORAGE_CLASS_GLOBAL:

	  SYMBOL_NAMESPACE (lsym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (lsym) = LOC_BASEREG;
	  SYMBOL_VALUE (lsym) = cventry.entry.address.scstruct.sca_offset - 276 ; 
	  SYMBOL_BASEREG (lsym) = 31;

#if 0
	  SYMBOL_NAMESPACE (lsym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (lsym) = LOC_STATIC;
	  SYMBOL_VALUE_ADDRESS (lsym) = cventry.entry.address.scstruct.sca_offset; 
	  SYMBOL_VALUE_ADDRESS (lsym) += ANOFFSET (objfile->section_offsets, SECT_OFF_DATA (obfjile));
#endif

	  break;

	case BFD_SYM_STORAGE_CLASS_REGISTER:
	  SYMBOL_NAMESPACE (lsym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (lsym) = LOC_REGISTER;
	  SYMBOL_VALUE (lsym) = cventry.entry.address.scstruct.sca_offset;
	  break;

	case BFD_SYM_STORAGE_CLASS_STACK_RELATIVE:
	case BFD_SYM_STORAGE_CLASS_FRAME_RELATIVE:
	  SYMBOL_NAMESPACE (lsym) = VAR_NAMESPACE;
	  SYMBOL_CLASS (lsym) = LOC_LOCAL;
	  SYMBOL_VALUE (lsym) = cventry.entry.address.scstruct.sca_offset;
	  break;

	default:
	  complain (&sym_complaint);
	  goto end;
	}

	break;

      default:
	complain (&sym_complaint);
	goto end;
      }

      if (cventry.entry.address.scstruct.sca_class == BFD_SYM_STORAGE_CLASS_GLOBAL) {
	localvec[nlocals] = lsym;
      } else {
	localvec[nlocals] = lsym;
      }
    }

    if (cventry.entry.address.scstruct.sca_class == BFD_SYM_STORAGE_CLASS_GLOBAL) { 
      nlocals++;
    } else {
      nlocals++;
    }
  }

 end:
  *pnargs = nargs;
  *pnlocals = nlocals;
  *pnglobals = nglobals;
}      

static void sym_read_functions
(struct objfile *objfile, 
 struct type **typevec, unsigned long ntypes,
 struct symbol ***pfuncvec, unsigned long *pnfuncs) 
{
  bfd *abfd = NULL;
  bfd_sym_data_struct *sdata = NULL;

  struct symbol **funcvec = NULL;
  unsigned long nfuncs = 0;

  unsigned long maxfuncs;
  unsigned long i, j;
  int ret;

  CHECK_FATAL (objfile != NULL);
  abfd = objfile->obfd;
  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  CHECK_FATAL (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  maxfuncs = sdata->header.dshb_mte.dti_object_count + 1;

  funcvec = obstack_alloc (&objfile->type_obstack, maxfuncs * sizeof (struct symbol *));

  for (i = 1; i < maxfuncs; i++) {
    
    bfd_sym_modules_table_entry entry;
    const unsigned char *name;

    struct field *argvec;
    unsigned long nargs;

    struct symbol **localvec;
    unsigned long nlocals;

    struct symbol **globalvec;
    unsigned long nglobals;

    struct symbol *fsymbol;
    struct block *fblock;
    struct type *ftype;

    ret = bfd_sym_fetch_modules_table_entry (abfd, &entry, i);
    if (ret < 0) { continue; }
  
    if ((entry.mte_scope) != BFD_SYM_SYMBOL_SCOPE_GLOBAL) { continue; }

    if ((entry.mte_kind != BFD_SYM_MODULE_KIND_FUNCTION)
	&& (entry.mte_kind != BFD_SYM_MODULE_KIND_PROCEDURE)) { continue; }

    sym_read_contained_variables (objfile, &entry, typevec, ntypes, NULL, &nargs, NULL, &nlocals, NULL, &nglobals);

    argvec = (struct field *) obstack_alloc (&objfile->symbol_obstack, nargs * sizeof (struct field));
    localvec = (struct symbol **) obstack_alloc (&objfile->symbol_obstack, nlocals * sizeof (struct symbol *));
    globalvec = (struct symbol **) obstack_alloc (&objfile->symbol_obstack, nglobals * sizeof (struct symbol *));

    sym_read_contained_variables (objfile, &entry, typevec, ntypes, argvec, &nargs, localvec, &nlocals, globalvec, &nglobals);

    ftype = alloc_type (objfile);

    TYPE_TARGET_TYPE (ftype) = builtin_type_error;
    TYPE_LENGTH (ftype) = 1;
    TYPE_CODE (ftype) = TYPE_CODE_FUNC;

    fblock = (struct block *) TYPE_ALLOC (ftype, (sizeof (struct block) + ((nlocals - 1) * sizeof (struct symbol *))));
    fsymbol = (struct symbol *) obstack_alloc (&objfile->symbol_obstack, sizeof (struct symbol));

    BLOCK_FUNCTION (fblock) = fsymbol;
    BLOCK_START (fblock) = entry.mte_res_offset;
    BLOCK_END (fblock) = entry.mte_res_offset + entry.mte_size;
    BLOCK_SUPERBLOCK (fblock) = NULL;
    BLOCK_GCC_COMPILED (fblock) = 0;

    BLOCK_START (fblock) += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
    BLOCK_END (fblock) += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));

    BLOCK_NSYMS (fblock) = nlocals;
    for (j = 0; j < nlocals; j++) {
      BLOCK_SYM (fblock, j) = localvec[j];
    }

    TYPE_NFIELDS (ftype) = nargs;
    TYPE_FIELDS (ftype) = argvec;

    SYMBOL_TYPE (fsymbol) = ftype;
    name = bfd_sym_symbol_name (abfd, entry.mte_nte_index);
    SYMBOL_NAME (fsymbol) = obstack_alloc (&objfile->symbol_obstack, name[0] + 1);
    if ((name[0] > 0) && (name[1] == '.')) {
      sprintf (SYMBOL_NAME (fsymbol), "%.*s", name[0] - 1, name + 2);
    } else {
      sprintf (SYMBOL_NAME (fsymbol), "%.*s", name[0], name + 1);
    }
    SYMBOL_BLOCK_VALUE (fsymbol) = fblock;
    SYMBOL_LANGUAGE (fsymbol) = language_cplus;
    SYMBOL_SECTION (fsymbol) = 0;
    SYMBOL_BFD_SECTION (fsymbol) = 0;
    SYMBOL_NAMESPACE (fsymbol) = VAR_NAMESPACE;
    SYMBOL_CLASS (fsymbol) = LOC_BLOCK;
    SYMBOL_LINE (fsymbol) = 0;
    SYMBOL_BASEREG (fsymbol) = 0;
    SYMBOL_ALIASES (fsymbol) = NULL; 
    SYMBOL_RANGES (fsymbol) = NULL;

    funcvec[nfuncs] = fsymbol;
    nfuncs++;
  }      

  *pnfuncs = nfuncs;
  *pfuncvec = funcvec;
}

static void sym_symfile_init (struct objfile *objfile)
{
  objfile->sym_stab_info = 
    xmmalloc (objfile->md, sizeof (struct dbx_symfile_info));

  memset ((PTR) objfile->sym_stab_info, 0, sizeof (struct dbx_symfile_info));

  objfile->sym_private = 
    xmmalloc (objfile->md,sizeof (struct sym_symfile_info));

  memset (objfile->sym_private, 0, sizeof (struct sym_symfile_info));

  objfile->flags |= OBJF_REORDERED;
  init_entry_point_info (objfile);
}

static void convert_path (unsigned char *dst, const unsigned char *src, size_t len)
{
  size_t i;

  dst[0] = '/';

  for (i = 0; i < len; i++) {
    if (src[i] == '/') {
      dst[i + 1] = ':';
    } else if (src[i] == ':') {
      dst[i + 1] = '/';
    } else {
      dst[i + 1] = src[i];
    }
  }

  dst[len + 1] = '\0';
}

static void sym_symfile_read (struct objfile *objfile, int mainline)
{
  bfd *abfd = NULL;
  bfd_sym_data_struct *sdata = NULL;

  struct blockvector *bv;
  struct block *gblock, *sblock;

  struct type **typevec = NULL;
  struct symbol **typedefvec = NULL;
  struct symbol **funcvec = NULL;

  unsigned long ntypes;
  unsigned long ntypedefs;
  unsigned long nfuncs;
  unsigned long maxtypes;

  CHECK_FATAL (objfile != NULL);
  abfd = objfile->obfd;
  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  CHECK_FATAL (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  sym_read_types (objfile, &maxtypes, &typevec, &ntypes, &typedefvec, &ntypedefs);
  sym_read_functions (objfile, typevec, maxtypes, &funcvec, &nfuncs);

  gblock = (struct block *) obstack_alloc 
    (&objfile->symbol_obstack, (sizeof (struct block) + ((ntypedefs + nfuncs - 1) * sizeof (struct symbol *))));

  BLOCK_NSYMS (gblock) = 0;
  BLOCK_SYM (gblock, 0) = NULL;
  BLOCK_FUNCTION (gblock) = NULL;
  BLOCK_START (gblock) = 0;
  BLOCK_END (gblock) = 0;
  BLOCK_SUPERBLOCK (gblock) = NULL;
  BLOCK_GCC_COMPILED (gblock) = 0;

  sblock = (struct block *) obstack_alloc 
    (&objfile->symbol_obstack, (sizeof (struct block) + (0 * sizeof (struct symbol *))));

  BLOCK_NSYMS (sblock) = 0;
  BLOCK_SYM (sblock, 0) = NULL;
  BLOCK_FUNCTION (sblock) = NULL;
  BLOCK_START (sblock) = 0;
  BLOCK_END (sblock) = 0;
  BLOCK_SUPERBLOCK (sblock) = NULL;
  BLOCK_GCC_COMPILED (sblock) = 0;

  BLOCK_SUPERBLOCK (sblock) = gblock;

  {
    unsigned long nsymbols = ntypedefs + nfuncs;
    unsigned long cur = 0;
    unsigned long i;

    BLOCK_NSYMS (gblock) = nsymbols;

    for (i = 0; i < maxtypes; i++) {
      if (typedefvec[i] != NULL) {
	BLOCK_SYM (gblock, cur) = typedefvec[i];
	cur++;
      }
    }
    
    for (i = 0; i < nfuncs; i++) {
      CHECK_FATAL (funcvec[i] != NULL);
      BLOCK_SYM (gblock, cur) = funcvec[i];
      cur++;
    }

    CHECK_FATAL (cur == nsymbols);
  }

  bv = (struct blockvector *) obstack_alloc
    (&objfile->symbol_obstack, (sizeof (struct blockvector) + (nfuncs + 2 - 1) * sizeof (struct block *)));

  BLOCKVECTOR_NBLOCKS (bv) = nfuncs + 2;

  BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK) = gblock;
  BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK) = sblock;

  {
    unsigned long i;

    for (i = 0; i < nfuncs; i++) {
      BLOCKVECTOR_BLOCK (bv, i + 2) = SYMBOL_BLOCK_VALUE (funcvec[i]);
      BLOCK_SUPERBLOCK (SYMBOL_BLOCK_VALUE (funcvec[i])) = sblock;
    }
  }

  {
    CORE_ADDR minaddr = (CORE_ADDR) -1;
    CORE_ADDR maxaddr = 0;
    unsigned long i;

    for (i = 0; i < nfuncs; i++) {
      if (BLOCK_START (BLOCKVECTOR_BLOCK (bv, i + 2)) < minaddr) {
	minaddr = BLOCK_START (BLOCKVECTOR_BLOCK (bv, i + 2));
      }
      if (BLOCK_END (BLOCKVECTOR_BLOCK (bv, i + 2)) > maxaddr) {
	maxaddr = BLOCK_END (BLOCKVECTOR_BLOCK (bv, i + 2));
      }
    }

    BLOCK_START (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK)) = minaddr;
    BLOCK_END (BLOCKVECTOR_BLOCK (bv, GLOBAL_BLOCK)) = maxaddr;
    BLOCK_START (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)) = minaddr;
    BLOCK_END (BLOCKVECTOR_BLOCK (bv, STATIC_BLOCK)) = maxaddr;
  }


  {
    unsigned int linetable_maxentries = 20000;
    struct symtab *symtab = NULL;
    struct linetable *linetable = NULL;
    unsigned long curpos = 0;
    unsigned long curitem = 0;
    int ret = 0;

    bfd_sym_contained_statements_table_entry entry;
    bfd_sym_file_references_table_entry frtentry;
    bfd_sym_modules_table_entry mtentry;

    unsigned long i;
  
    for (i = 1; i <= sdata->header.dshb_csnte.dti_object_count; i++) {

      ret = bfd_sym_fetch_contained_statements_table_entry (abfd, &entry, i);
      if (ret < 0) { break; }

      switch (entry.generic.type) {

      case BFD_SYM_END_OF_LIST: {
	
	struct linetable *temp = NULL;

	if (curitem >= linetable_maxentries) {
	  complain (&sym_complaint);
	} else if (0) {
	  linetable->item[curitem].line = -1;
	  linetable->item[curitem].pc = mtentry.mte_res_offset + mtentry.mte_size;
	  linetable->item[curitem].pc += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	  curitem++;
	  linetable->nitems = curitem;
	}

	temp = (struct linetable *) xmalloc
	  (sizeof (struct linetable) + sizeof (struct linetable_entry) * (linetable->nitems - 1));
	memcpy (temp, linetable,
		sizeof (struct linetable) + sizeof (struct linetable_entry) * (linetable->nitems - 1));
	xfree (linetable);
	linetable = temp;
	LINETABLE (symtab) = linetable;

	symtab = NULL;
	linetable = NULL;
	curpos = 0;
	curitem = 0;
	break;
      }

      case BFD_SYM_SOURCE_FILE_CHANGE: {

	const unsigned char *namebuf = NULL;
	char *name = NULL;

	ret = bfd_sym_fetch_file_references_table_entry (abfd, &frtentry, entry.file.fref.fref_frte_index);
	if ((ret < 0) || (frtentry.generic.type != BFD_SYM_FILE_NAME_INDEX)) {
	  name = "[UNKNOWN]";
	} else {
	  namebuf = bfd_sym_symbol_name (abfd, frtentry.filename.nte_index);
	  name = xmalloc (namebuf[0] + 1 + 1);
	  convert_path (name, namebuf + 1, namebuf[0]);
	}

	symtab = allocate_symtab (name, objfile);

	linetable = (struct linetable *) xmalloc
	  (sizeof (struct linetable) + sizeof (struct linetable_entry) * linetable_maxentries);
	linetable->lines_are_chars = 1;

	BLOCKVECTOR (symtab) = bv;
	LINETABLE (symtab) = linetable;
	symtab->dirname = NULL;
	symtab->free_code = free_nothing;
	symtab->free_ptr = NULL;
	symtab->primary = 0;
	symtab->debugformat = obsavestring ("xSYM", 4, &objfile->symbol_obstack);

	curpos = entry.file.fref.fref_offset;
	curitem = 0;

	break;
      }

      default: {

	ret = bfd_sym_fetch_modules_table_entry (abfd, &mtentry, entry.entry.mte_index);
	if (ret < 0) { break; }
	
	curpos += entry.entry.mte_offset;

	if (curitem >= linetable_maxentries) {
	  complain (&sym_complaint);
	} else {

	  if (curitem == 0) {
	    linetable->item[curitem].line = curpos;
	    linetable->item[curitem].pc = mtentry.mte_res_offset;
	    linetable->item[curitem].pc += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	    curitem++;
	    linetable->nitems = curitem;
	  }

	  linetable->item[curitem].line = curpos;
	  linetable->item[curitem].pc = mtentry.mte_res_offset + entry.entry.file_delta;
	  linetable->item[curitem].pc += ANOFFSET (objfile->section_offsets, SECT_OFF_TEXT (objfile));
	  curitem++;
	  linetable->nitems = curitem;
	}
      }
      }
    }
  }

  if (objfile->symtabs)
    objfile->symtabs->primary = 1;

  init_minimal_symbol_collection ();
  make_cleanup_discard_minimal_symbols ();
    
  install_minimal_symbols (objfile);
}

static void sym_symfile_finish (struct objfile *objfile)
{
}

static void sym_symfile_offsets (struct objfile *objfile, struct section_addr_info *addrs)
{
  unsigned int i;

  objfile->num_sections = SECT_OFF_MAX;
  objfile->section_offsets = (struct section_offsets *)
    obstack_alloc (&objfile->psymbol_obstack, SIZEOF_SECTION_OFFSETS);
  memset (objfile->section_offsets, 0, SIZEOF_SECTION_OFFSETS);

  for (i = 0; i < objfile->sections_end - objfile->sections; i++) {
    objfile->sections[i].addr += addrs->other[0].addr;
    objfile->sections[i].endaddr += addrs->other[0].addr;
  }

  for (i = 0; i < MAX_SECTIONS; i++) {
    objfile->section_offsets->offsets[i] = (long) addrs->other[0].addr;
  }

  objfile->sect_index_text = 0;
  objfile->sect_index_data = 0;
  objfile->sect_index_bss = 0;
  objfile->sect_index_rodata = 0;
}

static void sym_symfile_display (bfd *abfd, FILE *f)
{
  bfd_sym_data_struct *sdata = NULL;

  CHECK_FATAL (abfd != NULL);
  CHECK_FATAL (abfd->filename != NULL);

  CHECK_FATAL (bfd_sym_valid (abfd));
  sdata = abfd->tdata.sym_data;

  bfd_sym_display_header (f, &sdata->header);
  fprintf (f, "\n");
  bfd_sym_display_name_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_resources_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_modules_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_file_references_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_contained_modules_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_contained_variables_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_contained_statements_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_contained_labels_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_contained_types_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_type_information_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_file_references_index_table (abfd, f);
  fprintf (f, "\n");
  bfd_sym_display_constant_pool (abfd, f);
  fprintf (f, "\n");
}

static struct sym_fns sym_sym_fns =
{
  bfd_target_sym_flavour,

  sym_new_init,		/* sym_new_init: init anything gbl to entire symtab */
  sym_symfile_init,	/* sym_init: read initial info, setup for sym_read() */
  sym_symfile_read,	/* sym_read: read a symbol file into symtab */
  sym_symfile_finish,	/* sym_finish: finished with file, cleanup */
  sym_symfile_offsets,	/* sym_offsets:  xlate external to internal form */
  NULL			/* next: pointer to next struct sym_fns */
};

void sym_dump_command (char *args, int from_tty)
{
  char **argv;
  struct cleanup *cleanups;

  char *symname = NULL;
  char *filename = "/dev/tty";

  FILE *f = NULL;
  bfd *abfd = NULL;

  dont_repeat ();

  if (args == NULL) {
    error ("Usage: sym-dump <symfile> <outfile>");
  }
  if ((argv = buildargv (args)) == NULL) {
    nomem (0);
  }
  cleanups = make_cleanup_freeargv (argv);

  if (argv[0] == NULL) {
    error ("Usage: sym-dump <symfile> <outfile>");
  }
  symname = argv[0];
  if (argv[1] != NULL) {
    filename = argv[1];
  }
  if ((argv[1] != NULL) && (argv[2] != NULL)) {
    error ("Usage: sym-dump <symfile> <outfile>");
  }

  filename = tilde_expand (filename);
  make_cleanup (free, filename);

  symname = tilde_expand (symname);
  make_cleanup (free, symname);

  f = fopen (filename, "w");
  if (f == NULL) {
    error ("Unable to open \"%s\" for writing: %s", filename, strerror (errno));
  }

  abfd = bfd_openr (symname, "sym");
  if (abfd == NULL) {
    error ("Unable to open \"%s\" for reading: %s", symname, bfd_errmsg (bfd_get_error ()));
  }

  if (!bfd_check_format (abfd, bfd_object)) {
    error ("Unable to process \"%s\": %s", symname, bfd_errmsg (bfd_get_error ()));
  }

  immediate_quit++;
  sym_symfile_display (abfd, f);
  immediate_quit--;

  do_cleanups (cleanups);
}

#endif

void
_initialize_symread ()
{
#ifndef __i386__
  sym_builtin_type_pstr = make_pointer_type (builtin_type_char, NULL);
  sym_builtin_type_cstr = make_pointer_type (builtin_type_char, NULL);

  add_symtab_fns (&sym_sym_fns);

  add_com ("sym-dump", class_run, sym_dump_command,
	   "Print the contents of the specified SYM-format symbol file.");
#endif
}

