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
#ifdef SHLIB
#include "shlib.h"
#endif /* SHLIB */
/*
 * This file contains the routines that deal with 4 byte literals sections.
 * A literal in this section must beable to me moved freely with respect to
 * other literals.  This means relocation must not reach outside the size of
 * the literal.  The size of this this type of section must be a multiple of
 * 4 bytes in all input files.
 */
#include <stdlib.h>
#if !(defined(KLD) && defined(__STATIC__))
#include <stdio.h>
#include <mach/mach.h>
#else /* defined(KLD) && defined(__STATIC__) */
#include <mach/kern_return.h>
#endif /* !(defined(KLD) && defined(__STATIC__)) */
#include <stdarg.h>
#include <string.h>
#include <mach-o/loader.h>
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "ld.h"
#include "objects.h"
#include "sections.h"
#include "4byte_literals.h"
#include "8byte_literals.h"
#include "pass2.h"

/*
 * literal4_merge() merges 4 byte literals from the specified section in the
 * current object file (cur_obj).  It allocates a fine relocation map and
 * sets the fine_relocs field in the section_map to it (as well as the count).
 */
__private_extern__
void
literal4_merge(
struct literal4_data *data,
struct merged_section *ms,
struct section *s,
struct section_map *section_map)
{
    unsigned long nliteral4s, i;
    struct literal4 *literal4s;
    struct fine_reloc *fine_relocs;

	if(s->size == 0){
	    section_map->fine_relocs = NULL;
	    section_map->nfine_relocs = 0;
	    return;
	}
	/*
	 * Calcualte the number of literals so the size of the fine relocation
	 * structures can be allocated.
	 */
	if(s->size % 4 != 0){
	    error_with_cur_obj("4 byte literal section (%.16s,%.16s) size is "
			       "not a multiple of 4 bytes", ms->s.segname,
			       ms->s.sectname);
	    return;
	}
	nliteral4s = s->size / 4;
#ifdef DEBUG
	data->nfiles++;
	data->nliterals += nliteral4s;
#endif /* DEBUG */

	fine_relocs = allocate(nliteral4s * sizeof(struct fine_reloc));
	memset(fine_relocs, '\0', nliteral4s * sizeof(struct fine_reloc));

	/*
	 * lookup and enter each C string in the section and record the offsets
	 * in the input file and in the output file.
	 */
	literal4s = (struct literal4 *)(cur_obj->obj_addr + s->offset);
	for(i = 0; i < nliteral4s; i++){
	    fine_relocs[i].input_offset = i * 4;
	    fine_relocs[i].output_offset =
					lookup_literal4(literal4s[i], data, ms);
	}
	section_map->fine_relocs = fine_relocs;
	section_map->nfine_relocs = nliteral4s;
}

/*
 * literal4_order() enters 4 byte literals from the order_file from the merged
 * section structure.  Since this is called before any call to literal4_merge
 * and it enters the literals in the order of the file it causes the section
 * to be ordered.
 */
__private_extern__
void
literal4_order(
struct literal4_data *data,
struct merged_section *ms)
{
    unsigned long i, line_number;
    struct literal4 literal4;

	line_number = 1;
	i = 0;
	while(i < ms->order_size){
	    if(get_hex_from_sectorder(ms, &i, &(literal4.long0), line_number) ==
	       TRUE)
		(void)lookup_literal4(literal4, data, ms);
	    while(i < ms->order_size && ms->order_addr[i] != '\n')
		i++;
	    if(i < ms->order_size && ms->order_addr[i] == '\n')
		i++;
	    line_number++;
	}
}

/*
 * lookup_literal4() looks up the 4 byte literal passed to it in the
 * literal4_data passed to it and returns the offset the 4 byte literal will
 * have in the output file.  It creates the blocks to store the literals and
 * attaches them to the literal4_data passed to it.  The total size of the
 * section is accumulated in ms->s.size which is the merged section for this
 * literal section.  The literal is aligned to the alignment in the merged
 * section (ms->s.align).
 */
__private_extern__
unsigned long
lookup_literal4(
struct literal4 literal4,
struct literal4_data *data,
struct merged_section *ms)
{
    struct literal4_block **p, *literal4_block;
    unsigned long align_multiplier, output_offset, i;

	align_multiplier = 1;
 	if((1 << ms->s.align) > 4)
	    align_multiplier = (1 << ms->s.align) / 4;

