/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
#undef HOST_DEBUG
#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include <mach-o/ranlib.h>
#include <ar.h>
#include <ctype.h>
#include <strings.h>
#include <mach/mach.h>
#include "stuff/openstep_mach.h"
#include <libc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/hash_string.h"

#include "libgcc.h"
#include "mkshlib.h"

extern char *mktemp();

/*
 * The prefix to the file definition symbol names.  The base name of the
 * object file is the rest of the name.  The user should never need to link
 * to this symbol thus it should not be accessable from a high level language.
 */
#define FILEDEF_NAME	".file_definition_"

/*
 * This is used to mark private undefined externals inside the scan_objects()
 * routine.  It is assigned to the n_type field of a nlist structure.  It is
 * a bit hopefull that this will never become a leagal value of an n_type.
 */
#define N_PRIVATE_UNDF	0xff

/*
 * This structure holds the information of an external symbol.  The value is
 * the value from the target shared library.  If bp is not zero then it points
 * at the branch structure for this symbol and the value of the branch table
 * entry is in branch_value.
 */
struct ext {
    char *name;		  /* name of the symbol */
    long value;		  /* absolute value of the symbol */
    struct branch *bp;	  /* if non-zero the branch table slot for this sym */
    long branch_value;	  /* value of branch table slot if above is non-zero */
    struct object *op;	  /* object this symbol is defined in */
    struct ext *alias;	  /* if non-zero the real symbol for this alias */
    long private;	  /* This symbol to to be private, not put in host */
    struct ext *next;	  /* next ext on the hash chain */
};
#define EXT_HASH_SIZE	251
struct ext *ext_hash[EXT_HASH_SIZE];

/*
 * This structure notes a reference from one object file to another.  It is
 * pointed to by the refs field in the object structure and is a linked list.
 * The name is the file definition symbol for the object that hold the
 * definining reference for the reference the object makes.  This linked list
 * of references is built by make_references.
 */
struct ref {
    char *name;
    struct ref *next;
};

/*
 * The mach header of the target shared library.  This is read in make_exthash()
 * The segment commands from this information are used in write_lib_obj() to
 * build the library definition object.  There sections are removed and their
 * flags are set to SG_FVMLIB.
 */
static struct mach_header target_mh;
static struct load_command *load_commands;

/*
 * The information of the library definition object.  This information is
 * calculated in build_lib_obj() and then used in write_lib_obj().
 */
static struct lib_object {
    long object_size;		/* size of the object file */
    long nfvmsegs;		/* the number of segment commands */
    long fvmlib_name_size;	/* the size of the name in the LC_LOADFVMLIB */
    long string_size;		/* the size of the string table */
} lib_object = { 0 };

/*
 * The information of the library full reference object.  This information is
 * calculated in build_ref_obj() and then used in write_ref_obj().
 */
static struct ref_object {
    unsigned long object_size;		/* size of the object file */
    unsigned long string_size;		/* the size of the string table */
} ref_object = { 0 };

/*
 * The information for the table of contents of the host shared library.  The
 * number of ranlib structures and the size of their strings are calulated as
 * objects going into the host library are built.  Then the ranlib structs and
 * their string are set in build_archive() .
 */
static struct toc_object {
    unsigned long object_size;	/* size of the object file */
    unsigned long nranlibs;	/* number of ranlib structs */
    struct ranlib *ranlibs;	/* the ranlib structs */
    unsigned long ranlib_string_size;/* size of the strings for the structs */
    char *ranlib_strings;	/* the strings for the ranlib structs */
} toc_object = { 0 };

/*
 * The size of the host shared library.  Calculated in build_archive() .
 */
static long host_lib_size = 0;

static void make_exthash(
    void);
static void scan_objects(
    void);
static void scan_objects_processor(
    struct ofile *ofile,
    char *arch_name,
    void *cookie);
static void make_references(
    void);

static void build_archive(
    void);
static int ranlib_qsort(
    const struct ranlib *ran1,
    const struct ranlib *ran2);
static void build_lib_object(
    void);
static void build_ref_object(
    void);
static void build_host_objects(
    void);
static void init_symbol(
    struct object *op,
    char *name,
    struct relocation_info *reloc,
    long *value,
    long *defined,
    long symbolnum,
    long address);
static void write_archive(
    void);
static void write_lib_obj(
    char *buffer);
static void write_ref_obj(
    char *buffer);
static void write_object(
    struct object *op,
    char *buffer);

/*
 * Build the host shared library.
 */
void
host()
{
	/*
 	 * Make the hash table for all external symbols in the library.
	 */
	make_exthash();

	/*
 	 * Scan all the objects and collect their base symbol tables.
	 */
	scan_objects();

	/*
 	 * Make all the references between the host objects.
	 */
	make_references();

	/*
	 * Build the components of the archive which is the host shared library.
	 */
	build_archive();

	/*
	 * Write the components of the archive which is the host shared library.
	 */
	write_archive();
}

/*
 * make_exthash() builds the hash table of all the external symbols in the
 * target shared library.
 */
