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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <mach/mach.h>
#include <libc.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "libgcc.h"
#include "hack_libgcc.h"
#include "stuff/bool.h"
#include "stuff/allocate.h"
#include "stuff/breakout.h"
#include "stuff/hash_string.h"
#include "stuff/errors.h"
#include "stuff/round.h"

#undef DEBUG

#define NEW_LIBCGG_PREFIX "__libgcc_private_extern"
#define OLD_LIBCGG_PREFIX "__old_libgcc"

__private_extern__
char *progname = NULL;	/* name of the program for error messages (argv[0]) */

/* file name of the specification input file */
char *spec_filename = NULL;

static struct arch_flag arch_flag = { 0 }; /* the -arch flag */

/*
 * Where the information for each object file is stored.  See hack_libgcc.h
 * for the structure definition.
 */
struct object_info objects = { 0 };

/*
 * This will be set to true if a symbol name is changed.  If it is not set then
 * the input file does not have to be written out.
 */
static enum bool input_changed = FALSE;

/*
 * If -p is specified then this is set to TRUE and the new libgcc defined
 * symbols will have their private extern bit (N_PEXT) set.
 */
static enum bool set_private_extern = FALSE;

/*
 * Data structures to hold the symbol names (both the prefixed and non-prefixed
 * symbol names) from the libgcc defined symbols.
 */
struct symbol {
    char *name;			/* name of the symbol */
    char *prefixed_name;	/* prefixed name of the symbol */
    struct symbol *next;	/* next symbol in the hash chain */
};
/* The symbol hash table is hashed on the name field */
#define SYMBOL_HASH_SIZE	250
static struct symbol *new_symbol_hash[SYMBOL_HASH_SIZE];
static struct symbol *old_symbol_hash[SYMBOL_HASH_SIZE];

/*
 * The string table maintained by start_string_table(), add_to_string_table()
 * and end_string_table();
 */
#define INITIAL_STRING_TABLE_SIZE	40960
static struct {
    char *strings;
    unsigned long size;
    unsigned long index;
} string_table;

static void usage(
    void);

static void process(
    char *filename,
    enum bool first_pass,
    enum libgcc libgcc);

static void translate_input(
    struct arch *archs,
    unsigned long narchs,
    enum libgcc libgcc);

static void translate_object(
    struct arch *arch,
    struct member *member,
    struct object *object,
    enum libgcc libgcc);

static char * lookup_new_symbol(
    char *name,
    enum bool add);

static char * lookup_old_symbol(
    char *name,
    enum bool add);

static void start_string_table(
    void);

static long add_to_string_table(
    char *p);

static void end_string_table(
    void);

/*
 * The hack_libgcc(l) program takes the following options:
 *
 *	% hack_libgcc [-p] -arch <arch> -filelist filename[,dirname] ...
 *
 *	% hack_libgcc -arch <arch> -s spec_file
 *
 * It is used to build a 4.2mach and later System framework (and libsys) that is
 * built with a newer gcc than was used in 4.1mach and previous releases.  In
 * 4.2mach and into the future, libgcc that matches the compiler will be linked
 * into each image (executable, shared library, and bundle).  This libgcc will
 * be a static archive (objects compiled -dynamic) and all defined symbols being
 * __private_extern__ .  This allows only the code in the image to use the
 * parts of libgcc copied into it.
 *
 * There is a wrinkle.  System framework when compiled with the the new gcc is
 * linked with the new libgcc.  But it must also export the old libgcc functions
 * to maintain compatibility with 4.1mach and older programs.  This is where
 * this program comes in.  It takes the new libgcc and records all the names
 * of the defined symbols.  Then it changes the symbol names in the new libgcc
 * and all of the other objects with the prefix "_System."
 *
 * For System framework, the first -filelist argument is assumed to be for the
 * new libgcc objects.  The rest of the -filelist arguments are assumed to be
 * the other objects that make up System framework.   The old libgcc objects
 * should not ever be specified as an argument.
 *
 * For libsys, the spec_file has the new and old libgcc functions marked.
 */
