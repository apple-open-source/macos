/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#undef DEBUG

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libc.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/reloc.h>
#include "stuff/ofile.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/hash_string.h"
#include "stuff/hppa.h"

#include "branch.h"
#include "libgcc.h"
#include "mkshlib.h"

static void usage(
    void);

static void make_oldtarget_hashtable(
    char *old_target);

static void check_newtarget(
    char *new_target);

static void open_target(
    char *target,
    struct ofile *ofile,
    struct nlist **symbols,
    long *nsyms,
    char **strings,
    long *strsize,
    char **text,
    long *textaddr,
    long *textsize,
    long *text_nsect,
    long *data_nsect);

static void make_sorted_text_symbols(
    struct nlist *symbols,
    long nsyms,
    struct nlist **text_symbols,
    long *ntext_symbols);

static void make_global_symbols(
    struct nlist *symbols,
    long nsyms,
    struct nlist
    **global_symbols,
    long *nglobal_symbols);

static long get_target_addr(
    long value,
    char *name,
    char *text,
    long addr,
    long size,
    char *target);

static int cmp_qsort(
    struct nlist *sym1,
    struct nlist *sym2);

static int cmp_bsearch(
    long *value,
    struct nlist *sym);

static struct ext *lookup(
    char *name);

/* Structure of the members of the hash table for the old target */
struct ext {
    char *name;		  /* symbol name */
    long type;		  /* symbol type (n_type & N_TYPE) */
    long sect;		  /* symbol section (n_sect) */
    long value;		  /* symbol value */
    char *branch_target;  /* if non-zero the branch target symbol name */
    struct ext *next;	  /* next ext on the hash chain */
};
#define EXT_HASH_SIZE	251
struct ext *ext_hash[EXT_HASH_SIZE];

char *progname;		/* name of the program for error messages (argv[0]) */

/* file name of the specification input file */
char *spec_filename = NULL;

char *old_target = NULL;
char *new_target = NULL;

/*
 * The architecture as specified by -arch and the cputype and cpusubtype of the
 * shared libraries to be checked.
 */
struct arch_flag arch_flag = { 0 };

/*
 * The ofile structures for the old an new target shared libraries to check.
 */
static struct ofile old_target_ofile = { 0 };
static struct ofile new_target_ofile = { 0 };

/* the byte sex of the machine this program is running on */
static enum byte_sex host_byte_sex = UNKNOWN_BYTE_SEX;

int
main(
int argc,
char *argv[],
char *envp[])
{
    long i;

	progname = argv[0];
	host_byte_sex = get_host_byte_sex();

	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		if(strcmp("-s", argv[i]) == 0){
		    if(i + 1 > argc)
			usage();
		    spec_filename = argv[++i];
		}
		else if(strcmp(argv[i], "-arch") == 0){
		    if(++i >= argc)
			fatal("-arch: argument missing");
		    if(arch_flag.name != NULL)
			fatal("-arch: multiply specified");
		    if(get_arch_from_flag(argv[i], &arch_flag) == 0){
			error("unknown architecture specification flag: "
			      "-arch %s", argv[i]);
			arch_usage();
			usage();
		    }
		}
		else
		   usage();
	    }
	    else{
		if(old_target == NULL)
		    old_target = argv[i];
		else if(new_target == NULL)
		    new_target = argv[i];
		else
		    usage();
	    }
	}

	if(old_target == NULL || new_target == NULL)
	    usage();

	if(spec_filename != NULL){
	    minor_version = 1;
	    parse_spec();
	}
	else
	    fprintf(stderr, "%s: no -s spec_file specified (nobranch_text "
		    "symbols not checked)\n", progname);

	make_oldtarget_hashtable(old_target);
	check_newtarget(new_target);
	if(errors){
	    return(EXIT_FAILURE);
	}
	else{
	    return(EXIT_SUCCESS);
	}
}

/*
 * Print the usage line and exit indicating failure.
 */
static
void
usage(void)
{
	fprintf(stderr, "Usage: %s [-s spec_file] [-arch arch_type] "
		"<old target> <new target>\n", progname);
	exit(EXIT_FAILURE);
}

/*
 * Build the hash table of the external symbols of old target shared library.
 */
