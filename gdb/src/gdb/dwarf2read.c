/* DWARF 2 debugging format support for GDB.

   Copyright 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003,
   2004, 2005
   Free Software Foundation, Inc.

   Adapted by Gary Funck (gary@intrepid.com), Intrepid Technology,
   Inc.  with support from Florida State University (under contract
   with the Ada Joint Program Office), and Silicon Graphics, Inc.
   Initial contribution by Brent Benson, Harris Computer Systems, Inc.,
   based on Fred Fish's (Cygnus Support) implementation of DWARF 1
   support in dwarfread.c

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or (at
   your option) any later version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include "defs.h"
#include "bfd.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "objfiles.h"
#include "elf/dwarf2.h"
#include "buildsym.h"
#include "demangle.h"
#include "expression.h"
#include "filenames.h"	/* for DOSish file names */
#include "macrotab.h"
#include "language.h"
#include "complaints.h"
#include "bcache.h"
#include "dwarf2expr.h"
#include "dwarf2loc.h"
#include "cp-support.h"
#include "hashtab.h"
#include "command.h"
#include "gdbcmd.h"

#include <fcntl.h>
#include "gdb_string.h"
#include "gdb_assert.h"
#include <sys/types.h>
/* APPLE LOCAL - dwarf repository  */
#include "db-access-functions.h"
/* APPLE LOCAL - debug map */
#include "symfile.h"
/* APPLE LOCAL - subroutine inlining  */
#include "inlining.h"
/* APPLE LOCAL - address ranges  */
#include "block.h"
/* APPLE LOCAL - pubtypes reading for "gnutarget"  */
#include "gdbcore.h"
/* APPLE LOCAL - .o file translation data structure  */
#include "dwarf2read.h"
/* APPLE LOCAL psym equivalences  */
#include <ctype.h>
/* APPLE LOCAL objc_invalidate_objc_class */
#include "objc-lang.h"

/* A note on memory usage for this file.
   
   At the present time, this code reads the debug info sections into
   the objfile's objfile_obstack.  A definite improvement for startup
   time, on platforms which do not emit relocations for debug
   sections, would be to use mmap instead.  The object's complete
   debug information is loaded into memory, partly to simplify
   absolute DIE references.

   Whether using obstacks or mmap, the sections should remain loaded
   until the objfile is released, and pointers into the section data
   can be used for any other data associated to the objfile (symbol
   names, type names, location expressions to name a few).  */

#ifndef DWARF2_REG_TO_REGNUM
#define DWARF2_REG_TO_REGNUM(REG) (REG)
#endif

#if 0
/* .debug_info header for a compilation unit
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct comp_unit_header
  {
    unsigned int length;	/* length of the .debug_info
				   contribution */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int abbrev_offset;	/* offset into .debug_abbrev section */
    unsigned char addr_size;	/* byte size of an address -- 4 */
  }
_COMP_UNIT_HEADER;
#define _ACTUAL_COMP_UNIT_HEADER_SIZE 11
#endif

/* .debug_pubnames header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct pubnames_header
  {
    unsigned int length;	/* length of the .debug_pubnames
				   contribution  */
    unsigned char version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned int info_size;	/* byte size of .debug_info section
				   portion */
  }
_PUBNAMES_HEADER;
#define _ACTUAL_PUBNAMES_HEADER_SIZE 13

/* APPLE LOCAL: pubtypes header */

/* .debug_pubtypes header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct pubtypes_header
  {
    unsigned int length;	/* length of the .debug_pubtypes
				   contribution  */
    unsigned char version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned int info_size;	/* byte size of .debug_info section
				   portion */
  }
_PUBTYPES_HEADER;
#define _ACTUAL_PUBTYPES_HEADER_SIZE 13
/* END APPLE LOCAL: pubtypes */

/* .debug_aranges header
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct aranges_header
  {
    unsigned int length;	/* byte len of the .debug_aranges
				   contribution */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int info_offset;	/* offset into .debug_info section */
    unsigned char addr_size;	/* byte size of an address */
    unsigned char seg_size;	/* byte size of segment descriptor */
  }
_ARANGES_HEADER;
#define _ACTUAL_ARANGES_HEADER_SIZE 12

/* .debug_line statement program prologue
   Because of alignment constraints, this structure has padding and cannot
   be mapped directly onto the beginning of the .debug_info section.  */
typedef struct statement_prologue
  {
    unsigned int total_length;	/* byte length of the statement
				   information */
    unsigned short version;	/* version number -- 2 for DWARF
				   version 2 */
    unsigned int prologue_length;	/* # bytes between prologue &
					   stmt program */
    unsigned char minimum_instruction_length;	/* byte size of
						   smallest instr */
    unsigned char default_is_stmt;	/* initial value of is_stmt
					   register */
    char line_base;
    unsigned char line_range;
    unsigned char opcode_base;	/* number assigned to first special
				   opcode */
    unsigned char *standard_opcode_lengths;
  }
_STATEMENT_PROLOGUE;

static const struct objfile_data *dwarf2_objfile_data_key;

struct dwarf2_per_objfile
{
  /* Sizes of debugging sections.  */
  unsigned int info_size;
  unsigned int abbrev_size;
  unsigned int line_size;
  unsigned int pubnames_size;
  /* APPLE LOCAL: pubtypes */
  unsigned int pubtypes_size;
  unsigned int aranges_size;
  unsigned int loc_size;
  unsigned int macinfo_size;
  unsigned int str_size;
  unsigned int ranges_size;
  unsigned int frame_size;
  unsigned int eh_frame_size;
  /* APPLE LOCAL debug inlined section */
  unsigned int inlined_size;

  /* Loaded data from the sections.  */
  char *info_buffer;
  char *abbrev_buffer;
  char *line_buffer;
  char *str_buffer;
  char *macinfo_buffer;
  char *ranges_buffer;
  char *loc_buffer;
  /* APPLE LOCAL debug inlined section  */
  char *inlined_buffer;
  
  /* APPLE LOCAL: use mmap for dwarf sections.  */
#if 0
#ifdef HAVE_MMAP
  /* BFD mmap windows from the sections.  */
  bfd_window info_window;
  bfd_window abbrev_window;
  bfd_window line_window;
  bfd_window str_window;
  bfd_window macinfo_window;
  bfd_window ranges_window;
  bfd_window loc_window;
#endif /* HAVE_MMAP */
#endif
  /* END APPLE LOCAL */

  /* A list of all the compilation units.  This is used to locate
     the target compilation unit of a particular reference.  */
  struct dwarf2_per_cu_data **all_comp_units;

  /* The number of compilation units in ALL_COMP_UNITS.  */
  int n_comp_units;

  /* A chain of compilation units that are currently read in, so that
     they can be freed later.  */
  struct dwarf2_per_cu_data *read_in_chain;
};



/* APPLE LOCAL debug map  */
static CORE_ADDR translate_debug_map_address_with_tuple (
					    struct oso_to_final_addr_map *map,
					    struct oso_final_addr_tuple* match,
					    CORE_ADDR oso_addr, int highpc);

/* APPLE LOCAL begin subroutine inlining  */

/* APPLE LOCAL begin  red-black trees, part 1. */
/* The following data structures and function declarations are for the
   implementaion of red-black tree data structures.  Red-black trees are
   an efficient form of pseudo-balanced binary trees.  For more information
   see the comments labelled "red-black trees, part 2.  */

/* The following data structure is used by the Dwarf types repository
   code.  When the types repository uses a red-black tree, the important
   types repository data is stored in this structure, which the red-black
   tree  nodes point to via their "void * data" field.  */

struct rb_repository_data {
  struct type *type_data;
  struct die_info *die_data;
};

/* The definitions for rb_tree_colors and rb_tree_struct can be found in
   inlining.h.  */

/* Function declarations for red-black tree manipulation functions.  See
   function definitions for more informatino about the functions.  */

static struct rb_tree_node *rb_tree_find_and_remove_node 
                              (struct rb_tree_node **,
			       struct rb_tree_node *, long long, int);
static struct rb_tree_node *rb_tree_remove_node (struct rb_tree_node **, 
						  struct rb_tree_node *);
static struct rb_tree_node *rb_tree_minimum (struct rb_tree_node *);
static struct rb_tree_node *rb_tree_successor (struct rb_tree_node *);
static void left_rotate (struct rb_tree_node **, struct rb_tree_node *);
static void right_rotate (struct rb_tree_node **, struct rb_tree_node *);
static void rb_delete_fixup (struct rb_tree_node **, struct rb_tree_node *);
static void plain_tree_insert (struct rb_tree_node **, struct rb_tree_node *);
static int verify_rb_tree (struct rb_tree_node *);
static int verify_tree_heights (struct rb_tree_node *);
static int verify_tree_colors (struct rb_tree_node *);
static int tree_height (struct rb_tree_node *);
static int num_nodes_in_tree (struct rb_tree_node *);
static void rb_print_tree (struct rb_tree_node *, int);

/* APPLE LOCAL end  red-black trees, part 1. */

/* Flag controlling whether to use the code for maneuvering through inlined
   function calls or not.  */

int dwarf2_allow_inlined_stepping = 1;
int dwarf2_debug_inlined_stepping = 0;

/* APPLE LOCAL inlined function symbols & blocks  */
struct pending *inlined_subroutine_symbols = NULL;

/* APPLE LOCAL end subroutine inlining  */

static struct dwarf2_per_objfile *dwarf2_per_objfile;

static asection *dwarf_info_section;
static asection *dwarf_abbrev_section;
static asection *dwarf_line_section;
static asection *dwarf_pubnames_section;
/* APPLE LOCAL: pubtypes */
static asection *dwarf_pubtypes_section;
/* END APPLE LOCAL */
/* APPLE LOCAL debug inlined section  */
static asection *dwarf_inlined_section;
static asection *dwarf_aranges_section;
static asection *dwarf_loc_section;
static asection *dwarf_macinfo_section;
static asection *dwarf_str_section;
static asection *dwarf_ranges_section;
asection *dwarf_frame_section;
asection *dwarf_eh_frame_section;

/* names of the debugging sections */

/* APPLE LOCAL: Different names, eh.  */

#define INFO_SECTION     "LC_SEGMENT.__DWARF.__debug_info"
#define ABBREV_SECTION   "LC_SEGMENT.__DWARF.__debug_abbrev"
#define LINE_SECTION     "LC_SEGMENT.__DWARF.__debug_line"
#define PUBNAMES_SECTION "LC_SEGMENT.__DWARF.__debug_pubnames"
/* APPLE LOCAL: pubtypes */
#define PUBTYPES_SECTION "LC_SEGMENT.__DWARF.__debug_pubtypes"
/* END APPLE LOCAL */
/* APPLE LOCAL debug inlined section */
#define INLINED_SECTION  "LC_SEGMENT.__DWARF.__debug_inlined"
#define ARANGES_SECTION  "LC_SEGMENT.__DWARF.__debug_aranges"
#define LOC_SECTION      "LC_SEGMENT.__DWARF.__debug_loc"
#define MACINFO_SECTION  "LC_SEGMENT.__DWARF.__debug_macinfo"
#define STR_SECTION      "LC_SEGMENT.__DWARF.__debug_str"
#define FRAME_SECTION    "LC_SEGMENT.__DWARF.__debug_frame"
#define RANGES_SECTION   "LC_SEGMENT.__DWARF.__debug_ranges"
#define EH_FRAME_SECTION "LC_SEGMENT.__TEXT.__eh_frame"

/* local data types */

/* We hold several abbreviation tables in memory at the same time. */
#ifndef ABBREV_HASH_SIZE
#define ABBREV_HASH_SIZE 121
#endif

/* The data in a compilation unit header, after target2host
   translation, looks like this.  */
struct comp_unit_head
{
  unsigned long length;
  short version;
  unsigned int abbrev_offset;
  unsigned char addr_size;
  unsigned char signed_addr_p;

  /* Size of file offsets; either 4 or 8.  */
  unsigned int offset_size;

  /* Size of the length field; either 4 or 12.  */
  unsigned int initial_length_size;

  /* Offset to the first byte of this compilation unit header in the
     .debug_info section, for resolving relative reference dies.  */
  unsigned int offset;

  /* Pointer to this compilation unit header in the .debug_info
     section.  */
  char *cu_head_ptr;

  /* Pointer to the first die of this compilation unit.  This will be
     the first byte following the compilation unit header.  */
  char *first_die_ptr;

  /* Pointer to the next compilation unit header in the program.  */
  struct comp_unit_head *next;

  /* Base address of this compilation unit.  */
  /* APPLE LOCAL NB: I've changed this from BASE_ADDRESS to 
     BASE_ADDRESS_UNTRANSLATED to make it clear what this value
     is.  In the case of debug-info-in-.o-files or a kext dSYM,
     where the DWARF info does not have the final relocated
     addresses, this base address is the address of the CU
     in the .o file or kext dSYM.  */
  CORE_ADDR base_address_untranslated;

  /* Non-zero if base_address has been set.  */
  int base_known;
};

/* Fixed size for the DIE hash table.  */
#ifndef REF_HASH_SIZE
#define REF_HASH_SIZE 1021
#endif

/* Internal state when decoding a particular compilation unit.  */
struct dwarf2_cu
{
  /* The objfile containing this compilation unit.  */
  struct objfile *objfile;

  /* The header of the compilation unit.

     FIXME drow/2003-11-10: Some of the things from the comp_unit_head
     should logically be moved to the dwarf2_cu structure.  */
  struct comp_unit_head header;

  struct function_range *first_fn, *last_fn, *cached_fn;

  /* The language we are debugging.  */
  enum language language;
  const struct language_defn *language_defn;

  const char *producer;

  /* APPLE LOCAL: Retain the compilation directory pathname for header
     file relative pathnames (via gcc parameters like "-I../../../include").  */
  char *comp_dir;

  /* The generic symbol table building routines have separate lists for
     file scope symbols and all all other scopes (local scopes).  So
     we need to select the right one to pass to add_symbol_to_list().
     We do it by keeping a pointer to the correct list in list_in_scope.

     FIXME: The original dwarf code just treated the file scope as the
     first local scope, and all other local scopes as nested local
     scopes, and worked fine.  Check to see if we really need to
     distinguish these in buildsym.c.  */
  struct pending **list_in_scope;

  /* Maintain an array of referenced fundamental types for the current
     compilation unit being read.  For DWARF version 1, we have to construct
     the fundamental types on the fly, since no information about the
     fundamental types is supplied.  Each such fundamental type is created by
     calling a language dependent routine to create the type, and then a
     pointer to that type is then placed in the array at the index specified
     by it's FT_<TYPENAME> value.  The array has a fixed size set by the
     FT_NUM_MEMBERS compile time constant, which is the number of predefined
     fundamental types gdb knows how to construct.  */
  struct type *ftypes[FT_NUM_MEMBERS];	/* Fundamental types */

  /* DWARF abbreviation table associated with this compilation unit.  */
  struct abbrev_info **dwarf2_abbrevs;

  /* Storage for the abbrev table.  */
  struct obstack abbrev_obstack;

  /* Hash table holding all the loaded partial DIEs.  */
  htab_t partial_dies;

  /* Storage for things with the same lifetime as this read-in compilation
     unit, including partial DIEs.  */
  struct obstack comp_unit_obstack;

  /* When multiple dwarf2_cu structures are living in memory, this field
     chains them all together, so that they can be released efficiently.
     We will probably also want a generation counter so that most-recently-used
     compilation units are cached...  */
  struct dwarf2_per_cu_data *read_in_chain;

  /* Backchain to our per_cu entry if the tree has been built.  */
  struct dwarf2_per_cu_data *per_cu;

  /* How many compilation units ago was this CU last referenced?  */
  int last_used;

  /* A hash table of die offsets for following references.  */
  struct die_info *die_ref_table[REF_HASH_SIZE];

  /* Full DIEs if read in.  */
  struct die_info *dies;

  /* A set of pointers to dwarf2_per_cu_data objects for compilation
     units referenced by this one.  Only set during full symbol processing;
     partial symbol tables do not have dependencies.  */
  htab_t dependencies;

  /* Mark used when releasing cached dies.  */
  unsigned int mark : 1;

  /* This flag will be set if this compilation unit might include
     inter-compilation-unit references.  */
  unsigned int has_form_ref_addr : 1;

  /* This flag will be set if this compilation unit includes any
     DW_TAG_namespace DIEs.  If we know that there are explicit
     DIEs for namespaces, we don't need to try to infer them
     from mangled names.  */
  unsigned int has_namespace_info : 1;

  /* APPLE LOCAL begin dwarf repository  */
  sqlite3 *repository;

  char *repository_name;
  /* APPLE LOCAL end dwarf repository  */

  /* APPLE LOCAL debug map */
  struct oso_to_final_addr_map *addr_map;
};

/* Persistent data held for a compilation unit, even when not
   processing it.  We put a pointer to this structure in the
   read_symtab_private field of the psymtab.  If we encounter
   inter-compilation-unit references, we also maintain a sorted
   list of all compilation units.  */

struct dwarf2_per_cu_data
{
  /* The start offset and length of this compilation unit.  2**31-1
     bytes should suffice to store the length of any compilation unit
     - if it doesn't, GDB will fall over anyway.  */
  unsigned long offset;
  unsigned long length : 31;

  /* Flag indicating this compilation unit will be read in before
     any of the current compilation units are processed.  */
  unsigned long queued : 1;

  /* Set iff currently read in.  */
  struct dwarf2_cu *cu;

  /* If full symbols for this CU have been read in, then this field
     holds a map of DIE offsets to types.  It isn't always possible
     to reconstruct this information later, so we have to preserve
     it.  */
  htab_t type_hash;

  /* The partial symbol table associated with this compilation unit.  */
  struct partial_symtab *psymtab;
};

/* APPLE LOCAL begin psym equivalences */

/* This is a global flag indicating that we found partial dies with
   'equivalence' function names, e.g. 'putenv' and '*_putenv$UNIX2003'.  */

int psym_equivalences = 0;

/* APPLE LOCAL end psym equivalences  */

/* The line number information for a compilation unit (found in the
   .debug_line section) begins with a "statement program header",
   which contains the following information.  */
struct line_header
{
  unsigned int total_length;
  unsigned short version;
  unsigned int header_length;
  unsigned char minimum_instruction_length;
  unsigned char default_is_stmt;
  int line_base;
  unsigned char line_range;
  unsigned char opcode_base;

  /* standard_opcode_lengths[i] is the number of operands for the
     standard opcode whose value is i.  This means that
     standard_opcode_lengths[0] is unused, and the last meaningful
     element is standard_opcode_lengths[opcode_base - 1].  */
  unsigned char *standard_opcode_lengths;

  /* The include_directories table.  NOTE!  These strings are not
     allocated with xmalloc; instead, they are pointers into
     debug_line_buffer.  If you try to free them, `free' will get
     indigestion.  */
  unsigned int num_include_dirs, include_dirs_size;
  char **include_dirs;

  /* The file_names table.  NOTE!  These strings are not allocated
     with xmalloc; instead, they are pointers into debug_line_buffer.
     Don't try to free them directly.  */
  unsigned int num_file_names, file_names_size;
  struct file_entry
  {
    char *name;
    unsigned int dir_index;
    unsigned int mod_time;
    unsigned int length;
    int included_p; /* Non-zero if referenced by the Line Number Program.  */
  } *file_names;

  /* The start and end of the statement program following this
     header.  These point into dwarf2_per_objfile->line_buffer.  */
  char *statement_program_start, *statement_program_end;
};

/* When we construct a partial symbol table entry we only
   need this much information. */
struct partial_die_info
  {
    /* Offset of this DIE.  */
    unsigned int offset;

    /* DWARF-2 tag for this DIE.  */
    ENUM_BITFIELD(dwarf_tag) tag : 16;

    /* Language code associated with this DIE.  This is only used
       for the compilation unit DIE.  */
    unsigned int language : 8;

    /* Assorted flags describing the data found in this DIE.  */
    unsigned int has_children : 1;
    unsigned int is_external : 1;
    unsigned int is_declaration : 1;
    unsigned int has_type : 1;
    unsigned int has_specification : 1;
    unsigned int has_stmt_list : 1;
    unsigned int has_pc_info : 1;
    /* APPLE LOCAL begin dwarf repository  */
    unsigned int has_repo_specification : 1;
    unsigned int has_repository : 1;
    unsigned int has_repository_type : 1;
    /* APPLE LOCAL end dwarf repository  */

    /* Flag set if the SCOPE field of this structure has been
       computed.  */
    unsigned int scope_set : 1;

    /* The name of this DIE.  Normally the value of DW_AT_name, but
       sometimes DW_TAG_MIPS_linkage_name or a string computed in some
       other fashion.  */
    char *name;
    char *dirname;

    /* The scope to prepend to our children.  This is generally
       allocated on the comp_unit_obstack, so will disappear
       when this compilation unit leaves the cache.  */
    char *scope;

    /* The location description associated with this DIE, if any.  */
    struct dwarf_block *locdesc;

    /* If HAS_PC_INFO, the PC range associated with this DIE.  */
    CORE_ADDR lowpc;
    CORE_ADDR highpc;

    /* Pointer into the info_buffer pointing at the target of
       DW_AT_sibling, if any.  */
    char *sibling;

    /* If HAS_SPECIFICATION, the offset of the DIE referred to by
       DW_AT_specification (or DW_AT_abstract_origin or
       DW_AT_extension).  */
    unsigned int spec_offset;

    /* APPLE LOCAL begin dwarf repository  */
    /* If HAS_REPO_SPECIFICATION, the id of the DIE in the sql
       repository referred to by DW_AT_APPLE_repository_specification.  */

    unsigned int repo_spec_id;
    
    /* The filename of the dwarf sql repository file, if one was used.  */

    char *repo_name;
    /* APPLE LOCAL end dwarf repository  */

    /* If HAS_STMT_LIST, the offset of the Line Number Information data.  */
    unsigned int line_offset;

    /* Pointers to this DIE's parent, first child, and next sibling,
       if any.  */
    struct partial_die_info *die_parent, *die_child, *die_sibling;

   /* APPLE LOCAL begin psym equivalences  */
   /* The psym equivalence name this die contains, if any.  */
    char *equiv_name;		     
  /* APPLE LOCAL end psym equivalences  */
  };

/* This data structure holds the information of an abbrev. */
struct abbrev_info
  {
    unsigned int number;	/* number identifying abbrev */
    enum dwarf_tag tag;		/* dwarf tag */
    unsigned short has_children;		/* boolean */
    unsigned short num_attrs;	/* number of attributes */
    struct attr_abbrev *attrs;	/* an array of attribute descriptions */
    struct abbrev_info *next;	/* next in chain */
  };

struct attr_abbrev
  {
    enum dwarf_attribute name;
    enum dwarf_form form;
  };

/* This data structure holds a complete die structure. */
struct die_info
  {
    enum dwarf_tag tag;		/* Tag indicating type of die */
    unsigned int abbrev;	/* Abbrev number */
    unsigned int offset;	/* Offset in .debug_info section */
    /* APPLE LOCAL - dwarf repository  */
    unsigned int repository_id; /* Id number in debug repository */
    unsigned int num_attrs;	/* Number of attributes */
    struct attribute *attrs;	/* An array of attributes */
    struct die_info *next_ref;	/* Next die in ref hash table */

    /* The dies in a compilation unit form an n-ary tree.  PARENT
       points to this die's parent; CHILD points to the first child of
       this node; and all the children of a given node are chained
       together via their SIBLING fields, terminated by a die whose
       tag is zero.  */
    struct die_info *child;	/* Its first child, if any.  */
    struct die_info *sibling;	/* Its next sibling, if any.  */
    struct die_info *parent;	/* Its parent, if any.  */

    struct type *type;		/* Cached type information */
  };

/* Attributes have a name and a value */
struct attribute
  {
    enum dwarf_attribute name;
    enum dwarf_form form;
    union
      {
	char *str;
	struct dwarf_block *blk;
	unsigned long unsnd;
	long int snd;
	CORE_ADDR addr;
      }
    u;
  };

struct function_range
{
  const char *name;
  CORE_ADDR lowpc, highpc;
  int seen_line;
  struct function_range *next;
};

/* Get at parts of an attribute structure */

#define DW_STRING(attr)    ((attr)->u.str)
#define DW_UNSND(attr)     ((attr)->u.unsnd)
#define DW_BLOCK(attr)     ((attr)->u.blk)
#define DW_SND(attr)       ((attr)->u.snd)
#define DW_ADDR(attr)	   ((attr)->u.addr)

/* Blocks are a bunch of untyped bytes. */
struct dwarf_block
  {
    unsigned int size;
    char *data;
  };

#ifndef ATTR_ALLOC_CHUNK
#define ATTR_ALLOC_CHUNK 4
#endif

/* Allocate fields for structs, unions and enums in this size.  */
#ifndef DW_FIELD_ALLOC_CHUNK
#define DW_FIELD_ALLOC_CHUNK 4
#endif

/* APPLE LOCAL begin radar 6568709  */
/* Determine how many bytes a pointer/address is supposed to have.  */
#ifndef TARGET_ADDRESS_BYTES
#define TARGET_ADDRESS_BYTES (TARGET_LONG_BIT / TARGET_CHAR_BIT)
#endif
/* APPLE LOCAL end radar 6568709  */

/* A zeroed version of a partial die for initialization purposes.  */
/* APPLE LOCAL avoid unused var warning.  */
/* static struct partial_die_info zeroed_partial_die; */

/* APPLE LOCAL: Track the current common block symbol so we can properly 
   offset addresses within that common block.  */
static char *decode_locdesc_common = NULL;

/* FIXME: decode_locdesc sets these variables to describe the location
   to the caller.  These ought to be a structure or something.   If
   none of the flags are set, the object lives at the address returned
   by decode_locdesc.  */

static int isreg;		/* Object lives in register.
				   decode_locdesc's return value is
				   the register number.  */

/* FIXME: We might want to set this from BFD via bfd_arch_bits_per_byte,
   but this would require a corresponding change in unpack_field_as_long
   and friends.  */
static int bits_per_byte = 8;

/* The routines that read and process dies for a C struct or C++ class
   pass lists of data member fields and lists of member function fields
   in an instance of a field_info structure, as defined below.  */
struct field_info
  {
    /* List of data member and baseclasses fields. */
    struct nextfield
      {
	struct nextfield *next;
	int accessibility;
	int virtuality;
	struct field field;
      }
     *fields;

    /* Number of fields.  */
    int nfields;

    /* Number of baseclasses.  */
    int nbaseclasses;

    /* Set if the accesibility of one of the fields is not public.  */
    int non_public_fields;

    /* Member function fields array, entries are allocated in the order they
       are encountered in the object file.  */
    struct nextfnfield
      {
	struct nextfnfield *next;
	struct fn_field fnfield;
      }
     *fnfields;

    /* Member function fieldlist array, contains name of possibly overloaded
       member function, number of overloaded member functions and a pointer
       to the head of the member function field chain.  */
    struct fnfieldlist
      {
	char *name;
	int length;
	struct nextfnfield *head;
      }
     *fnfieldlists;

    /* Number of entries in the fnfieldlists array.  */
    int nfnfields;
  };

/* One item on the queue of compilation units to read in full symbols
   for.  */
struct dwarf2_queue_item
{
  struct dwarf2_per_cu_data *per_cu;
  struct dwarf2_queue_item *next;
};

/* The current queue.  */
static struct dwarf2_queue_item *dwarf2_queue, *dwarf2_queue_tail;

/* Loaded secondary compilation units are kept in memory until they
   have not been referenced for the processing of this many
   compilation units.  Set this to zero to disable caching.  Cache
   sizes of up to at least twenty will improve startup time for
   typical inter-CU-reference binaries, at an obvious memory cost.  */
static int dwarf2_max_cache_age = 5;
static void
show_dwarf2_max_cache_age (struct ui_file *file, int from_tty,
			   struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("\
The upper bound on the age of cached dwarf2 compilation units is %s.\n"),
		    value);
}

/* APPLE LOCAL: A way to find out what how the DWARF debug map is translating
   addresses.  Results in a lot of output.  */
static int debug_debugmap = 0;
static void
show_debug_debugmap (struct ui_file *file, int from_tty,
                     struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("DWARF debugmap debugging is %s.\n"), value);
}


/* Various complaints about symbol reading that don't abort the process */

static void
dwarf2_statement_list_fits_in_line_number_section_complaint (void)
{
  complaint (&symfile_complaints,
	     _("statement list doesn't fit in .debug_line section"));
}

static void
dwarf2_complex_location_expr_complaint (void)
{
  complaint (&symfile_complaints, _("location expression too complex"));
}

static void
dwarf2_const_value_length_mismatch_complaint (const char *arg1, int arg2,
					      int arg3)
{
  complaint (&symfile_complaints,
	     _("const value length mismatch for '%s', got %d, expected %d"), arg1,
	     arg2, arg3);
}

static void
dwarf2_macros_too_long_complaint (void)
{
  complaint (&symfile_complaints,
	     _("macro info runs off end of `.debug_macinfo' section"));
}

static void
dwarf2_macro_malformed_definition_complaint (const char *arg1)
{
  complaint (&symfile_complaints,
	     _("macro debug info contains a malformed macro definition:\n`%s'"),
	     arg1);
}

static void
dwarf2_invalid_attrib_class_complaint (const char *arg1, const char *arg2)
{
  complaint (&symfile_complaints,
	     _("invalid attribute class or form for '%s' in '%s'"), arg1, arg2);
}

/* local function prototypes */

static void dwarf2_locate_sections (bfd *, asection *, void *);
/* APPLE LOCAL begin debug inlined section  */
static bfd_boolean find_debug_inlined_section (bfd *, asection *, void *);
static bfd_boolean find_debug_str_section (bfd *, asection *, void *);
static void scan_partial_inlined_function_symbols (struct dwarf2_cu *);
static void fix_inlined_subroutine_symbols (void);
/* APPLE LOCAL end debug inlined section  */

#if 0
static void dwarf2_build_psymtabs_easy (struct objfile *, int);
#endif

static void dwarf2_create_include_psymtab (char *, struct partial_symtab *,
                                           struct objfile *);

static void dwarf2_build_include_psymtabs (struct dwarf2_cu *,
                                           struct partial_die_info *,
                                           struct partial_symtab *);

static void dwarf2_build_psymtabs_hard (struct objfile *, int);

/* APPLE LOCAL begin psym equivalences  */
static void scan_partial_symbols (struct partial_die_info *,
				  CORE_ADDR *, CORE_ADDR *,
				  struct dwarf2_cu *,
				  struct equiv_psym_list **);
/* APPLE LOCAL end psym equivalences  */

static void add_partial_symbol (struct partial_die_info *,
				struct dwarf2_cu *);

static int pdi_needs_namespace (enum dwarf_tag tag);

static void add_partial_namespace (struct partial_die_info *pdi,
				   CORE_ADDR *lowpc, CORE_ADDR *highpc,
				   struct dwarf2_cu *cu);

static void add_partial_enumeration (struct partial_die_info *enum_pdi,
				     struct dwarf2_cu *cu);

static char *locate_pdi_sibling (struct partial_die_info *orig_pdi,
				 char *info_ptr,
				 bfd *abfd,
				 struct dwarf2_cu *cu);

static void dwarf2_psymtab_to_symtab (struct partial_symtab *);

static void psymtab_to_symtab_1 (struct partial_symtab *);

static void dwarf2_read_abbrevs (bfd *abfd, struct dwarf2_cu *cu);

static void dwarf2_free_abbrev_table (void *);

static struct abbrev_info *peek_die_abbrev (char *, int *, struct dwarf2_cu *);

static struct abbrev_info *dwarf2_lookup_abbrev (unsigned int,
						 struct dwarf2_cu *);

static struct partial_die_info *load_partial_dies (bfd *, char *, int,
						   struct dwarf2_cu *);

static char *read_partial_die (struct partial_die_info *,
			       struct abbrev_info *abbrev, unsigned int,
			       bfd *, char *, struct dwarf2_cu *);

static struct partial_die_info *find_partial_die (unsigned long,
						  struct dwarf2_cu *);

static void fixup_partial_die (struct partial_die_info *,
			       struct dwarf2_cu *);

static char *read_full_die (struct die_info **, bfd *, char *,
			    struct dwarf2_cu *, int *);

static char *read_attribute (struct attribute *, struct attr_abbrev *,
			     bfd *, char *, struct dwarf2_cu *);

static char *read_attribute_value (struct attribute *, unsigned,
			     bfd *, char *, struct dwarf2_cu *);

static unsigned int read_1_byte (bfd *, char *);

static int read_1_signed_byte (bfd *, char *);

static unsigned int read_2_bytes (bfd *, char *);

static unsigned int read_4_bytes (bfd *, char *);

static unsigned long read_8_bytes (bfd *, char *);

static CORE_ADDR read_address (bfd *, char *ptr, struct dwarf2_cu *,
			       int *bytes_read);

static LONGEST read_initial_length (bfd *, char *,
                                    struct comp_unit_head *, int *bytes_read);

static LONGEST read_offset (bfd *, char *, const struct comp_unit_head *,
                            int *bytes_read);

static char *read_n_bytes (bfd *, char *, unsigned int);

static char *read_string (bfd *, char *, unsigned int *);

static char *read_indirect_string (bfd *, char *, const struct comp_unit_head *,
				   unsigned int *);

static unsigned long read_unsigned_leb128 (bfd *, char *, unsigned int *);

static long read_signed_leb128 (bfd *, char *, unsigned int *);

static char *skip_leb128 (bfd *, char *);

static void set_cu_language (unsigned int, struct dwarf2_cu *);

static struct attribute *dwarf2_attr (struct die_info *, unsigned int,
				      struct dwarf2_cu *);

static int dwarf2_flag_true_p (struct die_info *die, unsigned name,
                               struct dwarf2_cu *cu);

static int die_is_declaration (struct die_info *, struct dwarf2_cu *cu);

static struct die_info *die_specification (struct die_info *die,
					   struct dwarf2_cu *);

static void free_line_header (struct line_header *lh);

static void add_file_name (struct line_header *, char *, unsigned int,
                           unsigned int, unsigned int);

static struct line_header *(dwarf_decode_line_header
                            (unsigned int offset,
                             bfd *abfd, struct dwarf2_cu *cu));

static void dwarf_decode_lines (struct line_header *, char *, bfd *,
				struct dwarf2_cu *, struct partial_symtab *);

/* APPLE LOCAL: Third parameter.  */
static void dwarf2_start_subfile (char *, char *, char *);

static char * find_debug_info_for_pst (struct partial_symtab *pst);

static struct symbol *new_symbol (struct die_info *, struct type *,
				  struct dwarf2_cu *);

static void dwarf2_const_value (struct attribute *, struct symbol *,
				struct dwarf2_cu *);

static void dwarf2_const_value_data (struct attribute *attr,
				     struct symbol *sym,
				     int bits);

static struct type *die_type (struct die_info *, struct dwarf2_cu *);

static struct type *die_containing_type (struct die_info *,
					 struct dwarf2_cu *);

static struct type *tag_type_to_type (struct die_info *, struct dwarf2_cu *);

static void read_type_die (struct die_info *, struct dwarf2_cu *);

static char *determine_prefix (struct die_info *die, struct dwarf2_cu *);

static char *typename_concat (struct obstack *, const char *prefix, const char *suffix,
			      struct dwarf2_cu *);

static void read_typedef (struct die_info *, struct dwarf2_cu *);

static void read_base_type (struct die_info *, struct dwarf2_cu *);

static void read_subrange_type (struct die_info *die, struct dwarf2_cu *cu);

static void read_file_scope (struct die_info *, struct dwarf2_cu *);

static void read_func_scope (struct die_info *, struct dwarf2_cu *);

static void read_lexical_block_scope (struct die_info *, struct dwarf2_cu *);

/* APPLE LOCAL begin address ranges  */
static int dwarf2_get_pc_bounds (struct die_info *,
				 CORE_ADDR *, CORE_ADDR *, 
				 struct address_range_list **, 
				 struct dwarf2_cu *);
/* APPLE LOCAL end address ranges  */

static void get_scope_pc_bounds (struct die_info *,
				 CORE_ADDR *, CORE_ADDR *,
				 struct dwarf2_cu *);

static void dwarf2_add_field (struct field_info *, struct die_info *,
			      struct dwarf2_cu *);

static void dwarf2_attach_fields_to_type (struct field_info *,
					  struct type *, struct dwarf2_cu *);

static void dwarf2_add_member_fn (struct field_info *,
				  struct die_info *, struct type *,
				  struct dwarf2_cu *);

static void dwarf2_attach_fn_fields_to_type (struct field_info *,
					     struct type *, struct dwarf2_cu *);

static void read_structure_type (struct die_info *, struct dwarf2_cu *);

static void process_structure_scope (struct die_info *, struct dwarf2_cu *);

static char *determine_class_name (struct die_info *die, struct dwarf2_cu *cu);

static void read_common_block (struct die_info *, struct dwarf2_cu *);

static void read_namespace (struct die_info *die, struct dwarf2_cu *);

static const char *namespace_name (struct die_info *die,
				   int *is_anonymous, struct dwarf2_cu *);

static void read_enumeration_type (struct die_info *, struct dwarf2_cu *);

static void process_enumeration_scope (struct die_info *, struct dwarf2_cu *);

static struct type *dwarf_base_type (int, int, struct dwarf2_cu *);

static CORE_ADDR decode_locdesc (struct dwarf_block *, struct dwarf2_cu *);

static void read_array_type (struct die_info *, struct dwarf2_cu *);

static enum dwarf_array_dim_ordering read_array_order (struct die_info *, 
						       struct dwarf2_cu *);

static void read_tag_pointer_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_ptr_to_member_type (struct die_info *,
					 struct dwarf2_cu *);

static void read_tag_reference_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_const_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_volatile_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_restrict_type (struct die_info *, struct dwarf2_cu *);

static void read_tag_string_type (struct die_info *, struct dwarf2_cu *);

static void read_subroutine_type (struct die_info *, struct dwarf2_cu *);

/* APPLE LOCAL begin subroutine inlining  */
static void read_inlined_subroutine_scope (struct die_info *, struct dwarf2_cu *);
/* APPLE LOCAL end subroutine inlining  */

static struct die_info *read_comp_unit (char *, bfd *, struct dwarf2_cu *);

static struct die_info *read_die_and_children (char *info_ptr, bfd *abfd,
					       struct dwarf2_cu *,
					       char **new_info_ptr,
					       struct die_info *parent);

static struct die_info *read_die_and_siblings (char *info_ptr, bfd *abfd,
					       struct dwarf2_cu *,
					       char **new_info_ptr,
					       struct die_info *parent);

static void free_die_list (struct die_info *);

static void process_die (struct die_info *, struct dwarf2_cu *);

static char *dwarf2_linkage_name (struct die_info *, struct dwarf2_cu *);

static char *dwarf2_name (struct die_info *die, struct dwarf2_cu *);

static struct die_info *dwarf2_extension (struct die_info *die,
					  struct dwarf2_cu *);

static char *dwarf_tag_name (unsigned int);

static char *dwarf_attr_name (unsigned int);

static char *dwarf_form_name (unsigned int);

static char *dwarf_stack_op_name (unsigned int);

static char *dwarf_bool_name (unsigned int);

static char *dwarf_type_encoding_name (unsigned int);

#if 0
static char *dwarf_cfi_name (unsigned int);

struct die_info *copy_die (struct die_info *);
#endif

static struct die_info *sibling_die (struct die_info *);

static void dump_die (struct die_info *);

static void dump_die_list (struct die_info *);

static void store_in_ref_table (unsigned int, struct die_info *,
				struct dwarf2_cu *);

static unsigned int dwarf2_get_ref_die_offset (struct attribute *,
					       struct dwarf2_cu *);

static int dwarf2_get_attr_constant_value (struct attribute *, int);

static struct die_info *follow_die_ref (struct die_info *,
					struct attribute *,
					struct dwarf2_cu *);

static struct type *dwarf2_fundamental_type (struct objfile *, int,
					     struct dwarf2_cu *);

/* memory allocation interface */

static struct dwarf_block *dwarf_alloc_block (struct dwarf2_cu *);

static struct abbrev_info *dwarf_alloc_abbrev (struct dwarf2_cu *);

static struct die_info *dwarf_alloc_die (void);

static void initialize_cu_func_list (struct dwarf2_cu *);

static void add_to_cu_func_list (const char *, CORE_ADDR, CORE_ADDR,
				 struct dwarf2_cu *);

static void dwarf_decode_macros (struct line_header *, unsigned int,
                                 char *, bfd *, struct dwarf2_cu *);

static int attr_form_is_block (struct attribute *);

static void
dwarf2_symbol_mark_computed (struct attribute *attr, struct symbol *sym,
			     struct dwarf2_cu *cu);

static char *skip_one_die (char *info_ptr, struct abbrev_info *abbrev,
			   struct dwarf2_cu *cu);

static void free_stack_comp_unit (void *);

static void *hashtab_obstack_allocate (void *data, size_t size, size_t count);

static void dummy_obstack_deallocate (void *object, void *data);

static hashval_t partial_die_hash (const void *item);

static int partial_die_eq (const void *item_lhs, const void *item_rhs);

static struct dwarf2_per_cu_data *dwarf2_find_containing_comp_unit
  (unsigned long offset, struct objfile *objfile);

static struct dwarf2_per_cu_data *dwarf2_find_comp_unit
  (unsigned long offset, struct objfile *objfile);

static void free_one_comp_unit (void *);

static void free_cached_comp_units (void *);

static void age_cached_comp_units (void);

static void free_one_cached_comp_unit (void *);

static void set_die_type (struct die_info *, struct type *,
			  struct dwarf2_cu *);

static void reset_die_and_siblings_types (struct die_info *,
					  struct dwarf2_cu *);

static void create_all_comp_units (struct objfile *);

/* APPLE LOCAL debug map take an optional oso_to_final_addr_map parameter */
static struct dwarf2_cu *load_full_comp_unit (struct dwarf2_per_cu_data *per_cu,
                                             struct oso_to_final_addr_map *addr_map);

static void process_full_comp_unit (struct dwarf2_per_cu_data *);

static void dwarf2_add_dependence (struct dwarf2_cu *,
				   struct dwarf2_per_cu_data *);

static void dwarf2_mark (struct dwarf2_cu *);

static void dwarf2_clear_marks (struct dwarf2_per_cu_data *);

static void read_set_type (struct die_info *, struct dwarf2_cu *);

/* Try to locate the sections we need for DWARF 2 debugging
   information and return true if we have enough to do something.  */

/* APPLE LOCAL debug map: Rename this function from dwarf2_has_info to
   dwarf2_has_info_1 and add a bfd parameter.  Traditional callers go
   through dwarf2_has_info; debug map callers can go to dwarf2_has_info_1
   and specify the bfd explicitly.  */

static int
dwarf2_has_info_1 (struct objfile *objfile, bfd *abfd)
{
  struct dwarf2_per_objfile *data;
  
  /* Initialize per-objfile state.  */
  data = obstack_alloc (&objfile->objfile_obstack, sizeof (*data));
  memset (data, 0, sizeof (*data));
  set_objfile_data (objfile, dwarf2_objfile_data_key, data);
  dwarf2_per_objfile = data;
   
  dwarf_info_section = 0;
  dwarf_abbrev_section = 0;
  dwarf_line_section = 0;
  dwarf_str_section = 0;
  dwarf_macinfo_section = 0;
  dwarf_frame_section = 0;
  dwarf_eh_frame_section = 0;
  dwarf_ranges_section = 0;
  dwarf_loc_section = 0;
  /* APPLE LOCAL debug inlined section  */
  dwarf_inlined_section = 0;
 
  bfd_map_over_sections (abfd, dwarf2_locate_sections, NULL);
  return (dwarf_info_section != NULL && dwarf_abbrev_section != NULL);
}

int
dwarf2_has_info (struct objfile *objfile)
{
  return dwarf2_has_info_1 (objfile, objfile->obfd);
}

/* This function is mapped across the sections and remembers the
   offset and size of each of the debugging sections we are interested
   in.  */

static void
dwarf2_locate_sections (bfd *ignore_abfd, asection *sectp, void *ignore_ptr)
{
  if (strcmp (sectp->name, INFO_SECTION) == 0)
    {
      dwarf2_per_objfile->info_size = bfd_get_section_size (sectp);
      dwarf_info_section = sectp;
    }
  else if (strcmp (sectp->name, ABBREV_SECTION) == 0)
    {
      dwarf2_per_objfile->abbrev_size = bfd_get_section_size (sectp);
      dwarf_abbrev_section = sectp;
    }
  else if (strcmp (sectp->name, LINE_SECTION) == 0)
    {
      dwarf2_per_objfile->line_size = bfd_get_section_size (sectp);
      dwarf_line_section = sectp;
    }
  else if (strcmp (sectp->name, PUBNAMES_SECTION) == 0)
    {
      dwarf2_per_objfile->pubnames_size = bfd_get_section_size (sectp);
      dwarf_pubnames_section = sectp;
    }
  /* APPLE LOCAL: pubtypes */
  else if (strcmp (sectp->name, PUBTYPES_SECTION) == 0)
    {
      dwarf2_per_objfile->pubtypes_size = bfd_get_section_size (sectp);
      dwarf_pubtypes_section = sectp;
    }
  /* END APPLE LOCAL */
  /* APPLE LOCAL begin debug inlined section  */
  else if (strcmp (sectp->name, INLINED_SECTION) == 0)
    {
      dwarf2_per_objfile->inlined_size = bfd_get_section_size (sectp);
      dwarf_inlined_section = sectp;
    }
  /* APPLE LOCAL end debug inlined section */
  else if (strcmp (sectp->name, ARANGES_SECTION) == 0)
    {
      dwarf2_per_objfile->aranges_size = bfd_get_section_size (sectp);
      dwarf_aranges_section = sectp;
    }
  else if (strcmp (sectp->name, LOC_SECTION) == 0)
    {
      dwarf2_per_objfile->loc_size = bfd_get_section_size (sectp);
      dwarf_loc_section = sectp;
    }
  else if (strcmp (sectp->name, MACINFO_SECTION) == 0)
    {
      dwarf2_per_objfile->macinfo_size = bfd_get_section_size (sectp);
      dwarf_macinfo_section = sectp;
    }
  else if (strcmp (sectp->name, STR_SECTION) == 0)
    {
      dwarf2_per_objfile->str_size = bfd_get_section_size (sectp);
      dwarf_str_section = sectp;
    }
  else if (strcmp (sectp->name, FRAME_SECTION) == 0)
    {
      dwarf2_per_objfile->frame_size = bfd_get_section_size (sectp);
      dwarf_frame_section = sectp;
    }
  else if (strcmp (sectp->name, EH_FRAME_SECTION) == 0)
    {
      flagword aflag = bfd_get_section_flags (ignore_abfd, sectp);
      if (aflag & SEC_HAS_CONTENTS)
        {
          dwarf2_per_objfile->eh_frame_size = bfd_get_section_size (sectp);
          dwarf_eh_frame_section = sectp;
        }
    }
  else if (strcmp (sectp->name, RANGES_SECTION) == 0)
    {
      dwarf2_per_objfile->ranges_size = bfd_get_section_size (sectp);
      dwarf_ranges_section = sectp;
    }
}

/* APPLE LOCAL debug map pull part of dwarf2_build_psymtabs() out into
   this function which is called while expanding a dwarf debug map
   psymtab.  */

static void
dwarf2_copy_dwarf_from_file (struct objfile *objfile, bfd *abfd)
{
  /* We definitely need the .debug_info and .debug_abbrev sections */

  dwarf2_per_objfile->info_buffer = dwarf2_read_section (objfile, abfd, 
                                                         dwarf_info_section);
  dwarf2_per_objfile->abbrev_buffer = dwarf2_read_section (objfile, abfd, 
                                                          dwarf_abbrev_section);

  if (dwarf_line_section)
    dwarf2_per_objfile->line_buffer = dwarf2_read_section (objfile, abfd, 
                                                            dwarf_line_section);
  else
    dwarf2_per_objfile->line_buffer = NULL;

  if (dwarf_str_section)
    dwarf2_per_objfile->str_buffer = dwarf2_read_section (objfile, abfd, 
                                                          dwarf_str_section);
  else
    dwarf2_per_objfile->str_buffer = NULL;

  if (dwarf_macinfo_section)
    dwarf2_per_objfile->macinfo_buffer = dwarf2_read_section (objfile,
						abfd, dwarf_macinfo_section);
  else
    dwarf2_per_objfile->macinfo_buffer = NULL;

  if (dwarf_ranges_section)
    dwarf2_per_objfile->ranges_buffer = dwarf2_read_section (objfile, abfd, 
                                                          dwarf_ranges_section);
  else
    dwarf2_per_objfile->ranges_buffer = NULL;

  if (dwarf_loc_section)
    dwarf2_per_objfile->loc_buffer = dwarf2_read_section (objfile, abfd, 
                                                          dwarf_loc_section);
  else
    dwarf2_per_objfile->loc_buffer = NULL;

  /* APPLE LOCAL begin debug inlined section  */
  if (dwarf_inlined_section)
    dwarf2_per_objfile->inlined_buffer = dwarf2_read_section (objfile, abfd,
							      dwarf_inlined_section);
  else
    dwarf2_per_objfile->inlined_buffer = NULL;
  /* APPLE LOCAL end debug inlined section  */
}

/* Build a partial symbol table.  */

void
dwarf2_build_psymtabs (struct objfile *objfile, int mainline)
{
  /* APPLE LOCAL: Separate out the part of this function that copies in
     the dwarf sections.  */
  dwarf2_copy_dwarf_from_file (objfile, objfile->obfd);

  if (mainline
      || (objfile->global_psymbols.size == 0
	  && objfile->static_psymbols.size == 0))
    {
      init_psymbol_list (objfile, 1024);
    }

#if 0
  if (dwarf_aranges_offset && dwarf_pubnames_offset)
    {
      /* Things are significantly easier if we have .debug_aranges and
         .debug_pubnames sections */

      dwarf2_build_psymtabs_easy (objfile, mainline);
    }
  else
#endif
    /* only test this case for now */
    {
      /* In this case we have to work a bit harder */
      dwarf2_build_psymtabs_hard (objfile, mainline);
    }
}

/* APPLE LOCAL begin debug inlined section  */

/* Run from bfd_map_over_sections, finds the debug_inlined section.  */

static bfd_boolean
find_debug_inlined_section (bfd *ignore_abfd, asection *sectp, void *ignore)
{
  if (sectp->name
      && strcmp (sectp->name, INLINED_SECTION) == 0)
    {
      return 1;
    }
  else
    return 0;
}

/* Run from bfd_map_over_sections, finds the debug_inlined section.  */

static bfd_boolean
find_debug_str_section (bfd *ignore_abfd, asection *sectp, void *ignore)
{
  if (sectp->name
      && strcmp (sectp->name, STR_SECTION) == 0)
    {
      return 1;
    }
  else
    return 0;
}



/* Scan the debug inlined section stored in the OSO file for PST,
   and add any inlined subroutines you find there to the global
   pattial symbols for OBJFILE for this PST.  If we had to open an
   archive to fetch out the symbols for PST, we return the BFD for
   that archive  */

void
dwarf2_scan_inlined_section_for_psymbols (struct partial_symtab *pst,
					  struct objfile *objfile,
					  enum language psymtab_language)
{
  struct bfd *abfd;
  int cached;
  asection *inlined_section = NULL;
  bfd_window inlined_window;
  char *inlined_data, *inlined_ptr;
  bfd_size_type inlined_size;

  asection *str_section = NULL;
  bfd_window str_window;
  char *str_data;
  bfd_size_type str_size;

  int noerr;
  int bytes_read;
  struct cleanup *timing_cleanup;
  static int timer = -1;
  struct dwarf2_cu fake_cu;

  /* FIXME temporary workaround for debug_inlined sections that cause gdb
     to provide incorrect backtraces, v. <rdar://problem/6771834> 
     jmolenda/2009-04-08  */
  return;

  if (maint_use_timers)
    timing_cleanup = start_timer (&timer, "debug_inlined", 
				  PSYMTAB_OSO_NAME (pst));

  if (PSYMTAB_OSO_NAME (pst) == NULL || pst->readin)
    return;

  abfd = open_bfd_from_oso (pst, &cached);
  if (abfd == NULL)
    {
      warning ("Couldnot open OSO file %s "
	       "to scan for inlined section for objfile %s\n",
	       PSYMTAB_OSO_NAME (pst),
	       objfile->name ? objfile->name : "<unknown>");
      if (maint_use_timers)
	do_cleanups (timing_cleanup);
      return;
    }

  if (!bfd_check_format (abfd, bfd_object))
    {
      warning ("Not in bfd_object form");
      goto close_bfd;
    }

  inlined_section = bfd_sections_find_if (abfd, find_debug_inlined_section, 
					  NULL);

  if (inlined_section == NULL)
    goto close_bfd;

  str_section = bfd_sections_find_if (abfd, find_debug_str_section, NULL);

  if (str_section == NULL)
    goto close_bfd;

  inlined_size = bfd_get_section_size (inlined_section);
  str_size = bfd_get_section_size (str_section);

  bfd_init_window (&(inlined_window));
  noerr = bfd_get_section_contents_in_window_with_mode
    (abfd, inlined_section, &inlined_window, 0, inlined_size, 0);
  if (!noerr)
    {
      bfd_free_window (&inlined_window);
      bfd_close (abfd);
      perror_with_name (PSYMTAB_OSO_NAME (pst));
    }
  inlined_data = inlined_window.data;

  bfd_init_window (&(str_window));
  noerr = bfd_get_section_contents_in_window_with_mode
    (abfd, str_section, &str_window, 0, str_size, 0);
  if (!noerr)
    {
      bfd_free_window (&str_window);
      bfd_close (abfd);
      perror_with_name (PSYMTAB_OSO_NAME (pst));
    }
  str_data = str_window.data;

  /* Now read through the data.  First pick off the header.  */

  fake_cu.header.initial_length_size = 0;
  fake_cu.header.signed_addr_p = 0;
  inlined_ptr = inlined_data;

  while ((inlined_ptr - inlined_data) < inlined_size)
    {
      LONGEST length, total_bytes_read;
      unsigned char version;
      unsigned int addr_size;

      total_bytes_read = 0;

      length = read_initial_length (abfd, inlined_ptr,
				    &(fake_cu.header), &bytes_read);
      inlined_ptr += bytes_read;
      total_bytes_read += bytes_read;

      version = read_2_bytes (abfd, inlined_ptr);
      inlined_ptr += 2;
      total_bytes_read += 2;

      addr_size = read_1_byte (abfd, inlined_ptr);
      inlined_ptr += 1;
      total_bytes_read += 1;

      fake_cu.header.addr_size = addr_size;

      /* Keep reading entries until we have read the length of
	 the debug_inlined section.  */

      /* APPLE LOCAL begin radar 6568709  */
      if (addr_size != TARGET_ADDRESS_BYTES)
	{
	  /* There's something wrong with the address sizes; don't
	     attempt to read the rest of this section, just move the
	     pointer to the end.  */
	  total_bytes_read = length;
	  inlined_ptr += inlined_size;
  	  warning ("Unable to read debug info section for inlined subroutines.\n");
	}
      /* APPLE LOCAL end radar 6568709  */

      while (total_bytes_read < length)
	{
	  char *mips_name;
	  char *fn_name;
	  unsigned int num_entries;
	  unsigned int i;
          LONGEST str_offset;

	  /* Read one entry in the debug_inlined section.  An
	     entry consists of the die offset for the origin die,
	     the MIPS linkage name for the function, the regular name
	     for the function, the number of times the function was
	     inlined, followed by pairs of {low_pc address, die offset}
	     for each time the subroutine was inlined.  */

          str_offset = read_offset (abfd, inlined_ptr, &(fake_cu.header), 
                                    &bytes_read);
          if (str_offset >= str_size)
            error ("W_FORM_strp pointing outside of .debug_str section [in module %s]", bfd_get_filename (abfd));
          mips_name = str_data + str_offset;
	  inlined_ptr += bytes_read;
	  total_bytes_read += bytes_read;

          str_offset = read_offset (abfd, inlined_ptr, &(fake_cu.header), 
                                    &bytes_read);
          if (str_offset >= str_size)
            error ("W_FORM_strp pointing outside of .debug_str section [in module %s]", bfd_get_filename (abfd));
          fn_name = str_data + str_offset;
	  inlined_ptr += bytes_read;
	  total_bytes_read += bytes_read;

	  num_entries = read_unsigned_leb128 (abfd, inlined_ptr, 
					      (unsigned int *) &bytes_read);
	  inlined_ptr += bytes_read;
	  total_bytes_read += bytes_read;
	  
	  for (i = 0; i < num_entries; i++)
	    {
	      CORE_ADDR low_pc;
	      CORE_ADDR die_offset;

	      die_offset = read_offset (abfd, inlined_ptr, &(fake_cu.header),
					&bytes_read);
	      inlined_ptr += bytes_read;
	      total_bytes_read += bytes_read;

	      low_pc = read_address (abfd, inlined_ptr, &fake_cu,
				     &bytes_read);
	      inlined_ptr += bytes_read;
	      total_bytes_read += bytes_read;

	      /* Note:  The low_pc address here is *exactly* the one
		 the compiler generated; it has NOT been fixed up,
		 relinked, shifted, run through the debug_map or
		 anything else.  Therefore you should NOT use it
		 as a real address, at this point (unless you also
		 add the code to fix it up properly).  */

	      /* APPLE LOCAL begin radar 6600806  */
	      if (mips_name)
		{
		  /* It is possible to have inlined records without
		     function names, in which case we do not want to
		     try to create symbols for them.  */
		  add_psymbol_to_list (mips_name, strlen (mips_name),
				       VAR_DOMAIN, LOC_BLOCK,
				       &objfile->global_psymbols,
				       0, low_pc, 
				       psymtab_language, objfile);
		}
	      /* APPLE LOCAL end radar 6600806  */

	    }
	}
    }

  bfd_free_window (&inlined_window);
  bfd_free_window (&str_window);

 close_bfd:
      /* If we cached the bfd, we'll let our caller clean
	 it up, otherwise close the bfd here.  */
  if (!cached)
    close_bfd_or_archive (abfd);

  if (maint_use_timers)
    do_cleanups (timing_cleanup);
}
/* APPLE LOCAL end debug inlined section  */

/* Run from bfd_map_over_sections, finds the pubtypes section.  */

static bfd_boolean
find_pubtypes (bfd *ignore_abfd, asection *sectp, void *ignore)
{
  if (sectp->name 
      && strcmp (sectp->name, PUBTYPES_SECTION) == 0)
    {
      return 1;
    }
  else 
    return 0;
}

/* Scan the pubtype table stored in the OSO file for PST, and
   add any types you find there to the global partial symbols
   for OBJFILE for this PST.  If we had to open an archive to
   fetch out the symbols for PST, we return the BFD for that
   archive.  */

void
dwarf2_scan_pubtype_for_psymbols (struct partial_symtab *pst, 
			   struct objfile *objfile,
			   enum language psymtab_language)
{
  struct bfd *abfd;
  int cached;
  asection *pubtypes_section = NULL;
  bfd_window pubtypes_window;
  char *pubtypes_data, *pubtypes_ptr;
  struct comp_unit_head fake_cu_header; /* This is just to pass info to
					   read_offset.  */
  bfd_size_type pubtypes_size;
  int noerr;
  int bytes_read;
  struct cleanup *timing_cleanup;
  static int timer = -1;

  if (maint_use_timers)
    timing_cleanup = start_timer (&timer, "pubtypes", PSYMTAB_OSO_NAME (pst));

  if (PSYMTAB_OSO_NAME (pst) == NULL || pst->readin)
    return;

  abfd = open_bfd_from_oso (pst, &cached);
  if (abfd == NULL)
    {
      warning ("Could not open OSO file %s "
	       "to scan for pubtypes for objfile %s", 
	       PSYMTAB_OSO_NAME (pst), 
	       objfile->name ? objfile->name : "<unknown>");
      if (maint_use_timers)
	do_cleanups (timing_cleanup);
      return;
    }

  if (!bfd_check_format (abfd, bfd_object))
    {
      warning ("Not in bfd_object form");
      goto close_bfd;
    }

  pubtypes_section = bfd_sections_find_if (abfd, find_pubtypes, NULL);
  if (pubtypes_section == NULL)
    {
      goto close_bfd;
    }

  /* mmap in the pubtypes section.  
     FIXME: Insert a no-mmap version...   */

  pubtypes_size = bfd_get_section_size (pubtypes_section);

#ifdef HAVE_MMAP
  bfd_init_window (&(pubtypes_window));
  noerr = bfd_get_section_contents_in_window_with_mode
    (abfd, pubtypes_section, &pubtypes_window, 0, pubtypes_size, 0);
  if (!noerr)
    {
      bfd_free_window (&pubtypes_window);
      bfd_close (abfd);
      perror_with_name (PSYMTAB_OSO_NAME (pst));
    }
  pubtypes_data = pubtypes_window.data;
#else
  pubtypes_data = xmalloc (pubtypes_size + 1);
  noerr = bfd_get_section_contents (abfd, pubtypes_section, pubtypes_data, 0, pubtypes_size);
  if (!noerr)
    {
      xfree (pubtypes_data);
      bfd_close (abfd);
      perror_with_name (PSYMTAB_OSO_NAME (pst));
    }
#endif  /* HAVE_MMAP */

  /* Now read through the data.  First pick off the header
     then grab all the type strings.  We discard the pointers
     into the debug info here, since we are just going to use
     these to force psymtab->symtab readin.  */

  fake_cu_header.initial_length_size = 0;
  pubtypes_ptr = pubtypes_data;

  /* The pubtypes header is one or more sets of data.  Each set has a 
     header (which we aren't going to do anything with here) and then
     a pairs of {offset, type string} terminated by a O offset.  */

  while ((pubtypes_ptr - pubtypes_data) < pubtypes_size)
    {
      /* So here we read in the header.  */
      LONGEST length, info_offset, info_length;
      unsigned char version;

      length = read_initial_length (abfd, pubtypes_ptr, 
				    &fake_cu_header, &bytes_read);
      pubtypes_ptr += bytes_read;
      version = read_2_bytes (abfd, pubtypes_ptr);
      pubtypes_ptr += 2;
      info_offset = read_offset (abfd, pubtypes_ptr, &fake_cu_header, &bytes_read);
      pubtypes_ptr += bytes_read;
      info_length = read_offset (abfd, pubtypes_ptr, &fake_cu_header, &bytes_read);
      pubtypes_ptr += bytes_read;

      /* And here we read in the type strings and add them to the psymtab we were
	 passed in.  */

      while (1)
	{
	  char *type_name;
	  LONGEST offset;
	  unsigned int type_name_length;

	  offset = read_offset (abfd, pubtypes_ptr, &fake_cu_header, &bytes_read);
	  pubtypes_ptr += bytes_read;
	  if (offset == 0)
	    break;

	  /* read_string returns the length of the string WITH the null.  */
	  type_name = read_string (abfd, pubtypes_ptr, &type_name_length);
	  pubtypes_ptr += type_name_length;
	  
	  if (type_name != NULL)
	    {
	      /* We can't tell whether the name we've been given is a 
		 typedef or a tag name.  So add it to both.  Yuck...
		 But we want this scan to be really fast, so we don't
		 want to have to peek into the .debug_info.  */
	      /* Also, when we build psymtabs by scanning the actual
		 DWARF, we put the types in the global psymtabs for C++,
		 and the local one for C.  Since we can't tell the language
		 from the pubtypes table, we have to just choose one.
		 Putting it in the global table makes "ptype struct foo"
		 work correctly, so let's do that.  */

	      add_psymbol_to_list (type_name, type_name_length - 1,
				   STRUCT_DOMAIN, LOC_TYPEDEF,
				   &objfile->global_psymbols,
				   0, 0,
				   psymtab_language, objfile);
              /* APPLE LOCAL: Put the VAR_DOMAIN sym in the global psymbol
                 list as well. */
	      add_psymbol_to_list (type_name, type_name_length - 1,
				   VAR_DOMAIN, LOC_TYPEDEF,
				   &objfile->global_psymbols,
				   0, 0,
				   psymtab_language, objfile);
	    }	      

	}
    }
#ifdef HAVE_MMAP
  bfd_free_window (&pubtypes_window);
#else
      xfree (pubtypes_data);
#endif

 close_bfd:
      /* If we cached the bfd, we'll let our caller clean
	 it up, otherwise close the bfd here.  */
  if (!cached)
    close_bfd_or_archive (abfd);

  if (maint_use_timers)
    do_cleanups (timing_cleanup);
}

#if 0
/* Build the partial symbol table from the information in the
   .debug_pubnames and .debug_aranges sections.  */

static void
dwarf2_build_psymtabs_easy (struct objfile *objfile, int mainline)
{
  bfd *abfd = objfile->obfd;
  char *aranges_buffer, *pubnames_buffer;
  char *aranges_ptr, *pubnames_ptr;
  unsigned int entry_length, version, info_offset, info_size;

  /* APPLE LOCAL debug map: pass the bfd explicitly.  */
  pubnames_buffer = dwarf2_read_section (objfile, objfile->ofd,
					 dwarf_pubnames_section);
  pubnames_ptr = pubnames_buffer;
  while ((pubnames_ptr - pubnames_buffer) < dwarf2_per_objfile->pubnames_size)
    {
      struct comp_unit_head cu_header;
      int bytes_read;

      entry_length = read_initial_length (abfd, pubnames_ptr, &cu_header,
                                         &bytes_read);
      pubnames_ptr += bytes_read;
      version = read_1_byte (abfd, pubnames_ptr);
      pubnames_ptr += 1;
      info_offset = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
      info_size = read_4_bytes (abfd, pubnames_ptr);
      pubnames_ptr += 4;
    }

  /* APPLE LOCAL debug map: pass the bfd explicitly.  */
  aranges_buffer = dwarf2_read_section (objfile, objfile->ofd,
					dwarf_aranges_section);

}
#endif

/* Read in the comp unit header information from the debug_info at
   info_ptr.  */

static char *
read_comp_unit_head (struct comp_unit_head *cu_header,
		     char *info_ptr, bfd *abfd)
{
  /* APPLE LOCAL avoid unused var warning.  */
  /* int signed_addr;  */
  int bytes_read;
  cu_header->length = read_initial_length (abfd, info_ptr, cu_header,
                                           &bytes_read);
  info_ptr += bytes_read;
  cu_header->version = read_2_bytes (abfd, info_ptr);
  info_ptr += 2;
  cu_header->abbrev_offset = read_offset (abfd, info_ptr, cu_header,
                                          &bytes_read);
  info_ptr += bytes_read;
  cu_header->addr_size = read_1_byte (abfd, info_ptr);
  info_ptr += 1;
  /* APPLE LOCAL dwarf2 (using a non-elf file, duh) */
  cu_header->signed_addr_p = 0;
  return info_ptr;
}

static char *
partial_read_comp_unit_head (struct comp_unit_head *header, char *info_ptr,
			     bfd *abfd)
{
  char *beg_of_comp_unit = info_ptr;

  info_ptr = read_comp_unit_head (header, info_ptr, abfd);

  if (header->version != 2)
    error (_("Dwarf Error: wrong version in compilation unit header "
	   "(is %d, should be %d) [in module %s]"), header->version,
	   2, bfd_get_filename (abfd));

  if (header->abbrev_offset >= dwarf2_per_objfile->abbrev_size)
    error (_("Dwarf Error: bad offset (0x%lx) in compilation unit header "
	   "(offset 0x%lx + 6) [in module %s]"),
	   (long) header->abbrev_offset,
	   (long) (beg_of_comp_unit - dwarf2_per_objfile->info_buffer),
	   bfd_get_filename (abfd));

  if (beg_of_comp_unit + header->length + header->initial_length_size
      > dwarf2_per_objfile->info_buffer + dwarf2_per_objfile->info_size)
    error (_("Dwarf Error: bad length (0x%lx) in compilation unit header "
	   "(offset 0x%lx + 0) [in module %s]"),
	   (long) header->length,
	   (long) (beg_of_comp_unit - dwarf2_per_objfile->info_buffer),
	   bfd_get_filename (abfd));

  return info_ptr;
}

/* Allocate a new partial symtab for file named NAME and mark this new
   partial symtab as being an include of PST.  */

static void
dwarf2_create_include_psymtab (char *name, struct partial_symtab *pst,
                               struct objfile *objfile)
{
  struct partial_symtab *subpst = allocate_psymtab (name, objfile);

  subpst->section_offsets = pst->section_offsets;
  subpst->textlow = 0;
  subpst->texthigh = 0;

  subpst->dependencies = (struct partial_symtab **)
    obstack_alloc (&objfile->objfile_obstack,
                   sizeof (struct partial_symtab *));
  subpst->dependencies[0] = pst;
  subpst->number_of_dependencies = 1;

  subpst->globals_offset = 0;
  subpst->n_global_syms = 0;
  subpst->statics_offset = 0;
  subpst->n_static_syms = 0;
  subpst->symtab = NULL;
  subpst->read_symtab = pst->read_symtab;
  subpst->readin = 0;

  /* No private part is necessary for include psymtabs.  This property
     can be used to differentiate between such include psymtabs and
     the regular ones.  */
  subpst->read_symtab_private = NULL;
  /* APPLE LOCAL psym equivalences - Initialize field.   */
  subpst->equiv_psyms = NULL;
}

/* Read the Line Number Program data and extract the list of files
   included by the source file represented by PST.  Build an include
   partial symtab for each of these included files.
   
   This procedure assumes that there *is* a Line Number Program in
   the given CU.  Callers should check that PDI->HAS_STMT_LIST is set
   before calling this procedure.  */

static void
dwarf2_build_include_psymtabs (struct dwarf2_cu *cu,
                               struct partial_die_info *pdi,
                               struct partial_symtab *pst)
{
  struct objfile *objfile = cu->objfile;
  bfd *abfd = objfile->obfd;
  struct line_header *lh;

  lh = dwarf_decode_line_header (pdi->line_offset, abfd, cu);
  if (lh == NULL)
    return;  /* No linetable, so no includes.  */

  dwarf_decode_lines (lh, NULL, abfd, cu, pst);

  free_line_header (lh);
}

/* APPLE LOCAL begin dwarf repository  */
static int open_dwarf_repository (char *, char *, struct objfile *, 
				  struct dwarf2_cu *);
static void dwarf2_read_repository_abbrevs (struct dwarf2_cu *);
static struct dwarf2_cu *build_dummy_cu (struct objfile *, struct dwarf2_cu *);
static struct objfile *build_dummy_objfile (struct objfile *);
static void read_in_db_abbrev_table (struct abbrev_info **, sqlite3 *);
static void db_error (char *, char *, sqlite3 *);
static struct die_info *db_lookup_type (int , sqlite3 *, struct abbrev_info *);
static void fill_in_die_info (struct die_info *, int, uint8_t *,  uint8_t *, 
			      struct abbrev_info *, sqlite3 *);
static uint32_t get_uleb128 (uint8_t **);
static uint8_t *db_read_attribute_value (struct attribute *, unsigned, 
					 uint8_t **);
static struct die_info *follow_db_ref (struct die_info *, struct attribute *, 
				       struct dwarf2_cu *);
static void set_repository_cu_language (unsigned int, struct dwarf2_cu *);
static struct attribute *get_repository_name (struct attribute *, 
					      struct dwarf2_cu *);
static int finalize_stmts (sqlite3 *);
static struct database_info *find_open_repository (sqlite3 *);

enum db_status { DB_UNKNOWN, DB_OPEN, DB_ABBREVS_LOADED, DB_CLOSED };

struct objfile_list_node {
  struct objfile *ofile;
  struct objfile_list_node *next;
};

struct database_info {
  char *fullname;
  struct abbrev_info *abbrev_table;
  enum db_status current_status;
  struct rb_tree_node *db_types;
  struct objfile_list_node *num_uses;
  struct dwarf2_cu *dummy_cu;
  struct objfile *dummy_objfile;
  sqlite3 *db;
};

static void increment_use_count (struct database_info *, struct objfile *);
static void decrement_use_count (struct database_info *, struct objfile *);

static int byte_swap_p;
/* APPLE LOCAL end dwarf repository */

/* APPLE LOCAL begin debug inlined section */
static void
scan_partial_inlined_function_symbols (struct dwarf2_cu *cu)
{
  const struct partial_symbol *psym = NULL;
  static struct objfile *current_psym_objfile = NULL;

  /* FIXME temporary workaround for debug_inlined sections that cause gdb
     to provide incorrect backtraces, v. <rdar://problem/6771834>
     jmolenda/2009-04-08  */
  return;

  /* Check current cu's objfile; if we've already read the debug_inlined
     section for it, just return.  */

  if (current_psym_objfile != cu->objfile)
    current_psym_objfile = cu->objfile;
  else
    return;

  if (dwarf2_per_objfile->inlined_buffer && dwarf2_per_objfile->inlined_size)
    {
      char *inlined_ptr;
      struct objfile *objfile = cu->objfile;
      bfd *abfd = objfile->obfd;
      CORE_ADDR low_pc;
      CORE_ADDR baseaddr;
      CORE_ADDR inlined_die_ref;
      unsigned num_instances = 0;
      unsigned bytes_read = 0;
      unsigned i;
      unsigned total_length;
      unsigned version;
      unsigned addr_size;
 
      char *fn_name;
      char *mips_name;
      struct comp_unit_head *cu_header = &cu->header;

      /* NOTE: Final format for each entry (after the section header),
         is: mips name, regular name, # of inlining instances,
         followed by a {die_ref, lowpc} pair for each inlined
         subroutine call. 

         The section header consists of:  The length of the section,
	 the DWARF version, and the size of an address.  */

      baseaddr = objfile_text_section_offset (objfile);

      inlined_ptr = dwarf2_per_objfile->inlined_buffer;

      total_length = read_initial_length (abfd, inlined_ptr, &cu->header, 
					  (int *) &bytes_read);
      inlined_ptr += bytes_read;

      version = read_2_bytes (abfd, inlined_ptr);
      inlined_ptr += 2;

      addr_size = read_1_byte (abfd, inlined_ptr);
      inlined_ptr += 1;

      /* APPLE LOCAL begin radar 6568709  */
      if (addr_size != cu_header->addr_size)
	{
	  /* There's something wrong with the address sizes; don't
	     attempt to read the rest of this section, just move the
	     pointer to the end.  */
	  inlined_ptr = dwarf2_per_objfile->inlined_buffer
	                + dwarf2_per_objfile->inlined_size;
	  warning ("Unable to read debug info section for inlined subroutines.\n");
	}
      /* APPLE LOCAL end radar 6568709  */

      while (inlined_ptr < (dwarf2_per_objfile->inlined_buffer
			    + dwarf2_per_objfile->inlined_size))
	{
	  mips_name = read_indirect_string (abfd, inlined_ptr, cu_header,
					  &bytes_read);
	  inlined_ptr += bytes_read;

	  fn_name = read_indirect_string (abfd, inlined_ptr, cu_header,
					  &bytes_read);
	  inlined_ptr += bytes_read;

	  num_instances = read_unsigned_leb128 (abfd, inlined_ptr, 
						&bytes_read);
	  inlined_ptr += bytes_read;

	  for (i = 0; i < num_instances; i++)
	    {
	      CORE_ADDR addr;

	      inlined_die_ref = read_offset (abfd, inlined_ptr, &cu->header,
					     (int *) &bytes_read);
	      inlined_ptr += bytes_read;

	      low_pc = read_address (abfd, inlined_ptr, cu, 
				     (int *) &bytes_read);
	      inlined_ptr += bytes_read;
	      
	      if (translate_debug_map_address (cu->addr_map , low_pc, &addr, 
					       0))
		low_pc = addr;

	      /* APPLE LOCAL begin radar 6600806  */
	      if (mips_name)
		{
		  /* It is possible for some records to not have names,
		     in which case we can't make symbols for them.  */
		  psym = add_psymbol_to_list (mips_name, strlen (mips_name),
					      VAR_DOMAIN, LOC_BLOCK,
					      &objfile->global_psymbols,
					      0, low_pc + baseaddr,
					      cu->language, objfile);
		  
		  if (cu->language == language_cplus
		      && cu->has_namespace_info == 0
		      && psym != NULL
		      && SYMBOL_CPLUS_DEMANGLED_NAME (psym) != NULL)
		    cp_check_possible_namespace_symbols (SYMBOL_CPLUS_DEMANGLED_NAME (psym),
							 objfile);
		}
	      /* APPLE LOCAL end radar 6600806  */
	    }
	}
    }
}
/* APPLE LOCAL end debug inlined section  */

/* Build the partial symbol table by doing a quick pass through the
   .debug_info and .debug_abbrev sections.  */

static void
dwarf2_build_psymtabs_hard (struct objfile *objfile, int mainline)
{
  /* Instead of reading this into a big buffer, we should probably use
     mmap()  on architectures that support it. (FIXME) */
  bfd *abfd = objfile->obfd;
  char *info_ptr;
  char *beg_of_comp_unit;
  struct partial_die_info comp_unit_die;
  struct partial_symtab *pst;
  struct cleanup *back_to;
  CORE_ADDR lowpc, highpc, baseaddr;

  /* APPLE LOCAL begin dwarf repository  */
  if (bfd_big_endian (abfd) == BFD_ENDIAN_BIG)
    byte_swap_p = 0;
  else
    byte_swap_p = 1;
  /* APPLE LOCAL end dwarf repository  */
  info_ptr = dwarf2_per_objfile->info_buffer;

  /* Any cached compilation units will be linked by the per-objfile
     read_in_chain.  Make sure to free them when we're done.  */
  back_to = make_cleanup (free_cached_comp_units, NULL);

  create_all_comp_units (objfile);

  /* Since the objects we're extracting from .debug_info vary in
     length, only the individual functions to extract them (like
     read_comp_unit_head and load_partial_die) can really know whether
     the buffer is large enough to hold another complete object.

     At the moment, they don't actually check that.  If .debug_info
     holds just one extra byte after the last compilation unit's dies,
     then read_comp_unit_head will happily read off the end of the
     buffer.  read_partial_die is similarly casual.  Those functions
     should be fixed.

     For this loop condition, simply checking whether there's any data
     left at all should be sufficient.  */
  while (info_ptr < (dwarf2_per_objfile->info_buffer
		     + dwarf2_per_objfile->info_size))
    {
      struct cleanup *back_to_inner;
      struct dwarf2_cu cu;
      struct abbrev_info *abbrev;
      unsigned int bytes_read;
      struct dwarf2_per_cu_data *this_cu;

      beg_of_comp_unit = info_ptr;

      memset (&cu, 0, sizeof (cu));

      obstack_init (&cu.comp_unit_obstack);

      back_to_inner = make_cleanup (free_stack_comp_unit, &cu);

      cu.objfile = objfile;
      info_ptr = partial_read_comp_unit_head (&cu.header, info_ptr, abfd);

      /* Complete the cu_header */
      cu.header.offset = beg_of_comp_unit - dwarf2_per_objfile->info_buffer;
      cu.header.first_die_ptr = info_ptr;
      cu.header.cu_head_ptr = beg_of_comp_unit;

      cu.list_in_scope = &file_symbols;

      /* Read the abbrevs for this compilation unit into a table */
      dwarf2_read_abbrevs (abfd, &cu);
      make_cleanup (dwarf2_free_abbrev_table, &cu);

      this_cu = dwarf2_find_comp_unit (cu.header.offset, objfile);

      /* Read the compilation unit die */
      /* APPLE LOCAL Add cast to avoid type mismatch in arg2 warning.  */
      abbrev = peek_die_abbrev (info_ptr, (int *) &bytes_read, &cu);
      info_ptr = read_partial_die (&comp_unit_die, abbrev, bytes_read,
				   abfd, info_ptr, &cu);
      /* APPLE LOCAL begin dwarf repository  */
      if (comp_unit_die.has_repository)
	{
	  dwarf2_read_repository_abbrevs (&cu);
	  set_repository_cu_language (comp_unit_die.language, &cu);
	}
      /* APPLE LOCAL end dwarf repository  */

      /* Set the language we're debugging */
      set_cu_language (comp_unit_die.language, &cu);

      /* Allocate a new partial symbol table structure */
      pst = start_psymtab_common (objfile, objfile->section_offsets,
				  comp_unit_die.name ? comp_unit_die.name : "",
				  comp_unit_die.lowpc,
				  objfile->global_psymbols.next,
				  objfile->static_psymbols.next);

      if (comp_unit_die.dirname)
	pst->dirname = xstrdup (comp_unit_die.dirname);

      pst->read_symtab_private = (char *) this_cu;

      baseaddr = objfile_text_section_offset (objfile);

      /* Store the function that reads in the rest of the symbol table */
      pst->read_symtab = dwarf2_psymtab_to_symtab;

      /* If this compilation unit was already read in, free the
	 cached copy in order to read it in again.  This is
	 necessary because we skipped some symbols when we first
	 read in the compilation unit (see load_partial_dies).
	 This problem could be avoided, but the benefit is
	 unclear.  */
      if (this_cu->cu != NULL)
	free_one_cached_comp_unit (this_cu->cu);

      cu.per_cu = this_cu;

      /* Note that this is a pointer to our stack frame, being
	 added to a global data structure.  It will be cleaned up
	 in free_stack_comp_unit when we finish with this
	 compilation unit.  */
      this_cu->cu = &cu;

      this_cu->psymtab = pst;

      /* Check if comp unit has_children.
         If so, read the rest of the partial symbols from this comp unit.
         If not, there's no more debug_info for this comp unit. */
      if (comp_unit_die.has_children)
	{
	  struct partial_die_info *first_die;
	  /* APPLE LOCAL psym equivalences  */
	  struct equiv_psym_list *equiv_psyms = NULL;

	  lowpc = ((CORE_ADDR) -1);
	  highpc = ((CORE_ADDR) 0);

	  first_die = load_partial_dies (abfd, info_ptr, 1, &cu);

	  /* APPLE LOCAL begin psym equivalences  */
	  scan_partial_symbols (first_die, &lowpc, &highpc, &cu,
				&equiv_psyms);

	  /* APPLE LOCAL debug inlined section  */
	  scan_partial_inlined_function_symbols (&cu);

	  pst->equiv_psyms = equiv_psyms;
	  /* APPLE LOCAL end psym equivalences  */

	  /* If we didn't find a lowpc, set it to highpc to avoid
	     complaints from `maint check'.  */
	  if (lowpc == ((CORE_ADDR) -1))
	    lowpc = highpc;

	  /* If the compilation unit didn't have an explicit address range,
	     then use the information extracted from its child dies.  */
	  if (! comp_unit_die.has_pc_info)
	    {
	      comp_unit_die.lowpc = lowpc;
	      comp_unit_die.highpc = highpc;
	    }
	}
      pst->textlow = comp_unit_die.lowpc + baseaddr;
      pst->texthigh = comp_unit_die.highpc + baseaddr;

      pst->n_global_syms = objfile->global_psymbols.next -
	(objfile->global_psymbols.list + pst->globals_offset);
      pst->n_static_syms = objfile->static_psymbols.next -
	(objfile->static_psymbols.list + pst->statics_offset);
      sort_pst_symbols (pst);

      /* If there is already a psymtab or symtab for a file of this
         name, remove it. (If there is a symtab, more drastic things
         also happen.) This happens in VxWorks.  */
      free_named_symtabs (pst->filename);

      info_ptr = beg_of_comp_unit + cu.header.length
                                  + cu.header.initial_length_size;

      if (comp_unit_die.has_stmt_list)
        {
          /* Get the list of files included in the current compilation unit,
             and build a psymtab for each of them.  */
          dwarf2_build_include_psymtabs (&cu, &comp_unit_die, pst);
        }

      do_cleanups (back_to_inner);
    }
  do_cleanups (back_to);
}

/* Load the DIEs for a secondary CU into memory.  */

static void
load_comp_unit (struct dwarf2_per_cu_data *this_cu, struct objfile *objfile)
{
  bfd *abfd = objfile->obfd;
  char *info_ptr, *beg_of_comp_unit;
  struct partial_die_info comp_unit_die;
  struct dwarf2_cu *cu;
  struct abbrev_info *abbrev;
  unsigned int bytes_read;
  struct cleanup *back_to;

  info_ptr = dwarf2_per_objfile->info_buffer + this_cu->offset;
  beg_of_comp_unit = info_ptr;

  cu = xmalloc (sizeof (struct dwarf2_cu));
  memset (cu, 0, sizeof (struct dwarf2_cu));

  obstack_init (&cu->comp_unit_obstack);

  cu->objfile = objfile;
  info_ptr = partial_read_comp_unit_head (&cu->header, info_ptr, abfd);

  /* Complete the cu_header.  */
  cu->header.offset = beg_of_comp_unit - dwarf2_per_objfile->info_buffer;
  cu->header.first_die_ptr = info_ptr;
  cu->header.cu_head_ptr = beg_of_comp_unit;

  /* Read the abbrevs for this compilation unit into a table.  */
  dwarf2_read_abbrevs (abfd, cu);
  back_to = make_cleanup (dwarf2_free_abbrev_table, cu);

  /* Read the compilation unit die.  */
  /* APPLE LOCAL Add cast to avoid type mismatch in arg2 warning.  */
  abbrev = peek_die_abbrev (info_ptr, (int *) &bytes_read, cu);
  info_ptr = read_partial_die (&comp_unit_die, abbrev, bytes_read,
			       abfd, info_ptr, cu);

  /* Set the language we're debugging.  */
  set_cu_language (comp_unit_die.language, cu);

  /* Link this compilation unit into the compilation unit tree.  */
  this_cu->cu = cu;
  cu->per_cu = this_cu;

  /* Check if comp unit has_children.
     If so, read the rest of the partial symbols from this comp unit.
     If not, there's no more debug_info for this comp unit. */
  if (comp_unit_die.has_children)
    load_partial_dies (abfd, info_ptr, 0, cu);

  do_cleanups (back_to);
}

/* Create a list of all compilation units in OBJFILE.  We do this only
   if an inter-comp-unit reference is found; presumably if there is one,
   there will be many, and one will occur early in the .debug_info section.
   So there's no point in building this list incrementally.  */

static void
create_all_comp_units (struct objfile *objfile)
{
  int n_allocated;
  int n_comp_units;
  struct dwarf2_per_cu_data **all_comp_units;
  char *info_ptr = dwarf2_per_objfile->info_buffer;

  n_comp_units = 0;
  n_allocated = 10;
  all_comp_units = xmalloc (n_allocated
			    * sizeof (struct dwarf2_per_cu_data *));
  
  while (info_ptr < dwarf2_per_objfile->info_buffer + dwarf2_per_objfile->info_size)
    {
      struct comp_unit_head cu_header;
      /* APPLE LOCAL avoid unused var warning.  */
      /* char *beg_of_comp_unit; */
      struct dwarf2_per_cu_data *this_cu;
      unsigned long offset;
      int bytes_read;

      offset = info_ptr - dwarf2_per_objfile->info_buffer;

      /* Read just enough information to find out where the next
	 compilation unit is.  */
      cu_header.initial_length_size = 0;
      cu_header.length = read_initial_length (objfile->obfd, info_ptr,
					      &cu_header, &bytes_read);

      /* Save the compilation unit for later lookup.  */
      this_cu = obstack_alloc (&objfile->objfile_obstack,
			       sizeof (struct dwarf2_per_cu_data));
      memset (this_cu, 0, sizeof (*this_cu));
      this_cu->offset = offset;
      this_cu->length = cu_header.length + cu_header.initial_length_size;

      if (n_comp_units == n_allocated)
	{
	  n_allocated *= 2;
	  all_comp_units = xrealloc (all_comp_units,
				     n_allocated
				     * sizeof (struct dwarf2_per_cu_data *));
	}
      all_comp_units[n_comp_units++] = this_cu;

      info_ptr = info_ptr + this_cu->length;
    }

  dwarf2_per_objfile->all_comp_units
    = obstack_alloc (&objfile->objfile_obstack,
		     n_comp_units * sizeof (struct dwarf2_per_cu_data *));
  memcpy (dwarf2_per_objfile->all_comp_units, all_comp_units,
	  n_comp_units * sizeof (struct dwarf2_per_cu_data *));
  xfree (all_comp_units);
  dwarf2_per_objfile->n_comp_units = n_comp_units;
}

/* APPLE LOCAL begin psym equivalences  */
/* EQUIV_PSYMS is an array of equivalence psym names found in the
   current compile unit so far, along with some book-keeping
   information (size of the array, etc).  NAME is a new equivalence
   name to be added to the list.  This function makes sure the array
   is big enough, allocating memory and updating the book-keeping if
   necessary, and adds the new name to the array.  */

static void
add_equiv_psym (struct equiv_psym_list **equiv_psyms,
		char *name)
{
  int i;

  /* If the array has never been allocated, malloc it and start it out
     with 10 elements.  */

  if (*equiv_psyms == NULL)
    {
      *equiv_psyms = (struct equiv_psym_list *) xmalloc 
                                            (sizeof (struct equiv_psym_list));
      (*equiv_psyms)->sym_list = (char **) xmalloc (10 * sizeof (char *));
      (*equiv_psyms)->list_size = 10;
      (*equiv_psyms)->num_syms = 0;
      for (i = 0; i < 10; i++)
	(*equiv_psyms)->sym_list[i] = NULL;
    }

  /* If the array is full, double its size (using realloc).  */

  if ((*equiv_psyms)->num_syms == (*equiv_psyms)->list_size)
    {
      int new_size;
      new_size = (*equiv_psyms)->list_size * 2;
      (*equiv_psyms)->sym_list = (char **) xrealloc ((*equiv_psyms)->sym_list,
						     new_size * sizeof (char *));
      for (i = (*equiv_psyms)->list_size; i < new_size; i++)
	(*equiv_psyms)->sym_list[i] = NULL;
      (*equiv_psyms)->list_size = new_size;
    }

  /* Insert NAME into the list.  */

  (*equiv_psyms)->sym_list[(*equiv_psyms)->num_syms++] = name;
}
/* APPLE LOCAL end psym equivalences  */

/* Process all loaded DIEs for compilation unit CU, starting at FIRST_DIE.
   Also set *LOWPC and *HIGHPC to the lowest and highest PC values found
   in CU.  */

/* APPLE LOCAL begin psym equivalences  */
static void
scan_partial_symbols (struct partial_die_info *first_die, CORE_ADDR *lowpc,
		      CORE_ADDR *highpc, struct dwarf2_cu *cu,
		      struct equiv_psym_list **equiv_psyms)
/* APPLE LOCAL end psym equivalences  */
{
  /* APPLE LOCAL avoid unused var warnings.  */
  /* struct objfile *objfile = cu->objfile; */
  /* bfd *abfd = objfile->obfd; */
  struct partial_die_info *pdi;

  /* Now, march along the PDI's, descending into ones which have
     interesting children but skipping the children of the other ones,
     until we reach the end of the compilation unit.  */

  pdi = first_die;

  while (pdi != NULL)
    {
      fixup_partial_die (pdi, cu);

      /* Anonymous namespaces have no name but have interesting
	 children, so we need to look at them.  Ditto for anonymous
	 enums.  */

      if (pdi->name != NULL || pdi->tag == DW_TAG_namespace
	  || pdi->tag == DW_TAG_enumeration_type)
	{
	  switch (pdi->tag)
	    {
	    case DW_TAG_subprogram:
	      /* APPLE LOCAL begin psym equivalences  */
	      /* If the partial die has an equivalence name, add it to
		 the list for this compile unit.  */
	      if (pdi->equiv_name)
		add_equiv_psym (equiv_psyms, pdi->equiv_name);
	      /* APPLE LOCAL end psym equivalences  */
	      if (pdi->has_pc_info)
		{
		  if (pdi->lowpc < *lowpc)
		    {
		      *lowpc = pdi->lowpc;
		    }
		  if (pdi->highpc > *highpc)
		    {
		      *highpc = pdi->highpc;
		    }
		  if (!pdi->is_declaration)
		    {
		      add_partial_symbol (pdi, cu);
		    }
		}
	      break;
	    case DW_TAG_variable:
	    case DW_TAG_typedef:
	    case DW_TAG_union_type:
	      if (!pdi->is_declaration)
		{
		  add_partial_symbol (pdi, cu);
		}
	      break;
	    case DW_TAG_class_type:
	    case DW_TAG_structure_type:
	      if (!pdi->is_declaration)
		{
		  add_partial_symbol (pdi, cu);
		}
	      break;
	    case DW_TAG_enumeration_type:
	      if (!pdi->is_declaration)
		add_partial_enumeration (pdi, cu);
	      break;
	    case DW_TAG_base_type:
            case DW_TAG_subrange_type:
	      /* File scope base type definitions are added to the partial
	         symbol table.  */
	      add_partial_symbol (pdi, cu);
	      break;
	    case DW_TAG_namespace:
	      add_partial_namespace (pdi, lowpc, highpc, cu);
	      break;
	    default:
	      break;
	    }
	}

      /* If the die has a sibling, skip to the sibling.  */

      pdi = pdi->die_sibling;
    }
}

/* Functions used to compute the fully scoped name of a partial DIE.

   Normally, this is simple.  For C++, the parent DIE's fully scoped
   name is concatenated with "::" and the partial DIE's name.  For
   Java, the same thing occurs except that "." is used instead of "::".
   Enumerators are an exception; they use the scope of their parent
   enumeration type, i.e. the name of the enumeration type is not
   prepended to the enumerator.

   There are two complexities.  One is DW_AT_specification; in this
   case "parent" means the parent of the target of the specification,
   instead of the direct parent of the DIE.  The other is compilers
   which do not emit DW_TAG_namespace; in this case we try to guess
   the fully qualified name of structure types from their members'
   linkage names.  This must be done using the DIE's children rather
   than the children of any DW_AT_specification target.  We only need
   to do this for structures at the top level, i.e. if the target of
   any DW_AT_specification (if any; otherwise the DIE itself) does not
   have a parent.  */

/* Compute the scope prefix associated with PDI's parent, in
   compilation unit CU.  The result will be allocated on CU's
   comp_unit_obstack, or a copy of the already allocated PDI->NAME
   field.  NULL is returned if no prefix is necessary.  */
static char *
partial_die_parent_scope (struct partial_die_info *pdi,
			  struct dwarf2_cu *cu)
{
  char *grandparent_scope;
  struct partial_die_info *parent, *real_pdi;

  /* We need to look at our parent DIE; if we have a DW_AT_specification,
     then this means the parent of the specification DIE.  */

  real_pdi = pdi;
  while (real_pdi->has_specification)
    real_pdi = find_partial_die (real_pdi->spec_offset, cu);

  parent = real_pdi->die_parent;
  if (parent == NULL)
    return NULL;

  if (parent->scope_set)
    return parent->scope;

  fixup_partial_die (parent, cu);

  grandparent_scope = partial_die_parent_scope (parent, cu);

  if (parent->tag == DW_TAG_namespace
      || parent->tag == DW_TAG_structure_type
      || parent->tag == DW_TAG_class_type
      || parent->tag == DW_TAG_union_type)
    {
      if (grandparent_scope == NULL)
	parent->scope = parent->name;
      else
	parent->scope = typename_concat (&cu->comp_unit_obstack, grandparent_scope,
					 parent->name, cu);
    }
  else if (parent->tag == DW_TAG_enumeration_type)
    /* Enumerators should not get the name of the enumeration as a prefix.  */
    parent->scope = grandparent_scope;
  else
    {
      /* FIXME drow/2004-04-01: What should we be doing with
	 function-local names?  For partial symbols, we should probably be
	 ignoring them.  */
      complaint (&symfile_complaints,
		 _("unhandled containing DIE tag %d for DIE at %d"),
		 parent->tag, pdi->offset);
      parent->scope = grandparent_scope;
    }

  parent->scope_set = 1;
  return parent->scope;
}

/* Return the fully scoped name associated with PDI, from compilation unit
   CU.  The result will be allocated with malloc.  */
static char *
partial_die_full_name (struct partial_die_info *pdi,
		       struct dwarf2_cu *cu)
{
  char *parent_scope;

  parent_scope = partial_die_parent_scope (pdi, cu);
  if (parent_scope == NULL)
    return NULL;
  else
    return typename_concat (NULL, parent_scope, pdi->name, cu);
}

static void
add_partial_symbol (struct partial_die_info *pdi, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  CORE_ADDR addr = 0;
  char *actual_name;
  /* APPLE LOCAL avoid unused var warning.  */
  /* const char *my_prefix; */
  const struct partial_symbol *psym = NULL;
  CORE_ADDR baseaddr;
  int built_actual_name = 0;

  baseaddr = objfile_text_section_offset (objfile);

  actual_name = NULL;

  if (pdi_needs_namespace (pdi->tag))
    {
      actual_name = partial_die_full_name (pdi, cu);
      if (actual_name)
	built_actual_name = 1;
    }

  if (actual_name == NULL)
    actual_name = pdi->name;

  switch (pdi->tag)
    {
    case DW_TAG_subprogram:
      if (pdi->is_external)
	{
	  /*prim_record_minimal_symbol (actual_name, pdi->lowpc + baseaddr,
	     mst_text, objfile); */
	  psym = add_psymbol_to_list (actual_name, strlen (actual_name),
				      VAR_DOMAIN, LOC_BLOCK,
				      &objfile->global_psymbols,
				      0, pdi->lowpc + baseaddr,
				      cu->language, objfile);
	}
      else
	{
	  /*prim_record_minimal_symbol (actual_name, pdi->lowpc + baseaddr,
	     mst_file_text, objfile); */
	  psym = add_psymbol_to_list (actual_name, strlen (actual_name),
				      VAR_DOMAIN, LOC_BLOCK,
				      &objfile->static_psymbols,
				      0, pdi->lowpc + baseaddr,
				      cu->language, objfile);
	}
      break;
    case DW_TAG_variable:
      if (pdi->is_external)
	{
	  /* Global Variable.
	     Don't enter into the minimal symbol tables as there is
	     a minimal symbol table entry from the ELF symbols already.
	     Enter into partial symbol table if it has a location
	     descriptor or a type.
	     If the location descriptor is missing, new_symbol will create
	     a LOC_UNRESOLVED symbol, the address of the variable will then
	     be determined from the minimal symbol table whenever the variable
	     is referenced.
	     The address for the partial symbol table entry is not
	     used by GDB, but it comes in handy for debugging partial symbol
	     table building.  */

	  if (pdi->locdesc)
	    addr = decode_locdesc (pdi->locdesc, cu);
	  /* APPLE LOCAL - dwarf repository  */
	  if (pdi->locdesc || pdi->has_type || pdi->has_repository_type)
	    psym = add_psymbol_to_list (actual_name, strlen (actual_name),
					VAR_DOMAIN, LOC_STATIC,
					&objfile->global_psymbols,
					0, addr + baseaddr,
					cu->language, objfile);
	}
      else
	{
	  /* Static Variable. Skip symbols without location descriptors.  */
	  if (pdi->locdesc == NULL)
	    return;
	  addr = decode_locdesc (pdi->locdesc, cu);
	  /*prim_record_minimal_symbol (actual_name, addr + baseaddr,
	     mst_file_data, objfile); */
	  psym = add_psymbol_to_list (actual_name, strlen (actual_name),
				      VAR_DOMAIN, LOC_STATIC,
				      &objfile->static_psymbols,
				      0, addr + baseaddr,
				      cu->language, objfile);
	}
      break;
    case DW_TAG_typedef:
    case DW_TAG_base_type:
    case DW_TAG_subrange_type:
      /* APPLE LOCAL: Put it in the global_psymbols list, not 
         static_psymbols.  */
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   VAR_DOMAIN, LOC_TYPEDEF,
			   &objfile->global_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);
      break;
    case DW_TAG_namespace:
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   VAR_DOMAIN, LOC_TYPEDEF,
			   &objfile->global_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);
      break;
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_enumeration_type:
      /* Skip aggregate types without children, these are external
         references.  */
      /* NOTE: carlton/2003-10-07: See comment in new_symbol about
	 static vs. global.  */
      if (pdi->has_children == 0)
	return;
      /* APPLE LOCAL: Put it in the global_psymbols list, not 
         static_psymbols regardless of which language it came from.  */
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   STRUCT_DOMAIN, LOC_TYPEDEF,
			   &objfile->global_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);

      if (cu->language == language_cplus
          || cu->language == language_java)
	{
	  /* For C++ and Java, these implicitly act as typedefs as well. */
	  add_psymbol_to_list (actual_name, strlen (actual_name),
			       VAR_DOMAIN, LOC_TYPEDEF,
			       &objfile->global_psymbols,
			       0, (CORE_ADDR) 0, cu->language, objfile);
	}
      break;
    case DW_TAG_enumerator:
          /* APPLE LOCAL: Put it in the global_psymbols list regardless of
             language.  */
      add_psymbol_to_list (actual_name, strlen (actual_name),
			   VAR_DOMAIN, LOC_CONST,
			   &objfile->global_psymbols,
			   0, (CORE_ADDR) 0, cu->language, objfile);
      break;
    default:
      break;
    }

  /* Check to see if we should scan the name for possible namespace
     info.  Only do this if this is C++, if we don't have namespace
     debugging info in the file, if the psym is of an appropriate type
     (otherwise we'll have psym == NULL), and if we actually had a
     mangled name to begin with.  */

  /* FIXME drow/2004-02-22: Why don't we do this for classes, i.e. the
     cases which do not set PSYM above?  */

  if (cu->language == language_cplus
      && cu->has_namespace_info == 0
      && psym != NULL
      && SYMBOL_CPLUS_DEMANGLED_NAME (psym) != NULL)
    cp_check_possible_namespace_symbols (SYMBOL_CPLUS_DEMANGLED_NAME (psym),
					 objfile);

  if (built_actual_name)
    xfree (actual_name);
}

/* Determine whether a die of type TAG living in a C++ class or
   namespace needs to have the name of the scope prepended to the
   name listed in the die.  */

static int
pdi_needs_namespace (enum dwarf_tag tag)
{
  switch (tag)
    {
    case DW_TAG_namespace:
    case DW_TAG_typedef:
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_enumerator:
      return 1;
    default:
      return 0;
    }
}

/* Read a partial die corresponding to a namespace; also, add a symbol
   corresponding to that namespace to the symbol table.  NAMESPACE is
   the name of the enclosing namespace.  */

static void
add_partial_namespace (struct partial_die_info *pdi,
		       CORE_ADDR *lowpc, CORE_ADDR *highpc,
		       struct dwarf2_cu *cu)
{
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct objfile *objfile = cu->objfile; */
  /* APPLE LOCAL psym equivalences  */
  struct equiv_psym_list *equiv_psyms = NULL;

  /* Add a symbol for the namespace.  */

  add_partial_symbol (pdi, cu);

  /* Now scan partial symbols in that namespace.  */

  /* APPLE LOCAL begin psym equivalences  */
  if (pdi->has_children)
    scan_partial_symbols (pdi->die_child, lowpc, highpc, cu, &equiv_psyms);
  /* APPLE LOCAL end psym_equivalences  */
}

/* See if we can figure out if the class lives in a namespace.  We do
   this by looking for a member function; its demangled name will
   contain namespace info, if there is any.  */

static void
guess_structure_name (struct partial_die_info *struct_pdi,
		      struct dwarf2_cu *cu)
{
  if ((cu->language == language_cplus
       || cu->language == language_java)
      && cu->has_namespace_info == 0
      && struct_pdi->has_children)
    {
      /* NOTE: carlton/2003-10-07: Getting the info this way changes
	 what template types look like, because the demangler
	 frequently doesn't give the same name as the debug info.  We
	 could fix this by only using the demangled name to get the
	 prefix (but see comment in read_structure_type).  */

      struct partial_die_info *child_pdi = struct_pdi->die_child;
      struct partial_die_info *real_pdi;

      /* If this DIE (this DIE's specification, if any) has a parent, then
	 we should not do this.  We'll prepend the parent's fully qualified
         name when we create the partial symbol.  */

      real_pdi = struct_pdi;
      while (real_pdi->has_specification)
	real_pdi = find_partial_die (real_pdi->spec_offset, cu);

      if (real_pdi->die_parent != NULL)
	return;

      while (child_pdi != NULL)
	{
	  if (child_pdi->tag == DW_TAG_subprogram)
	    {
	      char *actual_class_name
		= language_class_name_from_physname (cu->language_defn,
						     child_pdi->name);
	      if (actual_class_name != NULL)
		{
		  struct_pdi->name
		    = obsavestring (actual_class_name,
				    strlen (actual_class_name),
				    &cu->comp_unit_obstack);
		  xfree (actual_class_name);
		}
	      break;
	    }

	  child_pdi = child_pdi->die_sibling;
	}
    }
}

/* Read a partial die corresponding to an enumeration type.  */

static void
add_partial_enumeration (struct partial_die_info *enum_pdi,
			 struct dwarf2_cu *cu)
{
  /* APPLE LOCAL avoid unused var warnings.  */
  /* struct objfile *objfile = cu->objfile; */
  /* bfd *abfd = objfile->obfd; */
  struct partial_die_info *pdi;

  if (enum_pdi->name != NULL)
    add_partial_symbol (enum_pdi, cu);

  pdi = enum_pdi->die_child;
  while (pdi)
    {
      if (pdi->tag != DW_TAG_enumerator || pdi->name == NULL)
	complaint (&symfile_complaints, _("malformed enumerator DIE ignored"));
      else
	add_partial_symbol (pdi, cu);
      pdi = pdi->die_sibling;
    }
}

/* Read the initial uleb128 in the die at INFO_PTR in compilation unit CU.
   Return the corresponding abbrev, or NULL if the number is zero (indicating
   an empty DIE).  In either case *BYTES_READ will be set to the length of
   the initial number.  */

static struct abbrev_info *
peek_die_abbrev (char *info_ptr, int *bytes_read, struct dwarf2_cu *cu)
{
  bfd *abfd = cu->objfile->obfd;
  unsigned int abbrev_number;
  struct abbrev_info *abbrev;

  /* APPLE LOCAL: If this DIE is beyond the end of the compile unit, the
     producer didn't correctly terminate the debug info. Return a NULL
     so we don't try and parse this DIE and also fill the BYTES_READ
     pointer with zero so we don't think we consumed any data.  */
  if (info_ptr - dwarf2_per_objfile->info_buffer >= 
      cu->header.offset + cu->header.length + cu->header.initial_length_size)
    {
      if (bytes_read)
	*bytes_read = 0;
      return NULL;
    }

  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, (unsigned *) bytes_read);

  if (abbrev_number == 0)
    return NULL;

  abbrev = dwarf2_lookup_abbrev (abbrev_number, cu);
  if (!abbrev)
    {
      error (_("Dwarf Error: Could not find abbrev number %d [in module %s]"), abbrev_number,
		      bfd_get_filename (abfd));
    }

  return abbrev;
}

/* Scan the debug information for CU starting at INFO_PTR.  Returns a
   pointer to the end of a series of DIEs, terminated by an empty
   DIE.  Any children of the skipped DIEs will also be skipped.  */

static char *
skip_children (char *info_ptr, struct dwarf2_cu *cu)
{
  struct abbrev_info *abbrev;
  unsigned int bytes_read;

  while (1)
    {
      /* APPLE LOCAL Add cast to avoid type mismatch in arg2 warning.  */
      abbrev = peek_die_abbrev (info_ptr, (int *) &bytes_read, cu);
      if (abbrev == NULL)
	return info_ptr + bytes_read;
      else
	info_ptr = skip_one_die (info_ptr + bytes_read, abbrev, cu);
    }
}

/* Scan the debug information for CU starting at INFO_PTR.  INFO_PTR
   should point just after the initial uleb128 of a DIE, and the
   abbrev corresponding to that skipped uleb128 should be passed in
   ABBREV.  Returns a pointer to this DIE's sibling, skipping any
   children.  */

static char *
skip_one_die (char *info_ptr, struct abbrev_info *abbrev,
	      struct dwarf2_cu *cu)
{
  unsigned int bytes_read;
  struct attribute attr;
  bfd *abfd = cu->objfile->obfd;
  unsigned int form, i;

  for (i = 0; i < abbrev->num_attrs; i++)
    {
      /* The only abbrev we care about is DW_AT_sibling.  */
      if (abbrev->attrs[i].name == DW_AT_sibling)
	{
	  read_attribute (&attr, &abbrev->attrs[i],
			  abfd, info_ptr, cu);
	  if (attr.form == DW_FORM_ref_addr)
	    complaint (&symfile_complaints, _("ignoring absolute DW_AT_sibling"));
	  else
	    return dwarf2_per_objfile->info_buffer
	      + dwarf2_get_ref_die_offset (&attr, cu);
	}

      /* If it isn't DW_AT_sibling, skip this attribute.  */
      form = abbrev->attrs[i].form;
    skip_attribute:
      switch (form)
	{
	case DW_FORM_addr:
	case DW_FORM_ref_addr:
	  info_ptr += cu->header.addr_size;
	  break;
	case DW_FORM_data1:
	case DW_FORM_ref1:
	case DW_FORM_flag:
	  info_ptr += 1;
	  break;
	case DW_FORM_data2:
	case DW_FORM_ref2:
	  info_ptr += 2;
	  break;
	case DW_FORM_data4:
	case DW_FORM_ref4:
	  info_ptr += 4;
	  break;
	case DW_FORM_data8:
	case DW_FORM_ref8:
	  info_ptr += 8;
	  break;
	case DW_FORM_string:
	  read_string (abfd, info_ptr, &bytes_read);
	  info_ptr += bytes_read;
	  break;
	case DW_FORM_strp:
	  info_ptr += cu->header.offset_size;
	  break;
	case DW_FORM_block:
	  info_ptr += read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	  info_ptr += bytes_read;
	  break;
	case DW_FORM_block1:
	  info_ptr += 1 + read_1_byte (abfd, info_ptr);
	  break;
	case DW_FORM_block2:
	  info_ptr += 2 + read_2_bytes (abfd, info_ptr);
	  break;
	case DW_FORM_block4:
	  info_ptr += 4 + read_4_bytes (abfd, info_ptr);
	  break;
	case DW_FORM_APPLE_db_str:
	case DW_FORM_sdata:
	case DW_FORM_udata:
	case DW_FORM_ref_udata:
	  info_ptr = skip_leb128 (abfd, info_ptr);
	  break;
	case DW_FORM_indirect:
	  form = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
	  info_ptr += bytes_read;
	  /* We need to continue parsing from here, so just go back to
	     the top.  */
	  goto skip_attribute;

	default:
	  error (_("Dwarf Error: Cannot handle %s in DWARF reader [in module %s]"),
		 dwarf_form_name (form),
		 bfd_get_filename (abfd));
	}
    }

  if (abbrev->has_children)
    return skip_children (info_ptr, cu);
  else
    return info_ptr;
}

/* Locate ORIG_PDI's sibling; INFO_PTR should point to the start of
   the next DIE after ORIG_PDI.  */

static char *
locate_pdi_sibling (struct partial_die_info *orig_pdi, char *info_ptr,
		    bfd *abfd, struct dwarf2_cu *cu)
{
  /* Do we know the sibling already?  */

  if (orig_pdi->sibling)
    return orig_pdi->sibling;

  /* Are there any children to deal with?  */

  if (!orig_pdi->has_children)
    return info_ptr;

  /* Skip the children the long way.  */

  return skip_children (info_ptr, cu);
}

/* Expand this partial symbol table into a full symbol table.  */

static void
dwarf2_psymtab_to_symtab (struct partial_symtab *pst)
{
  /* FIXME: This is barely more than a stub.  */
  if (pst != NULL)
    {
      if (pst->readin)
	{
	  warning (_("bug: psymtab for %s is already read in."), pst->filename);
	}
      else
	{
	  if (info_verbose)
	    {
	      printf_filtered (_("Reading in symbols for %s..."), pst->filename);
	      gdb_flush (gdb_stdout);
	    }

	  /* Restore our global data.  */
	  dwarf2_per_objfile = objfile_data (pst->objfile,
					     dwarf2_objfile_data_key);

	  psymtab_to_symtab_1 (pst);

	  /* Finish up the debug error message.  */
	  if (info_verbose)
	    printf_filtered (_("done.\n"));
	}
    }
}

/* APPLE LOCAL debug map: A call back function passed to qsort ().  */

static int 
compare_map_entries_oso_addr (const void *a, const void *b)
{
  struct oso_final_addr_tuple *map_a = (struct oso_final_addr_tuple *) a;
  struct oso_final_addr_tuple *map_b = (struct oso_final_addr_tuple *) b;

  if (map_a->oso_low_addr == map_b->oso_low_addr)
    {
      if (map_a->present_in_final == map_b->present_in_final)
	return 0;
      if (map_a->present_in_final < map_b->present_in_final)
	return -1;
      return 1;
    }
  if (map_a->oso_low_addr < map_b->oso_low_addr)
    return -1;
  return 1;
}

/* APPLE LOCAL debug map: A call back function passed to qsort_r ().
   Used when create an alternate index of the tuple array, sorted by 
   function-end address.  */

static int 
compare_map_entries_final_addr_index (void *thunk, const void *a, const void *b)
{
  struct oso_final_addr_tuple *tuples = (struct oso_final_addr_tuple *) thunk;
  int idx_a = *(int *) a;
  int idx_b = *(int *) b;
  CORE_ADDR addr_a = tuples[idx_a].final_addr;
  CORE_ADDR addr_b = tuples[idx_b].final_addr;

  /* Fall back and sort on OSO low addrs when final addrs are the same (which
     should only happen for stuff that didn't make it into the final 
     executable).  */
  if (addr_a == addr_b)
    {
      addr_a = tuples[idx_a].oso_low_addr;
      addr_b = tuples[idx_b].oso_low_addr;
    }

  if (addr_a == addr_b)
    return 0;
  else if (addr_a < addr_b)
    return -1;
  else 
    return 1;
}

/* APPLE LOCAL debug map: A call back function passed to qsort ().  */

static int 
compare_map_entries_name (const void *a, const void *b)
{
  struct oso_final_addr_tuple *map_a = (struct oso_final_addr_tuple *) a;
  struct oso_final_addr_tuple *map_b = (struct oso_final_addr_tuple *) b;

  return (strcmp (map_a->name, map_b->name));
}


/* APPLE LOCAL debug map: Given an array of symbol names & addresses from
   a .o file and the actual final addresses from an objfile's minsyms,
   create a mapping table between the two. 

   NLISTS is an array of symbol name/addresses from the .o file; 
   NOSO_NLISTS_COUNT is the number of elements in that array.  
   PST is the partial symbol table we're expanding.  

   This function frees NLISTS by the time it is done; it malloc's the
   space for the oso-to-final-address table.  */

static struct oso_to_final_addr_map *
convert_oso_map_to_final_map (struct nlist_rec *nlists,
                              int oso_nlists_count,
                              char **oso_common_symnames,
                              int oso_common_symnames_count,
                              struct partial_symtab *pst)
{
  int i, j;
  struct oso_to_final_addr_map *map = 
                             xmalloc (sizeof (struct oso_to_final_addr_map));
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct objfile *minsym_lookup_objfile; */

  /* Store the load address of the objfile in a convenient local variable.
     For an executable or dylib with a seg1addr that was honored, this 
     will be 0; for anything that slid, it will be the slide value.  */
  CORE_ADDR baseaddr = objfile_text_section_offset (pst->objfile);

  map->tuples = (struct oso_final_addr_tuple *) 
           xmalloc (sizeof (struct oso_final_addr_tuple) * oso_nlists_count);
  map->final_addr_index = NULL; 

  j = 0;
  for (i = 0; i < oso_nlists_count; i++)
    {
      struct minimal_symbol *minsym;
      struct partial_symbol *psym;
      char *demangled = NULL;
      int free_demangled = 0;

      if (pst->language == language_cplus
          || pst->language == language_objcplus
          || pst->language == language_auto)
        demangled  = cplus_demangle (nlists[i].name, DMGL_PARAMS | DMGL_ANSI);
      if (pst->language == language_java)
        demangled  = cplus_demangle (nlists[i].name, 
                                     DMGL_PARAMS | DMGL_ANSI | DMGL_JAVA);
      if (demangled != NULL)
        free_demangled = 1;     /* Remember to free it */
      else
        demangled = nlists[i].name; /* Name isn't mangled; source == linkage */

      /* First try looking it up in this psymtab -- static symbols may occur
         in multiple compilation units (psymtabs) and we must get the correct
         one.  This lookup is a linear search (the table is sorted by 
         source name, not linkage name) but I don't see a better option.  */

      psym = lookup_partial_symbol (pst, demangled, nlists[i].name, 
                                    1, VAR_DOMAIN);
      if (psym == NULL)
        psym = lookup_partial_symbol (pst, demangled, nlists[i].name, 
                                    0, VAR_DOMAIN);

      /* We're done with the demangled name now.  */
      if (free_demangled)
        xfree (demangled);

      /* Common symbols debug map entries (stab N_GSYM) have an
	 address of BASEADDR -- you have to look up the minsym to
	 find the actual address.  These are not static so there's
	 no danger of getting the wrong one via an unrestricted
	 lookup_minimal_symbol search.  */

      if (psym && SYMBOL_VALUE_ADDRESS (psym) != baseaddr)
         minsym = lookup_minimal_symbol_by_pc_section_from_objfile 
                         (SYMBOL_VALUE_ADDRESS (psym), 
                          SYMBOL_BFD_SECTION (psym), pst->objfile);
      else
        minsym = lookup_minimal_symbol (nlists[i].name, NULL, 
                                        pst->objfile);

      /* lookup_minimal_symbol() returns an address with the offset
         (e.g. the slide) already applied; we need to factor that offset
         back out of the address because all the callers will re-apply the
         offset when they translate the address.  In short, subtract off
         BASEADDR.  */

      if (minsym)
        {
          CORE_ADDR actual_address;
          if (minsym->type == mst_abs)
            actual_address = SYMBOL_VALUE_ADDRESS (minsym);
          else
            actual_address = SYMBOL_VALUE_ADDRESS (minsym) - baseaddr;

          /* Assume that no *real* symbol can end up at address 0 in a 
             final linked executable/dylib/bundle.  */
          if (actual_address == 0 && nlists[i].addr == 0)
            {
              xfree ((char *) nlists[i].name);
              continue;
            }
          map->tuples[j].name = nlists[i].name;
          map->tuples[j].oso_low_addr = nlists[i].addr;
          map->tuples[j].final_addr = actual_address;
          map->tuples[j].present_in_final = 1;
          j++;
        }
      else
        {
          map->tuples[j].name = nlists[i].name;
          map->tuples[j].oso_low_addr = nlists[i].addr;
          map->tuples[j].final_addr = (CORE_ADDR) -1;
          map->tuples[j].present_in_final = 0;
          j++;
        }
    }
  xfree (nlists);

  qsort (map->tuples, j, sizeof (struct oso_final_addr_tuple),
         compare_map_entries_oso_addr);
  map->entries = j;

  /* Initialize the oso_high_addr entries */

  if (map->entries > 1)
    {
      j = 0;  /* This holds valid the tuple insert index.  */

      for (i = 0; i < map->entries - 1; i++)
	{
	  if (map->tuples[i].oso_low_addr == map->tuples[i+1].oso_low_addr &&
	      map->tuples[i].present_in_final == 0)
	    {
	      if (debug_debugmap)
	      	{
		  fprintf_unfiltered (gdb_stdlog, 
				      "debugmap: removing tuple[%i] ('%s')\n", 
				      i, map->tuples[i].name);
		}
	      continue; /* Skip useless tuple, and don't increment J.  */
	    }
	  
	  /* If we have skipped some tuples, we need to copy subsequent ones 
	     back to fill the gaps.  */
	  if (j < i)
	    map->tuples[j] = map->tuples[i];
	    
	  map->tuples[j].oso_high_addr = map->tuples[i+1].oso_low_addr;
	  j++;
	}
		
      gdb_assert(j <= map->entries);
      map->tuples[j].oso_high_addr = (CORE_ADDR) -1;
      /* Adjust the number of tuple if they changed.  */
      map->entries = j + 1;
      
      /* Allocate the final_addr_index array.  */
      map->final_addr_index = (int *) xmalloc (sizeof (int) * map->entries);

      /* Initialize final_addr_index so it can be sorted using qsort_r.  */
      for (i = 0; i < map->entries; i++)
        map->final_addr_index[i] = i; 
  
      /* Sort the final_addr_index array.  */
      qsort_r (map->final_addr_index, map->entries, sizeof (int), 
	       map->tuples, compare_map_entries_final_addr_index);
    }
  else if (map->entries == 1)
    {
      map->tuples[0].oso_high_addr = (CORE_ADDR) -1;
      map->final_addr_index = (int *) xmalloc (sizeof (int));
      map->final_addr_index[0] = 0; 
    }
 
  /* Print out the final addr indexes array.  */
  if (debug_debugmap >= 6)
    {
      CORE_ADDR final_addr = 0;
      for (i = 0; i < map->entries; i++)
	{
	  int idx = map->final_addr_index[i];
	  final_addr = map->tuples[idx].final_addr;
	  fprintf_unfiltered (gdb_stdlog, 
			      "map->final_addr_index[%3i] = %3i (map->tuples[%3i] oso_addr [%s - %s) final_addr = %s for %s\n", 
			      i, idx, idx, 
			      paddr (map->tuples[idx].oso_low_addr),
			      paddr (map->tuples[idx].oso_high_addr),
			      map->tuples[idx].present_in_final ?
			      paddr (map->tuples[idx].final_addr) : "stripped",
			      map->tuples[idx].name);
	}
    }

  /* Now populate the common symbol names array */

  if (oso_common_symnames_count == 0)
    {
      map->common_entries = 0;
      map->common_pairs = NULL;
    }
  else
    {
      map->common_pairs = (struct oso_final_addr_tuple *)
               xmalloc (sizeof (struct oso_final_addr_tuple) * 
                        oso_common_symnames_count);
      j = 0;
      for (i = 0; i < oso_common_symnames_count; i++)
        {
          struct minimal_symbol *minsym;

          /* lookup_minimal_symbol() returns an address with the offset
             (e.g. the slide) already applied; we need to factor that offset
             back out of the address because all the callers will re-apply the
             offset when they translate the address.  */

          CORE_ADDR baseaddr = objfile_data_section_offset (pst->objfile);

          minsym = lookup_minimal_symbol (oso_common_symnames[i], NULL, 
                                          pst->objfile);

          /* We're only looking for data here */
          if (minsym && minsym->type == mst_data)
            {
              CORE_ADDR actual_address = SYMBOL_VALUE_ADDRESS (minsym) - 
                                         baseaddr;

              /* Assume that no *real* symbol can end up at address 0 in a 
                 final linked executable/dylib/bundle.  */
              if (actual_address == 0)
                {
                  xfree (oso_common_symnames[i]);
                  continue;
                }
              map->common_pairs[j].name = oso_common_symnames[i];
              map->common_pairs[j].oso_low_addr = 0;
              map->common_pairs[j].oso_high_addr = 0;
              map->common_pairs[j].final_addr = actual_address;
              j++;
            }
          else
            xfree (oso_common_symnames[i]);
        }
      xfree (oso_common_symnames);
      qsort (map->common_pairs, j, sizeof (struct oso_final_addr_tuple),
             compare_map_entries_name);
      map->common_entries = j;
    }
  /* I can't imagine xstrdup()'ing the pst filename is actually necessary;
     if the pst goes away I'd expect all traces of this objfile's symbols
     (and therefore any pointers to the translation map) to also go away..  */
  map->pst = pst;
  return (map);
}

/* APPLE LOCAL debug map: A version of create_kext_addr_map() which
   works for kext + dSYM objfiles.

   The main difference is that instead of going from the non-stab
   nlist records in the .o file, this version works from the psymtab's
   statics and globals.  The psymtab is derived from the debug map (stab)
   entries in the kextload-output .sym file.

   NLISTS is an array of symbol name/addresses from the .o file; 
   NOSO_NLISTS_COUNT is the number of elements in that array.  
   PST is the partial symbol table we're expanding.  

   This function frees NLISTS by the time it is done; it malloc's the
   space for the oso-to-final-address table.  */

static struct oso_to_final_addr_map *
create_kext_addr_map (struct nlist_rec *nlists,
                              int oso_nlists_count,
                              char **oso_common_symnames,
                              int oso_common_symnames_count,
                              struct partial_symtab *pst)
{
  int i, j;
  struct partial_symbol **psym;
  struct oso_to_final_addr_map *map = 
                             xmalloc (sizeof (struct oso_to_final_addr_map));
  map->entries = map->common_entries = 0;

  /* Store the load address of the objfile in a convenient local variable.
     For an executable or dylib with a seg1addr that was honored, this 
     will be 0; for anything that slid, it will be the slide value.  */
  CORE_ADDR baseaddr = objfile_text_section_offset (pst->objfile);

  int number_of_psymbols = 
     pst->n_global_syms + pst->n_static_syms;

  map->tuples = (struct oso_final_addr_tuple *) 
           xmalloc (sizeof (struct oso_final_addr_tuple) * number_of_psymbols);
  map->final_addr_index = NULL;

  j = 0;  /* Index into map->tuples */

  /* The so-called OSO nlists are actually *all* nlist records from the
     kext executable, irrespective of compilation unit because they are
     not grouped in compilation units by the time ld -r is finished with
     them. :(  The result is that we're going to do a full pass over all
     nlist records for each partial symbol we have in this pst.  
     I should sort the nlist records by symbol name and bsearch it... */

  for (psym = pst->objfile->global_psymbols.list + pst->globals_offset;
       psym < pst->objfile->global_psymbols.list + pst->globals_offset + pst->n_global_syms;
       psym++)
    {
      int matched = 0;
      struct minimal_symbol *minsym;

      /* FIXME Is there any reason why the nlists array isn't sorted by
         name to speed up these linear searches?  You can get a single
         psymtab with 50 psymbols and an executable with over a thousand
         nlist entries to slog through.  */
      for (i = 0; i < oso_nlists_count; i++)
        if (strcmp (SYMBOL_LINKAGE_NAME (*psym), nlists[i].name) == 0)
          {
            matched = 1;
            break;
          }
      if (!matched)
        continue;
      if (*psym && SYMBOL_VALUE_ADDRESS (*psym) != baseaddr)
         minsym = lookup_minimal_symbol_by_pc_section_from_objfile 
                         (SYMBOL_VALUE_ADDRESS (*psym), 
                          SYMBOL_BFD_SECTION (*psym),
                          pst->objfile->separate_debug_objfile_backlink);
      else
        minsym = lookup_minimal_symbol (nlists[i].name, NULL, 
                                pst->objfile->separate_debug_objfile_backlink);

      /* lookup_minimal_symbol() returns an address with the offset
         (e.g. the slide) already applied; we need to factor that offset
         back out of the address because all the callers will re-apply the
         offset when they translate the address.  In short, subtract off
         BASEADDR.  */

      if (minsym)
        {
          CORE_ADDR actual_address;
          if (minsym->type == mst_abs)
            actual_address = SYMBOL_VALUE_ADDRESS (minsym);
          else
            actual_address = SYMBOL_VALUE_ADDRESS (minsym) - baseaddr;

          /* Assume that no *real* symbol can end up at address 0 in a 
             final linked executable/dylib/bundle.  */
          if (actual_address == 0 && nlists[i].addr == 0)
            {
              xfree ((char *) nlists[i].name);
              continue;
            }
          map->tuples[j].name = xstrdup (nlists[i].name);
          map->tuples[j].oso_low_addr = nlists[i].addr;
          map->tuples[j].final_addr = actual_address;
          map->tuples[j].present_in_final = 1;
          j++;
        }
      else
        {
          map->tuples[j].name = xstrdup (nlists[i].name);
          map->tuples[j].oso_low_addr = nlists[i].addr;
          map->tuples[j].final_addr = (CORE_ADDR) -1;
          map->tuples[j].present_in_final = 0;
          j++;
        }
    }

  for (psym = pst->objfile->static_psymbols.list + pst->statics_offset;
       psym < pst->objfile->static_psymbols.list + pst->statics_offset + pst->n_static_syms;
       psym++)
    {
      int matched = 0;
      struct minimal_symbol *minsym;

      for (i = 0; i < oso_nlists_count; i++)
        if (strcmp (SYMBOL_LINKAGE_NAME (*psym), nlists[i].name) == 0)
          {
            matched = 1;
            break;
          }
      if (!matched)
        continue;
      if (*psym && SYMBOL_VALUE_ADDRESS (*psym) != baseaddr)
         minsym = lookup_minimal_symbol_by_pc_section_from_objfile 
                         (SYMBOL_VALUE_ADDRESS (*psym), 
                          SYMBOL_BFD_SECTION (*psym),
                          pst->objfile->separate_debug_objfile_backlink);
      else
        minsym = lookup_minimal_symbol (nlists[i].name, NULL, 
                                pst->objfile->separate_debug_objfile_backlink);

      /* lookup_minimal_symbol() returns an address with the offset
         (e.g. the slide) already applied; we need to factor that offset
         back out of the address because all the callers will re-apply the
         offset when they translate the address.  In short, subtract off
         BASEADDR.  */

      if (minsym)
        {
          CORE_ADDR actual_address;
          if (minsym->type == mst_abs)
            actual_address = SYMBOL_VALUE_ADDRESS (minsym);
          else
            actual_address = SYMBOL_VALUE_ADDRESS (minsym) - baseaddr;

          /* Assume that no *real* symbol can end up at address 0 in a 
             final linked executable/dylib/bundle.  */
          if (actual_address == 0 && nlists[i].addr == 0)
            {
              xfree ((char *) nlists[i].name);
              continue;
            }
          map->tuples[j].name = xstrdup (nlists[i].name);
          map->tuples[j].oso_low_addr = nlists[i].addr;
          map->tuples[j].final_addr = actual_address;
          map->tuples[j].present_in_final = 1;
          j++;
        }
      else
        {
          map->tuples[j].name = xstrdup (nlists[i].name);
          map->tuples[j].oso_low_addr = nlists[i].addr;
          map->tuples[j].present_in_final = 0;
          j++;
        }

    }

  for (i = 0; i < oso_nlists_count; i++)
    xfree (nlists[i].name);
  xfree (nlists);

  qsort (map->tuples, j, sizeof (struct oso_final_addr_tuple),
         compare_map_entries_oso_addr);
  map->entries = j;

  /* Initialize the oso_high_addr entries */

  if (map->entries > 1)
    {
      j = 0;  /* This holds valid the tuple insert index.  */

      for (i = 0; i < map->entries - 1; i++)
	{
	  if (map->tuples[i].oso_low_addr == map->tuples[i+1].oso_low_addr &&
	      map->tuples[i].present_in_final == 0)
	    {
	      if (debug_debugmap)
	      	{
		  fprintf_unfiltered (gdb_stdlog, 
				      "debugmap: removing tuple[%i] ('%s')\n", 
				      i, map->tuples[i].name);
		}
	      continue; /* Skip useless tuple, and don't increment J.  */
	    }
	  
	  /* If we have skipped some tuples, we need to copy subsequent ones 
	     back to fill the gaps.  */
	  if (j < i)
	    map->tuples[j] = map->tuples[i];
	    
	  map->tuples[j].oso_high_addr = map->tuples[i+1].oso_low_addr;
	  j++;
	}
		
      gdb_assert(j <= map->entries);
      map->tuples[j].oso_high_addr = (CORE_ADDR) -1;
      /* Adjust the number of tuple if they changed.  */
      map->entries = j + 1;
      
      /* Allocate the final_addr_index array.  */
      map->final_addr_index = (int *) xmalloc (sizeof (int) * map->entries);

      /* Initialize final_addr_index so it can be sorted using qsort_r.  */
      for (i = 0; i < map->entries; i++)
        map->final_addr_index[i] = i; 
  
      /* Sort the final_addr_index array.  */
      qsort_r (map->final_addr_index, map->entries, sizeof (int), 
	       map->tuples, compare_map_entries_final_addr_index);
    }
  else if (map->entries == 1)
    {
      map->tuples[0].oso_high_addr = (CORE_ADDR) -1;
      map->final_addr_index = (int *) xmalloc (sizeof (int));
      map->final_addr_index[0] = 0; 
    }
  
  /* Print out the final addr indexes array.  */
  if (debug_debugmap >= 6)
    {
      CORE_ADDR final_addr = 0;
      for (i = 0; i < map->entries; i++)
        {
          int idx = map->final_addr_index[i];
          final_addr = map->tuples[idx].final_addr;
          fprintf_unfiltered (gdb_stdlog,
                      "map->final_addr_index[%3d] = map->tuples[%3d] (0x%s)\n",
                              i, idx, paddr_nz (map->tuples[idx].final_addr));
        }                   
    }

  map->common_entries = 0;
  map->common_pairs = NULL;
  map->pst = pst;

  return (map);
}

/* APPLE LOCAL debug map: Callback function for bsearch() */
/* This function assumes the address *KEY is a DW_AT_high_pc.  */
static int 
compare_translation_tuples_highpc (const void *key, const void *arrmem)
{
  CORE_ADDR oso_addr = *((CORE_ADDR *) key);
  struct oso_final_addr_tuple *tuple = (struct oso_final_addr_tuple *) arrmem;

  if (oso_addr <= tuple->oso_low_addr)
    return -1;
  if (oso_addr <= tuple->oso_high_addr)
    return 0;
  return 1;
}

/* APPLE LOCAL debug map: Callback function for bsearch() */
/* This function assumes the address *KEY is not a DW_AT_high_pc.  */
static int 
compare_translation_tuples_nothighpc (const void *key, const void *arrmem)
{
  CORE_ADDR oso_addr = *((CORE_ADDR *) key);
  struct oso_final_addr_tuple *tuple = (struct oso_final_addr_tuple *) arrmem;

  if (oso_addr < tuple->oso_low_addr)
    return -1;
  if (oso_addr < tuple->oso_high_addr)
    return 0;
  return 1;
}

/* APPLE LOCAL debug map: Callback function for bsearch() */
/* This function compares if an address *KEY is contained in a tuple including
   if the address is equal to the high or low oso address:
   oso_low_addr <= address <= oso_high_addr. This is used when parsing line
   tables.
 */
static int 
compare_translation_tuples_inclusive (const void *key, const void *arrmem)
{
  CORE_ADDR oso_addr = *((CORE_ADDR *) key);
  struct oso_final_addr_tuple *tuple = (struct oso_final_addr_tuple *) arrmem;

  if (oso_addr < tuple->oso_low_addr)
    return -1;
  if (oso_addr <= tuple->oso_high_addr)
    return 0;
  return 1;
}

/* APPLE LOCAL debug map: Sometimes we need to be able to figure out
   what the next final address in a debug map is. This function compares
   final address values by iterating through the FINAL_ADDR_INDEX member
   in a struct oso_to_final_addr_map'. See comments in definition of the 
   'oso_to_final_addr_map' structure for full details on 
   FINAL_ADDR_INDEXES.  */

struct final_addr_key
{
  CORE_ADDR final_addr;
  struct oso_to_final_addr_map *map;
};

static int 
compare_translation_final_addr (const void *key, const void *arrmem)
{
  struct final_addr_key *final_addr_key = (struct final_addr_key *) key;
  int i = *(int *) arrmem;
  CORE_ADDR final_addr;

  // FIXME: Make this a complaint before we ship -- leave it as a warning
  // for a little bit so we can hopefully get a little feedback internally.
  // A gdb_assert would be the technically proper thing to do but I don't
  // want to abort the entire debug session just because we had a little
  // internals kerfuffle.

  if (i < 0 || i >= final_addr_key->map->entries)
    {
      warning ("GDB internal issue: "
               "Looking at entry %d when there are only %d entries",
               i, final_addr_key->map->entries);
      return 0;
    }

  final_addr = final_addr_key->map->tuples[i].final_addr;
  if (final_addr_key->final_addr == final_addr)
    return 0;
  else if (final_addr_key->final_addr < final_addr)
    return -1;
  return 1;
}

/* APPLE LOCAL debug map: Translate an address from a .o file's DWARF
   into the actual address in the linked executable.  
   OSO_ADDR is the address in the .o file to be translated.
   *ADDR is set to the translated (executable image) address.

   HIGHPC indicates that this address is from a DW_AT_high_pc.
   this is a hack to avoid the problem where we have 
      func1:  0x100 - 0x200  (DW_AT_low_pc = 0x100, DW_AT_high_pc = 0x200)
      func2:  0x200 - 0x300  (DW_AT_low_pc = 0x200, DW_AT_high_pc = 0x300)
   they end up in the final executable reordered
      func2: start at 0x3000
      func1: start at 0x4500
   Given 0x200, which does it get translated to?  0x3000 or 0x4600?  
   Hence the HIGHPC argument.

   When an address has succesfully been translated, 1 is returned.
   When an address has not been translated, 0 is returned.

   If there is no debug map, *ADDR is set to OSO_ADDR and a value
   of 1 is returned.  i.e. the input address and output address 
   are the same.

   If there is no matching translation entry, 0 is returned and
   *ADDR is not set to anything.  This will happen when the address
   is in a function that was coalesced or dead code stripped during
   the final link edit stage.  */

int
translate_debug_map_address (struct oso_to_final_addr_map *map, 
                             CORE_ADDR oso_addr, CORE_ADDR *addr, int highpc)
{
  /* Handle the case where this is traditional dwarf (no debug map) */
  if (map == NULL || map->tuples == NULL)
    {
      *addr = oso_addr;
      return 1;
    }

  /* Handle the case where we have a single symbol ("main") in a file
     specially.  */
  if (map->tuples[0].oso_low_addr < oso_addr && map->entries == 1)
    {
      int delta = oso_addr - map->tuples[0].oso_low_addr;
      *addr = map->tuples[0].final_addr + delta;
      if (debug_debugmap)
        fprintf_unfiltered (gdb_stdlog, 
                            "debugmap: translated 0x%s to 0x%s sym '%s' in %s\n",
                            paddr_nz (oso_addr), paddr_nz (*addr), 
                            map->tuples[0].name, map->pst->filename);
      return 1;
    }

  struct oso_final_addr_tuple *match;

  if (highpc == 0)
    match = bsearch (&oso_addr, map->tuples, map->entries, 
                     sizeof (struct oso_final_addr_tuple), 
                     compare_translation_tuples_nothighpc);
  else
    match = bsearch (&oso_addr, map->tuples, map->entries, 
                     sizeof (struct oso_final_addr_tuple), 
                     compare_translation_tuples_highpc);

  /* The address is in a block of code that failed to make it to the
     final executable, either by dead code stripping or coalescing.
     Return 0 to indicate that.  */

  if (match == NULL || !match->present_in_final)
    {
      if (debug_debugmap)      
	{
	  if (match == NULL)
	    fprintf_unfiltered (gdb_stdlog, 
				"debugmap: did not translate 0x%s in %s "
				"highpc == %d\n",
				paddr_nz (oso_addr), map->pst->filename,
				highpc);
	  else
	    fprintf_unfiltered (gdb_stdlog, 
				"debugmap: did not translate 0x%s "
				"(dead-stripped %s) in %s highpc == %d\n",
				paddr_nz (oso_addr), 
				match->name,
				map->pst->filename,
				highpc);
	}
      return 0;
    }

  *addr = translate_debug_map_address_with_tuple (map, match, oso_addr, highpc);
  return 1;
}

/* APPLE LOCAL:  Common symbols (global data not initialized to a value I
   believe) have no address in the .o file - nm will show the linker symbols
   of type 'C' with the "address" being the size of the item.  So the usual
   "Look up the .o file address from the dwarf file in our address map"
   technique fails -- all of these symbols have a .o address of 0.
   So instead we do the search by symbol name.  

   This function returns 1 if a matching symbol name was found, else 0.  */

static int
translate_common_symbol_debug_map_address (struct oso_to_final_addr_map *map, 
                                           const char *name, CORE_ADDR *addr)
{
  int i;

  /* Handle the case where this is traditional dwarf (no debug map) */
  if (map == NULL || map->common_pairs == NULL)
    return 0;

  /* A linear search through a sorted list - brilliant.  Make it work first,
     then change up to a binary search.  */

  for (i = 0; i < map->common_entries; i++)
    if (strcmp (map->common_pairs[i].name, name) == 0)
      {
        *addr = map->common_pairs[i].final_addr;
        if (debug_debugmap)
          fprintf_unfiltered (gdb_stdlog, 
                              "debugmap: translated common symbol '%s' to 0x%s in %s\n",
                              name, paddr_nz (*addr), 
                              map->pst->filename);
        return 1;
      }

  if (debug_debugmap)
    fprintf_unfiltered (gdb_stdlog, 
                        "debugmap: failed to translate common symbol '%s' in %s\n", 
                        name, map->pst->filename);
  return 0;
}

/* APPLE LOCAL  */

void 
dwarf2_kext_psymtab_to_symtab (struct partial_symtab *pst)
{
  bfd *containing_archive = NULL;
  bfd *oso_bfd;
  struct nlist_rec *oso_nlists;
  int oso_nlists_count;
  char **oso_common_symnames;
  int oso_common_symnames_count;
  struct oso_to_final_addr_map *addr_map;
  struct dwarf2_per_cu_data *this_cu;
  int bytes_read;
  int i;
  /* APPLE LOCAL avoid unused var warning  */
  /* int kext_plus_dsym = 0; */
  char *kext_dsym_comp_unit_start;
  /* APPLE LOCAL avoid unused var warning  */
  /* unsigned int kext_dsym_comp_unit_length; */

  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
        /* Inform about additional files that need to be read in.  */
        if (info_verbose)
          {
            /* FIXME: i18n: Need to make this a single string.  */
            fputs_filtered (" ", gdb_stdout);
            wrap_here ("");
            fputs_filtered ("and ", gdb_stdout);
            wrap_here ("");
            printf_filtered ("%s...", pst->dependencies[i]->filename);
            wrap_here ("");     /* Flush output */
            gdb_flush (gdb_stdout);
          }
        dwarf2_debug_map_psymtab_to_symtab (pst->dependencies[i]);
      }

  if (PSYMTAB_OSO_NAME (pst) == NULL || pst->readin)
    return;

  oso_bfd = symfile_bfd_open (pst->objfile->not_loaded_kext_filename, 0);
  if (oso_bfd == NULL)
    error ("Couldn't unloaded kext file '%s'", 
           pst->objfile->not_loaded_kext_filename);

  read_oso_nlists (oso_bfd, pst, &oso_nlists, &oso_nlists_count,
                   &oso_common_symnames, &oso_common_symnames_count);

  addr_map = create_kext_addr_map (oso_nlists, oso_nlists_count, 
                                           oso_common_symnames, 
                                           oso_common_symnames_count, pst);


  dwarf2_has_info_1 (pst->objfile, pst->objfile->obfd);
  dwarf2_copy_dwarf_from_file (pst->objfile, pst->objfile->obfd);
  kext_dsym_comp_unit_start = find_debug_info_for_pst (pst);
  
  /* Normally the struct dwarf2_per_cu_data is constructed as a part of
     the dwarf partial symtab creation.  So fake one up here.  */
  this_cu = (struct dwarf2_per_cu_data *)
                  obstack_alloc (&(pst->objfile->objfile_obstack),
                                 sizeof (struct dwarf2_per_cu_data));

  if (kext_dsym_comp_unit_start < dwarf2_per_objfile->info_buffer)
    error ("Unable to find compilation unit offset in DWARF debug info for %s",
           pst->filename);
  this_cu->offset = kext_dsym_comp_unit_start - dwarf2_per_objfile->info_buffer;
  this_cu->length = read_initial_length (oso_bfd, 
                                         kext_dsym_comp_unit_start, 
                                         NULL, &bytes_read);
  this_cu->queued = 0;
  this_cu->cu = NULL;
  this_cu->psymtab = pst;
  this_cu->type_hash = NULL;

  load_full_comp_unit (this_cu, addr_map);

  process_full_comp_unit (this_cu);

  if (containing_archive != NULL)
    bfd_close (containing_archive);
  bfd_close (oso_bfd);

  pst->readin = 1;
}


/* APPLE LOCAL debug map: Expand a partial symbol table to a symbol table
   in a DWARF debug map type situation.  I have to skip around between
   functions normally called a partial_symtab creation time and functions
   called at symtab creation time in this func so it's a little odd.  

   The address translation map is not freed at the end -- there may be pointers
   to it in location list expressions so we'll need to keep it around.  */

void 
dwarf2_debug_map_psymtab_to_symtab (struct partial_symtab *pst)
{
  int cached;
  bfd *oso_bfd;
  struct nlist_rec *oso_nlists;
  int oso_nlists_count;
  char **oso_common_symnames;
  int oso_common_symnames_count;
  struct oso_to_final_addr_map *addr_map;
  struct dwarf2_per_cu_data *this_cu;
  int bytes_read;
  int i;

  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
        /* Inform about additional files that need to be read in.  */
        if (info_verbose)
          {
            /* FIXME: i18n: Need to make this a single string.  */
            fputs_filtered (" ", gdb_stdout);
            wrap_here ("");
            fputs_filtered ("and ", gdb_stdout);
            wrap_here ("");
            printf_filtered ("%s...", pst->dependencies[i]->filename);
            wrap_here ("");     /* Flush output */
            gdb_flush (gdb_stdout);
          }
        dwarf2_debug_map_psymtab_to_symtab (pst->dependencies[i]);
      }

  if (PSYMTAB_OSO_NAME (pst) == NULL || pst->readin)
    return;

   if (info_verbose)
     {
       printf_filtered (_("Reading in symbols for %s..."), pst->filename);
       gdb_flush (gdb_stdout);
     }

  oso_bfd = open_bfd_from_oso (pst, &cached);
  if (oso_bfd == NULL)
    {
      /* If we have a dSYM file don't error() out.  */
      if (pst->objfile->separate_debug_objfile != NULL)
        {
          pst->readin = 1;
          return;
        }
      else
	{
	  /* Otherwise, warn, say we've read it in, and return.  */
	  warning ("Couldn't open object file '%s'", PSYMTAB_OSO_NAME (pst));
	  pst->readin = 1;
	  return;
	}
    }
  if (!bfd_check_format (oso_bfd, bfd_object))
    warning ("Not in bfd_object form");

  read_oso_nlists (oso_bfd, pst, &oso_nlists, &oso_nlists_count,
                   &oso_common_symnames, &oso_common_symnames_count);

  addr_map = convert_oso_map_to_final_map (oso_nlists, oso_nlists_count, 
                                           oso_common_symnames, 
                                           oso_common_symnames_count, pst);


  dwarf2_has_info_1 (pst->objfile, oso_bfd);
  dwarf2_copy_dwarf_from_file (pst->objfile, oso_bfd);
  
  /* Normally the struct dwarf2_per_cu_data is constructed as a part of
     the dwarf partial symtab creation.  So fake one up here.  */
  this_cu = (struct dwarf2_per_cu_data *)
                  obstack_alloc (&(pst->objfile->objfile_obstack),
                                 sizeof (struct dwarf2_per_cu_data));

  this_cu->offset = 0;   /* First (and only) CU in the debug_info */
  this_cu->length = read_initial_length (oso_bfd, 
                                         dwarf2_per_objfile->info_buffer, 
                                         NULL, &bytes_read);
  this_cu->queued = 0;
  this_cu->cu = NULL;
  this_cu->psymtab = pst;
  this_cu->type_hash = NULL;

  load_full_comp_unit (this_cu, addr_map);

  process_full_comp_unit (this_cu);

  if (cached)
    clear_containing_archive_cache ();
  else
    close_bfd_or_archive (oso_bfd);

  pst->readin = 1;
  /* Finish up the debug error message.  */
  if (info_verbose)
    printf_filtered (_("done.\n"));
}

char *
find_debug_info_for_pst (struct partial_symtab *pst)
{
  struct cleanup *back_to;
  char *info_ptr;

  bfd *dsym_abfd = pst->objfile->obfd;
  struct objfile *dsym_objfile = pst->objfile;

  /* Any cached compilation units will be linked by the per-objfile 
     read_in_chain.  Make sure to free them when we're done.  */
  info_ptr = dwarf2_per_objfile->info_buffer;
  back_to = make_cleanup (free_cached_comp_units, NULL);

  while (info_ptr < (dwarf2_per_objfile->info_buffer
		     + dwarf2_per_objfile->info_size))
    {
      struct cleanup *back_to_inner;
      struct dwarf2_cu cu;
      struct abbrev_info *abbrev;
      unsigned int bytes_read;
      /* APPLE LOCAL avoid unused var warning  */
      /* struct dwarf2_per_cu_data *this_cu; */
      char *beg_of_comp_unit;
      struct partial_die_info comp_unit_die;

      beg_of_comp_unit = info_ptr;

      memset (&cu, 0, sizeof (cu));

      obstack_init (&cu.comp_unit_obstack);

      back_to_inner = make_cleanup (free_stack_comp_unit, &cu);

      cu.objfile = dsym_objfile;
      info_ptr = partial_read_comp_unit_head (&cu.header, info_ptr, dsym_abfd);

      /* Complete the cu_header */
      cu.header.offset = beg_of_comp_unit - dwarf2_per_objfile->info_buffer;
      cu.header.first_die_ptr = info_ptr;
      cu.header.cu_head_ptr = beg_of_comp_unit;

      cu.list_in_scope = &file_symbols;

      /* Read the abbrevs for this compilation unit into a table */
      dwarf2_read_abbrevs (dsym_abfd, &cu);
      make_cleanup (dwarf2_free_abbrev_table, &cu);

      /* Read the compilation unit die */
      abbrev = peek_die_abbrev (info_ptr, (int *) &bytes_read, &cu);
      info_ptr = read_partial_die (&comp_unit_die, abbrev, bytes_read,
				   dsym_abfd, info_ptr, &cu);
      if (comp_unit_die.name != NULL
          && strcmp (comp_unit_die.name, pst->filename) == 0)
        {
          do_cleanups (back_to);
          return beg_of_comp_unit;
        }
 
      info_ptr = beg_of_comp_unit + cu.header.length
                                  + cu.header.initial_length_size;
      do_cleanups (back_to_inner);
    }

  do_cleanups (back_to);
  return NULL;
}

/* Add PER_CU to the queue.  */

static void
queue_comp_unit (struct dwarf2_per_cu_data *per_cu)
{
  struct dwarf2_queue_item *item;

  per_cu->queued = 1;
  item = xmalloc (sizeof (*item));
  item->per_cu = per_cu;
  item->next = NULL;

  if (dwarf2_queue == NULL)
    dwarf2_queue = item;
  else
    dwarf2_queue_tail->next = item;

  dwarf2_queue_tail = item;
}

/* Process the queue.  */

static void
process_queue (struct objfile *objfile)
{
  struct dwarf2_queue_item *item, *next_item;

  /* Initially, there is just one item on the queue.  Load its DIEs,
     and the DIEs of any other compilation units it requires,
     transitively.  */

  for (item = dwarf2_queue; item != NULL; item = item->next)
    {
      /* Read in this compilation unit.  This may add new items to
	 the end of the queue.  */
      /* APPLE LOCAL debug map: NULL second argument. */
      load_full_comp_unit (item->per_cu, NULL);

      item->per_cu->cu->read_in_chain = dwarf2_per_objfile->read_in_chain;
      dwarf2_per_objfile->read_in_chain = item->per_cu;

      /* If this compilation unit has already had full symbols created,
	 reset the TYPE fields in each DIE.  */
      if (item->per_cu->psymtab->readin)
	reset_die_and_siblings_types (item->per_cu->cu->dies,
				      item->per_cu->cu);
    }

  /* Now everything left on the queue needs to be read in.  Process
     them, one at a time, removing from the queue as we finish.  */
  for (item = dwarf2_queue; item != NULL; dwarf2_queue = item = next_item)
    {
      if (!item->per_cu->psymtab->readin)
	process_full_comp_unit (item->per_cu);

      item->per_cu->queued = 0;
      next_item = item->next;
      xfree (item);
    }

  dwarf2_queue_tail = NULL;
}

/* Free all allocated queue entries.  This function only releases anything if
   an error was thrown; if the queue was processed then it would have been
   freed as we went along.  */

static void
dwarf2_release_queue (void *dummy)
{
  struct dwarf2_queue_item *item, *last;

  item = dwarf2_queue;
  while (item)
    {
      /* Anything still marked queued is likely to be in an
	 inconsistent state, so discard it.  */
      if (item->per_cu->queued)
	{
	  if (item->per_cu->cu != NULL)
	    free_one_cached_comp_unit (item->per_cu->cu);
	  item->per_cu->queued = 0;
	}

      last = item;
      item = item->next;
      xfree (last);
    }

  dwarf2_queue = dwarf2_queue_tail = NULL;
}

/* Read in full symbols for PST, and anything it depends on.  */

static void
psymtab_to_symtab_1 (struct partial_symtab *pst)
{
  struct dwarf2_per_cu_data *per_cu;
  struct cleanup *back_to;
  int i;

  for (i = 0; i < pst->number_of_dependencies; i++)
    if (!pst->dependencies[i]->readin)
      {
	/* Inform about additional files that need to be read in.  */
	if (info_verbose)
	  {
	    /* FIXME: i18n: Need to make this a single string.  */
	    fputs_filtered (" ", gdb_stdout);
	    wrap_here ("");
	    fputs_filtered ("and ", gdb_stdout);
	    wrap_here ("");
	    printf_filtered ("%s...", pst->dependencies[i]->filename);
	    wrap_here ("");     /* Flush output */
	    gdb_flush (gdb_stdout);
	  }
	psymtab_to_symtab_1 (pst->dependencies[i]);
      }

  per_cu = (struct dwarf2_per_cu_data *) pst->read_symtab_private;

  if (per_cu == NULL)
    {
      /* It's an include file, no symbols to read for it
         Everything is in the parent symtab.  */
      pst->readin = 1;
      return;
    }

  back_to = make_cleanup (dwarf2_release_queue, NULL);

  queue_comp_unit (per_cu);

  process_queue (pst->objfile);

  /* Age the cache, releasing compilation units that have not
     been used recently.  */
  age_cached_comp_units ();

  do_cleanups (back_to);
}

/* Load the DIEs associated with PST and PER_CU into memory.  */
/* APPLE LOCAL debug map: Accept an optional 2nd parameter ADDR_MAP */

static struct dwarf2_cu *
load_full_comp_unit (struct dwarf2_per_cu_data *per_cu,
                     struct oso_to_final_addr_map *addr_map)
{
  struct partial_symtab *pst = per_cu->psymtab;
  bfd *abfd = pst->objfile->obfd;
  struct dwarf2_cu *cu;
  unsigned long offset;
  char *info_ptr;
  struct cleanup *back_to, *free_cu_cleanup;
  struct attribute *attr;
  /* APPLE LOCAL avoid unused var warning. */
  /* CORE_ADDR baseaddr; */

  /* Set local variables from the partial symbol table info.  */
  offset = per_cu->offset;

  info_ptr = dwarf2_per_objfile->info_buffer + offset;

  cu = xmalloc (sizeof (struct dwarf2_cu));
  memset (cu, 0, sizeof (struct dwarf2_cu));

  /* If an error occurs while loading, release our storage.  */
  free_cu_cleanup = make_cleanup (free_one_comp_unit, cu);

  cu->objfile = pst->objfile;

  /* read in the comp_unit header  */
  info_ptr = read_comp_unit_head (&cu->header, info_ptr, abfd);

  /* Read the abbrevs for this compilation unit  */
  dwarf2_read_abbrevs (abfd, cu);
  back_to = make_cleanup (dwarf2_free_abbrev_table, cu);

  cu->header.offset = offset;

  /* APPLE LOCAL debug map */
  cu->addr_map = addr_map;

  cu->per_cu = per_cu;
  per_cu->cu = cu;

  /* We use this obstack for block values in dwarf_alloc_block.  */
  obstack_init (&cu->comp_unit_obstack);

  cu->dies = read_comp_unit (info_ptr, abfd, cu);

  /* We try not to read any attributes in this function, because not
     all objfiles needed for references have been loaded yet, and symbol
     table processing isn't initialized.  But we have to set the CU language,
     or we won't be able to build types correctly.  */
  attr = dwarf2_attr (cu->dies, DW_AT_language, cu);
  if (attr)
    set_cu_language (DW_UNSND (attr), cu);
  else
    set_cu_language (language_minimal, cu);

  do_cleanups (back_to);

  /* We've successfully allocated this compilation unit.  Let our caller
     clean it up when finished with it.  */
  discard_cleanups (free_cu_cleanup);

  return cu;
}

/* APPLE LOCAL begin inlined function symbols & blocks  */
static void
fix_inlined_subroutine_symbols (void)
{
  int j;
  struct symbol *sym;
  struct pending *current;

  if (!inlined_subroutine_symbols)
    return;

  for (current = inlined_subroutine_symbols; current; current = current->next)
    for (j = current->nsyms - 1; j >= 0; --j)
      {
	sym = current->symbol[j];
	BLOCK_FUNCTION (SYMBOL_BLOCK_VALUE (sym)) = NULL;
      }
}
/* APPLE LOCAL end inlined function symbols & blocks  */

/* Generate full symbol information for PST and CU, whose DIEs have
   already been loaded into memory.  */

static void
process_full_comp_unit (struct dwarf2_per_cu_data *per_cu)
{
  struct partial_symtab *pst = per_cu->psymtab;
  struct dwarf2_cu *cu = per_cu->cu;
  struct objfile *objfile = pst->objfile;
  /* APPLE LOCAL avoid unused var warning.  */
  /* bfd *abfd = objfile->obfd; */
  CORE_ADDR lowpc, highpc;
  struct symtab *symtab;
  struct cleanup *back_to;
  struct attribute *attr;
  CORE_ADDR baseaddr;

  baseaddr = objfile_text_section_offset (objfile);

  /* We're in the global namespace.  */
  processing_current_prefix = "";

  buildsym_init ();
  back_to = make_cleanup (really_free_pendings, NULL);

  cu->list_in_scope = &file_symbols;

  /* Find the base address of the compilation unit for range lists and
     location lists.  It will normally be specified by DW_AT_low_pc.
     In DWARF-3 draft 4, the base address could be overridden by
     DW_AT_entry_pc.  It's been removed, but GCC still uses this for
     compilation units with discontinuous ranges.  */

  cu->header.base_known = 0;
  cu->header.base_address_untranslated = 0;

  if (cu->header.base_known == 0)
    {
      attr = dwarf2_attr (cu->dies, DW_AT_low_pc, cu);
      if (attr)
	{
          cu->header.base_address_untranslated = DW_ADDR (attr);
	  cu->header.base_known = 1;
	}
    }

  if (cu->header.base_known == 0)
    {
      attr = dwarf2_attr (cu->dies, DW_AT_entry_pc, cu);
      if (attr)
        {
          cu->header.base_address_untranslated = DW_ADDR (attr);
          cu->header.base_known = 1;
        }
    }

  /* APPLE LOCAL: Last ditch effort, use the actual psymtab
     textlow as the compilation unit's base address.  Better than
     nothing.  */
  if (cu->header.base_known == 0)
    {
      cu->header.base_address_untranslated = cu->per_cu->psymtab->textlow;
      cu->header.base_known = 1;
    }

  /* Do line number decoding in read_file_scope () */
  /* APPLE LOCAL begin inlined function symbols & blocks  */
  inlined_subroutine_symbols = NULL;
  process_die (cu->dies, cu);
  fix_inlined_subroutine_symbols ();
  /* APPLE LOCAL end inlined function symbols & blocks  */

  /* Some compilers don't define a DW_AT_high_pc attribute for the
     compilation unit.  If the DW_AT_high_pc is missing, synthesize
     it, by scanning the DIE's below the compilation unit.  */
  get_scope_pc_bounds (cu->dies, &lowpc, &highpc, cu);

  /* APPLE LOCAL: If the debug information came from a debug map and DWARF
     in .o files, trust the psymtab's textlow/texthigh.  v. comment above
     regarding why.  */
  if (cu->addr_map != NULL)
    {
      lowpc = cu->per_cu->psymtab->textlow;
      highpc = cu->per_cu->psymtab->texthigh;
    }

  symtab = end_symtab (highpc, objfile, SECT_OFF_TEXT (objfile));

  /* Set symtab language to language from DW_AT_language.
     If the compilation is from a C file generated by language preprocessors,
     do not set the language if it was already deduced by start_subfile.  */
  if (symtab != NULL
      && !(cu->language == language_c && symtab->language != language_c))
    {
      symtab->language = cu->language;
    }
  pst->symtab = symtab;
  pst->readin = 1;

  do_cleanups (back_to);
}

/* Process a die and its children.  */

static void
process_die (struct die_info *die, struct dwarf2_cu *cu)
{
  switch (die->tag)
    {
    case DW_TAG_padding:
      break;
    case DW_TAG_compile_unit:
      read_file_scope (die, cu);
      break;
    case DW_TAG_subprogram:
      read_subroutine_type (die, cu);
      read_func_scope (die, cu);
      break;
    case DW_TAG_inlined_subroutine:
      /* APPLE LOCAL begin subroutine inlining  */
      if (dwarf2_allow_inlined_stepping)
	{
	  read_subroutine_type (die, cu);
	  read_inlined_subroutine_scope (die, cu);
	}
      /* APPLE LOCAL end subroutine inlining  */
      break;
    case DW_TAG_lexical_block:
    case DW_TAG_try_block:
    case DW_TAG_catch_block:
      read_lexical_block_scope (die, cu);
      break;
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
      read_structure_type (die, cu);
      process_structure_scope (die, cu);
      break;
    case DW_TAG_enumeration_type:
      read_enumeration_type (die, cu);
      process_enumeration_scope (die, cu);
      break;

    /* FIXME drow/2004-03-14: These initialize die->type, but do not create
       a symbol or process any children.  Therefore it doesn't do anything
       that won't be done on-demand by read_type_die.  */
    case DW_TAG_subroutine_type:
      read_subroutine_type (die, cu);
      break;
    case DW_TAG_set_type:
      read_set_type (die, cu);
      break;
    case DW_TAG_array_type:
      read_array_type (die, cu);
      break;
    case DW_TAG_pointer_type:
      read_tag_pointer_type (die, cu);
      break;
    case DW_TAG_ptr_to_member_type:
      read_tag_ptr_to_member_type (die, cu);
      break;
    case DW_TAG_reference_type:
      read_tag_reference_type (die, cu);
      break;
    case DW_TAG_string_type:
      read_tag_string_type (die, cu);
      break;
    /* END FIXME */

    case DW_TAG_base_type:
      read_base_type (die, cu);
      /* Add a typedef symbol for the type definition, if it has a
	 DW_AT_name.  */
      new_symbol (die, die->type, cu);
      break;
    case DW_TAG_subrange_type:
      read_subrange_type (die, cu);
      /* Add a typedef symbol for the type definition, if it has a
         DW_AT_name.  */
      new_symbol (die, die->type, cu);
      break;
    case DW_TAG_common_block:
      read_common_block (die, cu);
      break;
    case DW_TAG_common_inclusion:
      break;
    case DW_TAG_namespace:
      processing_has_namespace_info = 1;
      read_namespace (die, cu);
      break;
    case DW_TAG_imported_declaration:
    case DW_TAG_imported_module:
      /* FIXME: carlton/2002-10-16: Eventually, we should use the
	 information contained in these.  DW_TAG_imported_declaration
	 dies shouldn't have children; DW_TAG_imported_module dies
	 shouldn't in the C++ case, but conceivably could in the
	 Fortran case, so we'll have to replace this gdb_assert if
	 Fortran compilers start generating that info.  */
      processing_has_namespace_info = 1;
      gdb_assert (die->child == NULL);
      break;
    /* APPLE LOCAL: Handle DW_TAG_const_type.  */
    case DW_TAG_const_type:
      read_type_die (die, cu);
      break;
    /* APPLE LOCAL: If we pass a NULL type to new_symbol, then
       the type registered in the symbol for the typedef will
       be the target of the typedef, not the typedef itself.  
       That's wrong, so process the typedef, and pass it's
       type to new_symbol.  */
    case DW_TAG_typedef:
      read_typedef (die, cu);
      new_symbol (die, die->type, cu);
      break;
    default:
      new_symbol (die, NULL, cu);
      break;
    }
}

static void
initialize_cu_func_list (struct dwarf2_cu *cu)
{
  cu->first_fn = cu->last_fn = cu->cached_fn = NULL;
}

static void
read_file_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct comp_unit_head *cu_header = &cu->header; */
  struct cleanup *back_to = make_cleanup (null_cleanup, 0);
  CORE_ADDR lowpc = ((CORE_ADDR) -1);
  CORE_ADDR highpc = ((CORE_ADDR) 0);
  struct attribute *attr;
  char *name = "<unknown>";
  char *comp_dir = NULL;
  struct die_info *child_die;
  bfd *abfd = objfile->obfd;
  struct line_header *line_header = 0;
  CORE_ADDR baseaddr;
  
  baseaddr = objfile_text_section_offset (objfile);

  /* APPLE LOCAL: If we have a debug map and reasonable looking
     textlow/texthigh values in the psymtab, use them.  These were
     set while scanning the N_FUN debug map stabs and are a good
     measurement of the maximum pc address range of this compilation
     unit in the final linked executable.  */

  if (cu->addr_map 
      && cu->per_cu->psymtab->textlow != -1 
      && cu->per_cu->psymtab->textlow != baseaddr
      && cu->per_cu->psymtab->texthigh != 0)
    {
      /* The psymtab addresses already have the slide applied.  */
      lowpc = cu->per_cu->psymtab->textlow;
      highpc = cu->per_cu->psymtab->texthigh;
    }
  else
    {
      get_scope_pc_bounds (die, &lowpc, &highpc, cu);

      /* If we didn't find a lowpc, set it to highpc to avoid complaints
         from finish_block.  */
      if (lowpc == ((CORE_ADDR) -1))
        lowpc = highpc;
      lowpc += baseaddr;
      highpc += baseaddr;
    }

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr)
    {
      name = DW_STRING (attr);
    }
  attr = dwarf2_attr (die, DW_AT_comp_dir, cu);
  if (attr)
    {
      comp_dir = DW_STRING (attr);
      if (comp_dir)
	{
	  /* Irix 6.2 native cc prepends <machine>.: to the compilation
	     directory, get rid of it.  */
	  char *cp = strchr (comp_dir, ':');

	  if (cp && cp != comp_dir && cp[-1] == '.' && cp[1] == '/')
	    comp_dir = cp + 1;
	}
      /* APPLE LOCAL: Retain the compilation directory.  */
      cu->comp_dir = comp_dir;
    }

  attr = dwarf2_attr (die, DW_AT_language, cu);
  if (attr)
    {
      set_cu_language (DW_UNSND (attr), cu);
    }

  attr = dwarf2_attr (die, DW_AT_producer, cu);
  if (attr) 
    cu->producer = DW_STRING (attr);
  
  /* We assume that we're processing GCC output. */
  processing_gcc_compilation = 2;
#if 0
  /* FIXME:Do something here.  */
  if (dip->at_producer != NULL)
    {
      handle_producer (dip->at_producer);
    }
#endif

  /* The compilation unit may be in a different language or objfile,
     zero out all remembered fundamental types.  */
  memset (cu->ftypes, 0, FT_NUM_MEMBERS * sizeof (struct type *));

  start_symtab (name, comp_dir, lowpc);
  record_debugformat ("DWARF 2");
  record_producer (cu->producer);

  initialize_cu_func_list (cu);

  /* Process all dies in compilation unit.  */
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }

  /* Decode line number information if present.  */
  attr = dwarf2_attr (die, DW_AT_stmt_list, cu);
  if (attr)
    {
      unsigned int line_offset = DW_UNSND (attr);
      line_header = dwarf_decode_line_header (line_offset, abfd, cu);
      if (line_header)
        {
          make_cleanup ((make_cleanup_ftype *) free_line_header,
                        (void *) line_header);
          dwarf_decode_lines (line_header, comp_dir, abfd, cu, NULL);
        }
    }

  /* Decode macro information, if present.  Dwarf 2 macro information
     refers to information in the line number info statement program
     header, so we can only read it if we've read the header
     successfully.  */
  attr = dwarf2_attr (die, DW_AT_macro_info, cu);
  if (attr && line_header)
    {
      unsigned int macro_offset = DW_UNSND (attr);
      dwarf_decode_macros (line_header, macro_offset,
                           comp_dir, abfd, cu);
    }
  do_cleanups (back_to);
}

static void
add_to_cu_func_list (const char *name, CORE_ADDR lowpc, CORE_ADDR highpc,
		     struct dwarf2_cu *cu)
{
  struct function_range *thisfn;

  thisfn = (struct function_range *)
    obstack_alloc (&cu->comp_unit_obstack, sizeof (struct function_range));
  thisfn->name = name;
  thisfn->lowpc = lowpc;
  thisfn->highpc = highpc;
  thisfn->seen_line = 0;
  thisfn->next = NULL;

  if (cu->last_fn == NULL)
      cu->first_fn = thisfn;
  else
      cu->last_fn->next = thisfn;

  cu->last_fn = thisfn;
}

/* APPLE LOCAL begin subroutine inlining  */
static void
dwarf2_add_to_list_of_inlined_calls (struct objfile *objfile,
				     struct attribute *file_attr, 
				     struct attribute *line_attr, 
				     struct attribute *column_attr,
				     CORE_ADDR lowpc, CORE_ADDR highpc,
				     /* APPLE LOCAL - address ranges  */
				     struct address_range_list *ranges,
				     char *name, char *parent_name,
				     struct attribute *decl_file,
				     /* APPLE LOCAL begin radar 6545149 */
				     struct attribute *decl_line,
				     struct symbol *func_sym)
				     /* APPLE LOCAL end radar 6545149 */
{
  struct dwarf_inlined_call_record *new;
  struct rb_tree_node *new_call_site;

  new = (struct dwarf_inlined_call_record *) xmalloc 
                                          (sizeof (struct dwarf_inlined_call_record));
  new->file_index = DW_UNSND (file_attr);
  new->line = DW_UNSND (line_attr);
  if (column_attr)
    new->column = DW_UNSND (column_attr);
  else
    new->column = 0;
  new->lowpc = lowpc;
  new->highpc = highpc;
  new->name = xstrdup (name);
  /* APPLE LOCAL begin address ranges  */
  new->ranges = ranges;
  /* APPLE LOCAL end address ranges  */
  new->parent_name = xstrdup (parent_name);

  if (decl_file)
    new->decl_file_index = DW_UNSND (decl_file);
  else
    new->decl_file_index = 0;

  if (decl_line)
    new->decl_line = DW_UNSND (decl_line);
  else
    new->decl_line = 0;

  /* APPLE LOCAL radar 6545149 */
  new->func_sym = func_sym;
  
  /* Now wrap call_site information into red-black tree node, for quick
     insertion/lookup in list of call sites.  */

  new_call_site = (struct rb_tree_node *) xmalloc (sizeof(struct rb_tree_node));
  new_call_site->key = new->lowpc;
  new_call_site->secondary_key = new->file_index;
  new_call_site->third_key = new->highpc;
  new_call_site->data = (void *) new;
  new_call_site->left = NULL;
  new_call_site->right = NULL;
  new_call_site->parent = NULL;
  new_call_site->color = UNINIT;

  /* Call site information is stored in a Red-Black Tree
     (sort-of-balanced binary tree).  It is sorted by three keys:
     Primary key is the lowpc (starting) address for the inlining;
     secondary key is the file index of the file containing the
     inlined function call; Third key is the highpc (ending) address
     for the inlining.  The information stored here is used in
     check_inlined_function_calls to put the appropriate information in
     the appropriate files' line tables.  */

  rb_tree_insert (&(objfile->inlined_call_sites), objfile->inlined_call_sites, 
		  new_call_site);
}


static void
read_inlined_subroutine_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct die_info *parent_die;
  struct die_info *abstract_origin;
  struct objfile *objfile = cu->objfile;
  CORE_ADDR lowpc;
  CORE_ADDR highpc;
  CORE_ADDR baseaddr;
  struct attribute *file_attr;
  struct attribute *line_attr;
  struct attribute *column_attr;
  struct attribute *decl_file = NULL;
  struct attribute *decl_line = NULL;
  struct attribute *abs_orig_attr = NULL;
  char *parent_name;
  char *name;
  struct die_info *child_die;
  /* APPLE LOCAL begin address ranges  */
  struct address_range_list *ranges = NULL;
  /* APPLE LOCAL end address ranges  */
  struct context_stack *new;

  parent_die = die->parent;

  baseaddr = objfile_text_section_offset (objfile);
  /* APPLE LOCAL begin address ranges  */
  if (!dwarf2_get_pc_bounds (die, &lowpc, &highpc, &ranges, cu))
    return;

  lowpc += baseaddr;
  highpc += baseaddr;

  if (ranges)
    {
      int i;
      for (i = 0; i < ranges->nelts; i++)
	{
	  ranges->ranges[i].startaddr += baseaddr;
	  ranges->ranges[i].endaddr += baseaddr;
	}
    }
  /* APPLE LOCAL end address ranges  */

  while (parent_die->tag == DW_TAG_lexical_block)
    parent_die = parent_die->parent;

  parent_name = dwarf2_linkage_name (parent_die, cu);
  name = dwarf2_linkage_name (die, cu);

  /* Add call_site file, line #, and lowpc to line table (waiting list)!  */
  
  file_attr = dwarf2_attr (die, DW_AT_call_file, cu);
  line_attr = dwarf2_attr (die, DW_AT_call_line, cu);
  column_attr = dwarf2_attr (die, DW_AT_call_column, cu);

  abs_orig_attr = dwarf2_attr (die, DW_AT_abstract_origin, cu);
  if (abs_orig_attr)
    {
      abstract_origin = follow_die_ref (die, abs_orig_attr, cu);
      if (!dwarf2_attr (abstract_origin, DW_AT_low_pc, cu))
	{
	  decl_file = dwarf2_attr (abstract_origin, DW_AT_decl_file, cu);
	  decl_line = dwarf2_attr (abstract_origin, DW_AT_decl_line, cu);
	}
    }

  /* APPLE LOCAL begin address ranges  */
  /* Fix up lowpc & highpc in presence of address ranges  */

  if (ranges && ranges->nelts > 0)
    {
      if (lowpc != ranges->ranges[0].startaddr)
	lowpc = ranges->ranges[0].startaddr;
      if (highpc != ranges->ranges[0].endaddr)
	highpc = ranges->ranges[0].endaddr;
    }
  /* APPLE LOCAL end address ranges  */

  /* APPLE LOCAL begin inlined function symbols & blocks  */
  /* APPLE LOCAL begin radar 6545149  - moved code to here (see below).  */
  new = push_context (0, lowpc);
  new->name = new_symbol (die, die->type, cu);
  add_symbol_to_inlined_subroutine_list (new->name, 
					 &inlined_subroutine_symbols);
  /* APPLE LOCAL end radar 6545149  - moved code to here.  */

  /* Check to make sure the compiler found the call site information before
     we try to make use of it.  Also make sure we have names for the caller
     and callee functions.  */

  if (file_attr 
      && line_attr
      && decl_file
      && decl_line
      && name
      && parent_name)
    /* APPLE LOCAL begin address ranges  */
    dwarf2_add_to_list_of_inlined_calls (objfile, file_attr, line_attr, 
					 column_attr, lowpc, highpc, ranges, 
					 name, parent_name, decl_file, 
					 /* APPLE LOCAL radar 6545149  */
					 decl_line, new->name);
    /* APPLE LOCAL end address ranges  */

  /* APPLE LOCAL radar 6545149  - moved code from here (see above).  */

  cu->list_in_scope = &local_symbols;
  /* APPLE LOCAL end inlined function symbols & blocks  */
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }
  new = pop_context ();

  /* APPLE LOCAL begin inlined function symbols & blocks  */
  finish_block (new->name, &local_symbols, new->old_blocks, lowpc,
		highpc, ranges, objfile);

  local_symbols = new->locals;
  param_symbols = new->params;
  /* APPLE LOCAL end inlined function symbols & blocks  */
}
/* APPLE LOCAL end subroutine inlining  */

static void
read_func_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct context_stack *new;
  CORE_ADDR lowpc;
  CORE_ADDR highpc;
  struct die_info *child_die;
  struct attribute *attr;
  char *name;
  const char *previous_prefix = processing_current_prefix;
  struct cleanup *back_to = NULL;
  CORE_ADDR baseaddr;
  /* APPLE LOCAL begin address ranges  */
  struct address_range_list *ranges = NULL;
  /* APPLE LOCAL end address ranges  */

  baseaddr = objfile_text_section_offset (objfile);

  name = dwarf2_linkage_name (die, cu);

  /* Ignore functions with missing or empty names and functions with
     missing or invalid low and high pc attributes.  */
  /* APPLE LOCAL begin address ranges  */
  if (name == NULL 
      || !dwarf2_get_pc_bounds (die, &lowpc, &highpc, &ranges, cu))
    return;
  /* APPLE LOCAL end address ranges  */

  if (cu->language == language_cplus
      || cu->language == language_objcplus
      || cu->language == language_java)
    {
      struct die_info *spec_die = die_specification (die, cu);

      /* NOTE: carlton/2004-01-23: We have to be careful in the
         presence of DW_AT_specification.  For example, with GCC 3.4,
         given the code

           namespace N {
             void foo() {
               // Definition of N::foo.
             }
           }

         then we'll have a tree of DIEs like this:

         1: DW_TAG_compile_unit
           2: DW_TAG_namespace        // N
             3: DW_TAG_subprogram     // declaration of N::foo
           4: DW_TAG_subprogram       // definition of N::foo
                DW_AT_specification   // refers to die #3

         Thus, when processing die #4, we have to pretend that we're
         in the context of its DW_AT_specification, namely the contex
         of die #3.  */
	
      if (spec_die != NULL)
	{
	  char *specification_prefix = determine_prefix (spec_die, cu);
	  processing_current_prefix = specification_prefix;
	  back_to = make_cleanup (xfree, specification_prefix);
	}
    }

  lowpc += baseaddr;
  highpc += baseaddr;

  /* APPLE LOCAL begin address ranges  */
  if (ranges)
    {
      int i;
      for (i = 0; i < ranges->nelts; i++)
	{
	  ranges->ranges[i].startaddr += baseaddr;
	  ranges->ranges[i].endaddr += baseaddr;
	}
    }
  /* APPLE LOCAL end address ranges  */

  /* Record the function range for dwarf_decode_lines.  */
  add_to_cu_func_list (name, lowpc, highpc, cu);

  new = push_context (0, lowpc);
  new->name = new_symbol (die, die->type, cu);

  /* If there is a location expression for DW_AT_frame_base, record
     it.  */
  attr = dwarf2_attr (die, DW_AT_frame_base, cu);
  if (attr)
    /* FIXME: cagney/2004-01-26: The DW_AT_frame_base's location
       expression is being recorded directly in the function's symbol
       and not in a separate frame-base object.  I guess this hack is
       to avoid adding some sort of frame-base adjunct/annex to the
       function's symbol :-(.  The problem with doing this is that it
       results in a function symbol with a location expression that
       has nothing to do with the location of the function, ouch!  The
       relationship should be: a function's symbol has-a frame base; a
       frame-base has-a location expression.  */
    dwarf2_symbol_mark_computed (attr, new->name, cu);

  cu->list_in_scope = &local_symbols;

  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }

  new = pop_context ();
  /* Make a block for the local symbols within.  */
  /* APPLE LOCAL begin address ranges  */
  finish_block (new->name, &local_symbols, new->old_blocks,
		lowpc, highpc, ranges, objfile);
  /* APPLE LOCAL end address ranges  */
  
  /* In C++, we can have functions nested inside functions (e.g., when
     a function declares a class that has methods).  This means that
     when we finish processing a function scope, we may need to go
     back to building a containing block's symbol lists.  */
  local_symbols = new->locals;
  param_symbols = new->params;

  /* If we've finished processing a top-level function, subsequent
     symbols go in the file symbol list.  */
  if (outermost_context_p ())
    cu->list_in_scope = &file_symbols;

  processing_current_prefix = previous_prefix;
  if (back_to != NULL)
    do_cleanups (back_to);
}

/* Process all the DIES contained within a lexical block scope.  Start
   a new scope, process the dies, and then close the scope.  */

static void
read_lexical_block_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct context_stack *new;
  CORE_ADDR lowpc, highpc;
  struct die_info *child_die;
  CORE_ADDR baseaddr;
  /* APPLE LOCAL begin address ranges  */
  struct address_range_list *ranges = NULL;
  /* APPLE LOCAL end address ranges  */

  baseaddr = objfile_text_section_offset (objfile);

  /* Ignore blocks with missing or invalid low and high pc attributes.  */
  /* ??? Perhaps consider discontiguous blocks defined by DW_AT_ranges
     as multiple lexical blocks?  Handling children in a sane way would
     be nasty.  Might be easier to properly extend generic blocks to 
     describe ranges.  */
  /* APPLE LOCAL begin address ranges  */
  if (!dwarf2_get_pc_bounds (die, &lowpc, &highpc, &ranges, cu))
    return;
  lowpc += baseaddr;
  highpc += baseaddr;

  if (ranges)
    {
      int i;
      for (i = 0; i < ranges->nelts; i++)
	{
	  ranges->ranges[i].startaddr += baseaddr;
	  ranges->ranges[i].endaddr += baseaddr;
	}
    }
  /* APPLE LOCAL end address ranges  */

  push_context (0, lowpc);
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }
  new = pop_context ();

  if (local_symbols != NULL)
    {
      /* APPLE LOCAL begin address ranges  */
      finish_block (0, &local_symbols, new->old_blocks, new->start_addr,
		    highpc, ranges, objfile);
      /* APPLE LOCAL end address ranges  */
    }
  local_symbols = new->locals;
}

/* APPLE LOCAL begin address ranges  */
/* Get low and high pc attributes from a die.  Return 1 if the attributes
   are present and valid, otherwise, return 0.  Return -1 if the range is
   discontinuous, i.e. derived from DW_AT_ranges information.  Return the
   actual discontinuous range in RANGES. */
/* APPLE LOCAL: What is returned in the case of a discontiguous range?
   I say the minimal low pc and the maximal high pc should be returned.  */
static int
dwarf2_get_pc_bounds (struct die_info *die, CORE_ADDR *lowpc, 
		      CORE_ADDR *highpc, struct address_range_list **ranges,
		      struct dwarf2_cu *cu)
/* APPLE LOCAL end address ranges  */
{
  struct objfile *objfile = cu->objfile;
  /* APPLE LOCAL begin address ranges  */
  struct address_range_list *tmp_ranges = NULL;
  /* APPLE LOCAL end address ranges  */
  struct comp_unit_head *cu_header = &cu->header;
  struct attribute *attr;
  bfd *obfd = objfile->obfd;
  CORE_ADDR low = 0;
  CORE_ADDR high = 0;
  int ret = 0;

  /* APPLE LOCAL begin address ranges  */
  *ranges = NULL;
  /* APPLE LOCAL end address ranges  */
  attr = dwarf2_attr (die, DW_AT_high_pc, cu);
  if (attr)
    {
      high = DW_ADDR (attr);
      attr = dwarf2_attr (die, DW_AT_low_pc, cu);
      if (attr)
	low = DW_ADDR (attr);
      else
	/* Found high w/o low attribute.  */
	return 0;

      /* Found consecutive range of addresses.  */
      ret = 1;
      /* APPLE LOCAL: debug map */
      if (!translate_debug_map_address (cu->addr_map, low,  &low,  0) ||
          !translate_debug_map_address (cu->addr_map, high, &high, 1))
        return 0;
    }
  else
    {
      attr = dwarf2_attr (die, DW_AT_ranges, cu);
      /* APPLE LOCAL: Add an attr == NULL case so we don't try to translate
         0's all the time and add debug debugmap spew.  */
      if (attr == NULL)
	{
          /* No DW_AT_high_pc or DW_AT_ranges -- no addresses for this DIE.  */
          return 0;
	}
      else
	{
	  unsigned int addr_size = cu_header->addr_size;
	  CORE_ADDR mask = ~(~(CORE_ADDR)1 << (addr_size * 8 - 1));
	  /* Value of the DW_AT_ranges attribute is the offset in the
	     .debug_ranges section.  */
	  unsigned int offset = DW_UNSND (attr);
	  /* Base address selection entry.  */
	  CORE_ADDR base;
	  int found_base;
	  int dummy;
	  char *buffer;
	  CORE_ADDR marker;
	  /* APPLE LOCAL begin address ranges  */
	  int max_elts;
	  int cur_elt = 0;

	  tmp_ranges = (struct address_range_list *) 
	                       xmalloc (sizeof (struct address_range_list));

	  max_elts = 10;
	  tmp_ranges->nelts = 0;
	  tmp_ranges->ranges = (struct address_range *) 
	                  xmalloc (max_elts * sizeof (struct address_range));

	  memset (tmp_ranges->ranges, 0, 
		  max_elts * sizeof (struct address_range));
	  /* APPLE LOCAL end address ranges  */

	  found_base = cu_header->base_known;
	  base = cu_header->base_address_untranslated;

	  if (offset >= dwarf2_per_objfile->ranges_size)
	    {
	      complaint (&symfile_complaints,
	                 _("Offset %d out of bounds for DW_AT_ranges attribute"),
			 offset);
	      return 0;
	    }
	  buffer = dwarf2_per_objfile->ranges_buffer + offset;

	  /* Read in the largest possible address.  */
	  marker = read_address (obfd, buffer, cu, &dummy);
	  if ((marker & mask) == mask)
	    {
	      /* If we found the largest possible address, then
		 read the base address.  */
	      base = read_address (obfd, buffer + addr_size, cu, &dummy);
	      buffer += 2 * addr_size;
	      offset += 2 * addr_size;
	      found_base = 1;
	    }

          low = ~((CORE_ADDR) 0);  /* Maximum possible unsigned CORE_ADDR val */
          high = 0;

	  while (1)
	    {
	      CORE_ADDR range_beginning, range_end;

	      /* APPLE LOCAL begin address ranges  */
	      if (cur_elt >= max_elts)
		{
		  max_elts = 2 * max_elts;
		  tmp_ranges->ranges = (struct address_range *) xrealloc
		    (tmp_ranges->ranges, 
		     max_elts * sizeof (struct address_range));
		}
	      /* APPLE LOCAL end address ranges  */

	      range_beginning = read_address (obfd, buffer, cu, &dummy);
	      buffer += addr_size;
	      range_end = read_address (obfd, buffer, cu, &dummy);
	      buffer += addr_size;
	      offset += 2 * addr_size;

	      /* An end of list marker is a pair of zero addresses.  */
	      if (range_beginning == 0 && range_end == 0)
		/* Found the end of list entry.  */
		break;

	      /* Each base address selection entry is a pair of 2 values.
		 The first is the largest possible address, the second is
		 the base address.  Check for a base address here.  */
	      if ((range_beginning & mask) == mask)
		{
		  /* If we found the largest possible address, then
		     read the base address.  */
		  base = read_address (obfd, buffer + addr_size, cu, &dummy);
		  found_base = 1;
		  continue;
		}

	      if (!found_base)
		{
		  /* We have no valid base address for the ranges
		     data.  */
		  complaint (&symfile_complaints,
			     _("Invalid .debug_ranges data (no base address)"));
		  return 0;
		}

	      range_beginning += base;
	      range_end += base;

	      if (!translate_debug_map_address (cu->addr_map, range_beginning,
						&range_beginning, 0)
		  || !translate_debug_map_address (cu->addr_map, range_end,
						   &range_end, 1))
		return 0;

              low = min (low, range_beginning);
              high = max (high, range_end);

	      tmp_ranges->ranges[cur_elt].startaddr = range_beginning;
	      tmp_ranges->ranges[cur_elt].endaddr   = range_end;
	      cur_elt++;
	      tmp_ranges->nelts = cur_elt;
	    }

	  *ranges = tmp_ranges;

	  /* If the first entry is an end-of-list marker, the range
	     describes an empty scope, i.e. no instructions.  */
	  if ((low == ~((CORE_ADDR) 0)) && high == 0)
	    return 0;

	  ret = -1;
	}
    }

  if (high < low)
    return 0;

  /* When using the GNU linker, .gnu.linkonce. sections are used to
     eliminate duplicate copies of functions and vtables and such.
     The linker will arbitrarily choose one and discard the others.
     The AT_*_pc values for such functions refer to local labels in
     these sections.  If the section from that file was discarded, the
     labels are not in the output, so the relocs get a value of 0.
     If this is a discarded function, mark the pc bounds as invalid,
     so that GDB will ignore it.  */
  if (low == 0 && (bfd_get_file_flags (obfd) & HAS_RELOC) == 0)
    return 0;

  *lowpc = low;
  *highpc = high;
  return ret;
}

/* Get the low and high pc's represented by the scope DIE, and store
   them in *LOWPC and *HIGHPC.  If the correct values can't be
   determined, set *LOWPC to -1 and *HIGHPC to 0.  */

static void
get_scope_pc_bounds (struct die_info *die,
		     CORE_ADDR *lowpc, CORE_ADDR *highpc,
		     struct dwarf2_cu *cu)
{
  CORE_ADDR best_low = (CORE_ADDR) -1;
  CORE_ADDR best_high = (CORE_ADDR) 0;
  CORE_ADDR current_low, current_high;
  /* APPLE LOCAL begin address ranges  */
  struct address_range_list *ranges = NULL;

  if (dwarf2_get_pc_bounds (die, &current_low, &current_high, &ranges, cu))
   /* APPLE LOCAL end address ranges  */
    {
      best_low = current_low;
      best_high = current_high;
    }
  else
    {
      struct die_info *child = die->child;

      while (child && child->tag)
	{
	  switch (child->tag) {
	  case DW_TAG_subprogram:
	    /* APPLE LOCAL begin address ranges  */
	    /* FIXME:  I'm not sure the logic here is correct in the
	       presence of multiple non-contiguous address ranges.   */
	    if (dwarf2_get_pc_bounds (child, &current_low, &current_high, 
				      &ranges, cu))
	    /* APPLE LOCAL end address ranges  */
	      {
		best_low = min (best_low, current_low);
		best_high = max (best_high, current_high);
	      }
	    break;
	  case DW_TAG_namespace:
	    /* FIXME: carlton/2004-01-16: Should we do this for
	       DW_TAG_class_type/DW_TAG_structure_type, too?  I think
	       that current GCC's always emit the DIEs corresponding
	       to definitions of methods of classes as children of a
	       DW_TAG_compile_unit or DW_TAG_namespace (as opposed to
	       the DIEs giving the declarations, which could be
	       anywhere).  But I don't see any reason why the
	       standards says that they have to be there.  */
	    get_scope_pc_bounds (child, &current_low, &current_high, cu);

	    if (current_low != ((CORE_ADDR) -1))
	      {
		best_low = min (best_low, current_low);
		best_high = max (best_high, current_high);
	      }
	    break;
	  default:
	    /* Ignore. */
	    break;
	  }

	  child = sibling_die (child);
	}
    }

  *lowpc = best_low;
  *highpc = best_high;
}

/* Add an aggregate field to the field list.  */

static void
dwarf2_add_field (struct field_info *fip, struct die_info *die,
		  struct dwarf2_cu *cu)
{ 
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct objfile *objfile = cu->objfile; */
  struct nextfield *new_field;
  struct attribute *attr;
  struct field *fp;
  char *fieldname = "";

  /* Allocate a new field list entry and link it in.  */
  new_field = (struct nextfield *) xmalloc (sizeof (struct nextfield));
  make_cleanup (xfree, new_field);
  memset (new_field, 0, sizeof (struct nextfield));
  new_field->next = fip->fields;
  fip->fields = new_field;
  fip->nfields++;

  /* Handle accessibility and virtuality of field.
     The default accessibility for members is public, the default
     accessibility for inheritance is private.  */
  if (die->tag != DW_TAG_inheritance)
    new_field->accessibility = DW_ACCESS_public;
  else
    new_field->accessibility = DW_ACCESS_private;
  new_field->virtuality = DW_VIRTUALITY_none;

  attr = dwarf2_attr (die, DW_AT_accessibility, cu);
  if (attr)
    new_field->accessibility = DW_UNSND (attr);
  if (new_field->accessibility != DW_ACCESS_public)
    fip->non_public_fields = 1;
  attr = dwarf2_attr (die, DW_AT_virtuality, cu);
  if (attr)
    new_field->virtuality = DW_UNSND (attr);

  fp = &new_field->field;

  if (die->tag == DW_TAG_member && ! die_is_declaration (die, cu))
    {
      /* Data member other than a C++ static data member.  */
      
      /* Get type of field.  */
      fp->type = die_type (die, cu);

      FIELD_STATIC_KIND (*fp) = 0;

      /* Get bit size of field (zero if none).  */
      attr = dwarf2_attr (die, DW_AT_bit_size, cu);
      if (attr)
	{
	  FIELD_BITSIZE (*fp) = DW_UNSND (attr);
	}
      else
	{
	  FIELD_BITSIZE (*fp) = 0;
	}

      /* Get bit offset of field.  */
      attr = dwarf2_attr (die, DW_AT_data_member_location, cu);
      if (attr)
	{
	  FIELD_BITPOS (*fp) =
	    decode_locdesc (DW_BLOCK (attr), cu) * bits_per_byte;
	}
      else
	FIELD_BITPOS (*fp) = 0;
      attr = dwarf2_attr (die, DW_AT_bit_offset, cu);
      if (attr)
	{
	  if (BITS_BIG_ENDIAN)
	    {
	      /* For big endian bits, the DW_AT_bit_offset gives the
	         additional bit offset from the MSB of the containing
	         anonymous object to the MSB of the field.  We don't
	         have to do anything special since we don't need to
	         know the size of the anonymous object.  */
	      FIELD_BITPOS (*fp) += DW_UNSND (attr);
	    }
	  else
	    {
	      /* For little endian bits, compute the bit offset to the
	         MSB of the anonymous object, subtract off the number of
	         bits from the MSB of the field to the MSB of the
	         object, and then subtract off the number of bits of
	         the field itself.  The result is the bit offset of
	         the LSB of the field.  */
	      int anonymous_size;
	      int bit_offset = DW_UNSND (attr);

	      attr = dwarf2_attr (die, DW_AT_byte_size, cu);
	      if (attr)
		{
		  /* The size of the anonymous object containing
		     the bit field is explicit, so use the
		     indicated size (in bytes).  */
		  anonymous_size = DW_UNSND (attr);
		}
	      else
		{
		  /* The size of the anonymous object containing
		     the bit field must be inferred from the type
		     attribute of the data member containing the
		     bit field.  */
		  anonymous_size = TYPE_LENGTH (fp->type);
		}
	      FIELD_BITPOS (*fp) += anonymous_size * bits_per_byte
		- bit_offset - FIELD_BITSIZE (*fp);
	    }
	}

      /* Get name of field.  */
      attr = dwarf2_attr (die, DW_AT_name, cu);
      if (attr && DW_STRING (attr))
	fieldname = DW_STRING (attr);

      /* The name is already allocated along with this objfile, so we don't
	 need to duplicate it for the type.  */
      fp->name = fieldname;

      /* Change accessibility for artificial fields (e.g. virtual table
         pointer or virtual base class pointer) to private.  */
      if (dwarf2_attr (die, DW_AT_artificial, cu))
	{
	  new_field->accessibility = DW_ACCESS_private;
	  fip->non_public_fields = 1;
	}
    }
  else if (die->tag == DW_TAG_member || die->tag == DW_TAG_variable)
    {
      /* C++ static member.  */

      /* NOTE: carlton/2002-11-05: It should be a DW_TAG_member that
	 is a declaration, but all versions of G++ as of this writing
	 (so through at least 3.2.1) incorrectly generate
	 DW_TAG_variable tags.  */
      
      char *physname;

      /* Get name of field.  */
      attr = dwarf2_attr (die, DW_AT_name, cu);
      if (attr && DW_STRING (attr))
	fieldname = DW_STRING (attr);
      else
	return;

      /* Get physical name.  */
      physname = dwarf2_linkage_name (die, cu);

      /* The name is already allocated along with this objfile, so we don't
	 need to duplicate it for the type.  */
      SET_FIELD_PHYSNAME (*fp, physname ? physname : "");
      FIELD_TYPE (*fp) = die_type (die, cu);
      FIELD_NAME (*fp) = fieldname;
    }
  else if (die->tag == DW_TAG_inheritance)
    {
      /* C++ base class field.  */
      attr = dwarf2_attr (die, DW_AT_data_member_location, cu);
      if (attr)
	FIELD_BITPOS (*fp) = (decode_locdesc (DW_BLOCK (attr), cu)
			      * bits_per_byte);
      FIELD_BITSIZE (*fp) = 0;
      FIELD_STATIC_KIND (*fp) = 0;
      FIELD_TYPE (*fp) = die_type (die, cu);
      FIELD_NAME (*fp) = type_name_no_tag (fp->type);
      fip->nbaseclasses++;
    }
}

/* Create the vector of fields, and attach it to the type.  */

static void
dwarf2_attach_fields_to_type (struct field_info *fip, struct type *type,
			      struct dwarf2_cu *cu)
{
  int nfields = fip->nfields;

  /* Record the field count, allocate space for the array of fields,
     and create blank accessibility bitfields if necessary.  */
  TYPE_NFIELDS (type) = nfields;
  TYPE_FIELDS (type) = (struct field *)
    TYPE_ALLOC (type, sizeof (struct field) * nfields);
  memset (TYPE_FIELDS (type), 0, sizeof (struct field) * nfields);

  if (fip->non_public_fields)
    {
      ALLOCATE_CPLUS_STRUCT_TYPE (type);

      TYPE_FIELD_PRIVATE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PRIVATE_BITS (type), nfields);

      TYPE_FIELD_PROTECTED_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_PROTECTED_BITS (type), nfields);

      TYPE_FIELD_IGNORE_BITS (type) =
	(B_TYPE *) TYPE_ALLOC (type, B_BYTES (nfields));
      B_CLRALL (TYPE_FIELD_IGNORE_BITS (type), nfields);
    }

  /* If the type has baseclasses, allocate and clear a bit vector for
     TYPE_FIELD_VIRTUAL_BITS.  */
  if (fip->nbaseclasses)
    {
      int num_bytes = B_BYTES (fip->nbaseclasses);
      char *pointer;

      ALLOCATE_CPLUS_STRUCT_TYPE (type);
      pointer = (char *) TYPE_ALLOC (type, num_bytes);
      TYPE_FIELD_VIRTUAL_BITS (type) = (B_TYPE *) pointer;
      B_CLRALL (TYPE_FIELD_VIRTUAL_BITS (type), fip->nbaseclasses);
      TYPE_N_BASECLASSES (type) = fip->nbaseclasses;
    }

  /* Copy the saved-up fields into the field vector.  Start from the head
     of the list, adding to the tail of the field array, so that they end
     up in the same order in the array in which they were added to the list.  */
  while (nfields-- > 0)
    {
      TYPE_FIELD (type, nfields) = fip->fields->field;
      switch (fip->fields->accessibility)
	{
	case DW_ACCESS_private:
	  SET_TYPE_FIELD_PRIVATE (type, nfields);
	  break;

	case DW_ACCESS_protected:
	  SET_TYPE_FIELD_PROTECTED (type, nfields);
	  break;

	case DW_ACCESS_public:
	  break;

	default:
	  /* Unknown accessibility.  Complain and treat it as public.  */
	  {
	    complaint (&symfile_complaints, _("unsupported accessibility %d"),
		       fip->fields->accessibility);
	  }
	  break;
	}
      if (nfields < fip->nbaseclasses)
	{
	  switch (fip->fields->virtuality)
	    {
	    case DW_VIRTUALITY_virtual:
	    case DW_VIRTUALITY_pure_virtual:
	      SET_TYPE_FIELD_VIRTUAL (type, nfields);
	      break;
	    }
	}
      fip->fields = fip->fields->next;
    }
}

/* Add a member function to the proper fieldlist.  */

static void
dwarf2_add_member_fn (struct field_info *fip, struct die_info *die,
		      struct type *type, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct attribute *attr;
  struct fnfieldlist *flp;
  int i;
  struct fn_field *fnp;
  char *fieldname;
  char *physname;
  struct nextfnfield *new_fnfield;

  /* Get name of member function.  */
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    fieldname = DW_STRING (attr);
  else
    return;

  /* Get the mangled name.  */
  physname = dwarf2_linkage_name (die, cu);

  /* Look up member function name in fieldlist.  */
  for (i = 0; i < fip->nfnfields; i++)
    {
      if (strcmp (fip->fnfieldlists[i].name, fieldname) == 0)
	break;
    }

  /* Create new list element if necessary.  */
  if (i < fip->nfnfields)
    flp = &fip->fnfieldlists[i];
  else
    {
      if ((fip->nfnfields % DW_FIELD_ALLOC_CHUNK) == 0)
	{
	  fip->fnfieldlists = (struct fnfieldlist *)
	    xrealloc (fip->fnfieldlists,
		      (fip->nfnfields + DW_FIELD_ALLOC_CHUNK)
		      * sizeof (struct fnfieldlist));
	  if (fip->nfnfields == 0)
	    make_cleanup (free_current_contents, &fip->fnfieldlists);
	}
      flp = &fip->fnfieldlists[fip->nfnfields];
      flp->name = fieldname;
      flp->length = 0;
      flp->head = NULL;
      fip->nfnfields++;
    }

  /* Create a new member function field and chain it to the field list
     entry. */
  new_fnfield = (struct nextfnfield *) xmalloc (sizeof (struct nextfnfield));
  make_cleanup (xfree, new_fnfield);
  memset (new_fnfield, 0, sizeof (struct nextfnfield));
  new_fnfield->next = flp->head;
  flp->head = new_fnfield;
  flp->length++;

  /* Fill in the member function field info.  */
  fnp = &new_fnfield->fnfield;
  /* The name is already allocated along with this objfile, so we don't
     need to duplicate it for the type.  */
  fnp->physname = physname ? physname : "";
  fnp->type = alloc_type (objfile);
  if (die->type && TYPE_CODE (die->type) == TYPE_CODE_FUNC)
    {
      int nparams = TYPE_NFIELDS (die->type);

      /* TYPE is the domain of this method, and DIE->TYPE is the type
	   of the method itself (TYPE_CODE_METHOD).  */
      smash_to_method_type (fnp->type, type,
			    TYPE_TARGET_TYPE (die->type),
			    TYPE_FIELDS (die->type),
			    TYPE_NFIELDS (die->type),
			    TYPE_VARARGS (die->type));

      /* Handle static member functions.
         Dwarf2 has no clean way to discern C++ static and non-static
         member functions. G++ helps GDB by marking the first
         parameter for non-static member functions (which is the
         this pointer) as artificial. We obtain this information
         from read_subroutine_type via TYPE_FIELD_ARTIFICIAL.  */
      if (nparams == 0 || TYPE_FIELD_ARTIFICIAL (die->type, 0) == 0)
	fnp->voffset = VOFFSET_STATIC;
    }
  else
    complaint (&symfile_complaints, _("member function type missing for '%s'"),
	       physname);

  /* Get fcontext from DW_AT_containing_type if present.  */
  if (dwarf2_attr (die, DW_AT_containing_type, cu) != NULL)
    fnp->fcontext = die_containing_type (die, cu);

  /* dwarf2 doesn't have stubbed physical names, so the setting of is_const
     and is_volatile is irrelevant, as it is needed by gdb_mangle_name only.  */

  /* Get accessibility.  */
  attr = dwarf2_attr (die, DW_AT_accessibility, cu);
  if (attr)
    {
      switch (DW_UNSND (attr))
	{
	case DW_ACCESS_private:
	  fnp->is_private = 1;
	  break;
	case DW_ACCESS_protected:
	  fnp->is_protected = 1;
	  break;
	}
    }

  /* Check for artificial methods.  */
  attr = dwarf2_attr (die, DW_AT_artificial, cu);
  if (attr && DW_UNSND (attr) != 0)
    fnp->is_artificial = 1;

  /* Get index in virtual function table if it is a virtual member function.  */
  attr = dwarf2_attr (die, DW_AT_vtable_elem_location, cu);
  if (attr)
    {
      /* Support the .debug_loc offsets */
      if (attr_form_is_block (attr))
        {
          fnp->voffset = decode_locdesc (DW_BLOCK (attr), cu) + 2;
        }
      else if (attr->form == DW_FORM_data4 || attr->form == DW_FORM_data8)
        {
	  dwarf2_complex_location_expr_complaint ();
        }
      else
        {
	  dwarf2_invalid_attrib_class_complaint ("DW_AT_vtable_elem_location",
						 fieldname);
        }
   }
}

/* Create the vector of member function fields, and attach it to the type.  */

static void
dwarf2_attach_fn_fields_to_type (struct field_info *fip, struct type *type,
				 struct dwarf2_cu *cu)
{
  struct fnfieldlist *flp;
  int total_length = 0;
  int i;

  ALLOCATE_CPLUS_STRUCT_TYPE (type);
  TYPE_FN_FIELDLISTS (type) = (struct fn_fieldlist *)
    TYPE_ALLOC (type, sizeof (struct fn_fieldlist) * fip->nfnfields);

  for (i = 0, flp = fip->fnfieldlists; i < fip->nfnfields; i++, flp++)
    {
      struct nextfnfield *nfp = flp->head;
      struct fn_fieldlist *fn_flp = &TYPE_FN_FIELDLIST (type, i);
      int k;

      TYPE_FN_FIELDLIST_NAME (type, i) = flp->name;
      TYPE_FN_FIELDLIST_LENGTH (type, i) = flp->length;
      fn_flp->fn_fields = (struct fn_field *)
	TYPE_ALLOC (type, sizeof (struct fn_field) * flp->length);
      for (k = flp->length; (k--, nfp); nfp = nfp->next)
	fn_flp->fn_fields[k] = nfp->fnfield;

      total_length += flp->length;
    }

  TYPE_NFN_FIELDS (type) = fip->nfnfields;
  TYPE_NFN_FIELDS_TOTAL (type) = total_length;
}


/* Returns non-zero if NAME is the name of a vtable member in CU's
   language, zero otherwise.  */
static int
is_vtable_name (const char *name, struct dwarf2_cu *cu)
{
  static const char vptr[] = "_vptr";
  static const char vtable[] = "vtable";

  /* Look for the C++ and Java forms of the vtable.  */
  if ((cu->language == language_java
       && strncmp (name, vtable, sizeof (vtable) - 1) == 0)
       || (strncmp (name, vptr, sizeof (vptr) - 1) == 0
       && is_cplus_marker (name[sizeof (vptr) - 1])))
    return 1;

  return 0;
}


/* Called when we find the DIE that starts a structure or union scope
   (definition) to process all dies that define the members of the
   structure or union.

   NOTE: we need to call struct_type regardless of whether or not the
   DIE has an at_name attribute, since it might be an anonymous
   structure or union.  This gets the type entered into our set of
   user defined types.

   However, if the structure is incomplete (an opaque struct/union)
   then suppress creating a symbol table entry for it since gdb only
   wants to find the one with the complete definition.  Note that if
   it is complete, we just call new_symbol, which does it's own
   checking about whether the struct/union is anonymous or not (and
   suppresses creating a symbol table entry itself).  */

static void
read_structure_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct attribute *attr;
  const char *previous_prefix = processing_current_prefix;
  struct cleanup *back_to = NULL;

  if (die->type)
    return;

  type = alloc_type (objfile);

  INIT_CPLUS_SPECIFIC (type);
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    {
      if (cu->language == language_cplus
	  || cu->language == language_objcplus
	  || cu->language == language_java)
	{
	  char *new_prefix = determine_class_name (die, cu);
	  TYPE_TAG_NAME (type) = obsavestring (new_prefix,
					       strlen (new_prefix),
					       &objfile->objfile_obstack);
	  back_to = make_cleanup (xfree, new_prefix);
	  processing_current_prefix = new_prefix;
	}
      else
	{
	  /* The name is already allocated along with this objfile, so
	     we don't need to duplicate it for the type.  */
	  TYPE_TAG_NAME (type) = DW_STRING (attr);
	}
    }

  if (die->tag == DW_TAG_structure_type)
    {
      TYPE_CODE (type) = TYPE_CODE_STRUCT;
    }
  else if (die->tag == DW_TAG_union_type)
    {
      TYPE_CODE (type) = TYPE_CODE_UNION;
    }
  else
    {
      /* FIXME: TYPE_CODE_CLASS is currently defined to TYPE_CODE_STRUCT
         in gdbtypes.h.  */
      TYPE_CODE (type) = TYPE_CODE_CLASS;
    }

  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      TYPE_LENGTH_ASSIGN (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH_ASSIGN (type) = 0;
    }

  if (die_is_declaration (die, cu))
    TYPE_FLAGS (type) |= TYPE_FLAG_STUB;

  /* We need to add the type field to the die immediately so we don't
     infinitely recurse when dealing with pointers to the structure
     type within the structure itself. */
  set_die_type (die, type, cu);

  if (die->child != NULL && ! die_is_declaration (die, cu))
    {
      struct field_info fi;
      struct die_info *child_die;
      struct cleanup *back_to = make_cleanup (null_cleanup, NULL);

      memset (&fi, 0, sizeof (struct field_info));

      child_die = die->child;

      while (child_die && child_die->tag)
	{
	  if (child_die->tag == DW_TAG_member
	      || child_die->tag == DW_TAG_variable)
	    {
	      /* NOTE: carlton/2002-11-05: A C++ static data member
		 should be a DW_TAG_member that is a declaration, but
		 all versions of G++ as of this writing (so through at
		 least 3.2.1) incorrectly generate DW_TAG_variable
		 tags for them instead.  */
	      dwarf2_add_field (&fi, child_die, cu);
	    }
	  else if (child_die->tag == DW_TAG_subprogram)
	    {
	      /* C++ member function. */
	      read_type_die (child_die, cu);
	      dwarf2_add_member_fn (&fi, child_die, type, cu);
	    }
	  else if (child_die->tag == DW_TAG_inheritance)
	    {
	      /* C++ base class field.  */
	      dwarf2_add_field (&fi, child_die, cu);
	    }
	  child_die = sibling_die (child_die);
	}

      /* Attach fields and member functions to the type.  */
      if (fi.nfields)
	dwarf2_attach_fields_to_type (&fi, type, cu);
      if (fi.nfnfields)
	{
	  dwarf2_attach_fn_fields_to_type (&fi, type, cu);

	  /* Get the type which refers to the base class (possibly this
	     class itself) which contains the vtable pointer for the current
	     class from the DW_AT_containing_type attribute.  */

	  if (dwarf2_attr (die, DW_AT_containing_type, cu) != NULL)
	    {
	      struct type *t = die_containing_type (die, cu);

	      TYPE_VPTR_BASETYPE (type) = t;
	      if (type == t)
		{
		  int i;

		  /* Our own class provides vtbl ptr.  */
		  for (i = TYPE_NFIELDS (t) - 1;
		       i >= TYPE_N_BASECLASSES (t);
		       --i)
		    {
		      char *fieldname = TYPE_FIELD_NAME (t, i);

                      if (is_vtable_name (fieldname, cu))
			{
			  TYPE_VPTR_FIELDNO (type) = i;
			  break;
			}
		    }

		  /* Complain if virtual function table field not found.  */
		  if (i < TYPE_N_BASECLASSES (t))
		    complaint (&symfile_complaints,
			       _("virtual function table pointer not found when defining class '%s'"),
			       TYPE_TAG_NAME (type) ? TYPE_TAG_NAME (type) :
			       "");
		}
	      else
		{
		  TYPE_VPTR_FIELDNO (type) = TYPE_VPTR_FIELDNO (t);
		}
	    }
	}

      do_cleanups (back_to);
    }

  /* APPLE LOCAL: Figure out which runtime this type belongs to...  
     Only do this for ObjC & ObjC++.  Otherwise we default to the
     C++ runtime.  We could add defines for ADA etc, but we don't
     use that, and it's really only important when you can mix runtimes
     in a single CU.  */
  if (cu->language == language_objc
      || cu->language == language_objcplus
      || cu->language == language_unknown)
    {
      int found = 0;
      attr = dwarf2_attr (die, DW_AT_APPLE_runtime_class, cu);
      if (attr)
	{
	  unsigned int language = DW_UNSND (attr);
	  switch (language)
	    {
	    case DW_LANG_ObjC:
	      /* For some reason gcc marks ObjC classes in .mm files
		 as ObjC_plus_plus which isn't right, but they mean
		 ObjC so we'll accept it...  */
	    case DW_LANG_ObjC_plus_plus:
	      /* Make sure we have cplus_stuff now or the
		 TYPE_RUNTIME will go nowhere...  */
	      ALLOCATE_CPLUS_STRUCT_TYPE (type);
	      TYPE_RUNTIME (type) = OBJC_RUNTIME;
	      found = 1;
	      break;
	    case DW_LANG_C_plus_plus:
	      /* Make sure we have cplus_stuff now or the
		 TYPE_RUNTIME will go nowhere...  */
	      ALLOCATE_CPLUS_STRUCT_TYPE (type);
	      TYPE_RUNTIME (type) = CPLUS_RUNTIME;
	      found = 1;
	      break;
	    default:
	      /* Anything else, just leave it the way it was.  */
	      found = 1;
	      break;
	    }
	}
	  
      if (!found)
	{
	  if (TYPE_N_BASECLASSES (type) == 0)
	    {
	      /* If there are no baseclasses, see if this is the 
		 base of the ObjC Hierarchy.  */
	      if (TYPE_TAG_NAME (type) 
		  && (strcmp(TYPE_TAG_NAME (type), "NSObject") == 0))
		{
		  /* Have to allocate the cplus_stuff up here, since if there is no
		     inheritance tag then we won't have set it up yet.  In the other
		     branches of this test we're implicitly using the fact that if
		     the cplus_stuff is not allocated, then we shouldn't set the
		     runtime (TYPE_RUNTIME doesn't set anything if the cplus_stuff 
		     is NULL.)  */
		  ALLOCATE_CPLUS_STRUCT_TYPE (type);
		  TYPE_RUNTIME (type) = OBJC_RUNTIME;
		}
	      else
		TYPE_RUNTIME (type) = CPLUS_RUNTIME;
	    }
	  else if (TYPE_N_BASECLASSES (type) > 1)
	    {
	      /* ObjC is single inheritance only, so is there's more than one
		 baseclass it can't be ObjC.  */
	      TYPE_RUNTIME (type) = CPLUS_RUNTIME;
	    }
	  else
	    {
	      /* The type inherits its runtime from its parent.  */
	      TYPE_RUNTIME (type)
		= TYPE_RUNTIME(TYPE_FIELD_TYPE (type, 0));
	    }
	}
    }
  else
    TYPE_RUNTIME (type) = CPLUS_RUNTIME;

  /* APPLE LOCAL: The ivar offsets of Obj classes can't
     be determined from the information in the declaring 
     header file.  So we need to fix them up from the
     runtime.  I don't want to have to do this for every
     class right now, however.  So I just mark all the 
     lengths invalid, and get around to fixing them up
     when they are read through TYPE_FIELD_BITPOS or
     TYPE_LENGTH.  */
  if (TYPE_RUNTIME (type) == OBJC_RUNTIME)
    objc_invalidate_objc_class (type);

  /* APPLE LOCAL: See if this is a Closure struct.  */
  attr = dwarf2_attr (die, DW_AT_APPLE_closure, cu);
  if (attr)
    TYPE_FLAGS (type) |= TYPE_FLAG_APPLE_CLOSURE;
  
  processing_current_prefix = previous_prefix;
  if (back_to != NULL)
    do_cleanups (back_to);
}

static void
process_structure_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct objfile *objfile = cu->objfile; */
  const char *previous_prefix = processing_current_prefix;
  struct die_info *child_die = die->child;

  if (TYPE_TAG_NAME (die->type) != NULL)
    processing_current_prefix = TYPE_TAG_NAME (die->type);

  /* NOTE: carlton/2004-03-16: GCC 3.4 (or at least one of its
     snapshots) has been known to create a die giving a declaration
     for a class that has, as a child, a die giving a definition for a
     nested class.  So we have to process our children even if the
     current die is a declaration.  Normally, of course, a declaration
     won't have any children at all.  */

  while (child_die != NULL && child_die->tag)
    {
      if (child_die->tag == DW_TAG_member
	  || (child_die->tag == DW_TAG_variable 
             /* APPLE LOCAL */
	      && dwarf2_attr (child_die, DW_AT_const_value, cu) == NULL)
	  || child_die->tag == DW_TAG_inheritance)
	{
	  /* Do nothing.  */
	}
      else
	process_die (child_die, cu);

      child_die = sibling_die (child_die);
    }

  if (die->child != NULL && ! die_is_declaration (die, cu))
    new_symbol (die, die->type, cu);

  processing_current_prefix = previous_prefix;
}

/* Given a DW_AT_enumeration_type die, set its type.  We do not
   complete the type's fields yet, or create any symbols.  */

static void
read_enumeration_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct attribute *attr;

  if (die->type)
    return;

  type = alloc_type (objfile);

  TYPE_CODE (type) = TYPE_CODE_ENUM;
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    {
      char *name = DW_STRING (attr);

      if (processing_has_namespace_info)
	{
	  TYPE_TAG_NAME (type) = typename_concat (&objfile->objfile_obstack,
						  processing_current_prefix,
						  name, cu);
	}
      else
	{
	  /* The name is already allocated along with this objfile, so
	     we don't need to duplicate it for the type.  */
	  TYPE_TAG_NAME (type) = name;
	}
    }

  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      TYPE_LENGTH_ASSIGN (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH_ASSIGN (type) = 0;
    }

  set_die_type (die, type, cu);
}

/* Determine the name of the type represented by DIE, which should be
   a named C++ or Java compound type.  Return the name in question; the caller
   is responsible for xfree()'ing it.  */

static char *
determine_class_name (struct die_info *die, struct dwarf2_cu *cu)
{
  struct cleanup *back_to = NULL;
  struct die_info *spec_die = die_specification (die, cu);
  char *new_prefix = NULL;

  /* If this is the definition of a class that is declared by another
     die, then processing_current_prefix may not be accurate; see
     read_func_scope for a similar example.  */
  if (spec_die != NULL)
    {
      char *specification_prefix = determine_prefix (spec_die, cu);
      processing_current_prefix = specification_prefix;
      back_to = make_cleanup (xfree, specification_prefix);
    }

  /* If we don't have namespace debug info, guess the name by trying
     to demangle the names of members, just like we did in
     guess_structure_name.  */
  if (!processing_has_namespace_info)
    {
      struct die_info *child;

      for (child = die->child;
	   child != NULL && child->tag != 0;
	   child = sibling_die (child))
	{
	  if (child->tag == DW_TAG_subprogram)
	    {
	      new_prefix 
		= language_class_name_from_physname (cu->language_defn,
						     dwarf2_linkage_name
						     (child, cu));

	      if (new_prefix != NULL)
		break;
	    }
	}
    }

  if (new_prefix == NULL)
    {
      const char *name = dwarf2_name (die, cu);
      new_prefix = typename_concat (NULL, processing_current_prefix,
				    name ? name : "<<anonymous>>", 
				    cu);
    }

  if (back_to != NULL)
    do_cleanups (back_to);

  return new_prefix;
}

/* Given a pointer to a die which begins an enumeration, process all
   the dies that define the members of the enumeration, and create the
   symbol for the enumeration type.

   NOTE: We reverse the order of the element list.  */

static void
process_enumeration_scope (struct die_info *die, struct dwarf2_cu *cu)
{
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct objfile *objfile = cu->objfile; */
  struct die_info *child_die;
  struct field *fields;
  struct attribute *attr;
  struct symbol *sym;
  int num_fields;
  int unsigned_enum = 1;

  num_fields = 0;
  fields = NULL;
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag != DW_TAG_enumerator)
	    {
	      process_die (child_die, cu);
	    }
	  else
	    {
	      attr = dwarf2_attr (child_die, DW_AT_name, cu);
	      if (attr)
		{
		  sym = new_symbol (child_die, die->type, cu);
		  if (SYMBOL_VALUE (sym) < 0)
		    unsigned_enum = 0;

		  if ((num_fields % DW_FIELD_ALLOC_CHUNK) == 0)
		    {
		      fields = (struct field *)
			xrealloc (fields,
				  (num_fields + DW_FIELD_ALLOC_CHUNK)
				  * sizeof (struct field));
		    }

		  FIELD_NAME (fields[num_fields]) = DEPRECATED_SYMBOL_NAME (sym);
		  FIELD_TYPE (fields[num_fields]) = NULL;
		  FIELD_BITPOS (fields[num_fields]) = SYMBOL_VALUE (sym);
		  FIELD_BITSIZE (fields[num_fields]) = 0;
		  FIELD_STATIC_KIND (fields[num_fields]) = 0;

		  num_fields++;
		}
	    }

	  child_die = sibling_die (child_die);
	}

      if (num_fields)
	{
	  TYPE_NFIELDS (die->type) = num_fields;
	  TYPE_FIELDS (die->type) = (struct field *)
	    TYPE_ALLOC (die->type, sizeof (struct field) * num_fields);
	  memcpy (TYPE_FIELDS (die->type), fields,
		  sizeof (struct field) * num_fields);
	  xfree (fields);
	}
      if (unsigned_enum)
	TYPE_FLAGS (die->type) |= TYPE_FLAG_UNSIGNED;
    }

  new_symbol (die, die->type, cu);
}

/* Extract all information from a DW_TAG_array_type DIE and put it in
   the DIE's type field.  For now, this only handles one dimensional
   arrays.  */

static void
read_array_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct die_info *child_die;
  struct type *type = NULL;
  struct type *element_type, *range_type, *index_type;
  struct type **range_types = NULL;
  struct attribute *attr;
  int ndim = 0;
  struct cleanup *back_to;

  /* Return if we've already decoded this type. */
  if (die->type)
    {
      return;
    }

  element_type = die_type (die, cu);

  /* Irix 6.2 native cc creates array types without children for
     arrays with unspecified length.  */
  if (die->child == NULL)
    {
      index_type = dwarf2_fundamental_type (objfile, FT_INTEGER, cu);
      range_type = create_range_type (NULL, index_type, 0, -1);
      set_die_type (die, create_array_type (NULL, element_type, range_type),
		    cu);
      return;
    }

  back_to = make_cleanup (null_cleanup, NULL);
  child_die = die->child;
  while (child_die && child_die->tag)
    {
      if (child_die->tag == DW_TAG_subrange_type)
	{
          read_subrange_type (child_die, cu);

          if (child_die->type != NULL)
            {
	      /* The range type was succesfully read. Save it for
                 the array type creation.  */
              if ((ndim % DW_FIELD_ALLOC_CHUNK) == 0)
                {
                  range_types = (struct type **)
                    xrealloc (range_types, (ndim + DW_FIELD_ALLOC_CHUNK)
                              * sizeof (struct type *));
                  if (ndim == 0)
                    make_cleanup (free_current_contents, &range_types);
	        }
	      range_types[ndim++] = child_die->type;
            }
	}
      child_die = sibling_die (child_die);
    }

  /* Dwarf2 dimensions are output from left to right, create the
     necessary array types in backwards order.  */

  type = element_type;

  if (read_array_order (die, cu) == DW_ORD_col_major)
    {
      int i = 0;
      while (i < ndim)
	type = create_array_type (NULL, type, range_types[i++]);
    }
  else
    {
      while (ndim-- > 0)
	type = create_array_type (NULL, type, range_types[ndim]);
    }

  /* Understand Dwarf2 support for vector types (like they occur on
     the PowerPC w/ AltiVec).  Gcc just adds another attribute to the
     array type.  This is not part of the Dwarf2/3 standard yet, but a
     custom vendor extension.  The main difference between a regular
     array and the vector variant is that vectors are passed by value
     to functions.  */
  attr = dwarf2_attr (die, DW_AT_GNU_vector, cu);
  if (attr)
    TYPE_FLAGS (type) |= TYPE_FLAG_VECTOR;

  do_cleanups (back_to);

  /* Install the type in the die. */
  set_die_type (die, type, cu);
}

static enum dwarf_array_dim_ordering
read_array_order (struct die_info *die, struct dwarf2_cu *cu) 
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_ordering, cu);

  if (attr) return DW_SND (attr);

  /*
    GNU F77 is a special case, as at 08/2004 array type info is the
    opposite order to the dwarf2 specification, but data is still 
    laid out as per normal fortran.

    FIXME: dsl/2004-8-20: If G77 is ever fixed, this will also need 
    version checking.
  */

  if (cu->language == language_fortran &&
      cu->producer && strstr (cu->producer, "GNU F77"))
    {
      return DW_ORD_row_major;
    }

  switch (cu->language_defn->la_array_ordering) 
    {
    case array_column_major:
      return DW_ORD_col_major;
    case array_row_major:
    default:
      return DW_ORD_row_major;
    };
}

/* Extract all information from a DW_TAG_set_type DIE and put it in
   the DIE's type field. */

static void
read_set_type (struct die_info *die, struct dwarf2_cu *cu)
{
  if (die->type == NULL)
    die->type = create_set_type ((struct type *) NULL, die_type (die, cu));
}

/* First cut: install each common block member as a global variable.  */

static void
read_common_block (struct die_info *die, struct dwarf2_cu *cu)
{
  struct die_info *child_die;
  struct attribute *attr;
  struct symbol *sym;
  CORE_ADDR base = (CORE_ADDR) 0;

  /* APPLE LOCAL: Keep track of the current common block name so we can
     offset symbols within that block that we find while processing this
     DIE or its children.  */
  struct attribute *nattr;
  nattr = dwarf2_attr (die, DW_AT_MIPS_linkage_name, cu);
  if (!nattr)
    nattr = dwarf2_attr (die, DW_AT_name, cu);
  if (nattr)
    decode_locdesc_common = DW_STRING (nattr);

  attr = dwarf2_attr (die, DW_AT_location, cu);
  if (attr)
    {
      /* Support the .debug_loc offsets */
      if (attr_form_is_block (attr))
        {
          base = decode_locdesc (DW_BLOCK (attr), cu);
        }
      else if (attr->form == DW_FORM_data4 || attr->form == DW_FORM_data8)
        {
	  dwarf2_complex_location_expr_complaint ();
        }
      else
        {
	  dwarf2_invalid_attrib_class_complaint ("DW_AT_location",
						 "common block member");
        }
    }
  if (die->child != NULL)
    {
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  sym = new_symbol (child_die, NULL, cu);
	  attr = dwarf2_attr (child_die, DW_AT_data_member_location, cu);
	  if (attr)
	    {
	      SYMBOL_VALUE_ADDRESS (sym) =
		base + decode_locdesc (DW_BLOCK (attr), cu);
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  child_die = sibling_die (child_die);
	}
    }
  /* APPLE LOCAL:  Finished processing addresses that may be relative to
     this common block.  */
  decode_locdesc_common = NULL;
}

/* Read a C++ namespace.  */

static void
read_namespace (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  const char *previous_prefix = processing_current_prefix;
  const char *name;
  int is_anonymous;
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct die_info *current_die; */
  struct cleanup *back_to = make_cleanup (null_cleanup, 0);

  name = namespace_name (die, &is_anonymous, cu);

  /* Now build the name of the current namespace.  */

  if (previous_prefix[0] == '\0')
    {
      processing_current_prefix = name;
    }
  else
    {
      char *temp_name = typename_concat (NULL, previous_prefix, name, cu);
      make_cleanup (xfree, temp_name);
      processing_current_prefix = temp_name;
    }

  /* Add a symbol associated to this if we haven't seen the namespace
     before.  Also, add a using directive if it's an anonymous
     namespace.  */

  if (dwarf2_extension (die, cu) == NULL)
    {
      struct type *type;

      /* FIXME: carlton/2003-06-27: Once GDB is more const-correct,
	 this cast will hopefully become unnecessary.  */
      type = init_type (TYPE_CODE_NAMESPACE, 0, 0,
			(char *) processing_current_prefix,
			objfile);
      TYPE_TAG_NAME (type) = TYPE_NAME (type);

      new_symbol (die, type, cu);
      set_die_type (die, type, cu);

      if (is_anonymous)
	cp_add_using_directive (processing_current_prefix,
				strlen (previous_prefix),
				strlen (processing_current_prefix));
    }

  if (die->child != NULL)
    {
      struct die_info *child_die = die->child;
      
      while (child_die && child_die->tag)
	{
	  process_die (child_die, cu);
	  child_die = sibling_die (child_die);
	}
    }

  processing_current_prefix = previous_prefix;
  do_cleanups (back_to);
}

/* Return the name of the namespace represented by DIE.  Set
   *IS_ANONYMOUS to tell whether or not the namespace is an anonymous
   namespace.  */

static const char *
namespace_name (struct die_info *die, int *is_anonymous, struct dwarf2_cu *cu)
{
  struct die_info *current_die;
  const char *name = NULL;

  /* Loop through the extensions until we find a name.  */

  for (current_die = die;
       current_die != NULL;
       current_die = dwarf2_extension (die, cu))
    {
      name = dwarf2_name (current_die, cu);
      if (name != NULL)
	break;
    }

  /* Is it an anonymous namespace?  */

  *is_anonymous = (name == NULL);
  if (*is_anonymous)
    name = "(anonymous namespace)";

  return name;
}

/* Extract all information from a DW_TAG_pointer_type DIE and add to
   the user defined type vector.  */

static void
read_tag_pointer_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  struct type *type;
  struct attribute *attr_byte_size;
  struct attribute *attr_address_class;
  int byte_size, addr_class;

  if (die->type)
    {
      return;
    }

  /* APPLE LOCAL: gcc 4.0 - 4.2 (at least) produce an odd die for the
     "id" and "Class" typedefs for objc.  e.g. objc.h actually has a
     typedef of "id" -> "struct objc_class *" but for some unknown
     reason, gcc produces a DW_TAG_pointer_type die with a NAME field
     of "id".  Convert that back to a typedef here, since this named
     pointer dingus is useless.  Since a named pointer is not
     something you can create in C or C++, I'm going to convert all
     such dies to typedef to the pointer, rather than just filtering
     for "id" and "Class".  That will protect us in case gcc decides
     to get quirky on us somewhere else.  */
  {
    struct attribute *attr;
    struct type *id_type;
    char *name;
    if (cu->language == language_objc 
	|| cu->language == language_objcplus)
      {
	attr = dwarf2_attr (die, DW_AT_name, cu);
	if (attr && DW_STRING (attr))
	  {
	    enum dwarf_tag old_tag;
	    name = DW_STRING (attr);
	    id_type = die_type (die, cu);
	    if (id_type == NULL)
	      {
		complaint (&symfile_complaints,
			   "Could not get target type for \"%s\" die at %d.",
			   name, die->offset);
		return;
	      }
	    id_type = make_pointer_type (id_type, NULL);
	    set_die_type (die, init_type (TYPE_CODE_TYPEDEF, 0, 
					  TYPE_FLAG_TARGET_STUB, 
					  name, cu->objfile), 
			  cu);
	    TYPE_TARGET_TYPE (die->type) = id_type;
	    /* We need to make the symbol for "id" here because
	       pointer types don't usually get symbols, so all
	       our callers will skip doing this.  
	       Also, since we're converting a pointer type
	       to a typedef, we need to get the tag right for
	       new_symbol or it won't do the right things.  */
	    old_tag = die->tag;
	    die->tag = DW_TAG_typedef;
	    new_symbol (die, die->type, cu);
	    die->tag = old_tag;
	    return;
	  }
      }
  }
  /* END APPLE LOCAL */

  type = lookup_pointer_type (die_type (die, cu));

  attr_byte_size = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr_byte_size)
    byte_size = DW_UNSND (attr_byte_size);
  else
    byte_size = cu_header->addr_size;

  attr_address_class = dwarf2_attr (die, DW_AT_address_class, cu);
  if (attr_address_class)
    addr_class = DW_UNSND (attr_address_class);
  else
    addr_class = DW_ADDR_none;

  /* If the pointer size or address class is different than the
     default, create a type variant marked as such and set the
     length accordingly.  */
  if (TYPE_LENGTH (type) != byte_size || addr_class != DW_ADDR_none)
    {
      if (ADDRESS_CLASS_TYPE_FLAGS_P ())
	{
	  int type_flags;

	  type_flags = ADDRESS_CLASS_TYPE_FLAGS (byte_size, addr_class);
	  gdb_assert ((type_flags & ~TYPE_FLAG_ADDRESS_CLASS_ALL) == 0);
	  type = make_type_with_address_space (type, type_flags);
	}
      else if (TYPE_LENGTH (type) != byte_size)
	{
	  complaint (&symfile_complaints, _("invalid pointer size %d"), byte_size);
	}
      else {
	/* Should we also complain about unhandled address classes?  */
      }
    }

  TYPE_LENGTH_ASSIGN (type) = byte_size;
  set_die_type (die, type, cu);
}

/* Extract all information from a DW_TAG_ptr_to_member_type DIE and add to
   the user defined type vector.  */

static void
read_tag_ptr_to_member_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct type *to_type;
  struct type *domain;

  if (die->type)
    {
      return;
    }

  type = alloc_type (objfile);
  to_type = die_type (die, cu);
  domain = die_containing_type (die, cu);
  smash_to_member_type (type, domain, to_type);

  set_die_type (die, type, cu);
}

/* Extract all information from a DW_TAG_reference_type DIE and add to
   the user defined type vector.  */

static void
read_tag_reference_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  struct type *type;
  struct attribute *attr;

  if (die->type)
    {
      return;
    }

  type = lookup_reference_type (die_type (die, cu));
  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      TYPE_LENGTH_ASSIGN (type) = DW_UNSND (attr);
    }
  else
    {
      TYPE_LENGTH_ASSIGN (type) = cu_header->addr_size;
    }
  set_die_type (die, type, cu);
}

static void
read_tag_const_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;

  if (die->type)
    {
      return;
    }

  base_type = die_type (die, cu);
  set_die_type (die, make_cvr_type (1, TYPE_VOLATILE (base_type), 
                                    TYPE_RESTRICT (base_type), base_type, 0),
		cu);
}

static void
read_tag_volatile_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;

  if (die->type)
    {
      return;
    }

  base_type = die_type (die, cu);
  set_die_type (die, make_cvr_type (TYPE_CONST (base_type), 1, 
                                    TYPE_RESTRICT (base_type), base_type, 0),
		cu);
}

static void
read_tag_restrict_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;

  if (die->type)
    {
      return;
    }

  base_type = die_type (die, cu);
  set_die_type (die, make_cvr_type (TYPE_CONST (base_type), 
                                   TYPE_VOLATILE (base_type), 1, base_type, 0),
		cu);
}


/* Extract all information from a DW_TAG_string_type DIE and add to
   the user defined type vector.  It isn't really a user defined type,
   but it behaves like one, with other DIE's using an AT_user_def_type
   attribute to reference it.  */

static void
read_tag_string_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type, *range_type, *index_type, *char_type;
  struct attribute *attr;
  unsigned int length;

  if (die->type)
    {
      return;
    }

  attr = dwarf2_attr (die, DW_AT_string_length, cu);
  if (attr)
    {
      length = DW_UNSND (attr);
    }
  else
    {
      /* check for the DW_AT_byte_size attribute */
      attr = dwarf2_attr (die, DW_AT_byte_size, cu);
      if (attr)
        {
          length = DW_UNSND (attr);
        }
      else
        {
          length = 1;
        }
    }
  index_type = dwarf2_fundamental_type (objfile, FT_INTEGER, cu);
  range_type = create_range_type (NULL, index_type, 1, length);
  if (cu->language == language_fortran)
    {
      /* Need to create a unique string type for bounds
         information */
      type = create_string_type (0, range_type);
    }
  else
    {
      char_type = dwarf2_fundamental_type (objfile, FT_CHAR, cu);
      type = create_string_type (char_type, range_type);
    }
  set_die_type (die, type, cu);
}

/* Handle DIES due to C code like:

   struct foo
   {
   int (*funcp)(int a, long l);
   int b;
   };

   ('funcp' generates a DW_TAG_subroutine_type DIE)
 */

static void
read_subroutine_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type;		/* Type that this function returns */
  struct type *ftype;		/* Function that returns above type */
  struct attribute *attr;

  /* Decode the type that this subroutine returns */
  if (die->type)
    {
      return;
    }
  type = die_type (die, cu);
  ftype = make_function_type (type, (struct type **) 0);

  /* All functions in C++ and Java have prototypes.  */
  attr = dwarf2_attr (die, DW_AT_prototyped, cu);
  if ((attr && (DW_UNSND (attr) != 0))
      || cu->language == language_cplus
      || cu->language == language_objcplus
      || cu->language == language_java)
    TYPE_FLAGS (ftype) |= TYPE_FLAG_PROTOTYPED;

  if (die->child != NULL)
    {
      struct die_info *child_die;
      int nparams = 0;
      int iparams = 0;

      /* Count the number of parameters.
         FIXME: GDB currently ignores vararg functions, but knows about
         vararg member functions.  */
      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag == DW_TAG_formal_parameter)
	    nparams++;
	  else if (child_die->tag == DW_TAG_unspecified_parameters)
	    TYPE_FLAGS (ftype) |= TYPE_FLAG_VARARGS;
	  child_die = sibling_die (child_die);
	}

      /* Allocate storage for parameters and fill them in.  */
      TYPE_NFIELDS (ftype) = nparams;
      TYPE_FIELDS (ftype) = (struct field *)
	TYPE_ALLOC (ftype, nparams * sizeof (struct field));

      child_die = die->child;
      while (child_die && child_die->tag)
	{
	  if (child_die->tag == DW_TAG_formal_parameter)
	    {
	      /* Dwarf2 has no clean way to discern C++ static and non-static
	         member functions. G++ helps GDB by marking the first
	         parameter for non-static member functions (which is the
	         this pointer) as artificial. We pass this information
	         to dwarf2_add_member_fn via TYPE_FIELD_ARTIFICIAL.  */
	      attr = dwarf2_attr (child_die, DW_AT_artificial, cu);
	      if (attr)
		TYPE_FIELD_ARTIFICIAL (ftype, iparams) = DW_UNSND (attr);
	      else
		TYPE_FIELD_ARTIFICIAL (ftype, iparams) = 0;
	      TYPE_FIELD_TYPE (ftype, iparams) = die_type (child_die, cu);
	      iparams++;
	    }
	  child_die = sibling_die (child_die);
	}
    }

  set_die_type (die, ftype, cu);
}

static void
read_typedef (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct attribute *attr;
  char *name = NULL;

  if (!die->type)
    {
      attr = dwarf2_attr (die, DW_AT_name, cu);
      if (attr && DW_STRING (attr))
	{
	  name = DW_STRING (attr);
	}
      set_die_type (die, init_type (TYPE_CODE_TYPEDEF, 0,
				    TYPE_FLAG_TARGET_STUB, name, objfile),
		    cu);
      TYPE_TARGET_TYPE (die->type) = die_type (die, cu);
    }
}

/* Find a representation of a given base type and install
   it in the TYPE field of the die.  */

static void
read_base_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct type *type;
  struct attribute *attr;
  int encoding = 0, size = 0;

  /* If we've already decoded this die, this is a no-op. */
  if (die->type)
    {
      return;
    }

  attr = dwarf2_attr (die, DW_AT_encoding, cu);
  if (attr)
    {
      encoding = DW_UNSND (attr);
    }
  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    {
      size = DW_UNSND (attr);
    }
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    {
      enum type_code code = TYPE_CODE_INT;
      int type_flags = 0;

      switch (encoding)
	{
	case DW_ATE_address:
	  /* Turn DW_ATE_address into a void * pointer.  */
	  code = TYPE_CODE_PTR;
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	case DW_ATE_boolean:
	  code = TYPE_CODE_BOOL;
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	case DW_ATE_complex_float:
	  code = TYPE_CODE_COMPLEX;
	  break;
	case DW_ATE_float:
	  code = TYPE_CODE_FLT;
	  break;
	case DW_ATE_signed:
	case DW_ATE_signed_char:
	  break;
	case DW_ATE_unsigned:
	case DW_ATE_unsigned_char:
	  type_flags |= TYPE_FLAG_UNSIGNED;
	  break;
	default:
	  complaint (&symfile_complaints, _("unsupported DW_AT_encoding: '%s'"),
		     dwarf_type_encoding_name (encoding));
	  break;
	}
      type = init_type (code, size, type_flags, DW_STRING (attr), objfile);
      if (encoding == DW_ATE_address)
	TYPE_TARGET_TYPE (type) = dwarf2_fundamental_type (objfile, FT_VOID,
							   cu);
      else if (encoding == DW_ATE_complex_float)
	{
	  if (size == 32)
	    TYPE_TARGET_TYPE (type)
	      = dwarf2_fundamental_type (objfile, FT_EXT_PREC_FLOAT, cu);
	  else if (size == 16)
	    TYPE_TARGET_TYPE (type)
	      = dwarf2_fundamental_type (objfile, FT_DBL_PREC_FLOAT, cu);
	  else if (size == 8)
	    TYPE_TARGET_TYPE (type)
	      = dwarf2_fundamental_type (objfile, FT_FLOAT, cu);
	}
    }
  else
    {
      type = dwarf_base_type (encoding, size, cu);
    }
  set_die_type (die, type, cu);
}

/* Read the given DW_AT_subrange DIE.  */

static void
read_subrange_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *base_type;
  struct type *range_type;
  struct attribute *attr;
  int low = 0;
  int high = -1;
  
  /* If we have already decoded this die, then nothing more to do.  */
  if (die->type)
    return;

  base_type = die_type (die, cu);
  if (base_type == NULL)
    {
      complaint (&symfile_complaints,
                _("DW_AT_type missing from DW_TAG_subrange_type"));
      return;
    }

  if (TYPE_CODE (base_type) == TYPE_CODE_VOID)
    base_type = alloc_type (NULL);

  if (cu->language == language_fortran)
    { 
      /* FORTRAN implies a lower bound of 1, if not given.  */
      low = 1;
    }

  /* FIXME: For variable sized arrays either of these could be
     a variable rather than a constant value.  We'll allow it,
     but we don't know how to handle it.  */
  attr = dwarf2_attr (die, DW_AT_lower_bound, cu);
  if (attr)
    low = dwarf2_get_attr_constant_value (attr, 0);

  attr = dwarf2_attr (die, DW_AT_upper_bound, cu);
  if (attr)
    {       
      if (attr->form == DW_FORM_block1)
        {
          /* GCC encodes arrays with unspecified or dynamic length
             with a DW_FORM_block1 attribute.
             FIXME: GDB does not yet know how to handle dynamic
             arrays properly, treat them as arrays with unspecified
             length for now.

             FIXME: jimb/2003-09-22: GDB does not really know
             how to handle arrays of unspecified length
             either; we just represent them as zero-length
             arrays.  Choose an appropriate upper bound given
             the lower bound we've computed above.  */
          high = low - 1;
        }
      else
        high = dwarf2_get_attr_constant_value (attr, 1);
    }

  range_type = create_range_type (NULL, base_type, low, high);

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    TYPE_NAME (range_type) = DW_STRING (attr);
  
  attr = dwarf2_attr (die, DW_AT_byte_size, cu);
  if (attr)
    TYPE_LENGTH_ASSIGN (range_type) = DW_UNSND (attr);

  set_die_type (die, range_type, cu);
}
  

/* Read a whole compilation unit into a linked list of dies.  */

static struct die_info *
read_comp_unit (char *info_ptr, bfd *abfd, struct dwarf2_cu *cu)
{
  return read_die_and_children (info_ptr, abfd, cu, &info_ptr, NULL);
}

/* Read a single die and all its descendents.  Set the die's sibling
   field to NULL; set other fields in the die correctly, and set all
   of the descendents' fields correctly.  Set *NEW_INFO_PTR to the
   location of the info_ptr after reading all of those dies.  PARENT
   is the parent of the die in question.  */

static struct die_info *
read_die_and_children (char *info_ptr, bfd *abfd,
		       struct dwarf2_cu *cu,
		       char **new_info_ptr,
		       struct die_info *parent)
{
  struct die_info *die;
  char *cur_ptr;
  int has_children;

  cur_ptr = read_full_die (&die, abfd, info_ptr, cu, &has_children);
  store_in_ref_table (die->offset, die, cu);

  if (has_children)
    {
      die->child = read_die_and_siblings (cur_ptr, abfd, cu,
					  new_info_ptr, die);
    }
  else
    {
      die->child = NULL;
      *new_info_ptr = cur_ptr;
    }

  die->sibling = NULL;
  die->parent = parent;
  return die;
}

/* Read a die, all of its descendents, and all of its siblings; set
   all of the fields of all of the dies correctly.  Arguments are as
   in read_die_and_children.  */

static struct die_info *
read_die_and_siblings (char *info_ptr, bfd *abfd,
		       struct dwarf2_cu *cu,
		       char **new_info_ptr,
		       struct die_info *parent)
{
  struct die_info *first_die, *last_sibling;
  char *cur_ptr;

  cur_ptr = info_ptr;
  first_die = last_sibling = NULL;

  while (1)
    {
      struct die_info *die
	= read_die_and_children (cur_ptr, abfd, cu, &cur_ptr, parent);

      if (!first_die)
	{
	  first_die = die;
	}
      else
	{
	  last_sibling->sibling = die;
	}

      if (die->tag == 0)
	{
	  *new_info_ptr = cur_ptr;
	  return first_die;
	}
      else
	{
	  last_sibling = die;
	}
    }
}

/* Free a linked list of dies.  */

static void
free_die_list (struct die_info *dies)
{
  struct die_info *die, *next;

  die = dies;
  while (die)
    {
      if (die->child != NULL)
	free_die_list (die->child);
      next = die->sibling;
      xfree (die->attrs);
      xfree (die);
      die = next;
    }
}

/* Read the contents of the section at OFFSET and of size SIZE from the
   object file specified by OBJFILE into the objfile_obstack and return it.  */

/* APPLE LOCAL debug map: New argument ABFD */

char *
dwarf2_read_section (struct objfile *objfile, bfd *abfd, asection *sectp)
{
  char *buf, *retbuf;
  bfd_size_type size = bfd_get_section_size (sectp);

  if (size == 0)
    return NULL;

  buf = (char *) obstack_alloc (&objfile->objfile_obstack, size);
  retbuf
    = (char *) symfile_relocate_debug_section (abfd, sectp, (bfd_byte *) buf);
  if (retbuf != NULL)
    return retbuf;

  if (bfd_seek (abfd, sectp->filepos, SEEK_SET) != 0
      || bfd_bread (buf, size, abfd) != size)
    error (_("Dwarf Error: Can't read DWARF data from '%s'"),
	   bfd_get_filename (abfd));

  return buf;
}

/* In DWARF version 2, the description of the debugging information is
   stored in a separate .debug_abbrev section.  Before we read any
   dies from a section we read in all abbreviations and install them
   in a hash table.  This function also sets flags in CU describing
   the data found in the abbrev table.  */

static void
dwarf2_read_abbrevs (bfd *abfd, struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  char *abbrev_ptr;
  struct abbrev_info *cur_abbrev;
  unsigned int abbrev_number, bytes_read, abbrev_name;
  unsigned int abbrev_form, hash_number;
  struct attr_abbrev *cur_attrs;
  unsigned int allocated_attrs;

  /* Initialize dwarf2 abbrevs */
  obstack_init (&cu->abbrev_obstack);
  cu->dwarf2_abbrevs = obstack_alloc (&cu->abbrev_obstack,
				      (ABBREV_HASH_SIZE
				       * sizeof (struct abbrev_info *)));
  memset (cu->dwarf2_abbrevs, 0,
          ABBREV_HASH_SIZE * sizeof (struct abbrev_info *));

  abbrev_ptr = dwarf2_per_objfile->abbrev_buffer + cu_header->abbrev_offset;
  abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
  abbrev_ptr += bytes_read;

  allocated_attrs = ATTR_ALLOC_CHUNK;
  cur_attrs = xmalloc (allocated_attrs * sizeof (struct attr_abbrev));
  
  /* loop until we reach an abbrev number of 0 */
  while (abbrev_number)
    {
      cur_abbrev = dwarf_alloc_abbrev (cu);

      /* read in abbrev header */
      cur_abbrev->number = abbrev_number;
      cur_abbrev->tag = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      cur_abbrev->has_children = read_1_byte (abfd, abbrev_ptr);
      abbrev_ptr += 1;

      if (cur_abbrev->tag == DW_TAG_namespace)
	cu->has_namespace_info = 1;

      /* now read in declarations */
      abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      while (abbrev_name)
	{
	  if (cur_abbrev->num_attrs == allocated_attrs)
	    {
	      allocated_attrs += ATTR_ALLOC_CHUNK;
	      cur_attrs
		= xrealloc (cur_attrs, (allocated_attrs
					* sizeof (struct attr_abbrev)));
	    }

	  /* Record whether this compilation unit might have
	     inter-compilation-unit references.  If we don't know what form
	     this attribute will have, then it might potentially be a
	     DW_FORM_ref_addr, so we conservatively expect inter-CU
	     references.  */

	  if (abbrev_form == DW_FORM_ref_addr
	      || abbrev_form == DW_FORM_indirect)
	    cu->has_form_ref_addr = 1;

	  cur_attrs[cur_abbrev->num_attrs].name = abbrev_name;
	  cur_attrs[cur_abbrev->num_attrs++].form = abbrev_form;
	  abbrev_name = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	  abbrev_form = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
	  abbrev_ptr += bytes_read;
	}

      cur_abbrev->attrs = obstack_alloc (&cu->abbrev_obstack,
					 (cur_abbrev->num_attrs
					  * sizeof (struct attr_abbrev)));
      memcpy (cur_abbrev->attrs, cur_attrs,
	      cur_abbrev->num_attrs * sizeof (struct attr_abbrev));

      hash_number = abbrev_number % ABBREV_HASH_SIZE;
      cur_abbrev->next = cu->dwarf2_abbrevs[hash_number];
      cu->dwarf2_abbrevs[hash_number] = cur_abbrev;

      /* Get next abbreviation.
         Under Irix6 the abbreviations for a compilation unit are not
         always properly terminated with an abbrev number of 0.
         Exit loop if we encounter an abbreviation which we have
         already read (which means we are about to read the abbreviations
         for the next compile unit) or if the end of the abbreviation
         table is reached.  */
      if ((unsigned int) (abbrev_ptr - dwarf2_per_objfile->abbrev_buffer)
	  >= dwarf2_per_objfile->abbrev_size)
	break;
      abbrev_number = read_unsigned_leb128 (abfd, abbrev_ptr, &bytes_read);
      abbrev_ptr += bytes_read;
      if (dwarf2_lookup_abbrev (abbrev_number, cu) != NULL)
	break;
    }

  xfree (cur_attrs);
}

/* Release the memory used by the abbrev table for a compilation unit.  */

static void
dwarf2_free_abbrev_table (void *ptr_to_cu)
{
  struct dwarf2_cu *cu = ptr_to_cu;

  obstack_free (&cu->abbrev_obstack, NULL);
  cu->dwarf2_abbrevs = NULL;
}

/* Lookup an abbrev_info structure in the abbrev hash table.  */

static struct abbrev_info *
dwarf2_lookup_abbrev (unsigned int number, struct dwarf2_cu *cu)
{
  unsigned int hash_number;
  struct abbrev_info *abbrev;

  hash_number = number % ABBREV_HASH_SIZE;
  abbrev = cu->dwarf2_abbrevs[hash_number];

  while (abbrev)
    {
      if (abbrev->number == number)
	return abbrev;
      else
	abbrev = abbrev->next;
    }
  return NULL;
}

/* Returns nonzero if TAG represents a type that we might generate a partial
   symbol for.  */

static int
is_type_tag_for_partial (int tag)
{
  switch (tag)
    {
#if 0
    /* Some types that would be reasonable to generate partial symbols for,
       that we don't at present.  */
    case DW_TAG_array_type:
    case DW_TAG_file_type:
    case DW_TAG_ptr_to_member_type:
    case DW_TAG_set_type:
    case DW_TAG_string_type:
    case DW_TAG_subroutine_type:
#endif
    case DW_TAG_base_type:
    case DW_TAG_class_type:
    case DW_TAG_enumeration_type:
    case DW_TAG_structure_type:
    case DW_TAG_subrange_type:
    case DW_TAG_typedef:
    case DW_TAG_union_type:
      return 1;
    default:
      return 0;
    }
}

/* Load all DIEs that are interesting for partial symbols into memory.  */

static struct partial_die_info *
load_partial_dies (bfd *abfd, char *info_ptr, int building_psymtab,
		   struct dwarf2_cu *cu)
{
  struct partial_die_info *part_die;
  struct partial_die_info *parent_die, *last_die, *first_die = NULL;
  struct abbrev_info *abbrev;
  unsigned int bytes_read;

  int nesting_level = 1;

  parent_die = NULL;
  last_die = NULL;

  cu->partial_dies
    = htab_create_alloc_ex (cu->header.length / 12,
			    partial_die_hash,
			    partial_die_eq,
			    NULL,
			    &cu->comp_unit_obstack,
			    hashtab_obstack_allocate,
			    dummy_obstack_deallocate);

  part_die = obstack_alloc (&cu->comp_unit_obstack,
			    sizeof (struct partial_die_info));

  while (1)
    {
      /* APPLE LOCAL Add cast to avoid type mismatch in arg2 warning.  */
      abbrev = peek_die_abbrev (info_ptr, (int *) &bytes_read, cu);

      /* A NULL abbrev means the end of a series of children.  */
      if (abbrev == NULL)
	{
	  if (--nesting_level == 0)
	    {
	      /* PART_DIE was probably the last thing allocated on the
		 comp_unit_obstack, so we could call obstack_free
		 here.  We don't do that because the waste is small,
		 and will be cleaned up when we're done with this
		 compilation unit.  This way, we're also more robust
		 against other users of the comp_unit_obstack.  */
	      return first_die;
	    }
	  info_ptr += bytes_read;
	  last_die = parent_die;
	  parent_die = parent_die->die_parent;
	  continue;
	}

      /* Check whether this DIE is interesting enough to save.  */
      if (!is_type_tag_for_partial (abbrev->tag)
	  && abbrev->tag != DW_TAG_enumerator
	  && abbrev->tag != DW_TAG_subprogram
	  && abbrev->tag != DW_TAG_variable
	  && abbrev->tag != DW_TAG_namespace)
	{
	  /* Otherwise we skip to the next sibling, if any.  */
	  info_ptr = skip_one_die (info_ptr + bytes_read, abbrev, cu);
	  continue;
	}

      info_ptr = read_partial_die (part_die, abbrev, bytes_read,
				   abfd, info_ptr, cu);

      /* This two-pass algorithm for processing partial symbols has a
	 high cost in cache pressure.  Thus, handle some simple cases
	 here which cover the majority of C partial symbols.  DIEs
	 which neither have specification tags in them, nor could have
	 specification tags elsewhere pointing at them, can simply be
	 processed and discarded.

	 This segment is also optional; scan_partial_symbols and
	 add_partial_symbol will handle these DIEs if we chain
	 them in normally.  When compilers which do not emit large
	 quantities of duplicate debug information are more common,
	 this code can probably be removed.  */

      /* Any complete simple types at the top level (pretty much all
	 of them, for a language without namespaces), can be processed
	 directly.  */
      if (parent_die == NULL
	  && part_die->has_specification == 0
	  && part_die->is_declaration == 0
	  && (part_die->tag == DW_TAG_typedef
	      || part_die->tag == DW_TAG_base_type
	      || part_die->tag == DW_TAG_subrange_type))
	{
	  if (building_psymtab && part_die->name != NULL)
            /* APPLE LOCAL: Put it in the global_psymbols list, not 
               static_psymbols.  */
	    add_psymbol_to_list (part_die->name, strlen (part_die->name),
				 VAR_DOMAIN, LOC_TYPEDEF,
				 &cu->objfile->global_psymbols,
				 0, (CORE_ADDR) 0, cu->language, cu->objfile);
	  info_ptr = locate_pdi_sibling (part_die, info_ptr, abfd, cu);
	  continue;
	}

      /* If we're at the second level, and we're an enumerator, and
	 our parent has no specification (meaning possibly lives in a
	 namespace elsewhere), then we can add the partial symbol now
	 instead of queueing it.  */
      if (part_die->tag == DW_TAG_enumerator
	  && parent_die != NULL
	  && parent_die->die_parent == NULL
	  && parent_die->tag == DW_TAG_enumeration_type
	  && parent_die->has_specification == 0)
	{
	  if (part_die->name == NULL)
	    complaint (&symfile_complaints, _("malformed enumerator DIE ignored"));
	  else if (building_psymtab)
            /* APPLE LOCAL: Put it in the global_psymbols list regardless
               of language.  */
	    add_psymbol_to_list (part_die->name, strlen (part_die->name),
				 VAR_DOMAIN, LOC_CONST,
				 &cu->objfile->global_psymbols,
				 0, (CORE_ADDR) 0, cu->language, cu->objfile);

	  info_ptr = locate_pdi_sibling (part_die, info_ptr, abfd, cu);
	  continue;
	}

      /* We'll save this DIE so link it in.  */
      part_die->die_parent = parent_die;
      part_die->die_sibling = NULL;
      part_die->die_child = NULL;

      if (last_die && last_die == parent_die)
	last_die->die_child = part_die;
      else if (last_die)
	last_die->die_sibling = part_die;

      last_die = part_die;

      if (first_die == NULL)
	first_die = part_die;

      /* Maybe add the DIE to the hash table.  Not all DIEs that we
	 find interesting need to be in the hash table, because we
	 also have the parent/sibling/child chains; only those that we
	 might refer to by offset later during partial symbol reading.

	 For now this means things that might have be the target of a
	 DW_AT_specification, DW_AT_abstract_origin, or
	 DW_AT_extension.  DW_AT_extension will refer only to
	 namespaces; DW_AT_abstract_origin refers to functions (and
	 many things under the function DIE, but we do not recurse
	 into function DIEs during partial symbol reading) and
	 possibly variables as well; DW_AT_specification refers to
	 declarations.  Declarations ought to have the DW_AT_declaration
	 flag.  It happens that GCC forgets to put it in sometimes, but
	 only for functions, not for types.

	 Adding more things than necessary to the hash table is harmless
	 except for the performance cost.  Adding too few will result in
	 internal errors in find_partial_die.  */

      if (abbrev->tag == DW_TAG_subprogram
	  || abbrev->tag == DW_TAG_variable
	  || abbrev->tag == DW_TAG_namespace
	  || part_die->is_declaration)
	{
	  void **slot;

	  slot = htab_find_slot_with_hash (cu->partial_dies, part_die,
					   part_die->offset, INSERT);
	  *slot = part_die;
	}

      part_die = obstack_alloc (&cu->comp_unit_obstack,
				sizeof (struct partial_die_info));

      /* For some DIEs we want to follow their children (if any).  For C
         we have no reason to follow the children of structures; for other
	 languages we have to, both so that we can get at method physnames
	 to infer fully qualified class names, and for DW_AT_specification.  */
      if (last_die->has_children
	  && (last_die->tag == DW_TAG_namespace
	      || last_die->tag == DW_TAG_enumeration_type
	      || (cu->language != language_c
		  && (last_die->tag == DW_TAG_class_type
		      || last_die->tag == DW_TAG_structure_type
		      || last_die->tag == DW_TAG_union_type))))
	{
	  nesting_level++;
	  parent_die = last_die;
	  continue;
	}

      /* Otherwise we skip to the next sibling, if any.  */
      info_ptr = locate_pdi_sibling (last_die, info_ptr, abfd, cu);

      /* Back to the top, do it again.  */
    }
}

/* Read a minimal amount of information into the minimal die structure.  */

static char *
read_partial_die (struct partial_die_info *part_die,
		  struct abbrev_info *abbrev,
		  unsigned int abbrev_len, bfd *abfd,
		  char *info_ptr, struct dwarf2_cu *cu)
{
  /* APPLE LOCAL avoid unused var warning.  */
  /* unsigned int bytes_read; */
  unsigned int i;
  struct attribute attr;
  int has_low_pc_attr = 0;
  int has_high_pc_attr = 0;

  memset (part_die, 0, sizeof (struct partial_die_info));

  part_die->offset = info_ptr - dwarf2_per_objfile->info_buffer;
  /* APPLE LOCAL begin  dwarf repository  */
  part_die->has_repository = 0;
  part_die->has_repo_specification = 0;
  part_die->has_repository_type = 0;
  /* APPLE LOCAL end dwarf repository  */

  info_ptr += abbrev_len;

  if (abbrev == NULL)
    return info_ptr;

  part_die->tag = abbrev->tag;
  part_die->has_children = abbrev->has_children;

  for (i = 0; i < abbrev->num_attrs; ++i)
    {
      info_ptr = read_attribute (&attr, &abbrev->attrs[i], abfd, info_ptr, cu);

      /* Store the data if it is of an attribute we want to keep in a
         partial symbol table.  */
      switch (attr.name)
	{
	case DW_AT_name:
	  /* APPLE LOCAL begin dwarf repository  */
	  /* Prefer DW_AT_MIPS_linkage_name over DW_AT_name.  */
	  if (part_die->name == NULL)
	    {
	      if (attr.form == DW_FORM_APPLE_db_str)
		part_die->name = DW_STRING (get_repository_name (&attr, cu));
	      else
		part_die->name = DW_STRING (&attr);
	    }
	  break;
	case DW_AT_APPLE_repository_file:
	  part_die->repo_name = DW_STRING (&attr);
	  part_die->has_repository = 1;
	  break;
	case DW_AT_comp_dir:
	  if (part_die->dirname == NULL)
	    {
	      if (attr.form == DW_FORM_APPLE_db_str)
		part_die->dirname = DW_STRING (get_repository_name (&attr, cu));
	      else
		part_die->dirname = DW_STRING (&attr);
	    }
	  break;
	case DW_AT_MIPS_linkage_name:
	    {
	      if (attr.form == DW_FORM_APPLE_db_str)
		part_die->name = DW_STRING (get_repository_name (&attr, cu));
	      else
		part_die->name = DW_STRING (&attr);
	      /* APPLE LOCAL begin psym equivalences  */
	      /* Check the linkage name to see if it is a psym equivalence
		 name.  If so, fill in the equiv_name field for the part die,
		 and set the global variable, psym_equivalences to 1.  */

	      /* To be a psym equivalence name, the name must begin with
		 '*_', must contain at least one  '$', and everything after
		 the '$' must be uppercase, a digit or a '$'.  For
	         example '*_putenv$UNIX2003' is a psym equivalence name for
	         'putenv'.  */

	      if (strlen (part_die->name) >=  5
		  && part_die->name[0] == '*'
		  && part_die->name[1] == '_')
		{
		  char *short_name;
		  char *extension;
		  char *short_end;
		  char *end = part_die->name + strlen(part_die->name);
		  int is_equivalence_name = 1;
		  
		  short_name = part_die->name + 2;
		  extension = strchr (short_name, '$');
		  if (extension)
		    {
		      short_end = extension;
		      while (extension[0] != '\0'
			     && extension < end
			     && is_equivalence_name)
			{
			  if (!isdigit(extension[0])
			      && !isupper(extension[0])
			      && extension[0] != '$')
			    is_equivalence_name = 0;
			  extension++;
			}
		      
		    }
		  else
		    is_equivalence_name = 0;
		  
		  if (is_equivalence_name)
		    {
		      psym_equivalences = 1;
		      short_end[0] = '\0';
		      part_die->name = xstrdup (short_name);
		      part_die->equiv_name = part_die->name;
		      short_end[0] = '$';
		    }
		  
		}
	      /* APPLE LOCAL end psym equivalences  */
	    }
	  break;
	/* APPLE LOCAL end dwarf repository  */
	case DW_AT_low_pc:
          /* APPLE LOCAL: debug map */
          {
            CORE_ADDR addr;
            if (translate_debug_map_address (cu->addr_map, DW_ADDR (&attr), &addr, 0))
              {
                part_die->lowpc = addr;
	        has_low_pc_attr = 1;
              }
          }
          break;
	case DW_AT_high_pc:
          /* APPLE LOCAL: debug map */
          {
            CORE_ADDR addr;
            if (translate_debug_map_address (cu->addr_map, DW_ADDR (&attr), &addr, 1))
              {
	        has_high_pc_attr = 1;
                part_die->highpc = addr;
              }
          }
	  break;
	case DW_AT_location:
          /* Support the .debug_loc offsets */
          if (attr_form_is_block (&attr))
            {
	       part_die->locdesc = DW_BLOCK (&attr);
            }
          else if (attr.form == DW_FORM_data4 || attr.form == DW_FORM_data8)
            {
	      dwarf2_complex_location_expr_complaint ();
            }
          else
            {
	      dwarf2_invalid_attrib_class_complaint ("DW_AT_location",
						     "partial symbol information");
            }
	  break;
	case DW_AT_language:
	  part_die->language = DW_UNSND (&attr);
	  break;
	case DW_AT_external:
	  part_die->is_external = DW_UNSND (&attr);
	  break;
	case DW_AT_declaration:
	  part_die->is_declaration = DW_UNSND (&attr);
	  break;
	case DW_AT_type:
	  part_die->has_type = 1;
	  break;
	case DW_AT_abstract_origin:
	case DW_AT_specification:
	case DW_AT_extension:
	  part_die->has_specification = 1;
	  part_die->spec_offset = dwarf2_get_ref_die_offset (&attr, cu);
	  break;
        /* APPLE LOCAL begin dwarf repository  */
	case DW_AT_APPLE_repository_specification:
	  part_die->has_repo_specification = 1;
	  part_die->repo_spec_id = DW_UNSND (&attr);
	  break;
	case DW_AT_APPLE_repository_type:
	  part_die->has_repository_type = 1;
	  break;
	/* APPLE LOCAL end dwarf repository  */
	case DW_AT_sibling:
	  /* Ignore absolute siblings, they might point outside of
	     the current compile unit.  */
	  if (attr.form == DW_FORM_ref_addr)
	    complaint (&symfile_complaints, _("ignoring absolute DW_AT_sibling"));
	  else
	    part_die->sibling = dwarf2_per_objfile->info_buffer
	      + dwarf2_get_ref_die_offset (&attr, cu);
	  break;
        case DW_AT_stmt_list:
          part_die->has_stmt_list = 1;
          part_die->line_offset = DW_UNSND (&attr);
          break;
	default:
	  break;
	}
    }

  /* When using the GNU linker, .gnu.linkonce. sections are used to
     eliminate duplicate copies of functions and vtables and such.
     The linker will arbitrarily choose one and discard the others.
     The AT_*_pc values for such functions refer to local labels in
     these sections.  If the section from that file was discarded, the
     labels are not in the output, so the relocs get a value of 0.
     If this is a discarded function, mark the pc bounds as invalid,
     so that GDB will ignore it.  */
  if (has_low_pc_attr && has_high_pc_attr
      && part_die->lowpc < part_die->highpc
      && (part_die->lowpc != 0
	  || (bfd_get_file_flags (abfd) & HAS_RELOC)))
    part_die->has_pc_info = 1;

  /* APPLE LOCAL begin dwarf repository  */
  if (part_die->has_repository)
    open_dwarf_repository (part_die->dirname, part_die->repo_name,
			   cu->objfile, cu);
  /* APPLE LOCAL end dwarf repository  */

  return info_ptr;
}

/* Find a cached partial DIE at OFFSET in CU.  */

static struct partial_die_info *
find_partial_die_in_comp_unit (unsigned long offset, struct dwarf2_cu *cu)
{
  struct partial_die_info *lookup_die = NULL;
  struct partial_die_info part_die;

  part_die.offset = offset;
  lookup_die = htab_find_with_hash (cu->partial_dies, &part_die, offset);

  /* FIXME: Remove this once <rdar://problem/6193416> is fixed */
  if (lookup_die == NULL)
    internal_error (__FILE__, __LINE__,
		    _("could not find partial DIE in cache\n"));

  return lookup_die;
}

/* Find a partial DIE at OFFSET, which may or may not be in CU.  */

static struct partial_die_info *
find_partial_die (unsigned long offset, struct dwarf2_cu *cu)
{
  struct dwarf2_per_cu_data *per_cu;

  if (offset >= cu->header.offset
      && offset < cu->header.offset + cu->header.length)
    return find_partial_die_in_comp_unit (offset, cu);

  per_cu = dwarf2_find_containing_comp_unit (offset, cu->objfile);

  if (per_cu->cu == NULL)
    {
      load_comp_unit (per_cu, cu->objfile);
      per_cu->cu->read_in_chain = dwarf2_per_objfile->read_in_chain;
      dwarf2_per_objfile->read_in_chain = per_cu;
    }

  per_cu->cu->last_used = 0;
  return find_partial_die_in_comp_unit (offset, per_cu->cu);
}

/* Adjust PART_DIE before generating a symbol for it.  This function
   may set the is_external flag or change the DIE's name.  */

static void
fixup_partial_die (struct partial_die_info *part_die,
		   struct dwarf2_cu *cu)
{
  /* If we found a reference attribute and the DIE has no name, try
     to find a name in the referred to DIE.  */

  if (part_die->name == NULL && part_die->has_specification)
    {
      struct partial_die_info *spec_die;

      spec_die = find_partial_die (part_die->spec_offset, cu);

      fixup_partial_die (spec_die, cu);

      if (spec_die->name)
	{
	  part_die->name = spec_die->name;

	  /* Copy DW_AT_external attribute if it is set.  */
	  if (spec_die->is_external)
	    part_die->is_external = spec_die->is_external;
	}
    }

  /* Set default names for some unnamed DIEs.  */
  if (part_die->name == NULL && (part_die->tag == DW_TAG_structure_type
				 || part_die->tag == DW_TAG_class_type))
    part_die->name = "(anonymous class)";

  if (part_die->name == NULL && part_die->tag == DW_TAG_namespace)
    part_die->name = "(anonymous namespace)";

  if (part_die->tag == DW_TAG_structure_type
      || part_die->tag == DW_TAG_class_type
      || part_die->tag == DW_TAG_union_type)
    guess_structure_name (part_die, cu);
}

/* Read the die from the .debug_info section buffer.  Set DIEP to
   point to a newly allocated die with its information, except for its
   child, sibling, and parent fields.  Set HAS_CHILDREN to tell
   whether the die has children or not.  */

static char *
read_full_die (struct die_info **diep, bfd *abfd, char *info_ptr,
	       struct dwarf2_cu *cu, int *has_children)
{
  unsigned int abbrev_number, bytes_read, i, offset;
  struct abbrev_info *abbrev;
  struct die_info *die;
  /* APPLE LOCAL begin dwarf repository  */
  char *repository_name = NULL;
  char *comp_dir = NULL;
  /* APPLE LOCAL end dwarf repository  */

  offset = info_ptr - dwarf2_per_objfile->info_buffer;
  abbrev_number = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
  info_ptr += bytes_read;
  if (!abbrev_number)
    {
      die = dwarf_alloc_die ();
      die->tag = 0;
      die->abbrev = abbrev_number;
      die->type = NULL;
      *diep = die;
      *has_children = 0;
      return info_ptr;
    }

  abbrev = dwarf2_lookup_abbrev (abbrev_number, cu);
  if (!abbrev)
    {
      error (_("Dwarf Error: could not find abbrev number %d [in module %s]"),
	     abbrev_number,
	     bfd_get_filename (abfd));
    }
  die = dwarf_alloc_die ();
  die->offset = offset;
  /* APPLE LOCAL - dwarf repository  */
  die->repository_id = 0;
  die->tag = abbrev->tag;
  die->abbrev = abbrev_number;
  die->type = NULL;

  die->num_attrs = abbrev->num_attrs;
  die->attrs = (struct attribute *)
    xmalloc (die->num_attrs * sizeof (struct attribute));

  for (i = 0; i < abbrev->num_attrs; ++i)
    {
      info_ptr = read_attribute (&die->attrs[i], &abbrev->attrs[i],
				 abfd, info_ptr, cu);

      /* APPLE LOCAL begin dwarf repository  */
      if (die->attrs[i].name == DW_AT_APPLE_repository_file)
	repository_name = DW_STRING (&die->attrs[i]);
      else if (die->attrs[i].name == DW_AT_comp_dir)
	comp_dir = DW_STRING (&die->attrs[i]);
      /* APPLE LOCAL end dwarf repository  */

      /* If this attribute is an absolute reference to a different
	 compilation unit, make sure that compilation unit is loaded
	 also.  */
      if (die->attrs[i].form == DW_FORM_ref_addr
	  && (DW_ADDR (&die->attrs[i]) < cu->header.offset
	      || (DW_ADDR (&die->attrs[i])
		  >= cu->header.offset + cu->header.length)))
	{
	  struct dwarf2_per_cu_data *per_cu;
	  per_cu = dwarf2_find_containing_comp_unit (DW_ADDR (&die->attrs[i]),
						     cu->objfile);

	  /* Mark the dependence relation so that we don't flush PER_CU
	     too early.  */
	  dwarf2_add_dependence (cu, per_cu);

	  /* If it's already on the queue, we have nothing to do.  */
	  if (per_cu->queued)
	    continue;

	  /* If the compilation unit is already loaded, just mark it as
	     used.  */
	  if (per_cu->cu != NULL)
	    {
	      per_cu->cu->last_used = 0;
	      continue;
	    }

	  /* Add it to the queue.  */
	  queue_comp_unit (per_cu);
       }
    }

  /* APPLE LOCAL begin dwarf repository  */
  if (repository_name)
    open_dwarf_repository (comp_dir, repository_name, cu->objfile, cu);
  /* APPLE LOCAL end dwarf repository  */

  *diep = die;
  *has_children = abbrev->has_children;
  return info_ptr;
}

/* Read an attribute value described by an attribute form.  */

static char *
read_attribute_value (struct attribute *attr, unsigned form,
		      bfd *abfd, char *info_ptr,
		      struct dwarf2_cu *cu)
{
  struct comp_unit_head *cu_header = &cu->header;
  unsigned int bytes_read;
  struct dwarf_block *blk;

  attr->form = form;
  switch (form)
    {
    case DW_FORM_addr:
    case DW_FORM_ref_addr:
      /* APPLE LOCAL Add cast to avoid type mismatch in arg4 warning.  */
      DW_ADDR (attr) = read_address (abfd, info_ptr, cu, (int *) &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_block2:
      blk = dwarf_alloc_block (cu);
      blk->size = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block4:
      blk = dwarf_alloc_block (cu);
      blk->size = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data2:
      DW_UNSND (attr) = read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      break;
    case DW_FORM_data4:
      DW_UNSND (attr) = read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      break;
    case DW_FORM_data8:
      DW_UNSND (attr) = read_8_bytes (abfd, info_ptr);
      info_ptr += 8;
      break;
    case DW_FORM_string:
      DW_STRING (attr) = read_string (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_strp:
      DW_STRING (attr) = read_indirect_string (abfd, info_ptr, cu_header,
					       &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_block:
      blk = dwarf_alloc_block (cu);
      blk->size = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block1:
      blk = dwarf_alloc_block (cu);
      blk->size = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      blk->data = read_n_bytes (abfd, info_ptr, blk->size);
      info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data1:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_flag:
      DW_UNSND (attr) = read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_sdata:
      DW_SND (attr) = read_signed_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_APPLE_db_str:
    case DW_FORM_udata:
      DW_UNSND (attr) = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      break;
    case DW_FORM_ref1:
      DW_ADDR (attr) = cu->header.offset + read_1_byte (abfd, info_ptr);
      info_ptr += 1;
      break;
    case DW_FORM_ref2:
      DW_ADDR (attr) = cu->header.offset + read_2_bytes (abfd, info_ptr);
      info_ptr += 2;
      break;
    case DW_FORM_ref4:
      DW_ADDR (attr) = cu->header.offset + read_4_bytes (abfd, info_ptr);
      info_ptr += 4;
      break;
    case DW_FORM_ref8:
      DW_ADDR (attr) = cu->header.offset + read_8_bytes (abfd, info_ptr);
      info_ptr += 8;
      break;
    case DW_FORM_ref_udata:
      DW_ADDR (attr) = (cu->header.offset
			+ read_unsigned_leb128 (abfd, info_ptr, &bytes_read));
      info_ptr += bytes_read;
      break;
    case DW_FORM_indirect:
      form = read_unsigned_leb128 (abfd, info_ptr, &bytes_read);
      info_ptr += bytes_read;
      info_ptr = read_attribute_value (attr, form, abfd, info_ptr, cu);
      break;
    default:
      error (_("Dwarf Error: Cannot handle %s in DWARF reader [in module %s]"),
	     dwarf_form_name (form),
	     bfd_get_filename (abfd));
    }
  return info_ptr;
}

/* Read an attribute described by an abbreviated attribute.  */

static char *
read_attribute (struct attribute *attr, struct attr_abbrev *abbrev,
		bfd *abfd, char *info_ptr, struct dwarf2_cu *cu)
{
  attr->name = abbrev->name;
  return read_attribute_value (attr, abbrev->form, abfd, info_ptr, cu);
}

/* read dwarf information from a buffer */

static unsigned int
read_1_byte (bfd *abfd, char *buf)
{
  return bfd_get_8 (abfd, (bfd_byte *) buf);
}

static int
read_1_signed_byte (bfd *abfd, char *buf)
{
  return bfd_get_signed_8 (abfd, (bfd_byte *) buf);
}

static unsigned int
read_2_bytes (bfd *abfd, char *buf)
{
  return bfd_get_16 (abfd, (bfd_byte *) buf);
}

static int
read_2_signed_bytes (bfd *abfd, char *buf)
{
  return bfd_get_signed_16 (abfd, (bfd_byte *) buf);
}

static unsigned int
read_4_bytes (bfd *abfd, char *buf)
{
  return bfd_get_32 (abfd, (bfd_byte *) buf);
}

static int
read_4_signed_bytes (bfd *abfd, char *buf)
{
  return bfd_get_signed_32 (abfd, (bfd_byte *) buf);
}

static unsigned long
read_8_bytes (bfd *abfd, char *buf)
{
  return bfd_get_64 (abfd, (bfd_byte *) buf);
}

static CORE_ADDR
read_address (bfd *abfd, char *buf, struct dwarf2_cu *cu, int *bytes_read)
{
  struct comp_unit_head *cu_header = &cu->header;
  CORE_ADDR retval = 0;

  if (cu_header->signed_addr_p)
    {
      switch (cu_header->addr_size)
	{
	case 2:
	  retval = bfd_get_signed_16 (abfd, (bfd_byte *) buf);
	  break;
	case 4:
	  retval = bfd_get_signed_32 (abfd, (bfd_byte *) buf);
	  break;
	case 8:
	  retval = bfd_get_signed_64 (abfd, (bfd_byte *) buf);
	  break;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("read_address: bad switch, signed [in module %s]"),
			  bfd_get_filename (abfd));
	}
    }
  else
    {
      switch (cu_header->addr_size)
	{
	case 2:
	  retval = bfd_get_16 (abfd, (bfd_byte *) buf);
	  break;
	case 4:
	  retval = bfd_get_32 (abfd, (bfd_byte *) buf);
	  break;
	case 8:
	  retval = bfd_get_64 (abfd, (bfd_byte *) buf);
	  break;
	default:
	  internal_error (__FILE__, __LINE__,
			  _("read_address: bad switch, unsigned [in module %s]"),
			  bfd_get_filename (abfd));
	}
    }

  *bytes_read = cu_header->addr_size;
  return retval;
}

/* Read the initial length from a section.  The (draft) DWARF 3
   specification allows the initial length to take up either 4 bytes
   or 12 bytes.  If the first 4 bytes are 0xffffffff, then the next 8
   bytes describe the length and all offsets will be 8 bytes in length
   instead of 4.

   An older, non-standard 64-bit format is also handled by this
   function.  The older format in question stores the initial length
   as an 8-byte quantity without an escape value.  Lengths greater
   than 2^32 aren't very common which means that the initial 4 bytes
   is almost always zero.  Since a length value of zero doesn't make
   sense for the 32-bit format, this initial zero can be considered to
   be an escape value which indicates the presence of the older 64-bit
   format.  As written, the code can't detect (old format) lengths
   greater than 4GB.  If it becomes necessary to handle lengths
   somewhat larger than 4GB, we could allow other small values (such
   as the non-sensical values of 1, 2, and 3) to also be used as
   escape values indicating the presence of the old format.

   The value returned via bytes_read should be used to increment the
   relevant pointer after calling read_initial_length().
   
   As a side effect, this function sets the fields initial_length_size
   and offset_size in cu_header to the values appropriate for the
   length field.  (The format of the initial length field determines
   the width of file offsets to be fetched later with read_offset().)
   
   [ Note:  read_initial_length() and read_offset() are based on the
     document entitled "DWARF Debugging Information Format", revision
     3, draft 8, dated November 19, 2001.  This document was obtained
     from:

	http://reality.sgiweb.org/davea/dwarf3-draft8-011125.pdf
     
     This document is only a draft and is subject to change.  (So beware.)

     Details regarding the older, non-standard 64-bit format were
     determined empirically by examining 64-bit ELF files produced by
     the SGI toolchain on an IRIX 6.5 machine.

     - Kevin, July 16, 2002
   ] */

static LONGEST
read_initial_length (bfd *abfd, char *buf, struct comp_unit_head *cu_header,
                     int *bytes_read)
{
  LONGEST length = bfd_get_32 (abfd, (bfd_byte *) buf);

  if (length == 0xffffffff)
    {
      length = bfd_get_64 (abfd, (bfd_byte *) buf + 4);
      *bytes_read = 12;
    }
  else if (length == 0)
    {
      /* Handle the (non-standard) 64-bit DWARF2 format used by IRIX.  */
      length = bfd_get_64 (abfd, (bfd_byte *) buf);
      *bytes_read = 8;
    }
  else
    {
      *bytes_read = 4;
    }

  if (cu_header)
    {
      gdb_assert (cu_header->initial_length_size == 0
		  || cu_header->initial_length_size == 4
		  || cu_header->initial_length_size == 8
		  || cu_header->initial_length_size == 12);

      if (cu_header->initial_length_size != 0
	  && cu_header->initial_length_size != *bytes_read)
	complaint (&symfile_complaints,
		   _("intermixed 32-bit and 64-bit DWARF sections"));

      cu_header->initial_length_size = *bytes_read;
      cu_header->offset_size = (*bytes_read == 4) ? 4 : 8;
    }

  return length;
}

/* Read an offset from the data stream.  The size of the offset is
   given by cu_header->offset_size.  */

static LONGEST
read_offset (bfd *abfd, char *buf, const struct comp_unit_head *cu_header,
             int *bytes_read)
{
  LONGEST retval = 0;

  switch (cu_header->offset_size)
    {
    case 4:
      retval = bfd_get_32 (abfd, (bfd_byte *) buf);
      *bytes_read = 4;
      break;
    case 8:
      retval = bfd_get_64 (abfd, (bfd_byte *) buf);
      *bytes_read = 8;
      break;
    default:
      internal_error (__FILE__, __LINE__,
		      _("read_offset: bad switch [in module %s]"),
		      bfd_get_filename (abfd));
    }

  return retval;
}

static char *
read_n_bytes (bfd *abfd, char *buf, unsigned int size)
{
  /* If the size of a host char is 8 bits, we can return a pointer
     to the buffer, otherwise we have to copy the data to a buffer
     allocated on the temporary obstack.  */
  gdb_assert (HOST_CHAR_BIT == 8);
  return buf;
}

static char *
read_string (bfd *abfd, char *buf, unsigned int *bytes_read_ptr)
{
  /* If the size of a host char is 8 bits, we can return a pointer
     to the string, otherwise we have to copy the string to a buffer
     allocated on the temporary obstack.  */
  gdb_assert (HOST_CHAR_BIT == 8);
  if (*buf == '\0')
    {
      *bytes_read_ptr = 1;
      return NULL;
    }
  *bytes_read_ptr = strlen (buf) + 1;
  return buf;
}

static char *
read_indirect_string (bfd *abfd, char *buf,
		      const struct comp_unit_head *cu_header,
		      unsigned int *bytes_read_ptr)
{
  LONGEST str_offset = read_offset (abfd, buf, cu_header,
				    (int *) bytes_read_ptr);

  if (dwarf2_per_objfile->str_buffer == NULL)
    {
      error (_("DW_FORM_strp used without .debug_str section [in module %s]"),
		      bfd_get_filename (abfd));
      return NULL;
    }
  if (str_offset >= dwarf2_per_objfile->str_size)
    {
      error (_("DW_FORM_strp pointing outside of .debug_str section [in module %s]"),
		      bfd_get_filename (abfd));
      return NULL;
    }
  gdb_assert (HOST_CHAR_BIT == 8);
  if (dwarf2_per_objfile->str_buffer[str_offset] == '\0')
    return NULL;
  return dwarf2_per_objfile->str_buffer + str_offset;
}

static unsigned long
read_unsigned_leb128 (bfd *abfd, char *buf, unsigned int *bytes_read_ptr)
{
  unsigned long result;
  unsigned int num_read;
  int i, shift;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((unsigned long)(byte & 127) << shift);
      if ((byte & 128) == 0)
	{
	  break;
	}
      shift += 7;
    }
  *bytes_read_ptr = num_read;
  return result;
}

static long
read_signed_leb128 (bfd *abfd, char *buf, unsigned int *bytes_read_ptr)
{
  long result;
  int i, shift, num_read;
  unsigned char byte;

  result = 0;
  shift = 0;
  num_read = 0;
  i = 0;
  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      num_read++;
      result |= ((long)(byte & 127) << shift);
      shift += 7;
      if ((byte & 128) == 0)
	{
	  break;
	}
    }
  if ((shift < 8 * sizeof (result)) && (byte & 0x40))
    result |= -(((long)1) << shift);
  *bytes_read_ptr = num_read;
  return result;
}

/* Return a pointer to just past the end of an LEB128 number in BUF.  */

static char *
skip_leb128 (bfd *abfd, char *buf)
{
  int byte;

  while (1)
    {
      byte = bfd_get_8 (abfd, (bfd_byte *) buf);
      buf++;
      if ((byte & 128) == 0)
	return buf;
    }
}

static void
set_cu_language (unsigned int lang, struct dwarf2_cu *cu)
{
  switch (lang)
    {
    case DW_LANG_C89:
    case DW_LANG_C:
      cu->language = language_c;
      break;
    case DW_LANG_C_plus_plus:
      cu->language = language_cplus;
      break;
    case DW_LANG_Fortran77:
    case DW_LANG_Fortran90:
    case DW_LANG_Fortran95:
      cu->language = language_fortran;
      break;
    case DW_LANG_Mips_Assembler:
      cu->language = language_asm;
      break;
    case DW_LANG_Java:
      cu->language = language_java;
      break;
    case DW_LANG_Ada83:
    case DW_LANG_Ada95:
      cu->language = language_ada;
      break;
    case DW_LANG_Pascal83:
      cu->language = language_pascal;
      break;
    /* APPLE LOCAL:  No need to be Apple local but not merged in to FSF..  */
    case DW_LANG_ObjC:
      cu->language = language_objc;
      break;
    /* APPLE LOCAL:  No need to be Apple local but not merged in to FSF..  */
    case DW_LANG_ObjC_plus_plus:
      cu->language = language_objcplus;
      break;
    case DW_LANG_Cobol74:
    case DW_LANG_Cobol85:
    case DW_LANG_Modula2:
    default:
      cu->language = language_minimal;
      break;
    }
  cu->language_defn = language_def (cu->language);
}

/* Return the named attribute or NULL if not there.  */

static struct attribute *
dwarf2_attr (struct die_info *die, unsigned int name, struct dwarf2_cu *cu)
{
  unsigned int i;
  struct attribute *spec = NULL;
  /* APPLE LOCAL - dwarf repository  */
  struct attribute *repository_spec = NULL;

  for (i = 0; i < die->num_attrs; ++i)
    {
      if (die->attrs[i].name == name)
	/* APPLE LOCAL begin dwarf repository  */
	{
	  if (die->attrs[i].form == DW_FORM_APPLE_db_str)
	    return get_repository_name (&(die->attrs[i]), cu);
	  else
	    return &die->attrs[i];
	}
      if (die->attrs[i].name == DW_AT_specification
	  || die->attrs[i].name == DW_AT_abstract_origin)
	spec = &die->attrs[i];
      if (die->attrs[i].name == DW_AT_APPLE_repository_specification)
	repository_spec = &die->attrs[i];
      if (cu->repository && die->attrs[i].name == DW_AT_APPLE_repository_name
	  && name == DW_AT_name)
	return get_repository_name (&(die->attrs[i]), cu);
      /* APPLE LOCAL end dwarf repository  */
    }

  if (spec)
    return dwarf2_attr (follow_die_ref (die, spec, cu), name, cu);
  
  /* APPLE LOCAL begin dwarf repository  */
  if (repository_spec)
    {
      return dwarf2_attr (follow_db_ref (die, repository_spec, cu), name, cu);
    }
  /* APPLE LOCAL end dwarf repository  */

  return NULL;
}

/* Return non-zero iff the attribute NAME is defined for the given DIE,
   and holds a non-zero value.  This function should only be used for
   DW_FORM_flag attributes.  */

static int
dwarf2_flag_true_p (struct die_info *die, unsigned name, struct dwarf2_cu *cu)
{
  struct attribute *attr = dwarf2_attr (die, name, cu);

  return (attr && DW_UNSND (attr));
}

static int
die_is_declaration (struct die_info *die, struct dwarf2_cu *cu)
{
  /* A DIE is a declaration if it has a DW_AT_declaration attribute
     which value is non-zero.  However, we have to be careful with
     DIEs having a DW_AT_specification attribute, because dwarf2_attr()
     (via dwarf2_flag_true_p) follows this attribute.  So we may
     end up accidently finding a declaration attribute that belongs
     to a different DIE referenced by the specification attribute,
     even though the given DIE does not have a declaration attribute.  */
  return (dwarf2_flag_true_p (die, DW_AT_declaration, cu)
	  && dwarf2_attr (die, DW_AT_specification, cu) == NULL);
}

/* Return the die giving the specification for DIE, if there is
   one.  */

static struct die_info *
die_specification (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *spec_attr = dwarf2_attr (die, DW_AT_specification, cu);

  if (spec_attr == NULL)
    return NULL;
  else
    return follow_die_ref (die, spec_attr, cu);
}

/* Free the line_header structure *LH, and any arrays and strings it
   refers to.  */
static void
free_line_header (struct line_header *lh)
{
  if (lh->standard_opcode_lengths)
    xfree (lh->standard_opcode_lengths);

  /* Remember that all the lh->file_names[i].name pointers are
     pointers into debug_line_buffer, and don't need to be freed.  */
  if (lh->file_names)
    xfree (lh->file_names);

  /* Similarly for the include directory names.  */
  if (lh->include_dirs)
    xfree (lh->include_dirs);

  xfree (lh);
}


/* Add an entry to LH's include directory table.  */
static void
add_include_dir (struct line_header *lh, char *include_dir)
{
  /* Grow the array if necessary.  */
  if (lh->include_dirs_size == 0)
    {
      lh->include_dirs_size = 1; /* for testing */
      lh->include_dirs = xmalloc (lh->include_dirs_size
                                  * sizeof (*lh->include_dirs));
    }
  else if (lh->num_include_dirs >= lh->include_dirs_size)
    {
      lh->include_dirs_size *= 2;
      lh->include_dirs = xrealloc (lh->include_dirs,
                                   (lh->include_dirs_size
                                    * sizeof (*lh->include_dirs)));
    }

  lh->include_dirs[lh->num_include_dirs++] = include_dir;
}
 

/* Add an entry to LH's file name table.  */
static void
add_file_name (struct line_header *lh,
               char *name,
               unsigned int dir_index,
               unsigned int mod_time,
               unsigned int length)
{
  struct file_entry *fe;

  /* Grow the array if necessary.  */
  if (lh->file_names_size == 0)
    {
      lh->file_names_size = 1; /* for testing */
      lh->file_names = xmalloc (lh->file_names_size
                                * sizeof (*lh->file_names));
    }
  else if (lh->num_file_names >= lh->file_names_size)
    {
      lh->file_names_size *= 2;
      lh->file_names = xrealloc (lh->file_names,
                                 (lh->file_names_size
                                  * sizeof (*lh->file_names)));
    }

  fe = &lh->file_names[lh->num_file_names++];
  fe->name = name;
  fe->dir_index = dir_index;
  fe->mod_time = mod_time;
  fe->length = length;
  fe->included_p = 0;
}
 

/* Read the statement program header starting at OFFSET in
   .debug_line, according to the endianness of ABFD.  Return a pointer
   to a struct line_header, allocated using xmalloc.

   NOTE: the strings in the include directory and file name tables of
   the returned object point into debug_line_buffer, and must not be
   freed.  */
static struct line_header *
dwarf_decode_line_header (unsigned int offset, bfd *abfd,
			  struct dwarf2_cu *cu)
{
  struct cleanup *back_to;
  struct line_header *lh;
  char *line_ptr;
  /* APPLE LOCAL avoid type warnings by making BYTES_READ unsigned.  */
  unsigned bytes_read;
  int i;
  char *cur_dir, *cur_file;

  if (dwarf2_per_objfile->line_buffer == NULL)
    {
      complaint (&symfile_complaints, _("missing .debug_line section"));
      return 0;
    }

  /* Make sure that at least there's room for the total_length field.
     That could be 12 bytes long, but we're just going to fudge that.  */
  if (offset + 4 >= dwarf2_per_objfile->line_size)
    {
      dwarf2_statement_list_fits_in_line_number_section_complaint ();
      return 0;
    }

  lh = xmalloc (sizeof (*lh));
  memset (lh, 0, sizeof (*lh));
  back_to = make_cleanup ((make_cleanup_ftype *) free_line_header,
                          (void *) lh);

  line_ptr = dwarf2_per_objfile->line_buffer + offset;

  /* Read in the header.  */
  /* APPLE LOCAL Add cast to avoid type mismatch in arg4 warning.  */
  lh->total_length = 
    read_initial_length (abfd, line_ptr, &cu->header, (int *) &bytes_read);
  line_ptr += bytes_read;
  if (line_ptr + lh->total_length > (dwarf2_per_objfile->line_buffer
				     + dwarf2_per_objfile->line_size))
    {
      dwarf2_statement_list_fits_in_line_number_section_complaint ();
      return 0;
    }
  lh->statement_program_end = line_ptr + lh->total_length;
  lh->version = read_2_bytes (abfd, line_ptr);
  line_ptr += 2;
  /* APPLE LOCAL Add cast to avoid type mismatch in arg4 warning.  */
  lh->header_length = read_offset (abfd, line_ptr, &cu->header, 
				   (int *) &bytes_read);
  line_ptr += bytes_read;
  lh->minimum_instruction_length = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->default_is_stmt = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->line_base = read_1_signed_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->line_range = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->opcode_base = read_1_byte (abfd, line_ptr);
  line_ptr += 1;
  lh->standard_opcode_lengths
    = (unsigned char *) xmalloc (lh->opcode_base * sizeof (unsigned char));

  lh->standard_opcode_lengths[0] = 1;  /* This should never be used anyway.  */
  for (i = 1; i < lh->opcode_base; ++i)
    {
      lh->standard_opcode_lengths[i] = read_1_byte (abfd, line_ptr);
      line_ptr += 1;
    }

  /* Read directory table.  */
  while ((cur_dir = read_string (abfd, line_ptr, &bytes_read)) != NULL)
    {
      line_ptr += bytes_read;
      add_include_dir (lh, cur_dir);
    }
  line_ptr += bytes_read;

  /* Read file name table.  */
  while ((cur_file = read_string (abfd, line_ptr, &bytes_read)) != NULL)
    {
      unsigned int dir_index, mod_time, length;

      line_ptr += bytes_read;
      dir_index = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      mod_time = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;
      length = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
      line_ptr += bytes_read;

      add_file_name (lh, cur_file, dir_index, mod_time, length);
    }
  line_ptr += bytes_read;
  lh->statement_program_start = line_ptr; 

  if (line_ptr > (dwarf2_per_objfile->line_buffer
		  + dwarf2_per_objfile->line_size))
    complaint (&symfile_complaints,
	       _("line number info header doesn't fit in `.debug_line' section"));

  discard_cleanups (back_to);
  return lh;
}

/* This function exists to work around a bug in certain compilers
   (particularly GCC 2.95), in which the first line number marker of a
   function does not show up until after the prologue, right before
   the second line number marker.  This function shifts ADDRESS down
   to the beginning of the function if necessary, and is called on
   addresses passed to record_line.  */

static CORE_ADDR
check_cu_functions (CORE_ADDR address, struct dwarf2_cu *cu)
{
  struct function_range *fn;

  /* Find the function_range containing address.  */
  if (!cu->first_fn)
    return address;

  if (!cu->cached_fn)
    cu->cached_fn = cu->first_fn;

  fn = cu->cached_fn;
  while (fn)
    if (fn->lowpc <= address && fn->highpc > address)
      goto found;
    else
      fn = fn->next;

  fn = cu->first_fn;
  while (fn && fn != cu->cached_fn)
    if (fn->lowpc <= address && fn->highpc > address)
      goto found;
    else
      fn = fn->next;

  return address;

 found:
  if (fn->seen_line)
    return address;
  if (address != fn->lowpc)
    complaint (&symfile_complaints,
	       _("misplaced first line number at 0x%lx for '%s'"),
	       (unsigned long) address, fn->name);
  fn->seen_line = 1;
  return fn->lowpc;
}

/* APPLE LOCAL begin subroutine inlining  */
/* Given a pc ADDRESS and FILE_INDEX, search through the objfile's
   inlined_call_sites data (a Red-Black tree), finding ALL inlined
   function calls that start at the given ADDRESS.  Remove each such
   call site from the data structure, writing the appropriate
   corresponding information into the appropriate file(s) line
   tables.  */

static void
check_inlined_function_calls (struct subfile *subfile, int file_index, int line,
			      CORE_ADDR address, struct line_header *lh,
			      struct dwarf2_cu *cu, char *comp_dir)
{
  struct dwarf_inlined_call_record *current = NULL;
  struct objfile *objfile = cu->objfile;
  struct subfile *tmp_subfile = NULL;
  struct subfile *call_site_subfile;
  struct subfile *decl_site_subfile;
  struct rb_tree_node *rb_node;
  int done = 0;
  const char *tmp_basename, *call_basename, *decl_basename;

  if (!objfile->inlined_call_sites)
    return;

  /* Loop until we've found ALL inlining instances for the current ADDRESS.
     Each time we find an instance, we remove it from objfile's
     inlined_call_sites.  */

  while (!done)
    {
      current = NULL;
      call_site_subfile = NULL;
      decl_site_subfile = NULL;
      tmp_basename = NULL;
      call_basename = NULL;
      decl_basename = NULL;

      /* Look for an inlining instance with the address and file_index passed
	 in.  */

      rb_node = rb_tree_find_and_remove_node (&(objfile->inlined_call_sites), 
					      objfile->inlined_call_sites, 
					      address, file_index);

      /* The secondary key in the rb_tree corresponds to the file index of
	 the call_site file.  It is possible that the subfile parameter
	 will not match either the call_site or the decl_site for the
	 inlined subroutine, in which case we will need to find (or
	 create) the correct subfile records for either or both of those
	 files.  */

      if (rb_node 
	  && rb_node->secondary_key == file_index)
	{
	  tmp_subfile = subfile;
	  call_site_subfile = subfile;
	  current = (struct dwarf_inlined_call_record *) rb_node->data;
	}
      else
	{
	  /* Look for an inlining instance with the same address but a
	     differnt file_index than was passed in.  */

	  if (!rb_node)
	    rb_node = rb_tree_find_and_remove_node 
	                                      (&(objfile->inlined_call_sites), 
					       objfile->inlined_call_sites, 
					       address, -1);

	  /* We found an inlining instance, but it has a different
	     file_index than was passed in.  We will have to igure out
	     what the file is, and make sure we have the appropriate
	     subfile for it.  */

	  if (rb_node)
	    current = (struct dwarf_inlined_call_record *) rb_node->data;
	}

      if (current
	  && ((current->file_index > lh->num_file_names)
	      || (current->decl_file_index > lh->num_file_names)))
	{
	  /* Something is seriously wrong: The dwarf record for the
	     inlining says the inlined function is declared in or
	     called from a file whose index did not make it into the
	     dwarf line table.  The best thing to do at this point is
	     to throw away the inlining record and hope for the
	     best.  */
	  current = NULL;
	}

      if (current)
	{
	  if (current->file_index == file_index)
	    call_site_subfile = subfile;
	  
	  if (current->decl_file_index == file_index)
	    decl_site_subfile = subfile;
	  
	  if (call_site_subfile == NULL)
            call_basename = lbasename 
                               (lh->file_names[current->file_index - 1].name);
	  
	  if (decl_site_subfile == NULL)
	    decl_basename = lbasename 
                          (lh->file_names[current->decl_file_index - 1].name);

	  /* Look for subfiles for call site and decl site files,
	     if necessary.  */

	  for (tmp_subfile = subfiles; 
	       tmp_subfile 
                 && (call_site_subfile == NULL || decl_site_subfile == NULL);
	       tmp_subfile = tmp_subfile->next)
	    {
	      tmp_basename = lbasename (tmp_subfile->name);

	      if (call_site_subfile == NULL 
                  && (strcmp (tmp_basename, call_basename) == 0))
		call_site_subfile = tmp_subfile;
	      
	      if (decl_site_subfile == NULL 
                  && (strcmp (tmp_basename, decl_basename) == 0))
		decl_site_subfile = tmp_subfile;
	    }
	}
      
      /* If the subfile record for either the call site file or the decl 
	 site file (or both) does not exist yet, we need to create the 
	 record(s).  */
      
      if (current && (call_site_subfile == NULL || decl_site_subfile == NULL))
	{
	  struct file_entry *fe;
	  char *dir;
	  
	  if (call_site_subfile == NULL)
	    {
	      fe = &lh->file_names[current->file_index - 1];
	      
	      if (fe->dir_index)
		dir = lh->include_dirs[fe->dir_index - 1];
	      else
		dir = comp_dir;
	      
	      /* Create the new subfile record.  */
	      
	      dwarf2_start_subfile (fe->name, dir, cu->comp_dir);
	    }
	  
	  if (decl_site_subfile == NULL)
	    {
	      fe = &lh->file_names[current->decl_file_index - 1];
	      if (fe->dir_index)
		dir = lh->include_dirs[fe->dir_index - 1];
	      else
		dir = comp_dir;
	      
	      /* Create the new subfile record.  */
	      
	      dwarf2_start_subfile (fe->name, dir, cu->comp_dir);
	    }
	  
	  /* Now we have to find the newly created record(s).  */
	  
	  for (tmp_subfile = subfiles; tmp_subfile; 
	       tmp_subfile = tmp_subfile->next)
	    {
	      tmp_basename = lbasename (tmp_subfile->name);
	      
	      if (call_site_subfile == NULL 
                  && (strcmp (tmp_basename, call_basename) == 0))
		call_site_subfile = tmp_subfile;
	      
	      if (decl_site_subfile == NULL 
                  && (strcmp (tmp_basename, decl_basename) == 0))
		decl_site_subfile = tmp_subfile;
	      
	      if (call_site_subfile && decl_site_subfile)
		break;
	    }
	  gdb_assert (call_site_subfile != NULL);
	  gdb_assert (decl_site_subfile != NULL);
	}

      /* Now we have the inlining instance and the files straight, write the
	 appropriate entries in the line tables.  */

      if (current)
	{
	  if (call_site_subfile != decl_site_subfile)
	    record_line (call_site_subfile, current->line, address, 0, 
			 NORMAL_LT_ENTRY);

	  if (dwarf2_debug_inlined_stepping)
	    fprintf_unfiltered (gdb_stdout, "%s inlined into %s:\n", 
				current->name, current->parent_name);
	  
	  record_line (call_site_subfile, current->line, current->lowpc, 
		       current->highpc,
		       INLINED_CALL_SITE_LT_ENTRY);
	  record_line (decl_site_subfile, current->decl_line + 1, 
		       current->lowpc, current->highpc,
		       INLINED_SUBROUTINE_LT_ENTRY);
	  /* APPLE LOCAL begin address ranges  */
	  inlined_function_add_function_names (objfile,
					       current->lowpc, 
					       current->highpc,
					       current->line, current->column,
					       current->name,
					       current->parent_name,
					       current->ranges,
					       /* APPLE LOCAL radar 6545149  */
					       current->func_sym);
	  /* APPLE LOCAL end address ranges  */
	}
      else
	/* No inlining entry was found, so stop looping.  */
	done = 1;
    }

  return;
}

/* APPLE LOCAL end subroutine inlining  */

/* APPLE LOCAL begin: debug map line table support. 

  Line tables have their addresses fixed up on the fly as object files are 
  read in. Function re-ordering and dead code stripping can seriously affect
  the DWARF line tables. This function will take care of properly mapping line
  table rows to what they should be in the internal gdb representation. If
  function re-ordering happens, we may have a single line table in the object
  file that looks something like:

  Address    File Line Function
  ---------- ---- ---- --------
  0x00000000    1   12 func_1
  0x00000014    1   13 func_1
  0x0000001c    1   14 func_1

  0x00000034    1   22 func_2
  0x00000050    1   23 func_2
  0x0000005c    1   24 func_2

  0x00000070    1   32 func_3
  0x00000090    1   33 func_3
  0x000000b0    1   34 func_3

  0x000000c8    1   42 func_4
  0x000000e4    1   43 func_4
  0x000000f0    1   44 func_4

  0x00000104    1   52 func_5
  0x00000120    1   53 func_5
  0x0000013c    1   54 func_5
  
  0x00000154    1   62 func_6
  0x00000178    1   64 func_6
  0x00000184    1   65 func_6
  0x0000019c    1   66 func_6
  0x000001dc    1   67 func_6
  0x000001f4    1   68 func_6
  0x00000200    1   69 func_6
  0x00000218    1   69 func_6   end_sequence

After function re-ordering and dead code stripping the map could look like:
lowpc        highpc	function
----------   ---------- -------
0x00001c3c - 0x00001d00 main
0x00001d00 - 0x00001d50 func_5
0x00001d50 - 0x00001d84 func_1

  When building the line table, we will try and translate each address. If the
  address is in the middle of a line table (isn't a end of sequence 
  (LNE_end_sequence)), we need to check each address to see if it is in mapped
  in the output both as a low and high pc address.
 */

static CORE_ADDR
translate_debug_map_address_with_tuple (struct oso_to_final_addr_map *map,
					struct oso_final_addr_tuple* match,
					CORE_ADDR oso_addr, int highpc)
{
  gdb_assert (map != NULL);
  gdb_assert (match != NULL && match->present_in_final);
  int delta = oso_addr - match->oso_low_addr;
  CORE_ADDR final_addr = match->final_addr + delta;

  /* For the high pc address, find the next final address and make sure it
     doesn't overlap into that next address range. If it does, trim the 
     current address range down. See comments in definition of the 
     'oso_to_final_addr_map' structure for full details.  */
  if (highpc && map->final_addr_index)
    {
      /* Create a search key and search for the final address index for
	 our match.  */
      struct final_addr_key final_addr_key = { match->final_addr, map };

      int *match_fa_idx_ptr = bsearch (&final_addr_key, 
				       map->final_addr_index, 
				       map->entries, sizeof (int),
				       compare_translation_final_addr);

      if (match_fa_idx_ptr != NULL
          && *match_fa_idx_ptr >= 0 
          && *match_fa_idx_ptr <= map->entries)
	{
	  /* We found a matching final address, now we can check the next
	     entry (if there is one) to see if we need to shrink the address
	     range for this function.  */
	  int *next_fa_idx_ptr = match_fa_idx_ptr + 1;
	  if (next_fa_idx_ptr - map->final_addr_index < map->entries 
	      && *next_fa_idx_ptr < map->entries)
	    {
	      if (map->tuples[*next_fa_idx_ptr].present_in_final)
		{
		  int new_delta = map->tuples[*next_fa_idx_ptr].final_addr - 
				  match->final_addr;
		  if (new_delta < delta)
		    {
		      /* The next final address was less than our current end
			 address, we need to shrink it to match the debug map
			 entry.  */
		      final_addr = match->final_addr + new_delta;
		      if (debug_debugmap > 5)
			fprintf_unfiltered (gdb_stdlog, 
					    "debugmap: decreasing end address"
					    " for '%s' in %s by %d bytes.\n",
					    match->name, map->pst->filename, 
					    delta - new_delta);
		    }
		}
	    }
	}
    }

  /*  FIXME: The code below is not quite right; if the thing being
      translated is not in the text section (e.g. it is in the data
      section) this capping stuff does the wrong thing.  ctice/2007-11-15

      Yeah I had some patches a while back that recorded the minsym's
      type (mst_text, etc) in the struct oso_final_addr_tuple.  I think
      that's necessary here.  molenda/2007-11-15 */

#if 0
  if (final_addr > map->pst->texthigh)
    {
      if (debug_debugmap > 5)
	fprintf_unfiltered (gdb_stdlog, 
			    "debugmap: address 0x%s beyond high text address "
			    "0x%s for %s, capping to 0x%s\n",
			    paddr_nz (final_addr), paddr_nz (map->pst->texthigh), 
			    match->name, paddr_nz (map->pst->texthigh));
      final_addr = map->pst->texthigh;
    }
#endif

  if (debug_debugmap)
    fprintf_unfiltered (gdb_stdlog, 
                        "debugmap: (highpc = %i) translated 0x%s to 0x%s "
			"for '%s' in %s\n", highpc, paddr (oso_addr), 
			paddr (final_addr), match->name, map->pst->filename);
  return final_addr;
}

/* 
  Properly records line table entries for executables with debug maps, or for
  executables with linked DWARF.
  
  There are many cases to watch out for when we have a debug map that can
  cause problems:
    - When function a b c were contiguous in the object file and:
      - a is dead stripped and b and c are in the linked image
      - b is dead stripped and a and c are in the linked image
      - c is dead srtipped and a and b are in the linked image
      - functions from other compile units get inserted between a b and c
        (order files)
      - function a b and c are reordered
	
  There are many variations, but the safest thing to do is to detect when we
  are translating ADDRESS that the same as OSO_HIGH_ADDR member of a
  tuple and take extra care when translating these entries. 
 */

static int
dwarf2_record_line (struct line_header *lh, char *comp_dir, struct dwarf2_cu *cu, 
		    CORE_ADDR address, CORE_ADDR baseaddr, int file, int line, 
		    int end_sequence)
{
  CORE_ADDR final_addr = (CORE_ADDR) -1;
  int record_final_addr = 0;
  int num_lines_recorded = 0;
  struct oso_to_final_addr_map *map = cu->addr_map;
  int fake_end_sequence = 0;
  if (map == NULL || map->tuples == NULL)
    {
      /* No debug map, just use the address we have.  */
      final_addr = address;
      record_final_addr = 1;
    }
  else
    {
      /* We have a debug map and we need to translate the address.  */
      const struct oso_final_addr_tuple *last_tuple = map->tuples + map->entries;
      struct oso_final_addr_tuple *match = NULL;
      if (map->entries == 1)
	{
	  /* Check the case where we just have one entry and only match
	     if our address is in the range.  */
	  if (compare_translation_tuples_inclusive (&address, &(map->tuples[0])) == 0)
	    match = map->tuples;
	}
      else
	{
	  /* Find a matching entry. Note that we can end up pickup up either
	     the correct entry for an normal or end address, but this can save
	     us from having to do two searches.  */
	  match = bsearch (&address, map->tuples, map->entries, 
			   sizeof (struct oso_final_addr_tuple), 
			   compare_translation_tuples_inclusive);
	}

      if (match != NULL)
	{
	  struct oso_final_addr_tuple *next = NULL;
	  /* Our binary search may kick up the next entry due to it including
	     both the start and end OSO address, so set MATCH to the previous 
	     entry if needed.  */
	  if (address == match->oso_low_addr)
	    {
	      if (match > map->tuples && match[-1].oso_high_addr == address)
		{
		  next = match;
		  match--;
		}
	    }

	  /* We now have MATCH that contains ADDRESS where ADDRESS may be 
	     equal to the end address in that range. We _might_ have NEXT
	     which is the next oso address range in our tuples, but it may
	     be NULL if MATCH is the last or only entry.  */
	  if (address < match->oso_high_addr)
	    {
	      /* ADDRESS is less than the OSO_HIGH_ADDR of MATCH, 
	         we have found the correct MATCH and don't need to do
		 anything special.  */
	      if (match->present_in_final)
		{
		  final_addr = translate_debug_map_address_with_tuple (map, 
							     	       match, 
							     	       address, 
    							           end_sequence);
		  record_final_addr = 1;
		}
	    }
	  else if (address == match->oso_high_addr)
	    {
	      /* ADDRESS matches the OSO_HIGH_ADDR of MATCH which means that
	         ADDRESS is at the highpc of a function (func_a), and possibly 
		 at the start address of another function (func_b). If this 
		 isn't an end_sequence line table row, we need to make sure the
		 func_a and func_b appear contiguously in the final executable. 
		 If they don't, we need to translate the address once as a high
		 PC for func_a, and again as a normal address for func_b and 
		 potentially add a line table entry for the end of func_a if 
		 they aren't contiguous in the final output.  */
	      if (next == NULL)
		{
		  /* Set NEXT correctly if it already isn't is there is a next
		     tuple.  */
		  if (match + 1 < last_tuple)
		    next = match + 1;
		}
	      
	      if (match->present_in_final)
		{
		  /* Translate ADDRESS as a highpc address for the function
		     described by MATCH (func_a).  */
		  final_addr = translate_debug_map_address_with_tuple (map, 
								       match, 
								       address, 
								       1);
		  record_final_addr = 1; 
		}

	      if (next)
		{
		  /* Make sure the next entry is in the linked executable.  */
		  if (next->present_in_final)
		    {
		      /* NEXT is in the linked executable, so translate this
			 address as normal address and store the result 
			 in NEXT_FINAL_ADDR.  */
		      CORE_ADDR next_final_addr;
		      next_final_addr = translate_debug_map_address_with_tuple (
							map, next, address, 0);
		      /* Check if the two functions are still congituous in the
		         final executable?  */
		      if (final_addr != next_final_addr)
			{
			  /* They are NOT contiguous, or MATCH was not in
			     the linked exectuable.  */
			  if (match->present_in_final)
			    {
			      /* We need to fabricate a fake end sequence since
			         both MATCH and NEXT are in the final executable
				 but are not contiguous. We already translated
				 ADDRESS as a highpc address above so we need 
				 to add this address to our line table.  */
			      final_addr += baseaddr;
			      final_addr = check_cu_functions (final_addr, cu);
			      if (debug_debugmap > 7)
				fprintf_unfiltered (gdb_stdlog, 
 "dwarf2_record_line: record_line (%s:%i, 0x%s) for %s (create end sequence)\n",
						current_subfile->name, 0, 
						paddr_nz (final_addr), 
						map->pst->filename);
			      record_line (current_subfile, 0, final_addr, 0, 
					   NORMAL_LT_ENTRY);
							
			      num_lines_recorded++;
			    }
			    
			  /* Change our current line's address to the next
			     entry's final address.  */
			  final_addr = next_final_addr;
			  record_final_addr = 1;
			}
		    }
		  else if (match->present_in_final)
		    {
		      if (end_sequence == 0)
			{
			  /* MATCH is in the final executable and NEXT is NOT
			     and we are in the middle of a line table, so make
			     this line table entry a end sequence so we can cap
			     off the current line entry correctly so it doesn't
			     end up with a larger address range that it actually
			     has.  */
			  fake_end_sequence = 1;
			  if (debug_debugmap > 7)
			    fprintf_unfiltered (gdb_stdlog, 
                   "dwarf2_record_line: turning row (0x%s) into end sequence.\n",
						paddr_nz (final_addr));
			}
		    }
		}
	    }
	}
      else
	{
	  /* Address doesn't exist in tuples.  */
	  if (debug_debugmap > 7)
	    fprintf_unfiltered (gdb_stdlog, 
                       "dwarf2_record_line: translate failed for 0x%s in %s\n",
				paddr_nz (final_addr), map->pst->filename);
	}
    }

  if (record_final_addr)
    {
      /* Now record our line table entry by adding BASEADDR and doing some 
	 other final checks.  */
      final_addr += baseaddr;
      final_addr = check_cu_functions (final_addr, cu);
      /* Set our FINAL_LINE correctly (zero means the end address range for
         the previous line table entry).  */
      int final_line = (end_sequence || fake_end_sequence) ? 0 : line;
      
      if (debug_debugmap > 7)
	fprintf_unfiltered (gdb_stdlog, 
                      "dwarf2_record_line: record_line (%s:%i, 0x%s) for %s\n",
			    current_subfile->name, final_line, 
			    paddr_nz (final_addr), map->pst->filename);

      record_line (current_subfile, final_line, final_addr, 0, NORMAL_LT_ENTRY);
      /* APPLE LOCAL begin subroutine inlining  */
      if (end_sequence == 0)
	{
	  /* Only generate the inlined function calls when we are in the middle
	     of a line table (not at a real end_sequence) otherwise we may try
	     and generate the inline function calls using the wrong subfile.  */
	  check_inlined_function_calls (current_subfile, file, final_line, 
					final_addr, lh, cu, comp_dir);
	}
      /* APPLE LOCAL end subroutine inlining  */
      num_lines_recorded++;
    }
  return num_lines_recorded;
}

/* APPLE LOCAL end: debug map line table support.  */



/* Decode the Line Number Program (LNP) for the given line_header
   structure and CU.  The actual information extracted and the type
   of structures created from the LNP depends on the value of PST.

   1. If PST is NULL, then this procedure uses the data from the program
      to create all necessary symbol tables, and their linetables.
      The compilation directory of the file is passed in COMP_DIR,
      and must not be NULL.
   
   2. If PST is not NULL, this procedure reads the program to determine
      the list of files included by the unit represented by PST, and
      builds all the associated partial symbol tables.  In this case,
      the value of COMP_DIR is ignored, and can thus be NULL (the COMP_DIR
      is not used to compute the full name of the symtab, and therefore
      omitting it when building the partial symtab does not introduce
      the potential for inconsistency - a partial symtab and its associated
      symbtab having a different fullname -).  */

static void
dwarf_decode_lines (struct line_header *lh, char *comp_dir, bfd *abfd,
		    struct dwarf2_cu *cu, struct partial_symtab *pst)
{
  char *line_ptr;
  char *line_end;
  unsigned int bytes_read;
  unsigned char op_code, extended_op, adj_opcode;
  CORE_ADDR baseaddr;
  struct objfile *objfile = cu->objfile;
  const int decode_for_pst_p = (pst != NULL);

  baseaddr = objfile_text_section_offset (objfile);

  line_ptr = lh->statement_program_start;
  line_end = lh->statement_program_end;

  /* Read the statement sequences until there's nothing left.  */
  while (line_ptr < line_end)
    {
      /* state machine registers  */
      CORE_ADDR address = 0;
      unsigned int file = 1;
      unsigned int line = 1;
      unsigned int column = 0;
      int is_stmt = lh->default_is_stmt;
      int basic_block = 0;
      int end_sequence = 0;

      if (!decode_for_pst_p && lh->num_file_names >= file)
	{
          /* Start a subfile for the current file of the state machine.  */
	  /* lh->include_dirs and lh->file_names are 0-based, but the
	     directory and file name numbers in the statement program
	     are 1-based.  */
          struct file_entry *fe = &lh->file_names[file - 1];
          char *dir;

          if (fe->dir_index)
            dir = lh->include_dirs[fe->dir_index - 1];
          else
            dir = comp_dir;
          /* APPLE LOCAL: Pass in the compilation directory of this CU.  */
	  dwarf2_start_subfile (fe->name, dir, cu->comp_dir);
	}

      /* Decode the table.  */
      while (!end_sequence)
	{
	  /* APPLE LOCAL: Check for missing DW_LNE_end_sequence
	     at the end of the line table. */
	  if (line_ptr >= line_end)
	    {
	      complaint (&symfile_complaints,
			 _("Missing end sequence in DWARF2 line table."));
	      op_code = DW_LNS_extended_op;
	    }
	  else
	    {
	      op_code = read_1_byte (abfd, line_ptr);
	      line_ptr += 1;
	    }

	  if (op_code >= lh->opcode_base)
	    {		
	      /* Special operand.  */
	      adj_opcode = op_code - lh->opcode_base;
	      address += (adj_opcode / lh->line_range)
		* lh->minimum_instruction_length;
	      line += lh->line_base + (adj_opcode % lh->line_range);
              lh->file_names[file - 1].included_p = 1;
              if (!decode_for_pst_p)
		dwarf2_record_line (lh, comp_dir, cu, address, baseaddr, file, 
				    line, end_sequence);
	      basic_block = 1;
	    }
	  else switch (op_code)
	    {
	    case DW_LNS_extended_op:
	      /* APPLE LOCAL: Check for missing DW_LNE_end_sequence at 
	         the end of the line table. */
	      if (line_ptr >= line_end)
		{
		  extended_op = DW_LNE_end_sequence;
		}
	      else
		{
		  read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		  line_ptr += bytes_read;
		  extended_op = read_1_byte (abfd, line_ptr);
		  line_ptr += 1;
		}

	      switch (extended_op)
		{
		case DW_LNE_end_sequence:
		  end_sequence = 1;
                  lh->file_names[file - 1].included_p = 1;
		  if (!decode_for_pst_p)
		    dwarf2_record_line (lh, comp_dir, cu, address, baseaddr, 
					file, line, end_sequence);
		  break;
		case DW_LNE_set_address:
                  /* APPLE LOCAL Add cast to avoid type mismatch in arg4 warn.*/
		  address = read_address (abfd, line_ptr, cu, 
					  (int *) &bytes_read);
		  line_ptr += bytes_read;
		  break;
		case DW_LNE_define_file:
                  {
                    char *cur_file;
                    unsigned int dir_index, mod_time, length;
                    
                    cur_file = read_string (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    dir_index =
                      read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    mod_time =
                      read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    length =
                      read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                    line_ptr += bytes_read;
                    add_file_name (lh, cur_file, dir_index, mod_time, length);
                  }
		  break;
		default:
		  complaint (&symfile_complaints,
			     _("mangled .debug_line section"));
		  return;
		}
	      break;
	    case DW_LNS_copy:
              lh->file_names[file - 1].included_p = 1;
              if (!decode_for_pst_p)
		dwarf2_record_line (lh, comp_dir, cu, address, baseaddr, file, 
				    line, end_sequence);
	      basic_block = 0;
	      break;
	    case DW_LNS_advance_pc:
	      address += lh->minimum_instruction_length
		* read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_advance_line:
	      line += read_signed_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_set_file:
              {
                /* The arrays lh->include_dirs and lh->file_names are
                   0-based, but the directory and file name numbers in
                   the statement program are 1-based.  */
                struct file_entry *fe;
                char *dir;

                file = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
                line_ptr += bytes_read;
                fe = &lh->file_names[file - 1];
                if (fe->dir_index)
                  dir = lh->include_dirs[fe->dir_index - 1];
                else
                  dir = comp_dir;
                /* APPLE LOCAL: Pass in the compilation dir of this CU.  */
                if (!decode_for_pst_p)
                  dwarf2_start_subfile (fe->name, dir, cu->comp_dir);
              }
	      break;
	    case DW_LNS_set_column:
	      column = read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
	      line_ptr += bytes_read;
	      break;
	    case DW_LNS_negate_stmt:
	      is_stmt = (!is_stmt);
	      break;
	    case DW_LNS_set_basic_block:
	      basic_block = 1;
	      break;
	    /* Add to the address register of the state machine the
	       address increment value corresponding to special opcode
	       255.  I.e., this value is scaled by the minimum
	       instruction length since special opcode 255 would have
	       scaled the the increment.  */
	    case DW_LNS_const_add_pc:
	      address += (lh->minimum_instruction_length
			  * ((255 - lh->opcode_base) / lh->line_range));
	      break;
	    case DW_LNS_fixed_advance_pc:
	      address += read_2_bytes (abfd, line_ptr);
	      line_ptr += 2;
	      break;
	    default:
	      {
		/* Unknown standard opcode, ignore it.  */
		int i;

		for (i = 0; i < lh->standard_opcode_lengths[op_code]; i++)
		  {
		    (void) read_unsigned_leb128 (abfd, line_ptr, &bytes_read);
		    line_ptr += bytes_read;
		  }
	      }
	    }
	}
    }

  if (decode_for_pst_p)
    {
      int file_index;

      /* Now that we're done scanning the Line Header Program, we can
         create the psymtab of each included file.  */
      for (file_index = 0; file_index < lh->num_file_names; file_index++)
        if (lh->file_names[file_index].included_p == 1)
          {
            const struct file_entry fe = lh->file_names [file_index];
            char *include_name = fe.name;
            char *dir_name = NULL;
            char *pst_filename = pst->filename;

            if (fe.dir_index)
              dir_name = lh->include_dirs[fe.dir_index - 1];

            if (!IS_ABSOLUTE_PATH (include_name) && dir_name != NULL)
              {
                include_name = concat (dir_name, SLASH_STRING,
				       include_name, (char *)NULL);
                make_cleanup (xfree, include_name);
              }

            /* APPLE LOCAL: Re-check include_name to make sure it is absolute 
               before making the psymtab filename absolute since "dir_name" 
               can be a relative path.  */
            if (IS_ABSOLUTE_PATH (include_name) 
                && !IS_ABSOLUTE_PATH (pst_filename) 
                && pst->dirname != NULL)
              {
                pst_filename = concat (pst->dirname, SLASH_STRING,
				       pst_filename, (char *)NULL);
                make_cleanup (xfree, pst_filename);
              }

            if (strcmp (include_name, pst_filename) != 0)
              dwarf2_create_include_psymtab (include_name, pst, objfile);
          }
    }
}

/* Start a subfile for DWARF.  FILENAME is the name of the file and
   DIRNAME the name of the source directory which contains FILENAME
   or NULL if not known.
   This routine tries to keep line numbers from identical absolute and
   relative file names in a common subfile.

   Using the `list' example from the GDB testsuite, which resides in
   /srcdir and compiling it with Irix6.2 cc in /compdir using a filename
   of /srcdir/list0.c yields the following debugging information for list0.c:

   DW_AT_name:          /srcdir/list0.c
   DW_AT_comp_dir:              /compdir
   files.files[0].name: list0.h
   files.files[0].dir:  /srcdir
   files.files[1].name: list0.c
   files.files[1].dir:  /srcdir

   The line number information for list0.c has to end up in a single
   subfile, so that `break /srcdir/list0.c:1' works as expected.  */

   /* APPLE LOCAL: Given the input of
             FILENAME foo.h
             DIRNAME  ../../../Xlib
             COMP_DIR /users/jason/sources/testprog
      Pass foo.h as the "filename" argument to start_subfile and
      "/users/jason/sources/testprog/../../../Xlib" as the dirname.  */

static void
dwarf2_start_subfile (char *filename, char *dirname, char *comp_dir)
{
  struct cleanup *clean = make_cleanup (null_cleanup, 0);

  /* If the filename isn't absolute, try to match an existing subfile
     with the full pathname.  */

  if (!IS_ABSOLUTE_PATH (filename) && dirname != NULL)
    {
      struct subfile *subfile;
      char *fullname = concat (dirname, "/", filename, (char *)NULL);

      for (subfile = subfiles; subfile; subfile = subfile->next)
	{
	  if (FILENAME_CMP (subfile->name, fullname) == 0)
	    {
	      current_subfile = subfile;
	      xfree (fullname);
	      return;
	    }
	}
      xfree (fullname);
    }

  /* APPLE LOCAL: If FILENAME isn't an absolute path and DIRNAME is either
     NULL or a relative path, prepend COMP_DIR on there to get an absolute
     path.  */

  if (!IS_ABSOLUTE_PATH (filename) && dirname == NULL)
    dirname = comp_dir;
  else 
    if (!IS_ABSOLUTE_PATH (filename) && !IS_ABSOLUTE_PATH (dirname))
      {
        dirname = concat (comp_dir, SLASH_STRING, dirname, (char *) NULL);
        make_cleanup (xfree, dirname);
      }
  start_subfile (filename, dirname);
  do_cleanups (clean);
}

static void
var_decode_location (struct attribute *attr, struct symbol *sym,
		     struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;

  /* NOTE drow/2003-01-30: There used to be a comment and some special
     code here to turn a symbol with DW_AT_external and a
     SYMBOL_VALUE_ADDRESS of 0 into a LOC_UNRESOLVED symbol.  This was
     necessary for platforms (maybe Alpha, certainly PowerPC GNU/Linux
     with some versions of binutils) where shared libraries could have
     relocations against symbols in their debug information - the
     minimal symbol would have the right address, but the debug info
     would not.  It's no longer necessary, because we will explicitly
     apply relocations when we read in the debug information now.  */

  /* A DW_AT_location attribute with no contents indicates that a
     variable has been optimized away.  */
  if (attr_form_is_block (attr) && DW_BLOCK (attr)->size == 0)
    {
      SYMBOL_CLASS (sym) = LOC_OPTIMIZED_OUT;
      return;
    }

  /* Handle one degenerate form of location expression specially, to
     preserve GDB's previous behavior when section offsets are
     specified.  If this is just a DW_OP_addr then mark this symbol
     as LOC_STATIC.  */

  if (attr_form_is_block (attr)
      && DW_BLOCK (attr)->size == 1 + cu_header->addr_size
      && DW_BLOCK (attr)->data[0] == DW_OP_addr)
    {
      int dummy;
      CORE_ADDR symaddr = read_address (objfile->obfd, 
                                        DW_BLOCK (attr)->data + 1, cu, &dummy);
      /* APPLE LOCAL: debug map */
      if (symaddr == 0)
        {
          CORE_ADDR actualaddr;
          if (translate_common_symbol_debug_map_address
                                 (cu->addr_map, SYMBOL_LINKAGE_NAME (sym), &actualaddr))
              symaddr = actualaddr;
          SYMBOL_VALUE_ADDRESS (sym) = symaddr;
        }
      else
        {
          CORE_ADDR addr;
          if (translate_debug_map_address (cu->addr_map, symaddr, &addr, 0))
            SYMBOL_VALUE_ADDRESS (sym) = addr;
          else
            {
              SYMBOL_CLASS (sym) = LOC_OPTIMIZED_OUT;
              return;
            }
        }
      fixup_symbol_section (sym, objfile);
      /* Offset using the main executable's section offsets.  */
      SYMBOL_VALUE_ADDRESS (sym) += objfile_section_offset (objfile, 
						SYMBOL_SECTION (sym));
      SYMBOL_CLASS (sym) = LOC_STATIC;
      return;
    }

  /* NOTE drow/2002-01-30: It might be worthwhile to have a static
     expression evaluator, and use LOC_COMPUTED only when necessary
     (i.e. when the value of a register or memory location is
     referenced, or a thread-local block, etc.).  Then again, it might
     not be worthwhile.  I'm assuming that it isn't unless performance
     or memory numbers show me otherwise.  */

  dwarf2_symbol_mark_computed (attr, sym, cu);
  SYMBOL_CLASS (sym) = LOC_COMPUTED;
}

/* Given a pointer to a DWARF information entry, figure out if we need
   to make a symbol table entry for it, and if so, create a new entry
   and return a pointer to it.
   If TYPE is NULL, determine symbol type from the die, otherwise
   used the passed type.  */

static struct symbol *
new_symbol (struct die_info *die, struct type *type, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct symbol *sym = NULL;
  char *name;
  struct attribute *attr = NULL;
  struct attribute *attr2 = NULL;
  CORE_ADDR baseaddr;

  baseaddr = objfile_text_section_offset (objfile);

  if (die->tag != DW_TAG_namespace)
    name = dwarf2_linkage_name (die, cu);
  else
    name = TYPE_NAME (type);

  if (name)
    {
      sym = (struct symbol *) obstack_alloc (&objfile->objfile_obstack,
					     sizeof (struct symbol));
      OBJSTAT (objfile, n_syms++);
      memset (sym, 0, sizeof (struct symbol));

      /* Cache this symbol's name and the name's demangled form (if any).  */
      SYMBOL_LANGUAGE (sym) = cu->language;
      SYMBOL_SET_NAMES (sym, name, strlen (name), objfile);

      /* Default assumptions.
         Use the passed type or decode it from the die.  */
      SYMBOL_DOMAIN (sym) = VAR_DOMAIN;
      SYMBOL_CLASS (sym) = LOC_STATIC;
      if (type != NULL)
	SYMBOL_TYPE (sym) = type;
      else
	SYMBOL_TYPE (sym) = die_type (die, cu);
      attr = dwarf2_attr (die, DW_AT_decl_line, cu);
      if (attr)
	{
	  SYMBOL_LINE (sym) = DW_UNSND (attr);
	}
      switch (die->tag)
	{
	case DW_TAG_label:
	  attr = dwarf2_attr (die, DW_AT_low_pc, cu);
	  if (attr)
	    {
              /* APPLE LOCAL: debug map */
              CORE_ADDR addr;
              if (translate_debug_map_address (cu->addr_map, DW_ADDR (attr), &addr, 0))
	        SYMBOL_VALUE_ADDRESS (sym) = addr + baseaddr;
              else
                return NULL; /* NB: Leaking a struct symbol, sigh. */
	    }
	  SYMBOL_CLASS (sym) = LOC_LABEL;
	  break;
	  /* APPLE LOCAL begin subroutine inlining  */
	case DW_TAG_inlined_subroutine:
	  /* APPLE LOCAL end subroutine inlining  */
	case DW_TAG_subprogram:
	  /* SYMBOL_BLOCK_VALUE (sym) will be filled in later by
	     finish_block.  */
	  SYMBOL_CLASS (sym) = LOC_BLOCK;
	  attr2 = dwarf2_attr (die, DW_AT_external, cu);
	  /* APPLE LOCAL begin inlined function symbols & blocks  */
	  if ((attr2 && (DW_UNSND (attr2) != 0))
	      || (die->tag == DW_TAG_inlined_subroutine))
	  /* APPLE LOCAL end inlined function symbols & blocks  */
	    {
	      add_symbol_to_list (sym, &global_symbols);
	    }
	  else
	    {
	      add_symbol_to_list (sym, cu->list_in_scope);
	    }
	  break;
	case DW_TAG_variable:
	  /* Compilation with minimal debug info may result in variables
	     with missing type entries. Change the misleading `void' type
	     to something sensible.  */
	  if (TYPE_CODE (SYMBOL_TYPE (sym)) == TYPE_CODE_VOID)
	    SYMBOL_TYPE (sym) = init_type (TYPE_CODE_INT,
					   TARGET_INT_BIT / HOST_CHAR_BIT, 0,
					   "<variable, no debug info>",
					   objfile);
	  attr = dwarf2_attr (die, DW_AT_const_value, cu);
	  if (attr)
	    {
	      dwarf2_const_value (attr, sym, cu);
	      attr2 = dwarf2_attr (die, DW_AT_external, cu);
	      if (attr2 && (DW_UNSND (attr2) != 0))
		add_symbol_to_list (sym, &global_symbols);
	      else
		add_symbol_to_list (sym, cu->list_in_scope);
	      break;
	    }
	  attr = dwarf2_attr (die, DW_AT_location, cu);
	  if (attr)
	    {
	      var_decode_location (attr, sym, cu);
	      attr2 = dwarf2_attr (die, DW_AT_external, cu);
	      if (attr2 && (DW_UNSND (attr2) != 0))
		add_symbol_to_list (sym, &global_symbols);
	      else
		add_symbol_to_list (sym, cu->list_in_scope);
	    }
	  else
	    {
	      /* We do not know the address of this symbol.
	         If it is an external symbol and we have type information
	         for it, enter the symbol as a LOC_UNRESOLVED symbol.
	         The address of the variable will then be determined from
	         the minimal symbol table whenever the variable is
	         referenced.  */
	      attr2 = dwarf2_attr (die, DW_AT_external, cu);
	      if (attr2 && (DW_UNSND (attr2) != 0)
		  && dwarf2_attr (die, DW_AT_type, cu) != NULL)
		{
		  SYMBOL_CLASS (sym) = LOC_UNRESOLVED;
		  add_symbol_to_list (sym, &global_symbols);
		}
	      /* APPLE LOCAL begin variable opt states.  */
	      else
		{
		  if (!dwarf2_attr (die, DW_AT_declaration, cu))
		    {
		      SYMBOL_CLASS (sym) = LOC_OPTIMIZED_OUT;
		      add_symbol_to_list (sym, cu->list_in_scope);
		    }
		}
	      /* APPLE LOCAL end variable opt states.  */
	    }
	  break;
	case DW_TAG_formal_parameter:
	  attr = dwarf2_attr (die, DW_AT_location, cu);
	  if (attr)
	    {
	      var_decode_location (attr, sym, cu);
	      /* FIXME drow/2003-07-31: Is LOC_COMPUTED_ARG necessary?  */
	      if (SYMBOL_CLASS (sym) == LOC_COMPUTED)
		SYMBOL_CLASS (sym) = LOC_COMPUTED_ARG;
	    }
	  else if (die->parent->tag == DW_TAG_inlined_subroutine)
	    {
	      struct attribute *abs_orig_attr;
	      struct die_info *abs_orig;
	      abs_orig_attr = dwarf2_attr (die, DW_AT_abstract_origin, cu);
	      if (abs_orig_attr)
		{
		  abs_orig = follow_die_ref (die, abs_orig_attr, cu);
		  attr = dwarf2_attr (abs_orig, DW_AT_location, cu);
		  if (attr)
		    {
		      var_decode_location (attr, sym, cu);
		      if (SYMBOL_CLASS (sym) == LOC_COMPUTED)
			SYMBOL_CLASS (sym) = LOC_COMPUTED_ARG;
		    }
		}
	    }
	  attr = dwarf2_attr (die, DW_AT_const_value, cu);
	  if (attr)
	    {
	      dwarf2_const_value (attr, sym, cu);
	    }
	  /* FIXME:  This assert should be true, but gcc isn't always giving
	     DW_AT_location for parameters of inlined subroutines yet.  
	     gdb_assert (SYMBOL_CLASS (sym) != LOC_STATIC);
	  */
	  add_symbol_to_list (sym, cu->list_in_scope);
	  break;
	case DW_TAG_unspecified_parameters:
	  /* From varargs functions; gdb doesn't seem to have any
	     interest in this information, so just ignore it for now.
	     (FIXME?) */
	  break;
	case DW_TAG_class_type:
	case DW_TAG_structure_type:
	case DW_TAG_union_type:
	case DW_TAG_set_type:
	case DW_TAG_enumeration_type:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_DOMAIN (sym) = STRUCT_DOMAIN;

	  /* Make sure that the symbol includes appropriate enclosing
	     classes/namespaces in its name.  These are calculated in
	     read_structure_type, and the correct name is saved in
	     the type.  */

	  if (cu->language == language_cplus
	      || cu->language == language_objcplus
	      || cu->language == language_java)
	    {
	      struct type *type = SYMBOL_TYPE (sym);
	      
	      if (TYPE_TAG_NAME (type) != NULL)
		{
		  /* FIXME: carlton/2003-11-10: Should this use
		     SYMBOL_SET_NAMES instead?  (The same problem also
		     arises further down in this function.)  */
		  /* The type's name is already allocated along with
		     this objfile, so we don't need to duplicate it
		     for the symbol.  */
		  SYMBOL_LINKAGE_NAME (sym) = TYPE_TAG_NAME (type);
		}
	    }

	  {
	    /* NOTE: carlton/2003-11-10: C++ and Java class symbols shouldn't
	       really ever be static objects: otherwise, if you try
	       to, say, break of a class's method and you're in a file
	       which doesn't mention that class, it won't work unless
	       the check for all static symbols in lookup_symbol_aux
	       saves you.  See the OtherFileClass tests in
	       gdb.c++/namespace.exp.  */

	    struct pending **list_to_add;

            /* APPLE LOCAL: Put type symbols in the global namespace for all
               languages.  For languages like C, types have a file-static
               scope but within gdb it doesn't buy us anything to scope their
               visibility the same way.  If we need a definition of a type in
               a compile unit where it isn't defined, we're going to have to
               find that definition in another compile unit - so we might as 
               well mark these as global and save ourselves a round of failing
               lookups.  */
	    list_to_add = (cu->list_in_scope == &file_symbols
			   ? &global_symbols : cu->list_in_scope);
	  
	    add_symbol_to_list (sym, list_to_add);

	    /* The semantics of C++ state that "struct foo { ... }" also
	       defines a typedef for "foo".  A Java class declaration also
	       defines a typedef for the class.  Synthesize a typedef symbol
	       so that "ptype foo" works as expected.  Objective C classes
	       can also benefit from this, but we must make sure to only add
	       ones that are classes and not ones that are just plain 
	       structures. We currently rely on the fact that an objective
	       C class will have C++ like attributes.  */
	    if (cu->language == language_cplus
		|| cu->language == language_java
		|| ((cu->language == language_objc || 
		    cu->language == language_objcplus) &&
		    HAVE_CPLUS_STRUCT (SYMBOL_TYPE (sym))))
	      {
		struct symbol *typedef_sym = (struct symbol *)
		  obstack_alloc (&objfile->objfile_obstack,
				 sizeof (struct symbol));
		*typedef_sym = *sym;
		SYMBOL_DOMAIN (typedef_sym) = VAR_DOMAIN;
		/* The symbol's name is already allocated along with
		   this objfile, so we don't need to duplicate it for
		   the type.  */
		if (TYPE_NAME (SYMBOL_TYPE (sym)) == 0)
		  TYPE_NAME (SYMBOL_TYPE (sym)) = SYMBOL_SEARCH_NAME (sym);
		add_symbol_to_list (typedef_sym, list_to_add);
	      }
	  }
	  break;
	case DW_TAG_typedef:
	  if (processing_has_namespace_info
	      && processing_current_prefix[0] != '\0')
	    {
	      SYMBOL_LINKAGE_NAME (sym) = typename_concat (&objfile->objfile_obstack,
							   processing_current_prefix,
							   name, cu);
	    }
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_DOMAIN (sym) = VAR_DOMAIN;
          /* APPLE LOCAL: Put typedefs at the global scope for easier 
             searching.  */
	  add_symbol_to_list (sym, &global_symbols);
	  break;
	case DW_TAG_base_type:
        case DW_TAG_subrange_type:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  SYMBOL_DOMAIN (sym) = VAR_DOMAIN;
          /* APPLE LOCAL: Put base types at the global scope for easier 
             searching.  */
	  add_symbol_to_list (sym, &global_symbols);
	  break;
	case DW_TAG_enumerator:
	  if (processing_has_namespace_info
	      && processing_current_prefix[0] != '\0')
	    {
	      SYMBOL_LINKAGE_NAME (sym) = typename_concat (&objfile->objfile_obstack,
							   processing_current_prefix,
							   name, cu);
	    }
	  attr = dwarf2_attr (die, DW_AT_const_value, cu);
	  if (attr)
	    {
	      dwarf2_const_value (attr, sym, cu);
	    }
	  {
	    /* NOTE: carlton/2003-11-10: See comment above in the
	       DW_TAG_class_type, etc. block.  */

	    struct pending **list_to_add;

	    list_to_add = (cu->list_in_scope == &file_symbols
			   && (cu->language == language_cplus
			       || cu->language == language_java)
			   ? &global_symbols : cu->list_in_scope);
	  
	    add_symbol_to_list (sym, list_to_add);
	  }
	  break;
	case DW_TAG_namespace:
	  SYMBOL_CLASS (sym) = LOC_TYPEDEF;
	  add_symbol_to_list (sym, &global_symbols);
	  break;
	default:
	  /* Not a tag we recognize.  Hopefully we aren't processing
	     trash data, but since we must specifically ignore things
	     we don't recognize, there is nothing else we should do at
	     this point. */
	  complaint (&symfile_complaints, _("unsupported tag: '%s'"),
		     dwarf_tag_name (die->tag));
	  break;
	}
    }
  return (sym);
}

/* Copy constant value from an attribute to a symbol.  */

static void
dwarf2_const_value (struct attribute *attr, struct symbol *sym,
		    struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  struct comp_unit_head *cu_header = &cu->header;
  struct dwarf_block *blk;

  switch (attr->form)
    {
    case DW_FORM_addr:
      if (TYPE_LENGTH (SYMBOL_TYPE (sym)) != cu_header->addr_size)
	dwarf2_const_value_length_mismatch_complaint (DEPRECATED_SYMBOL_NAME (sym),
						      cu_header->addr_size,
						      TYPE_LENGTH (SYMBOL_TYPE
								   (sym)));
      SYMBOL_VALUE_BYTES (sym) = (char *)
	obstack_alloc (&objfile->objfile_obstack, cu_header->addr_size);
      /* NOTE: cagney/2003-05-09: In-lined store_address call with
         it's body - store_unsigned_integer.  */
      /* APPLE LOCAL Add cast to avoid type mismatch in arg1 warning.  */
      /* APPLE LOCAL: debug map */
      {
        CORE_ADDR addr;
        if (translate_debug_map_address (cu->addr_map, DW_ADDR (attr), &addr, 0))
          store_unsigned_integer ((gdb_byte *) SYMBOL_VALUE_BYTES (sym), 
			          cu_header->addr_size, addr);
        else
          {
            SYMBOL_VALUE (sym) = 0;
            SYMBOL_CLASS (sym) = LOC_CONST;
            break;
          }
      }
      SYMBOL_CLASS (sym) = LOC_CONST_BYTES;
      break;
    case DW_FORM_block1:
    case DW_FORM_block2:
    case DW_FORM_block4:
    case DW_FORM_block:
      blk = DW_BLOCK (attr);
      if (TYPE_LENGTH (SYMBOL_TYPE (sym)) != blk->size)
	dwarf2_const_value_length_mismatch_complaint (DEPRECATED_SYMBOL_NAME (sym),
						      blk->size,
						      TYPE_LENGTH (SYMBOL_TYPE
								   (sym)));
      SYMBOL_VALUE_BYTES (sym) = (char *)
	obstack_alloc (&objfile->objfile_obstack, blk->size);
      memcpy (SYMBOL_VALUE_BYTES (sym), blk->data, blk->size);
      SYMBOL_CLASS (sym) = LOC_CONST_BYTES;
      break;

      /* The DW_AT_const_value attributes are supposed to carry the
	 symbol's value "represented as it would be on the target
	 architecture."  By the time we get here, it's already been
	 converted to host endianness, so we just need to sign- or
	 zero-extend it as appropriate.  */
    case DW_FORM_data1:
      dwarf2_const_value_data (attr, sym, 8);
      break;
    case DW_FORM_data2:
      dwarf2_const_value_data (attr, sym, 16);
      break;
    case DW_FORM_data4:
      dwarf2_const_value_data (attr, sym, 32);
      break;
    case DW_FORM_data8:
      dwarf2_const_value_data (attr, sym, 64);
      break;

    case DW_FORM_sdata:
      SYMBOL_VALUE (sym) = DW_SND (attr);
      SYMBOL_CLASS (sym) = LOC_CONST;
      break;

    case DW_FORM_APPLE_db_str:
    case DW_FORM_udata:
      SYMBOL_VALUE (sym) = DW_UNSND (attr);
      SYMBOL_CLASS (sym) = LOC_CONST;
      break;

    default:
      complaint (&symfile_complaints,
		 _("unsupported const value attribute form: '%s'"),
		 dwarf_form_name (attr->form));
      SYMBOL_VALUE (sym) = 0;
      SYMBOL_CLASS (sym) = LOC_CONST;
      break;
    }
}


/* Given an attr with a DW_FORM_dataN value in host byte order, sign-
   or zero-extend it as appropriate for the symbol's type.  */
static void
dwarf2_const_value_data (struct attribute *attr,
			 struct symbol *sym,
			 int bits)
{
  LONGEST l = DW_UNSND (attr);

  if (bits < sizeof (l) * 8)
    {
      if (TYPE_UNSIGNED (check_typedef (SYMBOL_TYPE (sym))))
	l &= ((LONGEST) 1 << bits) - 1;
      else
	l = (l << (sizeof (l) * 8 - bits)) >> (sizeof (l) * 8 - bits);
    }

  SYMBOL_VALUE (sym) = l;
  SYMBOL_CLASS (sym) = LOC_CONST;
}


/* Return the type of the die in question using its DW_AT_type attribute.  */

static struct type *
die_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type;
  struct attribute *type_attr;
  struct die_info *type_die;
  /* APPLE LOCAL avoid unused var warning.  */
  /* int type_id;  */

  type_attr = dwarf2_attr (die, DW_AT_type, cu);
  /* APPLE LOCAL begin dwarf repository  */
  if (type_attr)
    type_die = follow_die_ref (die, type_attr, cu);
  else
    {
      type_attr = dwarf2_attr (die, DW_AT_APPLE_repository_type, cu);
      if (type_attr)
	type_die = follow_db_ref (die, type_attr, cu);
    }

  if (!type_attr)
    {
      /* A missing DW_AT_type represents a void type.  */
      return dwarf2_fundamental_type (cu->objfile, FT_VOID, cu);
    }
  /* APPLE LOCAL end dwarf repository  */
	
  type = tag_type_to_type (type_die, cu);
  if (!type)
    {
      dump_die (type_die);
      error (_("Dwarf Error: Problem turning type die at offset into gdb type [in module %s]"),
		      cu->objfile->name);
    }
  return type;
}

/* Return the containing type of the die in question using its
   DW_AT_containing_type attribute.  */

static struct type *
die_containing_type (struct die_info *die, struct dwarf2_cu *cu)
{
  struct type *type = NULL;
  struct attribute *type_attr;
  struct die_info *type_die = NULL;

  type_attr = dwarf2_attr (die, DW_AT_containing_type, cu);
  if (type_attr)
    {
      type_die = follow_die_ref (die, type_attr, cu);
      type = tag_type_to_type (type_die, cu);
    }
  if (!type)
    {
      if (type_die)
	dump_die (type_die);
      error (_("Dwarf Error: Problem turning containing type into gdb type [in module %s]"), 
		      cu->objfile->name);
    }
  return type;
}

static struct type *
tag_type_to_type (struct die_info *die, struct dwarf2_cu *cu)
{
  if (die->type)
    {
      return die->type;
    }
  else
    {
      read_type_die (die, cu);
      if (!die->type)
	{
	  dump_die (die);
	  error (_("Dwarf Error: Cannot find type of die [in module %s]"), 
			  cu->objfile->name);
	}
      return die->type;
    }
}

static void
read_type_die (struct die_info *die, struct dwarf2_cu *cu)
{
  char *prefix = determine_prefix (die, cu);
  const char *old_prefix = processing_current_prefix;
  struct cleanup *back_to = make_cleanup (xfree, prefix);
  processing_current_prefix = prefix;
  
  switch (die->tag)
    {
    case DW_TAG_class_type:
    case DW_TAG_structure_type:
    case DW_TAG_union_type:
      read_structure_type (die, cu);
      break;
    case DW_TAG_enumeration_type:
      read_enumeration_type (die, cu);
      break;
    case DW_TAG_subprogram:
    case DW_TAG_subroutine_type:
      read_subroutine_type (die, cu);
      break;
    case DW_TAG_array_type:
      read_array_type (die, cu);
      break;
    case DW_TAG_set_type:
      read_set_type (die, cu);
      break;
    case DW_TAG_pointer_type:
      read_tag_pointer_type (die, cu);
      break;
    case DW_TAG_ptr_to_member_type:
      read_tag_ptr_to_member_type (die, cu);
      break;
    case DW_TAG_reference_type:
      read_tag_reference_type (die, cu);
      break;
    case DW_TAG_const_type:
      read_tag_const_type (die, cu);
      break;
    case DW_TAG_volatile_type:
      read_tag_volatile_type (die, cu);
      break;
    case DW_TAG_restrict_type:
      read_tag_restrict_type (die, cu);
      break;
    case DW_TAG_string_type:
      read_tag_string_type (die, cu);
      break;
    case DW_TAG_typedef:
      read_typedef (die, cu);
      break;
    case DW_TAG_subrange_type:
      read_subrange_type (die, cu);
      break;
    case DW_TAG_base_type:
      read_base_type (die, cu);
      break;
    default:
      complaint (&symfile_complaints, _("unexepected tag in read_type_die: '%s'"),
		 dwarf_tag_name (die->tag));
      break;
    }

  processing_current_prefix = old_prefix;
  do_cleanups (back_to);
}

/* Return the name of the namespace/class that DIE is defined within,
   or "" if we can't tell.  The caller should xfree the result.  */

/* NOTE: carlton/2004-01-23: See read_func_scope (and the comment
   therein) for an example of how to use this function to deal with
   DW_AT_specification.  */

static char *
determine_prefix (struct die_info *die, struct dwarf2_cu *cu)
{
  struct die_info *parent;

  if (cu->language != language_cplus
      && cu->language != language_objcplus
      && cu->language != language_java)
    return NULL;

  parent = die->parent;

  if (parent == NULL)
    {
      return xstrdup ("");
    }
  else
    {
      switch (parent->tag) {
      case DW_TAG_namespace:
	{
	  /* FIXME: carlton/2004-03-05: Should I follow extension dies
	     before doing this check?  */
	  if (parent->type != NULL && TYPE_TAG_NAME (parent->type) != NULL)
	    {
	      return xstrdup (TYPE_TAG_NAME (parent->type));
	    }
	  else
	    {
	      int dummy;
	      char *parent_prefix = determine_prefix (parent, cu);
	      char *retval = typename_concat (NULL, parent_prefix,
					      namespace_name (parent, &dummy,
							      cu),
					      cu);
	      xfree (parent_prefix);
	      return retval;
	    }
	}
	break;
      case DW_TAG_class_type:
      case DW_TAG_structure_type:
	{
	  if (parent->type != NULL && TYPE_TAG_NAME (parent->type) != NULL)
	    {
	      return xstrdup (TYPE_TAG_NAME (parent->type));
	    }
	  else
	    {
	      const char *old_prefix = processing_current_prefix;
	      char *new_prefix = determine_prefix (parent, cu);
	      char *retval;

	      processing_current_prefix = new_prefix;
	      retval = determine_class_name (parent, cu);
	      processing_current_prefix = old_prefix;

	      xfree (new_prefix);
	      return retval;
	    }
	}
      default:
	return determine_prefix (parent, cu);
      }
    }
}

/* Return a newly-allocated string formed by concatenating PREFIX and
   SUFFIX with appropriate separator.  If PREFIX or SUFFIX is NULL or empty, then
   simply copy the SUFFIX or PREFIX, respectively.  If OBS is non-null,
   perform an obconcat, otherwise allocate storage for the result.  The CU argument
   is used to determine the language and hence, the appropriate separator.  */

#define MAX_SEP_LEN 2  /* sizeof ("::")  */

static char *
typename_concat (struct obstack *obs, const char *prefix, const char *suffix, 
		 struct dwarf2_cu *cu)
{
  char *sep;

  if (suffix == NULL || suffix[0] == '\0' || prefix == NULL || prefix[0] == '\0')
    sep = "";
  else if (cu->language == language_java)
    sep = ".";
  else
    sep = "::";

  if (obs == NULL)
    {
      /* APPLE LOCAL: The FSF code passes prefix & suffix to strlen w/o checking 
	 if they are NULL.  */
      int suffixlen = 0, prefixlen = 0;
      char *retval;

      if (suffix != NULL)
	suffixlen = strlen (suffix);
      if (prefix != NULL)
	prefixlen = strlen (prefix);

      retval = xmalloc (prefixlen
			+ MAX_SEP_LEN 
			+ suffixlen 
			+ 1);

      /* END APPLE LOCAL */
      retval[0] = '\0';
      
      if (prefix)
	{
	  strcpy (retval, prefix);
	  strcat (retval, sep);
	}
      if (suffix)
	strcat (retval, suffix);
      
      return retval;
    }
  else
    {
      /* We have an obstack.  */
      /* APPLE LOCAL: Don't send NULL prefix or suffix to obconcat, 
	 it doesn't check.  */
      return obconcat (obs, prefix ? prefix : "", sep, suffix ? suffix : "");
    }
}

static struct type *
dwarf_base_type (int encoding, int size, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;

  /* FIXME - this should not produce a new (struct type *)
     every time.  It should cache base types.  */
  struct type *type;
  switch (encoding)
    {
    case DW_ATE_address:
      type = dwarf2_fundamental_type (objfile, FT_VOID, cu);
      return type;
    case DW_ATE_boolean:
      type = dwarf2_fundamental_type (objfile, FT_BOOLEAN, cu);
      return type;
    case DW_ATE_complex_float:
      if (size == 16)
	{
	  type = dwarf2_fundamental_type (objfile, FT_DBL_PREC_COMPLEX, cu);
	}
      else
	{
	  type = dwarf2_fundamental_type (objfile, FT_COMPLEX, cu);
	}
      return type;
    case DW_ATE_float:
      if (size == 8)
	{
	  type = dwarf2_fundamental_type (objfile, FT_DBL_PREC_FLOAT, cu);
	}
      else
	{
	  type = dwarf2_fundamental_type (objfile, FT_FLOAT, cu);
	}
      return type;
    case DW_ATE_signed:
      switch (size)
	{
	case 1:
	  type = dwarf2_fundamental_type (objfile, FT_SIGNED_CHAR, cu);
	  break;
	case 2:
	  type = dwarf2_fundamental_type (objfile, FT_SIGNED_SHORT, cu);
	  break;
	default:
	case 4:
	  type = dwarf2_fundamental_type (objfile, FT_SIGNED_INTEGER, cu);
	  break;
	}
      return type;
    case DW_ATE_signed_char:
      type = dwarf2_fundamental_type (objfile, FT_SIGNED_CHAR, cu);
      return type;
    case DW_ATE_unsigned:
      switch (size)
	{
	case 1:
	  type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_CHAR, cu);
	  break;
	case 2:
	  type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_SHORT, cu);
	  break;
	default:
	case 4:
	  type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_INTEGER, cu);
	  break;
	}
      return type;
    case DW_ATE_unsigned_char:
      type = dwarf2_fundamental_type (objfile, FT_UNSIGNED_CHAR, cu);
      return type;
    default:
      type = dwarf2_fundamental_type (objfile, FT_SIGNED_INTEGER, cu);
      return type;
    }
}

#if 0
struct die_info *
copy_die (struct die_info *old_die)
{
  struct die_info *new_die;
  int i, num_attrs;

  new_die = (struct die_info *) xmalloc (sizeof (struct die_info));
  memset (new_die, 0, sizeof (struct die_info));

  new_die->tag = old_die->tag;
  new_die->has_children = old_die->has_children;
  new_die->abbrev = old_die->abbrev;
  new_die->offset = old_die->offset;
  /* APPLE LOCAL - dwarf repository  */
  new_die->repository_id = old_die->repository_id;
  new_die->type = NULL;

  num_attrs = old_die->num_attrs;
  new_die->num_attrs = num_attrs;
  new_die->attrs = (struct attribute *)
    xmalloc (num_attrs * sizeof (struct attribute));

  for (i = 0; i < old_die->num_attrs; ++i)
    {
      new_die->attrs[i].name = old_die->attrs[i].name;
      new_die->attrs[i].form = old_die->attrs[i].form;
      new_die->attrs[i].u.addr = old_die->attrs[i].u.addr;
    }

  new_die->next = NULL;
  return new_die;
}
#endif

/* Return sibling of die, NULL if no sibling.  */

static struct die_info *
sibling_die (struct die_info *die)
{
  return die->sibling;
}

/* Get linkage name of a die, return NULL if not found.  */

static char *
dwarf2_linkage_name (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_MIPS_linkage_name, cu);
  if (attr && DW_STRING (attr))
    return DW_STRING (attr);
  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    return DW_STRING (attr);
  return NULL;
}

/* Get name of a die, return NULL if not found.  */

static char *
dwarf2_name (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_name, cu);
  if (attr && DW_STRING (attr))
    return DW_STRING (attr);
  return NULL;
}

/* Return the die that this die in an extension of, or NULL if there
   is none.  */

static struct die_info *
dwarf2_extension (struct die_info *die, struct dwarf2_cu *cu)
{
  struct attribute *attr;

  attr = dwarf2_attr (die, DW_AT_extension, cu);
  if (attr == NULL)
    return NULL;

  return follow_die_ref (die, attr, cu);
}

/* Convert a DIE tag into its string name.  */

static char *
dwarf_tag_name (unsigned tag)
{
  switch (tag)
    {
    case DW_TAG_padding:
      return "DW_TAG_padding";
    case DW_TAG_array_type:
      return "DW_TAG_array_type";
    case DW_TAG_class_type:
      return "DW_TAG_class_type";
    case DW_TAG_entry_point:
      return "DW_TAG_entry_point";
    case DW_TAG_enumeration_type:
      return "DW_TAG_enumeration_type";
    case DW_TAG_formal_parameter:
      return "DW_TAG_formal_parameter";
    case DW_TAG_imported_declaration:
      return "DW_TAG_imported_declaration";
    case DW_TAG_label:
      return "DW_TAG_label";
    case DW_TAG_lexical_block:
      return "DW_TAG_lexical_block";
    case DW_TAG_member:
      return "DW_TAG_member";
    case DW_TAG_pointer_type:
      return "DW_TAG_pointer_type";
    case DW_TAG_reference_type:
      return "DW_TAG_reference_type";
    case DW_TAG_compile_unit:
      return "DW_TAG_compile_unit";
    case DW_TAG_string_type:
      return "DW_TAG_string_type";
    case DW_TAG_structure_type:
      return "DW_TAG_structure_type";
    case DW_TAG_subroutine_type:
      return "DW_TAG_subroutine_type";
    case DW_TAG_typedef:
      return "DW_TAG_typedef";
    case DW_TAG_union_type:
      return "DW_TAG_union_type";
    case DW_TAG_unspecified_parameters:
      return "DW_TAG_unspecified_parameters";
    case DW_TAG_variant:
      return "DW_TAG_variant";
    case DW_TAG_common_block:
      return "DW_TAG_common_block";
    case DW_TAG_common_inclusion:
      return "DW_TAG_common_inclusion";
    case DW_TAG_inheritance:
      return "DW_TAG_inheritance";
    case DW_TAG_inlined_subroutine:
      return "DW_TAG_inlined_subroutine";
    case DW_TAG_module:
      return "DW_TAG_module";
    case DW_TAG_ptr_to_member_type:
      return "DW_TAG_ptr_to_member_type";
    case DW_TAG_set_type:
      return "DW_TAG_set_type";
    case DW_TAG_subrange_type:
      return "DW_TAG_subrange_type";
    case DW_TAG_with_stmt:
      return "DW_TAG_with_stmt";
    case DW_TAG_access_declaration:
      return "DW_TAG_access_declaration";
    case DW_TAG_base_type:
      return "DW_TAG_base_type";
    case DW_TAG_catch_block:
      return "DW_TAG_catch_block";
    case DW_TAG_const_type:
      return "DW_TAG_const_type";
    case DW_TAG_constant:
      return "DW_TAG_constant";
    case DW_TAG_enumerator:
      return "DW_TAG_enumerator";
    case DW_TAG_file_type:
      return "DW_TAG_file_type";
    case DW_TAG_friend:
      return "DW_TAG_friend";
    case DW_TAG_namelist:
      return "DW_TAG_namelist";
    case DW_TAG_namelist_item:
      return "DW_TAG_namelist_item";
    case DW_TAG_packed_type:
      return "DW_TAG_packed_type";
    case DW_TAG_subprogram:
      return "DW_TAG_subprogram";
    case DW_TAG_template_type_param:
      return "DW_TAG_template_type_param";
    case DW_TAG_template_value_param:
      return "DW_TAG_template_value_param";
    case DW_TAG_thrown_type:
      return "DW_TAG_thrown_type";
    case DW_TAG_try_block:
      return "DW_TAG_try_block";
    case DW_TAG_variant_part:
      return "DW_TAG_variant_part";
    case DW_TAG_variable:
      return "DW_TAG_variable";
    case DW_TAG_volatile_type:
      return "DW_TAG_volatile_type";
    case DW_TAG_dwarf_procedure:
      return "DW_TAG_dwarf_procedure";
    case DW_TAG_restrict_type:
      return "DW_TAG_restrict_type";
    case DW_TAG_interface_type:
      return "DW_TAG_interface_type";
    case DW_TAG_namespace:
      return "DW_TAG_namespace";
    case DW_TAG_imported_module:
      return "DW_TAG_imported_module";
    case DW_TAG_unspecified_type:
      return "DW_TAG_unspecified_type";
    case DW_TAG_partial_unit:
      return "DW_TAG_partial_unit";
    case DW_TAG_imported_unit:
      return "DW_TAG_imported_unit";
    case DW_TAG_MIPS_loop:
      return "DW_TAG_MIPS_loop";
    case DW_TAG_format_label:
      return "DW_TAG_format_label";
    case DW_TAG_function_template:
      return "DW_TAG_function_template";
    case DW_TAG_class_template:
      return "DW_TAG_class_template";
    default:
      return "DW_TAG_<unknown>";
    }
}

/* Convert a DWARF attribute code into its string name.  */

static char *
dwarf_attr_name (unsigned attr)
{
  switch (attr)
    {
    case DW_AT_sibling:
      return "DW_AT_sibling";
    case DW_AT_location:
      return "DW_AT_location";
    case DW_AT_name:
      return "DW_AT_name";
    case DW_AT_ordering:
      return "DW_AT_ordering";
    case DW_AT_subscr_data:
      return "DW_AT_subscr_data";
    case DW_AT_byte_size:
      return "DW_AT_byte_size";
    case DW_AT_bit_offset:
      return "DW_AT_bit_offset";
    case DW_AT_bit_size:
      return "DW_AT_bit_size";
    case DW_AT_element_list:
      return "DW_AT_element_list";
    case DW_AT_stmt_list:
      return "DW_AT_stmt_list";
    case DW_AT_low_pc:
      return "DW_AT_low_pc";
    case DW_AT_high_pc:
      return "DW_AT_high_pc";
    case DW_AT_language:
      return "DW_AT_language";
    case DW_AT_member:
      return "DW_AT_member";
    case DW_AT_discr:
      return "DW_AT_discr";
    case DW_AT_discr_value:
      return "DW_AT_discr_value";
    case DW_AT_visibility:
      return "DW_AT_visibility";
    case DW_AT_import:
      return "DW_AT_import";
    case DW_AT_string_length:
      return "DW_AT_string_length";
    case DW_AT_common_reference:
      return "DW_AT_common_reference";
    case DW_AT_comp_dir:
      return "DW_AT_comp_dir";
    case DW_AT_const_value:
      return "DW_AT_const_value";
    case DW_AT_containing_type:
      return "DW_AT_containing_type";
    case DW_AT_default_value:
      return "DW_AT_default_value";
    case DW_AT_inline:
      return "DW_AT_inline";
    case DW_AT_is_optional:
      return "DW_AT_is_optional";
    case DW_AT_lower_bound:
      return "DW_AT_lower_bound";
    case DW_AT_producer:
      return "DW_AT_producer";
    case DW_AT_prototyped:
      return "DW_AT_prototyped";
    case DW_AT_return_addr:
      return "DW_AT_return_addr";
    case DW_AT_start_scope:
      return "DW_AT_start_scope";
    case DW_AT_stride_size:
      return "DW_AT_stride_size";
    case DW_AT_upper_bound:
      return "DW_AT_upper_bound";
    case DW_AT_abstract_origin:
      return "DW_AT_abstract_origin";
    case DW_AT_accessibility:
      return "DW_AT_accessibility";
    case DW_AT_address_class:
      return "DW_AT_address_class";
    case DW_AT_artificial:
      return "DW_AT_artificial";
    case DW_AT_base_types:
      return "DW_AT_base_types";
    case DW_AT_calling_convention:
      return "DW_AT_calling_convention";
    case DW_AT_count:
      return "DW_AT_count";
    case DW_AT_data_member_location:
      return "DW_AT_data_member_location";
    case DW_AT_decl_column:
      return "DW_AT_decl_column";
    case DW_AT_decl_file:
      return "DW_AT_decl_file";
    case DW_AT_decl_line:
      return "DW_AT_decl_line";
    case DW_AT_declaration:
      return "DW_AT_declaration";
    case DW_AT_discr_list:
      return "DW_AT_discr_list";
    case DW_AT_encoding:
      return "DW_AT_encoding";
    case DW_AT_external:
      return "DW_AT_external";
    case DW_AT_frame_base:
      return "DW_AT_frame_base";
    case DW_AT_friend:
      return "DW_AT_friend";
    case DW_AT_identifier_case:
      return "DW_AT_identifier_case";
    case DW_AT_macro_info:
      return "DW_AT_macro_info";
    case DW_AT_namelist_items:
      return "DW_AT_namelist_items";
    case DW_AT_priority:
      return "DW_AT_priority";
    case DW_AT_segment:
      return "DW_AT_segment";
    case DW_AT_specification:
      return "DW_AT_specification";
    case DW_AT_static_link:
      return "DW_AT_static_link";
    case DW_AT_type:
      return "DW_AT_type";
    case DW_AT_use_location:
      return "DW_AT_use_location";
    case DW_AT_variable_parameter:
      return "DW_AT_variable_parameter";
    case DW_AT_virtuality:
      return "DW_AT_virtuality";
    case DW_AT_vtable_elem_location:
      return "DW_AT_vtable_elem_location";
    case DW_AT_allocated:
      return "DW_AT_allocated";
    case DW_AT_associated:
      return "DW_AT_associated";
    case DW_AT_data_location:
      return "DW_AT_data_location";
    case DW_AT_stride:
      return "DW_AT_stride";
    case DW_AT_entry_pc:
      return "DW_AT_entry_pc";
    case DW_AT_use_UTF8:
      return "DW_AT_use_UTF8";
    case DW_AT_extension:
      return "DW_AT_extension";
    case DW_AT_ranges:
      return "DW_AT_ranges";
    case DW_AT_trampoline:
      return "DW_AT_trampoline";
    case DW_AT_call_column:
      return "DW_AT_call_column";
    case DW_AT_call_file:
      return "DW_AT_call_file";
    case DW_AT_call_line:
      return "DW_AT_call_line";
#ifdef MIPS
    case DW_AT_MIPS_fde:
      return "DW_AT_MIPS_fde";
    case DW_AT_MIPS_loop_begin:
      return "DW_AT_MIPS_loop_begin";
    case DW_AT_MIPS_tail_loop_begin:
      return "DW_AT_MIPS_tail_loop_begin";
    case DW_AT_MIPS_epilog_begin:
      return "DW_AT_MIPS_epilog_begin";
    case DW_AT_MIPS_loop_unroll_factor:
      return "DW_AT_MIPS_loop_unroll_factor";
    case DW_AT_MIPS_software_pipeline_depth:
      return "DW_AT_MIPS_software_pipeline_depth";
#endif
    case DW_AT_MIPS_linkage_name:
      return "DW_AT_MIPS_linkage_name";

    case DW_AT_sf_names:
      return "DW_AT_sf_names";
    case DW_AT_src_info:
      return "DW_AT_src_info";
    case DW_AT_mac_info:
      return "DW_AT_mac_info";
    case DW_AT_src_coords:
      return "DW_AT_src_coords";
    case DW_AT_body_begin:
      return "DW_AT_body_begin";
    case DW_AT_body_end:
      return "DW_AT_body_end";
    case DW_AT_GNU_vector:
      return "DW_AT_GNU_vector";
    /* APPLE LOCAL begin dwarf repository  */
    case DW_AT_APPLE_repository_file:
      return "DW_AT_APPLE_repository_file";
    case DW_AT_APPLE_repository_type:
      return "DW_AT_APPLE_repository_type";
    case DW_AT_APPLE_repository_name:
      return "DW_AT_APPLE_repository_name";
    case DW_AT_APPLE_repository_specification:
      return "DW_AT_APPLE_repository_specification";
    case DW_AT_APPLE_repository_import:
      return "DW_AT_APPLE_repository_import";
    case DW_AT_APPLE_repository_abstract_origin:
      return "DW_AT_APPLE_repository_abstract_origin";
    /* APPLE LOCAL end dwarf repository  */
    default:
      return "DW_AT_<unknown>";
    }
}

/* Convert a DWARF value form code into its string name.  */

static char *
dwarf_form_name (unsigned form)
{
  switch (form)
    {
    case DW_FORM_addr:
      return "DW_FORM_addr";
    case DW_FORM_block2:
      return "DW_FORM_block2";
    case DW_FORM_block4:
      return "DW_FORM_block4";
    case DW_FORM_data2:
      return "DW_FORM_data2";
    case DW_FORM_data4:
      return "DW_FORM_data4";
    case DW_FORM_data8:
      return "DW_FORM_data8";
    case DW_FORM_string:
      return "DW_FORM_string";
    case DW_FORM_block:
      return "DW_FORM_block";
    case DW_FORM_block1:
      return "DW_FORM_block1";
    case DW_FORM_data1:
      return "DW_FORM_data1";
    case DW_FORM_flag:
      return "DW_FORM_flag";
    case DW_FORM_sdata:
      return "DW_FORM_sdata";
    case DW_FORM_strp:
      return "DW_FORM_strp";
    case DW_FORM_udata:
      return "DW_FORM_udata";
    case DW_FORM_ref_addr:
      return "DW_FORM_ref_addr";
    case DW_FORM_ref1:
      return "DW_FORM_ref1";
    case DW_FORM_ref2:
      return "DW_FORM_ref2";
    case DW_FORM_ref4:
      return "DW_FORM_ref4";
    case DW_FORM_ref8:
      return "DW_FORM_ref8";
    case DW_FORM_ref_udata:
      return "DW_FORM_ref_udata";
    case DW_FORM_indirect:
      return "DW_FORM_indirect";
    case DW_FORM_APPLE_db_str:
      return "DW_FORM_APPLE_db_str";
    default:
      return "DW_FORM_<unknown>";
    }
}

/* Convert a DWARF stack opcode into its string name.  */

static char *
dwarf_stack_op_name (unsigned op)
{
  switch (op)
    {
    case DW_OP_addr:
      return "DW_OP_addr";
    case DW_OP_deref:
      return "DW_OP_deref";
    case DW_OP_const1u:
      return "DW_OP_const1u";
    case DW_OP_const1s:
      return "DW_OP_const1s";
    case DW_OP_const2u:
      return "DW_OP_const2u";
    case DW_OP_const2s:
      return "DW_OP_const2s";
    case DW_OP_const4u:
      return "DW_OP_const4u";
    case DW_OP_const4s:
      return "DW_OP_const4s";
    case DW_OP_const8u:
      return "DW_OP_const8u";
    case DW_OP_const8s:
      return "DW_OP_const8s";
    case DW_OP_constu:
      return "DW_OP_constu";
    case DW_OP_consts:
      return "DW_OP_consts";
    case DW_OP_dup:
      return "DW_OP_dup";
    case DW_OP_drop:
      return "DW_OP_drop";
    case DW_OP_over:
      return "DW_OP_over";
    case DW_OP_pick:
      return "DW_OP_pick";
    case DW_OP_swap:
      return "DW_OP_swap";
    case DW_OP_rot:
      return "DW_OP_rot";
    case DW_OP_xderef:
      return "DW_OP_xderef";
    case DW_OP_abs:
      return "DW_OP_abs";
    case DW_OP_and:
      return "DW_OP_and";
    case DW_OP_div:
      return "DW_OP_div";
    case DW_OP_minus:
      return "DW_OP_minus";
    case DW_OP_mod:
      return "DW_OP_mod";
    case DW_OP_mul:
      return "DW_OP_mul";
    case DW_OP_neg:
      return "DW_OP_neg";
    case DW_OP_not:
      return "DW_OP_not";
    case DW_OP_or:
      return "DW_OP_or";
    case DW_OP_plus:
      return "DW_OP_plus";
    case DW_OP_plus_uconst:
      return "DW_OP_plus_uconst";
    case DW_OP_shl:
      return "DW_OP_shl";
    case DW_OP_shr:
      return "DW_OP_shr";
    case DW_OP_shra:
      return "DW_OP_shra";
    case DW_OP_xor:
      return "DW_OP_xor";
    case DW_OP_bra:
      return "DW_OP_bra";
    case DW_OP_eq:
      return "DW_OP_eq";
    case DW_OP_ge:
      return "DW_OP_ge";
    case DW_OP_gt:
      return "DW_OP_gt";
    case DW_OP_le:
      return "DW_OP_le";
    case DW_OP_lt:
      return "DW_OP_lt";
    case DW_OP_ne:
      return "DW_OP_ne";
    case DW_OP_skip:
      return "DW_OP_skip";
    case DW_OP_lit0:
      return "DW_OP_lit0";
    case DW_OP_lit1:
      return "DW_OP_lit1";
    case DW_OP_lit2:
      return "DW_OP_lit2";
    case DW_OP_lit3:
      return "DW_OP_lit3";
    case DW_OP_lit4:
      return "DW_OP_lit4";
    case DW_OP_lit5:
      return "DW_OP_lit5";
    case DW_OP_lit6:
      return "DW_OP_lit6";
    case DW_OP_lit7:
      return "DW_OP_lit7";
    case DW_OP_lit8:
      return "DW_OP_lit8";
    case DW_OP_lit9:
      return "DW_OP_lit9";
    case DW_OP_lit10:
      return "DW_OP_lit10";
    case DW_OP_lit11:
      return "DW_OP_lit11";
    case DW_OP_lit12:
      return "DW_OP_lit12";
    case DW_OP_lit13:
      return "DW_OP_lit13";
    case DW_OP_lit14:
      return "DW_OP_lit14";
    case DW_OP_lit15:
      return "DW_OP_lit15";
    case DW_OP_lit16:
      return "DW_OP_lit16";
    case DW_OP_lit17:
      return "DW_OP_lit17";
    case DW_OP_lit18:
      return "DW_OP_lit18";
    case DW_OP_lit19:
      return "DW_OP_lit19";
    case DW_OP_lit20:
      return "DW_OP_lit20";
    case DW_OP_lit21:
      return "DW_OP_lit21";
    case DW_OP_lit22:
      return "DW_OP_lit22";
    case DW_OP_lit23:
      return "DW_OP_lit23";
    case DW_OP_lit24:
      return "DW_OP_lit24";
    case DW_OP_lit25:
      return "DW_OP_lit25";
    case DW_OP_lit26:
      return "DW_OP_lit26";
    case DW_OP_lit27:
      return "DW_OP_lit27";
    case DW_OP_lit28:
      return "DW_OP_lit28";
    case DW_OP_lit29:
      return "DW_OP_lit29";
    case DW_OP_lit30:
      return "DW_OP_lit30";
    case DW_OP_lit31:
      return "DW_OP_lit31";
    case DW_OP_reg0:
      return "DW_OP_reg0";
    case DW_OP_reg1:
      return "DW_OP_reg1";
    case DW_OP_reg2:
      return "DW_OP_reg2";
    case DW_OP_reg3:
      return "DW_OP_reg3";
    case DW_OP_reg4:
      return "DW_OP_reg4";
    case DW_OP_reg5:
      return "DW_OP_reg5";
    case DW_OP_reg6:
      return "DW_OP_reg6";
    case DW_OP_reg7:
      return "DW_OP_reg7";
    case DW_OP_reg8:
      return "DW_OP_reg8";
    case DW_OP_reg9:
      return "DW_OP_reg9";
    case DW_OP_reg10:
      return "DW_OP_reg10";
    case DW_OP_reg11:
      return "DW_OP_reg11";
    case DW_OP_reg12:
      return "DW_OP_reg12";
    case DW_OP_reg13:
      return "DW_OP_reg13";
    case DW_OP_reg14:
      return "DW_OP_reg14";
    case DW_OP_reg15:
      return "DW_OP_reg15";
    case DW_OP_reg16:
      return "DW_OP_reg16";
    case DW_OP_reg17:
      return "DW_OP_reg17";
    case DW_OP_reg18:
      return "DW_OP_reg18";
    case DW_OP_reg19:
      return "DW_OP_reg19";
    case DW_OP_reg20:
      return "DW_OP_reg20";
    case DW_OP_reg21:
      return "DW_OP_reg21";
    case DW_OP_reg22:
      return "DW_OP_reg22";
    case DW_OP_reg23:
      return "DW_OP_reg23";
    case DW_OP_reg24:
      return "DW_OP_reg24";
    case DW_OP_reg25:
      return "DW_OP_reg25";
    case DW_OP_reg26:
      return "DW_OP_reg26";
    case DW_OP_reg27:
      return "DW_OP_reg27";
    case DW_OP_reg28:
      return "DW_OP_reg28";
    case DW_OP_reg29:
      return "DW_OP_reg29";
    case DW_OP_reg30:
      return "DW_OP_reg30";
    case DW_OP_reg31:
      return "DW_OP_reg31";
    case DW_OP_breg0:
      return "DW_OP_breg0";
    case DW_OP_breg1:
      return "DW_OP_breg1";
    case DW_OP_breg2:
      return "DW_OP_breg2";
    case DW_OP_breg3:
      return "DW_OP_breg3";
    case DW_OP_breg4:
      return "DW_OP_breg4";
    case DW_OP_breg5:
      return "DW_OP_breg5";
    case DW_OP_breg6:
      return "DW_OP_breg6";
    case DW_OP_breg7:
      return "DW_OP_breg7";
    case DW_OP_breg8:
      return "DW_OP_breg8";
    case DW_OP_breg9:
      return "DW_OP_breg9";
    case DW_OP_breg10:
      return "DW_OP_breg10";
    case DW_OP_breg11:
      return "DW_OP_breg11";
    case DW_OP_breg12:
      return "DW_OP_breg12";
    case DW_OP_breg13:
      return "DW_OP_breg13";
    case DW_OP_breg14:
      return "DW_OP_breg14";
    case DW_OP_breg15:
      return "DW_OP_breg15";
    case DW_OP_breg16:
      return "DW_OP_breg16";
    case DW_OP_breg17:
      return "DW_OP_breg17";
    case DW_OP_breg18:
      return "DW_OP_breg18";
    case DW_OP_breg19:
      return "DW_OP_breg19";
    case DW_OP_breg20:
      return "DW_OP_breg20";
    case DW_OP_breg21:
      return "DW_OP_breg21";
    case DW_OP_breg22:
      return "DW_OP_breg22";
    case DW_OP_breg23:
      return "DW_OP_breg23";
    case DW_OP_breg24:
      return "DW_OP_breg24";
    case DW_OP_breg25:
      return "DW_OP_breg25";
    case DW_OP_breg26:
      return "DW_OP_breg26";
    case DW_OP_breg27:
      return "DW_OP_breg27";
    case DW_OP_breg28:
      return "DW_OP_breg28";
    case DW_OP_breg29:
      return "DW_OP_breg29";
    case DW_OP_breg30:
      return "DW_OP_breg30";
    case DW_OP_breg31:
      return "DW_OP_breg31";
    case DW_OP_regx:
      return "DW_OP_regx";
    case DW_OP_fbreg:
      return "DW_OP_fbreg";
    case DW_OP_bregx:
      return "DW_OP_bregx";
    case DW_OP_piece:
      return "DW_OP_piece";
    case DW_OP_deref_size:
      return "DW_OP_deref_size";
    case DW_OP_xderef_size:
      return "DW_OP_xderef_size";
    case DW_OP_nop:
      return "DW_OP_nop";
      /* DWARF 3 extensions.  */
    case DW_OP_push_object_address:
      return "DW_OP_push_object_address";
    case DW_OP_call2:
      return "DW_OP_call2";
    case DW_OP_call4:
      return "DW_OP_call4";
    case DW_OP_call_ref:
      return "DW_OP_call_ref";
      /* GNU extensions.  */
    case DW_OP_GNU_push_tls_address:
      return "DW_OP_GNU_push_tls_address";
    /* APPLE LOCAL begin variable initialized status  */
    case DW_OP_APPLE_uninit:
      return "DW_OP_APPLE_uninit";
    /* APPLE LOCAL end variable initialized status  */
    default:
      return "OP_<unknown>";
    }
}

static char *
dwarf_bool_name (unsigned mybool)
{
  if (mybool)
    return "TRUE";
  else
    return "FALSE";
}

/* Convert a DWARF type code into its string name.  */

static char *
dwarf_type_encoding_name (unsigned enc)
{
  switch (enc)
    {
    case DW_ATE_address:
      return "DW_ATE_address";
    case DW_ATE_boolean:
      return "DW_ATE_boolean";
    case DW_ATE_complex_float:
      return "DW_ATE_complex_float";
    case DW_ATE_float:
      return "DW_ATE_float";
    case DW_ATE_signed:
      return "DW_ATE_signed";
    case DW_ATE_signed_char:
      return "DW_ATE_signed_char";
    case DW_ATE_unsigned:
      return "DW_ATE_unsigned";
    case DW_ATE_unsigned_char:
      return "DW_ATE_unsigned_char";
    case DW_ATE_imaginary_float:
      return "DW_ATE_imaginary_float";
    default:
      return "DW_ATE_<unknown>";
    }
}

/* Convert a DWARF call frame info operation to its string name. */

#if 0
static char *
dwarf_cfi_name (unsigned cfi_opc)
{
  switch (cfi_opc)
    {
    case DW_CFA_advance_loc:
      return "DW_CFA_advance_loc";
    case DW_CFA_offset:
      return "DW_CFA_offset";
    case DW_CFA_restore:
      return "DW_CFA_restore";
    case DW_CFA_nop:
      return "DW_CFA_nop";
    case DW_CFA_set_loc:
      return "DW_CFA_set_loc";
    case DW_CFA_advance_loc1:
      return "DW_CFA_advance_loc1";
    case DW_CFA_advance_loc2:
      return "DW_CFA_advance_loc2";
    case DW_CFA_advance_loc4:
      return "DW_CFA_advance_loc4";
    case DW_CFA_offset_extended:
      return "DW_CFA_offset_extended";
    case DW_CFA_restore_extended:
      return "DW_CFA_restore_extended";
    case DW_CFA_undefined:
      return "DW_CFA_undefined";
    case DW_CFA_same_value:
      return "DW_CFA_same_value";
    case DW_CFA_register:
      return "DW_CFA_register";
    case DW_CFA_remember_state:
      return "DW_CFA_remember_state";
    case DW_CFA_restore_state:
      return "DW_CFA_restore_state";
    case DW_CFA_def_cfa:
      return "DW_CFA_def_cfa";
    case DW_CFA_def_cfa_register:
      return "DW_CFA_def_cfa_register";
    case DW_CFA_def_cfa_offset:
      return "DW_CFA_def_cfa_offset";

    /* DWARF 3 */
    case DW_CFA_def_cfa_expression:
      return "DW_CFA_def_cfa_expression";
    case DW_CFA_expression:
      return "DW_CFA_expression";
    case DW_CFA_offset_extended_sf:
      return "DW_CFA_offset_extended_sf";
    case DW_CFA_def_cfa_sf:
      return "DW_CFA_def_cfa_sf";
    case DW_CFA_def_cfa_offset_sf:
      return "DW_CFA_def_cfa_offset_sf";

      /* SGI/MIPS specific */
    case DW_CFA_MIPS_advance_loc8:
      return "DW_CFA_MIPS_advance_loc8";

    /* GNU extensions */
    case DW_CFA_GNU_window_save:
      return "DW_CFA_GNU_window_save";
    case DW_CFA_GNU_args_size:
      return "DW_CFA_GNU_args_size";
    case DW_CFA_GNU_negative_offset_extended:
      return "DW_CFA_GNU_negative_offset_extended";

    default:
      return "DW_CFA_<unknown>";
    }
}
#endif

static void
dump_die (struct die_info *die)
{
  unsigned int i;

  fprintf_unfiltered (gdb_stderr, "Die: %s (abbrev = %d, offset = %d)\n",
	   dwarf_tag_name (die->tag), die->abbrev, die->offset);
  fprintf_unfiltered (gdb_stderr, "\thas children: %s\n",
	   dwarf_bool_name (die->child != NULL));

  fprintf_unfiltered (gdb_stderr, "\tattributes:\n");
  for (i = 0; i < die->num_attrs; ++i)
    {
      fprintf_unfiltered (gdb_stderr, "\t\t%s (%s) ",
	       dwarf_attr_name (die->attrs[i].name),
	       dwarf_form_name (die->attrs[i].form));
      switch (die->attrs[i].form)
	{
	case DW_FORM_ref_addr:
	case DW_FORM_addr:
	  fprintf_unfiltered (gdb_stderr, "address: ");
	  deprecated_print_address_numeric (DW_ADDR (&die->attrs[i]), 1, gdb_stderr);
	  break;
	case DW_FORM_block2:
	case DW_FORM_block4:
	case DW_FORM_block:
	case DW_FORM_block1:
	  fprintf_unfiltered (gdb_stderr, "block: size %d", DW_BLOCK (&die->attrs[i])->size);
	  break;
	case DW_FORM_ref1:
	case DW_FORM_ref2:
	case DW_FORM_ref4:
	  fprintf_unfiltered (gdb_stderr, "constant ref: %ld (adjusted)",
			      (long) (DW_ADDR (&die->attrs[i])));
	  break;
	case DW_FORM_APPLE_db_str:
	case DW_FORM_data1:
	case DW_FORM_data2:
	case DW_FORM_data4:
	case DW_FORM_data8:
	case DW_FORM_udata:
	case DW_FORM_sdata:
	  fprintf_unfiltered (gdb_stderr, "constant: %ld", DW_UNSND (&die->attrs[i]));
	  break;
	case DW_FORM_string:
	case DW_FORM_strp:
	  fprintf_unfiltered (gdb_stderr, "string: \"%s\"",
		   DW_STRING (&die->attrs[i])
		   ? DW_STRING (&die->attrs[i]) : "");
	  break;
	case DW_FORM_flag:
	  if (DW_UNSND (&die->attrs[i]))
	    fprintf_unfiltered (gdb_stderr, "flag: TRUE");
	  else
	    fprintf_unfiltered (gdb_stderr, "flag: FALSE");
	  break;
	case DW_FORM_indirect:
	  /* the reader will have reduced the indirect form to
	     the "base form" so this form should not occur */
	  fprintf_unfiltered (gdb_stderr, "unexpected attribute form: DW_FORM_indirect");
	  break;
	default:
	  fprintf_unfiltered (gdb_stderr, "unsupported attribute form: %d.",
		   die->attrs[i].form);
	}
      fprintf_unfiltered (gdb_stderr, "\n");
    }
}

static void
dump_die_list (struct die_info *die)
{
  while (die)
    {
      dump_die (die);
      if (die->child != NULL)
	dump_die_list (die->child);
      if (die->sibling != NULL)
	dump_die_list (die->sibling);
    }
}

static void
store_in_ref_table (unsigned int offset, struct die_info *die,
		    struct dwarf2_cu *cu)
{
  int h;
  struct die_info *old;

  h = (offset % REF_HASH_SIZE);
  old = cu->die_ref_table[h];
  die->next_ref = old;
  cu->die_ref_table[h] = die;
}

static unsigned int
dwarf2_get_ref_die_offset (struct attribute *attr, struct dwarf2_cu *cu)
{
  unsigned int result = 0;

  switch (attr->form)
    {
    case DW_FORM_ref_addr:
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
      result = DW_ADDR (attr);
      break;
    default:
      complaint (&symfile_complaints,
		 _("unsupported die ref attribute form: '%s'"),
		 dwarf_form_name (attr->form));
    }
  return result;
}

/* Return the constant value held by the given attribute.  Return -1
   if the value held by the attribute is not constant.  */

static int
dwarf2_get_attr_constant_value (struct attribute *attr, int default_value)
{
  if (attr->form == DW_FORM_sdata)
    return DW_SND (attr);
  else if (attr->form == DW_FORM_udata
           || attr->form == DW_FORM_data1
           || attr->form == DW_FORM_data2
           || attr->form == DW_FORM_data4
           || attr->form == DW_FORM_data8
	   || attr->form == DW_FORM_APPLE_db_str)
    return DW_UNSND (attr);
  else
    {
      complaint (&symfile_complaints, _("Attribute value is not a constant (%s)"),
                 dwarf_form_name (attr->form));
      return default_value;
    }
}

static struct die_info *
follow_die_ref (struct die_info *src_die, struct attribute *attr,
		struct dwarf2_cu *cu)
{
  struct die_info *die;
  unsigned int offset;
  int h;
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct die_info temp_die; */
  struct dwarf2_cu *target_cu;

  offset = dwarf2_get_ref_die_offset (attr, cu);

  /* APPLE LOCAL: Be sure to add in header.initial_length_size or
     the pointer to a type that is the last DIE in a file will be
     handled incorrectly.  */
  if (DW_ADDR (attr) < cu->header.offset
      || DW_ADDR (attr) >= cu->header.offset + cu->header.length +
                           cu->header.initial_length_size)
    {
      struct dwarf2_per_cu_data *per_cu;
      per_cu = dwarf2_find_containing_comp_unit (DW_ADDR (attr), 
                                                 cu->objfile);
      target_cu = per_cu->cu;
    }
  else
    target_cu = cu;

  h = (offset % REF_HASH_SIZE);
  die = target_cu->die_ref_table[h];
  while (die)
    {
      if (die->offset == offset)
	return die;
      die = die->next_ref;
    }

  error (_("Dwarf Error: Cannot find DIE at 0x%lx referenced from DIE "
	 "at 0x%lx [in module %s]"),
	 (long) src_die->offset, (long) offset, cu->objfile->name);

  return NULL;
}

static struct type *
dwarf2_fundamental_type (struct objfile *objfile, int typeid,
			 struct dwarf2_cu *cu)
{
  if (typeid < 0 || typeid >= FT_NUM_MEMBERS)
    {
      error (_("Dwarf Error: internal error - invalid fundamental type id %d [in module %s]"),
	     typeid, objfile->name);
    }

  /* Look for this particular type in the fundamental type vector.  If
     one is not found, create and install one appropriate for the
     current language and the current target machine. */

  if (cu->ftypes[typeid] == NULL)
    {
      cu->ftypes[typeid] = cu->language_defn->la_fund_type (objfile, typeid);
    }

  return (cu->ftypes[typeid]);
}

/* Decode simple location descriptions.
   Given a pointer to a dwarf block that defines a location, compute
   the location and return the value.

   NOTE drow/2003-11-18: This function is called in two situations
   now: for the address of static or global variables (partial symbols
   only) and for offsets into structures which are expected to be
   (more or less) constant.  The partial symbol case should go away,
   and only the constant case should remain.  That will let this
   function complain more accurately.  A few special modes are allowed
   without complaint for global variables (for instance, global
   register values and thread-local values).

   A location description containing no operations indicates that the
   object is optimized out.  The return value is 0 for that case.
   FIXME drow/2003-11-16: No callers check for this case any more; soon all
   callers will only want a very basic result and this can become a
   complaint.

   When the result is a register number, the global isreg flag is set,
   otherwise it is cleared.

   Note that stack[0] is unused except as a default error return. */

/* APPLE LOCAL: Size of the location expression stack in bytes, 
   for bounds checking.  */
#define LOCDESC_STACKSIZE 64

static CORE_ADDR
decode_locdesc (struct dwarf_block *blk, struct dwarf2_cu *cu)
{
  struct objfile *objfile = cu->objfile;
  /* APPLE LOCAL avoid unused var warning.  */
  /* struct comp_unit_head *cu_header = &cu->header; */
  int i;
  int size = blk->size;
  char *data = blk->data;
  CORE_ADDR stack[LOCDESC_STACKSIZE];
  int stacki;
  unsigned int bytes_read, unsnd;
  unsigned char op;

  i = 0;
  stacki = 0;
  stack[stacki] = 0;
  isreg = 0;

  /* APPLE LOCAL: Add stack array bounds check.  */
  while (i < size && stacki < LOCDESC_STACKSIZE)
    {
      op = data[i++];
      switch (op)
	{
	case DW_OP_lit0:
	case DW_OP_lit1:
	case DW_OP_lit2:
	case DW_OP_lit3:
	case DW_OP_lit4:
	case DW_OP_lit5:
	case DW_OP_lit6:
	case DW_OP_lit7:
	case DW_OP_lit8:
	case DW_OP_lit9:
	case DW_OP_lit10:
	case DW_OP_lit11:
	case DW_OP_lit12:
	case DW_OP_lit13:
	case DW_OP_lit14:
	case DW_OP_lit15:
	case DW_OP_lit16:
	case DW_OP_lit17:
	case DW_OP_lit18:
	case DW_OP_lit19:
	case DW_OP_lit20:
	case DW_OP_lit21:
	case DW_OP_lit22:
	case DW_OP_lit23:
	case DW_OP_lit24:
	case DW_OP_lit25:
	case DW_OP_lit26:
	case DW_OP_lit27:
	case DW_OP_lit28:
	case DW_OP_lit29:
	case DW_OP_lit30:
	case DW_OP_lit31:
	  stack[++stacki] = op - DW_OP_lit0;
	  break;

	case DW_OP_reg0:
	case DW_OP_reg1:
	case DW_OP_reg2:
	case DW_OP_reg3:
	case DW_OP_reg4:
	case DW_OP_reg5:
	case DW_OP_reg6:
	case DW_OP_reg7:
	case DW_OP_reg8:
	case DW_OP_reg9:
	case DW_OP_reg10:
	case DW_OP_reg11:
	case DW_OP_reg12:
	case DW_OP_reg13:
	case DW_OP_reg14:
	case DW_OP_reg15:
	case DW_OP_reg16:
	case DW_OP_reg17:
	case DW_OP_reg18:
	case DW_OP_reg19:
	case DW_OP_reg20:
	case DW_OP_reg21:
	case DW_OP_reg22:
	case DW_OP_reg23:
	case DW_OP_reg24:
	case DW_OP_reg25:
	case DW_OP_reg26:
	case DW_OP_reg27:
	case DW_OP_reg28:
	case DW_OP_reg29:
	case DW_OP_reg30:
	case DW_OP_reg31:
	  isreg = 1;
	  stack[++stacki] = op - DW_OP_reg0;
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
	  break;

	case DW_OP_regx:
	  isreg = 1;
	  unsnd = read_unsigned_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  stack[++stacki] = unsnd;
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
	  break;

	case DW_OP_addr:
          /* APPLE LOCAL Add cast to avoid type mismatch in arg4 warning.  */
          /* APPLE LOCAL: debug map */
          {
            CORE_ADDR addr;
            /* APPLE LOCAL: If we're in the middle of processing a 
               DW_TAG_common_block, DW_OP_addr refers to an offset within 
               that common block, I guess. */
            if (decode_locdesc_common &&
                translate_common_symbol_debug_map_address (cu->addr_map, 
                                           decode_locdesc_common, &addr))
             {
                CORE_ADDR off = read_address (objfile->obfd, &data[i], cu,
                                            (int *) &bytes_read);
                stack[++stacki] = addr + off;
             }
            else if (translate_debug_map_address (cu->addr_map, 
                                          read_address (objfile->obfd, &data[i],
					  cu, (int *) &bytes_read), &addr, 0))
              stack[++stacki] = addr;
            else
              return 0;
          }
	  i += bytes_read;
	  break;

	case DW_OP_const1u:
	  stack[++stacki] = read_1_byte (objfile->obfd, &data[i]);
	  i += 1;
	  break;

	case DW_OP_const1s:
	  stack[++stacki] = read_1_signed_byte (objfile->obfd, &data[i]);
	  i += 1;
	  break;

	case DW_OP_const2u:
	  stack[++stacki] = read_2_bytes (objfile->obfd, &data[i]);
	  i += 2;
	  break;

	case DW_OP_const2s:
	  stack[++stacki] = read_2_signed_bytes (objfile->obfd, &data[i]);
	  i += 2;
	  break;

	case DW_OP_const4u:
	  stack[++stacki] = read_4_bytes (objfile->obfd, &data[i]);
	  i += 4;
	  break;

	case DW_OP_const4s:
	  stack[++stacki] = read_4_signed_bytes (objfile->obfd, &data[i]);
	  i += 4;
	  break;

	case DW_OP_constu:
	  stack[++stacki] = read_unsigned_leb128 (NULL, (data + i),
						  &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_consts:
	  stack[++stacki] = read_signed_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_dup:
	  stack[stacki + 1] = stack[stacki];
	  stacki++;
	  break;

	case DW_OP_plus:
          /* APPLE LOCAL: Don't allow references outside the array.  */
          if (stacki < 1)
	    dwarf2_complex_location_expr_complaint ();
	  stack[stacki - 1] += stack[stacki];
	  stacki--;
	  break;

	case DW_OP_plus_uconst:
	  stack[stacki] += read_unsigned_leb128 (NULL, (data + i), &bytes_read);
	  i += bytes_read;
	  break;

	case DW_OP_minus:
          /* APPLE LOCAL: Don't allow references outside the array.  */
          if (stacki < 1)
	    dwarf2_complex_location_expr_complaint ();
	  stack[stacki - 1] -= stack[stacki];
	  stacki--;
	  break;

	case DW_OP_deref:
	  /* If we're not the last op, then we definitely can't encode
	     this using GDB's address_class enum.  This is valid for partial
	     global symbols, although the variable's address will be bogus
	     in the psymtab.  */
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
	  break;

        case DW_OP_GNU_push_tls_address:
	  /* The top of the stack has the offset from the beginning
	     of the thread control block at which the variable is located.  */
	  /* Nothing should follow this operator, so the top of stack would
	     be returned.  */
	  /* This is valid for partial global symbols, but the variable's
	     address will be bogus in the psymtab.  */
	  if (i < size)
	    dwarf2_complex_location_expr_complaint ();
          break;
	  
	/* APPLE LOCAL begin variable initialized status  */
	case DW_OP_APPLE_uninit:
	  break;
	/* APPLE LOCAL end variable initialized status  */

	default:
	  complaint (&symfile_complaints, _("unsupported stack op: '%s'"),
		     dwarf_stack_op_name (op));
	  return (stack[stacki]);
	}
      /* APPLE LOCAL: Add stack array bounds check.  */
      if (stacki >= LOCDESC_STACKSIZE - 1)
	internal_error (__FILE__, __LINE__,
	                _("location description stack too deep: %d"),
	                stacki);
    }
  return (stack[stacki]);
}

/* memory allocation interface */

static struct dwarf_block *
dwarf_alloc_block (struct dwarf2_cu *cu)
{
  struct dwarf_block *blk;

  blk = (struct dwarf_block *)
    obstack_alloc (&cu->comp_unit_obstack, sizeof (struct dwarf_block));
  return (blk);
}

static struct abbrev_info *
dwarf_alloc_abbrev (struct dwarf2_cu *cu)
{
  struct abbrev_info *abbrev;

  abbrev = (struct abbrev_info *)
    obstack_alloc (&cu->abbrev_obstack, sizeof (struct abbrev_info));
  memset (abbrev, 0, sizeof (struct abbrev_info));
  return (abbrev);
}

static struct die_info *
dwarf_alloc_die (void)
{
  struct die_info *die;

  die = (struct die_info *) xmalloc (sizeof (struct die_info));
  memset (die, 0, sizeof (struct die_info));
  return (die);
}


/* Macro support.  */


/* Return the full name of file number I in *LH's file name table.
   Use COMP_DIR as the name of the current directory of the
   compilation.  The result is allocated using xmalloc; the caller is
   responsible for freeing it.  */
static char *
file_full_name (int file, struct line_header *lh, const char *comp_dir)
{
  struct file_entry *fe = &lh->file_names[file - 1];
  
  if (IS_ABSOLUTE_PATH (fe->name))
    return xstrdup (fe->name);
  else
    {
      const char *dir;
      int dir_len;
      char *full_name;

      if (fe->dir_index)
        dir = lh->include_dirs[fe->dir_index - 1];
      else
        dir = comp_dir;

      if (dir)
        {
          dir_len = strlen (dir);
          full_name = xmalloc (dir_len + 1 + strlen (fe->name) + 1);
          strcpy (full_name, dir);
          full_name[dir_len] = '/';
          strcpy (full_name + dir_len + 1, fe->name);
          return full_name;
        }
      else
        return xstrdup (fe->name);
    }
}


static struct macro_source_file *
macro_start_file (int file, int line,
                  struct macro_source_file *current_file,
                  const char *comp_dir,
                  struct line_header *lh, struct objfile *objfile)
{
  /* The full name of this source file.  */
  char *full_name = file_full_name (file, lh, comp_dir);

  /* We don't create a macro table for this compilation unit
     at all until we actually get a filename.  */
  if (! pending_macros)
    pending_macros = new_macro_table (&objfile->objfile_obstack,
                                      objfile->macro_cache);

  if (! current_file)
    /* If we have no current file, then this must be the start_file
       directive for the compilation unit's main source file.  */
    current_file = macro_set_main (pending_macros, full_name);
  else
    current_file = macro_include (current_file, line, full_name);

  xfree (full_name);
              
  return current_file;
}


/* Copy the LEN characters at BUF to a xmalloc'ed block of memory,
   followed by a null byte.  */
static char *
copy_string (const char *buf, int len)
{
  char *s = xmalloc (len + 1);
  memcpy (s, buf, len);
  s[len] = '\0';

  return s;
}


static const char *
consume_improper_spaces (const char *p, const char *body)
{
  if (*p == ' ')
    {
      complaint (&symfile_complaints,
		 _("macro definition contains spaces in formal argument list:\n`%s'"),
		 body);

      while (*p == ' ')
        p++;
    }

  return p;
}


static void
parse_macro_definition (struct macro_source_file *file, int line,
                        const char *body)
{
  const char *p;

  /* The body string takes one of two forms.  For object-like macro
     definitions, it should be:

        <macro name> " " <definition>

     For function-like macro definitions, it should be:

        <macro name> "() " <definition>
     or
        <macro name> "(" <arg name> ( "," <arg name> ) * ") " <definition>

     Spaces may appear only where explicitly indicated, and in the
     <definition>.

     The Dwarf 2 spec says that an object-like macro's name is always
     followed by a space, but versions of GCC around March 2002 omit
     the space when the macro's definition is the empty string. 

     The Dwarf 2 spec says that there should be no spaces between the
     formal arguments in a function-like macro's formal argument list,
     but versions of GCC around March 2002 include spaces after the
     commas.  */


  /* Find the extent of the macro name.  The macro name is terminated
     by either a space or null character (for an object-like macro) or
     an opening paren (for a function-like macro).  */
  for (p = body; *p; p++)
    if (*p == ' ' || *p == '(')
      break;

  if (*p == ' ' || *p == '\0')
    {
      /* It's an object-like macro.  */
      int name_len = p - body;
      char *name = copy_string (body, name_len);
      const char *replacement;

      if (*p == ' ')
        replacement = body + name_len + 1;
      else
        {
	  dwarf2_macro_malformed_definition_complaint (body);
          replacement = body + name_len;
        }
      
      macro_define_object (file, line, name, replacement);

      xfree (name);
    }
  else if (*p == '(')
    {
      /* It's a function-like macro.  */
      char *name = copy_string (body, p - body);
      int argc = 0;
      int argv_size = 1;
      char **argv = xmalloc (argv_size * sizeof (*argv));

      p++;

      p = consume_improper_spaces (p, body);

      /* Parse the formal argument list.  */
      while (*p && *p != ')')
        {
          /* Find the extent of the current argument name.  */
          const char *arg_start = p;

          while (*p && *p != ',' && *p != ')' && *p != ' ')
            p++;

          if (! *p || p == arg_start)
	    dwarf2_macro_malformed_definition_complaint (body);
          else
            {
              /* Make sure argv has room for the new argument.  */
              if (argc >= argv_size)
                {
                  argv_size *= 2;
                  argv = xrealloc (argv, argv_size * sizeof (*argv));
                }

              argv[argc++] = copy_string (arg_start, p - arg_start);
            }

          p = consume_improper_spaces (p, body);

          /* Consume the comma, if present.  */
          if (*p == ',')
            {
              p++;

              p = consume_improper_spaces (p, body);
            }
        }

      if (*p == ')')
        {
          p++;

          if (*p == ' ')
            /* Perfectly formed definition, no complaints.  */
            macro_define_function (file, line, name,
                                   argc, (const char **) argv, 
                                   p + 1);
          else if (*p == '\0')
            {
              /* Complain, but do define it.  */
	      dwarf2_macro_malformed_definition_complaint (body);
              macro_define_function (file, line, name,
                                     argc, (const char **) argv, 
                                     p);
            }
          else
            /* Just complain.  */
	    dwarf2_macro_malformed_definition_complaint (body);
        }
      else
        /* Just complain.  */
	dwarf2_macro_malformed_definition_complaint (body);

      xfree (name);
      {
        int i;

        for (i = 0; i < argc; i++)
          xfree (argv[i]);
      }
      xfree (argv);
    }
  else
    dwarf2_macro_malformed_definition_complaint (body);
}


static void
dwarf_decode_macros (struct line_header *lh, unsigned int offset,
                     char *comp_dir, bfd *abfd,
                     struct dwarf2_cu *cu)
{
  char *mac_ptr, *mac_end;
  struct macro_source_file *current_file = 0;

  if (dwarf2_per_objfile->macinfo_buffer == NULL)
    {
      complaint (&symfile_complaints, _("missing .debug_macinfo section"));
      return;
    }

  mac_ptr = dwarf2_per_objfile->macinfo_buffer + offset;
  mac_end = dwarf2_per_objfile->macinfo_buffer
    + dwarf2_per_objfile->macinfo_size;

  for (;;)
    {
      enum dwarf_macinfo_record_type macinfo_type;

      /* Do we at least have room for a macinfo type byte?  */
      if (mac_ptr >= mac_end)
        {
	  dwarf2_macros_too_long_complaint ();
          return;
        }

      macinfo_type = read_1_byte (abfd, mac_ptr);
      mac_ptr++;

      /* A zero macinfo type indicates the end of the macro information.  */
      if (macinfo_type == 0)
        return;

      switch (macinfo_type)
        {
        case DW_MACINFO_define:
        case DW_MACINFO_undef:
          {
	    /* APPLE LOCAL change type to unsigned to avoid type warnings. */
            unsigned bytes_read;
            int line;
            char *body;

            line = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;
            body = read_string (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;

            if (! current_file)
	      complaint (&symfile_complaints,
			 _("debug info gives macro %s outside of any file: %s"),
			 macinfo_type ==
			 DW_MACINFO_define ? "definition" : macinfo_type ==
			 DW_MACINFO_undef ? "undefinition" :
			 "something-or-other", body);
            else
              {
                if (macinfo_type == DW_MACINFO_define)
                  parse_macro_definition (current_file, line, body);
                else if (macinfo_type == DW_MACINFO_undef)
                  macro_undef (current_file, line, body);
              }
          }
          break;

        case DW_MACINFO_start_file:
          {
	    /* APPLE LOCAL change type to unsigned to avoid type warnings. */
            unsigned bytes_read;
            int line, file;

            line = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;
            file = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;

            current_file = macro_start_file (file, line,
                                             current_file, comp_dir,
                                             lh, cu->objfile);
          }
          break;

        case DW_MACINFO_end_file:
          if (! current_file)
	    complaint (&symfile_complaints,
		       _("macro debug info has an unmatched `close_file' directive"));
          else
            {
              current_file = current_file->included_by;
              if (! current_file)
                {
                  enum dwarf_macinfo_record_type next_type;

                  /* GCC circa March 2002 doesn't produce the zero
                     type byte marking the end of the compilation
                     unit.  Complain if it's not there, but exit no
                     matter what.  */

                  /* Do we at least have room for a macinfo type byte?  */
                  if (mac_ptr >= mac_end)
                    {
		      dwarf2_macros_too_long_complaint ();
                      return;
                    }

                  /* We don't increment mac_ptr here, so this is just
                     a look-ahead.  */
                  next_type = read_1_byte (abfd, mac_ptr);
                  if (next_type != 0)
		    complaint (&symfile_complaints,
			       _("no terminating 0-type entry for macros in `.debug_macinfo' section"));

                  return;
                }
            }
          break;

        case DW_MACINFO_vendor_ext:
          {
	    /* APPLE LOCAL change type to unsigned to avoid type warnings. */
            unsigned bytes_read;
            int constant;
            char *string;

            constant = read_unsigned_leb128 (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;
            string = read_string (abfd, mac_ptr, &bytes_read);
            mac_ptr += bytes_read;

            /* We don't recognize any vendor extensions.  */
          }
          break;
        }
    }
}

/* Check if the attribute's form is a DW_FORM_block*
   if so return true else false. */
static int
attr_form_is_block (struct attribute *attr)
{
  return (attr == NULL ? 0 :
      attr->form == DW_FORM_block1
      || attr->form == DW_FORM_block2
      || attr->form == DW_FORM_block4
      || attr->form == DW_FORM_block);
}

static void
dwarf2_symbol_mark_computed (struct attribute *attr, struct symbol *sym,
			     struct dwarf2_cu *cu)
{
  if (attr->form == DW_FORM_data4 || attr->form == DW_FORM_data8)
    {
      struct dwarf2_loclist_baton *baton;

      baton = obstack_alloc (&cu->objfile->objfile_obstack,
			     sizeof (struct dwarf2_loclist_baton));
      baton->objfile = cu->objfile;

      /* APPLE LOCAL: We'll need to translate addresses for the
         debug-info-in-.o-files case */
      baton->addr_map = cu->addr_map;

      /* We don't know how long the location list is, but make sure we
	 don't run off the edge of the section.  */
      baton->size = dwarf2_per_objfile->loc_size - DW_UNSND (attr);
      /* APPLE LOCAL add cast to avoid type warning.  */
      baton->data = (gdb_byte *) dwarf2_per_objfile->loc_buffer + 
                                 DW_UNSND (attr);
      baton->base_address_untranslated = cu->header.base_address_untranslated;

      if (cu->header.base_known == 0)
	complaint (&symfile_complaints,
		   _("Location list used without specifying the CU base address."));

      SYMBOL_OPS (sym) = &dwarf2_loclist_funcs;
      SYMBOL_LOCATION_BATON (sym) = baton;
    }
  else
    {
      struct dwarf2_locexpr_baton *baton;

      baton = obstack_alloc (&cu->objfile->objfile_obstack,
			     sizeof (struct dwarf2_locexpr_baton));
      baton->objfile = cu->objfile;

      /* APPLE LOCAL: We'll need to translate addresses for the
         debug-info-in-.o-files case */
      baton->addr_map = cu->addr_map;

      if (attr_form_is_block (attr))
	{
	  /* Note that we're just copying the block's data pointer
	     here, not the actual data.  We're still pointing into the
	     info_buffer for SYM's objfile; right now we never release
	     that buffer, but when we do clean up properly this may
	     need to change.  */
	  baton->size = DW_BLOCK (attr)->size;
	  /* APPLE LOCAL add cast to avoid type warnings. */
	  baton->data = (gdb_byte *) DW_BLOCK (attr)->data;
	}
      else
	{
	  dwarf2_invalid_attrib_class_complaint ("location description",
						 SYMBOL_NATURAL_NAME (sym));
	  baton->size = 0;
	  baton->data = NULL;
	}
      
      SYMBOL_OPS (sym) = &dwarf2_locexpr_funcs;
      SYMBOL_LOCATION_BATON (sym) = baton;
    }
}

/* Locate the compilation unit from CU's objfile which contains the
   DIE at OFFSET.  Raises an error on failure.  */

static struct dwarf2_per_cu_data *
dwarf2_find_containing_comp_unit (unsigned long offset,
				  struct objfile *objfile)
{
  struct dwarf2_per_cu_data *this_cu;
  int low, high;

  low = 0;
  high = dwarf2_per_objfile->n_comp_units - 1;
  while (high > low)
    {
      int mid = low + (high - low) / 2;
      if (dwarf2_per_objfile->all_comp_units[mid]->offset >= offset)
	high = mid;
      else
	low = mid + 1;
    }
  gdb_assert (low == high);
  if (dwarf2_per_objfile->all_comp_units[low]->offset > offset)
    {
      if (low == 0)
	error (_("Dwarf Error: could not find partial DIE containing "
	       "offset 0x%lx [in module %s]"),
	       (long) offset, bfd_get_filename (objfile->obfd));

      gdb_assert (dwarf2_per_objfile->all_comp_units[low-1]->offset <= offset);
      return dwarf2_per_objfile->all_comp_units[low-1];
    }
  else
    {
      this_cu = dwarf2_per_objfile->all_comp_units[low];
      if (low == dwarf2_per_objfile->n_comp_units - 1
	  && offset >= this_cu->offset + this_cu->length)
	error (_("invalid dwarf2 offset %ld"), offset);
      gdb_assert (offset < this_cu->offset + this_cu->length);
      return this_cu;
    }
}

/* Locate the compilation unit from OBJFILE which is located at exactly
   OFFSET.  Raises an error on failure.  */

static struct dwarf2_per_cu_data *
dwarf2_find_comp_unit (unsigned long offset, struct objfile *objfile)
{
  struct dwarf2_per_cu_data *this_cu;
  this_cu = dwarf2_find_containing_comp_unit (offset, objfile);
  if (this_cu->offset != offset)
    error (_("no compilation unit with offset %ld."), offset);
  return this_cu;
}

/* Release one cached compilation unit, CU.  We unlink it from the tree
   of compilation units, but we don't remove it from the read_in_chain;
   the caller is responsible for that.  */

static void
free_one_comp_unit (void *data)
{
  struct dwarf2_cu *cu = data;

  if (cu->per_cu != NULL)
    cu->per_cu->cu = NULL;
  cu->per_cu = NULL;

  obstack_free (&cu->comp_unit_obstack, NULL);
  if (cu->dies)
    free_die_list (cu->dies);

  xfree (cu);
}

/* This cleanup function is passed the address of a dwarf2_cu on the stack
   when we're finished with it.  We can't free the pointer itself, but be
   sure to unlink it from the cache.  Also release any associated storage
   and perform cache maintenance.

   Only used during partial symbol parsing.  */

static void
free_stack_comp_unit (void *data)
{
  struct dwarf2_cu *cu = data;

  obstack_free (&cu->comp_unit_obstack, NULL);
  cu->partial_dies = NULL;

  if (cu->per_cu != NULL)
    {
      /* This compilation unit is on the stack in our caller, so we
	 should not xfree it.  Just unlink it.  */
      cu->per_cu->cu = NULL;
      cu->per_cu = NULL;

      /* If we had a per-cu pointer, then we may have other compilation
	 units loaded, so age them now.  */
      age_cached_comp_units ();
    }
}

/* Free all cached compilation units.  */

static void
free_cached_comp_units (void *data)
{
  struct dwarf2_per_cu_data *per_cu, **last_chain;

  per_cu = dwarf2_per_objfile->read_in_chain;
  last_chain = &dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      struct dwarf2_per_cu_data *next_cu;

      next_cu = per_cu->cu->read_in_chain;

      free_one_comp_unit (per_cu->cu);
      *last_chain = next_cu;

      per_cu = next_cu;
    }
}

/* Increase the age counter on each cached compilation unit, and free
   any that are too old.  */

static void
age_cached_comp_units (void)
{
  struct dwarf2_per_cu_data *per_cu, **last_chain;

  dwarf2_clear_marks (dwarf2_per_objfile->read_in_chain);
  per_cu = dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      per_cu->cu->last_used ++;
      if (per_cu->cu->last_used <= dwarf2_max_cache_age)
	dwarf2_mark (per_cu->cu);
      per_cu = per_cu->cu->read_in_chain;
    }

  per_cu = dwarf2_per_objfile->read_in_chain;
  last_chain = &dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      struct dwarf2_per_cu_data *next_cu;

      next_cu = per_cu->cu->read_in_chain;

      if (!per_cu->cu->mark)
	{
	  free_one_comp_unit (per_cu->cu);
	  *last_chain = next_cu;
	}
      else
	last_chain = &per_cu->cu->read_in_chain;

      per_cu = next_cu;
    }
}

/* Remove a single compilation unit from the cache.  */

static void
free_one_cached_comp_unit (void *target_cu)
{
  struct dwarf2_per_cu_data *per_cu, **last_chain;

  per_cu = dwarf2_per_objfile->read_in_chain;
  last_chain = &dwarf2_per_objfile->read_in_chain;
  while (per_cu != NULL)
    {
      struct dwarf2_per_cu_data *next_cu;

      next_cu = per_cu->cu->read_in_chain;

      if (per_cu->cu == target_cu)
	{
	  free_one_comp_unit (per_cu->cu);
	  *last_chain = next_cu;
	  break;
	}
      else
	last_chain = &per_cu->cu->read_in_chain;

      per_cu = next_cu;
    }
}

/* A pair of DIE offset and GDB type pointer.  We store these
   in a hash table separate from the DIEs, and preserve them
   when the DIEs are flushed out of cache.  */

struct dwarf2_offset_and_type
{
  unsigned int offset;
  struct type *type;
};

/* Hash function for a dwarf2_offset_and_type.  */

static hashval_t
offset_and_type_hash (const void *item)
{
  const struct dwarf2_offset_and_type *ofs = item;
  return ofs->offset;
}

/* Equality function for a dwarf2_offset_and_type.  */

static int
offset_and_type_eq (const void *item_lhs, const void *item_rhs)
{
  const struct dwarf2_offset_and_type *ofs_lhs = item_lhs;
  const struct dwarf2_offset_and_type *ofs_rhs = item_rhs;
  return ofs_lhs->offset == ofs_rhs->offset;
}

/* Set the type associated with DIE to TYPE.  Save it in CU's hash
   table if necessary.  */

static void
set_die_type (struct die_info *die, struct type *type, struct dwarf2_cu *cu)
{
  struct dwarf2_offset_and_type **slot, ofs;

  die->type = type;

  if (cu->per_cu == NULL)
    return;

  if (cu->per_cu->type_hash == NULL)
    cu->per_cu->type_hash
      = htab_create_alloc_ex (cu->header.length / 24,
			      offset_and_type_hash,
			      offset_and_type_eq,
			      NULL,
			      &cu->objfile->objfile_obstack,
			      hashtab_obstack_allocate,
			      dummy_obstack_deallocate);

  ofs.offset = die->offset;
  ofs.type = type;
  slot = (struct dwarf2_offset_and_type **)
    htab_find_slot_with_hash (cu->per_cu->type_hash, &ofs, ofs.offset, INSERT);
  *slot = obstack_alloc (&cu->objfile->objfile_obstack, sizeof (**slot));
  **slot = ofs;
}

/* Find the type for DIE in TYPE_HASH, or return NULL if DIE does not
   have a saved type.  */

static struct type *
get_die_type (struct die_info *die, htab_t type_hash)
{
  struct dwarf2_offset_and_type *slot, ofs;

  ofs.offset = die->offset;
  slot = htab_find_with_hash (type_hash, &ofs, ofs.offset);
  if (slot)
    return slot->type;
  else
    return NULL;
}

/* Restore the types of the DIE tree starting at START_DIE from the hash
   table saved in CU.  */

static void
reset_die_and_siblings_types (struct die_info *start_die, struct dwarf2_cu *cu)
{
  struct die_info *die;

  if (cu->per_cu->type_hash == NULL)
    return;

  for (die = start_die; die != NULL; die = die->sibling)
    {
      die->type = get_die_type (die, cu->per_cu->type_hash);
      if (die->child != NULL)
	reset_die_and_siblings_types (die->child, cu);
    }
}

/* Set the mark field in CU and in every other compilation unit in the
   cache that we must keep because we are keeping CU.  */

/* Add a dependence relationship from CU to REF_PER_CU.  */

static void
dwarf2_add_dependence (struct dwarf2_cu *cu,
		       struct dwarf2_per_cu_data *ref_per_cu)
{
  void **slot;

  if (cu->dependencies == NULL)
    cu->dependencies
      = htab_create_alloc_ex (5, htab_hash_pointer, htab_eq_pointer,
			      NULL, &cu->comp_unit_obstack,
			      hashtab_obstack_allocate,
			      dummy_obstack_deallocate);

  slot = htab_find_slot (cu->dependencies, ref_per_cu, INSERT);
  if (*slot == NULL)
    *slot = ref_per_cu;
}

/* Set the mark field in CU and in every other compilation unit in the
   cache that we must keep because we are keeping CU.  */

static int
dwarf2_mark_helper (void **slot, void *data)
{
  struct dwarf2_per_cu_data *per_cu;

  per_cu = (struct dwarf2_per_cu_data *) *slot;
  if (per_cu->cu->mark)
    return 1;
  per_cu->cu->mark = 1;

  if (per_cu->cu->dependencies != NULL)
    htab_traverse (per_cu->cu->dependencies, dwarf2_mark_helper, NULL);

  return 1;
}

static void
dwarf2_mark (struct dwarf2_cu *cu)
{
  if (cu->mark)
    return;
  cu->mark = 1;
  if (cu->dependencies != NULL)
    htab_traverse (cu->dependencies, dwarf2_mark_helper, NULL);
}

static void
dwarf2_clear_marks (struct dwarf2_per_cu_data *per_cu)
{
  while (per_cu)
    {
      per_cu->cu->mark = 0;
      per_cu = per_cu->cu->read_in_chain;
    }
}

/* Allocation function for the libiberty hash table which uses an
   obstack.  */

static void *
hashtab_obstack_allocate (void *data, size_t size, size_t count)
{
  unsigned int total = size * count;
  void *ptr = obstack_alloc ((struct obstack *) data, total);
  memset (ptr, 0, total);
  return ptr;
}

/* Trivial deallocation function for the libiberty splay tree and hash
   table - don't deallocate anything.  Rely on later deletion of the
   obstack.  */

static void
dummy_obstack_deallocate (void *object, void *data)
{
  return;
}

/* Trivial hash function for partial_die_info: the hash value of a DIE
   is its offset in .debug_info for this objfile.  */

static hashval_t
partial_die_hash (const void *item)
{
  const struct partial_die_info *part_die = item;
  return part_die->offset;
}

/* Trivial comparison function for partial_die_info structures: two DIEs
   are equal if they have the same offset.  */

static int
partial_die_eq (const void *item_lhs, const void *item_rhs)
{
  const struct partial_die_info *part_die_lhs = item_lhs;
  const struct partial_die_info *part_die_rhs = item_rhs;
  return part_die_lhs->offset == part_die_rhs->offset;
}

static struct cmd_list_element *set_dwarf2_cmdlist;
static struct cmd_list_element *show_dwarf2_cmdlist;

static void
set_dwarf2_cmd (char *args, int from_tty)
{
  help_list (set_dwarf2_cmdlist, "maintenance set dwarf2 ", -1, gdb_stdout);
}

static void
show_dwarf2_cmd (char *args, int from_tty)
{ 
  cmd_show_list (show_dwarf2_cmdlist, from_tty, "");
}

void _initialize_dwarf2_read (void);

void
_initialize_dwarf2_read (void)
{
  dwarf2_objfile_data_key = register_objfile_data ();

  add_prefix_cmd ("dwarf2", class_maintenance, set_dwarf2_cmd, _("\
Set DWARF 2 specific variables.\n\
Configure DWARF 2 variables such as the cache size"),
                  &set_dwarf2_cmdlist, "maintenance set dwarf2 ",
                  0/*allow-unknown*/, &maintenance_set_cmdlist);

  add_prefix_cmd ("dwarf2", class_maintenance, show_dwarf2_cmd, _("\
Show DWARF 2 specific variables\n\
Show DWARF 2 variables such as the cache size"),
                  &show_dwarf2_cmdlist, "maintenance show dwarf2 ",
                  0/*allow-unknown*/, &maintenance_show_cmdlist);

  /* APPLE LOCAL */
  add_setshow_zinteger_cmd ("debugmap", class_maintenance, &debug_debugmap, _("\
Set DWARF debug map debugging."), _("\
Show DWARF debug map debugging."), _("\
When non-zero, debug map specific debugging is enabled."),
                            NULL,
                            show_debug_debugmap,
                            &setdebuglist, &showdebuglist);

  add_setshow_zinteger_cmd ("max-cache-age", class_obscure,
			    &dwarf2_max_cache_age, _("\
Set the upper bound on the age of cached dwarf2 compilation units."), _("\
Show the upper bound on the age of cached dwarf2 compilation units."), _("\
A higher limit means that cached compilation units will be stored\n\
in memory longer, and more total memory will be used.  Zero disables\n\
caching, which can slow down startup."),
			    NULL,
			    show_dwarf2_max_cache_age,
			    &set_dwarf2_cmdlist,
			    &show_dwarf2_cmdlist);

  /* APPLE LOCAL begin subroutine inlining  */
  add_setshow_boolean_cmd ("inlined-stepping", class_support, 
			   &dwarf2_allow_inlined_stepping,
	      _("Set the ability to maneuver through inlined function calls as if they were normal calls."),
	      _("Show the ability to maneuver through inlined function calls as if they were normal calls."),
			   NULL, NULL, NULL, &setlist, &showlist);

  add_setshow_boolean_cmd ("inlined-stepping", class_obscure, 
			   &dwarf2_debug_inlined_stepping,
	      _("Set the extra information for debugging gdb's maneuvering through inlined function calls."),
	      _("Show the extra information for debugging gdb's maneuvering through inlined function calls."),
			   NULL, NULL, NULL, &setdebuglist, &showdebuglist);
  /* APPLE LOCAL end subroutine inlining  */
}

/* APPLE LOCAL begin dwarf repository  */
/* NOTE:  Everything from here to the end of the file is APPLE LOCAL  */
/* *********************** REPOSITORY STUFF STARTS HERE *********************** */
/*
  This "section" contains several sub-sections:

  1. Red Black Trees.  This sub-section contains code for defining, creating 
  and manipulating red-black trees, which is how we efficiently keep track of 
  (and access) types already retrieved and decoded from a repository.

  2. Global repositories and data structures.  This sub-section contains code
  for tracking, controlling, and manipulating the dwarf types repositories.
  The code in this section is more high-level, dealing with stuff at the
  level of entire repositories.

  3. Accessing the sqlite3 database and decoding the dies.  This contains the
  low-level database access functions.  It includes the code for reading and
  decoding the dies, and translating the dwarf type information into gdb
  type structures.

*/

/* APPLE LOCAL begin red-black trees, part 2.  */
/* Begin repository sub-section 1: Red-black trees.  This section  implements
   the red-black tree algorithms from "Introduction to Algorithms" by Cormen,
   Leiserson, and Rivest.  A red-black tree is a 'semi-balanced' binary tree,
   where by semi-balanced it means that for any node in the tree, the height of
   one sub-tree is guaranteed to never be greater than twice the height of the
   other sub-tree.  Each node is colored either red or black, and a parent must
   never be the same color as its children.

   The following types, used by the functions in this section, are defined
   near the beginning of this file  (look for the label "red-black trees, 
   part 1"):

        enum rb_tree_colors;  (type)
        struct rb_tree_node;  (type)

   This section defines the following functions:

        rb_tree_find_node (function)
	rb_tree_find_node_all_keys (function)
	rb_tree_find_and_remove_node (function)
	left_rotate       (function)
	right_rotate      (function)
	plain_tree_insert (function)
	rb_tree_insert    (function)
	rb_tree_remove_node (function)
	rb_tree_minimun (function)
	rb_tree_successor (function)
	rb_delete_fixup (function)
	rb_tree_remove_node (function)
*/

/* This function searches the tree ROOT recursively until it
   finds a node with the key KEY, which it returns.  If there
   is no such node in the tree it returns NULL.  */

struct rb_tree_node *
rb_tree_find_node (struct rb_tree_node *root, long long key, int secondary_key)
{
  if (!root)
    return NULL;

  if (key == root->key)
    {
      if (secondary_key < 0)
	return root;
      else if (secondary_key < root->secondary_key)
	return rb_tree_find_node (root->left, key, secondary_key);
      else
	return rb_tree_find_node (root->right, key, secondary_key);
    }
  else if (key < root->key)
    return rb_tree_find_node (root->left, key, secondary_key);
  else
    return rb_tree_find_node (root->right, key, secondary_key);
}


/* This function searches the tree ROOT recursively until it
   finds a node with the key KEY, secondary key SECONDARY_KEY and third key
   THIRD_KEY, which it returns.  If there is no such node in the tree it 
   returns NULL.  */

struct rb_tree_node *
rb_tree_find_node_all_keys (struct rb_tree_node *root, long long key, 
			    int secondary_key, long long third_key)
{
  if (!root)
    return NULL;

  if (key == root->key)
    {
      if (secondary_key < root->secondary_key)
	return rb_tree_find_node_all_keys (root->left, key, secondary_key,
					   third_key);
      else if (secondary_key > root->secondary_key)
	return rb_tree_find_node_all_keys (root->right, key, secondary_key, 
					   third_key);
      else /* (secondary_key == root->secondary_key)  */
	{
	  if (third_key == root->third_key)
	    return root;
	  else if (third_key < root->third_key)
	    return rb_tree_find_node_all_keys (root->left, key, secondary_key,
					       third_key);
	  else
	    return rb_tree_find_node_all_keys (root->right, key, secondary_key,
					       third_key);
	}
    }
  else if (key < root->key)
    return rb_tree_find_node_all_keys (root->left, key, secondary_key, 
				       third_key);
  else
    return rb_tree_find_node_all_keys (root->right, key, secondary_key,
				       third_key);
}


/* This function, given a red-black tree (ROOT), a current position in the
   tree (CUR_NODE), a primary key (KEY), and a SECONDARY_KEY,  searches for
   a node in the tree that matches the keys given, removes the node from
   the tree, and returns a copy of the node.  */

static struct rb_tree_node *
rb_tree_find_and_remove_node (struct rb_tree_node **root, 
			      struct rb_tree_node *cur_node, long long key, 
			      int secondary_key)
{
  struct rb_tree_node *result;

  if (!cur_node)
    return NULL;

  if (key == cur_node->key)
    {
      if (cur_node->left
	  && cur_node->left->key == key)
	return rb_tree_find_and_remove_node (root, cur_node->left, key, 
					     secondary_key);
     
      result = rb_tree_remove_node (root, cur_node);
      return result;
    }
  else if (key < cur_node->key)
    return rb_tree_find_and_remove_node (root, cur_node->left, key, 
					 secondary_key);
  else
    return rb_tree_find_and_remove_node (root, cur_node->right, key, 
					 secondary_key);
}

/* Given a red-black tree NODE, return the node in the tree that has the
   smallest "value".  */

static struct rb_tree_node *
rb_tree_minimum (struct rb_tree_node *node)
{
  while (node->left)
    node = node->left;
  return  node;
}

/* Given a NODE in a red-black tree, this function returns the
   descendant of that node in the tree that has the smallest "value"
   that is greater than the "value" of NODE.  */

static struct rb_tree_node *
rb_tree_successor (struct rb_tree_node *node)
{
  struct rb_tree_node *y;
  if (node->right)
    return rb_tree_minimum (node->right);
  else
    {
      y = node->parent;
      while (y && node == y->right)
	{
	  node = y;
	  y = node->parent;
	}
    }
  return y;
}

/* This function takes a red-black tree (ROOT) that has had a node
   removed at X, and restores the red-black properties to the tree. 
   It uses the algorithm from pate 274 of the Corman et. al. textbook.  */

static void
rb_delete_fixup (struct rb_tree_node **root, struct rb_tree_node *x)
{
  struct rb_tree_node *w;

  /* On entering this function, the tree is not correct.  'x' is carrying
     the "blackness" of the node that was deleted as well as its own color.
     If x is red we can just color it black and be done.  But if 'x' is black
     we need to do some re-coloring and rotating to push the extra blackness
     up the tree (once it reaches the root of the tree everything is properly
     balanced again).

     'w' is the sibling in the tree of 'x'.  'w' must be non-NULL, otherwise
     the tree was messed up to begin with. 

     For details about the particular cases mentioned below, see the
     algorithm explanation in the book.  */

  while (x != *root
	 && x->color == BLACK)
    {
      if (x == x->parent->left)  /* x LEFT child of its parent.  */
	{
	  w = x->parent->right;

	  /* Case 1:  w is RED.  Color it black and do a rotation,
	     converting this to case 2, 3 or 4.  */

	  if (w->color == RED)   /* Case 1 */
	    {
	      w->color = BLACK;
	      x->parent->color = RED;
	      left_rotate (root, x->parent);
	      w = x->parent->right;
	    }

	  /* Case 2: Both of w's children are BLACK (where NULL counts
	     as BLACK).  In this case, color w red, and push the blackness
	     up the tree one node, making what used to be x's parent be 
	     the new x (and return to top of loop).  */

	  if ((!w->left || w->left->color == BLACK)   /* Case 2  */
	      && (!w->right || w->right->color == BLACK))
	    {
	      w->color = RED;
	      x = x->parent;
	    }
	  else  /* Cases 3 & 4 (w is black, one of its children is red)  */
	    {

	      /* Case 3: w's right child is black.  */

	      if (!w->right || w->right->color == BLACK)  /* Case 3  */
		{
		  if (w->left)
		    w->left->color = BLACK;
		  w->color = RED;
		  right_rotate (root, w);
		  w = x->parent->right;
		}

	      /* Case 4  */
	      
	      w->color = x->parent->color;
	      x->parent->color = BLACK;
	      if (w->right)
		w->right->color = BLACK;
	      left_rotate (root, x->parent);
	      x = *root;
	    }
	}
      else  /* x is the RIGHT child of its parent.  */
	{
	  w = x->parent->left;

	  /* Case 1:  w is RED.  Color it black and do a rotation,
	     converting this to case 2, 3 or 4.  */

	  if (w->color == RED)
	    {
	      w->color = BLACK;
	      x->parent->color = RED;
	      right_rotate (root, x->parent);
	      w = x->parent->left;
	    }

	  /* Case 2: Both of w's children are BLACK (where NULL counts
	     as BLACK).  In this case, color w red, and push the blackness
	     up the tree one node, making what used to be x's parent be 
	     the new x (and return to top of loop).  */

	  if ((!w->right || w->right->color == BLACK)
	      && (!w->left || w->left->color == BLACK))
	    {
	      w->color = RED;
	      x = x->parent;
	    }
	  else /* Cases 3 & 4 (w is black, one of its children is red)  */
	    {

	      /* Case 3: w's left  child is black.  */

	      if (!w->left || w->left->color == BLACK)
		{
		  if (w->right)
		    w->right->color = BLACK;
		  w->color = RED;
		  left_rotate (root, w);
		  w = x->parent->left;
		}

	      /* Case 4  */

	      w->color = x->parent->color;
	      x->parent->color = BLACK;
	      if (w->left)
		w->left->color = BLACK;
	      right_rotate (root, x->parent);
	      x = *root;
	    }
	}
    }
  x->color = BLACK;
}

/* Red-Black tree delete node:  Given a tree (ROOT) and a node in the tree
   (NODE), remove the NODE from the TREE, keeping the tree properly balanced
   and colored, and return a copy of the removed node.  This function uses
   the algorithm on page 273 of the Corman, Leiserson and Rivest textbook
   mentioned previously. 

   First we make a copy of the node to be deleted, so we can return the
   data from that node.  We need to make a copy rather than returning the
   node because of the way some tree deletions are handled (see the next
   paragraph).

   The basic idea is: If NODE has no children, just remove it.  If
   NODE has one child, splice out NODE (make its parent point to its
   child).  The tricky part is when NODE has two children.  In that
   case we find the successor to NODE in NODE's right subtree (the
   "smallest" node whose "value" is larger than the "value" of node,
   where "smallest" and "value" are determined by the nodes' keys).
   The successor is guaranteed to have ony one child.  Therefore we
   first splice out the successor (make its parent point to its
   child).  Next we *overwrite the keys and data* of NODE with the
   keys and data of its successor node.  The net effect of this is
   that NODE has been replaced by its successor, and NODE is no longer
   in the tree.

   Finally, we may need to re-color or re-balance a portion of the tree.
 */


static struct rb_tree_node *
rb_tree_remove_node (struct rb_tree_node **root, struct rb_tree_node *node)
{
  struct rb_tree_node *deleted_node;
  struct rb_tree_node *z = node;
  struct rb_tree_node *x;
  struct rb_tree_node *y;
  struct rb_tree_node *y_parent;
  int x_child_pos;  /* 0 == left child; 1 == right child  */

  if (dwarf2_debug_inlined_stepping)
    gdb_assert (verify_rb_tree (*root));

  /* Make a copy of the node to be "deleted" from the tree.  The copy is what
     will be returned by this function.  */

  deleted_node = (struct rb_tree_node *) xmalloc (sizeof (struct rb_tree_node));
  deleted_node->key = node->key;
  deleted_node->secondary_key = node->secondary_key;
  deleted_node->third_key = node->third_key;
  deleted_node->data = node->data;
  deleted_node->color = node->color;
  deleted_node->left = NULL;
  deleted_node->right = NULL;
  deleted_node->parent = NULL;

  /* Now proceed to 'delete' the node ("z") from the tree.  */
  

  /* Removing a node with one child from a red-black tree is not too
     difficult, but removing a node with two children IS difficult.
     Therefore if the node to be removed has at most one child, it
     will be removed directly.

     If "z" has TWO children, we will not actually remove node "z"
     from the tree; instead we will find z's successor in the tree
     (which is guaranteed to have at most one child), remove THAT node
     from the tree, and overwrite the keys and data value in z with
     the keys and data value in z's successor.  */

  /* 'y' will point to the node that actually gets removed from the
     tree.  If 'z' has at most one child, 'y' will point to the same
     node as 'z'.  If 'z' has two children, 'y' will point to 'z's
     successor in the tree.  */
  
  if (!z->left || !z->right)
    y = z;
  else
    y = rb_tree_successor (z);

  /* 'y' is now guaranteed to have at most one child.  Make 'x' point
     to that child.  If y has no children, x will be NULL.  */

  if (y->left)
    x = y->left;
  else
    x = y->right;

  /* Make y's parent be x's parent (it used to be x's grandparent).  */

  if (x)
    x->parent = y->parent;

  y_parent = y->parent;

  /* Make 'x' be the child of y's parent that y used to be.  */

  if (!y->parent)
    *root = x;
  else if (y == y->parent->left)
    {
      y->parent->left = x;
      x_child_pos = 0;
    }
  else
    {
      y->parent->right = x;
      x_child_pos = 1;
    }

  /* If y is not the same as 'node', then y is the successor to
     'node'; since node has two children and cannot actually be
     removed from the tree, and since y has now been spliced out of
     the tree, overwrite node's keys and data with y's keys and data.
     (This is why we made a copy of node above, to be the return
     value.)  */

  if (y != node)
    {
      node->key = y->key;
      node->secondary_key = y->secondary_key;
      node->third_key = y->third_key;
      node->data = y->data;
    }

  /* If the color of 'y' was RED, then the properties of the red-black
     tree have not been violated by removing it so nothing else needs
     to be done.  But if the color of y was BLACK, then we need to fix
     up the tree, starting at 'x' (which now occupies the position
     where y was removed).  */

  if (y->color == BLACK && x == NULL && y_parent != NULL)
    {
      struct rb_tree_node *w;

      /* Since x is NULL, we can't call rb_delete_fixup directly (it
	 assumes a non-NULL x.  Therefore we do the first iteration of
	 the while loop from that function here.  At the end of this
	 first iteration, x is no longer NULL, so we can call the
	 function on the new non-NULL x.  */

      if (x_child_pos == 0)
	w = y_parent->right;
      else
	w = y_parent->left;

      if (!w)
	x = *root;
      else
	{
	  if (w->color == RED)
	    {
	      w->color = BLACK;
	      y_parent->color = RED;
	      if (x_child_pos == 0)
		{
		  left_rotate (root, y_parent);
		  w = y_parent->right;
		}
	      else
		{
		  right_rotate (root, y_parent);
		  w = y_parent->left;
		}
	    }
	  
	  if ((!w->left || w->left->color == BLACK)
	      && (!w->right || w->right->color == BLACK))
	    {
	      w->color = RED;
	      x = y_parent;
	    }
	  else if (x_child_pos == 0)
	    {
	      if (!w->right || w->right->color == BLACK)
		{
		  if (w->left)
		    w->left->color = BLACK;
		  w->color = RED;
		  right_rotate (root, w);
		  w = y_parent->right;
		}
	      
	      w->color = y_parent->color;
	      y_parent->color = BLACK;
	      if (w->right)
		w->right->color = BLACK;
	      left_rotate (root, y_parent);
	      x = *root;
	    }
	  else
	    {
	      if (!w->left || w->left->color == BLACK)
		{
		  if (w->right)
		    w->right->color = BLACK;
		  w->color = RED;
		  left_rotate (root, w);
		  w = y_parent->left;
		}
	      
	      w->color = y_parent->color;
	      y_parent->color = BLACK;
	      if (w->left)
		w->left->color = BLACK;
	      right_rotate (root, y_parent);
	      x = *root;
	    }
	}
    }

  if (y->color == BLACK && x)
    rb_delete_fixup (root, x);

  if (dwarf2_debug_inlined_stepping)
    gdb_assert (verify_rb_tree (*root));

  return deleted_node;
}

/* Given a (red-black) tree structure like the one on the left, 
   perform a "left-rotation" so that the result is like the one
   on the right (parent, x, and y are individual tree nodes; a, b,
   and c represent sub-trees, possibly null):

     parent                            parent
        |                                |
        x                                y
       / \               ==>>           / \
     a    y                            x   c
         / \                          / \
        b   c                        a   b

*/

static void
left_rotate (struct rb_tree_node **root, struct rb_tree_node *x)
{
  struct rb_tree_node *y;
  
  if (!x->right)
    return;

  y = x->right;

  x->right = y->left;
  if (y->left != NULL)
    y->left->parent = x;

  y->parent = x->parent;

  if (x->parent == NULL)
    *root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else
    x->parent->right = y;

  y->left = x;
  x->parent = y;
}

/* Given a (red-black) tree structure like the one on the left, 
   perform a "right-rotation" so that the result is like the one
   on the right (parent, x, and y are individual tree nodes; a, b,
   and c represent sub-trees, possibly null):

     parent                            parent
        |                                |
        x                                y
       / \               ==>>           / \
     y    c                            a   x
    / \                                   / \
   a   b                                 b   c

*/

static void
right_rotate (struct rb_tree_node **root, struct rb_tree_node *x)
{
  struct rb_tree_node *y;

  if (!x->left)
    return;

  y = x->left;

  x->left = y->right;
  if (y->right != NULL)
    y->right->parent = x;

  y->parent = x->parent;
  
  if (x->parent == NULL)
    *root = y;
  else if (x == x->parent->left)
    x->parent->left = y;
  else 
    x->parent->right = y;

  y->right = x;
  x->parent = y;
}

/* Basic binary tree insertion, with parent node, and assuming we know the
   NEW_NODE is not already in the tree.  */

static void
plain_tree_insert (struct rb_tree_node **root, struct rb_tree_node *new_node)
{
  struct rb_tree_node *tree = *root;

  if (tree == NULL)
    *root = new_node;
  else if (new_node->key < tree->key)
    {
      if (tree->left)
	plain_tree_insert (&tree->left, new_node);
      else
	{
	  tree->left = new_node;
	  new_node->parent = tree;
	}
    }
  else if (new_node->key > tree->key)
    {
      if (tree->right)
	plain_tree_insert (&tree->right, new_node);
      else
	{
	  tree->right = new_node;
	  new_node->parent = tree;
	}
    }
  else if (new_node->key == tree->key)
    {
      if (new_node->secondary_key < tree->secondary_key)
	{
	  if (tree->left)
	    plain_tree_insert (&tree->left, new_node);
	  else
	    {
	      tree->left = new_node;
	      new_node->parent = tree;
	    }
	}
      else if (new_node->secondary_key > tree->secondary_key)
	{
	  if (tree->right)
	    plain_tree_insert (&tree->right, new_node);
	  else
	    {
	      tree->right = new_node;
	      new_node->parent = tree;
	    }
	}
      else if (new_node->secondary_key == tree->secondary_key)
	{
	  if (new_node->third_key < tree->third_key)
	    {
	      if (tree->left)
		plain_tree_insert (&tree->left, new_node);
	      else
		{
		  tree->left = new_node;
		  new_node->parent = tree;
		}
	    }
	  else /* if (new_node->third_key > tree->third_key) */
	    {
	      if (tree->right)
		plain_tree_insert (&tree->right, new_node);
	      else
		{
		  tree->right = new_node;
		  new_node->parent = tree;
		}
	    }
 	}
    }
}

/* Red-Black tree node insert.  Based on algorithm in "Introduction to
   Algorithms", by Corman, Leiserson, and Rivest, Chapter 14.  The
   resulting binary tree is "roughly balanced", i.e. for any node, the height
   of one subtree will never be more than twice the height of the other.
   Every node has a color, either red or black.  The root is always black;
   the color of a node's children are supposed to be different from the
   color of the node.
*/

void
rb_tree_insert (struct rb_tree_node **root, struct rb_tree_node *tree,
		struct rb_tree_node *new_node)
{
  struct rb_tree_node *y;

  plain_tree_insert (root, new_node);
  new_node->color = RED;
  while (new_node != *root
	 && new_node->parent->color == RED)
    {
      if (new_node->parent == new_node->parent->parent->left)
	{
	  y = new_node->parent->parent->right;
	  if (y && y->color == RED)
	    {
	      new_node->parent->color = BLACK;
	      y->color = BLACK;
	      new_node->parent->parent->color = RED;
	      new_node = new_node->parent->parent;
	    }
	  else if (new_node == new_node->parent->right)
	    {
	      new_node = new_node->parent;
	      left_rotate (root, new_node);
	    }
	  else
	    {
	      new_node->parent->color = BLACK;
	      new_node->parent->parent->color = RED;
	      right_rotate (root, new_node->parent->parent);
	    }
	}
      else
	{
	  y = new_node->parent->parent->left;
	  if (y && y->color == RED)
	    {
	      new_node->parent->color = BLACK;
	      y->color = BLACK;
	      new_node->parent->parent->color = RED;
	      new_node = new_node->parent->parent;
	    }
	  else if (new_node == new_node->parent->left)
	    {
	      new_node = new_node->parent;
	      right_rotate (root, new_node);
	    }
	  else
	    {
	      new_node->parent->color = BLACK;
	      new_node->parent->parent->color = RED;
	      left_rotate (root, new_node->parent->parent);
	    }
	}
    }
  (*root)->color = BLACK;
}

/* End repository sub-section 1:  Red-black trees.  */
/* APPLE LOCAL end red-black trees, part 2.  */

/* Begin repository sub-section 2:  Global repositories data structures.
   This section defines the following:
          MAX_OPEN_DBS  (global constant) 

	  The following two type definitions got moved earlier in this file,
	  but are reproduced below:

      	  enum db_status       (type)
	  struct database_info (type)

	  enum db_status { DB_UNKNOWN, DB_OPEN, DB_ABBREVS_LOADED, DB_CLOSED };

	  struct database_info {
	    char *fullname;
	    struct abbrev_info *abbrev_table;
	    enum db_status current_status;
	    struct rb_tree_node *db_types;
	    struct objfile_list_node *num_uses;
	    struct dwarf2_cu *dummy_cu;
	    struct objfile *dummy_objfile;
	    sqlite3 *db;
	  };


	  repositories (global variable, array of struct database_info)
	  num_open_dbs (global variable, int)
	  
	  find_open_repository    (function)
	  lookup_repository_type  (function)
	  initialize_repositories (function)
	  open_dwarf_repository   (function)
	  close_dwarf_repositories (function)
	  dwarf2_read_repository_abbrevs (function)
*/

#define MAX_OPEN_DBS 100


struct database_info *repositories = NULL;
int num_open_dbs = 0;

/* Given an open sqlite3 db (probably obtained from an objfile struct), find and 
   return the global repository record for that db.  */

static struct database_info *
find_open_repository (sqlite3 *db)
{
  int i;

  for (i = 0; i < num_open_dbs; i++)
    {
      if (repositories[i].db == db)
	return &(repositories[i]);
    }

  return NULL;
}

/* Given a repository TYPE_ID number and the DB repository in which
   it's supposed to be defined, return a struct type containing the
   type definition.  */

static void *
lookup_repository_type (int type_id, sqlite3 *db, struct dwarf2_cu *cu,
			int return_die)
{
  struct database_info *repository = NULL;
  struct type *temp_type = NULL;
  struct die_info *type_die = NULL;
  struct rb_tree_node *new_node = NULL;
  struct rb_repository_data *rb_tmp = NULL;

  repository = find_open_repository (db);
  if (repository)
    {
      if (repository->db_types)
	{
	  new_node = rb_tree_find_node (repository->db_types, type_id, -1);
	  if (new_node)
	    {
	      rb_tmp = (struct rb_repository_data *) new_node->data;
	      temp_type = rb_tmp->type_data;
	      type_die = rb_tmp->die_data;
	    }	
	}

      if (!new_node)
	{
	  struct rb_repository_data *tmp_node;

	  type_die = db_lookup_type (type_id, db, repository->abbrev_table);
	  new_node = (struct rb_tree_node *) 
	    xmalloc (sizeof(struct rb_tree_node));

	  tmp_node = (struct rb_repository_data *) xmalloc (sizeof (struct rb_repository_data));
	  tmp_node->die_data = type_die;
	  tmp_node->type_data = NULL;
	  new_node->key = type_id;
	  new_node->data = (void *) tmp_node;
	  new_node->left = NULL;
	  new_node->right = NULL;
	  new_node->parent = NULL;
	  new_node->color = UNINIT;
	  rb_tree_insert (&repository->db_types, 
			  repository->db_types, new_node);
	  temp_type = tag_type_to_type (type_die, cu);
	  ((struct rb_repository_data *) new_node->data)->type_data = temp_type;
	}
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("Cannot find open repository.\n"));

  if (temp_type && !type_die->type)
    type_die->type = temp_type;

  if (return_die)
    return type_die;
  else
    return temp_type;
}

/* Initialize the global array of repository records.  */

static void
initialize_repositories (void)
{
  int i;

  repositories = (struct database_info *) xmalloc (MAX_OPEN_DBS *
						  sizeof (struct database_info));

  for (i = 0; i < MAX_OPEN_DBS; i++)
    {
      repositories[i].fullname = NULL;
      repositories[i].abbrev_table = NULL;
      repositories[i].current_status = DB_UNKNOWN;
      repositories[i].num_uses = NULL;
      repositories[i].db_types = NULL;
      repositories[i].dummy_cu = NULL;
      repositories[i].dummy_objfile = NULL;
      repositories[i].db = NULL;
    }
}

/* Given a directory and filename for a repository (and an objfile that
   contains compilation units that reference the repository), open
   the repository (if not already open), initialize the appropriate objfile
   fields, and update the corresponding global repository record 
   appropriately (including incrementing the use-count).  */

static int
open_dwarf_repository (char *dirname, char *filename, struct objfile *objfile,
		       struct dwarf2_cu *cu)
{
  int db_status;
  int i;
  char *fullname;

  if (!repositories)
    initialize_repositories();

  fullname = (char *) xmalloc (strlen (dirname) +  strlen (filename) + 2);
  sprintf (fullname, "%s/%s", dirname, filename);

  if (cu->repository)
    {
      if (strcmp (cu->repository_name, fullname) == 0)
	return SQLITE_OK;
      else
	internal_error (__FILE__, __LINE__,
		    _("Multiple repositories found for a single cu\n"));
    }
  else
    {
      for (i = 0; i < num_open_dbs; i++)
	{
	  if (strcmp (fullname, repositories[i].fullname) == 0)
	    {
	      sqlite3 *db = repositories[i].db;
	      if (repositories[i].current_status != DB_OPEN
		  && repositories[i].current_status != DB_ABBREVS_LOADED)
		{
		  sqlite3_open (fullname, &(repositories[i].db));
		  cu->repository = repositories[i].db;
		}
	      else
		cu->repository = db;

	      if (!repositories[i].dummy_objfile)
		{
		  repositories[i].dummy_objfile = build_dummy_objfile (objfile);
		}
	      if (!repositories[i].dummy_cu)
		{
		  repositories[i].dummy_cu = build_dummy_cu (objfile, cu);
		  repositories[i].dummy_cu->objfile = 
		                                    repositories[i].dummy_objfile;
		  repositories[i].dummy_cu->repository = repositories[i].db;
		}
	      increment_use_count (&(repositories[i]), objfile);
	      objfile->uses_sql_repository = 1;
	      cu->repository_name = fullname;
	      return SQLITE_OK;
	    }
	}
    }

  db_status = sqlite3_open (fullname, &(repositories[num_open_dbs].db));
  cu->repository = repositories[num_open_dbs].db;

  if (db_status != SQLITE_OK)
    db_error ("main", "sqlite3_open failed", cu->repository);

  objfile->uses_sql_repository = 1;
  cu->repository_name = fullname;

  if (num_open_dbs < MAX_OPEN_DBS)
    {
      repositories[num_open_dbs].fullname = fullname;
      repositories[num_open_dbs].current_status = DB_OPEN;
      increment_use_count (&(repositories[num_open_dbs]), objfile);
      repositories[num_open_dbs].dummy_cu = build_dummy_cu (objfile, cu);
      repositories[num_open_dbs].dummy_objfile = build_dummy_objfile (objfile);
      num_open_dbs++;
    }
  else
    internal_error (__FILE__, __LINE__,
		    _("Too many databases open at once.\n"));

  return db_status;
}


/* Given an open sqlite3 DB (repository), find the appropriate
   global repository record, decrement the use-count, and close
   the database if the use-count hits zero.  */

int
close_dwarf_repositories (struct objfile *objfile)
{
  sqlite3 *db;
  int db_status;
  int i;

  for (i = 0; i < num_open_dbs; i++)
    {
      decrement_use_count (&(repositories[i]), objfile);
      if (repositories[i].num_uses == NULL)
	{
	  db = repositories[i].db;
	  finalize_stmts (db);
	  db_status = sqlite3_close (db);
	  repositories[i].abbrev_table = NULL;
	  repositories[i].current_status = DB_CLOSED;
	  repositories[i].db_types = NULL;
	  repositories[i].dummy_cu = NULL;
	  obstack_free (&repositories[i].dummy_objfile->objfile_obstack, 0);
	  repositories[i].dummy_objfile = NULL;
	  repositories[i].db = NULL;
	}
    }

  return db_status;
}

/* Given a compilation unit, find the corresponding db and global
   repository record, check to see if dwarf abbreviations table has
   been read in or not, and read it in if it hasn't.  */

static void
dwarf2_read_repository_abbrevs (struct dwarf2_cu *cu)
{
  sqlite3 *db;
  struct database_info *repository = NULL;

  db = cu->repository;
  repository = find_open_repository (db);

  if (repository)
    {
      if (!repository->abbrev_table)
	read_in_db_abbrev_table (&(repository->abbrev_table), db);
    }
}

/* End repository sub-section 2: Global repositories data structures.  */

/* Begin repository sub-section 3:  Accessing the sql database & decoding dies.
   This section defines the following:

       SELECT_DIE_STR  (global constant)
       FIND_STRING_STR (global constant)

       struct attr_pair (struct type);

       db_stmt1 (global variable)
       db_stmt2 (global variable)

       get_uleb128             (function)
       read_in_db_abbrev_table (function)
       fill_in_die_info        (function)
       db_lookup_type          (function)
       db_error                (function)
       build_dummy_cu          (function)
       build_dummy_objfile     (function)
       db_read_1_byte          (function)
       db_read_2_bytes         (function)
       db_read_4_bytes         (function)
       db_read_8_bytes         (function)
       db_read_n_bytes         (function)
       db_read_unsigned_leb128 (function)
       db_read_signed_leb128   (function)
       db_read_attribute_value (function)
       follow_db_ref           (function)
       set_repository_cu_language (function)
       get_repository_name     (function)
       finalize_stmts          (function)
       increment_use_count     (function)
       decrement_use_count     (function)
*/


#define SELECT_DIE_STR "SELECT long_canonical FROM debug_info WHERE die_id == ?"
#define FIND_STRING_STR   "SELECT string FROM debug_str WHERE string_id == ?"

sqlite3_stmt *db_stmt1 = NULL;
sqlite3_stmt *db_stmt2 = NULL;

static uint32_t
get_uleb128 (uint8_t **addr)
{
  uint32_t result = 0;
  int shift = 0;
  const uint8_t *src = *addr;
  uint8_t byte;
  int bytecount = 0;

  while (1)
    {
      bytecount++;
      byte = *src++;
      result |= (byte & 0x7f) << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
        break;
    }

  *addr += bytecount;

  return result;
}

struct attr_pair {
  int attribute;
  int form;
  struct attr_pair *next;
};


static void
read_in_db_abbrev_table (struct abbrev_info **abbrev_table, sqlite3 *db)
{
  struct attr_pair *attr_node;
  struct attr_pair *a_list;
  struct attr_pair *tail;
  int cur_table_size = 100;
  int db_status;
  const char *select_string = "SELECT ALL abbrev_id, attribute_data FROM debug_abbrev";
  const char *pzTail;
  sqlite3_stmt *dbStmt15;
  int abbrev_id;
  int new_size;
  uint8_t *attributes;
  uint8_t *temp_bytes;
  uint8_t *a_ptr;
  int num_attribs;
  int attrib;
  int form;
  int max_id = 0;
  int idx;
  int attributes_len;

  *abbrev_table = (struct abbrev_info *) xmalloc (cur_table_size 
						 * sizeof (struct abbrev_info));

  db_status = sqlite3_prepare (db, select_string, strlen (select_string),
			       &dbStmt15, &pzTail);
  if (db_status == SQLITE_OK)
    {
      db_status = sqlite3_step (dbStmt15);
      while (db_status == SQLITE_ROW)
	{
	  abbrev_id = sqlite3_column_int (dbStmt15, 0);
	  attributes_len = sqlite3_column_bytes (dbStmt15, 1);
	  temp_bytes = (uint8_t *) sqlite3_column_blob (dbStmt15, 1);
	  attributes = (uint8_t *) xmalloc (attributes_len);
	  memcpy (attributes, temp_bytes, attributes_len);
	  /* Build up attributes list & abbrev_record */
	  if (abbrev_id >=  cur_table_size)
	    {
	      if (abbrev_id > 2 * cur_table_size)
		new_size = abbrev_id;
	      else
		new_size = cur_table_size;
	      *abbrev_table = (struct abbrev_info *) realloc (*abbrev_table,
					  new_size * sizeof (struct abbrev_info));
	      cur_table_size = new_size;
	    }
	  if (abbrev_id > max_id)
	    max_id = abbrev_id;
	  a_ptr = attributes;
	  (*abbrev_table)[abbrev_id].number = abbrev_id;
	  (*abbrev_table)[abbrev_id].tag = get_uleb128 (&a_ptr);
	  (*abbrev_table)[abbrev_id].has_children = (int) *a_ptr;
	  (*abbrev_table)[abbrev_id].next = NULL;
	  a_ptr++;
	  num_attribs = 0;
	  a_list = NULL;
	  tail = NULL;
	  do {
	    attrib = get_uleb128 (&a_ptr);
	    form = get_uleb128 (&a_ptr);
	    if (form || attrib)
	      {
		num_attribs++;
		attr_node = (struct attr_pair *) xmalloc (sizeof 
							 (struct attr_pair));
		attr_node->attribute = attrib;
		attr_node->form = form;
		if (!a_list)
		  a_list = attr_node;
		if (tail)
		  tail->next = attr_node;
		tail = attr_node;
	      }
	  } while (attrib != 0 || form != 0);
	  (*abbrev_table)[abbrev_id].num_attrs = num_attribs;
	  (*abbrev_table)[abbrev_id].attrs = 
	    (struct attr_abbrev *) 
	                    xmalloc (num_attribs * sizeof (struct attr_abbrev));
	  for (attr_node = a_list, idx = 0; attr_node && idx < num_attribs;
	       attr_node = attr_node->next, idx++)
	    {
	      if (attr_node->attribute == DW_AT_type)
		(*abbrev_table)[abbrev_id].attrs[idx].name =
                                                      DW_AT_APPLE_repository_type;
	      else
		(*abbrev_table)[abbrev_id].attrs[idx].name = attr_node->attribute;
	      (*abbrev_table)[abbrev_id].attrs[idx].form = attr_node->form;
	    }
	  db_status = sqlite3_step (dbStmt15);
	}
      if (db_status != SQLITE_OK && db_status != SQLITE_DONE)
	db_error ("read_in_abbrev_table", "sqlite3_step failed", db);
    }
  else
    db_error ("read_in_abbrev_table", "sqlite3_prepare failed", db);

  db_status = sqlite3_finalize (dbStmt15);
  if (db_status != SQLITE_OK)
    db_error ("read_in_abbrev_table", "sqlite3_finalize failed", db);
}


static void
fill_in_die_info (struct die_info *new_die, int die_len, uint8_t *die_bytes, 
		  uint8_t *d_ptr, struct abbrev_info *abbrev_table,
		  sqlite3 *db)
{
  int i;
  struct abbrev_info abbrev;
  int abbrev_id = new_die->abbrev;
  int num_attrs = new_die->num_attrs;

  abbrev = abbrev_table[abbrev_id];
  new_die->attrs = (struct attribute *) 
                                xmalloc (num_attrs * sizeof (struct attribute));
  for (i = 0; i < abbrev.num_attrs; i++)
    {
      new_die->attrs[i].name = abbrev.attrs[i].name;
      db_read_attribute_value (&(new_die->attrs[i]), 
			       abbrev.attrs[i].form,
			       &d_ptr);
    }

  if (abbrev.has_children)
    {
      int j;
      int num_children = get_uleb128 (&d_ptr);
      struct die_info *last_child = NULL;

      for (j = 0; (j < num_children
		   && (d_ptr < (die_bytes + die_len))); j++)
	{
	  int child_id = get_uleb128 (&d_ptr);
	  if (child_id == new_die->repository_id)
	    internal_error (__FILE__, __LINE__,
		    _("Recursive child id in repository?\n"));
	  if (!last_child)
	    {
	      new_die->child = db_lookup_type (child_id, db, abbrev_table);
	      last_child = new_die->child;
	      last_child->parent = new_die;
	    }
	  else
	    {
	      last_child->sibling = db_lookup_type (child_id, db, abbrev_table);
	      last_child = last_child->sibling;
	      last_child->parent = new_die;
	    }
	}
    }
  
}

static struct die_info *
db_lookup_type (int type_id, sqlite3 *db, struct abbrev_info *abbrev_table)
{
  int db_status;
  int die_len;
  uint8_t *tmp_bytes;
  uint8_t *die_bytes;
  uint8_t *d_ptr;
  const char *pzTail;
  struct die_info *new_die = NULL;

  db_status = sqlite3_prepare (db, SELECT_DIE_STR,
                               strlen (SELECT_DIE_STR), &db_stmt1, &pzTail);
  if (db_status == SQLITE_OK)
    {
      db_status = sqlite3_bind_int (db_stmt1, 1, type_id);
      
      if (db_status != SQLITE_OK)
	db_error ("db_lookup_type", "sqlite3_bind_int failed", db);
      
      db_status = sqlite3_step (db_stmt1);

      if (db_status == SQLITE_ROW)
	{
	  die_len = sqlite3_column_bytes (db_stmt1, 0);
	  tmp_bytes = (uint8_t *) sqlite3_column_blob (db_stmt1, 0);
	  die_bytes = (uint8_t *) xmalloc (die_len);
	  memcpy (die_bytes, tmp_bytes, die_len);
	  d_ptr = die_bytes;

	  new_die = (struct die_info *) xmalloc (sizeof (struct die_info));

	  new_die->abbrev = get_uleb128 (&d_ptr);
	  new_die->tag = abbrev_table[new_die->abbrev].tag;
	  new_die->offset = 0;
	  new_die->repository_id = type_id;
	  new_die->next_ref = NULL;
	  new_die->type = NULL;
	  new_die->child = NULL;
	  new_die->sibling = NULL;
	  new_die->parent = NULL;
	  new_die->num_attrs = abbrev_table[new_die->abbrev].num_attrs;
	  fill_in_die_info (new_die, die_len, die_bytes, d_ptr, abbrev_table, db);
	}
      else if (db_status != SQLITE_OK && db_status != SQLITE_DONE)
	db_error ("db_lookup_type", "sqlite3_step failed", db);

      while (db_status == SQLITE_ROW)
	db_status = sqlite3_step (db_stmt1);
    }
  else
    db_error ("db_lookup_type", 
	      db_stmt1 ? "sqlite3_reset failed" : "sqlite3_prepare failed", db);

  return new_die;
}

static void
db_error (char *function_name, char *db_action_description, sqlite3 *db)
{
  int len = strlen (sqlite3_errmsg (db)) + 1;
  char *message = (char *) xmalloc (len);
  strcpy (message, sqlite3_errmsg (db));
  finalize_stmts (db);
  sqlite3_close (db);
  internal_error (__FILE__, __LINE__, "%s", message);
}

static struct dwarf2_cu *
build_dummy_cu (struct objfile *old_objfile, struct dwarf2_cu *old_cu)
{
  struct dwarf2_cu *new_cu;

  new_cu = xmalloc (sizeof (struct dwarf2_cu));

  memset (new_cu, 0, sizeof (struct dwarf2_cu));
  obstack_init (&old_cu->comp_unit_obstack);
  new_cu->language = old_cu->language;
  if (old_cu->producer)
    new_cu->producer = xstrdup (old_cu->producer);
  /* APPLE LOCAL: Copy the added comp_dir field.  */
  if (old_cu->comp_dir)
    new_cu->comp_dir = xstrdup (old_cu->comp_dir);
  new_cu->language_defn = old_cu->language_defn;
  new_cu->repository = old_cu->repository;
  if (old_cu->repository_name)
    {
      new_cu->repository_name = (char *) xmalloc
	                                 (strlen (old_cu->repository_name) + 1);
      strcpy (new_cu->repository_name, old_cu->repository_name);
    }

  memset (new_cu->ftypes, 0, FT_NUM_MEMBERS * sizeof (struct type *));

  return new_cu;
}

static struct objfile *
build_dummy_objfile (struct objfile *old_objfile)
{
  struct objfile *new_objfile;

  new_objfile = (struct objfile *) xmalloc (sizeof (struct objfile));

  memset (new_objfile, 0, sizeof (struct objfile));
  new_objfile->md = NULL;
  obstack_specify_allocation (&new_objfile->objfile_obstack, 0, 0, xmalloc, 
			      xfree);

  new_objfile->data = NULL;
  new_objfile->num_data = get_objfile_registry_num_registrations ();
  new_objfile->data = XCALLOC (new_objfile->num_data, void *);
  new_objfile->sect_index_text = -1;
  new_objfile->sect_index_data = -1;
  new_objfile->sect_index_bss = -1;
  new_objfile->sect_index_rodata = -1;
  new_objfile->uses_sql_repository = 1;

  return new_objfile;
}

static uint8_t
db_read_1_byte (uint8_t *info_ptr)
{
  uint8_t src = *info_ptr;
  return src;
}

static uint16_t
db_read_2_bytes (uint8_t *info_ptr)
{
  uint16_t src = *((uint16_t *) info_ptr);
  if (byte_swap_p)
    return (uint16_t) (src & 0x00ff) << 8 | (src & 0xff00) >> 8;
  else
    return src;
}

static uint32_t
db_read_4_bytes (uint8_t *info_ptr)
{
  uint32_t src = *((uint32_t *) info_ptr);
  if (byte_swap_p)
    return (uint32_t)
      (src & 0x000000ff) << 24 |
      (src & 0x0000ff00) << 8 |
      (src & 0x00ff0000) >> 8 |
      (src & 0xff000000) >> 24;
  else
    return (uint32_t) src;
}

static uint64_t
db_read_8_bytes (uint8_t *info_ptr)
{
  union {
    char c[8];
    uint64_t i;
  } in, out;
  in.i = *((uint64_t *) info_ptr);

  if (byte_swap_p)
    {
      out.c[0] = in.c[7];
      out.c[1] = in.c[6];
      out.c[2] = in.c[5];
      out.c[3] = in.c[4];
      out.c[4] = in.c[3];
      out.c[5] = in.c[2];
      out.c[6] = in.c[1];
      out.c[7] = in.c[0];
    }
  else
    out.i = in.i;
  return out.i;
}

static uint8_t *
db_read_n_bytes  (uint8_t *info_ptr, unsigned int num_bytes)
{
  gdb_assert (HOST_CHAR_BIT == 8);
  return info_ptr;
}

static uint32_t
db_read_unsigned_leb128 (uint8_t *info_ptr, unsigned int *bytes_read)
{
  uint32_t result = 0;
  int shift = 0;
  const uint8_t *src = (const uint8_t *) info_ptr;
  uint8_t byte;
  int bytecount = 0;

  while (1)
    {
      bytecount++;
      byte = *src++;
      result |= (byte & 0x7f) << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	break;
    }

  *bytes_read = bytecount;

  return result;
}

static int32_t
db_read_signed_leb128 (uint8_t *info_ptr, unsigned int *bytes_read)
{
  int32_t result = 0;
  int shift = 0;
  int size = sizeof (uint32_t) * 8;
  const uint8_t *src = (const uint8_t *) info_ptr;
  uint8_t byte;
  int bytecount = 0;

  while (1)
    {
      bytecount++;
      byte = *src++;
      result |= (byte & 0x7f) << shift;
      shift += 7;
      if ((byte & 0x80) == 0)
	break;
    }

  if (shift < size && (byte & 0x40))
    result |= - (1 << shift);

  *bytes_read = bytecount;

  return result;
}

static uint8_t *
db_read_attribute_value (struct attribute *attr, unsigned form, 
			 uint8_t **info_ptr)
{
  unsigned int bytes_read;
  struct dwarf_block *blk;

  attr->form = form;
  switch (form)
    {
    case DW_FORM_block2:
      blk = (struct dwarf_block *) xmalloc (sizeof (struct dwarf_block));
      blk->size = db_read_2_bytes (*info_ptr);
      *info_ptr += 2;
      blk->data = (char *) db_read_n_bytes (*info_ptr, blk->size);
      *info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block4:
      blk = (struct dwarf_block *) xmalloc (sizeof (struct dwarf_block));
      blk->size = db_read_4_bytes (*info_ptr);
      *info_ptr += 4;
      blk->data = (char *) db_read_n_bytes (*info_ptr, blk->size);
      *info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data2:
      DW_UNSND (attr) = db_read_2_bytes (*info_ptr);
      *info_ptr += 2;
      break;
    case DW_FORM_data4:
      DW_UNSND (attr) = db_read_4_bytes (*info_ptr);
      *info_ptr += 4;
      break;
    case DW_FORM_data8:
      DW_UNSND (attr) = db_read_8_bytes (*info_ptr);
      *info_ptr += 8;
      break;
    case DW_FORM_block:
      blk = (struct dwarf_block *) xmalloc (sizeof (struct dwarf_block));
      blk->size = db_read_unsigned_leb128 (*info_ptr, &bytes_read);
      *info_ptr += bytes_read;
      blk->data = (char *) db_read_n_bytes (*info_ptr, blk->size);
      *info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_block1:
      blk = (struct dwarf_block *) xmalloc (sizeof (struct dwarf_block));
      blk->size = db_read_1_byte (*info_ptr);
      *info_ptr += 1;
      blk->data = (char *) db_read_n_bytes (*info_ptr, blk->size);
      *info_ptr += blk->size;
      DW_BLOCK (attr) = blk;
      break;
    case DW_FORM_data1:
      DW_UNSND (attr) = db_read_1_byte (*info_ptr);
      *info_ptr += 1;
      break;
    case DW_FORM_flag:
      DW_UNSND (attr) = db_read_1_byte (*info_ptr);
      *info_ptr += 1;
      break;
    case DW_FORM_sdata:
      DW_SND (attr) = db_read_signed_leb128 (*info_ptr, &bytes_read);
      *info_ptr += bytes_read;
      break;
    case DW_FORM_APPLE_db_str:
    case DW_FORM_udata:
      DW_UNSND (attr) = db_read_unsigned_leb128 (*info_ptr, &bytes_read);
      *info_ptr += bytes_read;
      break;
    case DW_FORM_indirect:
      form = db_read_unsigned_leb128 (*info_ptr, &bytes_read);
      *info_ptr += bytes_read;
      *info_ptr = db_read_attribute_value (attr, form, info_ptr);
      break; 
    case DW_FORM_ref1:
    case DW_FORM_ref2:
    case DW_FORM_ref4:
    case DW_FORM_ref8:
    case DW_FORM_ref_udata:
    case DW_FORM_addr:
    case DW_FORM_ref_addr:
    case DW_FORM_string:
    case DW_FORM_strp:
    default:
      error (_("Dwarf Error: Cannot handle %s in DWARF reader [in module %s]"),
	     dwarf_form_name (form),
	     "db_read_attribute_value");
    }
  return *info_ptr;
}

static struct die_info *
follow_db_ref (struct die_info *die, struct attribute *repository_spec, 
	       struct dwarf2_cu *cu)
{
  int db_id;
  int want_die_p = 1;
  sqlite3 *db = cu->repository;
  struct die_info *db_die;
 
  db_id = DW_UNSND (repository_spec);
  db_die = (struct die_info *) lookup_repository_type (db_id, db, cu, want_die_p);

  return db_die;
}

static void
set_repository_cu_language (unsigned int language, struct dwarf2_cu *old_cu)
{
  sqlite3 *db = old_cu->repository;
  struct database_info *repository = NULL;

  if (!db)
    internal_error (__FILE__, __LINE__, _("Missing database.\n"));
  
  repository = find_open_repository (db);
  if (repository)
    set_cu_language (language, repository->dummy_cu);
}

static struct attribute *
get_repository_name (struct attribute *attr, struct dwarf2_cu *cu)
{
  sqlite3 *db = cu->repository;
  int string_id;
  int db_status;
  struct attribute *name_attribute = NULL;
  const char *pzTail;
  char *name;

  string_id = DW_UNSND (attr);

  if (db)
    {
	db_status = sqlite3_prepare (db, FIND_STRING_STR, 
				     strlen (FIND_STRING_STR), &db_stmt2,
				     &pzTail);

      if (db_status != SQLITE_OK)
	db_error ("get_repository_name",
		  (db_stmt2 ? "sqlite3_reset failed" : "sqlite_prepare3 failed"),
		  db);

      db_status = sqlite3_bind_int (db_stmt2, 1, string_id);

      if (db_status != SQLITE_OK)
	db_error ("get_repository_name", "sqlite3_bind_int failed", db);

      db_status = sqlite3_step (db_stmt2);
      while (db_status == SQLITE_ROW)
	{
	  name = (char *) sqlite3_column_text (db_stmt2, 0);
	  db_status = sqlite3_step (db_stmt2);
	}

      if (name)
	{
	  name_attribute = (struct attribute *) xmalloc 
	                                            (sizeof (struct attribute));
	  name_attribute->name = DW_AT_name;
	  name_attribute->form = DW_FORM_string;
	  DW_STRING(name_attribute) = (char *) xmalloc (strlen (name) + 1);
	  strcpy (DW_STRING(name_attribute), name);
	}
    }

  return name_attribute;
}

static int
finalize_stmts (sqlite3 *db)
{
 int db_status;

  db_status = sqlite3_finalize (db_stmt1);

  if (db_status != SQLITE_OK)
    db_error ("finalize_stmts", "failed on db_stmt1", db);

  db_status = sqlite3_finalize (db_stmt2);

  if (db_status != SQLITE_OK)
    db_error ("finalize_stmts", "failed on db_stmt2", db);

  db_stmt1 = NULL;
  db_stmt2 = NULL;

  return db_status;
}


static void
increment_use_count (struct database_info *repository, struct objfile *ofile)
{
  struct objfile_list_node *current;
  struct objfile_list_node *new_node;

  /* Check to see if ofile is already in the list; if so, return.  */

  for (current = repository->num_uses; current; current = current->next)
    if (current->ofile == ofile)
      return;

  /* We ran off the list without finding ofile, so we need to add it to the
     list (at the front).  */

  new_node = (struct objfile_list_node *) xmalloc (sizeof (struct objfile_list_node));
  new_node->ofile = ofile;
  new_node->next = repository->num_uses;
  repository->num_uses = new_node;
}

static void
decrement_use_count (struct database_info *repository, struct objfile *ofile)
{
  struct objfile_list_node *current;
  struct objfile_list_node *prev;

  if (repository->num_uses == NULL)
    return;

  for (prev = NULL, current = repository->num_uses; current;
       prev = current, current = current->next)
    {
      if (current->ofile == ofile)
	{
	  if (prev)
	    prev->next = current->next;
	  else
	    repository->num_uses = current->next;
	  return;
	}
    }

}

/* End repository sub-section 3: Accessing the sql database & decoding dies. */

/* Functions for debugging red-black trees.  */

static int
num_nodes_in_tree (struct rb_tree_node *tree)
{
  int total;

  if (tree == NULL)
    total = 0;
  else
    total = num_nodes_in_tree (tree->left) +  num_nodes_in_tree (tree->right) + 1;

  return total;
}


static int
tree_height (struct rb_tree_node *tree)
{
  int left_height;
  int right_height;
  int height;

  if (tree == NULL)
    height = 0;
  else
    {
      left_height = tree_height (tree->left);
      right_height = tree_height (tree->right);
      if (left_height > right_height)
	height = left_height;
      else
	height = right_height;
      if (tree->color == BLACK)
	height++;
    }

  return height;
}

static int
verify_tree_colors (struct rb_tree_node *tree)
{
  int colors_okay;

  if (tree == NULL)
    colors_okay = 1;
  else if  (tree->color == RED)
      colors_okay = ((!tree->left || tree->left->color == BLACK)
		     && (!tree->right || tree->right->color == BLACK)
		     && verify_tree_colors (tree->left)
		     && verify_tree_colors (tree->right));
  else if (tree->color == BLACK)
      colors_okay = (verify_tree_colors (tree->left)
		     && verify_tree_colors (tree->right));
  else
    colors_okay = 0;

  return colors_okay;
}

static int
verify_tree_heights (struct rb_tree_node *tree)
{
  int heights_okay;

  if (tree == NULL)
    heights_okay = 1;
  else
    heights_okay = (tree_height (tree->left) == tree_height (tree->right));

  return heights_okay;
}

static int
verify_rb_tree (struct rb_tree_node *tree)
{
  if (!verify_tree_colors (tree))
    {
      fprintf (stderr, "rb_tree is not colored correctly.\n");
      return 0;
    }

  if (!verify_tree_heights (tree))
    {
      fprintf (stderr, "rb_tree is not properly balanced.\n");
      return 0;
    }

  return 1;
}

static void
rb_print_node (struct rb_tree_node *tree)
{

  if (tree == NULL)
    fprintf (stdout, "(NULL)\n");
  else
    {
      if (tree->color == RED)
	fprintf (stdout, "(Red");
      else if (tree->color == BLACK)
	fprintf (stdout, "(Black");
      else
	fprintf (stdout, "(Unknown");
      fprintf (stdout, ", 0x%s, %d, 0x%s)\n", paddr_nz (tree->key),
               tree->secondary_key, paddr_nz (tree->third_key));
    }
}

static void 
rb_print_tree (struct rb_tree_node *tree, int indent_level)
{
  char *spaces;

  spaces = (char *) xmalloc (indent_level);
  memset (spaces, ' ', indent_level);

  fprintf (stdout, "%s", spaces);
  rb_print_node (tree);
  if (tree)
    {
      rb_print_tree (tree->left, indent_level + 3);
      rb_print_tree (tree->right, indent_level + 3);
    }

  xfree (spaces);
}

/* End functions for debugging red-black trees.  */

/* APPLE LOCAL end dwarf repository  */