int
main(
int argc,
char *argv[],
char *envp[])
{
    unsigned long i, j, nfiles;
    char *p, *filelist, *dirname, *addr;
    int fd;
    struct stat stat_buf;
    enum libgcc libgcc;
    kern_return_t r;

	/* set the name of the program for error messages */
	progname = argv[0];

	/* the first filelist argument is assumed to be for libgcc */
	libgcc = NEW_LIBGCC;

	/* parse the command line arguments */
	for(i = 1; i < argc; i++){
	    if(strcmp(argv[i], "-arch") == 0){
		if(arch_flag.name != NULL)
		    fatal("only one -arch can be specified");
		if(i + 1 >= argc){
		    error("missing argument(s) to %s option", argv[i]);
		    usage();
		}
		if(strcmp("all", argv[i+1]) == 0)
		    fatal("-arch all can't be specified");
		if(get_arch_from_flag(argv[i+1], &arch_flag) == 0){
		    error("unknown architecture specification flag: "
			  "%s %s", argv[i], argv[i+1]);
		    arch_usage();
		    usage();
		}
		i++;
	    }
	    else if(strcmp(argv[i], "-filelist") == 0){
		if(spec_filename != NULL){
		    error("can't specify both -s <specfile> and -filelist "
			  "arguments");
		    usage();
		}
		if(i + 1 == argc){
		    error("missing argument to: %s option", argv[i]);
		    usage();
		}
		filelist = argv[i + 1];
		dirname = strrchr(filelist, ',');
		if(dirname != NULL){
		    *dirname = '\0';
		    dirname++;
		}
		else
		    dirname = "";
		if((fd = open(filelist, O_RDONLY, 0)) == -1)
		    system_fatal("can't open file list file: %s", filelist);
		if(fstat(fd, &stat_buf) == -1)
		    system_fatal("can't stat file list file: %s", filelist);
		/*
		 * For some reason mapping files with zero size fails
		 * so it has to be handled specially.
		 */
		if(stat_buf.st_size != 0){
		    if((r = map_fd((int)fd, (vm_offset_t)0,
			(vm_offset_t *)&(addr), (boolean_t)TRUE,
			(vm_size_t)stat_buf.st_size)) != KERN_SUCCESS)
			mach_fatal(r, "can't map file list file: %s",
			    filelist);
		}
		else{
		    fatal("file list file: %s is empty", filelist);
		}
		if(*dirname != '\0')
		    dirname[-1] = ',';
		close(fd);

		/*
		 * Count the number of files listed in this filelist.
		 * Note since the file can't be zero length and does not need
		 * to end in a '\n' there is always at least one file.
		 */
		nfiles = 0;
		for(j = 0; j < stat_buf.st_size; j++){
		    if(addr[j] == '\n')
			nfiles++;
		}
		if(addr[stat_buf.st_size - 1] != '\n')
		    nfiles++;

		/*
		 * Allocate some space to put the file names which includes the
		 * dirname prepended to them.  And space to store info about
		 * each object.
		 */
		p = allocate((strlen(dirname) + 1) * nfiles +
			     stat_buf.st_size + 1);
		objects.names = reallocate(objects.names,
				sizeof(char *) * (objects.nobjects + nfiles));
		objects.filelists = reallocate(objects.filelists,
				sizeof(char *) * (objects.nobjects + nfiles));
		objects.libgcc = reallocate(objects.libgcc,
			   sizeof(enum libgcc) * (objects.nobjects + nfiles));

		objects.names[objects.nobjects] = p;
		objects.filelists[objects.nobjects] = filelist;
		objects.libgcc[objects.nobjects] = libgcc;
		objects.nobjects++;

		if(*dirname != '\0'){
		    strcpy(p, dirname);
		    p += strlen(dirname);
		    *p++ = '/';
		}
		for(j = 0; j < stat_buf.st_size; j++){
		    if(addr[j] != '\n')
			*p++ = addr[j];
		    else{
			*p++ = '\0';
			if(j != stat_buf.st_size - 1){
			    objects.names[objects.nobjects] = p;
			    objects.filelists[objects.nobjects] = filelist;
			    objects.libgcc[objects.nobjects] = libgcc;
			    objects.nobjects++;
			    if(*dirname != '\0'){
				strcpy(p, dirname);
				p += strlen(dirname);
				*p++ = '/';
			    }
			}
		    }
		}
		if(addr[stat_buf.st_size - 1] != '\n')
		    *p = '\0';
		/* skip the filelist argument */
		i++;
		/* all other filelists will be assumed not to be libgcc */
		libgcc = NOT_LIBGCC;
	    }
	    else if(strcmp(argv[i], "-s") == 0){
		if(objects.nobjects != 0){
		    error("can't specify both -s <specfile> and -filelist "
			  "arguments");
		    usage();
		}
		if(i + 1 == argc){
		    error("missing argument to: %s option", argv[i]);
		    usage();
		}
		if(spec_filename != NULL){
		    error("only one -s <specfile> argument can be specified");
		}
		spec_filename = argv[i + 1];
		i++;
	    }
	    else if(strcmp(argv[i], "-p") == 0){
		set_private_extern = TRUE;
	    }
	    else{
		error("unknown argument: %s", argv[i]);
		usage();
	    }
        }

	/*
	 * If we are using the -s <specfile> option parse the spec file and
	 * create objects for each one.
	 */
	if(spec_filename != NULL){
	    load_objects_from_specfile();
	}

	/*
	 * Make sure the needed arguments are specified.
	 */
	if(arch_flag.name == NULL){
	    error("-arch must be specified");
	    usage();
	}
	if(objects.nobjects == 0){
	    error("no -filelist arguments or -s <specfile> with #objects "
		  "specified");
	    usage();
	}

	/*
	 * Process the objects.  Two passes are required over the libgcc objects
	 * to make sure all the symbols are picked up and all the referenced
	 * symbols are changed.  In the first pass no objects are written.
	 * Once all the libgcc objects have been processed then all the other
	 * objects are processed.
	 */
	for(i = 0; i < objects.nobjects; i++){
	    if(objects.libgcc[i] == NEW_LIBGCC ||
	       objects.libgcc[i] == OLD_LIBGCC)
		process(objects.names[i], TRUE, objects.libgcc[i]);
	}
	for(i = 0; i < objects.nobjects; i++){
	    if(objects.libgcc[i] == NEW_LIBGCC ||
	       objects.libgcc[i] == OLD_LIBGCC)
		process(objects.names[i], FALSE, objects.libgcc[i]);
	}
	for(i = 0; i < objects.nobjects; i++){
	    if(objects.libgcc[i] == NOT_LIBGCC)
		process(objects.names[i], FALSE, objects.libgcc[i]);
	}

	if(errors)
	    return(EXIT_FAILURE);
	else
	    return(EXIT_SUCCESS);
}

