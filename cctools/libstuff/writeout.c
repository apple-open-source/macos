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
#include <mach/mach.h>
#include "stuff/openstep_mach.h"
#include <libc.h>
#ifndef __OPENSTEP__
#include <utime.h>
#endif
#include "stuff/ofile.h"
#include "stuff/breakout.h"
#include "stuff/allocate.h"
#include "stuff/round.h"
#include "stuff/errors.h"

static void copy_new_symbol_info(
    char *p,
    unsigned long *size,
    struct dysymtab_command *dyst,
    struct dysymtab_command *old_dyst,
    struct twolevel_hints_command *hints_cmd,
    struct twolevel_hints_command *old_hints_cmd,
    struct object *object);

static void make_table_of_contents(
    struct arch *archs,
    char *output,
    long toc_time,
    enum bool sort_toc,
    enum bool commons_in_toc,
    enum bool library_warnings);

static enum bool toc_symbol(
    struct nlist *symbol,
    enum bool commons_in_toc,
    struct section **sections);

static int ranlib_name_qsort(
    const struct ranlib *ran1,
    const struct ranlib *ran2);

static int ranlib_offset_qsort(
    const struct ranlib *ran1,
    const struct ranlib *ran2);

static enum bool check_sort_ranlibs(
    struct arch *arch,
    char *output,
    enum bool library_warnings);

static void warn_member(
    struct arch *arch,
    struct member *member,
    const char *format, ...) __attribute__ ((format (printf, 3, 4)));

/*
 * writeout() creates an ofile from the data structure pointed to by
 * archs (of narchs size) into the specified output file (output).  The file is
 * created with the mode, mode.  If there are libraries in the data structures
 * a new table of contents is created and is sorted if sort_toc is TRUE and
 * commons symbols are included in the table of contents if commons_in_toc is
 * TRUE.  The normal use will have sort_toc == TRUE and commons_in_toc == FALSE.
 * If warnings about unusual libraries are printed if library_warnings == TRUE.
 */
__private_extern__
void
writeout(
struct arch *archs,
unsigned long narchs,
char *output,
unsigned short mode,
enum bool sort_toc,
enum bool commons_in_toc,
enum bool library_warnings)
{
    unsigned long i, j, k, l, file_size, offset, pad, size;
    enum byte_sex target_byte_sex, host_byte_sex;
    char *file, *p;
    kern_return_t r;
    struct fat_header *fat_header;
    struct fat_arch *fat_arch;
    int fd;
#ifndef __OPENSTEP__
    struct utimbuf timep;
#else
    time_t timep[2];
#endif
    struct dysymtab_command dyst;
    struct twolevel_hints_command hints_cmd;
    unsigned long mh_flags;
    struct load_command *lc;
    struct dylib_command *dl;

    /*
     * The time the table of contents' are set to and the time to base the
     * modification time of the output file to be set to.
     */
    long toc_time;

	toc_time = time(0);

	fat_arch = NULL; /* here to quite compiler maybe warning message */
	fat_header = NULL;

	if(narchs == 0){
	    error("no contents for file: %s (not created)", output);
	    return;
	}

	host_byte_sex = get_host_byte_sex();

	/*
	 * Calculate the total size of the file and the final size of each
	 * architecture.
	 */
	if(narchs > 1 || archs[0].fat_arch != NULL)
	    file_size = sizeof(struct fat_header) +
			       sizeof(struct fat_arch) * narchs;
	else
	    file_size = 0;
	for(i = 0; i < narchs; i++){
	    /*
	     * For each arch that is an archive recreate the table of contents.
	     */
	    if(archs[i].type == OFILE_ARCHIVE){
		make_table_of_contents(archs + i, output, toc_time, sort_toc,
				       commons_in_toc, library_warnings);
		archs[i].library_size += SARMAG + archs[i].toc_size;
		if(archs[i].fat_arch != NULL)
		    file_size = round(file_size, 1 << archs[i].fat_arch->align);
		file_size += archs[i].library_size;
		if(archs[i].fat_arch != NULL)
		    archs[i].fat_arch->size = archs[i].library_size;
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		size = archs[i].object->object_size
		       - archs[i].object->input_sym_info_size
		       + archs[i].object->output_sym_info_size;
		if(archs[i].fat_arch != NULL)
		    file_size = round(file_size, 1 << archs[i].fat_arch->align);
		file_size += size;
		if(archs[i].fat_arch != NULL)
		    archs[i].fat_arch->size = size;
	    }
	    else{ /* archs[i].type == OFILE_UNKNOWN */
		if(archs[i].fat_arch != NULL)
		    file_size = round(file_size, 1 << archs[i].fat_arch->align);
		file_size += archs[i].unknown_size;
		if(archs[i].fat_arch != NULL)
		    archs[i].fat_arch->size = archs[i].unknown_size;
	    }
	}

