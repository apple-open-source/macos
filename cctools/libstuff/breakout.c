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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stuff/ofile.h"
#include "stuff/breakout.h"
#include "stuff/allocate.h"
#include "stuff/errors.h"
#include "stuff/round.h"
#include "stuff/crc32.h"

static void cksum_object(
    struct arch *arch,
    enum bool calculate_input_prebind_cksum);
static struct arch *new_arch(
    struct arch **archs,
    unsigned long *narchs);
static struct member *new_member(
    struct arch *arch);

__private_extern__
struct ofile *
breakout(
char *filename,
struct arch **archs,
unsigned long *narchs,
enum bool calculate_input_prebind_cksum)
{
    struct ofile *ofile;
    struct arch *arch;
    struct member *member;
    unsigned long previous_errors, size;
    enum bool flag;
    struct ar_hdr *ar_hdr;

	*archs = NULL;
	*narchs = 0;
	ofile = allocate(sizeof(struct ofile));
	/*
	 * Rely on the ofile_*() routines to do all the checking and only
	 * return valid ofiles files broken out.
	 */
	if(ofile_map(filename, NULL, NULL, ofile, FALSE) == FALSE){
	    free(ofile);
	    return(NULL);
	}

	previous_errors = errors;
	errors = 0;
	if(ofile->file_type == OFILE_FAT && errors == 0){
	    /* loop through the fat architectures (can't have zero archs) */
	    (void)ofile_first_arch(ofile);
	    do{
		if(errors != 0)
		    break;
		arch = new_arch(archs, narchs);
		arch->file_name = savestr(filename);
		arch->type = ofile->arch_type;
		arch->fat_arch = ofile->fat_archs + ofile->narch;
		arch->fat_arch_name = savestr(ofile->arch_flag.name);

		if(ofile->arch_type == OFILE_ARCHIVE){
		    /* loop through archive (can be empty) */
		    if((flag = ofile_first_member(ofile)) == TRUE){
			/*
			 * If the first member is a table of contents then skip
			 * it as it is always rebuilt (so to get the time to
			 * match the modtime so it won't appear out of date).
			 */
			if(ofile->member_ar_hdr != NULL &&
			   strncmp(ofile->member_name, SYMDEF,
				   sizeof(SYMDEF) - 1) == 0){
			    arch->toc_long_name = (enum bool)
						(ofile->member_name !=
						 ofile->member_ar_hdr->ar_name);
			    flag = ofile_next_member(ofile);
			}
			while(flag == TRUE){
			    member = new_member(arch);
			    member->type = ofile->member_type;
			    member->ar_hdr = ofile->member_ar_hdr;
			    member->member_name = ofile->member_name;
			    member->member_name_size = ofile->member_name_size;
			    member->member_long_name = (enum bool)
						(ofile->member_name !=
						 ofile->member_ar_hdr->ar_name);
			    member->offset = arch->library_size;
			    size = sizeof(struct ar_hdr) +
				   round(ofile->member_size,
					 sizeof(long));
			    if(member->member_long_name == TRUE)
				size += round(ofile->member_name_size,
					      sizeof(long));
			    arch->library_size += size;
			    /*
			     * For library members that are not a multiple of
			     * sizeof(long) their size is incresed and '\n' are
			     * added in writeout() to make it so.
			     */
			    if(ofile->member_size != 
			       round(ofile->member_size, sizeof(long)) ||
			       ofile->member_name_size !=
			       round(ofile->member_name_size, sizeof(long))){
				ar_hdr = allocate(sizeof(struct ar_hdr));
				*ar_hdr = *(ofile->member_ar_hdr);
	    			sprintf(ar_hdr->ar_size, "%-*ld",
	       				(int)sizeof(ar_hdr->ar_size), size);
				/*
				 * This has to be done by hand because sprintf
				 * puts a null at the end of the buffer.
				 */
				memcpy(ar_hdr->ar_fmag, ARFMAG,
				       (int)sizeof(ar_hdr->ar_fmag));
				member->ar_hdr = ar_hdr;
			    }
			    member->input_ar_hdr = ofile->member_ar_hdr;
			    member->input_file_name = filename;
			    if(ofile->member_type == OFILE_Mach_O){
				member->object =
					allocate(sizeof(struct object));
				memset(member->object, '\0',
					sizeof(struct object));
				member->object->object_addr =
					ofile->object_addr;
				member->object->object_size =
					ofile->object_size;
				member->object->object_byte_sex =
					ofile->object_byte_sex;
				member->object->mh = ofile->mh;
				member->object->load_commands =
					ofile->load_commands;
			    }
			    else{ /* ofile->member_type == OFILE_UNKNOWN */
				member->unknown_addr = ofile->member_addr;
				member->unknown_size = ofile->member_size;
			    }
			    flag = ofile_next_member(ofile);
			}
		    }
		}
		else if(ofile->arch_type == OFILE_Mach_O){
		    arch->object = allocate(sizeof(struct object));
		    memset(arch->object, '\0', sizeof(struct object));
		    arch->object->object_addr = ofile->object_addr;
		    arch->object->object_size = ofile->object_size;
		    arch->object->object_byte_sex = ofile->object_byte_sex;
		    arch->object->mh = ofile->mh;
		    arch->object->load_commands = ofile->load_commands;
		    cksum_object(arch, calculate_input_prebind_cksum);
		}
		else{ /* ofile->arch_type == OFILE_UNKNOWN */
		    arch->unknown_addr = ofile->file_addr +
					 arch->fat_arch->offset;
		    arch->unknown_size = arch->fat_arch->size;
		}
	    }while(ofile_next_arch(ofile) == TRUE);
	}
	else if(ofile->file_type == OFILE_ARCHIVE && errors == 0){
	    arch = new_arch(archs, narchs);
	    arch->file_name = savestr(filename);
	    arch->type = ofile->file_type;

	    /* loop through archive (can be empty) */
	    if((flag = ofile_first_member(ofile)) == TRUE && errors == 0){
		/*
		 * If the first member is a table of contents then skip
		 * it as it is always rebuilt (so to get the time to
		 * match the modtime so it won't appear out of date).
		 */
		if(ofile->member_ar_hdr != NULL &&
		   strncmp(ofile->member_name, SYMDEF,
			   sizeof(SYMDEF) - 1) == 0){
		    arch->toc_long_name = (enum bool)
					  (ofile->member_name !=
					   ofile->member_ar_hdr->ar_name);
		    flag = ofile_next_member(ofile);
		}
		while(flag == TRUE && errors == 0){
		    member = new_member(arch);
		    member->type = ofile->member_type;
		    member->ar_hdr = ofile->member_ar_hdr;
		    member->member_name = ofile->member_name;
		    member->member_name_size = ofile->member_name_size;
		    member->member_long_name = (enum bool)
					(ofile->member_name !=
					 ofile->member_ar_hdr->ar_name);
		    member->offset = arch->library_size;
		    size = sizeof(struct ar_hdr) +
			   round(ofile->member_size,
				 sizeof(long));
		    if(member->member_long_name == TRUE)
			size += round(ofile->member_name_size,
				      sizeof(long));
		    arch->library_size += size;
		    /*
		     * For library members that are not a multiple of
		     * sizeof(long) their size is incresed and '\n' are
		     * added in writeout() to make it so.
		     */
		    if(ofile->member_size != 
		       round(ofile->member_size, sizeof(long)) ||
		       ofile->member_name_size !=
		       round(ofile->member_name_size, sizeof(long))){
			ar_hdr = allocate(sizeof(struct ar_hdr));
			*ar_hdr = *(ofile->member_ar_hdr);
			sprintf(ar_hdr->ar_size, "%-*ld",
				(int)sizeof(ar_hdr->ar_size), size);
			/*
			 * This has to be done by hand because sprintf
			 * puts a null at the end of the buffer.
			 */
			memcpy(ar_hdr->ar_fmag, ARFMAG,
			       (int)sizeof(ar_hdr->ar_fmag));
			member->ar_hdr = ar_hdr;
		    }
		    member->input_ar_hdr = ofile->member_ar_hdr;
		    member->input_file_name = filename;
		    if(ofile->member_type == OFILE_Mach_O){
			member->object = allocate(sizeof(struct object));
			memset(member->object, '\0', sizeof(struct object));
			member->object->object_addr = ofile->object_addr;
			member->object->object_size = ofile->object_size;
			member->object->object_byte_sex =ofile->object_byte_sex;
			member->object->mh = ofile->mh;
			member->object->load_commands = ofile->load_commands;
		    }
		    else{ /* ofile->member_type == OFILE_UNKNOWN */
			member->unknown_addr = ofile->member_addr;
			member->unknown_size = ofile->member_size;
		    }
		    flag = ofile_next_member(ofile);
		}
	    }
	}
	else if(ofile->file_type == OFILE_Mach_O && errors == 0){
	    arch = new_arch(archs, narchs);
	    arch->file_name = savestr(filename);
	    arch->type = ofile->file_type;
	    arch->object = allocate(sizeof(struct object));
	    memset(arch->object, '\0', sizeof(struct object));
	    arch->object->object_addr = ofile->object_addr;
	    arch->object->object_size = ofile->object_size;
	    arch->object->object_byte_sex = ofile->object_byte_sex;
	    arch->object->mh = ofile->mh;
	    arch->object->load_commands = ofile->load_commands;
	    cksum_object(arch, calculate_input_prebind_cksum);
	}
	else if(errors == 0){ /* ofile->file_type == OFILE_UNKNOWN */
	    arch = new_arch(archs, narchs);
	    arch->file_name = savestr(filename);
	    arch->type = ofile->file_type;
	    arch->unknown_addr = ofile->file_addr;
	    arch->unknown_size = ofile->file_size;
	}
	if(errors != 0){
	    free_archs(*archs, *narchs);
	}
	errors += previous_errors;
	return(ofile);
}