/*
 * usage() prints the current usage message and exits indicating failure.
 */
static
void
usage(
void)
{
	fprintf(stderr, "Usage: %s [-p] -arch <arch_flag> "
			"-filelist filename[,dirname] ... OR\n", progname);
	fprintf(stderr, "Usage: %s -arch <arch_flag> -s <specfile>\n",progname);
	exit(EXIT_FAILURE);
}

/*
 * process() takes an object file name and drives the process to make sure the
 * libgcc symbols are prefixed.  If the first_pass parameter is true then no
 * objects are written (this is used to scan all the libgcc objects and pick
 * up the symbol names).  The parameter tells if this object comes from the
 * new or old libgcc or neither.
 */
static
void
process(
char *filename,
enum bool first_pass,
enum libgcc libgcc)
{
    struct arch *archs;
    unsigned long narchs;
    struct stat stat_buf;

#ifdef DEBUG
	printf("process(%s,",filename);
	switch(libgcc){
	case NOT_LIBGCC:
	    printf("NOT_LIBGCC)\n");
	    break;
	case NEW_LIBGCC:
	    printf("NEW_LIBGCC)\n");
	    break;
	case OLD_LIBGCC:
	    printf("OLD_LIBGCC)\n");
	    break;
	}
#endif

	archs = NULL;
	narchs = 0;
	input_changed = FALSE;