	/*
	 * This buffer is vm_allocate'ed to make sure all holes are filled with
	 * zero bytes.
	 */
	if((r = vm_allocate(mach_task_self(), (vm_address_t *)&file,
			    file_size, TRUE)) != KERN_SUCCESS)
	    mach_fatal(r, "can't vm_allocate() buffer for output file: %s of "
		       "size %lu", output, file_size);

	/*
	 * If there is more than one architecture then fill in the fat file
	 * header and the fat_arch structures in the buffer.
	 */
	if(narchs > 1 || archs[0].fat_arch != NULL){
	    fat_header = (struct fat_header *)file;
	    fat_header->magic = FAT_MAGIC;
	    fat_header->nfat_arch = narchs;
	    offset = sizeof(struct fat_header) +
			    sizeof(struct fat_arch) * narchs;
	    fat_arch = (struct fat_arch *)(file + sizeof(struct fat_header));
	    for(i = 0; i < narchs; i++){
		fat_arch[i].cputype = archs[i].fat_arch->cputype;
		fat_arch[i].cpusubtype = archs[i].fat_arch->cpusubtype;
		offset = round(offset, 1 << archs[i].fat_arch->align);
		fat_arch[i].offset = offset;
		fat_arch[i].size = archs[i].fat_arch->size;
		fat_arch[i].align = archs[i].fat_arch->align;
		offset += archs[i].fat_arch->size;
	    }
	}