static
void
make_oldtarget_hashtable(
char *old_target)
{
    struct nlist *symbols, *text_symbols, *np;
    long nsyms, strsize, textaddr, textsize, i, ntext_symbols,
	target_addr, hash_key, text_nsect, data_nsect;
    char *text, *strings;
    struct ext *extp;

	fprintf(stderr, "%s: building hash table of external symbols for old "
		"target: %s\n", progname, old_target);

	open_target(old_target, &old_target_ofile, &symbols, &nsyms, &strings,
		    &strsize, &text, &textaddr, &textsize, &text_nsect,
		    &data_nsect);

	if(old_target_ofile.mh->cputype != CPU_TYPE_MC680x0 &&
	   old_target_ofile.mh->cputype != CPU_TYPE_MC88000 &&
	   old_target_ofile.mh->cputype != CPU_TYPE_POWERPC &&
	   old_target_ofile.mh->cputype != CPU_TYPE_HPPA &&
	   old_target_ofile.mh->cputype != CPU_TYPE_SPARC &&
	   old_target_ofile.mh->cputype != CPU_TYPE_I386){
	    if(arch_flag.name == NULL)
		fatal("unknown or unsupported cputype (%d) in shared library: "
		      "%s", old_target_ofile.mh->cputype, old_target);
	    else
		fatal("unsupported architecture for -arch %s", arch_flag.name);
	}
	if(arch_flag.cputype == 0){
	    arch_flag.cputype = old_target_ofile.mh->cputype;
	    arch_flag.cpusubtype = old_target_ofile.mh->cpusubtype;
	}
	if(arch_flag.name == NULL)
	    set_arch_flag_name(&arch_flag);

	fprintf(stderr, "%s: checking architecture %s\n", progname,
		arch_flag.name);

	make_sorted_text_symbols(symbols, nsyms, &text_symbols, &ntext_symbols);

	/*
	 * Build the hash table of external symbols.
	 */
	for(i = 0; i < nsyms; i++){
	    if((symbols[i].n_type & N_EXT) != N_EXT)
		continue;
	    hash_key = hash_string(symbols[i].n_un.n_name) % EXT_HASH_SIZE;
	    extp = ext_hash[hash_key];
	    while(extp != (struct ext *)0){
		if(strcmp(symbols[i].n_un.n_name, extp->name) == 0)
		    fatal("Symbol %s appears more than once in target shared "
			  "library: %s", extp->name, old_target);
		extp = extp->next;
	    }
	    extp = allocate(sizeof(struct ext));
	    extp->name = symbols[i].n_un.n_name;
	    extp->type = symbols[i].n_type & N_TYPE;
	    extp->sect = symbols[i].n_sect;
	    extp->value = symbols[i].n_value;
	    extp->branch_target = (char *)0;
	    extp->next = ext_hash[hash_key];
	    ext_hash[hash_key] = extp;
	    if(extp->type == N_SECT && extp->sect == text_nsect)
		ntext_symbols++;
	}

	/*
	 * For each branch table slot find the name of the symbol in that slot.
	 */
	for(i = 0; i < EXT_HASH_SIZE; i++){
	    extp = ext_hash[i];
	    while(extp != (struct ext *)0){
		if((extp->type == N_SECT && extp->sect == text_nsect) &&
		   strncmp(extp->name, BRANCH_SLOT_NAME,
		           sizeof(BRANCH_SLOT_NAME) - 1) == 0){
		    target_addr = get_target_addr(extp->value, extp->name,
				    text, textaddr, textsize, old_target);
		    np = bsearch(&target_addr, text_symbols, ntext_symbols,
				 sizeof(struct nlist),
			     (int (*)(const void *, const void *)) cmp_bsearch);
		    if(np == (struct nlist *)0)
			fatal("Can't find a text symbol for the target (0x%x) "
			      "of branch table symbol: %s in: %s",
			      (unsigned int)target_addr,extp->name, old_target);
		    extp->branch_target = np->n_un.n_name;
		}
		extp = extp->next;
	    }
	}

#ifdef DEBUG
	for(i = 0; i < EXT_HASH_SIZE; i++){
	    extp = ext_hash[i];
	    while(extp != (struct ext *)0){
		printf("name = %s value = 0x%x type = 0x%x sect = %d ",
		       extp->name, extp->value, extp->type, extp->sect);
		if(extp->branch_target != (char *)0)
		    printf("branch_target = %s\n", extp->branch_target);
		else
		    printf("\n");
		extp = extp->next;
	    }
	}
#endif DEBUG
}