static
void
make_exthash(void)
{
    struct ofile ofile;
    unsigned int string_size, hash_key, i, nsyms;
    struct nlist *symbols;
    char *strings, *p;
    struct ext *extp, *bextp;
    struct branch *bp;
    struct alias *ap;
    struct load_command *lc;
    struct symtab_command *st;
    struct oddball *obp;


	/*
	 * Map the target shared library and check to see if is is a shared
	 * library and has a symbol table.
	 */
	if(ofile_map(target_filename, NULL, NULL, &ofile, FALSE) == FALSE)
	    exit(1);

	if(ofile.file_type != OFILE_Mach_O || ofile.mh->filetype != MH_FVMLIB)
	    fatal("file: %s is not a target shared library", target_name);

	target_mh = *(ofile.mh);
	load_commands = ofile.load_commands;

	/* Get the load command for the symbol table */
	lc = ofile.load_commands;
	st = (struct symtab_command *)0;
	for(i = 0; i < ofile.mh->ncmds; i++){
	    switch(lc->cmd){ 
	    case LC_SYMTAB:
		if(st != (struct symtab_command *)0)
		    fatal("More than one symtab_command in: %s", target_name);
		else
		    st = (struct symtab_command *)lc;
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/* Check to see if the symbol table load command is good */
	if(st == (struct symtab_command *)0)
	    fatal("No symtab_command in: %s", target_name);
	if(st->nsyms == 0)
	    fatal("No symbol table in: %s", target_name);
	if(st->strsize == 0)
	    fatal("No string table in: %s", target_name);
	symbols = (struct nlist *)(ofile.object_addr + st->symoff);
	nsyms = st->nsyms;
	if(ofile.object_byte_sex != host_byte_sex)
	    swap_nlist(symbols, nsyms, host_byte_sex);
	strings = (char *)(ofile.object_addr + st->stroff);
	string_size = st->strsize;

	/*
	 * Now enter each external symbol in the external hash table.
	 */
	for(i = 0; i < nsyms; i++){
	    /* if it is not an external symbol skip it */
	    if((symbols[i].n_type & N_EXT) == 0)
		continue;
	    if(symbols[i].n_un.n_strx < 0 ||
	       (unsigned long)symbols[i].n_un.n_strx > string_size)
		fatal("bad string index for symbol %d in target shared "
		      "library: %s", i, target_filename);
	    symbols[i].n_un.n_name = strings + symbols[i].n_un.n_strx;
	    hash_key = hash_string(symbols[i].n_un.n_name) % EXT_HASH_SIZE;
	    extp = ext_hash[hash_key];
	    while(extp != (struct ext *)0){
		if(strcmp(symbols[i].n_un.n_name, extp->name) == 0)
		    fatal("symbol %s appears more than once in target shared "
			  "library: %s", extp->name, target_filename);
		extp = extp->next;
	    }
	    extp = allocate(sizeof(struct ext));
	    extp->name = savestr(symbols[i].n_un.n_name);
	    extp->value = symbols[i].n_value;
	    extp->bp = (struct branch *)0;
	    extp->branch_value = BAD_ADDRESS;
	    extp->op = (struct object *)0;
	    extp->alias = (struct ext *)0;
	    extp->private = 0;
	    extp->next = ext_hash[hash_key];
	    ext_hash[hash_key] = extp;

	    /*
	     * If this symbol itself is not a branch slot symbol or the library
	     * definition symbol then see if this symbol has a branch table
	     * entry and record it.
	     */
	    if(is_branch_slot_symbol(extp->name))
		continue;
	    if(is_libdef_symbol(extp->name))
		continue;
	    hash_key = hash_string(extp->name) % BRANCH_HASH_SIZE;
	    bp = branch_hash[hash_key];
	    while(bp != (struct branch *)0){
		if(strcmp(bp->name, extp->name) == 0)
		    break;
		bp = bp->next;
	    }
	    extp->bp = bp;

	    hash_key = hash_string(extp->name) % ODDBALL_HASH_SIZE;
	    obp = oddball_hash[hash_key];
	    while(obp != (struct oddball *)0){
		if(strcmp(obp->name, extp->name) == 0)
		    break;
		obp = obp->next;
	    }
	    if(obp != (struct oddball *)0 && obp->private)
		extp->private = 1;

	    /*
	     * The text section is always section 1 because the branch table
	     * object is loaded first and contains only text.
	     */
	    if((symbols[i].n_type & N_TYPE) == N_SECT &&
	        symbols[i].n_sect == 1){
		if(extp->bp == (struct branch *)0 &&
		   (obp == (struct oddball *)0 ||
		    (obp != (struct oddball *)0 &&
		    (obp->private == 0 && obp->nobranch == 0)) ) )
		    error("external text symbol: %s does not have an entry in "
			  "the branch table", extp->name);
	    }
	    else{
		if(extp->bp != (struct branch *)0)
		    error("non external text symbol: %s has an entry in "
			  "the branch table", extp->name);
	    }
	}

	/*
	 * Put the aliased symbols in the external hash table and look up
	 * their real symbols.
	 */
	ap = aliases;
	while(ap != (struct alias *)0){
	    hash_key = hash_string(ap->alias_name) % EXT_HASH_SIZE;
	    extp = ext_hash[hash_key];
	    while(extp != (struct ext *)0){
		if(strcmp(ap->alias_name, extp->name) == 0)
		    break;
		extp = extp->next;
	    }
	    if(extp == (struct ext *)0)
		fatal("aliased symbol %s does not appears (that should) in "
		      "target shared library: %s", extp->name, target_filename);

	    hash_key = hash_string(ap->real_name) % EXT_HASH_SIZE;
	    extp->alias = ext_hash[hash_key];
	    while(extp->alias != (struct ext *)0){
		if(strcmp(ap->real_name, extp->alias->name) == 0)
		    break;
		extp->alias = extp->alias->next;
	    }
	    if(extp->alias == (struct ext *)0)
		fatal("real symbol %s for aliased symbol %s not found in "
		      "target shared library\n", ap->real_name,
		      extp->alias->name);

	    ap = ap->next;
	}
	
	/*
	 * For all external symbols with branch table entries look up the
	 * branch slot symbol and record its value.
	 */
	for(i = 0; i < EXT_HASH_SIZE; i++){
	    extp = ext_hash[i];
	    while(extp != (struct ext *)0){
		if(extp->bp != (struct branch *)0){
		    p = branch_slot_symbol(extp->bp->max_slotnum);
		    hash_key = hash_string(p) % EXT_HASH_SIZE;
		    bextp = ext_hash[hash_key];
		    while(bextp != (struct ext *)0){
			if(strcmp(p, bextp->name) == 0)
			    break;
			bextp = bextp->next;
		    }
		    if(bextp == (struct ext *)0)
			fatal("branch table symbol for: %s not found in target "
			      "shared library: %s", extp->name,target_filename);
		    extp->branch_value = bextp->value;
		}
		extp = extp->next;
	    }
	}

#ifdef HOST_DEBUG
	printf("Just after building external symbol table\n");
	for(i = 0; i < EXT_HASH_SIZE; i++){
	    extp = ext_hash[i];
	    while(extp != (struct ext *)0){
		printf("\textp = 0x%x { %s, 0x%x, 0x%x, 0x%x, 0x%x }\n", extp,
			extp->name, extp->value, extp->bp, extp->branch_value,
			extp->next);
		if(extp->bp != (struct branch *)0)
		    printf("\t\tbp = 0x%x { %s, %d, 0x%x }\n", extp->bp,
			   extp->bp->name, extp->bp->max_slotnum,
			   extp->bp->next);
		extp = extp->next;
	    }
	}
#endif /* HOST_DEBUG */
}

/*
 * This routine reads each object and creates a symbol table for the
 * basis of the host object file for each object.  Only external symbols
 * are put in the symbol table.
 */
static
void
scan_objects(void)
{

    unsigned long i;

	for(i = 0; i < nobject_list; i++){
	    ofile_process(object_list[i]->name, &arch_flag, 1, FALSE, FALSE,
			  TRUE, FALSE, scan_objects_processor, (void *)&i);
	}
}

static
void
scan_objects_processor(
struct ofile *ofile,
char *arch_name,
void *cookie)
{
    unsigned long i, j, k, l, nsyms, string_size, hash_key;
    struct load_command *lc;
    struct symtab_command *st;
    struct nlist *symbols;
    char *strings, *p, *q;
    struct ext *extp;

	i = *((long *)cookie);
	st = NULL;
	lc = ofile->load_commands;
	for(j = 0; j < ofile->mh->ncmds; j++){
	    if(lc->cmd == LC_SYMTAB){
		if(st != NULL)
		    fatal("more than one symtab command in object file: %s",
			  ofile->file_name);
		st = (struct symtab_command *)lc;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(st == NULL)
	    return;

	/*
	 * Size the number of external symbols and their strings.  The first
	 * byte of the string table can't be used for a non-null named
	 * symbol so the string table size starts with one byte.
	 */
	object_list[i]->nsymbols = 0;
	object_list[i]->string_size = 1;

	if(st->nsyms == 0){
	    return;
	}

	/* read the symbol table */
	symbols = (struct nlist *)(ofile->object_addr + st->symoff);
	nsyms = st->nsyms;
	if(ofile->object_byte_sex != host_byte_sex)
	    swap_nlist(symbols, nsyms, host_byte_sex);

	/* read the string table */
	strings = ofile->object_addr + st->stroff;
	string_size = st->strsize;
/*
printf("processing object = %s\n", ofile->file_name);
*/
	for(j = 0; j < nsyms; j++){
	    /* if it is not an external symbol skip it */
	    if((symbols[j].n_type & N_EXT) == 0)
		continue;
	    if(symbols[j].n_un.n_strx < 0 ||
	       (unsigned long)symbols[j].n_un.n_strx > string_size)
		fatal("bad string index for symbol %ld in object file: %s ",
		      j, ofile->file_name);
	    symbols[j].n_un.n_name = strings + symbols[j].n_un.n_strx;

	    /*
	     * If this symbol is aliased to another symbol then change
	     * it's name to the real symbol.
	     */
	    p = symbols[j].n_un.n_name;
	    hash_key = hash_string(p) % EXT_HASH_SIZE;
	    extp = ext_hash[hash_key];
	    while(extp != (struct ext *)0){
		if(strcmp(p, extp->name) == 0)
		    break;
		extp = extp->next;
	    }
	    if(extp == (struct ext *)0)
		fatal("symbol: %s in object: %s not found in target "
		      "shared library: %s", p, ofile->file_name,
		      target_filename);
	    if(extp->alias != (struct ext *)0){
		symbols[j].n_un.n_name = extp->alias->name;
	    }
	    else{
		/*
		 * This is the real symbol and not an alias so record
		 * which object it is defined in.  That is set op field
		 * in the external symbol table.
		 */
		if((symbols[j].n_type & N_TYPE) != N_UNDF){
		    if(extp->op != (struct object *)0)
			fatal("symbol: %s defined in both: %s and %s", p,
			      ofile->file_name, extp->op->name);
		    extp->op = object_list[i];
		}
	    }

	    /*
	     * See if this symbol is listed as a private extern.  If it is
	     * also an undefined symbol then mark it by changing the type
	     * to N_PRIVATE_UNDF and added it to the size of the private
	     * undefined exteral symbol table.  If it is not undefined just
	     * turn off the extern bit so the next loop will not put in in
	     * the host shared library's symbol table.
	     */
	    if(extp->private ||
	       (extp->alias != (struct ext *)0 && extp->alias->private) ){
		if(symbols[j].n_type == (N_UNDF|N_EXT)){
		    object_list[i]->pu_nsymbols++;
		    object_list[i]->pu_string_size +=
					strlen(symbols[j].n_un.n_name) + 1;
/*
printf("private undefined = %s\n", symbols[j].n_un.n_name);
*/
		    symbols[j].n_type = N_PRIVATE_UNDF;
		}
		else{
/*
printf("private defined = %s\n", symbols[j].n_un.n_name);
*/
		    symbols[j].n_type &= ~N_EXT;
		}
		continue;
	    }

	    object_list[i]->nsymbols++;
	    object_list[i]->string_size +=
				    strlen(symbols[j].n_un.n_name) + 1;
	}

	/*
	 * Save the of external symbols and their strings to be used
	 * as the basis of the host object file for this object file.
	 */
	object_list[i]->symbols = allocate(object_list[i]->nsymbols *
					   sizeof(struct nlist));
	object_list[i]->strings = allocate(object_list[i]->string_size);
	object_list[i]->pu_symbols = allocate(object_list[i]->pu_nsymbols *
					      sizeof(struct nlist));
	object_list[i]->pu_strings =
			    allocate(object_list[i]->pu_string_size);
	k = 0;
	p = object_list[i]->strings;
	*p++ = '\0'; /* the first byte */
	l = 0;
	q = object_list[i]->pu_strings;
	for(j = 0; j < nsyms; j++){
	    if(symbols[j].n_type == N_PRIVATE_UNDF){
		object_list[i]->pu_symbols[l] = symbols[j];
		strcpy(q, symbols[j].n_un.n_name);
		object_list[i]->pu_symbols[l].n_un.n_name = q;
/*
printf("recording private undefined = %s value = 0x%x\n",
object_list[i]->pu_symbols[l].n_un.n_name, 
object_list[i]->pu_symbols[l].n_value);
*/
		l++;
		q += strlen(symbols[j].n_un.n_name) + 1;
		continue;
	    }
	    /* again if it is not an external symbol skip it */
	    if((symbols[j].n_type & N_EXT) == 0)
		continue;
	    object_list[i]->symbols[k] = symbols[j];
	    strcpy(p, symbols[j].n_un.n_name);
	    object_list[i]->symbols[k].n_un.n_name = p;
	    k++;
	    p += strlen(symbols[j].n_un.n_name) + 1;
	}

#ifdef HOST_DEBUG
	printf("Symbol tables of host objects\n");
	for(i = 0; i < nobject_list; i++){
	    printf("\tobject = %s\n", ofile->file_name);
	    for(j = 0;
		j < object_list[i]->nsymbols;
		j++){
		printf("\t\tsymbol = %s value = 0x%x\n",
			object_list[i]->symbols[j].n_un.n_name, 
			object_list[i]->symbols[j].n_value);
	    }
	    for(j = 0;
		j < object_list[i]->pu_nsymbols;
		j++){
		printf("\t\tprivate undefined = %s value = 0x%x\n",
			object_list[i]->pu_symbols[j].n_un.n_name, 
			object_list[i]->pu_symbols[j].n_value);
	    }
	}
#endif /* HOST_DEBUG */
}

/*
 * This routine records the references between objects.  It creates a file
 * definition symbol name for each file and builds a reference to that for
 * objects that references symbols in other objects.  In this way it will
 * guarentee that those objects get loaded and cause any multiply defined
 * error thus to avoid building an executable that uses two different things
 * for the same symbol.
 */
static
void
make_references(void)
{
    unsigned long i, j, hash_key;
    struct ext *extp;
    struct ref *refp;
    char *p;

#ifdef HOST_DEBUG
	printf("External symbol table with defining references\n");
	for(i = 0; i < nobject_list; i++){
	    extp = ext_hash[i];
	    while(extp != (struct ext *)0){
		if(extp->op != (struct object *)0)
		    printf("\text =  %s defined in %s\n",
			   extp->name, extp->op->name);
		else
		    printf("\text =  %s not defined in any object\n",
			   extp->name);
		extp = extp->next;
	    }
	}
#endif /* HOST_DEBUG */

	/*
	 * Create a file definition symbol name for each host object file.
	 */
	for(i = 0; i < nobject_list; i++){
	    object_list[i]->filedef_name =
		makestr(FILEDEF_NAME, target_base_name, "_",
			object_list[i]->base_name, (char *)0);
	}

	/*
	 * Build a linked list of references for each object file that has
	 * undefined references that are defined in other objects in the
	 * shared library.
	 */
	for(i = 0; i < nobject_list; i++){
	    for(j = 0;
		j < object_list[i]->nsymbols;
		j++){
		if((object_list[i]->symbols[j].n_type & N_TYPE) == N_UNDF){
		    p = object_list[i]->symbols[j].n_un.n_name;
		    hash_key = hash_string(p) % EXT_HASH_SIZE;
		    extp = ext_hash[hash_key];
		    while(extp != (struct ext *)0){
			if(strcmp(p, extp->name) == 0)
			    break;
			extp = extp->next;
		    }
		    if(extp == (struct ext *)0)
			fatal("symbol: %s in object: %s not found in target "
			      "shared library: %s", p, object_list[i]->name,
			      target_filename);
		    /*
		     * If this symbol is an alias then uses the real symbol
		     * for this symbol.
		     */
		    if(extp->alias != (struct ext *)0)
			extp = extp->alias;

		    /*
		     * If this symbol is defined in a object in the shared
		     * library make sure there that object's file definition
		     * symbol name is on the reference list.  If not add it.
		     */
		    if(extp->op != (struct object *)0){
			refp = object_list[i]->refs;
			while(refp != (struct ref *)0){
			    if(strcmp(refp->name, extp->op->filedef_name) == 0)
				break;
			    refp = refp->next;
			}
			if(refp == (struct ref *)0){
			    refp = allocate(sizeof(struct ref));
			    refp->name = extp->op->filedef_name;
			    refp->next = object_list[i]->refs;
			    object_list[i]->refs = refp;
			}
		    }
		}
	    }
	    for(j = 0;
		j < object_list[i]->pu_nsymbols;
		j++){
		p = object_list[i]->pu_symbols[j].n_un.n_name;
		hash_key = hash_string(p) % EXT_HASH_SIZE;
		extp = ext_hash[hash_key];
		while(extp != (struct ext *)0){
		    if(strcmp(p, extp->name) == 0)
			break;
		    extp = extp->next;
		}
		if(extp == (struct ext *)0)
		    fatal("symbol: %s in object: %s not found in target "
			  "shared library: %s", p, object_list[i]->name,
			  target_filename);
		/*
		 * If this symbol is an alias then uses the real symbol
		 * for this symbol.
		 */
		if(extp->alias != (struct ext *)0)
		    extp = extp->alias;

		/*
		 * If this symbol is defined in a object in the shared
		 * library make sure there that object's file definition
		 * symbol name is on the reference list.  If not add it.
		 */
		if(extp->op != (struct object *)0){
		    refp = object_list[i]->refs;
		    while(refp != (struct ref *)0){
			if(strcmp(refp->name, extp->op->filedef_name) == 0)
			    break;
			refp = refp->next;
		    }
		    if(refp == (struct ref *)0){
			refp = allocate(sizeof(struct ref));
			refp->name = extp->op->filedef_name;
			refp->next = object_list[i]->refs;
			object_list[i]->refs = refp;
		    }
		}
	    }
	}

#ifdef HOST_DEBUG
	printf("References from host objects to each other\n");
	for(i = 0; i < nobject_list; i++){
	    printf("\tObject: %s references:\n", object_list[i]->name);
	    refp = object_list[i]->refs;
	    while(refp != (struct ref *)0){
		printf("\t\t%s\n", refp->name);
		refp = refp->next;
	    }
	}
#endif /* HOST_DEBUG */
}

/*
 * build_archive causes all the component parts of the archive that is the
 * host shared library to be built.
 */
static
void
build_archive(void)
{
    unsigned long ran_index, ran_strx, ran_off, i, j;
    struct object *op;
    char *symbol_name;

	/*
	 * Build the componets of the object that contains library definition
	 * symbol.
	 */
	build_lib_object();

	/*
	 * Build the componets of the object that contains library full
	 * reference symbol.
	 */
	build_ref_object();

	/*
	 * Build the components of the host object files.
	 */
	build_host_objects();

	/*
	 * The rest of this routine builds the last part of the host library
	 * that needs to be built after all it's object's are built.  This is
	 * the table of contents.  After this is built then the size of the
	 * host library `host_lib_size' can be calculated and is set at the end.
	 */
	toc_object.ranlibs = allocate(toc_object.nranlibs *
				      sizeof(struct ranlib));
	toc_object.ranlib_strings =
		allocate(round(toc_object.ranlib_string_size, sizeof(long)));
	/* make sure the rounded area allways has the same value */
	memset(toc_object.ranlib_strings + toc_object.ranlib_string_size, '\0',
	       round(toc_object.ranlib_string_size, sizeof(long)) -
	       toc_object.ranlib_string_size);
	toc_object.object_size =
	    sizeof(long) +
	    toc_object.nranlibs * sizeof(struct ranlib) +
	    sizeof(long) +
	    round(toc_object.ranlib_string_size, sizeof(long));

	ran_index = 0;
	ran_strx = 0;
	ran_off = SARMAG + sizeof(struct ar_hdr) + toc_object.object_size;

	/*
	 * The library definition symbol is defined in the library definition
	 * object (which is the first member of the archive after the table
	 * of contents).
	 */
	toc_object.ranlibs[ran_index].ran_un.ran_strx = ran_strx;
	toc_object.ranlibs[ran_index].ran_off = ran_off;
	strcpy(toc_object.ranlib_strings + ran_strx, shlib_symbol_name);
	ran_index++;
	ran_strx += strlen(shlib_symbol_name) + 1;
	ran_off += sizeof(struct ar_hdr) + lib_object.object_size;

	/*
	 * The library reference symbol is defined in the library reference
	 * object (which is the second member of the archive after the table
	 * of contents).
	 */
	toc_object.ranlibs[ran_index].ran_un.ran_strx = ran_strx;
	toc_object.ranlibs[ran_index].ran_off = ran_off;
	strcpy(toc_object.ranlib_strings + ran_strx, shlib_reference_name);
	ran_index++;
	ran_strx += strlen(shlib_reference_name) + 1;
	ran_off += sizeof(struct ar_hdr) + ref_object.object_size;

	/*
	 * For each defined symbol in each object file add it to the table of
	 * contents.  Note there are only undefined and absolute symbols in the
	 * symbol tables.
	 */
	for(i = 0; i < nobject_list; i++){
	    op = object_list[i];
	    for(j = 0; j < op->nsymbols; j++){
		if((op->symbols[j].n_type & N_TYPE) != N_UNDF){
		    symbol_name = op->strings + op->symbols[j].n_un.n_strx;
		    toc_object.ranlibs[ran_index].ran_un.ran_strx = ran_strx;
		    toc_object.ranlibs[ran_index].ran_off = ran_off;
		    strcpy(toc_object.ranlib_strings + ran_strx, symbol_name);
		    ran_index++;
		    ran_strx += strlen(symbol_name) + 1;
		}
	    }
	    ran_off += sizeof(struct ar_hdr) + op->object_size;
	}
	host_lib_size = ran_off;

	if(ran_index != toc_object.nranlibs)
	    fatal("internal error: ran_index (%ld) not equal to "
		  "toc_object.nranlibs (%ld)", ran_index, toc_object.nranlibs);
	if(ran_strx != toc_object.ranlib_string_size)
	    fatal("internal error: ran_strx (%ld) not equal to "
		  "toc_object.ranlib_string_size (%ld)", ran_strx,
		  toc_object.ranlib_string_size);
	toc_object.ranlib_string_size = round(toc_object.ranlib_string_size,
					      sizeof(long));

	/*
	 * This can be sorted without fear of having the same symbol defined
	 * in more than one object file because these symbols come from the
	 * target library and there can't be multiply defined symbols.
	 */
	qsort(toc_object.ranlibs, toc_object.nranlibs, sizeof(struct ranlib),
	      (int (*)(const void *, const void *))ranlib_qsort);
}

/*
 * Function for qsort() for comparing ranlib structures by name.
 */
static
int
ranlib_qsort(
const struct ranlib *ran1,
const struct ranlib *ran2)
{
	return(strcmp(toc_object.ranlib_strings + ran1->ran_un.ran_strx,
		      toc_object.ranlib_strings + ran2->ran_un.ran_strx));
}

/*
 * build_lib_object builds the object file that contains the shared library
 * definition symbol.  It is later put in the host shared library archive.
 */
static
void
build_lib_object(void)
{
    unsigned long i;
    struct load_command *lc;

	/*
	 * This object file has segment commands for the segments of the 
	 * shared library (marked with SG_FLAGS).  These are so the Mach-O
	 * link editor can check the segments for overlap.  
	 */
	lib_object.nfvmsegs = 0;
	lc = (struct load_command *)load_commands;
	for(i = 0; i < target_mh.ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		lib_object.nfvmsegs++;
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	/*
	 * This object file also has the load fvmlib command which the link
	 * editor will propagate to its output so the library gets loaded when
	 * the output file is executed.
	 */
	lib_object.fvmlib_name_size = round(strlen(target_name) + 1,
					    sizeof(long));
	/*
	 * This object file has one symbol.  It is the defining occurence of
	 * the shared library definition symbol.  So this is added to the table
	 * of contents.  Since symbol name is the only string in the string
	 * table it starts at byte 1 (because not null symbol names can't be
	 * at byte 0).
	 */
	toc_object.nranlibs++;
	toc_object.ranlib_string_size += strlen(shlib_symbol_name) + 1;
	lib_object.string_size = round(strlen(shlib_symbol_name) + 2,
				       sizeof(long));

	lib_object.object_size =
	    sizeof(struct mach_header) +
	    lib_object.nfvmsegs * sizeof(struct segment_command) +
	    sizeof(struct symtab_command) +
	    sizeof(struct fvmlib_command) +
	    lib_object.fvmlib_name_size +
	    sizeof(struct nlist) + 
	    lib_object.string_size;
}

/*
 * build_ref_object builds the object file that contains the shared library
 * reference symbol.  It is later put in the host shared library archive.
 */
static
void
build_ref_object(void)
{
    unsigned long i;

	/*
	 * This object file has one defined symbol.  It is the defining
	 * occurence of the shared library reference symbol.  So this is added
	 * to the table of contents.
	 */
	toc_object.nranlibs++;
	toc_object.ranlib_string_size += strlen(shlib_reference_name) + 1;

	/*
	 * This object contains an undefined reference to all of the object's
	 * file definition symbols.  So the string table is sized here.
	 * Non-null symbol names start at byte 1 thus the starting size of 1.
	 */
	ref_object.string_size = 1;
	ref_object.string_size += strlen(shlib_reference_name) + 2;
	for(i = 0; i < nobject_list; i++)
	    ref_object.string_size += strlen(object_list[i]->filedef_name) + 1;
	ref_object.string_size = round(ref_object.string_size, sizeof(long));

	ref_object.object_size =
	    sizeof(struct mach_header) +
	    sizeof(struct symtab_command) +
	    sizeof(struct nlist) * (1 + nobject_list) + 
	    ref_object.string_size;
}

/*
 * This routine builds the parts of the host shared library objects.  There will
 * be an object in the host library for each object specified in the #objects
 * list.  These objects have the same global symbols but with their values
 * changed to the absolute value the symbol has in the target shared library.
 * If the symbol has a branch table slot that value is used as the symbol's
 * value.  The other symbols a host object are the it's file definition symbol
 * and references to them and a reference to the library definition symbol.
 */
static
void
build_host_objects(void)
{
    unsigned long i, j, strx, hash_key, len, new_symbols, new_string_size;
    char *p;
    struct object *op;
    struct ref *refp;
    struct ext *extp;
    struct init *ip;

	/*
	 * Build the peices of the host object for each object specified.
	 */
	for(i = 0; i < nobject_list; i++){
	    op = object_list[i];

	    /*
	     * If the symbol is not undefined then change it to an absolute
	     * symbol of the value in the target.  If it has a branch table
	     * entry used the branch value.  Also convert all symbols' names
	     * to string table offsets.  Their strings have been allocated
	     * contiguiously and is used directly as the string table for
	     * the base symbols.
	     */
	    strx = 1; /* room for the first zero byte */
	    for(j = 0; j < op->nsymbols; j++){
		len = strlen(op->symbols[j].n_un.n_name) + 1;
		if((op->symbols[j].n_type & N_TYPE) != N_UNDF){
		    p = op->symbols[j].n_un.n_name;
		    hash_key = hash_string(p) % EXT_HASH_SIZE;
		    extp = ext_hash[hash_key];
		    while(extp != (struct ext *)0){
			if(strcmp(p, extp->name) == 0)
			    break;
			extp = extp->next;
		    }
		    if(extp == (struct ext *)0)
			fatal("symbol: %s in object: %s not found in target "
			      "shared library: %s", p, object_list[i]->name,
			      target_filename);
		    if(extp->bp != (struct branch *)0)
			op->symbols[j].n_value = extp->branch_value;
		    else
			op->symbols[j].n_value = extp->value;
		    op->symbols[j].n_type = N_ABS | N_EXT;
		    op->symbols[j].n_sect = NO_SECT;
		    toc_object.nranlibs++;
		    toc_object.ranlib_string_size += len;
		}
		op->symbols[j].n_un.n_strx = strx;
		strx += len;
	    }

	    /*
	     * Now count the number of new symbols to be added to the base
	     * symbols and the size of their strings.  The symbols for the
	     * initialization are place just after the base symbols and any
	     * external relocation entries for them use this fact so to set
	     * the symbol index (op->nsymbols + new_symbols).  So after the
	     * the size of the new symbol is determined these new symbols must
	     * be added in this order.
	     */
	    new_symbols = 0;
	    /*
	     * If the object has no strings then the first byte of the string
	     * table is allocated here.
	     */
	    if(object_list[i]->string_size == 0)
		new_string_size = 1;
	    else
		new_string_size = 0;
	    op->ninit = 0;
	    if(op->inits != (struct init *)0){
		ip = op->inits;
		while(ip != (struct init *)0){
		    init_symbol(op, ip->sinit, &(ip->sreloc), &(ip->svalue),
			        &(ip->sdefined), op->nsymbols + new_symbols,
				(op->ninit * 2) * sizeof(long));
		    if(!ip->sdefined){
			new_symbols++;
			new_string_size += strlen(ip->sinit) + 1;
		    }
		    init_symbol(op, ip->pinit, &(ip->preloc), &(ip->pvalue),
			        &(ip->pdefined), op->nsymbols + new_symbols,
				(op->ninit * 2 + 1) * sizeof(long));
		    if(!ip->pdefined){
			new_symbols++;
			new_string_size += strlen(ip->pinit) + 1;
		    }
		    op->ninit++;
		    ip = ip->next;
		}
	    }
	    /*
	     * Count the number of symbols for the references and the
	     * size their strings.
	     */
	    refp = op->refs;
	    while(refp != (struct ref *)0){
		new_symbols++;
		new_string_size += strlen(refp->name) + 1;
		refp = refp->next;
	    }
	    /*
	     * Count and size the file definition symbol (this will be defined).
	     */
	    new_symbols++;
	    len = strlen(op->filedef_name) + 1;
	    new_string_size += len;
	    toc_object.nranlibs++;
	    toc_object.ranlib_string_size += len;
	    /*
	     * Count and size the shared library definition symbol.
	     */
	    new_symbols++;
	    new_string_size += strlen(shlib_symbol_name) + 1;


	    /*
	     * Allocate additional room for the new symbols and their strings.
	     */
	    object_list[i]->symbols =
			 reallocate(object_list[i]->symbols,
				    (object_list[i]->nsymbols + new_symbols) *
				    sizeof(struct nlist));
	    object_list[i]->strings =
			reallocate(object_list[i]->strings,
	    			   round(object_list[i]->string_size +
				         new_string_size, sizeof(long)));
	    /* zero the rounded area */
	    memset(object_list[i]->strings +
		   object_list[i]->string_size + new_string_size, '\0',
	    	   round(object_list[i]->string_size + new_string_size,
			 sizeof(long)) -
		   (object_list[i]->string_size + new_string_size) );

	    object_list[i]->string_size = round(object_list[i]->string_size +
					        new_string_size, sizeof(long));
	    /*
	     * Add the new symbols and their strings to the symbol and string
	     * table.  The init symbol must be added first and in this order
	     * because relocation entries are depending on the symbol indexes.
	     * See comments above where the new symbols are counted.
	     */
	    if(op->inits != (struct init *)0){
		ip = op->inits;
		while(ip != (struct init *)0){
		    if(!ip->sdefined){
			op->symbols[op->nsymbols].n_un.n_strx = strx;
			op->symbols[op->nsymbols].n_type = N_UNDF | N_EXT;
			op->symbols[op->nsymbols].n_sect = NO_SECT;
			op->symbols[op->nsymbols].n_desc = 0;
			op->symbols[op->nsymbols].n_value = 0;
			strcpy(op->strings + strx, ip->sinit);
			op->nsymbols++;
			strx += strlen(ip->sinit) + 1;
		    }
		    if(!ip->pdefined){
			op->symbols[op->nsymbols].n_un.n_strx = strx;
			op->symbols[op->nsymbols].n_type = N_UNDF | N_EXT;
			op->symbols[op->nsymbols].n_sect = NO_SECT;
			op->symbols[op->nsymbols].n_desc = 0;
			op->symbols[op->nsymbols].n_value = 0;
			strcpy(op->strings + strx, ip->pinit);
			op->nsymbols++;
			strx += strlen(ip->pinit) + 1;
		    }
		    ip = ip->next;
		}
	    }
	    /*
	     * Add the symbols for the references.
	     */
	    refp = op->refs;
	    while(refp != (struct ref *)0){
		op->symbols[op->nsymbols].n_un.n_strx = strx;
		op->symbols[op->nsymbols].n_type = N_UNDF | N_EXT;
		op->symbols[op->nsymbols].n_sect = NO_SECT;
		op->symbols[op->nsymbols].n_desc = 0;
		op->symbols[op->nsymbols].n_value = 0;
		strcpy(op->strings + strx, refp->name);
		op->nsymbols++;
		strx += strlen(refp->name) + 1;
		refp = refp->next;
	    }
	    /*
	     * Add the file definition symbol (this is defined).
	     */
	    op->symbols[op->nsymbols].n_un.n_strx = strx;
	    op->symbols[op->nsymbols].n_type = N_ABS | N_EXT;
	    op->symbols[op->nsymbols].n_sect = NO_SECT;
	    op->symbols[op->nsymbols].n_desc = 0;
	    op->symbols[op->nsymbols].n_value = 0;
	    strcpy(op->strings + strx, op->filedef_name);
	    op->nsymbols++;
	    strx += strlen(op->filedef_name) + 1;
	    /*
	     * Count and size the shared library definition symbol.
	     */
	    op->symbols[op->nsymbols].n_un.n_strx = strx;
	    op->symbols[op->nsymbols].n_type = N_UNDF | N_EXT;
	    op->symbols[op->nsymbols].n_sect = NO_SECT;
	    op->symbols[op->nsymbols].n_desc = 0;
	    op->symbols[op->nsymbols].n_value = 0;
	    strcpy(op->strings + strx, shlib_symbol_name);
	    op->nsymbols++;
	    strx += strlen(shlib_symbol_name) + 1;

	    /*
	     * Calculate the size of this object file.  It will look like:
	     *	Mach header
	     *	LC_SEGMENT load command
	     *	    the section header (for initialization section)
	     *	LC_SYMTAB load command
	     *	The contents of the initialization section
	     *	The relocation entries for the initialization section
	     *  The symbol table
	     *  The string table
	     */
	    op->object_size = sizeof(struct mach_header) +
			      sizeof(struct segment_command) +
				  sizeof(struct section) +
			      sizeof(struct symtab_command) +
			      op->ninit * 2 * sizeof(long) +
			      op->ninit * 2 * sizeof(struct relocation_info) +
			      op->nsymbols * sizeof(struct nlist) +
			      op->string_size;
	}
}

/*
 * init_symbol() sets a bunch of things for the specified symbol `name' of an
 * #init directive for the object pointed to by `op'.  How the fields get set
 * depends on if the symbol is defined in the object.  The things that get set
 * are the fields of the relocation entry pointed to by 'reloc', the value of
 * the symbol gets set into what is pointed by `value', if the symbol is defined
 * then what is pointed to by `defined' is set to 1 (otherwise set to 0).  To
 * set the symbolnum field of an external relocation entry for a symbol that is
 * not defined the symbol index into the symbol table for the symbol table is
 * needed.  The value of `symbolnum' is the symbol index used.  The caller
 * passes the next symbol after the last symbol index and after init_symbol()
 * returns and `defined' is not set will create a symbol table entry for this
 * symbol at that symbol index.  The value of `address' is the section offset
 * (in bytes) to where the value of the symbol will be in that section.
 */
static
void
init_symbol(
struct object *op,
char *name,
struct relocation_info *reloc,
long *value,
long *defined,
long symbolnum,
long address)
{
    long hash_key;
    struct ext *extp;

	/*
	 * Look up the symbol to see if it is defined and what object file it is
	 * defined in.
	 */
	hash_key = hash_string(name) % EXT_HASH_SIZE;
	extp = ext_hash[hash_key];
	while(extp != (struct ext *)0){
	    if(strcmp(name, extp->name) == 0)
		break;
	    extp = extp->next;
	}
	/*
	 * See if this symbol is defined in this object.
	 */
	if(extp == (struct ext *)0 || extp->op != op){
	    /*
	     * It not is defined in this object so an external relocation entry
	     * will be created.
	     */
	    reloc->r_address = address;
	    reloc->r_symbolnum = symbolnum;
	    reloc->r_pcrel = 0;  /* FALSE */
	    reloc->r_length = 2; /* long */
	    reloc->r_extern = 1; /* TRUE */
	    reloc->r_type = 0;
	    *value = 0;
	    *defined = 0;
	}
	else{
	    /*
	     * It is defined in this object so a local relocation entry
	     * will be created.
	     */
	    reloc->r_address = address;
	    reloc->r_symbolnum = R_ABS;
	    reloc->r_pcrel = 0;  /* FALSE */
	    reloc->r_length = 2; /* long */
	    reloc->r_extern = 0; /* FALSE */
	    reloc->r_type = 0;
	    if(extp->bp != (struct branch *)0)
		*value = extp->branch_value;
	    else
		*value = extp->value;
	    *defined = 1;
	}
}

/*
 * write_archive() causes the host shared library to be written to the host
 * library file.
 */
static
void
write_archive(void)
{
    int fd;
    kern_return_t r;
    char *buffer;
    unsigned long offset;
    struct stat stat;
    long mtime, symdef_mtime, mode;
    uid_t uid;
    gid_t gid;
    struct ar_hdr ar_hdr;
    unsigned long i, l;
    struct object *op;
    struct ranlib *ranlibs;

	/*
	 * Create the file for the host archive and the buffer for it's
	 * contents.
	 */
	(void)unlink(host_filename);
	if((fd = open(host_filename, O_WRONLY | O_CREAT | O_TRUNC, 0666)) == -1)
	    system_fatal("can't create file: %s", host_filename);
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&buffer,
			    host_lib_size, TRUE)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate() buffer for host library of size "
		       "%ld", host_lib_size);

	/*
	 * Set the variables needed for the archive headers.
	 */
	if(fstat(fd, &stat) == -1)
	    system_fatal("can't stat host library file: %s", host_filename);
	mtime = time(0);
	symdef_mtime = (mtime > stat.st_mtime) ? mtime + 5 : stat.st_mtime + 5;
	uid = getuid();
	gid = getgid();
	mode = stat.st_mode & 0777;

	/*
	 * Now start putting the contents of the host library into the buffer.
	 * First goes the archive magic string.
	 */
	offset = 0;
	memcpy(buffer + offset, ARMAG, SARMAG);
	offset += SARMAG;
	/*
	 * The first member of the archive is the table of contents.  Put
	 * its archive header in the buffer.
	 */
	sprintf(buffer + offset, "%-*.*s%-*ld%-*u%-*u%-*o%-*ld%-*s",
		(int)sizeof(ar_hdr.ar_name),
		(int)sizeof(ar_hdr.ar_name),
		    SYMDEF_SORTED,
		(int)sizeof(ar_hdr.ar_date),
		    symdef_mtime,
		(int)sizeof(ar_hdr.ar_uid),
		    (unsigned int)uid,
		(int)sizeof(ar_hdr.ar_gid),
		    (unsigned int)gid,
		(int)sizeof(ar_hdr.ar_mode),
		    (unsigned int)mode,
		(int)sizeof(ar_hdr.ar_size),
		    toc_object.object_size,
		(int)sizeof(ar_hdr.ar_fmag),
		    ARFMAG);
	offset += sizeof(struct ar_hdr);
	/*
	 * Now put the table of contents itself into the buffer
	 */
	l = toc_object.nranlibs * sizeof(struct ranlib);
	if(host_byte_sex != target_byte_sex)
	    l = SWAP_LONG(l);
	*((long *)(buffer + offset)) = l;
	offset += sizeof(long);
	ranlibs = (struct ranlib *)(buffer + offset);
	memcpy(buffer + offset, toc_object.ranlibs,
	       toc_object.nranlibs * sizeof(struct ranlib));
	if(host_byte_sex != target_byte_sex)
	    swap_ranlib(ranlibs, toc_object.nranlibs, target_byte_sex);
	offset += toc_object.nranlibs * sizeof(struct ranlib);
	l = toc_object.ranlib_string_size;
	if(host_byte_sex != target_byte_sex)
	    l = SWAP_LONG(l);
	*((long *)(buffer + offset)) = l;
	offset += sizeof(long);
	memcpy(buffer + offset, toc_object.ranlib_strings,
	       toc_object.ranlib_string_size);
	offset += toc_object.ranlib_string_size;

	/*
	 * The second member of the archive is the library definition object.
	 * Put its archive header in the buffer.
	 */
	sprintf(buffer + offset, "%-*.*s%-*ld%-*u%-*u%-*o%-*ld%-*s",
		(int)sizeof(ar_hdr.ar_name),
		(int)sizeof(ar_hdr.ar_name),
		    "__.FVMLIB",
		(int)sizeof(ar_hdr.ar_date),
		    mtime,
		(int)sizeof(ar_hdr.ar_uid),
		    (unsigned int)uid,
		(int)sizeof(ar_hdr.ar_gid),
		    (unsigned int)gid,
		(int)sizeof(ar_hdr.ar_mode),
		    (unsigned int)mode,
		(int)sizeof(ar_hdr.ar_size),
		    lib_object.object_size,
		(int)sizeof(ar_hdr.ar_fmag),
		    ARFMAG);
	offset += sizeof(struct ar_hdr);
	/*
	 * Call write_lib_obj() to write the library definition object into the
	 * buffer address it is passed.
	 */
	write_lib_obj(buffer + offset);
	offset += lib_object.object_size;

	/*
	 * The third member of the archive is the library reference object.
	 * Put its archive header in the buffer.
	 */
	sprintf(buffer + offset, "%-*.*s%-*ld%-*u%-*u%-*o%-*ld%-*s",
		(int)sizeof(ar_hdr.ar_name),
		(int)sizeof(ar_hdr.ar_name),
		    "__.FVMLIB_REF",
		(int)sizeof(ar_hdr.ar_date),
		    mtime,
		(int)sizeof(ar_hdr.ar_uid),
		    (unsigned int)uid,
		(int)sizeof(ar_hdr.ar_gid),
		    (unsigned int)gid,
		(int)sizeof(ar_hdr.ar_mode),
		    (unsigned int)mode,
		(int)sizeof(ar_hdr.ar_size),
		    ref_object.object_size,
		(int)sizeof(ar_hdr.ar_fmag),
		    ARFMAG);
	offset += sizeof(struct ar_hdr);
	/*
	 * Call write_ref_obj() to write the library reference object into the
	 * buffer address it is passed.
	 */
	write_ref_obj(buffer + offset);
	offset += ref_object.object_size;

	/*
	 * The remaining members of the archive are the host objects.
	 */
	for(i = 0; i < nobject_list; i++){
	    op = object_list[i];
	    /*
	     * Put this object's archive header in the buffer.
	     */
	    sprintf(buffer + offset, "%-*.*s%-*ld%-*u%-*u%-*o%-*ld%-*s",
		    (int)sizeof(ar_hdr.ar_name),
		    (int)sizeof(ar_hdr.ar_name),
			op->base_name,
		    (int)sizeof(ar_hdr.ar_date),
			mtime,
		    (int)sizeof(ar_hdr.ar_uid),
			(unsigned int)uid,
		    (int)sizeof(ar_hdr.ar_gid),
			(unsigned int)gid,
		    (int)sizeof(ar_hdr.ar_mode),
			(unsigned int)mode,
		    (int)sizeof(ar_hdr.ar_size),
			op->object_size,
		    (int)sizeof(ar_hdr.ar_fmag),
			ARFMAG);
	    offset += sizeof(struct ar_hdr);
	    /*
	     * Call write_object() to write the host object into the
	     * buffer at the address passed to it.
	     */
	    write_object(op, buffer + offset);
	    offset += op->object_size;
	}

	/*
	 * Write the buffer to the host archive file and deallocate the buffer.
	 */
	if(write(fd, buffer, host_lib_size) != host_lib_size)
	    system_fatal("can't write host library file: %s", host_filename);
#ifdef OS_BUG
	fsync(fd);
#endif
	if(close(fd) == -1)
	    system_fatal("can't close file: %s", host_filename);
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)buffer,
			      host_lib_size)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_deallocate() buffer for host library");
}

/*
 * write_lib_obj() writes the library definition object into the buffer passed
 * to it.
 */
static
void
write_lib_obj(
char *buffer)
{
    unsigned long offset, i;
    struct mach_header *mh;
    struct load_command *lc, *lib_obj_load_commands;
    struct segment_command *sg;
    struct fvmlib_command *fvmlib;
    struct symtab_command *st;
    struct nlist *symbol;

	offset = 0;

	/*
	 * This file is made up of the following:
	 *	mach header
	 * 	one LC_SEGMENT for each one in the target marked with SG_FVMLIB
	 *	an LC_LOADFVMLIB command for the library
	 *	an LC_SYMTAB command
	 *	one symbol (the library definition symbol)
	 *	the string table (with one string in it)
	 */
	mh = (struct mach_header *)(buffer + offset);
	mh->magic = MH_MAGIC;
	mh->cputype = arch_flag.cputype;
	mh->cpusubtype = arch_flag.cpusubtype;
	mh->filetype = MH_OBJECT;
	mh->ncmds = 2 + lib_object.nfvmsegs;
	mh->sizeofcmds = lib_object.nfvmsegs * sizeof(struct segment_command) +
			 sizeof(struct symtab_command) +
			 sizeof(struct fvmlib_command) +
			 lib_object.fvmlib_name_size;
	mh->flags = MH_NOUNDEFS;
	offset += sizeof(struct mach_header);

	/*
	 * This object file has segment commands for the segments of the 
	 * shared library (marked with SG_FLAGS).  These are so the Mach-O
	 * link editor can check the segments for overlap.  
	 */
	lc = (struct load_command *)load_commands;
	lib_obj_load_commands = (struct load_command *)(buffer + offset);
	for(i = 0; i < target_mh.ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)(buffer + offset);
		*sg = *((struct segment_command *)lc);
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0)
		    sg->vmsize = 0;
		sg->cmdsize = sizeof(struct segment_command);
		sg->fileoff = 0;
		sg->filesize = 0;
		sg->nsects = 0;
		sg->flags = SG_FVMLIB;
		offset += sizeof(struct segment_command);
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * The LC_LOADFVMLIB command for the library.  This appearing in an
	 * object is what causes the target library to be mapped in when it
	 * is executed.  The link editor collects these into the output file
	 * it builds from the input files.  Since every object in the host
	 * library refers to the library definition symbol defined in here
	 * this object is always linked with anything that uses this library.
	 */
	fvmlib = (struct fvmlib_command *)(buffer + offset);
	fvmlib->cmd = LC_LOADFVMLIB;
	fvmlib->cmdsize = sizeof(struct fvmlib_command) +
			  lib_object.fvmlib_name_size;
	fvmlib->fvmlib.name.offset = sizeof(struct fvmlib_command);
	fvmlib->fvmlib.minor_version = minor_version;
	fvmlib->fvmlib.header_addr = text_addr;
	offset += sizeof(struct fvmlib_command);
	strcpy(buffer + offset, target_name);
	offset += lib_object.fvmlib_name_size;

	st = (struct symtab_command *)(buffer + offset);
	st->cmd = LC_SYMTAB;
	st->cmdsize = sizeof(struct symtab_command);
	st->symoff = sizeof(struct mach_header) + mh->sizeofcmds;
	st->nsyms = 1;
	st->stroff = st->symoff + sizeof(struct nlist);
	st->strsize = lib_object.string_size;
	offset += sizeof(struct symtab_command);

	/*
	 * This is the library definition symbol.
	 * For the first (and only) string is place at index 1 rather than
	 * index 0.  An index of zero is assumed to have the symbol name of "".
	 */
	symbol = (struct nlist *)(buffer + offset);
	symbol->n_un.n_strx = 1;
	symbol->n_type = N_ABS | N_EXT;
	symbol->n_sect = NO_SECT;
	symbol->n_desc = 0;
	symbol->n_value = 0;
	offset += sizeof(struct nlist);

	strcpy(buffer + offset + 1, shlib_symbol_name);
	offset += lib_object.string_size;

	if(host_byte_sex != target_byte_sex){
	    if(swap_object_headers(mh, lib_obj_load_commands) == FALSE)
		fatal("internal error: swap_object_headers() failed");
	    swap_nlist(symbol, 1, target_byte_sex);
	}
}

