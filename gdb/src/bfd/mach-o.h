#ifndef _BFD_MACH_O_H_
#define _BFD_MACH_O_H_

#include "bfd.h"

#define BFD_MACH_O_N_STAB  0xe0	/* if any of these bits set, a symbolic debugging entry */
#define BFD_MACH_O_N_PEXT  0x10	/* private external symbol bit */
#define BFD_MACH_O_N_TYPE  0x0e	/* mask for the type bits */
#define BFD_MACH_O_N_EXT   0x01	/* external symbol bit, set for external symbols */

#define BFD_MACH_O_N_UNDF  0x0	/* undefined, n_sect == NO_SECT */
#define BFD_MACH_O_N_ABS   0x2	/* absolute, n_sect == NO_SECT */
#define BFD_MACH_O_N_SECT  0xe	/* defined in section number n_sect */
#define BFD_MACH_O_N_PBUD  0xc  /* prebound undefined (defined in a dylib) */
#define BFD_MACH_O_N_INDR  0xa	/* indirect */

typedef enum bfd_mach_o_ppc_thread_flavour {
  BFD_MACH_O_PPC_THREAD_STATE = 1,
  BFD_MACH_O_PPC_FLOAT_STATE = 2,
  BFD_MACH_O_PPC_EXCEPTION_STATE = 3,
  BFD_MACH_O_PPC_VECTOR_STATE = 4
} bfd_mach_o_ppc_thread_flavour;

typedef enum bfd_mach_o_i386_thread_flavour {
  BFD_MACH_O_i386_NEW_THREAD_STATE = 1,
  BFD_MACH_O_i386_FLOAT_STATE = 2,
  BFD_MACH_O_i386_ISA_PORT_MAP_STATE = 3,
  BFD_MACH_O_i386_V86_ASSIST_STATE = 4,
  BFD_MACH_O_i386_REGS_SEGS_STATE = 5,
  BFD_MACH_O_i386_THREAD_SYSCALL_STATE = 6,
  BFD_MACH_O_i386_THREAD_STATE_NONE = 7,
  BFD_MACH_O_i386_SAVED_STATE = 8,
  BFD_MACH_O_i386_THREAD_STATE = -1,
  BFD_MACH_O_i386_THREAD_FPSTATE = -2,
  BFD_MACH_O_i386_THREAD_EXCEPTSTATE = -3,
  BFD_MACH_O_i386_THREAD_CTHREADSTATE = -4,
} bfd_mach_o_i386_thread_flavour;

typedef enum bfd_mach_o_load_command_type { 
  BFD_MACH_O_LC_SEGMENT = 0x1,		/* file segment to be mapped */
  BFD_MACH_O_LC_SYMTAB = 0x2,		/* link-edit stab symbol table info (obsolete) */
  BFD_MACH_O_LC_SYMSEG = 0x3,		/* link-edit gdb symbol table info */
  BFD_MACH_O_LC_THREAD = 0x4,		/* thread */
  BFD_MACH_O_LC_UNIXTHREAD = 0x5,	/* unix thread (includes a stack) */
  BFD_MACH_O_LC_LOADFVMLIB = 0x6,	/* load a fixed VM shared library */
  BFD_MACH_O_LC_IDFVMLIB = 0x7,		/* fixed VM shared library id */
  BFD_MACH_O_LC_IDENT = 0x8,		/* object identification information (obsolete) */
  BFD_MACH_O_LC_FVMFILE = 0x9,		/* fixed VM file inclusion */
  BFD_MACH_O_LC_PREPAGE = 0xa,		/* prepage command (internal use) */
  BFD_MACH_O_LC_DYSYMTAB = 0xb,		/* dynamic link-edit symbol table info */
  BFD_MACH_O_LC_LOAD_DYLIB = 0xc,	/* load a dynamicly linked shared library */
  BFD_MACH_O_LC_ID_DYLIB = 0xd,		/* dynamicly linked shared lib identification */
  BFD_MACH_O_LC_LOAD_DYLINKER = 0xe,	/* load a dynamic linker */
  BFD_MACH_O_LC_ID_DYLINKER = 0xf,	/* dynamic linker identification */
  BFD_MACH_O_LC_PREBOUND_DYLIB = 0x10,	/* modules prebound for a dynamicly */
  BFD_MACH_O_LC_ROUTINES = 0x11,	/* image routines */
  BFD_MACH_O_LC_SUB_FRAMEWORK = 0x12,	/* sub framework */
  BFD_MACH_O_LC_SUB_UMBRELLA = 0x13,	/* sub umbrella */
  BFD_MACH_O_LC_SUB_CLIENT = 0x14,	/* sub client */
  BFD_MACH_O_LC_SUB_LIBRARY = 0x15,     /* sub library */
  BFD_MACH_O_LC_TWOLEVEL_HINTS = 0x16,  /* two-level namespace lookup hints */
  BFD_MACH_O_LC_PREBIND_CKSUM = 0x17,   /* prebind checksum */
  /* load a dynamicly linked shared library that is allowed to be
     missing (weak)*/
  BFD_MACH_O_LC_LOAD_WEAK_DYLIB = 0x18
} bfd_mach_o_load_command_type;