	output_offset = 0;
	for(p = &(data->literal4_blocks); *p ; p = &(literal4_block->next)){
	    literal4_block = *p;
	    for(i = 0; i < literal4_block->used; i++){
		if(literal4.long0 == literal4_block->literal4s[i].long0)
		    return(output_offset + i * 4 * align_multiplier);
	    }
	    if(literal4_block->used != LITERAL4_BLOCK_SIZE){
		literal4_block->literal4s[i].long0 = literal4.long0;
		literal4_block->used++;
		ms->s.size += 4 * align_multiplier;
		return(output_offset + i * 4 * align_multiplier);
	    }
	    output_offset += literal4_block->used * 4 * align_multiplier;
	}
	*p = allocate(sizeof(struct literal4_block));
	literal4_block = *p;
	literal4_block->used = 1;
	literal4_block->literal4s[0].long0 = literal4.long0;
	literal4_block->next = NULL;

	ms->s.size += 4 * align_multiplier;
	return(output_offset);
}

/*
 * literal4_output() copies the 4 byte literals for the data passed to it into
 * the output file's buffer.  The pointer to the merged section passed to it is
 * used to tell where in the output file this section goes.  Then this routine
 * calls literal4_free to free() up all space used by the data block except the
 * data block itself.
 */
__private_extern__
void
literal4_output(
struct literal4_data *data,
struct merged_section *ms)
{
    unsigned long align_multiplier, i, offset;
    struct literal4_block **p, *literal4_block;

	align_multiplier = 1;
 	if((1 << ms->s.align) > 4)
	    align_multiplier = (1 << ms->s.align) / 4;

	/*
	 * Copy the literals into the output file.
	 */
	offset = ms->s.offset;
	for(p = &(data->literal4_blocks); *p ;){
	    literal4_block = *p;
	    for(i = 0; i < literal4_block->used; i++){
		memcpy(output_addr + offset,
		       literal4_block->literal4s + i,
		       sizeof(struct literal4));
		offset += 4 * align_multiplier;
	    }
	    p = &(literal4_block->next);
	}
#ifndef RLD
	output_flush(ms->s.offset, offset - ms->s.offset);
#endif /* !defined(RLD) */
}

/*
 * literal4_free() free()'s up all space used by the data block except the
 * data block itself.
 */
__private_extern__
void
literal4_free(
struct literal4_data *data)
{
    struct literal4_block *literal4_block, *next_literal4_block;

	/*
	 * Free all data for this block.
	 */
	for(literal4_block = data->literal4_blocks; literal4_block ;){
	    next_literal4_block = literal4_block->next;
	    free(literal4_block);
	    literal4_block = next_literal4_block;
	}
	data->literal4_blocks = NULL;
}

#ifdef DEBUG
/*
 * print_literal4_data() prints a literal4_data.  Used for debugging.
 */
__private_extern__
void
print_literal4_data(
struct literal4_data *data,
char *indent)
{
    unsigned long i;
    struct literal4_block **p, *literal4_block;

	print("%s4 byte literal data at 0x%x\n", indent, (unsigned int)data);
	if(data == NULL)
	    return;
	print("%s   literal4_blocks 0x%x\n", indent,
	      (unsigned int)(data->literal4_blocks));
	for(p = &(data->literal4_blocks); *p ; p = &(literal4_block->next)){
	    literal4_block = *p;
	    print("%s\tused %lu\n", indent, literal4_block->used);
	    print("%s\tnext 0x%x\n", indent,
		  (unsigned int)(literal4_block->next));
	    print("%s\tliteral4s\n", indent);
	    for(i = 0; i < literal4_block->used; i++){
		print("%s\t    0x%08x\n", indent,
		      (unsigned int)(literal4_block->literal4s[i].long0));
	    }
	}
}

/*
 * literal4_data_stats() prints the literal4_data stats.  Used for tuning.
 */
__private_extern__
void
literal4_data_stats(
struct literal4_data *data,
struct merged_section *ms)
{
	if(data == NULL)
	    return;
	print("literal4 section (%.16s,%.16s) contains:\n",
	      ms->s.segname, ms->s.sectname);
	print("    %lu merged literals \n", ms->s.size / 4);
	print("    from %lu files and %lu total literals from those "
	      "files\n", data->nfiles, data->nliterals);
	print("    average number of literals per file %g\n",
	      (double)((double)data->nliterals / (double)(data->nfiles)));
}
#endif /* DEBUG */
