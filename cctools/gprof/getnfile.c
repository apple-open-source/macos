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
#include <string.h>
#include <mach-o/loader.h>
#include <mach-o/stab.h>
#include <mach-o/rld.h>
#include "stuff/ofile.h"
#include "stuff/allocate.h"
#include "stuff/errors.h"
#include "gprof.h"

struct shlib_text_range *shlib_text_ranges = NULL;
unsigned long nshlib_text_ranges = 0;

static struct ofile ofile = { 0 };

#ifdef __OPENSTEP__
static unsigned long link_edit_address;
static unsigned long address_func(
    unsigned long size,
    unsigned long headers_size);
#endif

static void count_func_symbols(
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strsize);

static void load_func_symbols(
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strsize,
    unsigned long vmaddr_slide);

static enum bool funcsymbol(
    struct nlist *nlistp,
    char *name);

static int valcmp(
    nltype *p1,
    nltype *p2);

static void count_N_SO_stabs(
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strsize);

static void load_files(
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strsize);

void
getnfile(
void)
{
    unsigned long i, j, text_highpc;
    struct arch_flag host_arch_flag;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;
    struct symtab_command *st;
#ifdef notdef
    struct ofile lib_ofile;
    unsigned long k;
    struct fvmlib_command *fl;
    struct load_command *lib_lc;
    struct symtab_command *lib_st;
    char *lib_name;
#endif

	nname = 0;
	n_files = 0;

	if(get_arch_from_host(&host_arch_flag, NULL) == 0)
	    fatal("can't determine the host architecture");

	if(ofile_map(a_outname, &host_arch_flag, NULL, &ofile, FALSE) == FALSE)
	    return;

	if(ofile.mh == NULL ||
	   (ofile.mh->filetype != MH_EXECUTE &&
	    ofile.mh->filetype != MH_DYLIB &&
	    ofile.mh->filetype != MH_DYLINKER))
	    fatal("file: %s is not a Mach-O executable file", a_outname);

	/*
	 * Pass 1 count symbols and files.
	 */
	st = NULL;
	lc = ofile.load_commands;
	text_highpc = 0xfffffffe;
	for(i = 0; i < ofile.mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
		      ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->sectname, SECT_TEXT) == 0 &&
		       strcmp(s->segname, SEG_TEXT) == 0){
			textspace = (unsigned char *)ofile.object_addr +
				    s->offset;
			text_highpc = s->addr + s->size;
		    }
		    s++;
		}
	    }
#ifdef notdef
	    else if(lc->cmd == LC_LOADFVMLIB){
		fl = (struct fvmlib_command *)lc;
		lib_name = (char *)fl + fl->fvmlib.name.offset;
		if(ofile_map(lib_name, &host_arch_flag, NULL, &lib_ofile,
			     FALSE) == FALSE)
		    goto done1;
		if(lib_ofile.mh == NULL || lib_ofile.mh->filetype != MH_FVMLIB){
		    warning("file: %s is not a shared library file", lib_name);
		    goto done_and_unmap1;
		}
		lib_st = NULL;
		lib_lc = lib_ofile.load_commands;
		for(j = 0; j < lib_ofile.mh->ncmds; j++){
		    if(lib_st == NULL && lib_lc->cmd == LC_SYMTAB){
			lib_st = (struct symtab_command *)lib_lc;
			count_func_symbols((struct nlist *)
				(lib_ofile.object_addr + lib_st->symoff),
				lib_st->nsyms,
				lib_ofile.object_addr + lib_st->stroff,
				lib_st->strsize);
			break;
		    }
		    else if(lib_lc->cmd == LC_SEGMENT){
			sg = (struct segment_command *)lib_lc;
			s = (struct section *)
			      ((char *)sg + sizeof(struct segment_command));
			for(k = 0; k < sg->nsects; k++){
			    if(strcmp(s->sectname, SECT_TEXT) == 0 &&
			       strcmp(s->segname, SEG_TEXT) == 0){
				shlib_text_ranges =
				    reallocate(shlib_text_ranges,
					       (nshlib_text_ranges + 1) *
					       sizeof(struct shlib_text_range));
				shlib_text_ranges[nshlib_text_ranges].lowpc = 
				    s->addr;
				shlib_text_ranges[nshlib_text_ranges].highpc = 
				    s->addr + s->size;
				nshlib_text_ranges++;
			    }
			    s++;
			}
		    }
		    lib_lc = (struct load_command *)
				((char *)lib_lc + lib_lc->cmdsize);
		}
done_and_unmap1:
		ofile_unmap(&lib_ofile);
done1:		;
	    }
