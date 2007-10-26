/* Definitions for symbol-reading containing "stabs", for GDB.
   Copyright 1992, 1993, 1995, 1996, 1997, 1999, 2000
   Free Software Foundation, Inc.
   Contributed by Cygnus Support.  Written by John Gilmore.

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

/* This file exists to hold the common definitions required of most of
   the symbol-readers that end up using stabs.  The common use of
   these `symbol-type-specific' customizations of the generic data
   structures makes the stabs-oriented symbol readers able to call
   each others' functions as required.  */

#if !defined (GDBSTABS_H)
#define GDBSTABS_H

/* The stab_section_info chain remembers info from the ELF symbol table,
   while psymtabs are being built for the other symbol tables in the 
   objfile.  It is destroyed at the complation of psymtab-reading.
   Any info that was used from it has been copied into psymtabs.  */

struct stab_section_info
  {
    char *filename;
    struct stab_section_info *next;
    int found;			/* Count of times it's found in searching */
    size_t num_sections;
    CORE_ADDR sections[1];
  };

/* Information is passed among various dbxread routines for accessing
   symbol files.  A pointer to this structure is kept in the
   deprecated_sym_stab_info field of the objfile struct.  */

struct dbx_symfile_info
  {
    CORE_ADDR text_addr;	/* Start of text section */
    int text_size;		/* Size of text section */
    /* APPLE LOCAL begin coalesced symbols */
    CORE_ADDR coalesced_text_addr;	/* Start of coalesced_text section */
    int coalesced_text_size;		/* Size of coalesced_text section */
    /* APPLE LOCAL end coalesced symbols */
    int symcount;		/* How many symbols are there in the file */
    char *stringtab;		/* The actual string table */
    int stringtab_size;		/* Its size */
    file_ptr symtab_offset;	/* Offset in file to symbol table */
    int symbol_size;		/* Bytes in a single symbol */
    struct stab_section_info *stab_section_info;	/* section starting points
							   of the original .o files before linking. */

    /* See stabsread.h for the use of the following. */
    struct header_file *header_files;
    int n_header_files;
    int n_allocated_header_files;

    /* APPLE LOCAL: Pointers to struct obj_sections.
       We need the slid addresses of these sections; FSF gdb uses BFD asections
       which have the original file's intended load address.
       These are used to speed up the building of minimal symbols.  */
    struct obj_section *text_section;
    struct obj_section *coalesced_text_section;
    struct obj_section *data_section;
    struct obj_section *bss_section;

    /* Pointer to the separate ".stab" section, if there is one.  */
    asection *stab_section;

    /* APPLE LOCAL begin local stabs */
    /* Record the # and offset of local stab nlist records and
       non-local stab nlist records.  If this information is not
       provided by the static link editor, these will have 0
       values. */
    file_ptr local_stab_offset;
    int local_stab_count;
    file_ptr nonlocal_stab_offset;
    int nonlocal_stab_count;
    /* APPLE LOCAL end local stabs */
  };

#define DBX_SYMFILE_INFO(o)	((o)->deprecated_sym_stab_info)
#define DBX_TEXT_ADDR(o)	(DBX_SYMFILE_INFO(o)->text_addr)
#define DBX_TEXT_SIZE(o)	(DBX_SYMFILE_INFO(o)->text_size)
#define DBX_COALESCED_TEXT_ADDR(o)	(DBX_SYMFILE_INFO(o)->coalesced_text_addr)
#define DBX_COALESCED_TEXT_SIZE(o)	(DBX_SYMFILE_INFO(o)->coalesced_text_size)
#define DBX_SYMCOUNT(o)		(DBX_SYMFILE_INFO(o)->symcount)
#define DBX_STRINGTAB(o)	(DBX_SYMFILE_INFO(o)->stringtab)
#define DBX_STRINGTAB_SIZE(o)	(DBX_SYMFILE_INFO(o)->stringtab_size)
#define DBX_SYMTAB_OFFSET(o)	(DBX_SYMFILE_INFO(o)->symtab_offset)
#define DBX_SYMBOL_SIZE(o)	(DBX_SYMFILE_INFO(o)->symbol_size)
#define DBX_TEXT_SECTION(o)	(DBX_SYMFILE_INFO(o)->text_section)
#define DBX_COALESCED_TEXT_SECTION(o)	(DBX_SYMFILE_INFO(o)->coalesced_text_section)
#define DBX_DATA_SECTION(o)	(DBX_SYMFILE_INFO(o)->data_section)
#define DBX_BSS_SECTION(o)	(DBX_SYMFILE_INFO(o)->bss_section)
#define DBX_STAB_SECTION(o)	(DBX_SYMFILE_INFO(o)->stab_section)

/* APPLE LOCAL: Accessors for the local / non-local stab nlist records */
#define DBX_LOCAL_STAB_OFFSET(o) (DBX_SYMFILE_INFO(o)->local_stab_offset)
#define DBX_LOCAL_STAB_COUNT(o) (DBX_SYMFILE_INFO(o)->local_stab_count)
#define DBX_NONLOCAL_STAB_OFFSET(o) (DBX_SYMFILE_INFO(o)->nonlocal_stab_offset)
#define DBX_NONLOCAL_STAB_COUNT(o) (DBX_SYMFILE_INFO(o)->nonlocal_stab_count)

#endif /* GDBSTABS_H */
