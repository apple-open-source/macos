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
#include <string.h>
#include "stuff/ofile.h"
#include "stuff/breakout.h"

static void check_object(
    struct arch *arch,
    struct member *member,
    struct object *object);

static void symbol_string_at_end(
    struct arch *arch,
    struct member *member,
    struct object *object);

static void dyld_order(
    struct arch *arch,
    struct member *member,
    struct object *object);

static void order_error(
    struct arch *arch,
    struct member *member,
    char *reason);

__private_extern__
void
checkout(
struct arch *archs,
unsigned long narchs)
{
    unsigned long i, j;

	for(i = 0; i < narchs; i++){
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O){
			check_object(archs + i, archs[i].members + j,
					 archs[i].members[j].object);
		    }
		}
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		check_object(archs + i, NULL, archs[i].object);
	    }
	}
}

static
void
check_object(
struct arch *arch,
struct member *member,
struct object *object)
{

    unsigned long i;
    struct load_command *lc;
    struct segment_command *sg;

	/*
	 * Set up the symtab load command field and link edit segment feilds in
	 * the object structure.
	 */
	object->st = NULL;
	object->dyst = NULL;
	object->seg_linkedit = NULL;
	lc = object->load_commands;
	for(i = 0; i < object->mh->ncmds; i++){
	    if(lc->cmd == LC_SYMTAB){
		if(object->st != NULL)
		    fatal_arch(arch, member, "malformed file (more than one "
			"LC_SYMTAB load command): ");
		object->st = (struct symtab_command *)lc;
	    }
	    else if(lc->cmd == LC_DYSYMTAB){
		if(object->dyst != NULL)
		    fatal_arch(arch, member, "malformed file (more than one "
			"LC_DYSYMTAB load command): ");
		object->dyst = (struct dysymtab_command *)lc;
	    }
	    else if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0){
		    if(object->seg_linkedit != NULL)
			fatal_arch(arch, member, "malformed file (more than "
			    "one " SEG_LINKEDIT "segment): ");
		    object->seg_linkedit = sg;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * For objects without a dynamic symbol table check to see that the
	 * string table is at the end of the file and that the symbol table is
	 * just before it.
	 */
	if(object->dyst == NULL){
	    symbol_string_at_end(arch, member, object);
	}
	else{
	    /*
	     * This file has a dynamic symbol table command.  We handle three
	     * cases, a dynamic shared library, a file for the dynamic linker,
	     * and a relocatable object file.  Since it has a dynamic symbol
	     * table command it could have an indirect symbol table.
	     */
	    if(object->mh->filetype == MH_DYLIB){
		/*
		 * This is a dynamic shared library.
		 * The order of the symbolic info is:
		 * 	local relocation entries
		 *	symbol table
		 *		local symbols
		 *		defined external symbols
		 *		undefined symbols
		 * 	external relocation entries
		 *	table of contents
		 * 	module table
		 *	reference table
		 *	indirect symbol table
		 *	string table
		 *		strings for external symbols
		 *		strings for local symbols
		 */
		dyld_order(arch, member, object);
	    }
	    else if(object->mh->flags & MH_DYLDLINK){
		/*
		 * This is a file for the dynamic linker (output of ld(1) with
		 * -output_for_dyld .  That is the relocation entries are split
		 * into local and external and hanging off the dysymtab not off
		 * the sections.
		 * The order of the symbolic info is:
		 * 	local relocation entries
		 *	symbol table
		 *		local symbols (in order as appeared in stabs)
		 *		defined external symbols (sorted by name)
		 *		undefined symbols (sorted by name)
		 * 	external relocation entries
		 *	indirect symbol table
		 *	string table
		 *		strings for external symbols
		 *		strings for local symbols
		 */
		dyld_order(arch, member, object);
	    }
	    else{
		/*
		 * This is a relocatable object file either the output of the
		 * assembler or output of ld(1) with -r.  For the output of
		 * the assembler:
		 * The order of the symbolic info is:
		 * 	relocation entries (by section)
		 *	indirect symbol table
		 *	symbol table
		 *		local symbols (in order as appeared in stabs)
		 *		defined external symbols (sorted by name)
		 *		undefined symbols (sorted by name)
		 *	string table
		 *		strings for external symbols
		 *		strings for local symbols
		 * With this order the symbol table can be replaced and the
		 * relocation entries and the indirect symbol table entries
		 * can be updated in the file and not moved.
		 * For the output of ld -r:
		 * The order of the symbolic info is:
		 * 	relocation entries (by section)
		 *	symbol table
		 *		local symbols (in order as appeared in stabs)
		 *		defined external symbols (sorted by name)
		 *		undefined symbols (sorted by name)
		 *	indirect symbol table
		 *	string table
		 *		strings for external symbols
		 *		strings for local symbols
		 */
		symbol_string_at_end(arch, member, object);
	    }
	}
}

static
void
dyld_order(
struct arch *arch,
struct member *member,
struct object *object)
{
    unsigned long offset;