#endif
	    else if(st == NULL && lc->cmd == LC_SYMTAB){
		st = (struct symtab_command *)lc;
		count_func_symbols((struct nlist *)
				   (ofile.object_addr + st->symoff), st->nsyms,
				   ofile.object_addr + st->stroff, st->strsize);
		count_N_SO_stabs((struct nlist *)
				 (ofile.object_addr + st->symoff), st->nsyms,
				 ofile.object_addr + st->stroff, st->strsize);
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}

	if(nname == 0)
	    fatal("executable file %s: has no symbols", a_outname);
	/*
	 * Allocate the data structures for the symbols and files.
	 */
	nl = (nltype *)calloc(nname + 2, sizeof(nltype));
	if(nl == NULL)
	    fatal("No room for %lu bytes of symbol table\n",
		  (nname + 2) * sizeof(nltype));
	npe = nl;
	files = (struct file *)calloc(n_files/2, sizeof(struct file));
	if(files == NULL)
	    fatal("No room for %lu bytes of file table\n",
		  n_files/2 * sizeof(struct file));
	n_files = 0;

	/*
	 * Pass 2 load symbols and files.
	 */
	if(st != NULL){
	    load_func_symbols((struct nlist *)
			      (ofile.object_addr + st->symoff), st->nsyms,
			       ofile.object_addr + st->stroff, st->strsize, 0);
	    load_files((struct nlist *)
		       (ofile.object_addr + st->symoff), st->nsyms,
			ofile.object_addr + st->stroff, st->strsize);
	}
