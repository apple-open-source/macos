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
#import <stdio.h>
#import <stdlib.h>
#import <string.h>
#import "stuff/ofile.h"
#import "stuff/errors.h"
#import "stuff/allocate.h"

enum file_part_type {
    FP_FAT_HEADERS,
    FP_MACH_O,
    FP_EMPTY_SPACE
};
char *file_part_type_names[] = {
    "FP_FAT_HEADERS",
    "FP_MACH_O",
    "FP_EMPTY_SPACE"
};

struct file_part {
    unsigned long offset;
    unsigned long size;
    enum file_part_type type;
    struct mach_o_part *mp;
    struct mach_header *mh;
    struct symtab_command *st;
    struct nlist *symbols;
    char *strings; 
    struct file_part *prev;
    struct file_part *next;
};
struct file_part *file_parts = NULL;

enum mach_o_part_type {
    MP_MACH_HEADERS,
    MP_SECTION,
    MP_RELOCS,
    MP_LOCAL_SYMBOLS,
    MP_EXTDEF_SYMBOLS,
    MP_UNDEF_SYMBOLS,
    MP_TOC,
    MP_MODULE_TABLE,
    MP_REFERENCE_TABLE,
    MP_INDIRECT_SYMBOL_TABLE,
    MP_EXT_RELOCS,
    MP_LOC_RELOCS,
    MP_SYMBOL_TABLE,
    MP_HINTS_TABLE,
    MP_STRING_TABLE,
    MP_EXT_STRING_TABLE,
    MP_LOC_STRING_TABLE,
    MP_EMPTY_SPACE
};
static char *mach_o_part_type_names[] = {
    "MP_MACH_HEADERS",
    "MP_SECTION",
    "MP_RELOCS",
    "MP_LOCAL_SYMBOLS",
    "MP_EXTDEF_SYMBOLS",
    "MP_UNDEF_SYMBOLS",
    "MP_TOC",
    "MP_MODULE_TABLE",
    "MP_REFERENCE_TABLE",
    "MP_INDIRECT_SYMBOL_TABLE",
    "MP_EXT_RELOCS",
    "MP_LOC_RELOCS",
    "MP_SYMBOL_TABLE",
    "MP_HINTS_TABLE",
    "MP_STRING_TABLE",
    "MP_EXT_STRING_TABLE",
    "MP_LOC_STRING_TABLE",
    "MP_EMPTY_SPACE"
};

struct mach_o_part {
    unsigned long offset;
    unsigned long size;
    enum mach_o_part_type type;
    struct section *s;
    struct mach_o_part *prev;
    struct mach_o_part *next;
};

/* The name of the program for error messages */
char *progname = NULL;

/* The ofile for the Mach-O file argument to the program */
static struct ofile ofile;

static struct nlist *sorted_symbols = NULL;

static void create_file_parts(
    char *file_name);
static struct file_part *new_file_part(
    void);
static void insert_file_part(
    struct file_part *new);
static void print_file_parts(
    void);

static void create_mach_o_parts(
    struct file_part *fp);
static struct mach_o_part *new_mach_o_part(
    void);
static void insert_mach_o_part(
    struct file_part *fp,
    struct mach_o_part *new);
static void print_mach_o_parts(
    struct mach_o_part *mp);

static void print_parts_for_page(
    unsigned long page_number);
static void print_arch(
    struct mach_header *mh);
static void print_file_part(
    struct file_part *fp);
static void print_mach_o_part(
    struct mach_o_part *mp);
static void print_symbols(
    struct file_part *fp,
    unsigned long low_addr,
    unsigned long high_addr);
static int compare(
    struct nlist *p1,
    struct nlist *p2);

/*
 * pagestuff is invoked as follows:
 *
 *	% pagestuff mach-o pagenumber [pagenumber ...]
 *
 * It prints out what stuff is on the page numbers listed.
 */