	/* breakout the input file for processing */
	breakout(filename, &archs, &narchs, FALSE);
	if(errors)
	    exit(EXIT_FAILURE);

	/* checkout the input file for symbol table replacement processing */
	checkout(archs, narchs);

	/* translate the symbols in the input file */
	translate_input(archs, narchs, libgcc);

	if(errors)
	    exit(EXIT_FAILURE);

	if(first_pass == FALSE && input_changed == TRUE){
#ifdef DEBUG
#endif
	    printf("process(%s) was changed\n", filename);
	    /* create the output file */
	    if(stat(filename, &stat_buf) == -1)
		system_error("can't stat file: %s", filename);
	    writeout(archs, narchs, filename, stat_buf.st_mode & 0777, TRUE,
		     FALSE, FALSE, NULL);
	}

	if(errors)
	    exit(EXIT_FAILURE);
}

/*
 * translate_input() is passed a set of archs.  It digs through the archs and
 * if one matches the arch_flag specified it will cause that arch to translated.
 * If the arch comes from new libgcc then libgcc parameter is NEW_LIBGCC, from
 * the old libgcc OLD_LIBGCC and neither then NOT_LIBGCC.
 */
static
void
translate_input(
struct arch *archs,
unsigned long narchs,
enum libgcc libgcc)
{
    unsigned long i, j, offset, size;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;


	/*
	 * Using the specified arch_flag process specified objects for this
	 * architecure.
	 */
	for(i = 0; i < narchs; i++){
#ifdef DEBUG
	    printf("translate_input(arch = %lu, %s)\n", i, archs[i].file_name);
#endif
	    /*
	     * Determine the architecture (cputype and cpusubtype) of arch[i]
	     */
	    cputype = 0;
	    cpusubtype = 0;
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O){
			cputype = archs[i].members[j].object->mh->cputype;
			cpusubtype = archs[i].members[j].object->mh->cpusubtype;
			break;
		    }
		}
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		cputype = archs[i].object->mh->cputype;
		cpusubtype = archs[i].object->mh->cpusubtype;
	    }
	    else if(archs[i].fat_arch != NULL){
		cputype = archs[i].fat_arch->cputype;
		cpusubtype = archs[i].fat_arch->cpusubtype;
	    }

	    if(arch_flag.cputype != cputype ||
	       arch_flag.cpusubtype != cpusubtype)
		continue;

	    /*
	     * Now this arch[i] has been selected to be processed so process it
	     * according to it's type.
	     */
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O){
			translate_object(archs + i, archs[i].members + j,
					 archs[i].members[j].object, libgcc);
		    }
		}
		/*
		 * Reset the library offsets and size.
		 */
		offset = 0;
		for(j = 0; j < archs[i].nmembers; j++){
		    archs[i].members[j].offset = offset;
		    if(archs[i].members[j].object != NULL){
			size = archs[i].members[j].object->object_size
			   - archs[i].members[j].object->input_sym_info_size
			   + archs[i].members[j].object->output_sym_info_size;
			sprintf(archs[i].members[j].ar_hdr->ar_size, "%-*ld",
			       (int)sizeof(archs[i].members[j].ar_hdr->ar_size),
			       (long)(size));
			/*
			 * This has to be done by hand because sprintf puts a
			 * null at the end of the buffer.
			 */
			memcpy(archs[i].members[j].ar_hdr->ar_fmag, ARFMAG,
			      (int)sizeof(archs[i].members[j].ar_hdr->ar_fmag));
		    }
		    else{
			size = archs[i].members[j].unknown_size;
		    }
		    offset += sizeof(struct ar_hdr) + size;
		}
		archs[i].library_size = offset;
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		translate_object(archs + i, NULL, archs[i].object, libgcc);
	    }
	    else {
		fatal_arch(archs + i, NULL, "can't process non-object and "
			   "non-archive file: ");
	    }
	}
}

