/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Globals declared in mkshlib.c
 */
extern char *progname;	/* name of the program for error messages (argv[0]) */

/* file name of the specification input file */
extern char *spec_filename;

/* file name of the host shared library output file */
extern char *host_filename;

/* file name of the target shared library output file */
extern char *target_filename;

/* shared library definision symbol name */
extern char *shlib_symbol_name;
/* shared library full reference symbol name */
extern char *shlib_reference_name;
/* base name of target shared lib */
extern char *target_base_name;

extern long vflag;	/* the verbose flag (-v) */
extern long dflag;	/* the debug flag (-d) */

/*
 * The architecture of the output file as specified by -arch and the cputype
 * and cpusubtype of the object files being loaded which will be the output
 * cputype and cpusubtype.
 */
extern struct arch_flag arch_flag;
/* the byte sex of the machine this program is running on */
extern enum byte_sex host_byte_sex;
/* the byte sex of the output files */
extern enum byte_sex target_byte_sex;

extern char *asname;		/* name of the assember being used */
extern char *ldname;		/* name of the link editior being used */

/*
 * Array to hold ld(1) flags.
 */
extern char **ldflags;
extern unsigned long nldflags;

extern void cleanup(
    int sig);
extern char *branch_slot_symbol(
    long i);
extern long is_branch_slot_symbol(
    char *p);
extern long is_libdef_symbol(
    char *p);
extern long run(
    void);
extern void add_runlist(
    char *str);
extern void reset_runlist(
    void);
extern long round(
    long v,
    unsigned long r);

/*
 * Initial illegal address for segments and an illegal address for symbols
 * without branch table entries.
 */
#define	BAD_ADDRESS ((unsigned long)-1)

/*
 * Globals declared in parse_spec.c
 */
/*
 * These globals are set after parse_spec() returns with the values read from
 * the library shared library specification file.
 */
extern char *target_name;		/* shared library target name */
extern unsigned long minor_version;	/* shared library minor version number*/
extern unsigned long image_version;	/* shared library image version number*/
extern unsigned long text_addr;		/* shared library text segment addr */
extern unsigned long data_addr;		/* shared library data segment addr */

/*
 * The names of the symbols in the #branch directive are stored in a
 * branch structure.  A pointer to this structure is in an ordered list,
 * branch_list, which has nbranch_list entries.
 */
struct branch {
    char *name;			/* name of symbol for this branch table slot */
    char *old_name;		/* old name for this branch table slot(in any)*/
    long empty_slot;		/* these slots are to be empty */
    long max_slotnum;		/* highest slot number this symbol is in */
    struct branch *next;	/* next branch in hash table chain */
};
/*
 * The reserved name for an empty branch table slot.
 */
#define EMPTY_SLOT_NAME ".empty_slot"

/*
 * branch list is a linear list of branch slots with nbranch_list slots.
 * It is created and filled in parse_spec() and used in creating the branch
 * branch table for the target library.
 */
extern struct branch **branch_list;
extern long nbranch_list;		 

/*
 * Hash table for branch slots, filled in parse_spec() and used in creating
 * the host library in changing symbol values to their branch table entries.
 * Used in scan_objects() to find a symbol's branch table slot.
 */
#define BRANCH_HASH_SIZE 1301 /* branch table hash size (fixed, prime number) */
extern struct branch *branch_hash[BRANCH_HASH_SIZE];

/*
 * The names of the symbols in the #nobranch_text, #private_externs and
 * #undefined directives are stored in a oddball structure.
 */
struct oddball {
    char *name;			/* name of symbol */
    long nobranch;		/* not to have a branch table slot */
    long private;		/* private symbol not to appear in host shlib */
    long undefined;		/* allowed undefined symbol in shlib */
    struct oddball *next;	/* next branch in hash table chain */
};

/*
 * Hash table for oddball symbols, filled in parse_spec() and used in creating
 * the host library by not including priviate symbols.  And is used in creating
 * the target library in not complaining about external symbols that don't
 * have branch table entries.
 */
#define ODDBALL_HASH_SIZE 251 /* oddball table hash size (fixed, prime number)*/
extern struct oddball *oddball_hash[ODDBALL_HASH_SIZE];