typedef enum bfd_mach_o_cpu_type {
  BFD_MACH_O_CPU_TYPE_VAX = 1,
  BFD_MACH_O_CPU_TYPE_MC680x0 = 6,
  BFD_MACH_O_CPU_TYPE_I386 = 7,
  BFD_MACH_O_CPU_TYPE_MIPS = 8,
  BFD_MACH_O_CPU_TYPE_MC98000 = 10,
  BFD_MACH_O_CPU_TYPE_HPPA = 11,
  BFD_MACH_O_CPU_TYPE_ARM = 12,
  BFD_MACH_O_CPU_TYPE_MC88000 = 13,
  BFD_MACH_O_CPU_TYPE_SPARC = 14,
  BFD_MACH_O_CPU_TYPE_I860 = 15,
  BFD_MACH_O_CPU_TYPE_ALPHA = 16,
  BFD_MACH_O_CPU_TYPE_POWERPC = 18
} bfd_mach_o_cpu_type;

typedef enum bfd_mach_o_filetype {
  BFD_MACH_O_MH_OBJECT = 1,
  BFD_MACH_O_MH_EXECUTE = 2,
  BFD_MACH_O_MH_FVMLIB = 3,
  BFD_MACH_O_MH_CORE = 4,
  BFD_MACH_O_MH_PRELOAD = 5,
  BFD_MACH_O_MH_DYLIB = 6,
  BFD_MACH_O_MH_DYLINKER = 7,
  BFD_MACH_O_MH_BUNDLE = 8
} bfd_mach_o_filetype;

/* Constants for the type of a section */

typedef enum bfd_mach_o_section_type {

  /* regular section */
  BFD_MACH_O_S_REGULAR = 0x0,	

  /* zero fill on demand section */
  BFD_MACH_O_S_ZEROFILL = 0x1,

  /* section with only literal C strings*/
  BFD_MACH_O_S_CSTRING_LITERALS = 0x2, 

  /* section with only 4 byte literals */
  BFD_MACH_O_S_4BYTE_LITERALS = 0x3,

  /* section with only 8 byte literals */
  BFD_MACH_O_S_8BYTE_LITERALS = 0x4,

  /* section with only pointers to literals */
  BFD_MACH_O_S_LITERAL_POINTERS = 0x5,

  /* For the two types of symbol pointers sections and the symbol stubs
     section they have indirect symbol table entries.  For each of the
     entries in the section the indirect symbol table entries, in
     corresponding order in the indirect symbol table, start at the index
     stored in the reserved1 field of the section structure.  Since the
     indirect symbol table entries correspond to the entries in the
     section the number of indirect symbol table entries is inferred from
     the size of the section divided by the size of the entries in the
     section.  For symbol pointers sections the size of the entries in
     the section is 4 bytes and for symbol stubs sections the byte size
     of the stubs is stored in the reserved2 field of the section
     structure.  */

  /* section with only non-lazy symbol pointers */
  BFD_MACH_O_S_NON_LAZY_SYMBOL_POINTERS = 0x6,
  
  /* section with only lazy symbol pointers */
  BFD_MACH_O_S_LAZY_SYMBOL_POINTERS = 0x7,
  
  /* section with only symbol stubs, byte size of stub in the reserved2 field */
  BFD_MACH_O_S_SYMBOL_STUBS = 0x8,
  
  /* section with only function pointers for initialization */
  BFD_MACH_O_S_MOD_INIT_FUNC_POINTERS = 0x9

} bfd_mach_o_section_type;

