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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stuff/errors.h"
#include "stuff/breakout.h"
#include "stuff/round.h"

/* used by error routines as the name of the program */
char *progname = NULL;

static void usage(
    void);

static void process(
    struct arch *archs,
    unsigned long narchs);

static void setup_object_symbolic_info(
    struct object *object);

int
main(
int argc,
char **argv,
char **envp)
{
    unsigned long i;
    char *input, *output;
    struct arch *archs;
    unsigned long narchs;

	progname = argv[0];
	input = NULL;
	output = NULL;
	archs = NULL;
	narchs = 0;
	for(i = 1; i < argc; i++){
	    if(strcmp(argv[i], "-o") == 0){
		if(i + 1 == argc){
		    error("missing argument(s) to: %s option", argv[i]);
		    usage();
		}
		if(output != NULL){
		    error("more than one: %s option specified", argv[i]);
		    usage();
		}
		output = argv[i+1];
		i++;
	    }
	    else{
		if(input != NULL){
		    error("more than one input file specified (%s and %s)",
			  argv[i], input);
		    usage();
		}
		input = argv[i];
	    }
	}
	if(input == NULL || output == NULL)
	    usage();

	breakout(input, &archs, &narchs, FALSE);
	if(errors)
	    exit(EXIT_FAILURE);

	checkout(archs, narchs);

	process(archs, narchs);

	writeout(archs, narchs, output, 0777, TRUE, FALSE, FALSE);

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
	fprintf(stderr, "Usage: %s input -o output\n", progname);
	exit(EXIT_FAILURE);
}

static
void
process(
struct arch *archs,
unsigned long narchs)
{
    unsigned long i, j, offset, size;
    struct object *object;

	for(i = 0; i < narchs; i++){
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O){
			object = archs[i].members[j].object;
			setup_object_symbolic_info(object);
		    }
		}
		/*
		 * Reset the library offsets and size.
		 */
		offset = 0;
		for(j = 0; j < archs[i].nmembers; j++){
		    archs[i].members[j].offset = offset;
		    size = 0;
		    if(archs[i].members[j].member_long_name == TRUE){
			size = round(archs[i].members[j].member_name_size,
				     sizeof(long));
			archs[i].toc_long_name = TRUE;
		    }
		    if(archs[i].members[j].object != NULL){
			size += archs[i].members[j].object->object_size
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
			size += archs[i].members[j].unknown_size;
		    }
		    offset += sizeof(struct ar_hdr) + size;
		}
		archs[i].library_size = offset;
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		object = archs[i].object;
		setup_object_symbolic_info(object);
	    }
	}
}

static
void
setup_object_symbolic_info(
struct object *object)
{
	if(object->st != NULL && object->st->nsyms != 0){
	    object->output_symbols = (struct nlist *)
		(object->object_addr + object->st->symoff);
	    if(object->object_byte_sex != get_host_byte_sex())
		swap_nlist(object->output_symbols,
			   object->st->nsyms,
			   get_host_byte_sex());
	    object->output_nsymbols = object->st->nsyms;
	    object->output_strings =
		object->object_addr + object->st->stroff;
	    object->output_strings_size = object->st->strsize;
	    object->input_sym_info_size =
		object->st->nsyms * sizeof(struct nlist) +
		object->st->strsize;
	    object->output_sym_info_size =
		object->input_sym_info_size;
	}
	if(object->dyst != NULL){
	    object->output_ilocalsym = object->dyst->ilocalsym;
	    object->output_nlocalsym = object->dyst->nlocalsym;
	    object->output_iextdefsym = object->dyst->iextdefsym;
	    object->output_nextdefsym = object->dyst->nextdefsym;
	    object->output_iundefsym = object->dyst->iundefsym;
	    object->output_nundefsym = object->dyst->nundefsym;

	    object->output_loc_relocs = (struct relocation_info *)
		(object->object_addr + object->dyst->locreloff);
	    object->output_ext_relocs = (struct relocation_info *)
		(object->object_addr + object->dyst->extreloff);
	    object->output_indirect_symtab = (unsigned long *)
		(object->object_addr + object->dyst->indirectsymoff);
	    object->output_tocs =
		(struct dylib_table_of_contents *)
		(object->object_addr + object->dyst->tocoff);
	    object->output_ntoc = object->dyst->ntoc;
	    object->output_mods = (struct dylib_module *)
		(object->object_addr + object->dyst->modtaboff);
	    object->output_nmodtab = object->dyst->nmodtab;
	    object->output_refs = (struct dylib_reference *)
		(object->object_addr + object->dyst->extrefsymoff);
	    object->output_nextrefsyms = object->dyst->nextrefsyms;
	    object->input_sym_info_size +=
		object->dyst->nlocrel *
		    sizeof(struct relocation_info) +
		object->dyst->nextrel *
		    sizeof(struct relocation_info) +
		object->dyst->nindirectsyms *
		    sizeof(unsigned long) +
		object->dyst->ntoc *
		    sizeof(struct dylib_table_of_contents)+
		object->dyst->nmodtab *
		    sizeof(struct dylib_module) +
		object->dyst->nextrefsyms *
		    sizeof(struct dylib_reference);
	    object->output_sym_info_size +=
		object->dyst->nlocrel *
		    sizeof(struct relocation_info) +
		object->dyst->nextrel *
		    sizeof(struct relocation_info) +
		object->dyst->nindirectsyms *
		    sizeof(unsigned long) +
		object->dyst->ntoc *
		    sizeof(struct dylib_table_of_contents)+
		object->dyst->nmodtab *
		    sizeof(struct dylib_module) +
		object->dyst->nextrefsyms *
		    sizeof(struct dylib_reference);
	    if(object->hints_cmd != NULL){
		object->output_hints = (struct twolevel_hint *)
		    (object->object_addr +
		     object->hints_cmd->offset);
		object->input_sym_info_size +=
		    object->hints_cmd->nhints *
		    sizeof(struct twolevel_hint);
		object->output_sym_info_size +=
		    object->hints_cmd->nhints *
		    sizeof(struct twolevel_hint);
	    }
	}
}