/*
 * The object names from the #objects directive are stored in a object
 * structure.  A pointer to this structure is in an ordered list, object_list,
 * which has nobject_list entries.  The the pairs of symbol names from the
 * #init directive are stored in a init structure and chained together
 * off of the object structure they are for.
 */ 
struct init {
    char *pinit;	   /* name of pointer to be initialized */
    long pdefined;	   /* 1 if the pointer is defined in the object */
    long pvalue;	   /* the value of the pointer (zero if undefined) */
    struct relocation_info /* the relocation entry for the pointer */
	preloc;
    char *sinit;	   /* name of symbol to initalize pointer with */
    long sdefined;	   /* 1 if the symbol is defined in the object */
    long svalue;	   /* the value of the symbol (zero if undefined) */
    struct relocation_info /* relocation entry for the symbol */
	sreloc;
    struct init *next;	   /* next init in the chain */
};
struct object {
    char *name;		   /* name of object */
    char *base_name;	   /* base name of object */
    struct init *inits;    /* a chain of init structs for this object */
    long init_only;	   /* object name only seen in an #init directive */
    unsigned long ninit;   /* number of init structs for this object (set */
			   /*  when building the host object) */
    long object_size;	   /* size of host object file */
    unsigned long nsymbols;/* number of host symbols */
    struct nlist *symbols; /* host symbol table */
    unsigned long string_size;/* size of string table for host symbol table */
    char *strings;	   /* string table for host symbol table */
    struct ref *refs;	   /* linked list of references to other files */
    char *filedef_name;	   /* name of file definition symbol */
    /* symbol and string tables for private undefined exterals */
    unsigned long pu_nsymbols;/* number of private undefined symbols */
    struct nlist *pu_symbols; /* private undefined symbol table */
    unsigned long pu_string_size;/* size of string table for above sym table */
    char *pu_strings;/* string table for above symbol table */
    enum libgcc libgcc_state; /* part of new_libgcc, old_libgcc or not */
    struct object *next;   /* next object in hash table chain */
};
/* list of objects in order to be loaded */
extern struct object **object_list;

/* number of objects in object_list */
extern unsigned long nobject_list;

/*
 * The the pairs of alias symbol names from the #alias directive are stored in
 * a alias structure and are all chained together off of aliases.
 */ 
struct alias {
    char *alias_name;	/* name of symbol to be aliased */
    char *real_name;	/* name of symbol for the aliasing */
    struct alias *next;	/* next alias in the chain */
};
extern struct alias *aliases;

extern void parse_spec(void);

/*
 * Globals declared in target.c
 */
/*
 * These contain the names of the temporary files used for the branch table
 * assembly language source and resulting object.
 */
extern char *branch_source_filename;
extern char *branch_object_filename;
/*
 * This is the name of the temporary file for the library idenification
 * object file.
 */
extern char *lib_id_object_filename;

extern void target(void);

/*
 * Globals declared in host.c
 */
extern char *hostdir;	/* directory name to build host objects in */

extern void host(void);

/*
 * The following stuff was declared in the 1.0 version of <ldsyms.h>
 */

/*
 * The prefix to the library definition symbol name.  The base name of the
 * target object file is the rest of the name.  In release 1.0 the link editor
 * looked for these names if the link editor symbol _SHLIB_INFO was referenced
 * and built a table of the addresses of shared library symbol names for the
 * link editor defined symbol (zero terminated).   The link editor also looked
 * for these symbols when building Mach-O executables and Mach-O shared
 * libraries and read the data for these symbols from the relocatable files to
 * propagate the target names into the load commands. These symbols MUST be
 * shlib_info structs.  As of release 2.0 (cc-17) this information is propagated
 * by Mach-O load commands.  These symbols however can't go away because they
 * take up space in the text of the shared library and to make them remain
 * compatable this symbol must continue to  exist.
 */
#define SHLIB_SYMBOL_NAME	".shared_library_information_"

#define MAX_TARGET_SHLIB_NAME_LEN	64
struct shlib_info {
    long text_addr;
    long text_size;
    long data_addr;
    long data_size;
    long minor_version;
    char name[MAX_TARGET_SHLIB_NAME_LEN];
};
#define SHLIB_STRUCT \
"	.long %ld\n \
	.long %d\n \
	.long %ld\n \
	.long %d\n \
	.long %ld\n \
	.ascii \"%s\\0\"\n \
	.space %lu\n"