int
main(
int argc,
char *argv[])
{
    unsigned long i, start, page_number;
    char *endp;

	progname = argv[0];
	if(argc < 3){
	    fprintf(stderr, "Usage: %s mach-o pagenumber [pagenumber ...]\n",
		    progname);
	    exit(EXIT_FAILURE);
	}
	start = 2;

	create_file_parts(argv[1]);
	if(strcmp(argv[start], "-p") == 0){
	    print_file_parts();
	    start++;
	}
	if(errors)
	    exit(EXIT_FAILURE);
	if(start >= argc)
	    exit(EXIT_SUCCESS);

	if(strcmp(argv[start], "-a") == 0){
	    page_number = (ofile.file_size + vm_page_size - 1) / vm_page_size;
	    for(i = 0; i < page_number; i++){
		print_parts_for_page(i);
	    }
	    start++;
	}
	if(start >= argc)
	    exit(EXIT_SUCCESS);

	for(i = start; i < argc; i++){
	    page_number = strtoul(argv[i], &endp, 10);
	    if(*endp != '\0')
		fatal("page number argument: %s is not a proper unsigned "
		      "number", argv[i]);
	    print_parts_for_page(page_number);
	}

	return(EXIT_SUCCESS);
}

static
void
create_file_parts(
char *file_name)
{
    static struct file_part *fp;

	if(ofile_map(file_name, NULL, NULL, &ofile, FALSE) == FALSE)
	    exit(EXIT_FAILURE);

	/* first create an empty space for the whole file */
	fp = new_file_part();
	fp->offset = 0;
	fp->size = ofile.file_size;
	fp->type = FP_EMPTY_SPACE;
	file_parts = fp;

	if(ofile.file_type == OFILE_FAT){
	    fp = new_file_part();
	    fp->offset = 0;
	    fp->size = sizeof(struct fat_header) +
		       ofile.fat_header->nfat_arch * sizeof(struct fat_arch);
	    fp->type = FP_FAT_HEADERS;
	    insert_file_part(fp);

	    (void)ofile_first_arch(&ofile);
	    do{
		if(ofile.arch_type == OFILE_ARCHIVE){
		    error("for architecture: %s file: %s is an archive (not "
			"supported with %s)", ofile.arch_flag.name, file_name,
			progname);
		}
		else if(ofile.arch_type == OFILE_Mach_O){
		    /* make mach-o parts for this */
		    fp = new_file_part();
		    fp->offset = ofile.fat_archs[ofile.narch].offset;
		    fp->size = ofile.fat_archs[ofile.narch].size;
		    fp->type = FP_MACH_O;
		    insert_file_part(fp);
		    create_mach_o_parts(fp);
		}
		else if(ofile.arch_type == OFILE_UNKNOWN){
		    error("for architecture: %s file: %s is not an object file "
			"(not supported with %s)", ofile.arch_flag.name,
			file_name, progname);
		}
	    }while(ofile_next_arch(&ofile) == TRUE);
	}
	else if(ofile.file_type == OFILE_ARCHIVE){
	    error("file: %s is an archive (not supported with %s)", file_name,
		progname);
	}
	else if(ofile.file_type == OFILE_Mach_O){
	    /* make mach-o parts for this */
	    fp = new_file_part();
	    fp->offset = 0;
	    fp->size = ofile.file_size;
	    fp->type = FP_MACH_O;
	    insert_file_part(fp);
	    create_mach_o_parts(fp);
	}
	else{ /* ofile.file_type == OFILE_UNKNOWN */
	    error("file: %s is not an object file (not supported with %s)",
		file_name, progname);
	}
}

static
struct file_part *
new_file_part(
void)
{
    struct file_part *fp;

	fp = allocate(sizeof(struct file_part));
	memset(fp, '\0', sizeof(struct file_part));
	return(fp);
}

static
void
insert_file_part(
struct file_part *new)
{
    struct file_part *p, *q;