	/*
	 * Now put each arch in the buffer.
	 */
	for(i = 0; i < narchs; i++){
	    if(archs[i].fat_arch != NULL)
		p = file + fat_arch[i].offset;
	    else
		p = file;

	    if(archs[i].type == OFILE_ARCHIVE){
		/*
		 * If the input files only contains non-object files then the
		 * byte sex of the output can't be determined which is needed
		 * for the two binary long's of the table of contents.  But
		 * since these will be zero (the same in both byte sexes)
		 * because there are no symbols in the table of contents if
		 * there are no object files.
		 */

		/* put in the archive magic string */
		memcpy(p, ARMAG, SARMAG);
		p += SARMAG;

		/*
		 * Warn for what really is a bad library that has an empty
		 * table of contents but this is allowed in the original
		 * bsd4.3 ranlib(1) implementation.
		 */
		if(library_warnings == TRUE && archs[i].toc_nranlibs == 0){
		    if(narchs > 1 || archs[i].fat_arch != NULL)
			warning("warning library: %s for architecture: %s the "
			        "table of contents is empty (no object file "
			        "members in the library)", output,
			         archs[i].fat_arch_name);
		    else
			warning("warning for library: %s the table of contents "
				"is empty (no object file members in the "
				"library)", output);
		}

		/*
		 * Pick the byte sex to write the table of contents in.
		 */
		target_byte_sex = UNKNOWN_BYTE_SEX;
		for(j = 0;
		    j < archs[i].nmembers && target_byte_sex == UNKNOWN_BYTE_SEX;
		    j++){
		    if(archs[i].members[j].type == OFILE_Mach_O)
			target_byte_sex =
				archs[i].members[j].object->object_byte_sex;
		}
		if(target_byte_sex == UNKNOWN_BYTE_SEX)
		    target_byte_sex = host_byte_sex;

		/*
		 * Put in the table of contents member:
		 *	the archive header
		 *	a long for the number of bytes of the ranlib structs
		 *	the ranlib structs
		 *	a long for the number of bytes of the ranlib strings
		 *	the strings for the ranlib structs
		 */
		memcpy(p, (char *)(&archs[i].toc_ar_hdr),sizeof(struct ar_hdr));
		p += sizeof(struct ar_hdr);

		if(archs[i].toc_long_name == TRUE){
		    memcpy(p, archs[i].toc_name, archs[i].toc_name_size);
		    p += round(archs[i].toc_name_size, sizeof(long));
		}

		l = archs[i].toc_nranlibs * sizeof(struct ranlib);
		if(target_byte_sex != host_byte_sex)
		    l = SWAP_LONG(l);
		memcpy(p, (char *)&l, sizeof(long));
		p += sizeof(long);

		if(target_byte_sex != host_byte_sex)
		    swap_ranlib(archs[i].toc_ranlibs, archs[i].toc_nranlibs,
				target_byte_sex);
		memcpy(p, (char *)archs[i].toc_ranlibs,
		       archs[i].toc_nranlibs * sizeof(struct ranlib));
		p += archs[i].toc_nranlibs * sizeof(struct ranlib);

		l = archs[i].toc_strsize;
		if(target_byte_sex != host_byte_sex)
		    l = SWAP_LONG(l);
		memcpy(p, (char *)&l, sizeof(long));
		p += sizeof(long);

		memcpy(p, (char *)archs[i].toc_strings, archs[i].toc_strsize);
		p += archs[i].toc_strsize;

		/*
		 * Put in the archive header and member contents for each
		 * member in the buffer.
		 */
		for(j = 0; j < archs[i].nmembers; j++){
		    memcpy(p, (char *)(archs[i].members[j].ar_hdr),
			   sizeof(struct ar_hdr));
		    p += sizeof(struct ar_hdr);

		    if(archs[i].members[j].member_long_name == TRUE){
			memcpy(p, archs[i].members[j].member_name,
			       archs[i].members[j].member_name_size);
			p += round(archs[i].members[j].member_name_size,
				   sizeof(long));
		    }

		    if(archs[i].members[j].type == OFILE_Mach_O){
			/*
			 * ofile_map swaps the headers to the host_byte_sex if
			 * the object's byte sex is not the same as the host
			 * byte sex so if this is the case swap them back
			 * before writing them out.
			 */
			memset(&dyst, '\0', sizeof(struct dysymtab_command));
			if(archs[i].members[j].object->dyst != NULL)
			    dyst = *(archs[i].members[j].object->dyst);
			if(archs[i].members[j].object->hints_cmd != NULL)
			   hints_cmd = *(archs[i].members[j].object->hints_cmd);
			mh_flags = archs[i].members[j].object->mh->flags;
			if(archs[i].members[j].object->object_byte_sex !=
								host_byte_sex){
			    if(swap_object_headers(
				archs[i].members[j].object->mh,
			        archs[i].members[j].object->load_commands) ==
									FALSE)
				    fatal("internal error: swap_object_headers"
					  "() failed");
			    if(archs[i].members[j].object->output_nsymbols != 0)
				swap_nlist(
				   archs[i].members[j].object->output_symbols,
				   archs[i].members[j].object->output_nsymbols,
				   archs[i].members[j].object->object_byte_sex);
			}
			if(archs[i].members[j].object->
				output_sym_info_size == 0 &&
			   archs[i].members[j].object->
				input_sym_info_size == 0){
			    size = archs[i].members[j].object->object_size;
			    memcpy(p, archs[i].members[j].object->object_addr,
				   size);
			}
			else{
			    size = archs[i].members[j].object->object_size
				   - archs[i].members[j].object->
							input_sym_info_size;
			    memcpy(p, archs[i].members[j].object->object_addr,
				   size);
			    copy_new_symbol_info(p, &size, &dyst,
				archs[i].members[j].object->dyst, &hints_cmd,
				archs[i].members[j].object->hints_cmd,
				archs[i].members[j].object);
			}
			p += size;
			pad = round(size, sizeof(long)) - size;
		    }
		    else{
			memcpy(p, archs[i].members[j].unknown_addr, 
			       archs[i].members[j].unknown_size);
			p += archs[i].members[j].unknown_size;
			pad = round(archs[i].members[j].unknown_size,
								sizeof(long)) -
				    archs[i].members[j].unknown_size;
		    }
		    /* as with the UNIX ar(1) program pad with '\n' chars */
		    for(k = 0; k < pad; k++)
			*p++ = '\n';
		}
	    }
	    else if(archs[i].type == OFILE_Mach_O){
		memset(&dyst, '\0', sizeof(struct dysymtab_command));
		if(archs[i].object->dyst != NULL)
		    dyst = *(archs[i].object->dyst);
		if(archs[i].object->hints_cmd != NULL)
		    hints_cmd = *(archs[i].object->hints_cmd);
		mh_flags = archs[i].object->mh->flags;
		if(archs[i].object->mh->filetype == MH_DYLIB){
		    lc = archs[i].object->load_commands;
		    for(j = 0; j < archs[i].object->mh->ncmds; j++){
			if(lc->cmd == LC_ID_DYLIB){
			    dl = (struct dylib_command *)lc;
			    dl->dylib.timestamp = toc_time;
			    break;
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		}
		if(archs[i].object->object_byte_sex != host_byte_sex){
		    if(swap_object_headers(archs[i].object->mh,
			       archs[i].object->load_commands) == FALSE)
			fatal("internal error: swap_object_headers() failed");
		    if(archs[i].object->output_nsymbols != 0)
			swap_nlist(archs[i].object->output_symbols,
				   archs[i].object->output_nsymbols,
				   archs[i].object->object_byte_sex);
		}
		if(archs[i].object->output_sym_info_size == 0 &&
		   archs[i].object->input_sym_info_size == 0){
		    size = archs[i].object->object_size;
		    memcpy(p, archs[i].object->object_addr, size);
		}
		else{
		    size = archs[i].object->object_size
			   - archs[i].object->input_sym_info_size;
		    memcpy(p, archs[i].object->object_addr, size);
		    copy_new_symbol_info(p, &size, &dyst,
				archs[i].object->dyst, &hints_cmd,
				archs[i].object->hints_cmd,
				archs[i].object);
		}
	    }
	    else{ /* archs[i].type == OFILE_UNKNOWN */
		memcpy(p, archs[i].unknown_addr, archs[i].unknown_size);
	    }
	}
#ifdef __LITTLE_ENDIAN__
	if(narchs > 1 || archs[0].fat_arch != NULL){
	    swap_fat_header(fat_header, BIG_ENDIAN_BYTE_SEX);
	    swap_fat_arch(fat_arch, narchs, BIG_ENDIAN_BYTE_SEX);
	}
#endif /* __LITTLE_ENDIAN__ */

	/*
	 * Create the output file.  The unlink() is done to handle the problem
	 * when the outputfile is not writable but the directory allows the
	 * file to be removed (since the file may not be there the return code
	 * of the unlink() is ignored).
	 */
	(void)unlink(output);
	if((fd = open(output, O_WRONLY | O_CREAT | O_TRUNC, mode)) == -1){
	    system_error("can't create output file: %s", output);
	    goto cleanup;
	}
	if(write(fd, file, file_size) != file_size){
	    system_error("can't write output file: %s", output);
	    goto cleanup;
	}
	if(close(fd) == -1){
	    system_fatal("can't close output file: %s", output);
	    goto cleanup;
	}
#ifndef __OPENSTEP__
	timep.actime = toc_time - 5;
	timep.modtime = toc_time - 5;
	if(utime(output, &timep) == -1)
#else
	timep[0] = toc_time - 5;
	timep[1] = toc_time - 5;
	if(utime(output, timep) == -1)
#endif
	{
	    system_fatal("can't set the modifiy times in output file: %s",
			 output);
	    goto cleanup;
	}
cleanup:
	if((r = vm_deallocate(mach_task_self(), (vm_address_t)file,
			      file_size)) != KERN_SUCCESS){
	    my_mach_error(r, "can't vm_deallocate() buffer for output file");
	    return;
	}
}

/*
 * copy_new_symbol_info() copies the new and updated symbolic information into
 * the buffer for the object.
 */
static
void
copy_new_symbol_info(
char *p,
unsigned long *size,
struct dysymtab_command *dyst,
struct dysymtab_command *old_dyst,
struct twolevel_hints_command *hints_cmd,
struct twolevel_hints_command *old_hints_cmd,
struct object *object)
{
	if(old_dyst != NULL){
	    memcpy(p + *size, object->output_loc_relocs,
		   dyst->nlocrel * sizeof(struct relocation_info));
	    *size += dyst->nlocrel *
		     sizeof(struct relocation_info);
	    memcpy(p + *size, object->output_symbols,
		   object->output_nsymbols * sizeof(struct nlist));
	    *size += object->output_nsymbols *
		     sizeof(struct nlist);
	    if(old_hints_cmd != NULL){
		memcpy(p + *size, object->output_hints,
		       hints_cmd->nhints * sizeof(struct twolevel_hint));
		*size += hints_cmd->nhints *
			 sizeof(struct twolevel_hint);
	    }
	    memcpy(p + *size, object->output_ext_relocs,
		   dyst->nextrel * sizeof(struct relocation_info));
	    *size += dyst->nextrel *
		     sizeof(struct relocation_info);
	    memcpy(p + *size, object->output_indirect_symtab,
		   dyst->nindirectsyms * sizeof(unsigned long));
	    *size += dyst->nindirectsyms *
		     sizeof(unsigned long);
	    memcpy(p + *size, object->output_tocs,
		   object->output_ntoc *sizeof(struct dylib_table_of_contents));
	    *size += object->output_ntoc *
		     sizeof(struct dylib_table_of_contents);
	    memcpy(p + *size, object->output_mods,
		   object->output_nmodtab * sizeof(struct dylib_module));
	    *size += object->output_nmodtab *
		     sizeof(struct dylib_module);
	    memcpy(p + *size, object->output_refs,
		   object->output_nextrefsyms * sizeof(struct dylib_reference));
	    *size += object->output_nextrefsyms *
		     sizeof(struct dylib_reference);
	    memcpy(p + *size, object->output_strings,
		   object->output_strings_size);
	    *size += object->output_strings_size;
	}
	else{
	    memcpy(p + *size, object->output_symbols,
		   object->output_nsymbols * sizeof(struct nlist));
	    *size += object->output_nsymbols *
		     sizeof(struct nlist);
	    memcpy(p + *size, object->output_strings,
		   object->output_strings_size);
	    *size += object->output_strings_size;
	}
}

/*
 * make_table_of_contents() make the table of contents for the specified arch
 * and fills in the toc_* fields in the arch.  Output is the name of the output
 * file for error messages.
 */
static
void
make_table_of_contents(
struct arch *arch,
char *output,
long toc_time,
enum bool sort_toc,
enum bool commons_in_toc,
enum bool library_warnings)
{
    unsigned long i, j, k, r, s, nsects;
    struct member *member;
    struct object *object;
    struct load_command *lc;
    struct segment_command *sg;
    struct nlist *symbols;
    unsigned long nsymbols;
    char *strings;
    unsigned long strings_size;
    enum bool sorted;
    unsigned short toc_mode;
    int oumask, numask;
    char *ar_name;
    struct section *section;

	symbols = NULL; /* here to quite compiler maybe warning message */
	strings = NULL; /* here to quite compiler maybe warning message */

	/*
	 * First pass over the members to count how many ranlib structs are
	 * needed and the size of the strings in the toc that are needed.
	 */
	for(i = 0; i < arch->nmembers; i++){
	    member = arch->members + i;
	    if(member->type == OFILE_Mach_O){
		object = member->object;
		nsymbols = 0;
		nsects = 0;
		lc = object->load_commands;
		for(j = 0; j < object->mh->ncmds; j++){
		    if(lc->cmd == LC_SEGMENT){
			sg = (struct segment_command *)lc;
			nsects += sg->nsects;
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
		object->sections = allocate(nsects *
					    sizeof(struct section *));
		nsects = 0;
		lc = object->load_commands;
		for(j = 0; j < object->mh->ncmds; j++){
		    if(lc->cmd == LC_SEGMENT){
			sg = (struct segment_command *)lc;
			section = (struct section *)
			    ((char *)sg + sizeof(struct segment_command));
			for(k = 0; k < sg->nsects; k++){
			    object->sections[nsects++] = section++;
			}
		    }
		    lc = (struct load_command *)((char *)lc + lc->cmdsize);
		}
		if(object->output_sym_info_size == 0){
		    lc = object->load_commands;
		    for(j = 0; j < object->mh->ncmds; j++){
			if(lc->cmd == LC_SYMTAB){
			    object->st = (struct symtab_command *)lc;
			    break;
			}
			lc = (struct load_command *)((char *)lc + lc->cmdsize);
		    }
		    if(object->st != NULL && object->st->nsyms != 0){
			symbols = (struct nlist *)(object->object_addr +
						   object->st->symoff);
			if(object->object_byte_sex != get_host_byte_sex())
			    swap_nlist(symbols, object->st->nsyms,
				       get_host_byte_sex());
			nsymbols = object->st->nsyms;
			strings = object->object_addr + object->st->stroff;
			strings_size = object->st->strsize;
		    }
		}
		else /* object->output_sym_info_size != 0 */ {
		    symbols = object->output_symbols;
		    nsymbols = object->output_nsymbols;
		    strings = object->output_strings;
		    strings_size = object->output_strings_size;
		}
		for(j = 0; j < nsymbols; j++){
		    if(toc_symbol(symbols + j, commons_in_toc,
		       object->sections) == TRUE){
			arch->toc_nranlibs++;
			arch->toc_strsize += strlen(strings +
						    symbols[j].n_un.n_strx) + 1;
		    }
		}
	    }
	}

	/*
	 * Allocate the space for the ranlib structs and strings for the
	 * table of contents.
	 */
	arch->toc_ranlibs = allocate(sizeof(struct ranlib) *arch->toc_nranlibs);
	arch->toc_strsize = round(arch->toc_strsize, sizeof(long));
	arch->toc_strings = allocate(arch->toc_strsize);

	/*
	 * Second pass over the members to fill in the ranlib structs and
	 * the strings for the table of contents.  The ran_name field is
	 * filled in with a pointer to a string contained in arch->toc_strings
	 * for easy sorting and conversion to an index.  The ran_off field is
	 * filled in with the member index plus one to allow marking with it's
	 * negative value by check_sort_ranlibs() and easy conversion to the
	 * real offset.
	 */
	r = 0;
	s = 0;
	for(i = 0; i < arch->nmembers; i++){
	    member = arch->members + i;
	    if(member->type == OFILE_Mach_O){
		object = member->object;
		nsymbols = 0;
		if(object->output_sym_info_size == 0){
		    if(object->st != NULL){
			symbols = (struct nlist *)(object->object_addr +
						   object->st->symoff);
			nsymbols = object->st->nsyms;
			strings = object->object_addr + object->st->stroff;
			strings_size = object->st->strsize;
		    }
		    else{
			symbols = NULL;
			nsymbols = 0;
			strings = NULL;
			strings_size = 0;
		    }
		}
		else{
		    symbols = object->output_symbols;
		    nsymbols = object->output_nsymbols;
		    strings = object->output_strings;
		    strings_size = object->output_strings_size;
		}
		for(j = 0; j < nsymbols; j++){
		    if(symbols[j].n_un.n_strx > strings_size)
			continue;
		    if(toc_symbol(symbols + j, commons_in_toc,
		       object->sections) == TRUE){
			strcpy(arch->toc_strings + s, 
			       strings + symbols[j].n_un.n_strx);
			arch->toc_ranlibs[r].ran_un.ran_name =
						    arch->toc_strings + s;
			arch->toc_ranlibs[r].ran_off = i + 1;
			r++;
			s += strlen(strings + symbols[j].n_un.n_strx) + 1;
		    }
		}
		if(object->output_sym_info_size == 0){
		    if(object->object_byte_sex != get_host_byte_sex())
			swap_nlist(symbols, nsymbols, object->object_byte_sex);
		}
	    }
	}

	/*
	 * If the table of contents is to be sorted by symbol name then try to
	 * sort it and leave it sorted if no duplicates.
	 */
	if(sort_toc == TRUE){
	    qsort(arch->toc_ranlibs, arch->toc_nranlibs, sizeof(struct ranlib),
		  (int (*)(const void *, const void *))ranlib_name_qsort);
	    sorted = check_sort_ranlibs(arch, output, library_warnings);
	    if(sorted == FALSE){
		qsort(arch->toc_ranlibs, arch->toc_nranlibs,
		      sizeof(struct ranlib),
		      (int (*)(const void *, const void *))ranlib_offset_qsort);
		arch->toc_long_name = FALSE;
	    }
	}
	else{
	    sorted = FALSE;
	    arch->toc_long_name = FALSE;
	}

	/*
	 * Now set the ran_off and ran_un.ran_strx fields of the ranlib structs.
	 * To do this the size of the toc member must be know because it comes
	 * first in the library.  The size of the toc member is made up of the
	 * sizeof an archive header struct, plus the sizeof the name if we are
	 * using extended format #1 for the long name, then the toc which is
	 * (as defined in ranlib.h):
	 *	a long for the number of bytes of the ranlib structs
	 *	the ranlib structures
	 *	a long for the number of bytes of the strings
	 *	the strings
	 */
	/*
	 * We use a long name for the table of contents only for the sorted
	 * case.  Which the name is SYMDEF_SORTED is "__.SYMDEF SORTED".
	 * This code assumes SYMDEF_SORTED is 16 characters.
	 */
	if(arch->toc_long_name == TRUE){
	    ar_name = AR_EFMT1 "16";
	    arch->toc_name_size = 16;
	    arch->toc_name = SYMDEF_SORTED;
	}
	else{
	    if(sorted == TRUE){
		ar_name = SYMDEF_SORTED;
		arch->toc_name_size = sizeof(SYMDEF_SORTED) - 1;
		arch->toc_name = ar_name;
	    }
	    else{
		ar_name = SYMDEF;
		arch->toc_name_size = sizeof(SYMDEF) - 1;
		arch->toc_name = ar_name;
	    }
	}
	arch->toc_size = sizeof(struct ar_hdr) +
			 sizeof(long) +
			 arch->toc_nranlibs * sizeof(struct ranlib) +
			 sizeof(long) +
			 arch->toc_strsize;
	if(arch->toc_long_name == TRUE)
	    arch->toc_size += round(arch->toc_name_size, sizeof(long));
	for(i = 0; i < arch->nmembers; i++)
	    arch->members[i].offset += SARMAG + arch->toc_size;
	for(i = 0; i < arch->toc_nranlibs; i++){
	    arch->toc_ranlibs[i].ran_un.ran_strx = 
		arch->toc_ranlibs[i].ran_un.ran_name - arch->toc_strings;
	    arch->toc_ranlibs[i].ran_off = 
		arch->members[arch->toc_ranlibs[i].ran_off - 1].offset;
	}

	numask = 0;
	oumask = umask(numask);
	toc_mode = S_IFREG | (0666 & ~oumask);
	(void)umask(oumask);

	sprintf((char *)(&arch->toc_ar_hdr), "%-*s%-*ld%-*u%-*u%-*o%-*ld",
	   (int)sizeof(arch->toc_ar_hdr.ar_name),
	       ar_name,
	   (int)sizeof(arch->toc_ar_hdr.ar_date),
	       toc_time,
	   (int)sizeof(arch->toc_ar_hdr.ar_uid),
	       (unsigned short)getuid(),
	   (int)sizeof(arch->toc_ar_hdr.ar_gid),
	       (unsigned short)getgid(),
	   (int)sizeof(arch->toc_ar_hdr.ar_mode),
	       (unsigned int)toc_mode,
	   (int)sizeof(arch->toc_ar_hdr.ar_size),
	       (long)(arch->toc_size - sizeof(struct ar_hdr)));
	/*
	 * This has to be done by hand because sprintf puts a null
	 * at the end of the buffer.
	 */
	memcpy(arch->toc_ar_hdr.ar_fmag, ARFMAG,
	       (int)sizeof(arch->toc_ar_hdr.ar_fmag));
}

/*
 * toc_symbol() returns TRUE if the symbol is to be included in the table of
 * contents otherwise it returns FALSE.
 */
static
enum bool
toc_symbol(
struct nlist *symbol,
enum bool commons_in_toc,
struct section **sections)
{
	/* if the name is NULL then it won't be in the table of contents */
	if(symbol->n_un.n_strx == 0)
	    return(FALSE);
	/* if symbol is not external then it won't be in the toc */
	if((symbol->n_type & N_EXT) == 0)
	    return(FALSE);
	/* if symbol is undefined then it won't be in the toc */
	if((symbol->n_type & N_TYPE) == N_UNDF && symbol->n_value == 0)
	    return(FALSE);
	/* if symbol is common and the commons are not to be in the toc */
	if((symbol->n_type & N_TYPE) == N_UNDF && symbol->n_value != 0 &&
	   commons_in_toc == FALSE)
	    return(FALSE);
	/* if the symbol is in a section marked NO_TOC then ... */
	if((symbol->n_type & N_TYPE) == N_SECT &&
	   (sections[symbol->n_sect - 1]->flags & S_ATTR_NO_TOC) != 0)
	    return(FALSE);

	return(TRUE);
}

/*
 * Function for qsort() for comparing ranlib structures by name.
 */
static
int
ranlib_name_qsort(
const struct ranlib *ran1,
const struct ranlib *ran2)
{
	return(strcmp(ran1->ran_un.ran_name, ran2->ran_un.ran_name));
}

/*
 * Function for qsort() for comparing ranlib structures by offset.
 */
static
int
ranlib_offset_qsort(
const struct ranlib *ran1,
const struct ranlib *ran2)
{
	if(ran1->ran_off < ran2->ran_off)
	    return(-1);
	if(ran1->ran_off > ran2->ran_off)
	    return(1);
	/* ran1->ran_off == ran2->ran_off */
	    return(0);
}

/*
 * check_sort_ranlibs() checks the table of contents for the specified arch
 * which is sorted by name for more then one object defining the same symbol.
 * It this is the case it prints each symbol that is defined in more than one
 * object along with the object it is defined in.  It returns TRUE if there are
 * no multiple definitions and FALSE otherwise.
 */
static
enum bool
check_sort_ranlibs(
struct arch *arch,
char *output,
enum bool library_warnings)
{
    long i;
    enum bool multiple_defs;
    struct member *member;

	/*
	 * Since the symbol table is sorted by name look to any two adjcent
	 * entries with the same name.  If such entries are found print them
	 * only once (marked by changing the sign of their ran_off).
	 */
	multiple_defs = FALSE;
	for(i = 0; i < (long)arch->toc_nranlibs - 1; i++){
	    if(strcmp(arch->toc_ranlibs[i].ran_un.ran_name,
		      arch->toc_ranlibs[i+1].ran_un.ran_name) == 0){
		if(multiple_defs == FALSE){
		    if(library_warnings == FALSE)
			return(FALSE);
		    fprintf(stderr, "%s: same symbol defined in more than one "
			    "member ", progname);
		    if(arch->fat_arch != NULL)
			fprintf(stderr, "for architecture: %s ",
				arch->fat_arch_name);
		    fprintf(stderr, "in: %s (table of contents will not be "
			    "sorted)\n", output);
		    multiple_defs = TRUE;
		}
		if(arch->toc_ranlibs[i].ran_off > 0){
		    member = arch->members + arch->toc_ranlibs[i].ran_off - 1;
		    warn_member(arch, member, "defines symbol: %s",
				arch->toc_ranlibs[i].ran_un.ran_name);
		    arch->toc_ranlibs[i].ran_off =
				-(arch->toc_ranlibs[i].ran_off);
		}
		if(arch->toc_ranlibs[i+1].ran_off > 0){
		    member = arch->members + arch->toc_ranlibs[i+1].ran_off - 1;
		    warn_member(arch, member, "defines symbol: %s",
				arch->toc_ranlibs[i+1].ran_un.ran_name);
		    arch->toc_ranlibs[i+1].ran_off =
				-(arch->toc_ranlibs[i+1].ran_off);
		}
	    }
	}

	if(multiple_defs == FALSE)
	    return(TRUE);
	else{
	    for(i = 0; i < arch->toc_nranlibs; i++)
		if((int)arch->toc_ranlibs[i].ran_off < 0)
		    arch->toc_ranlibs[i].ran_off =
			-(arch->toc_ranlibs[i].ran_off);
	    return(FALSE);
	}
}

/*
 * warn_member() is like the error routines it prints the program name the
 * member name specified and message specified.
 */
static
void
warn_member(
struct arch *arch,
struct member *member,
const char *format, ...)
{
    va_list ap;

	fprintf(stderr, "%s: ", progname);
	if(arch->fat_arch != NULL)
	    fprintf(stderr, "for architecture: %s ", arch->fat_arch_name);

	if(member->input_ar_hdr != NULL){
	    fprintf(stderr, "file: %s(%.*s) ", member->input_file_name,
		    (int)member->member_name_size, member->member_name);
	}
	else
	    fprintf(stderr, "file: %s ", member->input_file_name);

	va_start(ap, format);
	vfprintf(stderr, format, ap);
        fprintf(stderr, "\n");
	va_end(ap);
}