/*
 * write_ref_obj() writes the library reference object into the buffer passed
 * to it.
 */
static
void
write_ref_obj(
char *buffer)
{
    unsigned long offset, strx, i;
    struct mach_header *mh;
    struct symtab_command *st;
    struct nlist *symbols, *first_symbol;
    char *strings;

	offset = 0;

	/*
	 * This file is made up of the following:
	 *	mach header
	 *	an LC_SYMTAB command
	 *	the symbol table
	 *	the string table
	 */
	mh = (struct mach_header *)(buffer + offset);
	mh->magic = MH_MAGIC;
	mh->cputype = arch_flag.cputype;
	mh->cpusubtype = arch_flag.cpusubtype;
	mh->filetype = MH_OBJECT;
	mh->ncmds = 1;
	mh->sizeofcmds = sizeof(struct symtab_command);
	mh->flags = 0;
	offset += sizeof(struct mach_header);

	st = (struct symtab_command *)(buffer + offset);
	st->cmd = LC_SYMTAB;
	st->cmdsize = sizeof(struct symtab_command);
	st->symoff = sizeof(struct mach_header) + mh->sizeofcmds;
	st->nsyms = 1 + nobject_list;
	st->stroff = st->symoff + (st->nsyms * sizeof(struct nlist));
	st->strsize = ref_object.string_size;
	offset += sizeof(struct symtab_command);

	/*
	 * Setup to put all the symbols and strings in the buffer.  The first
	 * string is place at index 1 rather than index 0.  An index of zero is
	 * assumed to have the symbol name of "" (or NULL).
	 */
	strings = (char *)(buffer + st->stroff);
	symbols = (struct nlist *)(buffer + st->symoff);
	first_symbol = symbols;
	strx = 1;

	/* The first symbol is the library reference symbol which is defined. */
	symbols->n_un.n_strx = strx;
	strcpy(strings + strx, shlib_reference_name);
	strx += strlen(shlib_reference_name) + 1;
	symbols->n_type = N_ABS | N_EXT;
	symbols->n_sect = NO_SECT;
	symbols->n_desc = 0;
	symbols->n_value = 0;
	symbols++;

	/*
	 * All the other symbols are undefined references to the file definition
	 * symbols of each object.
	 */
	for(i = 0; i < nobject_list; i++){
	    symbols->n_un.n_strx = strx;
	    strcpy(strings + strx, object_list[i]->filedef_name);
	    strx += strlen(object_list[i]->filedef_name) + 1;
	    symbols->n_type = N_UNDF | N_EXT;
	    symbols->n_sect = NO_SECT;
	    symbols->n_desc = 0;
	    symbols->n_value = 0;
	    symbols++;
	}

	if(host_byte_sex != target_byte_sex){
	    swap_mach_header(mh, target_byte_sex);
	    swap_symtab_command(st, target_byte_sex);
	    swap_nlist(first_symbol, 1 + nobject_list, target_byte_sex);
	}
}