	for(p = file_parts; p != NULL; p = p->next){
	    /* find an existing part that contains the new part */
	    if(new->offset >= p->offset &&
	       new->offset + new->size <= p->offset + p->size){
		if(p->type != FP_EMPTY_SPACE)
		    fatal("internal error: new file part not contained in "
			"empty space");
		/* if new is the same as p replace p with new */
		if(new->offset == p->offset &&
	           new->size == p->size){
		    new->prev = p->prev;
		    new->next = p->next;
		    if(p->next != NULL)
			p->next->prev = new;
		    if(p->prev != NULL)
			p->prev->next = new;
		    if(file_parts == p)
			file_parts = new;
		    free(p);
		    return;
		}
		/* if new starts at p put new before p and move p's offset */
		if(new->offset == p->offset){
		    p->offset = new->offset + new->size;
		    p->size = p->size - new->size;
		    if(p->prev != NULL)
			p->prev->next = new;
		    new->prev = p->prev;
		    p->prev = new;
		    new->next = p;
		    if(file_parts == p)
			file_parts = new;
		    return;
		}
		/* if new ends at p put new after p and change p's size */
		if(new->offset + new->size == p->offset + p->size){
		    p->size = new->offset - p->offset;
		    new->next = p->next;
		    if(p->next != NULL)
			p->next->prev = new;
		    p->next = new;
		    new->prev = p;
		    return;
		}
		/* new is in the middle of p */
		q = new_file_part();
		q->type = FP_EMPTY_SPACE;
		q->offset = new->offset + new->size;
		q->size = p->offset + p->size - (new->offset + new->size);
		p->size = new->offset - p->offset;
		new->prev = p;
		new->next = q;
		q->prev = new;
		q->next = p->next;
		if(p->next != NULL)
		    p->next->prev = q;
		p->next = new;
		return;
	    }
	}
	fatal("internal error: new file part not found in existing part");
}

static
void
print_file_parts(
void)
{
    struct file_part *p, *prev;
    unsigned long offset;

	prev = NULL;
	offset = 0;
	for(p = file_parts; p != NULL; p = p->next){
	    printf("%s\n", file_part_type_names[p->type]);
	    printf("    offset = %lu\n", p->offset);
	    printf("    size = %lu\n", p->size);
	    if(prev != NULL)
		if(prev != p->prev)
		    printf("bad prev pointer\n");
	    prev = p;
	    if(offset != p->offset)
		    printf("bad offset\n");
	    offset += p->size;
	    if(p->type == FP_MACH_O)
		print_mach_o_parts(p->mp);
	}
}

static
void
create_mach_o_parts(
struct file_part *fp)
{
    unsigned long i, j;
    struct mach_o_part *mp;
    struct load_command *lc;
    struct symtab_command *st;
    struct dysymtab_command *dyst;
    struct twolevel_hints_command *hints;
    struct segment_command *sg;
    struct section *s;
    struct nlist *allocated_symbols, *symbols;
    unsigned long ext_low, ext_high, local_low, local_high;
    char *strings;
    struct dylib_module *modtab;

	mp = new_mach_o_part();
	mp->offset = fp->offset;
	mp->size = ofile.object_size;
	mp->type = MP_EMPTY_SPACE;
	fp->mp = mp;

	mp = new_mach_o_part();
	mp->offset = fp->offset;
	mp->size = sizeof(struct mach_header) + ofile.mh->sizeofcmds;
	mp->type = MP_MACH_HEADERS;
	insert_mach_o_part(fp, mp);

	fp->mh = ofile.mh;