typedef unsigned long bfd_mach_o_cpu_subtype;

typedef struct bfd_mach_o_header {
  unsigned long magic;
  unsigned long cputype;
  unsigned long cpusubtype;
  unsigned long filetype;
  unsigned long ncmds;
  unsigned long sizeofcmds;
  unsigned long flags;
  enum bfd_endian byteorder;
} bfd_mach_o_header;

typedef struct bfd_mach_o_section {
  asection *bfdsection;
  char sectname[16 + 1];
  char segname[16 + 1];
  bfd_vma addr;
  bfd_vma size;
  bfd_vma offset;
  unsigned long align;
  bfd_vma reloff;
  unsigned long nreloc;
  unsigned long flags;
  unsigned long reserved1;
  unsigned long reserved2;
} bfd_mach_o_section;

typedef struct bfd_mach_o_segment_command {
  char segname[16];
  bfd_vma vmaddr;
  bfd_vma vmsize;
  bfd_vma fileoff;
  unsigned long filesize;
  unsigned long nsects;
  unsigned long flags;
  bfd_mach_o_section *sections;
  asection *segment;
} bfd_mach_o_segment_command;

typedef struct bfd_mach_o_symtab_command {
  unsigned long symoff;
  unsigned long nsyms;
  unsigned long stroff;
  unsigned long strsize;
  asymbol *symbols;
  char *strtab;
  asection *stabs_segment;
  asection *stabstr_segment;
} bfd_mach_o_symtab_command;

/*
 * This is the second set of the symbolic information which is used to support
 * the data structures for the dynamicly link editor.
 *
 * The original set of symbolic information in the symtab_command which contains
 * the symbol and string tables must also be present when this load command is
 * present.  When this load command is present the symbol table is organized
 * into three groups of symbols:
 *      local symbols (static and debugging symbols) - grouped by module
 *      defined external symbols - grouped by module (sorted by name if not lib)
 *      undefined external symbols (sorted by name)
 * In this load command there are offsets and counts to each of the three groups
 * of symbols.
 *
 * This load command contains a the offsets and sizes of the following new
 * symbolic information tables:
 *      table of contents
 *      module table
 *      reference symbol table
 *      indirect symbol table
 * The first three tables above (the table of contents, module table and
 * reference symbol table) are only present if the file is a dynamicly linked
 * shared library.  For executable and object modules, which are files
 * containing only one module, the information that would be in these three
 * tables is determined as follows:
 *      table of contents - the defined external symbols are sorted by name
 *      module table - the file contains only one module so everything in the
 *                     file is part of the module.
 *      reference symbol table - is the defined and undefined external symbols
 *
 * For dynamicly linked shared library files this load command also contains
 * offsets and sizes to the pool of relocation entries for all sections
 * separated into two groups:
 *      external relocation entries
 *      local relocation entries
 * For executable and object modules the relocation entries continue to hang
 * off the section structures.
 */