/*
 * translate_object() is passed an object to be have it's libgcc symbols
 * translated to ones with suffixes.  The symbol may have already had their
 * libgcc symbols suffixed.  If the object comes from new libgcc then libgcc
 * parameter is NEW_LIBGCC, from the old libgcc OLD_LIBGCC and neither then
 * NOT_LIBGCC.
 */
static
void
translate_object(
struct arch *arch,
struct member *member,
struct object *object,
enum libgcc libgcc)
{
    unsigned long i;
    enum byte_sex host_byte_sex;
    struct nlist *symbols, *nlistp;
    unsigned long nsyms, offset;
    char *strings, *p;
    unsigned long strings_size;

#ifdef DEBUG
	printf("translate_object(%s)\n", arch->file_name);
#endif

	if(object->mh->filetype != MH_OBJECT){
	    fatal_arch(arch, member,"is not an MH_OBJECT file (invalid input)");
	    return;
	}

	host_byte_sex = get_host_byte_sex();

	if(object->st == NULL || object->st->nsyms == 0)
	    return;

	symbols = (struct nlist *)(object->object_addr + object->st->symoff);
	nsyms = object->st->nsyms;
	if(object->object_byte_sex != host_byte_sex)
	    swap_nlist(symbols, nsyms, host_byte_sex);
	strings = object->object_addr + object->st->stroff;
	strings_size = object->st->strsize;

	object->output_symbols = symbols;
	object->output_nsymbols = nsyms;
	object->input_sym_info_size = nsyms * sizeof(struct nlist) +
				      object->st->strsize;
	if(object->dyst != NULL){
	    object->input_sym_info_size +=
		    object->dyst->nindirectsyms * sizeof(unsigned long);
	}

	start_string_table();
	nlistp = symbols;
	for(i = 0; i < nsyms; i++){
	    if(nlistp->n_type & N_EXT){
		if(nlistp->n_un.n_strx){
		    if(nlistp->n_un.n_strx > 0 &&
		       nlistp->n_un.n_strx < strings_size){
			if(libgcc == OLD_LIBGCC){
			    p = lookup_old_symbol(strings + nlistp->n_un.n_strx,
				    ((nlistp->n_type & N_TYPE) != N_UNDF));
			    if(p != NULL){
				nlistp->n_un.n_strx = add_to_string_table(p);
			    }
			    else{
				nlistp->n_un.n_strx = add_to_string_table(
					strings + nlistp->n_un.n_strx);
			    }
			}
			else{
			    p = lookup_new_symbol(strings + nlistp->n_un.n_strx,
				libgcc == NEW_LIBGCC &&
				    ((nlistp->n_type & N_TYPE) != N_UNDF));
			    if(p != NULL){
				nlistp->n_un.n_strx = add_to_string_table(p);
			    }
			    else{
				nlistp->n_un.n_strx = add_to_string_table(
					strings + nlistp->n_un.n_strx);
			    }
			    if(libgcc == NEW_LIBGCC &&
			       set_private_extern == TRUE &&
			       (nlistp->n_type & N_TYPE) != N_UNDF &&
			       (nlistp->n_type & N_PEXT) != N_PEXT){
				nlistp->n_type |= N_PEXT;
				input_changed = TRUE;
#ifdef DEBUG
				printf("translate_object(%s) setting N_PEXT on "
				       "%s\n", arch->file_name, strings +
				       nlistp->n_un.n_strx);
#endif
			    }
			}
		    }
		    else
			fatal_arch(arch, member, "bad string table "
				    "index in symbol %lu in: ", i);
		}
	    }
	    else{
		if(nlistp->n_un.n_strx){
		    if(nlistp->n_un.n_strx > 0 && nlistp->n_un.n_strx <
								strings_size)
			nlistp->n_un.n_strx = add_to_string_table(
				strings + nlistp->n_un.n_strx);
		    else
			fatal_arch(arch, member, "bad string table "
				    "index in symbol %lu in: ", i);
		}
	    }
	    nlistp++;
	}