/*
 * write_object() writes the host object specified by `op' into the buffer
 * specified.
 */
static
void
write_object(
struct object *op,
char *buffer)
{
    long offset;
    struct mach_header *mh;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
    struct init *ip;
    long *contents;
    struct nlist *symbols;
    struct relocation_info *reloc, *relocs;

	offset = 0;

	/*
	 * Each host object file contains:
	 *	mach header
	 * 	one LC_SEGMENT
	 * 	    a section header for the initialization section
	 *	an LC_SYMTAB command
	 * 	the contents of the initialization section
	 * 	the relocation entries for the initialization section
	 *	the symbol table
	 *	the string table
	 */
	mh = (struct mach_header *)(buffer + offset);
	mh->magic = MH_MAGIC;
	mh->cputype = arch_flag.cputype;
	mh->cpusubtype = arch_flag.cpusubtype;
	mh->filetype = MH_OBJECT;
	mh->ncmds = 2;
	mh->sizeofcmds = sizeof(struct segment_command) +
			 sizeof(struct section) +
			 sizeof(struct symtab_command);
	mh->flags = 0;
	offset += sizeof(struct mach_header);

	sg = (struct segment_command *)(buffer + offset);
	sg->cmd = LC_SEGMENT;
	sg->cmdsize = sizeof(struct segment_command) + sizeof(struct section);
	/*
	 * This could be done but since the buffer is vm_allocate()'ed it has
	 * zeros in it already
	 * memset(sg->segname, '\0', sizeof(sg->segname));
	 */
	sg->vmaddr = 0;
	sg->vmsize = op->ninit * 2 * sizeof(long);
	sg->fileoff = sizeof(struct mach_header) + mh->sizeofcmds;
	sg->filesize = op->ninit * 2 * sizeof(long);
	sg->maxprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	sg->initprot = VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE;
	sg->nsects = 1;
	sg->flags = 0;
	offset += sizeof(struct segment_command);

	s = (struct section *)(buffer + offset);
	strcpy(s->sectname, SECT_FVMLIB_INIT0);
	strcpy(s->segname, SEG_TEXT);
	s->addr = 0;
	s->size = op->ninit * 2 * sizeof(long);
	s->offset = sizeof(struct mach_header) + mh->sizeofcmds;
	s->align = 2;
	s->reloff = sizeof(struct mach_header) + mh->sizeofcmds +
		    op->ninit * 2 * sizeof(long);
	s->nreloc = op->ninit * 2;
	s->flags = 0;
	s->reserved1 = 0;
	s->reserved2 = 0;
	offset += sizeof(struct section);

	st = (struct symtab_command *)(buffer + offset);
	st->cmd = LC_SYMTAB;
	st->cmdsize = sizeof(struct symtab_command);
	st->symoff = sizeof(struct mach_header) +
		     mh->sizeofcmds +
		     op->ninit * 2 * sizeof(long) +
		     op->ninit * 2 * sizeof(struct relocation_info);
	st->nsyms = op->nsymbols;
	st->stroff = sizeof(struct mach_header) +
		     mh->sizeofcmds +
		     op->ninit * 2 * sizeof(long) +
		     op->ninit * 2 * sizeof(struct relocation_info) +
		     op->nsymbols * sizeof(struct nlist);
	st->strsize = op->string_size;
	offset += sizeof(struct symtab_command);

	/*
	 * Put the contents of the initialization section in the buffer.
	 */
	if(op->inits != (struct init *)0){
	    ip = op->inits;
	    while(ip != (struct init *)0){
		contents = (long *)(buffer + offset);
		if(host_byte_sex != target_byte_sex)
		    *contents = SWAP_LONG(ip->svalue);
		else
		    *contents = ip->svalue;
		offset += sizeof(long);

		contents = (long *)(buffer + offset);
		if(host_byte_sex != target_byte_sex)
		    *contents = SWAP_LONG(ip->pvalue);
		else
		    *contents = ip->pvalue;
		offset += sizeof(long);

		ip = ip->next;
	    }
	}

	/*
	 * Put the relocation entries for the initialization section in the
	 * buffer.
	 */
	relocs = (struct relocation_info *)(buffer + offset);
	if(op->inits != (struct init *)0){
	    ip = op->inits;
	    while(ip != (struct init *)0){
		reloc = (struct relocation_info *)(buffer + offset);
		*reloc = ip->sreloc;
		offset += sizeof(struct relocation_info);

		reloc = (struct relocation_info *)(buffer + offset);
		*reloc = ip->preloc;
		offset += sizeof(struct relocation_info);

		ip = ip->next;
	    }
	}

	/*
	 * Copy the symbols into the buffer.
	 */
	symbols = (struct nlist *)(buffer + offset);
	memcpy(buffer + offset, op->symbols,
	       op->nsymbols * sizeof(struct nlist));
	offset += op->nsymbols * sizeof(struct nlist);

	/*
	 * Copy the strings into the buffer.
	 */
	memcpy(buffer + offset, op->strings, op->string_size);
	offset += op->string_size;

	if(host_byte_sex != target_byte_sex){
	    swap_mach_header(mh, target_byte_sex);
	    swap_segment_command(sg, target_byte_sex);
	    swap_section(s, 1, target_byte_sex);
	    swap_symtab_command(st, target_byte_sex);
	    swap_relocation_info(relocs, op->ninit * 2, target_byte_sex);
	    swap_nlist(symbols, op->nsymbols, target_byte_sex);
	}
}