/*
 * Check the new target shared library against the old target shared library
 * hash table of symbols.
 */
static
void
check_newtarget(
char *new_target)
{
    struct nlist *symbols, *text_symbols, *global_symbols, *np;
    long nsyms, strsize, textaddr, textsize, ntext_symbols, i, j, target_addr,
	first_new_data, printed_first_new_data, nglobal_symbols, old_name_error,
	text_nsect, data_nsect;
    char *strings, *text;
    struct ext *extp;
    long hash_key;
    struct oddball *obp;
    struct branch *bp;
    long slot_number;

	fprintf(stderr, "%s: checking external symbols of new target: "
	        "%s\n", progname, new_target);

	open_target(new_target, &new_target_ofile, &symbols, &nsyms, &strings,
		    &strsize, &text, &textaddr, &textsize, &text_nsect,
		    &data_nsect);

	if(new_target_ofile.mh->cputype != old_target_ofile.mh->cputype)
	    fatal("cputypes and cpusubtypes of the two shared libraries are "
		  "not the same");

	make_sorted_text_symbols(symbols, nsyms, &text_symbols, &ntext_symbols);

	make_global_symbols(symbols, nsyms, &global_symbols, &nglobal_symbols);

	/* sort the global symbols by value so they get checked in order */
	qsort(global_symbols, nglobal_symbols, sizeof(struct nlist),
	      (int (*)(const void *, const void *))cmp_qsort);

	/* Check jump table targets */
	fprintf(stderr, "Checking the jump table targets\n");
	for(i = 0; i < nglobal_symbols; i++){
	    if((global_symbols[i].n_type & N_EXT) != N_EXT)
		continue;
	    if((global_symbols[i].n_type & N_TYPE) == N_SECT && 
	        global_symbols[i].n_sect == text_nsect){
		if(strncmp(global_symbols[i].n_un.n_name, BRANCH_SLOT_NAME,
			   sizeof(BRANCH_SLOT_NAME) - 1) == 0){
		    extp = lookup(global_symbols[i].n_un.n_name);
		    if(extp == (struct ext *)0)
			continue;
		    target_addr = get_target_addr(extp->value, extp->name,
				    text, textaddr, textsize, new_target);
		    np = bsearch(&target_addr, text_symbols, ntext_symbols,
				 sizeof(struct nlist),
			      (int (*)(const void *, const void *))cmp_bsearch);
		    if(np == (struct nlist *)0)
			fatal("Can't find a text symbol for the target (0x%x) "
			      "of branch table symbol: %s in: %s",
			      (unsigned int)target_addr,extp->name, new_target);
		    /*
		     * It is possible that more than one symbol exists with the
		     * same value as the branch table target.  Since the text
		     * symbols are sorted by value find the first one with the
		     * same value.  Then find the one that is the name of
		     * the branch table.
		     */
		    while(np != text_symbols &&
			  np[0].n_value == np[-1].n_value)
			np--;
		    if(np[0].n_value == np[1].n_value){
			slot_number = atol(global_symbols[i].n_un.n_name +
					   sizeof(BRANCH_SLOT_NAME) - 1);
			bp = NULL;
			for(j = 0; j < nbranch_list; j++){
			    if(branch_list[j]->max_slotnum == slot_number){
				bp = branch_list[j];
				break;
			    }
			}
			if(bp != NULL){
			    while(np[0].n_value == np[1].n_value){
				if(strcmp(bp->name, np->n_un.n_name) == 0)
				    break;
				np++;
			    }
			}
		    }

		    if(strcmp(extp->branch_target, np->n_un.n_name) != 0){
			/*
			 * see if this branch name has an old name to check for
			 */
			old_name_error = 0;
			hash_key = hash_string(np->n_un.n_name) %
				   BRANCH_HASH_SIZE;
			bp = branch_hash[hash_key];
			while(bp != NULL){
			    if(strcmp(bp->name, np->n_un.n_name) == 0){
				if(bp->old_name != NULL){
				    if(strcmp(extp->branch_target,
					      bp->old_name) != 0){
					error("Branch table symbol: %s has "
					      "different targets (%s and %s) "
					      "and does not match old_name %s",
					      extp->name, extp->branch_target,
					      np->n_un.n_name, bp->old_name);
				    }
				    old_name_error = 1;
				}
				break;
			    }
			    bp = bp->next;
			}
			if(old_name_error == 0){
			    error("Branch table symbol: %s has different "
				  "targets (%s and %s)", extp->name,
				  extp->branch_target, np->n_un.n_name);
			}
		    }
		}
	    }

	}

	/* Check global data addresses */
	fprintf(stderr, "Checking the global data addresses\n");
	first_new_data = -1;
	printed_first_new_data = 0;
	for(i = 0; i < nglobal_symbols; i++){
	    if((global_symbols[i].n_type & N_EXT) != N_EXT)
		continue;
	    if((global_symbols[i].n_type & N_TYPE) == N_SECT && 
	        global_symbols[i].n_sect == data_nsect){

		/* if it is a private extern don't check it */
		hash_key = hash_string(global_symbols[i].n_un.n_name) %
							      ODDBALL_HASH_SIZE;
		obp = oddball_hash[hash_key];
		while(obp != (struct oddball *)0){
		    if(strcmp(obp->name, global_symbols[i].n_un.n_name) == 0)
			break;
		    obp = obp->next;
		}
		if(obp != (struct oddball *)0 && obp->private == 1)
		   continue;

		extp = lookup(global_symbols[i].n_un.n_name);
		if(extp == (struct ext *)0){
		    if(first_new_data == -1)
			first_new_data = i;
		    continue;
		}
		if(global_symbols[i].n_value != extp->value){
		    if(!printed_first_new_data && first_new_data != -1){
			error("First new data symbol: %s at address: 0x%x "
			      "before previous old data",
			      global_symbols[first_new_data].n_un.n_name,
			      (unsigned int)
			      global_symbols[first_new_data].n_value);
			printed_first_new_data = 1;
		    }
		    error("External data symbol: %s does NOT have the same "
			  "address (0x%x and 0x%x)", extp->name,
			  (unsigned int)extp->value,
			  (unsigned int)global_symbols[i].n_value);
		}
	    }
	    else if((global_symbols[i].n_type & N_TYPE) == N_SECT && 
	            global_symbols[i].n_sect == 1){
		hash_key = hash_string(global_symbols[i].n_un.n_name) %
							      ODDBALL_HASH_SIZE;
		obp = oddball_hash[hash_key];
		while(obp != (struct oddball *)0){
		    if(strcmp(obp->name, global_symbols[i].n_un.n_name) == 0)
			break;
		    obp = obp->next;
		}
		if(obp != (struct oddball *)0 &&
		   obp->nobranch == 1 && obp->private == 0){
		    extp = lookup(global_symbols[i].n_un.n_name);
		    if(extp != NULL){
			if(global_symbols[i].n_value != extp->value){
			    error("External nobranch_text symbol: %s does "
				  "NOT have the same address (0x%x and "
				  "0x%x)", extp->name,(unsigned int)extp->value,
				  (unsigned int)global_symbols[i].n_value);
			}
		    }
		}
	    }
	}
}