	st = NULL;
	dyst = NULL;
	hints = NULL;
	symbols = NULL;
	strings = NULL;
	lc = ofile.load_commands;
	for(i = 0; i < ofile.mh->ncmds; i++){
	    if(st == NULL && lc->cmd == LC_SYMTAB){
		st = (struct symtab_command *)lc;
	    }
	    else if(dyst == NULL && lc->cmd == LC_DYSYMTAB){
		dyst = (struct dysymtab_command *)lc;
	    }
	    else if(hints == NULL && lc->cmd == LC_TWOLEVEL_HINTS){
		hints = (struct twolevel_hints_command *)lc;
	    }
	    else if(lc->cmd == LC_SEGMENT){
		sg = (struct segment_command *)lc;
		s = (struct section *)
		      ((char *)sg + sizeof(struct segment_command));
		for(j = 0; j < sg->nsects; j++){
		    if(s->nreloc != 0){
			mp = new_mach_o_part();
			mp->offset = fp->offset + s->reloff;
			mp->size = s->nreloc * sizeof(struct relocation_info);
			mp->type = MP_RELOCS;
			mp->s = s;
			insert_mach_o_part(fp, mp);
		    }
		    if((s->flags & SECTION_TYPE) != S_ZEROFILL && s->size != 0){
			mp = new_mach_o_part();
			mp->offset = fp->offset + s->offset;
			mp->size = s->size;
			mp->type = MP_SECTION;
			mp->s = s;
			insert_mach_o_part(fp, mp);
		    }
		    if((s->flags & SECTION_TYPE) == S_ZEROFILL && s->size != 0){
			if(s->addr - sg->vmaddr < sg->filesize){
			    mp = new_mach_o_part();
			    mp->offset = fp->offset + sg->fileoff +
					 s->addr - sg->vmaddr;
			    if(s->addr - sg->vmaddr + s->size <= sg->filesize)
				mp->size = s->size;
			    else
				mp->size = sg->filesize -(s->addr - sg->vmaddr);
			    mp->type = MP_SECTION;
			    mp->s = s;
			    insert_mach_o_part(fp, mp);
			}
		    }
		    s++;
		}
	    }
	    lc = (struct load_command *)((char *)lc + lc->cmdsize);
	}
	if(st != NULL){
	    allocated_symbols = NULL;
	    symbols = (struct nlist *)(ofile.object_addr + st->symoff);
	    if(ofile.object_byte_sex != get_host_byte_sex()){
		allocated_symbols = allocate(sizeof(struct nlist) *
					     st->nsyms);
		memcpy(allocated_symbols, symbols,
		       sizeof(struct nlist) * st->nsyms);
		swap_nlist(allocated_symbols, st->nsyms,
			   get_host_byte_sex());
		symbols = allocated_symbols;
	    }
	    strings = ofile.object_addr + st->stroff;
	    fp->st = st;
	    fp->symbols = symbols;
	    fp->strings = strings;
	}