typedef struct bfd_mach_o_dysymtab_command {

  /*
   * The symbols indicated by symoff and nsyms of the LC_SYMTAB load command
   * are grouped into the following three groups:
   *    local symbols (further grouped by the module they are from)
   *    defined external symbols (further grouped by the module they are from)
   *    undefined symbols
   *
   * The local symbols are used only for debugging.  The dynamic binding
   * process may have to use them to indicate to the debugger the local
   * symbols for a module that is being bound.
   *
   * The last two groups are used by the dynamic binding process to do the
   * binding (indirectly through the module table and the reference symbol
   * table when this is a dynamicly linked shared library file).
   */

  unsigned long ilocalsym;    /* index to local symbols */
  unsigned long nlocalsym;    /* number of local symbols */

  unsigned long iextdefsym;   /* index to externally defined symbols */
  unsigned long nextdefsym;   /* number of externally defined symbols */

  unsigned long iundefsym;    /* index to undefined symbols */
  unsigned long nundefsym;    /* number of undefined symbols */

  /*
   * For the for the dynamic binding process to find which module a symbol
   * is defined in the table of contents is used (analogous to the ranlib
   * structure in an archive) which maps defined external symbols to modules
   * they are defined in.  This exists only in a dynamicly linked shared
   * library file.  For executable and object modules the defined external
   * symbols are sorted by name and is use as the table of contents.
   */

  unsigned long tocoff;       /* file offset to table of contents */
  unsigned long ntoc;         /* number of entries in table of contents */

  /*
   * To support dynamic binding of "modules" (whole object files) the symbol
   * table must reflect the modules that the file was created from.  This is
   * done by having a module table that has indexes and counts into the merged
   * tables for each module.  The module structure that these two entries
   * refer to is described below.  This exists only in a dynamicly linked
   * shared library file.  For executable and object modules the file only
   * contains one module so everything in the file belongs to the module.
   */

  unsigned long modtaboff;    /* file offset to module table */
  unsigned long nmodtab;      /* number of module table entries */

  /*
   * To support dynamic module binding the module structure for each module
   * indicates the external references (defined and undefined) each module
   * makes.  For each module there is an offset and a count into the
   * reference symbol table for the symbols that the module references.
   * This exists only in a dynamicly linked shared library file.  For
   * executable and object modules the defined external symbols and the
   * undefined external symbols indicates the external references.
   */

  unsigned long extrefsymoff;  /* offset to referenced symbol table */
  unsigned long nextrefsyms;   /* number of referenced symbol table entries */

  /*
   * The sections that contain "symbol pointers" and "routine stubs" have
   * indexes and (implied counts based on the size of the section and fixed
   * size of the entry) into the "indirect symbol" table for each pointer
   * and stub.  For every section of these two types the index into the
   * indirect symbol table is stored in the section header in the field
   * reserved1.  An indirect symbol table entry is simply a 32bit index into
   * the symbol table to the symbol that the pointer or stub is referring to.
   * The indirect symbol table is ordered to match the entries in the section.
   */

  unsigned long indirectsymoff; /* file offset to the indirect symbol table */
  unsigned long nindirectsyms;  /* number of indirect symbol table entries */

  /*
   * To support relocating an individual module in a library file quickly the
   * external relocation entries for each module in the library need to be
   * accessed efficiently.  Since the relocation entries can't be accessed
   * through the section headers for a library file they are separated into
   * groups of local and external entries further grouped by module.  In this
   * case the presents of this load command who's extreloff, nextrel,
   * locreloff and nlocrel fields are non-zero indicates that the relocation
   * entries of non-merged sections are not referenced through the section
   * structures (and the reloff and nreloc fields in the section headers are
   * set to zero).
   *
   * Since the relocation entries are not accessed through the section headers
   * this requires the r_address field to be something other than a section
   * offset to identify the item to be relocated.  In this case r_address is
   * set to the offset from the vmaddr of the first LC_SEGMENT command.
   *
   * The relocation entries are grouped by module and the module table
   * entries have indexes and counts into them for the group of external
   * relocation entries for that the module.
   *
   * For sections that are merged across modules there must not be any
   * remaining external relocation entries for them (for merged sections
   * remaining relocation entries must be local).
   */

  unsigned long extreloff;    /* offset to external relocation entries */
  unsigned long nextrel;      /* number of external relocation entries */

  /*
   * All the local relocation entries are grouped together (they are not
   * grouped by their module since they are only used if the object is moved
   * from it staticly link edited address).
   */

   unsigned long locreloff;    /* offset to local relocation entries */
   unsigned long nlocrel;      /* number of local relocation entries */

} bfd_mach_o_dysymtab_command;      

/*
 * An indirect symbol table entry is simply a 32bit index into the symbol table 
 * to the symbol that the pointer or stub is refering to.  Unless it is for a
 * non-lazy symbol pointer section for a defined symbol which strip(1) as 
 * removed.  In which case it has the value INDIRECT_SYMBOL_LOCAL.  If the
 * symbol was also absolute INDIRECT_SYMBOL_ABS is or'ed with that.
 */