/*
 * Open the target shared library and return information for the symbol and
 * string table and the text segment.
 */
static
void
open_target(
char *target, 	       /* name of the target shared library to open */
struct ofile *ofile,   /* pointer to the ofile struct to use for target */
struct nlist **symbols,/* pointer to the symbol table to return */
long *nsyms,	       /* pointer to the number of symbols to return */
char **strings,	       /* pointer to the string table to return */
long *strsize,	       /* pointer to the string table size to return */
char **text,	       /* pointer to the text segment to return */
long *textaddr,	       /* pointer to the text segment's address to return */
long *textsize,	       /* pointer to the text segment's size to return */
long *text_nsect,      /* pointer to the text section number to return */
long *data_nsect)      /* pointer to the data section number to return */
{
    long i, j, nsect;
    struct load_command *lcp;
    struct symtab_command *stp;
    struct segment_command *text_seg, *sgp;
    struct section *s;
    enum bool family;
    const struct arch_flag *family_arch_flag;

	family = FALSE;
	if(arch_flag.name != NULL){
	    family_arch_flag = get_arch_family_from_cputype(arch_flag.cputype);
	    if(family_arch_flag != NULL)
		family = family_arch_flag->cpusubtype == arch_flag.cpusubtype;

	}

	if(ofile_map(target, NULL, NULL, ofile, FALSE) == FALSE)
	    exit(1);

	if(ofile->file_type == OFILE_FAT){
	    if(arch_flag.name == NULL && ofile->fat_header->nfat_arch != 1)
		fatal("-arch must be specified with use of fat files that "
		      "contain more than one architecture");

	    /*
	     * Set up the ofile struct for the architecture specified by the
	     * -arch flag or the first architecture if -arch flag is NULL and
	     * there is only one architecture.
	     */
	    if(arch_flag.name == NULL){
	       /* && ofile->fat_header->nfat_arch == 1 */
		if(ofile_first_arch(ofile) == FALSE)
		    exit(1);
		arch_flag.cputype = ofile->fat_archs[0].cputype;
		arch_flag.cpusubtype = ofile->fat_archs[0].cpusubtype;
	    }
	    else{
		if(ofile_first_arch(ofile) == FALSE)
		    if(errors)
			exit(1);
		do{
		    if(arch_flag.cputype == ofile->arch_flag.cputype &&
		       (arch_flag.cpusubtype == ofile->arch_flag.cpusubtype ||
			family == TRUE))
			break;
		}while(ofile_next_arch(ofile) == TRUE);
		if(errors)
		    exit(1);

		if(arch_flag.cputype != ofile->arch_flag.cputype ||
		   (arch_flag.cpusubtype != ofile->arch_flag.cpusubtype &&
		    family == FALSE))
		    fatal("architecture for -arch %s not found in fat file: %s",
			  arch_flag.name, target);
		arch_flag.cputype = ofile->arch_flag.cputype;
		arch_flag.cpusubtype = ofile->arch_flag.cpusubtype;
	    }
	    /*
	     * Now that the the ofile struct for the selected architecture
	     * is selected check to see that it is a target shared library.
	     */
	    if(ofile->arch_type == OFILE_ARCHIVE){
		fatal("fat file: %s for architecture %s is not a target shared "
		      "library (file is an archive)", target,
		       ofile->arch_flag.name);
	    }
	    else if(ofile->arch_type == OFILE_Mach_O){
		if(ofile->mh->filetype != MH_FVMLIB)
		    fatal("fat file: %s for architecture %s is not a target "
			  "shared library (wrong filetype)", target,
		          ofile->arch_flag.name);
	    }
	    else{ /* ofile->arch_type == OFILE_UNKNOWN */
		fatal("fat file: %s for architecture %s is not a target shared "
		      "library (wrong magic number)", target,
		      ofile->arch_flag.name);
	    }
	}
	else if(ofile->file_type == OFILE_ARCHIVE){
	    fatal("file: %s is not a target shared library (file is an "
		  "archive)", target);
	}
	else if(ofile->file_type == OFILE_Mach_O){
	    if(arch_flag.name != NULL){
		if(arch_flag.cputype != ofile->mh->cputype ||
		   (arch_flag.cpusubtype != ofile->mh->cpusubtype &&
		    family == FALSE))
		    fatal("architecture for -arch %s does match architecture "
			  "of file: %s", arch_flag.name, target);
	    }
	    if(ofile->mh->filetype != MH_FVMLIB)
		fatal("file: %s is not a target shared library (wrong "
		      "filetype)", target);
	}
	else{ /* ofile->file_type == OFILE_UNKNOWN */
	    fatal("file: %s is not a target shared library (wrong magic "
		  "number)", target);
	}

	/* Get the load comand for the symbol table and text segment */
	nsect = 1;
	lcp = ofile->load_commands;
	stp = (struct symtab_command *)0;
	text_seg = (struct segment_command *)0;
	for(i = 0; i < ofile->mh->ncmds; i++){
	    switch(lcp->cmd){ 
	    case LC_SYMTAB:
		if(stp != (struct symtab_command *)0)
		    fatal("More than one symtab_command in: %s", target);
		else
		    stp = (struct symtab_command *)lcp;
		break;
	    case LC_SEGMENT:
		sgp = (struct segment_command *)lcp;
		if(strcmp(sgp->segname, SEG_TEXT) == 0){
		    if(text_seg != (struct segment_command *)0)
			fatal("More than one segment_command for the %s "
			      "segment in: %s", SEG_TEXT, target);
		    else
			text_seg = (struct segment_command *)lcp;
		}
		s = (struct section *)((char *)sgp +
				       sizeof(struct segment_command));
		for(j = 0 ; j < sgp->nsects ; j++){
		    if(strcmp(s->segname, SEG_TEXT) == 0 &&
		       strcmp(s->sectname, SECT_TEXT) == 0)
			*text_nsect = nsect;
		    if(strcmp(s->segname, SEG_DATA) == 0 &&
		       strcmp(s->sectname, SECT_DATA) == 0)
			*data_nsect = nsect;
		    nsect++;
		    s++;
		}
		break;
	    }
	    lcp = (struct load_command *)((char *)lcp + lcp->cmdsize);
	}
	/* Check to see if the symbol table load command is good */
	if(stp == (struct symtab_command *)0)
	    fatal("No symtab_command in: %s", target);
	if(stp->nsyms == 0)
	    fatal("No symbol table in: %s", target);
	if(stp->strsize == 0)
	    fatal("No string table in: %s", target);
	*symbols = (struct nlist *)(ofile->object_addr + stp->symoff);
	*nsyms = stp->nsyms;
	if(ofile->object_byte_sex != host_byte_sex)
	    swap_nlist(*symbols, *nsyms, host_byte_sex);
	*strings = (char *)(ofile->object_addr + stp->stroff);
	*strsize = stp->strsize;

	for(i = 0; i < *nsyms; i++){
	    if(((*symbols)[i].n_type & N_EXT) != N_EXT)
		continue;
	    if((*symbols)[i].n_un.n_strx < 0 ||
	       (*symbols)[i].n_un.n_strx > *strsize)
		fatal("Bad string table index (%ld) for symbol %ld in: %s", 
	    	      (*symbols)[i].n_un.n_strx, i, target);
	    (*symbols)[i].n_un.n_name = *strings + (*symbols)[i].n_un.n_strx;
	}

	/* Check to see if the load command for the text segment is good */
	if(text_seg == (struct segment_command *)0)
	    fatal("No segment_command in: %s for the %s segment", target,
		  SEG_TEXT);
	*text = (char *)(ofile->object_addr + text_seg->fileoff);
	*textaddr = text_seg->vmaddr;
	*textsize = text_seg->filesize;
}