	if(dyst != NULL && st != NULL){
	    if(dyst->nlocalsym != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + st->symoff +
			     dyst->ilocalsym * sizeof(struct nlist);
		mp->size = dyst->nlocalsym * sizeof(struct nlist);
		mp->type = MP_LOCAL_SYMBOLS;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nextdefsym != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + st->symoff +
			     dyst->iextdefsym * sizeof(struct nlist);
		mp->size = dyst->nextdefsym * sizeof(struct nlist);
		mp->type = MP_EXTDEF_SYMBOLS;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nundefsym != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + st->symoff +
			     dyst->iundefsym * sizeof(struct nlist);
		mp->size = dyst->nundefsym * sizeof(struct nlist);
		mp->type = MP_UNDEF_SYMBOLS;
		insert_mach_o_part(fp, mp);
	    }
	    if(hints != NULL && hints->nhints != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + hints->offset;
		mp->size = hints->nhints *
			   sizeof(struct twolevel_hint);
		mp->type = MP_HINTS_TABLE;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->ntoc != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + dyst->tocoff;
		mp->size = dyst->ntoc * sizeof(struct dylib_table_of_contents);
		mp->type = MP_TOC;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nmodtab != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + dyst->modtaboff;
		mp->size = dyst->nmodtab * sizeof(struct dylib_module);
		mp->type = MP_MODULE_TABLE;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nextrefsyms != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + dyst->extrefsymoff;
		mp->size = dyst->nextrefsyms * sizeof(struct dylib_reference);
		mp->type = MP_REFERENCE_TABLE;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nindirectsyms != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + dyst->indirectsymoff;
		mp->size = dyst->nindirectsyms * sizeof(unsigned long);
		mp->type = MP_INDIRECT_SYMBOL_TABLE;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nextrel != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + dyst->extreloff;
		mp->size = dyst->nextrel * sizeof(struct relocation_info);
		mp->type = MP_EXT_RELOCS;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nlocrel != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + dyst->locreloff;
		mp->size = dyst->nlocrel * sizeof(struct relocation_info);
		mp->type = MP_LOC_RELOCS;
		insert_mach_o_part(fp, mp);
	    }
	    if(dyst->nlocalsym != 0 &&
	       (dyst->nextdefsym != 0 || dyst->nundefsym != 0)){
		/*
		 * Try to break the string table into the local and external
		 * parts knowing the tools should put all the external strings
		 * first and all the local string last.
		 */
		ext_low = st->strsize;
		local_low = st->strsize;
		ext_high = 0;
		local_high = 0;
		for(i = 0; i < st->nsyms; i++){
		    if(symbols[i].n_un.n_strx == 0)
			continue;
		    if(symbols[i].n_type & N_EXT ||
		       (fp->mh->filetype == MH_EXECUTE &&
		        symbols[i].n_type & N_PEXT)){
			if(symbols[i].n_un.n_strx > ext_high)
			    ext_high = symbols[i].n_un.n_strx;
			if(symbols[i].n_un.n_strx < ext_low)
			    ext_low = symbols[i].n_un.n_strx;
		    }
		    else{
			if(symbols[i].n_un.n_strx > local_high)
			    local_high = symbols[i].n_un.n_strx;
			if(symbols[i].n_un.n_strx < local_low)
			    local_low = symbols[i].n_un.n_strx;
		    }
		}

		modtab = (struct dylib_module *)(ofile.object_addr +
						 dyst->modtaboff);
		if(ofile.object_byte_sex != get_host_byte_sex())
		    swap_dylib_module(modtab, dyst->nmodtab,
			   get_host_byte_sex());
		for(i = 0; i < dyst->nmodtab; i++){
		    if(modtab[i].module_name > local_high)
			local_high = modtab[i].module_name;
		    if(modtab[i].module_name < local_low)
			local_low = modtab[i].module_name;
		}

		if(ext_high < local_low){
		    mp = new_mach_o_part();
		    mp->offset = fp->offset + st->stroff + ext_low;
		    mp->size = ext_high - ext_low +
			       strlen(strings + ext_high) + 1;
		    mp->type = MP_EXT_STRING_TABLE;
		    insert_mach_o_part(fp, mp);

		    mp = new_mach_o_part();
		    mp->offset = fp->offset + st->stroff + local_low;
		    mp->size = local_high - local_low +
			       strlen(strings + local_high) + 1;
		    mp->type = MP_LOC_STRING_TABLE;
		    insert_mach_o_part(fp, mp);
		}
		else{
		    mp = new_mach_o_part();
		    mp->offset = fp->offset + st->stroff;
		    mp->size = st->strsize;
		    mp->type = MP_STRING_TABLE;
		    insert_mach_o_part(fp, mp);
		}
	    }
	    else if(dyst->nextdefsym != 0 || dyst->nundefsym != 0){
		if(st->strsize != 0){
		    mp = new_mach_o_part();
		    mp->offset = fp->offset + st->stroff;
		    mp->size = st->strsize;
		    mp->type = MP_EXT_STRING_TABLE;
		    insert_mach_o_part(fp, mp);
		}
	    }
	    else{
		if(st->strsize != 0){
		    mp = new_mach_o_part();
		    mp->offset = fp->offset + st->stroff;
		    mp->size = st->strsize;
		    mp->type = MP_LOC_STRING_TABLE;
		    insert_mach_o_part(fp, mp);
		}
	    }
	}
	else if(st != NULL){
	    if(st->nsyms != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + st->symoff;
		mp->size = st->nsyms * sizeof(struct nlist);
		mp->type = MP_SYMBOL_TABLE;
		insert_mach_o_part(fp, mp);
	    }
	    if(st->strsize != 0){
		mp = new_mach_o_part();
		mp->offset = fp->offset + st->stroff;
		mp->size = st->strsize;
		mp->type = MP_STRING_TABLE;
		insert_mach_o_part(fp, mp);
	    }
	}
}

static
struct mach_o_part *
new_mach_o_part(
void)
{
    struct mach_o_part *mp;

	mp = allocate(sizeof(struct mach_o_part));
	memset(mp, '\0', sizeof(struct mach_o_part));
	return(mp);
}

static
void
insert_mach_o_part(
struct file_part *fp,
struct mach_o_part *new)
{
    struct mach_o_part *p, *q;

