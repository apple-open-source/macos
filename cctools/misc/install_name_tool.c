/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include "stuff/errors.h"
#include "stuff/breakout.h"
#include "stuff/round.h"
#include "stuff/allocate.h"

/* used by error routines as the name of the program */
char *progname = NULL;

static void usage(
    void);

static void process(
    struct arch *archs,
    unsigned long narchs);

static void write_on_input(
    struct arch *archs,
    unsigned long narchs,
    char *input);

static void setup_object_symbolic_info(
    struct object *object);

static void update_load_commands(
    struct arch *arch,
    unsigned long *header_size);

/* the argument to the -id option */
static char *id = NULL;

/* the arguments to the -change options */
struct changes {
    char *old;
    char *new;
};
static struct changes *changes = NULL;
static unsigned long nchanges = 0;

/*
 * This is a pointer to an array of the original header sizes (mach header and
 * load commands) for each architecture which is used when we are writing on the
 * input file.
 */
static unsigned long *arch_header_sizes = NULL;

/*
 * The -o output option is not enabled as it is not needed and has the
 * unintended side effect of changing the time stamps in LC_ID_DYLIB commands
 * which is not the desired functionality of this command.
 */
#undef OUTPUT_OPTION

/*
 * The install_name_tool allow the dynamic shared library install names of a
 * Mach-O binary to be changed.  For this tool to work when the install names
 * are larger the binary should be built with the ld(1)
 * -headerpad_max_install_names option.
 *
 *    Usage: install_name_tool [-change old new] ... [-id name] input
 *
 * The "-change old new" option changes the "old" install name to the "new"
 * install name if found in the binary.
 *
 * The "-id name" option changes the install name in the LC_ID_DYLIB load
 * command for a dynamic shared library.
 */