/*
 * Build a sorted list of external text symbols.
 */
static
void
make_sorted_text_symbols(
struct nlist *symbols,
long nsyms,
struct nlist **text_symbols,
long *ntext_symbols)
{
    long i, j;

	*ntext_symbols = 0;
	for(i = 0; i < nsyms; i++){
	    if((symbols[i].n_type == (N_SECT | N_EXT) && 
	        symbols[i].n_sect == 1))
		(*ntext_symbols)++;
	}

	/* Build a table of the external text symbols sorted by their address */
	*text_symbols = allocate(*ntext_symbols * sizeof(struct nlist));
	j = 0;
	for(i = 0; i < nsyms; i++){
	    if((symbols[i].n_type & N_EXT) != N_EXT)
		continue;
	    if((symbols[i].n_type & N_TYPE) == N_SECT && 
	       symbols[i].n_sect == 1)
		(*text_symbols)[j++] = symbols[i];
	}
	qsort(*text_symbols, *ntext_symbols, sizeof(struct nlist),
	      (int (*)(const void *, const void *))cmp_qsort);
}

/*
 * Build a list of external symbols.
 */
static
void
make_global_symbols(
struct nlist *symbols,
long nsyms,
struct nlist **global_symbols,
long *nglobal_symbols)
{
    long i, j;

	*nglobal_symbols = 0;
	for(i = 0; i < nsyms; i++){
	    if((symbols[i].n_type & N_EXT) == N_EXT)
		(*nglobal_symbols)++;
	}

	/* Build a table of the external symbols */
	*global_symbols = allocate(*nglobal_symbols * sizeof(struct nlist));
	j = 0;
	for(i = 0; i < nsyms; i++){
	    if((symbols[i].n_type & N_EXT) == N_EXT)
		(*global_symbols)[j++] = symbols[i];
	}
}