	for(p = fp->mp; p != NULL; p = p->next){
	    /* find an existing part that contains the new part */
	    if(new->offset >= p->offset &&
	       new->offset + new->size <= p->offset + p->size){
		if(p->type != MP_EMPTY_SPACE)
		    fatal("internal error: new mach_o part not contained in "
			"empty space");
		/* if new is the same as p replace p with new */
		if(new->offset == p->offset &&
	           new->size == p->size){
		    new->prev = p->prev;
		    new->next = p->next;
		    if(p->next != NULL)
			p->next->prev = new;
		    if(p->prev != NULL)
			p->prev->next = new;
		    if(fp->mp == p)
			fp->mp = new;
		    free(p);
		    return;
		}
		/* if new starts at p put new before p and move p's offset */
		if(new->offset == p->offset){
		    p->offset = new->offset + new->size;
		    p->size = p->size - new->size;
		    new->prev = p->prev;
		    if(p->prev != NULL)
			p->prev->next = new;
		    p->prev = new;
		    new->next = p;
		    if(fp->mp == p)
			fp->mp = new;
		    return;
		}
		/* if new ends at p put new after p and change p's size */
		if(new->offset + new->size == p->offset + p->size){
		    p->size = new->offset - p->offset;
		    new->next = p->next;
		    if(p->next != NULL)
			p->next->prev = new;
		    p->next = new;
		    new->prev = p;
		    return;
		}
		/* new is in the middle of p */
		q = new_mach_o_part();
		q->type = MP_EMPTY_SPACE;
		q->offset = new->offset + new->size;
		q->size = p->offset + p->size - (new->offset + new->size);
		p->size = new->offset - p->offset;
		new->prev = p;
		new->next = q;
		q->prev = new;
		q->next = p->next;
		if(p->next != NULL)
		    p->next->prev = q;
		p->next = new;
		return;
	    }
	}
	fatal("internal error: new mach_o part not found in existing part");
}

static
void
print_mach_o_parts(
struct mach_o_part *mp)
{
    struct mach_o_part *p, *prev;
    unsigned long offset;

	offset = 0;
	prev = NULL;
	if(mp != NULL)
	    offset = mp->offset;
	for(p = mp; p != NULL; p = p->next){
	    if(p->type == MP_SECTION)
		printf("    MP_SECTION (%.16s,%.16s)\n",
		p->s->segname, p->s->sectname);
	    else
		printf("    %s\n", mach_o_part_type_names[p->type]);
	    printf("\toffset = %lu\n", p->offset);
	    printf("\tsize = %lu\n", p->size);
	    if(prev != NULL)
		if(prev != p->prev)
		    printf("bad prev pointer\n");
	    prev = p;
	    if(offset != p->offset)
		    printf("bad offset\n");
	    offset += p->size;
	}
}