	end_string_table();
	object->output_strings = allocate(string_table.index);
	memcpy(object->output_strings, string_table.strings,string_table.index);
	object->output_strings_size = string_table.index;

	object->output_sym_info_size =
		nsyms * sizeof(struct nlist) +
		string_table.index;
	if(object->dyst != NULL){
	    object->output_sym_info_size +=
		    object->dyst->nindirectsyms * sizeof(unsigned long);
	}

	if(object->seg_linkedit != NULL){
	    object->seg_linkedit->filesize += object->output_sym_info_size -
					      object->input_sym_info_size;
	    object->seg_linkedit->vmsize = object->seg_linkedit->filesize;
	}

	if(object->dyst != NULL){
	    object->st->nsyms = nsyms;
	    object->st->strsize = string_table.index;

	    offset = ULONG_MAX;
	    if(object->st->nsyms != 0 &&
	       object->st->symoff < offset)
		offset = object->st->symoff;
	    if(object->dyst->nindirectsyms != 0 &&
	       object->dyst->indirectsymoff < offset)
		offset = object->dyst->indirectsymoff;
	    if(object->st->strsize != 0 &&
	       object->st->stroff < offset)
		offset = object->st->stroff;

	    if(object->st->nsyms != 0){
		object->st->symoff = offset;
		offset += object->st->nsyms * sizeof(struct nlist);
	    }
	    else
		object->st->symoff = 0;

	    if(object->dyst->nindirectsyms != 0){
		object->output_indirect_symtab = (unsigned long *)
		    (object->object_addr + object->dyst->indirectsymoff);
		object->dyst->indirectsymoff = offset;
		offset += object->dyst->nindirectsyms *
			  sizeof(unsigned long);
	    }
	    else
		object->dyst->indirectsymoff = 0;;

	    if(object->st->strsize != 0){
		object->st->stroff = offset;
		offset += object->st->strsize;
	    }
	    else
		object->st->stroff = 0;
	}
	else{
	    object->st->nsyms = nsyms;
	    object->st->stroff = object->st->symoff +
				 nsyms * sizeof(struct nlist);
	    object->st->strsize = string_table.index;
	}
}

/*
 * lookup_new_symbol() takes a symbol name which could possibly the name of a
 * new libgcc symbol.  If the symbol is a new libgcc function it returns the
 * prefixed name for the symbol.  The symbol may already have the prefix when
 * passed in.  If this symbol is from new libgcc and not undefined then the
 * parameter add is TRUE and it is added to the hash table if not found.
 */
static
char *
lookup_new_symbol(
char *name,
enum bool add)
{
    char *p, *q;
    long hash_key;
    struct symbol *sp;

#ifdef DEBUG
	printf("lookup_new_symbol(%s, %s)\n", name,
	       add == TRUE ? "TRUE" : "FALSE");
#endif
	if(strncmp(name, NEW_LIBCGG_PREFIX, strlen(NEW_LIBCGG_PREFIX)) == 0)
	    p = name + strlen(NEW_LIBCGG_PREFIX);
	else
	    p = name;

	hash_key = hash_string(p) % SYMBOL_HASH_SIZE;
	sp = new_symbol_hash[hash_key];
	while(sp != NULL){
	    if(strcmp(p, sp->name) == 0){
		if(p == name)
		    input_changed = TRUE;
		return(sp->prefixed_name);
	    }
	    sp = sp->next;
	}
	if(add == TRUE){
#ifdef DEBUG
	    printf("lookup_new_symbol adding %s\n", name);
#endif
	    sp = (struct symbol *)allocate(sizeof(struct symbol));
	    q = allocate(strlen(NEW_LIBCGG_PREFIX) + strlen(p) + 1);
	    strcpy(q, NEW_LIBCGG_PREFIX);
	    strcat(q, p);
	    sp->name = q + strlen(NEW_LIBCGG_PREFIX);
	    sp->prefixed_name = q;
	    sp->next = new_symbol_hash[hash_key];
	    new_symbol_hash[hash_key] = sp;
	    if(p == name)
		input_changed = TRUE;
	    return(sp->prefixed_name);
	}
	return(NULL);
}

