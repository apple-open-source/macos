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
#if defined(__MWERKS__) && !defined(__private_extern__)
#define __private_extern__ __declspec(private_extern)
#endif

#import "stuff/ofile.h"

/*
 * The input files are broken out in to their object files and then placed in
 * these structures.  These structures are then used to edit the object files'
 * symbol table.  And then finally used to reassemble the object file for
 * output.
 */
struct arch {
    char *file_name;		/* name of file this arch came from */
    enum ofile_type type;	/* The type of file for this architecture */
				/*  can be OFILE_ARCHIVE, OFILE_Mach_O or */
    				/*  OFILE_UNKNOWN. */
    struct fat_arch *fat_arch;	/* If this came from fat file this is valid */
			        /*  and not NULL (needed for the align value */
				/*  and to output a fat file if only one arch)*/
    char *fat_arch_name;	/* If this came from fat file this is valid */
				/*  and is tthe name of this architecture */
				/*  (used for error messages). */

    /* if this is an archive: the members of this archive */
    struct member *members;	/* the members of the library for this arch */
    unsigned long nmembers;	/* the number of the above members */
    /*
     * The output table of contents (toc) for this arch in the library (this
     * must be recreated, or at least the time of the toc member set, when
     * the output is modified because modifiy time is shared by all libraries
     * in the file).
     */
    unsigned long  toc_size;	/* total size of the toc including ar_hdr */
    struct ar_hdr  toc_ar_hdr;	/* the archive header for this member */
    enum bool      toc_long_name;/* use the long name in the output */
    char	  *toc_name;	 /* name of toc member */
    unsigned long  toc_name_size;/* size of name of toc member */
    struct ranlib *toc_ranlibs;	/* ranlib structs */
    unsigned long  toc_nranlibs;/* number of ranlib structs */
    char	  *toc_strings;	/* strings of symbol names for ranlib structs */
    unsigned long  toc_strsize;	/* number of bytes for the strings above */
    unsigned long library_size;	/* current working size and final output size */
				/*  for this arch when it's a library (used */
				/*  for creating the toc entries). */

    /* if this is an object file: the object file */
    struct object *object;	/* the object file */

    /* if this is an unknown file: the addr and size of the file */
    char *unknown_addr;
    unsigned long unknown_size;

    /* don't update LC_ID_DYLIB timestamp */
    enum bool dont_update_LC_ID_DYLIB_timestamp;
};

struct member {
    enum ofile_type type;	/* the type of this member can be OFILE_Mach_O*/
				/*  or OFILE_UNKNOWN */
    struct ar_hdr *ar_hdr;	/* the archive header for this member */
    unsigned long offset;	/* current working offset and final offset */
				/*  use in creating the table of contents */

    /* the name of the member in the output */
    char         *member_name;	    /* the member name */
    unsigned long member_name_size; /* the size of the member name */
    enum bool     member_long_name; /* use the extended format #1 for the
				       member name in the output */

    /* if this member is an object file: the object file */
    struct object *object;	/* the object file */

    /* if this member is an unknown file: the addr and size of the member */
    char *unknown_addr;
    unsigned long unknown_size;

    /*
     * If this member was created from a file then input_file_name is set else
     * it is NULL and input_ar_hdr is set (these are recorded to allow
     * warn_member() messages to be printed)
     */
    char *input_file_name;
    struct ar_hdr *input_ar_hdr;
};

struct object {
    char *object_addr;		    /* the address of the object file */
    unsigned long object_size;	    /* the size of the object file on input */
    enum byte_sex object_byte_sex;  /* the byte sex of the object file */
    struct mach_header *mh;	    /* the mach_header of the object file */
    struct load_command		    /* the start of the load commands */
	*load_commands;
    struct symtab_command *st;	    /* the symbol table command */
    struct dysymtab_command *dyst;  /* the dynamic symbol table command */
    struct twolevel_hints_command   /* the two-level namespace hints command */
	*hints_cmd;
    struct prebind_cksum_command *cs;/* the prebind check sum command */
    struct segment_command
	*seg_linkedit;	    	    /* the link edit segment command */
    struct section **sections;	    /* array of section structs */

    /*
     * This is only used for redo_prebinding and is calculated by breakout()
     * if the calculate_input_prebind_cksum parameter is TRUE and there is an
     * LC_PREBIND_CKSUM load command that has a zero value for the cksum field
     * (if so this will be value of the cksum field on output).
     */
    unsigned long calculated_input_prebind_cksum;

    unsigned long input_sym_info_size;
    unsigned long output_sym_info_size;

    struct nlist *output_symbols;
    unsigned long output_nsymbols;
    char	 *output_strings;
    unsigned long output_strings_size;

    unsigned long output_ilocalsym;
    unsigned long output_nlocalsym;
    unsigned long output_iextdefsym;
    unsigned long output_nextdefsym;
    unsigned long output_iundefsym;
    unsigned long output_nundefsym;

    struct twolevel_hint *output_hints;

    struct relocation_info *output_loc_relocs;
    struct relocation_info *output_ext_relocs;
    unsigned long *output_indirect_symtab;

    struct dylib_table_of_contents *output_tocs;
    unsigned long output_ntoc;
    struct dylib_module *output_mods;
    unsigned long output_nmodtab;
    struct dylib_reference *output_refs;
    unsigned long output_nextrefsyms;
};

__private_extern__ struct ofile * breakout(
    char *filename,
    struct arch **archs,
    unsigned long *narchs,
    enum bool calculate_input_prebind_cksum);

__private_extern__ void free_archs(
    struct arch *archs,
    unsigned long narchs);

__private_extern__ void writeout(
    struct arch *archs,
    unsigned long narchs,
    char *output,
    unsigned short mode,
    enum bool sort_toc,
    enum bool commons_in_toc,
    enum bool library_warnings,
    unsigned long *throttle);

__private_extern__ void checkout(
    struct arch *archs,
    unsigned long narchs);

void warning_arch(
    struct arch *arch,
    struct member *member,
    char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 3, 4)))
#endif
    ;

void error_arch(
    struct arch *arch,
    struct member *member,
    char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 3, 4)))
#endif
    ;

void fatal_arch(
    struct arch *arch,
    struct member *member,
    char *format, ...)
#ifdef __GNUC__
    __attribute__ ((format (printf, 3, 4)))
#endif
    ;