/*
 * Return the target address of the jmp instruction in the text for the given
 * value.
 */
static
long
get_target_addr(
long value,
char *name,
char *text,
long addr,
long size,
char *target)
{
    long offset;
    unsigned long target_addr;

	offset = value - addr;
	if(offset < 0 || offset > size)
	    fatal("Value (0x%x) of branch table symbol: %s not in "
		  "the %s segment of: %s", (unsigned int)value, name,
		  SEG_TEXT, target);

	if(arch_flag.cputype == CPU_TYPE_MC680x0){
	    unsigned short jmp;

	    if(offset + sizeof(short) + sizeof(long) > size)
		fatal("Branch instruction for branch table symbol: %s "
		      "would extend past the end of the %s segment in: "
		      " %s", name, SEG_TEXT, target);
	    memcpy(&jmp, text + offset, sizeof(unsigned short));
	    if(host_byte_sex != BIG_ENDIAN_BYTE_SEX)
		jmp = SWAP_SHORT(jmp);
	    /*
	     * look for a jump op code with a absolute long effective address
	     * (a "jmp (xxx).L" instruction)
	     */
	    if(jmp != 0x4ef9)
		fatal("Branch instruction not found at branch table "
		      "symbol: %s in: %s", name, target);
	    memcpy(&target_addr,
		   text + offset + sizeof(unsigned short),
		   sizeof(unsigned long));
	    if(host_byte_sex != BIG_ENDIAN_BYTE_SEX)
		target_addr = SWAP_LONG(target_addr);
	}
	else if(arch_flag.cputype == CPU_TYPE_POWERPC){
	    unsigned long inst, disp;

	    if(offset + sizeof(long) > size)
		fatal("Branch instruction for branch table symbol: %s "
		      "would extend past the end of the %s segment in: "
		      " %s", name, SEG_TEXT, target);
	    memcpy(&inst, text + offset, sizeof(unsigned long));
	    if(host_byte_sex != BIG_ENDIAN_BYTE_SEX)
		inst = SWAP_LONG(inst);
	    /* look for an uncondition branch op code (a "b" instruction) */
	    if((inst & 0xfc000003) != 0x48000000)
		fatal("Branch instruction not found at branch table "
		      "symbol: %s in: %s", name, target);
	    if((inst & 0x02000000) != 0)
		disp = 0xfc000000 | (inst & 0x03fffffc);
	    else
		disp = (inst & 0x03fffffc);
	    target_addr = value + disp;
	}
	else if(arch_flag.cputype == CPU_TYPE_MC88000){
	    unsigned long inst, disp;

	    if(offset + sizeof(long) > size)
		fatal("Branch instruction for branch table symbol: %s "
		      "would extend past the end of the %s segment in: "
		      " %s", name, SEG_TEXT, target);
	    memcpy(&inst, text + offset, sizeof(unsigned long));
	    if(host_byte_sex != BIG_ENDIAN_BYTE_SEX)
		inst = SWAP_LONG(inst);
	    /* look for an uncondition branch op code (a br instruction) */
	    if((inst & 0xfc000000) != 0xc0000000)
		fatal("Branch instruction not found at branch table "
		      "symbol: %s in: %s", name, target);
	    if((inst & 0x02000000) != 0)
		disp = 0xf0000000 | ((inst & 0x03ffffff) << 2);
	    else
		disp = (inst & 0x03ffffff) << 2;
	    target_addr = value + disp;
	}
	else if(arch_flag.cputype == CPU_TYPE_I386){
	    unsigned char jmp;
	    unsigned long disp;

	    if(offset + sizeof(char) + sizeof(long) > size)
		fatal("Branch instruction for branch table symbol: %s "
		      "would extend past the end of the %s segment in: "
		      " %s", name, SEG_TEXT, target);
	    memcpy(&jmp, text + offset, sizeof(unsigned char));
	    /*
	     * look for a jump op code with a 32 bit displacement
	     * (a JMP rel32 instruction)
	     */
	    if(jmp != 0xe9)
		fatal("Branch instruction not found at branch table "
		      "symbol: %s in: %s", name, target);
	    memcpy(&disp,
		   text + offset + sizeof(unsigned char),
		   sizeof(unsigned long));
	    if(host_byte_sex != LITTLE_ENDIAN_BYTE_SEX)
		disp = SWAP_LONG(disp);
	    target_addr = value + sizeof(unsigned char) +
			  sizeof(unsigned long) + disp;
	}
	else if(arch_flag.cputype == CPU_TYPE_HPPA){
	    unsigned long inst[2], disp;

	    if(offset + 2*sizeof(long) > size)
		fatal("Branch instruction for branch table symbol: %s "
		      "would extend past the end of the %s segment in: "
		      " %s", name, SEG_TEXT, target);
	    memcpy(&inst, text + offset, 2*sizeof(unsigned long));
	    if(host_byte_sex != BIG_ENDIAN_BYTE_SEX) {
		inst[0] = SWAP_LONG(inst[0]);
		inst[1] = SWAP_LONG(inst[1]);
		}
	    /*
	     * branch slot is following 2 instruction sequence:
	     *   ldil L`addr,%r1
             *   be,n R`addr(%sr4,%r1)
             */
	    if( ((inst[0] & 0xffe00000) != 0x20200000) ||
	        ((inst[1] & 0xffe0e002) != 0xe0202002))
		fatal("Branch instruction not found at branch table "
		      "symbol: %s in: %s", name, target);
	    disp = assemble_21(inst[0]&0x1fffff)<<11;
	    disp += sign_ext(assemble_17((inst[1]&0x1f0000)>>16,
	                        (inst[1]&0x1ffc)>>2,
			         inst[1]&1),17)<<2;
	    target_addr = disp;
	}
	else if(arch_flag.cputype == CPU_TYPE_SPARC){
	    unsigned long inst, disp;

	    if(offset + sizeof(long) > size)
		fatal("Branch instruction for branch table symbol: %s "
		      "would extend past the end of the %s segment in: "
		      " %s", name, SEG_TEXT, target);
	    memcpy(&inst, text + offset, sizeof(unsigned long));
	    if(host_byte_sex != BIG_ENDIAN_BYTE_SEX)
		inst = SWAP_LONG(inst);
	    /* look for an uncondition branch op code (a ba,a instruction) */
	    if((inst & 0xffc00000) != 0x30800000)
		fatal("Branch instruction not found at branch table "
		      "symbol: %s in: %s", name, target);
	    disp = (inst & 0x03fffff) << 2;
	    target_addr = value + disp;
	}
	else{
	    fatal("internal error: get_target_addr() called with arch_flag."
		  "cputype set to unknown value");
	    target_addr = 0;
	}
	return(target_addr);
}

/*
 * Function for qsort for comparing symbols.
 */
static
int
cmp_qsort(
struct nlist *sym1,
struct nlist *sym2)
{
	return(sym1->n_value - sym2->n_value);
}

/*
 * Function for bsearch for finding a symbol.
 */
static
int
cmp_bsearch(
long *value,
struct nlist *sym)
{
	return(*value - sym->n_value);
}

/*
 * Lookup a name in the external hash table.
 */
static
struct ext *
lookup(
char *name)
{
    long hash_key;
    struct ext *extp;

	hash_key = hash_string(name) % EXT_HASH_SIZE;
	extp = ext_hash[hash_key];
	while(extp != (struct ext *)0){
	    if(strcmp(name, extp->name) == 0)
		return(extp);
	    extp = extp->next;
	}
	return((struct ext *)0);
}


/*
 * a dummy routine called by the fatal() like routines
 */
void
cleanup(
int sig)
{
	exit(EXIT_FAILURE);
}