#ifdef notdef
	lc = ofile.load_commands;
	for(i = 0; i < ofile.mh->ncmds; i++){
	    if(lc->cmd == LC_LOADFVMLIB){
		fl = (struct fvmlib_command *)lc;
		lib_name = (char *)fl + fl->fvmlib.name.offset;
		if(ofile_map(lib_name, &host_arch_flag, NULL, &lib_ofile, 
			     FALSE) == TRUE){
		    lib_st = NULL;
		    lib_lc = lib_ofile.load_commands;
		    for(j = 0; j < lib_ofile.mh->ncmds; j++){
			lib_lc = (struct load_command *)
				    ((char *)lib_lc + lib_lc->cmdsize);
			if(lib_st == NULL && lib_lc->cmd == LC_SYMTAB){
			    lib_st = (struct symtab_command *)lib_lc;
			    load_func_symbols((struct nlist *)
				    (lib_ofile.object_addr + lib_st->symoff),
				    lib_st->nsyms,
				    lib_ofile.object_addr + lib_st->stroff,
				    lib_st->strsize, 0);
			    break;
			}
		    }
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
#endif

	npe->value = text_highpc;
	npe->name = "past end of text";
	npe++;
	nname++;

	npe->value = npe->svalue = 0xffffffff;
	npe->name = "top of memory";

	qsort(nl, nname, sizeof(nltype),
	      (int (*)(const void *, const void *))valcmp);
#ifdef DEBUG
	if(debug & AOUTDEBUG){
	    for(i = 0; i < nname; i++){
		printf("[getnfile] 0x%08x\t%s\n", (unsigned int)nl[i].value, 
		       nl[i].name);
	    }
	}
#endif DEBUG
}

#ifdef __OPENSTEP__
void
get_rld_state_symbols(
void)
{
    unsigned long i, j, save_nname;
    NXStream *stream;
    struct mach_header **headers;
    char *object_addr;
    struct load_command *lc;
    struct symtab_command *st;

	if(grld_nloaded_states == 0)
	    return;
	headers = allocate(grld_nloaded_states * sizeof(struct mach_header *));

	/*
	 * Load the a_outname file as the base file.
	 */
	stream = NXOpenFile(fileno(stdout), NX_WRITEONLY);
	if(rld_load_basefile(stream, a_outname) == 0){
	    NXFlush(stream);
	    fflush(stdout);
	    fatal("can't load: %s as base file", a_outname);
	}
	/*
	 * Preform an rld_load() for each state at the state's address.
	 */
	for(i = 0; i < grld_nloaded_states; i++){
	    link_edit_address = (unsigned long)grld_loaded_state[i].header_addr;
	    rld_address_func(address_func);
	    if(rld_load(stream, &(headers[i]),
			grld_loaded_state[i].object_filenames,
			RLD_DEBUG_OUTPUT_FILENAME) == 0){
		NXFlush(stream);
		fflush(stdout);
		fatal("rld_load() failed");
	    }
	}

	/*
	 * Pass 1 count symbols
	 */
	save_nname = nname;
	for(i = 0; i < grld_nloaded_states; i++){
	    st = NULL;
	    object_addr = (char *)headers[i];
	    lc = (struct load_command *)
		 (object_addr + sizeof(struct mach_header));
	    for(j = 0; j < headers[i]->ncmds; j++){
		if(st == NULL && lc->cmd == LC_SYMTAB){
		    st = (struct symtab_command *)lc;
		    count_func_symbols((struct nlist *)
				       (object_addr + st->symoff),
				       st->nsyms,
				       object_addr + st->stroff,
				       st->strsize);
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
	/*
	 * Reallocate the data structures for the symbols.
	 */
	nl = (nltype *)realloc(nl, (nname + 1) * sizeof(nltype));
	if(nl == NULL)
	    fatal("No room for %lu bytes of symbol table\n",
		  (nname + 1) * sizeof(nltype));
	npe = nl + (save_nname + 1);
	memset(npe, '\0', (nname - save_nname) * sizeof(nltype));
	/*
	 * Pass 2 load symbols.
	 */
	for(i = 0; i < grld_nloaded_states; i++){
	    st = NULL;
	    object_addr = (char *)headers[i];
	    lc = (struct load_command *)
		 (object_addr + sizeof(struct mach_header));
	    for(j = 0; j < headers[i]->ncmds; j++){
		if(st == NULL && lc->cmd == LC_SYMTAB){
		    st = (struct symtab_command *)lc;
		    load_func_symbols((struct nlist *)
				      (object_addr + st->symoff),
				      st->nsyms,
				      object_addr + st->stroff,
				      st->strsize,
				      0);
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
#ifdef DEBUG
	if(debug & RLDDEBUG){
	    for(i = save_nname + 1; i < nname + 1; i++){
		printf("[get_rld_state_symbols] 0x%08x\t%s\n",
			(unsigned int)nl[i].value, nl[i].name);
	    }
	}
#endif DEBUG
	/*
	 * Resort the symbol table.
	 */
	qsort(nl, nname + 1, sizeof(nltype),
	      (int (*)(const void *, const void *))valcmp);
	free(headers);
}

static
unsigned long
address_func(
unsigned long size,
unsigned long headers_size)
{
	return(link_edit_address);
}
#endif /* defined(__OPENSTEP__) */

void
get_dyld_state_symbols(
void)
{
    unsigned long i, j, save_nname;
    struct ofile *ofiles;
    struct arch_flag host_arch_flag;
    struct load_command *lc;
    struct symtab_command *st;

	if(image_count == 0)
	    return;
	/*
	 * Create an ofile for each image.
	 */
	if(get_arch_from_host(&host_arch_flag, NULL) == 0)
	    fatal("can't determine the host architecture");
	ofiles = allocate(image_count * sizeof(struct ofile));
	for(i = 0; i < image_count; i++){
	    if(ofile_map(dyld_images[i].name, &host_arch_flag, NULL, ofiles + i,
			 FALSE) == 0)
		fatal("ofile_map() failed");
	    if(ofiles[i].mh == NULL)
		fatal("file from dyld loaded state: %s is not a Mach-O file",
		      dyld_images[i].name);
	}

	/*
	 * Pass 1 count symbols
	 */
	save_nname = nname;
	for(i = 0; i < image_count; i++){
	    st = NULL;
	    lc = ofiles[i].load_commands;
	    for(j = 0; j < ofiles[i].mh->ncmds; j++){
		if(st == NULL && lc->cmd == LC_SYMTAB){
		    st = (struct symtab_command *)lc;
		    count_func_symbols((struct nlist *)
				       (ofiles[i].object_addr + st->symoff),
				       st->nsyms,
				       ofiles[i].object_addr + st->stroff,
				       st->strsize);
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
	/*
	 * Reallocate the data structures for the symbols.
	 */
	nl = (nltype *)realloc(nl, (nname + 1) * sizeof(nltype));
	if(nl == NULL)
	    fatal("No room for %lu bytes of symbol table\n",
		  (nname + 1) * sizeof(nltype));
	npe = nl + (save_nname + 1);
	memset(npe, '\0', (nname - save_nname) * sizeof(nltype));
	/*
	 * Pass 2 load symbols.
	 */
	for(i = 0; i < image_count; i++){
	    st = NULL;
	    lc = ofiles[i].load_commands;
	    for(j = 0; j < ofiles[i].mh->ncmds; j++){
		if(st == NULL && lc->cmd == LC_SYMTAB){
		    st = (struct symtab_command *)lc;
		    load_func_symbols((struct nlist *)
				      (ofiles[i].object_addr + st->symoff),
				      st->nsyms,
				      ofiles[i].object_addr + st->stroff,
				      st->strsize,
				      dyld_images[i].vmaddr_slide);
		}
		lc = (struct load_command *)((char *)lc + lc->cmdsize);
	    }
	}
#ifdef DEBUG
	if(debug & DYLDDEBUG){
	    for(i = save_nname + 1; i < nname + 1; i++){
		printf("[get_dyld_state_symbols] 0x%08x\t%s\n",
			(unsigned int)nl[i].value, nl[i].name);
	    }
	}
#endif DEBUG
	/*
	 * Resort the symbol table.
	 */
	qsort(nl, nname + 1, sizeof(nltype),
	      (int (*)(const void *, const void *))valcmp);
	free(ofiles);
}

static
void
count_func_symbols(
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strsize)
{
    unsigned long i;

	for(i = 0; i < nsymbols; i++){
	    if(symbols[i].n_un.n_strx != 0 && symbols[i].n_un.n_strx < strsize){
		if(funcsymbol(symbols + i, strings + symbols[i].n_un.n_strx))
		    nname++;
	    }
	}
}

static
void
load_func_symbols(
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strsize,
unsigned long vmaddr_slide)
{
    unsigned long i;

	for(i = 0; i < nsymbols; i++){
	    if(symbols[i].n_un.n_strx != 0 && symbols[i].n_un.n_strx < strsize){
		if(funcsymbol(symbols + i, strings + symbols[i].n_un.n_strx)){
		    npe->value = symbols[i].n_value + vmaddr_slide;
		    npe->name = strings + symbols[i].n_un.n_strx;
		    npe++;
		}
	    }
	}
}

static
enum bool
funcsymbol(
struct nlist *nlistp,
char *name)
{
    int type;

	/*
	 *	must be a text symbol,
	 *	and static text symbols don't qualify if aflag set.
	 */
	if(nlistp->n_type & N_STAB)
	    return(FALSE);
	type = (nlistp->n_type & N_TYPE);
	if(type == N_SECT && nlistp->n_sect == 1)
	    type = N_TEXT;
	if(type != N_TEXT)
	    return FALSE;
	if((!(nlistp->n_type&N_EXT)) && aflag)
	    return(FALSE);
	/*
	 * can't have any `funny' characters in name,
	 * where `funny' includes	`.', .o file names
	 *			and	`$', pascal labels.
	 */
	for( ; *name ; name += 1 ){
	    if(*name == '.' || *name == '$'){
		return(FALSE);
	    }
	}
	return(TRUE);
}

static
int
valcmp(
nltype *p1,
nltype *p2)
{
	if(p1->value < p2->value){
	    return(LESSTHAN);
	}
	if(p1->value > p2->value){
	    return(GREATERTHAN);
	}
	return(EQUALTO);
}

static
void
count_N_SO_stabs(
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strsize)
{
    unsigned long i, len;
    char *name;

	for(i = 0; i < nsymbols; i++){
	    if(symbols[i].n_type == N_SO){
		/* skip the N_SO for the directory name that ends in a '/' */
		if(symbols[i].n_un.n_strx != 0 &&
		   symbols[i].n_un.n_strx < strsize){
		    name = strings + symbols[i].n_un.n_strx;
		    len = strlen(name);
		    if(len != 0 && name[len-1] == '/')
			continue;
		}
		n_files++;
	    }
	}
}

static
void
load_files(
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strsize)
{
    unsigned long i;
    char *s, *name;
    int len;
    int oddeven;

	oddeven = 0;
	for(i = 0; i < nsymbols; i++){
	    if(symbols[i].n_type == N_SO){
		/* skip the N_SO for the directory name that ends in a '/' */
		if(symbols[i].n_un.n_strx != 0 &&
		   symbols[i].n_un.n_strx < strsize){
		    name = strings + symbols[i].n_un.n_strx;
		    len = strlen(name);
		    if(len != 0 && name[len-1] == '/')
			continue;
		}
		if(oddeven){
		    files[n_files++].lastpc = symbols[i].n_value;
		    oddeven = 0;
		}
		else {
		    s = strings + symbols[i].n_un.n_strx;
		    len = strlen(s);
		    if(len > 0)
			s[len-1] = 'o';
		    files[n_files].name = files[n_files].what_name = s;
		    files[n_files].firstpc = symbols[i].n_value; 
		    oddeven = 1;
		}
	    }
	}
#ifdef notdef
	for(i = 0; i < n_files; i++){
	    fprintf(stderr, "files[%lu] firstpc = 0x%x name = %s\n", i,
		    (unsigned int)(files[i].firstpc), files[i].name);
	}
#endif
}

void
get_text_min_max(
unsigned long *text_min,
unsigned long *text_max)
{
    unsigned long i, j;
    struct load_command *lc;
    struct segment_command *sg;
    struct section *s;

	*text_min = 0;
	*text_max = 0xffffffff;

	lc = ofile.load_commands;
	for (i = 0; i < ofile.mh->ncmds; i++){
	    if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
		      ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(strcmp(s->sectname, SECT_TEXT) == 0 &&
		       strcmp(s->segname, SEG_TEXT) == 0){
			*text_min = s->addr;
			*text_max = s->addr + s->size;
			return;
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
}