static
void
print_parts_for_page(
unsigned long page_number)
{
    unsigned long offset, size,
		  low_addr, high_addr, new_low_addr, new_high_addr;
    struct file_part *fp;
    struct mach_o_part *mp;
    enum bool printed;
    enum bool sections;

	offset = page_number * vm_page_size;
	size = vm_page_size;
	low_addr = 0;
	high_addr = 0;

	if(offset > ofile.file_size){
	    printf("File has no page %lu (file has only %lu pages)\n",
		   page_number,
		   (ofile.file_size + vm_page_size - 1) / vm_page_size);
	    return;
	}

	/*
	 * First pass through the file printing whats on the page ignoring the
	 * empty spaces.
	 */
	printed = FALSE;
	for(fp = file_parts; fp != NULL; fp = fp->next){
	    if(offset + size <= fp->offset)
		continue;
	    if(offset > fp->offset + fp->size)
		continue;
	    switch(fp->type){
	    case FP_FAT_HEADERS:
		printf("File Page %lu contains fat file headers\n",
		       page_number);
		printed = TRUE;
		break;
	    case FP_MACH_O:
		sections = FALSE;
		for(mp = fp->mp; mp != NULL; mp = mp->next){
		    if(offset + size <= mp->offset)
			continue;
		    if(offset > mp->offset + mp->size)
			continue;
		    switch(mp->type){
		    case MP_MACH_HEADERS:
			printf("File Page %lu contains Mach-O headers",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_SECTION:
			printf("File Page %lu contains contents of "
			       "section (%.16s,%.16s)", page_number,
			       mp->s->segname, mp->s->sectname);
			print_arch(fp->mh);
			printed = TRUE;
			if(offset < mp->offset)
			    new_low_addr = mp->s->addr;
			else
			    new_low_addr = mp->s->addr + offset - mp->offset;
			if(offset + size > mp->offset + mp->size)
			    new_high_addr = mp->s->addr + mp->s->size;
			else
			    new_high_addr = mp->s->addr +
				(offset + size - mp->offset);
			if(sections == FALSE){
			    low_addr = new_low_addr;
			    high_addr = new_high_addr;
			}
			else{
			    if(new_low_addr < low_addr)
				low_addr = new_low_addr;
			    if(new_high_addr > high_addr)
				high_addr = new_high_addr;
		        }
			sections = TRUE;
			break;
		    case MP_RELOCS:
			printf("File Page %lu contains relocation entries for "
			       "section (%.16s,%.16s)", page_number,
			       mp->s->segname, mp->s->sectname);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_LOCAL_SYMBOLS:
			printf("File Page %lu contains symbol table for "	
			       "non-global symbols", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_EXTDEF_SYMBOLS:
			printf("File Page %lu contains symbol table for "	
			       "defined global symbols", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_UNDEF_SYMBOLS:
			printf("File Page %lu contains symbol table for "	
			       "undefined symbols", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_TOC:
			printf("File Page %lu contains table of contents",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_MODULE_TABLE:
			printf("File Page %lu contains module table",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_REFERENCE_TABLE:
			printf("File Page %lu contains reference table",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_INDIRECT_SYMBOL_TABLE:
			printf("File Page %lu contains indirect symbols table",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_EXT_RELOCS:
			printf("File Page %lu contains external relocation "
			       "entries", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_LOC_RELOCS:
			printf("File Page %lu contains local relocation "
			       "entries", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_SYMBOL_TABLE:
			printf("File Page %lu contains symbol table",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_HINTS_TABLE:
			printf("File Page %lu contains hints table",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_STRING_TABLE:
			printf("File Page %lu contains string table",
			       page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_EXT_STRING_TABLE:
			printf("File Page %lu contains string table for "
			       "external symbols", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_LOC_STRING_TABLE:
			printf("File Page %lu contains string table for "
			       "local symbols", page_number);
			print_arch(fp->mh);
			printed = TRUE;
			break;
		    case MP_EMPTY_SPACE:
			break;
		    }
		}
		if(sections == TRUE){
		    printf("Symbols on file page %lu virtual address 0x%x to "
			   "0x%x\n", page_number, (unsigned int)low_addr,
			   (unsigned int)high_addr);
		    print_symbols(fp, low_addr, high_addr);
		}
		break;

	    case FP_EMPTY_SPACE:
		break;
	    }
	}

	/*
	 * Make a second pass through the file if nothing got printing on the
	 * first pass because the page covers empty space in the file.
	 */
	if(printed == TRUE)
	    return;
	for(fp = file_parts; fp != NULL; fp = fp->next){
	    if(offset + size <= fp->offset)
		continue;
	    if(offset > fp->offset + fp->size)
		continue;
	    if(fp->type == FP_MACH_O){
		for(mp = fp->mp; mp != NULL; mp = mp->next){
		    if(offset + size <= mp->offset)
			continue;
		    if(offset > mp->offset + mp->size)
			continue;
		    printf("File Page %lu contains empty space in the Mach-O "
			   "file for %s between:\n", page_number,
			   get_arch_name_from_types(fp->mh->cputype,
						    fp->mh->cpusubtype));
		    if(mp->prev == NULL)
			printf("    the start of the Mach-O file");
		    else{
			printf("    ");
			print_mach_o_part(mp->prev);
		    }
		    printf(" and\n");
		    if(mp->next == NULL)
			printf("    the end of the Mach-O file\n");
		    else{
			printf("    ");
			print_mach_o_part(mp->next);
		    }
		    printf("\n");
		    return;
		}
		break;
	    }
	    printf("File Page %lu contains empty space in the file between:\n",
		   page_number);
	    if(fp->prev == NULL)
		printf("    the start of the file");
	    else{
		printf("    ");
		print_file_part(fp->prev);
	    }
	    printf(" and\n");
	    if(fp->next == NULL)
		printf("    the end of the file\n");
	    else{
		printf("    ");
		print_file_part(fp->next);
	    }
	    printf("\n");
	    return;
	}
}

static
void
print_arch(
struct mach_header *mh)
{
	if(ofile.file_type == OFILE_FAT){
	    printf(" (%s)\n",
		   get_arch_name_from_types(mh->cputype, mh->cpusubtype));
	}
	else
	    printf("\n");
}

static
void
print_file_part(
struct file_part *fp)
{
	switch(fp->type){
	case FP_FAT_HEADERS:
	    printf("fat file headers");
	    break;
	case FP_MACH_O:
	    printf("Mach-O file for %s",
		   get_arch_name_from_types(fp->mh->cputype,
					    fp->mh->cpusubtype));
	    break;
	case FP_EMPTY_SPACE:
	    printf("empty space");
	    break;
	}
}

static
void
print_mach_o_part(
struct mach_o_part *mp)
{
	switch(mp->type){
	case MP_MACH_HEADERS:
	    printf("Mach-O headers");
	    break;
	case MP_SECTION:
	    printf("contents of section (%.16s,%.16s)",
		   mp->s->segname, mp->s->sectname);
	    break;
	case MP_RELOCS:
	    printf("relocation entries for section (%.16s,%.16s)",
		   mp->s->segname, mp->s->sectname);
	    break;
	case MP_LOCAL_SYMBOLS:
	    printf("symbol table for non-global symbols");
	    break;
	case MP_EXTDEF_SYMBOLS:
	    printf("symbol table for defined global symbols");
	    break;
	case MP_UNDEF_SYMBOLS:
	    printf("symbol table for undefined symbols");
	    break;
	case MP_TOC:
	    printf("table of contents");
	    break;
	case MP_MODULE_TABLE:
	    printf("module table");
	    break;
	case MP_REFERENCE_TABLE:
	    printf("reference table");
	    break;
	case MP_INDIRECT_SYMBOL_TABLE:
	    printf("indirect symbol table");
	    break;
	case MP_EXT_RELOCS:
	    printf("external relocation entries");
	    break;
	case MP_LOC_RELOCS:
	    printf("local relocation entries");
	    break;
	case MP_SYMBOL_TABLE:
	    printf("symbol table");
	    break;
	case MP_HINTS_TABLE:
	    printf("hints table");
	    break;
	case MP_STRING_TABLE:
	    printf("string table");
	    break;
	case MP_EXT_STRING_TABLE:
	    printf("string table for external symbols");
	    break;
	case MP_LOC_STRING_TABLE:
	    printf("string table for local symbols");
	    break;
	case MP_EMPTY_SPACE:
	    printf("empty space");
	    break;
	}
}

static
void
print_symbols(
struct file_part *fp,
unsigned long low_addr,
unsigned long high_addr)
{
    unsigned long i, count;

	if(sorted_symbols == NULL)
	    sorted_symbols = allocate(sizeof(struct nlist) * fp->st->nsyms);

	count = 0;
	for(i = 0; i < fp->st->nsyms; i++){
	    if((fp->symbols[i].n_type & N_STAB) != 0)
		continue;
	    if(fp->symbols[i].n_value >= low_addr &&
	       fp->symbols[i].n_value <= high_addr){
		sorted_symbols[count] = fp->symbols[i];
		count++;
	    }
	}

	qsort(sorted_symbols, count, sizeof(struct nlist),
	      (int (*)(const void *, const void *))compare);

	for(i = 0; i < count; i++){
	    printf("  0x%08x %s\n", (unsigned int)sorted_symbols[i].n_value,
		fp->strings + sorted_symbols[i].n_un.n_strx);
	}
}

static
int
compare(
struct nlist *p1,
struct nlist *p2)
{
	if(p1->n_value > p2->n_value)
	    return(1);
	else if(p1->n_value < p2->n_value)
	    return(-1);
	else
	    return(0);
}