	if(object->seg_linkedit == NULL)
	    fatal_arch(arch, member, "malformed file (no " SEG_LINKEDIT
		" segment): ");
	if(object->seg_linkedit->filesize != 0 &&
	   object->seg_linkedit->fileoff +
	   object->seg_linkedit->filesize != object->object_size)
	    fatal_arch(arch, member, "the " SEG_LINKEDIT " segment "
		"does not cover the end of the file (can't "
		"be processed) in: ");

	offset = object->seg_linkedit->fileoff;
	if(object->dyst->nlocrel != 0){
	    if(object->dyst->locreloff != offset)
		order_error(arch, member, "local relocation entries "
		    "out of place");
	    offset += object->dyst->nlocrel *
		      sizeof(struct relocation_info);
	}
	if(object->st->nsyms != 0){
	    if(object->st->symoff != offset)
		order_error(arch, member, "symbol table out of place");
	    offset += object->st->nsyms * sizeof(struct nlist);
	}
	if(object->dyst->nextrel != 0){
	    if(object->dyst->extreloff != offset)
		order_error(arch, member, "external relocation entries"
		    " out of place");
	    offset += object->dyst->nextrel *
		      sizeof(struct relocation_info);
	}
	if(object->dyst->nindirectsyms != 0){
	    if(object->dyst->indirectsymoff != offset)
		order_error(arch, member, "indirect symbol table "
		    "out of place");
	    offset += object->dyst->nindirectsyms *
		      sizeof(unsigned long);
	}
	if(object->dyst->ntoc != 0){
	    if(object->dyst->tocoff != offset)
		order_error(arch, member, "table of contents out of place");
	    offset += object->dyst->ntoc *
		      sizeof(struct dylib_table_of_contents);
	}
	if(object->dyst->nmodtab != 0){
	    if(object->dyst->modtaboff != offset)
		order_error(arch, member, "module table out of place");
	    offset += object->dyst->nmodtab *
		      sizeof(struct dylib_module);
	}
	if(object->dyst->nextrefsyms != 0){
	    if(object->dyst->extrefsymoff != offset)
		order_error(arch, member, "reference table out of place");
	    offset += object->dyst->nextrefsyms *
		      sizeof(struct dylib_reference);
	}
	if(object->st->strsize != 0){
	    if(object->st->stroff != offset)
		order_error(arch, member, "string table out of place");
	    offset += object->st->strsize;
	}
	if(offset != object->object_size)
	    order_error(arch, member, "string table out of place");
}

static
void
order_error(
struct arch *arch,
struct member *member,
char *reason)
{
	fatal_arch(arch, member, "file not in an order that can be processed "
		   "(%s): ", reason);
}

static
void
symbol_string_at_end(
struct arch *arch,
struct member *member,
struct object *object)
{
    unsigned long end;


	if(object->st != NULL && object->st->nsyms != 0){
	    end = object->object_size;
	    if(object->st->strsize != 0){
		if(object->st->stroff + object->st->strsize != end)
		    fatal_arch(arch, member, "string table not at the end "
			"of the file (can't be processed) in file: ");
		end = object->st->stroff;
	    }
	    if(object->dyst != NULL &&
	       object->dyst->nindirectsyms != 0 &&
	       object->st->nsyms != 0 &&
	       object->dyst->indirectsymoff > object->st->symoff){
		if(object->dyst->indirectsymoff +
		   object->dyst->nindirectsyms * sizeof(unsigned long) != end){
		    fatal_arch(arch, member, "indirect symbol table does not "
			"directly preceed the string table (can't be "
			"processed) in file: ");
		}
		end = object->dyst->indirectsymoff;
		if(object->st->symoff +
		   object->st->nsyms * sizeof(struct nlist) != end)
		    fatal_arch(arch, member, "symbol table does not directly "
			"preceed the indirect symbol table (can't be "
			"processed) in file: ");
	    }
	    else if(object->st->symoff +
	       object->st->nsyms * sizeof(struct nlist) != end){
		fatal_arch(arch, member, "symbol table and string table "
		    "not at the end of the file (can't be processed) in "
		    "file: ");
	    }
	    if(object->seg_linkedit != NULL &&
	       (object->seg_linkedit->flags & SG_FVMLIB) != SG_FVMLIB &&
	       object->seg_linkedit->filesize != 0){
		if(object->seg_linkedit->fileoff +
		   object->seg_linkedit->filesize != object->object_size)
		    fatal_arch(arch, member, "the " SEG_LINKEDIT " segment "
			"does not cover the symbol and string table (can't "
			"be processed) in file: ");
	    }
	}
}