/*
 * lookup_old_symbol() takes a symbol name which could possibly the name of an
 * old libgcc symbol.  If the symbol is an old libgcc function it returns the
 * prefixed name for the symbol.  The symbol may already have the prefix when
 * passed in.  If this symbol is from old libgcc and not undefined then the
 * parameter add is TRUE and it is added to the hash table if not found.
 */
static
char *
lookup_old_symbol(
char *name,
enum bool add)
{
    char *p, *q;
    long hash_key;
    struct symbol *sp;

#ifdef DEBUG
	printf("lookup_old_symbol(%s, %s)\n", name,
	       add == TRUE ? "TRUE" : "FALSE");
#endif
	if(strncmp(name, OLD_LIBCGG_PREFIX, strlen(OLD_LIBCGG_PREFIX)) == 0)
	    p = name + strlen(OLD_LIBCGG_PREFIX);
	else
	    p = name;

	hash_key = hash_string(p) % SYMBOL_HASH_SIZE;
	sp = old_symbol_hash[hash_key];
	while(sp != NULL){
	    if(strcmp(p, sp->name) == 0){
		if(p == name)
		    input_changed = TRUE;
		return(sp->prefixed_name);
	    }
	    sp = sp->next;
	}
	if(add == TRUE){
#ifdef DEBUG
	    printf("lookup_old_symbol adding %s\n", name);
#endif
	    sp = (struct symbol *)allocate(sizeof(struct symbol));
	    q = allocate(strlen(OLD_LIBCGG_PREFIX) + strlen(p) + 1);
	    strcpy(q, OLD_LIBCGG_PREFIX);
	    strcat(q, p);
	    sp->name = q + strlen(OLD_LIBCGG_PREFIX);
	    sp->prefixed_name = q;
	    sp->next = old_symbol_hash[hash_key];
	    old_symbol_hash[hash_key] = sp;
	    if(p == name)
		input_changed = TRUE;
	    return(sp->prefixed_name);
	}
	return(NULL);
}

/*
 * This routine is called before calls to add_to_string_table() are made to
 * setup or reset the string table structure.  The first four bytes string
 * table are zeroed and the first string is placed after that  (this was for
 * the string table length in a 4.3bsd a.out along time ago).  The first four
 * bytes are kept zero even thought only the first byte can't be used as valid
 * string offset (because that is defined to be a NULL string) but to avoid
 * breaking programs that don't know this the first byte is left zero and the
 * first 4 bytes are not stuffed with the size because on a little endian
 * machine that first byte is likely to be non-zero.
 */
static
void
start_string_table()
{
	if(string_table.size == 0){
	    string_table.size = INITIAL_STRING_TABLE_SIZE;
	    string_table.strings = (char *)allocate(string_table.size);
	}
	memset(string_table.strings, '\0', sizeof(long));
	string_table.index = sizeof(long);
}

/*
 * This routine adds the specified string to the string table structure and
 * returns the index of the string in the table.
 */
static
long
add_to_string_table(
char *p)
{
    long len, index;

	len = strlen(p) + 1;
	if(string_table.size < string_table.index + len){
	    string_table.strings = (char *)reallocate(string_table.strings,
						      string_table.size * 2);
	    string_table.size *= 2;
	}
	index = string_table.index;
	strcpy(string_table.strings + string_table.index, p);
	string_table.index += len;
	return(index);
}

/*
 * This routine is called after all calls to add_to_string_table() are made
 * to round off the size of the string table.  It zeros the rounded bytes.
 */
static
void
end_string_table()
{
    unsigned long length;

	length = round(string_table.index, sizeof(unsigned long));
	memset(string_table.strings + string_table.index, '\0',
	       length - string_table.index);
	string_table.index = length;
}