/*
 * cksum_object() is called to set the pointer to the LC_PREBIND_CKSUM load
 * command in the object struct for the specified arch.  If the parameter
 * calculate_input_prebind_cksum is TRUE then calculate the value
 * of the check sum for the input object if needed, set that into the
 * the calculated_input_prebind_cksum field of the object struct for the
 * specified arch.  This is needed for prebound files where the original
 * checksum (or zero) is recorded in the LC_PREBIND_CKSUM load command.
 * Only redo_prebinding operations sets the value of the cksum field to
 * non-zero and only if previously zero.  All other operations will set this
 * field to zero indicating a new original prebound file.
 */
static
void
cksum_object(
struct arch *arch,
enum bool calculate_input_prebind_cksum)
{
    unsigned long i, buf_size;
    struct load_command *lc;
    enum byte_sex host_byte_sex;
    char *buf;

	arch->object->cs = NULL;
	lc = arch->object->load_commands;
	for(i = 0;
	    i < arch->object->mh->ncmds && arch->object->cs == NULL;
	    i++){
	    if(lc->cmd == LC_PREBIND_CKSUM)
		arch->object->cs = (struct prebind_cksum_command *)lc;
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	/*
	 * If we don't want to calculate the input check sum, or there is no
	 * LC_PREBIND_CKSUM load command or there is one and the check sum is
	 * not zero then return.
	 */
	if(calculate_input_prebind_cksum == FALSE ||
	   arch->object->cs == NULL ||
	   arch->object->cs->cksum != 0)
	    return;


	host_byte_sex = get_host_byte_sex();
	buf_size = 0;
	buf = NULL;
	if(arch->object->object_byte_sex != host_byte_sex){
	    buf_size = sizeof(struct mach_header) +
		       arch->object->mh->sizeofcmds;
	    buf = allocate(buf_size);
	    memcpy(buf, arch->object->mh, buf_size);
	    if(swap_object_headers(arch->object->mh,
				   arch->object->load_commands) == FALSE)
		return;
	}

	arch->object->calculated_input_prebind_cksum =
	    crc32(arch->object->object_addr, arch->object->object_size);

	if(arch->object->object_byte_sex != host_byte_sex){
	    memcpy(arch->object->mh, buf, buf_size);
	    free(buf);
	}
}

__private_extern__
void
free_archs(
struct arch *archs,
unsigned long narchs)
{
    unsigned long i, j;

	for(i = 0; i < narchs; i++){
	    if(archs[i].type == OFILE_ARCHIVE){
		for(j = 0; j < archs[i].nmembers; j++){
		    if(archs[i].members[j].type == OFILE_Mach_O)
			free(archs[i].members[j].object);
		}
		if(archs[i].nmembers > 0 && archs[i].members != NULL)
		    free(archs[i].members);
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		free(archs[i].object);
	    }
	}
	if(narchs > 0 && archs != NULL)
	    free(archs);
}

static
struct arch *
new_arch(
struct arch **archs,
unsigned long *narchs)
{
    struct arch *arch;

	*archs = reallocate(*archs, (*narchs + 1) * sizeof(struct arch));
	arch = *archs + *narchs;
	*narchs = *narchs + 1;
	memset(arch, '\0', sizeof(struct arch));
	return(arch);
}

static
struct member *
new_member(
struct arch *arch)
{
    struct member *member;

	arch->members = reallocate(arch->members,
				  (arch->nmembers + 1) * sizeof(struct member));
	member = arch->members + arch->nmembers;
	arch->nmembers++;
	memset(member, '\0', sizeof(struct member));
	return(member);
}
