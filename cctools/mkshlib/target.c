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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <libc.h>
#include <sys/file.h>
#include <mach/mach.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/reloc.h>
#include "stuff/arch.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"

#include "libgcc.h"
#include "mkshlib.h"

extern char *mktemp();

/*
 * These are the names of the temporary files used for the branch table
 * assembly language source and resulting object.
 */
char *branch_source_filename;
char *branch_object_filename;
/*
 * This is the name of the temporary file for the library idenification
 * object file.
 */
char *lib_id_object_filename;

/*
 * The names of the the assembly branch and trap instructions we are using for
 * the output file's architecture.
 */
static char *branch = NULL;
static char *trap = NULL;
static unsigned long branch_slot_size = 0;

static void build_branch_object(void);
static void build_lib_id_object(void);

/*
 * Build the target shared library.
 */
void
target(void)
{
    int fd;
    unsigned long magic;
    struct mach_header mh;
    char text_addr_string[10], data_addr_string[10];
    struct alias *ap;
    struct oddball *obp;
    unsigned long i;
    const struct arch_flag *family_arch_flag;

	/*
	 * Determine the cputype and cupsubtype of the output file use the
	 * specified -arch is one is specified.  If not and if no object files
	 * use the host architecture.  If an -arch flag was not specified then
	 * get the cputype and subtype from the first object file.  If the
	 * first object file is a fat file then -arch must be specified.
	 */
	if(arch_flag.name == NULL && nobject_list == 0)
	    if(get_arch_from_host(&arch_flag, NULL) == 0)
		fatal("can't determine the host architecture (specify an "
		      "-arch flag or fix get_arch_from_host() )");
	if(arch_flag.name == NULL){
	    if((fd = open(object_list[0]->name, O_RDONLY, 0)) == -1)
		system_fatal("can't open object file: %s",
			     object_list[0]->name);
	    if(read(fd, &magic, sizeof(unsigned long)) !=
		    sizeof(unsigned long))
		system_fatal("can't read magic number of object file: %s",
			     object_list[0]->name);

#ifdef __BIG_ENDIAN__
	    if(magic == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	    if(magic == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	    {
		fatal("-arch must be specified with use of fat files");
	    }
	    else if(magic == MH_MAGIC || magic == SWAP_LONG(MH_MAGIC)){
		lseek(fd, 0, L_SET);
		if(read(fd, &mh, sizeof(struct mach_header)) !=
		   sizeof(struct mach_header))
		    system_fatal("can't read mach_header of object file: "
				 "%s", object_list[0]->name);
		if(magic == SWAP_LONG(MH_MAGIC))
		    swap_mach_header(&mh, host_byte_sex);
		arch_flag.cputype = mh.cputype;
		arch_flag.cpusubtype = mh.cpusubtype;
	    }
	    else
		fatal("first file listed in #object directive: %s is not "
		      "an object file (bad magic number)",
		      object_list[0]->name);
	    close(fd);
	}

	if(arch_flag.cputype == CPU_TYPE_MC680x0){
	    branch = "jmp";
	    trap = "trapt.l\t#0xfeadface"; 
	    target_byte_sex = BIG_ENDIAN_BYTE_SEX;
	    branch_slot_size = 6;
	}
	else if(arch_flag.cputype == CPU_TYPE_POWERPC){
	    branch = "b";
	    trap = ".long 0";
	    target_byte_sex = BIG_ENDIAN_BYTE_SEX;
	    branch_slot_size = 4;
	}
	else if(arch_flag.cputype == CPU_TYPE_MC88000){
	    branch = "br";
	    trap = "illop1";
	    target_byte_sex = BIG_ENDIAN_BYTE_SEX;
	    branch_slot_size = 4;
	}
	else if(arch_flag.cputype == CPU_TYPE_I386){
	    branch = "jmp";
	    trap = ".byte 0xf4,0xf4,0xf4,0xf4,0xf4";
	    target_byte_sex = LITTLE_ENDIAN_BYTE_SEX;
	    branch_slot_size = 5;
	}
	else if(arch_flag.cputype == CPU_TYPE_HPPA){
	    trap = "break 0,0\nbreak 0,0";
	    target_byte_sex = BIG_ENDIAN_BYTE_SEX;
	    branch_slot_size = 8;
	}
	else if(arch_flag.cputype == CPU_TYPE_SPARC){
	    branch = "ba,a";
	    trap = "ta 0";
	    target_byte_sex = BIG_ENDIAN_BYTE_SEX;
	    branch_slot_size = 4;
	}
	else{
	    if(arch_flag.name == NULL)
		fatal("unknown or unsupported cputype (%d) in object file: "
		      "%s", mh.cputype, object_list[0]->name);
	    else
		fatal("unsupported architecture for -arch %s", arch_flag.name);
	}

	family_arch_flag = get_arch_family_from_cputype(arch_flag.cputype);
	if(family_arch_flag != NULL){
	    arch_flag.name = (char *)(family_arch_flag->name);
	    arch_flag.cpusubtype = family_arch_flag->cpusubtype;
	}

	/*
	 * Build the branch table object from the branch table data structrure
	 * created from the #branch directive in the specification file.
	 */
	build_branch_object();

	/*
	 * Build the library identification object.
	 */
	build_lib_id_object();

	/*
	 * Check to see if all the objects listed exist and are readable
	 */
	for(i = 0; i < nobject_list; i++){
	    if(access(object_list[i]->name, R_OK) == -1)
		system_error("can't read object file: %s",
			     object_list[i]->name);
	}
	if(errors)
	    cleanup(0);

	/*
	 * Link the branch table object will all the objects in the object data
	 * structure created from the #objects directive in the specification
	 * file, at the addresses specified in there creating the target shared
	 * library.
	 */
	reset_runlist();
	add_runlist(ldname);
	add_runlist("-arch");
	add_runlist(arch_flag.name);
	add_runlist("-fvmlib");
	for(i = 0; i < ODDBALL_HASH_SIZE; i++){
	    obp = oddball_hash[i];
	    while(obp != (struct oddball *)0){
		if(obp->undefined){
		    add_runlist("-U");
		    add_runlist(obp->name);
		}
		obp = obp->next;
	    }
	}
	ap = aliases;
	while(ap != (struct alias *)0){
	    add_runlist(makestr("-i", ap->alias_name, ":", ap->real_name,
			(char *)0) );
	    ap = ap->next;
	}
	add_runlist("-segaddr");
	add_runlist(SEG_TEXT);
	sprintf(text_addr_string, "%x", (unsigned int)text_addr);
	add_runlist(text_addr_string);
	add_runlist("-segaddr");
	add_runlist(SEG_DATA);
	sprintf(data_addr_string, "%x", (unsigned int)data_addr);
	add_runlist(data_addr_string);

	for(i = 0; i < nldflags; i++)
	    add_runlist(ldflags[i]);

	add_runlist("-o");
	add_runlist(target_filename);
	add_runlist(branch_object_filename);
	add_runlist(lib_id_object_filename);
	for(i = 0; i < nobject_list; i++)
	    add_runlist(object_list[i]->name);

	if(run())
	    fatal("internal link edit using: %s failed", ldname);

	if(!dflag)
	    unlink(lib_id_object_filename);
}

/*
 * Build the branch table object by writing an assembly source file and
 * assembling it.  The assembly source has the form:
 *		.text
 *		.globl <slot name>
 *	<slot name>:	jmp	<symbol name>
 * Where <slot name> is some name (out of the name space for high level
 * languages) unique to that slot number and <symbol name> is the name of
 * symbol in that branch table slot.
 */
static
void
build_branch_object(void)
{
    long i, text_size, pad;
    FILE *stream;
    char *p;
    struct shlib_info shlib;

	branch_source_filename = "branch.s";
	branch_object_filename = "branch.o";

	(void)unlink(branch_source_filename);
	if((stream = fopen(branch_source_filename, "w")) == NULL)
	    system_fatal("can't create temporary file: %s for branch table "
			 "assembly source", branch_source_filename);
	if(fprintf(stream, "\t.text\n") == EOF)
	    system_fatal("can't write to temporary file: %s (branch table "
			 "assembly source)", branch_source_filename);
	if(fprintf(stream, "\t.globl %s\n%s:\n", shlib_symbol_name,
		   shlib_symbol_name) == EOF)
	    system_fatal("can't write to temporary file: %s (branch table "
			 "assembly source)", branch_source_filename);
	if(fprintf(stream, SHLIB_STRUCT, text_addr, 0, data_addr, 0,
		   minor_version, target_name, sizeof(shlib.name) -
	       (strlen(target_name) + 1)) == EOF)
	system_fatal("can't write to temporary file: %s (branch table "
		     "assembly source)", branch_source_filename);
	for(i = 0; i < nbranch_list; i++){
	    if(branch_list[i]->empty_slot){
		if(fprintf(stream, "\t%s\n", trap) == EOF)
		    system_fatal("can't write to temporary file: %s (branch "
				 "table assembly source)",
				 branch_source_filename);
	    }
	    else{
		p = branch_slot_symbol(i);
		if(fprintf(stream, "\t.globl %s\n%s:",p,p) == EOF)
		    system_fatal("can't write to temporary file: %s (branch "
				 "table assembly source)",
				 branch_source_filename);
		/* special case for hppa, since this machine needs to use
		 * multiple instructions to reach the branch target.
		 */
		if(arch_flag.cputype == CPU_TYPE_HPPA) {
		  if (fprintf(stream,"\tldil L`%s,%%r1\n\tbe,n R`%s(%%sr4,%%r1)\n",
				     branch_list[i]->name,
				     branch_list[i]->name) == EOF)
		      system_fatal("can't write to temporary file: %s (branch "
				   "table assembly source)",
				   branch_source_filename);
		  }
		else {
		  if (fprintf(stream,"\t%s\t%s\n",
		              branch,branch_list[i]->name) == EOF) 
		     system_fatal("can't write to temporary file: %s (branch "
				 "table assembly source)",
				 branch_source_filename);
		  }
	    }
	}
	/*
	 * The old assembler use to pad all sections to 4 bytes.  Since the new
	 * assemble does not and we need this for compatiblity the pad is added
	 * here.
	 */
	text_size = sizeof(struct shlib_info) + nbranch_list * branch_slot_size;
	pad = round(text_size, 4) - text_size;
	for(i = 0; i < pad; i++){
	    if(fprintf(stream, "\t.byte 0\n") == EOF)
		system_fatal("can't write to temporary file: %s (branch table "
			     "assembly source)", branch_source_filename);
	}
	if(fclose(stream) == EOF)
	    system_fatal("can't close temporary file: %s for branch table "
			 "assembly source", branch_source_filename);

	/*
	 * Assemble the source for the branch table and create the object file.
	 */
	reset_runlist();
	add_runlist(asname);
	add_runlist("-arch");
	add_runlist(arch_flag.name);
	add_runlist("-o");
	add_runlist(branch_object_filename);
	add_runlist(branch_source_filename);

	if(run())
	    fatal("internal assembly using: %s failed", asname);
}

/*
 * Build the library identification object.  This is done by directly writing
 * the object file.  This object file just contains the library identification
 * load command.
 */
static
void
build_lib_id_object(void)
{
    int fd;
    long fvmlib_name_size, object_size, offset;
    char *buffer;
    struct mach_header *mh;
    struct fvmlib_command *fvmlib;

	if(dflag){
	    lib_id_object_filename = "lib_id.o";
	}
	else{
	    lib_id_object_filename = mktemp(savestr("mkshlib_lid_XXXXXX"));
	}

	(void)unlink(lib_id_object_filename);
	if((fd = open(lib_id_object_filename, O_WRONLY | O_CREAT | O_TRUNC,
		      0666)) == -1)
	    system_fatal("can't create file: %s", lib_id_object_filename);


	/*
	 * This file is made up of the following:
	 *	mach header
	 *	an LC_IDFVMLIB command for the library
	 */
	fvmlib_name_size = round(strlen(target_name) + 1, sizeof(long));
	object_size = sizeof(struct mach_header) +
		      sizeof(struct fvmlib_command) +
		      fvmlib_name_size;
	buffer = allocate(object_size);

	offset = 0;
	mh = (struct mach_header *)(buffer + offset);
	mh->magic = MH_MAGIC;
	mh->cputype = arch_flag.cputype;
	mh->cpusubtype = arch_flag.cpusubtype;
	mh->filetype = MH_OBJECT;
	mh->ncmds = 1;
	mh->sizeofcmds = sizeof(struct fvmlib_command) +
			 fvmlib_name_size;
	mh->flags = MH_NOUNDEFS;
	offset += sizeof(struct mach_header);

	/*
	 * The LC_IDFVMLIB command for the library.  This appearing in an
	 * object is what causes the target library to be mapped in when it
	 * is executed.  The link editor collects these into the output file
	 * it builds from the input files.  Since every object in the host
	 * library refers to the library definition symbol defined in here
	 * this object is always linked with anything that uses this library.
	 */
	fvmlib = (struct fvmlib_command *)(buffer + offset);
	fvmlib->cmd = LC_IDFVMLIB;
	fvmlib->cmdsize = sizeof(struct fvmlib_command) +
			  fvmlib_name_size;
	fvmlib->fvmlib.name.offset = sizeof(struct fvmlib_command);
	if(image_version != 0)
	    fvmlib->fvmlib.minor_version = image_version;
	else
	    fvmlib->fvmlib.minor_version = minor_version;
	fvmlib->fvmlib.header_addr = text_addr;
	offset += sizeof(struct fvmlib_command);
	strcpy(buffer + offset, target_name);
	offset += fvmlib_name_size;

	if(host_byte_sex != target_byte_sex){
	    swap_mach_header(mh, target_byte_sex);
	    swap_fvmlib_command(fvmlib, target_byte_sex);
	}
	if(write(fd, buffer, object_size) != object_size)
	    system_fatal("can't write file: %s", lib_id_object_filename);
	if(close(fd) == -1)
	    system_fatal("can't close file: %s", lib_id_object_filename);
	free(buffer);
}
