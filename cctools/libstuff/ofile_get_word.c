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
#include <string.h>
#include "stuff/ofile.h"

/*
 * ofile_get_word() gets a 32 bit word for the address in the object file.
 */
__private_extern__
long
ofile_get_word(
unsigned long addr,
unsigned long *word,
void *get_word_data /* struct mach_object_file *ofile */ )
{
    unsigned long i, j;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    struct ofile *ofile;

	ofile = (struct ofile *)get_word_data;
	for(i = 0, lc = ofile->load_commands; i < ofile->mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		for(j = 0 ; j < sg->nsects ; j++){
		    if(addr >= s->addr && addr < s->addr + s->size){
			if(s->flags == S_ZEROFILL)
			    *word = 0;
			else {
			    if(s->offset > ofile->object_size ||
			       s->offset + s->size > ofile->object_size ||
			       s->offset % sizeof(long) != 0 ||
			       (addr - s->addr) % sizeof(long) != 0)
				return(-1);
			    else{
				memcpy(word, (ofile->object_addr +
					       (s->offset + addr - s->addr)),
					sizeof(unsigned long));
				if(ofile->object_byte_sex !=get_host_byte_sex())
				    *word = SWAP_LONG(*word);
			    }
			}
			return(0);
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	return(-1);
}