#define INDIRECT_SYMBOL_LOCAL   0x80000000
#define INDIRECT_SYMBOL_ABS     0x40000000

typedef struct bfd_mach_o_thread_flavour {
  unsigned long flavour;
  bfd_vma offset;
  unsigned long size;
} bfd_mach_o_thread_flavour;

typedef struct bfd_mach_o_thread_command {
  unsigned long nflavours;
  bfd_mach_o_thread_flavour *flavours; 
  asection *section;
} bfd_mach_o_thread_command;

typedef struct bfd_mach_o_dylinker_command {
  unsigned long cmd;                   /* LC_ID_DYLIB or LC_LOAD_DYLIB */
  unsigned long cmdsize;               /* includes pathname string */
  unsigned long name_offset;           /* offset to library's path name */
  unsigned long name_len;              /* offset to library's path name */
  asection *section;
} bfd_mach_o_dylinker_command;

typedef struct bfd_mach_o_dylib_command {
  unsigned long cmd;                   /* LC_ID_DYLIB or LC_LOAD_DYLIB */
  unsigned long cmdsize;               /* includes pathname string */
  unsigned long name_offset;           /* offset to library's path name */
  unsigned long name_len;              /* offset to library's path name */
  unsigned long timestamp;	       /* library's build time stamp */
  unsigned long current_version;       /* library's current version number */
  unsigned long compatibility_version; /* library's compatibility vers number*/
  asection *section;
} bfd_mach_o_dylib_command;

typedef struct bfd_mach_o_prebound_dylib_command {
  unsigned long cmd;                 /* LC_PREBOUND_DYLIB */
  unsigned long cmdsize;             /* includes strings */
  unsigned long name;                /* library's path name */
  unsigned long nmodules;            /* number of modules in library */
  unsigned long linked_modules;      /* bit vector of linked modules */
  asection *section;
} bfd_mach_o_prebound_dylib_command;

typedef struct bfd_mach_o_load_command {
  bfd_mach_o_load_command_type type;
  bfd_vma offset;
  bfd_vma len;
  union {
    bfd_mach_o_segment_command segment;
    bfd_mach_o_symtab_command symtab;
    bfd_mach_o_dysymtab_command dysymtab;
    bfd_mach_o_thread_command thread;
    bfd_mach_o_dylib_command dylib;
    bfd_mach_o_dylinker_command dylinker;
    bfd_mach_o_prebound_dylib_command prebound_dylib;
  } command;
} bfd_mach_o_load_command;

typedef struct mach_o_data_struct {
  bfd_mach_o_header header;
  bfd_mach_o_load_command *commands;
  unsigned long nsymbols;
  asymbol *symbols;
  unsigned long nsects;
  bfd_mach_o_section **sections;
  bfd *ibfd;
} mach_o_data_struct;

typedef struct mach_o_data_struct bfd_mach_o_data_struct;

boolean bfd_mach_o_valid
(bfd *abfd);

int bfd_mach_o_lookup_section 
(bfd *abfd, asection *section, 
 bfd_mach_o_load_command **mcommand, bfd_mach_o_section **msection);

int bfd_mach_o_lookup_command 
(bfd *abfd, bfd_mach_o_load_command_type type, bfd_mach_o_load_command **mcommand);

int bfd_mach_o_scan_read_symtab_symbols
(bfd *abfd, bfd_mach_o_symtab_command *sym);

int bfd_mach_o_scan_read_symtab_strtab
(bfd *abfd, bfd_mach_o_symtab_command *sym);

int bfd_mach_o_scan_read_symtab_symbol
(bfd *abfd, bfd_mach_o_symtab_command *sym, asymbol *s, unsigned long i);

int bfd_mach_o_scan_read_dysymtab_symbol
(bfd *abfd, bfd_mach_o_dysymtab_command *dysym, bfd_mach_o_symtab_command *sym,
 asymbol *s, unsigned long i);

extern const bfd_target mach_o_be_vec;
extern const bfd_target mach_o_le_vec;
extern const bfd_target mach_o_fat_vec;

#endif /* _BFD_MACH_O_H_ */