int
main(
int argc,
char **argv,
char **envp)
{
    int i;
    struct arch *archs;
    unsigned long narchs;
    char *input;
    char *output;

	output = NULL;
	progname = argv[0];
	input = NULL;
	archs = NULL;
	narchs = 0;
	for(i = 1; i < argc; i++){
#ifdef OUTPUT_OPTION
	    if(strcmp(argv[i], "-o") == 0){
		if(i + 1 == argc){
		    error("missing argument to: %s option", argv[i]);
		    usage();
		}
		if(output != NULL){
		    error("more than one: %s option specified", argv[i]);
		    usage();
		}
		output = argv[i+1];
		i++;
	    }
	    else
#endif /* OUTPUT_OPTION */
	    if(strcmp(argv[i], "-id") == 0){
		if(i + 1 == argc){
		    error("missing argument to: %s option", argv[i]);
		    usage();
		}
		if(id != NULL){
		    error("more than one: %s option specified", argv[i]);
		    usage();
		}
		id = argv[i+1];
		i++;
	    }
	    else if(strcmp(argv[i], "-change") == 0){
		if(i + 2 == argc){
		    error("missing argument(s) to: %s option", argv[i]);
		    usage();
		}
		changes = reallocate(changes,
				     sizeof(struct changes) * (nchanges + 1));
		changes[nchanges].old = argv[i+1];
		changes[nchanges].new = argv[i+2];
		nchanges += 1;
		i += 2;
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
	if(input == NULL || (id == NULL && nchanges == 0))
	    usage();

	breakout(input, &archs, &narchs, FALSE);
	if(errors)
	    exit(EXIT_FAILURE);

	checkout(archs, narchs);
	if(errors)
	    exit(EXIT_FAILURE);

	arch_header_sizes = allocate(narchs * sizeof(unsigned long));
	process(archs, narchs);
	if(errors)
	    exit(EXIT_FAILURE);

	if(output != NULL)
	    writeout(archs, narchs, output, 0777, TRUE, FALSE, FALSE, NULL);
	else
	    write_on_input(archs, narchs, input);

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
	fprintf(stderr, "Usage: %s [-change old new] ... [-id name] input"
#ifdef OUTPUT_OPTION
		" [-o output]"
#endif /* OUTPUT_OPTION */
		"\n", progname);
	exit(EXIT_FAILURE);
}

static
void
process(
struct arch *archs,
unsigned long narchs)
{
    unsigned long i;
    struct object *object;

	for(i = 0; i < narchs; i++){
	    if(archs[i].type == OFILE_Mach_O){
		object = archs[i].object;
		if(object->mh->filetype == MH_DYLIB_STUB)
		    fatal("input file: %s is Mach-O dynamic shared library stub"
			  " file and can't be changed", archs[i].file_name);
		setup_object_symbolic_info(object);
		update_load_commands(archs + i, arch_header_sizes + i);
	    }
	    else{
		error("input file: %s is not a Mach-O file",archs[i].file_name);
		return;
	    }
	}
}

/*
 * write_on_input() takes the modified archs and writes the load commands
 * directly into the input file.
 */
static
void
write_on_input(
struct arch *archs,
unsigned long narchs,
char *input)
{
    int fd;
    unsigned long i, offset, size;
    char *headers;
    struct mach_header *mh;
    struct load_command *lc;
    enum byte_sex host_byte_sex;

	host_byte_sex = get_host_byte_sex();

	fd = open(input, O_WRONLY, 0);
	if(fd == -1)
	    system_error("can't open input file: %s for writing", input);

	for(i = 0; i < narchs; i++){
	    if(archs[i].fat_arch != NULL)
		offset = archs[i].fat_arch->offset;
	    else
		offset = 0;
	    if(lseek(fd, offset, SEEK_SET) == -1)
		system_error("can't lseek to offset: %lu in file: %s for "
			     "writing", offset, input);
	    /*
	     * Since the new headers may be smaller than the old headers and
	     * we want to make sure any old unused bytes are zero in the file
	     * we allocate the size of the original headers into a buffer and
	     * zero it out. Then copy the new headers into the buffer and write
	     * out the size of the original headers to the file.
	     */
	    if(arch_header_sizes[i] >
	       sizeof(struct mach_header) +
	       archs[i].object->mh->sizeofcmds)
		size = arch_header_sizes[i];
	    else
		size = sizeof(struct mach_header) +
		       archs[i].object->mh->sizeofcmds;
	    headers = allocate(size);
	    memset(headers, '\0', size);

	    mh = (struct mach_header *)headers;
	    lc = (struct load_command *)(headers + sizeof(struct mach_header));
	    *mh = *(archs[i].object->mh);
	    memcpy(lc, archs[i].object->load_commands, mh->sizeofcmds);
	    if(archs[i].object->object_byte_sex != host_byte_sex)
		if(swap_object_headers(mh, lc) == FALSE)
		    fatal("internal error: swap_object_headers() failed");

	    if(write(fd, headers, size) != (int)size)
		system_error("can't write new headers in file: %s", input);

	    free(headers);
	}
	if(close(fd) == -1)
	    system_error("can't close written on input file: %s", input);
}

static
void
setup_object_symbolic_info(
struct object *object)
{
#ifdef OUTPUT_OPTION
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
#endif /* OUTPUT_OPTION */
}

/*
 * update_load_commands() changes the install names the LC_LOAD_DYLIB,
 * LC_LOAD_WEAK_DYLIB and LC_PREBOUND_DYLIB commands for the specified arch.
 */
static
void
update_load_commands(
struct arch *arch,
unsigned long *header_size)
{
    unsigned long i, j, new_sizeofcmds, low_fileoff, new_size,					  linked_modules_size;
    struct load_command *lc1, *lc2, *new_load_commands;
    struct dylib_command *dl_load1, *dl_load2, *dl_id1, *dl_id2;
    struct prebound_dylib_command *pbdylib1, *pbdylib2;
    char *dylib_name1, *dylib_name2, *arch_name, *linked_modules1,
	 *linked_modules2;
    struct segment_command *sg;
    struct section *s;
    struct arch_flag arch_flag;

	/*
	 * Make a pass through the load commands and figure out what the new
	 * size of the the commands needs to be and how much room there is for
	 * them.
	 */
	new_sizeofcmds = arch->object->mh->sizeofcmds;
	low_fileoff = ULONG_MAX;
	lc1 = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    switch(lc1->cmd){
	    case LC_ID_DYLIB:
		dl_id1 = (struct dylib_command *)lc1;
		dylib_name1 = (char *)dl_id1 + dl_id1->dylib.name.offset;
		if(id != NULL){
		    new_size = sizeof(struct dylib_command) +
			       round(strlen(id) + 1, sizeof(long));
		    new_sizeofcmds += (new_size - dl_id1->cmdsize);
		}
		break;

	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		dl_load1 = (struct dylib_command *)lc1;
		dylib_name1 = (char *)dl_load1 + dl_load1->dylib.name.offset;
		for(j = 0; j < nchanges; j++){
		    if(strcmp(changes[j].old, dylib_name1) == 0){
			new_size = sizeof(struct dylib_command) +
				   round(strlen(changes[j].new) + 1,
					 sizeof(long));
			new_sizeofcmds += (new_size - dl_load1->cmdsize);
			break;
		    }
		}
		break;

	    case LC_PREBOUND_DYLIB:
		pbdylib1 = (struct prebound_dylib_command *)lc1;
		dylib_name1 = (char *)pbdylib1 + pbdylib1->name.offset;
		for(j = 0; j < nchanges; j++){
		    if(strcmp(changes[j].old, dylib_name1) == 0){
			linked_modules_size = pbdylib1->cmdsize - (
				sizeof(struct prebound_dylib_command) +
				round(strlen(dylib_name1) + 1, sizeof(long)));
			new_size = sizeof(struct prebound_dylib_command) +
				   round(strlen(changes[j].new) + 1,
					 sizeof(long)) +
				   linked_modules_size;
			new_sizeofcmds += (new_size - pbdylib1->cmdsize);
			break;
		    }
		}
		break;

	    case LC_SEGMENT:
		sg = (struct segment_command *)lc1;
		s = (struct section *)
		    ((char *)sg + sizeof(struct segment_command));
		if(sg->nsects != 0){
		    for(j = 0; j < sg->nsects; j++){
			if(s->size != 0 &&
			   (s->flags & S_ZEROFILL) != S_ZEROFILL &&
			   s->offset < low_fileoff)
			    low_fileoff = s->offset;
			s++;
		    }
		}
		else{
		    if(sg->filesize != 0 && sg->fileoff < low_fileoff)
			low_fileoff = sg->fileoff;
		}
		break;
	    }
	    lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	}

	if(new_sizeofcmds + sizeof(struct mach_header) > low_fileoff){
	    arch_flag.cputype = arch->object->mh->cputype;
	    arch_flag.cpusubtype = arch->object->mh->cpusubtype;
	    set_arch_flag_name(&arch_flag);
	    arch_name = arch_flag.name;
	    error("changing install names can't be redone for: %s (for "
		  "architecture %s) because larger updated load commands do "
		  "not fit (the program must be relinked)", arch->file_name,
		  arch_name);
	    return;
	}

	/*
	 * Allocate space for the new load commands and zero it out so any holes
	 * will be zero bytes.  Note this may be smaller than the original size
	 * of the load commands.
	 */
	new_load_commands = allocate(new_sizeofcmds);
	memset(new_load_commands, '\0', new_sizeofcmds);

	/*
	 * Fill in the new load commands by copying in the non-modified
	 * commands and updating ones with install name changes.
	 */
	lc1 = arch->object->load_commands;
	lc2 = new_load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    switch(lc1->cmd){
	    case LC_ID_DYLIB:
		if(id != NULL){
		    memcpy(lc2, lc1, sizeof(struct dylib_command));
		    dl_id2 = (struct dylib_command *)lc2;
		    dl_id2->cmdsize = sizeof(struct dylib_command) +
				     round(strlen(id) + 1, sizeof(long));
		    dl_id2->dylib.name.offset = sizeof(struct dylib_command);
		    dylib_name2 = (char *)dl_id2 + dl_id2->dylib.name.offset;
		    strcpy(dylib_name2, id);
		}
		else{
		    memcpy(lc2, lc1, lc1->cmdsize);
		}
		break;

	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		dl_load1 = (struct dylib_command *)lc1;
		dylib_name1 = (char *)dl_load1 + dl_load1->dylib.name.offset;
		for(j = 0; j < nchanges; j++){
		    if(strcmp(changes[j].old, dylib_name1) == 0){
			memcpy(lc2, lc1, sizeof(struct dylib_command));
			dl_load2 = (struct dylib_command *)lc2;
			dl_load2->cmdsize = sizeof(struct dylib_command) +
					    round(strlen(changes[j].new) + 1,
						  sizeof(long));
			dl_load2->dylib.name.offset =
			    sizeof(struct dylib_command);
			dylib_name2 = (char *)dl_load2 +
				      dl_load2->dylib.name.offset;
			strcpy(dylib_name2, changes[j].new);
			break;
		    }
		}
		if(j >= nchanges){
		    memcpy(lc2, lc1, lc1->cmdsize);
		}
		break;

	    case LC_PREBOUND_DYLIB:
		pbdylib1 = (struct prebound_dylib_command *)lc1;
		dylib_name1 = (char *)pbdylib1 + pbdylib1->name.offset;
		for(j = 0; j < nchanges; j++){
		    if(strcmp(changes[j].old, dylib_name1) == 0){
			memcpy(lc2, lc1, sizeof(struct prebound_dylib_command));
			pbdylib2 = (struct prebound_dylib_command *)lc2;
			linked_modules_size = pbdylib1->cmdsize - (
			    sizeof(struct prebound_dylib_command) +
			    round(strlen(dylib_name1) + 1, sizeof(long)));
			pbdylib2->cmdsize =
			    sizeof(struct prebound_dylib_command) +
			    round(strlen(changes[j].new) + 1, sizeof(long)) +
			    linked_modules_size;

			pbdylib2->name.offset =
			    sizeof(struct prebound_dylib_command);
			dylib_name2 = (char *)pbdylib2 +
				      pbdylib2->name.offset;
			strcpy(dylib_name2, changes[j].new);
			
			pbdylib2->linked_modules.offset = 
			    sizeof(struct prebound_dylib_command) +
			    round(strlen(changes[j].new) + 1, sizeof(long));
			linked_modules1 = (char *)pbdylib1 +
					  pbdylib1->linked_modules.offset;
			linked_modules2 = (char *)pbdylib2 +
					  pbdylib2->linked_modules.offset;
			memcpy(linked_modules2, linked_modules1,
			       linked_modules_size);
			break;
		    }
		}
		if(j >= nchanges){
		    memcpy(lc2, lc1, lc1->cmdsize);
		}
		break;

	    default:
		memcpy(lc2, lc1, lc1->cmdsize);
		break;
	    }
	    lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	    lc2 = (struct load_command *)((char *)lc2 + lc2->cmdsize);
	}

	/*
	 * Finally copy the updated load commands over the existing load
	 * commands. Since the headers could be smaller we save away the old
	 * header_size (for use when writing on the input) and also put zero
	 * bytes on the part that is no longer used for headers.
	 */
	*header_size = sizeof(struct mach_header) +
		       arch->object->mh->sizeofcmds;
	if(new_sizeofcmds < arch->object->mh->sizeofcmds){
	    memset(((char *)arch->object->load_commands) + new_sizeofcmds, '\0',
		   arch->object->mh->sizeofcmds - new_sizeofcmds);
	}
	memcpy(arch->object->load_commands, new_load_commands, new_sizeofcmds);
	arch->object->mh->sizeofcmds = new_sizeofcmds;
	free(new_load_commands);

	/* reset the pointers into the load commands */
	lc1 = arch->object->load_commands;
	for(i = 0; i < arch->object->mh->ncmds; i++){
	    switch(lc1->cmd){
	    case LC_SYMTAB:
		arch->object->st = (struct symtab_command *)lc1;
	        break;
	    case LC_DYSYMTAB:
		arch->object->dyst = (struct dysymtab_command *)lc1;
		break;
	    case LC_TWOLEVEL_HINTS:
		arch->object->hints_cmd = (struct twolevel_hints_command *)lc1;
		break;
	    case LC_PREBIND_CKSUM:
		arch->object->cs = (struct prebind_cksum_command *)lc1;
		break;
	    case LC_SEGMENT:
		sg = (struct segment_command *)lc1;
		if(strcmp(sg->segname, SEG_LINKEDIT) == 0)
		    arch->object->seg_linkedit = sg;
	    }
	    lc1 = (struct load_command *)((char *)lc1 + lc1->cmdsize);
	}
}
