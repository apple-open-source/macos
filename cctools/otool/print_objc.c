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
#include "stdio.h"
#include "string.h"
#include "mach-o/loader.h"
#include "objc/objc-runtime.h"
#include "objc/Protocol.h"
#include "stuff/allocate.h"
#include "stuff/bytesex.h"
#include "ofile_print.h"

struct objc_protocol
{
    @defs(Protocol)
};

/*
 * The header file "objc/NXString.h" has gone away and there is no real public
 * header file to get this definition from anymore.
 */
struct objc_string_object {
    struct objc_class *isa;
    char *characters;
    unsigned int _length;
};
typedef struct objc_string_object NXConstantString;

#define SIZEHASHTABLE 821
struct _hashEntry {
    struct _hashEntry *next;
    char *sel;
};

void
swap_objc_module(
struct objc_module *module,
enum byte_sex target_byte_sex)
{
	module->version = SWAP_LONG(module->version);
	module->size = SWAP_LONG(module->size);
	module->name = (char *) SWAP_LONG((long)module->name);
	module->symtab = (Symtab) SWAP_LONG((long)module->symtab);
}

void
swap_objc_symtab(
struct objc_symtab *symtab,
enum byte_sex target_byte_sex)
{
	symtab->sel_ref_cnt = SWAP_LONG(symtab->sel_ref_cnt);
	symtab->refs = (SEL *) SWAP_LONG((long)symtab->refs);
	symtab->cls_def_cnt = SWAP_SHORT(symtab->cls_def_cnt);
	symtab->cat_def_cnt = SWAP_SHORT(symtab->cat_def_cnt);
}

void
swap_objc_class(
struct objc_class *objc_class,
enum byte_sex target_byte_sex)
{
	objc_class->isa = (struct objc_class *)
		SWAP_LONG((long)objc_class->isa);
	objc_class->super_class = (struct objc_class *)
		SWAP_LONG((long)objc_class->super_class);
	objc_class->name = (const char *)
		SWAP_LONG((long)objc_class->name);		
	objc_class->version =
		SWAP_LONG(objc_class->version);
	objc_class->info =
		SWAP_LONG(objc_class->info);
	objc_class->instance_size =
		SWAP_LONG(objc_class->instance_size);
	objc_class->ivars = (struct objc_ivar_list *)
		SWAP_LONG((long)objc_class->ivars);
	objc_class->methodLists = (struct objc_method_list **)
		SWAP_LONG((long)objc_class->methodLists);
	objc_class->cache = (struct objc_cache *)
		SWAP_LONG((long)objc_class->cache);
	objc_class->protocols = (struct objc_protocol_list *)
		SWAP_LONG((long)objc_class->protocols);
}

void
swap_objc_category(
struct objc_category *objc_category,
enum byte_sex target_byte_sex)
{
	objc_category->category_name = (char *)
		SWAP_LONG((long)objc_category->category_name);
	objc_category->class_name = (char *)
		SWAP_LONG((long)objc_category->class_name);
	objc_category->instance_methods = (struct objc_method_list *)
		SWAP_LONG((long)objc_category->instance_methods);
	objc_category->class_methods = (struct objc_method_list *)
		SWAP_LONG((long)objc_category->class_methods);
	objc_category->protocols = (struct objc_protocol_list *)
		SWAP_LONG((long)objc_category->protocols);
}

void
swap_objc_ivar_list(
struct objc_ivar_list *objc_ivar_list,
enum byte_sex target_byte_sex)
{
	objc_ivar_list->ivar_count = SWAP_LONG(objc_ivar_list->ivar_count);
}

void
swap_objc_ivar(
struct objc_ivar *objc_ivar,
enum byte_sex target_byte_sex)
{
	objc_ivar->ivar_name = (char *)
		SWAP_LONG((long)objc_ivar->ivar_name);
	objc_ivar->ivar_type = (char *)
		SWAP_LONG((long)objc_ivar->ivar_type);
	objc_ivar->ivar_offset = 
		SWAP_LONG(objc_ivar->ivar_offset);
}

void
swap_objc_method_list(
struct objc_method_list *method_list,
enum byte_sex target_byte_sex)
{
	method_list->obsolete = (struct objc_method_list *)
		SWAP_LONG((long)method_list->obsolete);
	method_list->method_count = 
		SWAP_LONG(method_list->method_count);
}

void
swap_objc_method(
struct objc_method *method,
enum byte_sex target_byte_sex)
{
	method->method_name = (SEL)
		SWAP_LONG((long)method->method_name);
	method->method_types = (char *)
		SWAP_LONG((long)method->method_types);
	method->method_imp = (IMP)
		SWAP_LONG((long)method->method_imp);
}

void
swap_objc_protocol_list(
struct objc_protocol_list *protocol_list,
enum byte_sex target_byte_sex)
{
	protocol_list->next = (struct objc_protocol_list *)
		SWAP_LONG((long)protocol_list->next);
	protocol_list->count =
		SWAP_LONG(protocol_list->count);
}

void
swap_objc_protocol(
Protocol *p,
enum byte_sex target_byte_sex)
{
    struct objc_protocol *protocol;

	protocol = (struct objc_protocol *)p;

	protocol->isa = (struct objc_class *)
		SWAP_LONG((long)protocol->isa);
	protocol->protocol_name = (char *)
		SWAP_LONG((long)protocol->protocol_name);
	protocol->protocol_list = (struct objc_protocol_list *)
		SWAP_LONG((long)protocol->protocol_list);
	protocol->instance_methods = (struct objc_method_description_list *)
		SWAP_LONG((long)protocol->instance_methods);
	protocol->class_methods = (struct objc_method_description_list *)
		SWAP_LONG((long)protocol->class_methods);

}

void
swap_objc_method_description_list(
struct objc_method_description_list *mdl,
enum byte_sex target_byte_sex)
{
	mdl->count = SWAP_LONG(mdl->count);
}

void
swap_objc_method_description(
struct objc_method_description *md,
enum byte_sex target_byte_sex)
{
	md->name = (SEL)SWAP_LONG((long)md->name);
	md->types = (char *)SWAP_LONG((long)md->types);
}

void
swap_string_object(
NXConstantString *p,
enum byte_sex target_byte_sex)
{
    struct objc_string_object *string_object;

	string_object = (struct objc_string_object *)p;

	string_object->isa = (struct objc_class *)
		SWAP_LONG((long)string_object->isa);
	string_object->characters = (char *)
		SWAP_LONG((long)string_object->characters);
	string_object->_length =
		SWAP_LONG(string_object->_length);
}

void
swap_hashEntry(
struct _hashEntry *_hashEntry,
enum byte_sex target_byte_sex)
{
	_hashEntry->next = (struct _hashEntry *)
		SWAP_LONG((long)_hashEntry->next);
	_hashEntry->sel = (char *)
		SWAP_LONG((long)_hashEntry->sel);
}

struct section_info {
    struct section s;
    char *contents;
    unsigned long size;
};

static void get_objc_sections(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex object_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct section_info **objc_sections,
    unsigned long *nobjc_sections,
    char *sectname,
    char **sect,
    unsigned long *sect_addr,
    unsigned long *sect_size);

static void get_cstring_section(
    struct mach_header *mh,
    struct load_command *load_commands,
    enum byte_sex object_byte_sex,
    char *object_addr,
    unsigned long object_size,
    struct section_info *cstring_section_ptr);

static enum bool print_method_list(
    struct objc_method_list *addr,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    struct section_info *cstring_section_ptr,
    enum byte_sex host_byte_sex,
    enum bool swapped,
    struct nlist *sorted_symbols,
    unsigned long nsorted_symbols,
    enum bool verbose);

static enum bool print_protocol_list(
    unsigned long indent,
    struct objc_protocol_list *addr,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    struct section_info *cstring_section_ptr,
    enum byte_sex host_byte_sex,
    enum bool swapped,
    enum bool verbose);

static void print_protocol(
    unsigned long indent,
    struct objc_protocol *protocol,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    struct section_info *cstring_section_ptr,
    enum byte_sex host_byte_sex,
    enum bool swapped,
    enum bool verbose);

static enum bool print_method_description_list(
    unsigned long indent,
    struct objc_method_description_list *addr,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    struct section_info *cstring_section_ptr,
    enum byte_sex host_byte_sex,
    enum bool swapped,
    enum bool verbose);

static enum bool print_PHASH(
    unsigned long indent,
    struct _hashEntry *addr,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    struct section_info *cstring_section_ptr,
    enum byte_sex host_byte_sex,
    enum bool swapped,
    enum bool verbose);

static void print_indent(
    unsigned long indent);

static void *get_pointer(
    void *p,
    unsigned long *left,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    struct section_info *cstring_section_ptr);

static enum bool get_symtab(
    void *p,
    struct objc_symtab *symtab,
    void ***defs,
    unsigned long *left,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_objc_class(
    unsigned long addr,
    struct objc_class *objc_class,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_objc_category(
    unsigned long addr,
    struct objc_category *objc_category,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_ivar_list(
    void *p,
    struct objc_ivar_list *objc_ivar_list,
    struct objc_ivar **ivar_list,
    unsigned long *left,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_method_list(
    void *p,
    struct objc_method_list *method_list,
    struct objc_method **methods,
    unsigned long *left,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_protocol_list(
    void *p,
    struct objc_protocol_list *protocol_list,
    struct objc_protocol ***list,
    unsigned long *left,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_protocol(
    unsigned long addr,
    struct objc_protocol *protocol,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_method_description_list(
    void *p,
    struct objc_method_description_list *mdl,
    struct objc_method_description **list,
    unsigned long *left,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

static enum bool get_hashEntry(
    unsigned long addr,
    struct _hashEntry *_hashEntry,
    enum bool *trunc,
    struct section_info *objc_sections,
    unsigned long nobjc_sections,
    enum byte_sex host_byte_sex,
    enum bool swapped);

/*
 * Print the objc segment.
 */
void
print_objc_segment(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped, trunc;
    unsigned long i, j, left, size, defs_left, def, ivar_list_left;
    char *p;
    struct section_info *objc_sections;
    unsigned long nobjc_sections;
    struct section_info cstring_section;

    struct objc_module *modules, *m, module;
    unsigned long modules_addr, modules_size;
    struct objc_symtab symtab;
    void **defs;
    struct objc_class objc_class;
    struct objc_ivar_list objc_ivar_list;
    struct objc_ivar *ivar_list, ivar;
    struct objc_category objc_category;

	printf("Objective-C segment\n");
	get_objc_sections(mh, load_commands, object_byte_sex, object_addr,
		object_size, &objc_sections, &nobjc_sections, SECT_OBJC_MODULES,
		(char **)&modules, &modules_addr, &modules_size);

	if(modules == NULL){
	    printf("can't print objective-C information no (" SEG_OBJC ","
		   SECT_OBJC_MODULES ") section\n");
	    return;
	}

    if (verbose)
        get_cstring_section(mh, load_commands, object_byte_sex,
                            object_addr, object_size, &cstring_section);

    host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	memset(&module, '\0', sizeof(struct objc_module));

	for(m = modules;
	    (char *)m < (char *)modules + modules_size;
	    m = (struct objc_module *)((char *)m + module.size) ){

	    memset(&module, '\0', sizeof(struct objc_module));
	    left = modules_size - (m - modules); 
	    size = left < sizeof(struct objc_module) ?
		   left : sizeof(struct objc_module);
	    memcpy(&module, m, size);
	    if(swapped)
		swap_objc_module(&module, host_byte_sex);

	    if((char *)m + module.size > (char *)m + modules_size)
		printf("module extends past end of " SECT_OBJC_MODULES
		       " section\n");
	    printf("Module 0x%x\n", (unsigned int)
		   (modules_addr + (char *)m - (char *)modules));

	    printf("    version %lu\n", module.version);
	    printf("       size %lu\n", module.size);
	    if(verbose){
		p = get_pointer((void *)module.name, &left,
		    objc_sections, nobjc_sections, &cstring_section);
		if(p != NULL)
		    printf("       name %.*s\n", (int)left, p);
		else
		    printf("       name 0x%08x (not in an " SEG_OBJC
			   " section)\n", (unsigned int)module.name);
	    }
	    else
		printf("       name 0x%08x\n", (unsigned int)(module.name));

	    if(get_symtab(module.symtab, &symtab, &defs, &defs_left, &trunc,
		    objc_sections, nobjc_sections,
		    host_byte_sex, swapped) == FALSE){
		printf("     symtab 0x%08x (not in an " SEG_OBJC
		       " section)\n", (unsigned int)module.symtab);
		continue;
	    }
	    printf("     symtab 0x%08x\n", (unsigned int)module.symtab);
	    if(trunc == TRUE)
		printf("\tsymtab extends past end of an " SEG_OBJC
		       " section\n");
	    printf("\tsel_ref_cnt %lu\n", symtab.sel_ref_cnt);
	    p = get_pointer(symtab.refs, &left,
                     objc_sections, nobjc_sections, &cstring_section);
	    if(p != NULL)
		printf("\trefs 0x%08x", (unsigned int)symtab.refs);
	    else
		printf("\trefs 0x%08x (not in an " SEG_OBJC " section)\n",
		       (unsigned int)symtab.refs);

	    printf("\tcls_def_cnt %d\n", symtab.cls_def_cnt);
	    printf("\tcat_def_cnt %d\n", symtab.cat_def_cnt);
	    if(symtab.cls_def_cnt > 0)
		printf("\tClass Definitions\n");
	    for(i = 0; i < symtab.cls_def_cnt; i++){
		if((i + 1) * sizeof(void *) > defs_left){
		    printf("\t(remaining class defs entries entends past "
			   "the end of the section)\n");
		    break;
		}
	    
		memcpy(&def, defs + i, sizeof(void *));
		if(swapped)
		    def = SWAP_LONG(def);

		if(get_objc_class(def, &objc_class, &trunc, objc_sections,
			  nobjc_sections, host_byte_sex, swapped) == TRUE){
		    printf("\tdefs[%lu] 0x%08x", i, (unsigned int)def);
print_objc_class:
		    if(trunc == TRUE)
			printf(" (entends past the end of the section)\n");
		    else
			printf("\n");
		    printf("\t\t      isa 0x%08x",
			   (unsigned int)objc_class.isa);

		    if(verbose && CLS_GETINFO(&objc_class, CLS_META)){
			p = get_pointer(objc_class.isa, &left,
					objc_sections, nobjc_sections, &cstring_section);
			if(p != NULL)
			    printf(" %.*s\n", (int)left, p);
			else
			    printf(" (not in an " SEG_OBJC " section)\n");
		    }
		    else
			printf("\n");

		    printf("\t      super_class 0x%08x",
			   (unsigned int)objc_class.super_class);
		    if(verbose){
			p = get_pointer(objc_class.super_class, &left,
					objc_sections, nobjc_sections, &cstring_section);
			if(p != NULL)
			    printf(" %.*s\n", (int)left, p);
			else
			    printf(" (not in an " SEG_OBJC " section)\n");
		    }
		    else
			printf("\n");

		    printf("\t\t     name 0x%08x",
			   (unsigned int)objc_class.name);
		    if(verbose){
			p = get_pointer((void *)objc_class.name, &left,
					objc_sections, nobjc_sections, &cstring_section);
			if(p != NULL)
			    printf(" %.*s\n", (int)left, p);
			else
			    printf(" (not in an " SEG_OBJC " section)\n");
		    }
		    else
			printf("\n");
		    printf("\t\t  version 0x%08x\n",
			   (unsigned int)objc_class.version);
		    printf("\t\t     info 0x%08x",
			   (unsigned int)objc_class.info);
		    if(verbose){
			if(CLS_GETINFO(&objc_class, CLS_CLASS))
			    printf(" CLS_CLASS\n");
			else if(CLS_GETINFO(&objc_class, CLS_META))
			    printf(" CLS_META\n");
			else
			    printf("\n");
		    }
		    else
			printf("\n");
		    printf("\t    instance_size 0x%08x\n",
			   (unsigned int)objc_class.instance_size);

		    if(get_ivar_list(objc_class.ivars, &objc_ivar_list,
			    &ivar_list, &ivar_list_left, &trunc,
			    objc_sections, nobjc_sections,
                host_byte_sex, swapped) == TRUE){
			printf("\t\t    ivars 0x%08x\n",
			       (unsigned int)objc_class.ivars);
			if(trunc == TRUE)
			    printf("\t\t objc_ivar_list extends past end "
				   "of " SECT_OBJC_SYMBOLS " section\n");
			printf("\t\t       ivar_count %d\n", 
				    objc_ivar_list.ivar_count);
			for(j = 0; j < objc_ivar_list.ivar_count; j++){
			    if((j + 1) * sizeof(struct objc_ivar) >
			       ivar_list_left){
				printf("\t\t remaining ivar's extend past "
				       " the of the section\n");
				continue;
			    }
			    memcpy(&ivar, ivar_list + j,
				   sizeof(struct objc_ivar));
			    if(swapped)
				swap_objc_ivar(&ivar, host_byte_sex);

			    printf("\t\t\tivar_name 0x%08x",
				   (unsigned int)ivar.ivar_name);
			    if(verbose){
				p = get_pointer(ivar.ivar_name, &left,
					    objc_sections, nobjc_sections, &cstring_section);
				if(p != NULL)
				    printf(" %.*s\n", (int)left, p);
				else
				    printf(" (not in an " SEG_OBJC
					   " section)\n");
			    }
			    else
				printf("\n");
			    printf("\t\t\tivar_type 0x%08x",
				   (unsigned int)ivar.ivar_type);
			    if(verbose){
				p = get_pointer(ivar.ivar_type, &left,
					    objc_sections, nobjc_sections, &cstring_section);
				if(p != NULL)
				    printf(" %.*s\n", (int)left, p);
				else
				    printf(" (not in an " SEG_OBJC
					   " section)\n");
			    }
			    else
				printf("\n");
			    printf("\t\t      ivar_offset 0x%08x\n",
				   (unsigned int)ivar.ivar_offset);
			}
		    }
		    else{
			printf("\t\t    ivars 0x%08x (not in an " SEG_OBJC
			       " section)\n",
			       (unsigned int)objc_class.ivars);
		    }

		    printf("\t\t  methods 0x%08x",
			   (unsigned int)objc_class.methodLists);
		    if(print_method_list((struct objc_method_list *)
					 objc_class.methodLists,
					 objc_sections, nobjc_sections, &cstring_section,
					 host_byte_sex, swapped, sorted_symbols,
					 nsorted_symbols, verbose) == FALSE)
			printf(" (not in an " SEG_OBJC " section)\n");

		    printf("\t\t    cache 0x%08x\n",
			   (unsigned int)objc_class.cache);

		    printf("\t\tprotocols 0x%08x",
			   (unsigned int)objc_class.protocols);
		    if(print_protocol_list(16, objc_class.protocols,
			    objc_sections, nobjc_sections, &cstring_section,
                host_byte_sex, swapped, verbose) == FALSE)
			printf(" (not in an " SEG_OBJC " section)\n");

		    if(CLS_GETINFO((&objc_class), CLS_CLASS)){
			printf("\tMeta Class");
			if(get_objc_class((unsigned long)objc_class.isa,
				 &objc_class, &trunc, objc_sections, nobjc_sections,
				 host_byte_sex, swapped) == TRUE){
			    goto print_objc_class;
			}
			else
			    printf(" (not in " SECT_OBJC_SYMBOLS
				   " section)\n");
		    }
		}
		else
		    printf("\tdefs[%lu] 0x%08x (not in an " SEG_OBJC
			   " section)\n", i, (unsigned int)def);
	    }
	    if(symtab.cat_def_cnt > 0)
		printf("\tCategory Definitions\n");
	    for(i = 0; i < symtab.cat_def_cnt; i++){
		if((i + symtab.cls_def_cnt + 1) * sizeof(void *) >
							      defs_left){
		    printf("\t(remaining category defs entries entends "
			   "past the end of the section)\n");
		    break;
		}
	    
		memcpy(&def, defs + i + symtab.cls_def_cnt, sizeof(void *));
		if(swapped)
		    def = SWAP_LONG(def);

		if(get_objc_category(def, &objc_category, &trunc,
			  objc_sections, nobjc_sections,
              host_byte_sex, swapped) == TRUE){
		    printf("\tdefs[%lu] 0x%08x", i + symtab.cls_def_cnt,
			   (unsigned int)def);
		    if(trunc == TRUE)
			printf(" (entends past the end of the section)\n");
		    else
			printf("\n");
		    printf("\t       category name 0x%08x",
			   (unsigned int)objc_category.category_name);
		    if(verbose){
			p = get_pointer(objc_category.category_name, &left,
					objc_sections, nobjc_sections, &cstring_section);
			if(p != NULL)
			    printf(" %.*s\n", (int)left, p);
			else
			    printf(" (not in an " SEG_OBJC " section)\n");
		    }
		    else
			printf("\n");

		    printf("\t\t  class name 0x%08x",
			   (unsigned int)objc_category.class_name);
		    if(verbose){
			p = get_pointer(objc_category.class_name, &left,
					objc_sections, nobjc_sections, &cstring_section);
			if(p != NULL)
			    printf(" %.*s\n", (int)left, p);
			else
			    printf(" (not in an " SEG_OBJC " section)\n");
		    }
		    else
			printf("\n");

		    printf("\t    instance methods 0x%08x",
			   (unsigned int)objc_category.instance_methods);
		    if(print_method_list(objc_category.instance_methods,
					 objc_sections, nobjc_sections, &cstring_section,
					 host_byte_sex, swapped,
					 sorted_symbols, nsorted_symbols,
					 verbose) == FALSE)
			printf(" (not in an " SEG_OBJC " section)\n");

		    printf("\t       class methods 0x%08x",
			   (unsigned int)objc_category.class_methods);
		    if(print_method_list(objc_category.class_methods,
					 objc_sections, nobjc_sections, &cstring_section,
					 host_byte_sex, swapped,
					 sorted_symbols, nsorted_symbols,
					 verbose) == FALSE)
			printf(" (not in an " SEG_OBJC " section)\n");
		}
		else
		    printf("\tdefs[%lu] 0x%08x (not in an " SEG_OBJC
			   " section)\n", i + symtab.cls_def_cnt,
			   (unsigned int)def);
	    }
	}
}

void
print_objc_protocol_section(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    struct section_info *objc_sections, cstring_section;
    unsigned long nobjc_sections;
    struct objc_protocol *protocols, *p, protocol;
    unsigned long protocols_addr, protocols_size;
    unsigned long size, left;

	printf("Contents of (" SEG_OBJC ",__protocol) section\n");
	get_objc_sections(mh, load_commands, object_byte_sex, object_addr,
		object_size, &objc_sections, &nobjc_sections, "__protocol",
		(char **)&protocols, &protocols_addr, &protocols_size);

    if (verbose)
        get_cstring_section(mh, load_commands, object_byte_sex,
                            object_addr, object_size, &cstring_section);

    host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	for(p = protocols; (char *)p < (char *)protocols + protocols_size; p++){

	    memset(&protocol, '\0', sizeof(struct objc_protocol));
	    left = protocols_size - (p - protocols); 
	    size = left < sizeof(struct objc_protocol) ?
		   left : sizeof(struct objc_protocol);
	    memcpy(&protocol, p, size);

	    if((char *)p + sizeof(struct objc_protocol) >
	       (char *)p + protocols_size)
		printf("Protocol extends past end of __protocol section\n");
	    printf("Protocol 0x%x\n", (unsigned int)
		   (protocols_addr + (char *)p - (char *)protocols));

	    print_protocol(0, &protocol,
			      objc_sections, nobjc_sections, &cstring_section,
			      host_byte_sex, swapped, verbose);
	}
}

void
print_objc_string_object_section(
char *sectname,
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    struct section_info *objc_sections;
    unsigned long nobjc_sections;
    struct section_info cstring_section;
    struct objc_string_object *string_objects, *s, string_object;
    unsigned long string_objects_addr, string_objects_size;
    unsigned long size, left;
    char *p;

	printf("Contents of (" SEG_OBJC ",%s) section\n", sectname);
	get_objc_sections(mh, load_commands, object_byte_sex, object_addr,
		object_size, &objc_sections, &nobjc_sections, sectname,
		(char **)&string_objects, &string_objects_addr,
		&string_objects_size);

	get_cstring_section(mh, load_commands, object_byte_sex, object_addr,
    		object_size, &cstring_section);

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	for(s = string_objects;
	    (char *)s < (char *)string_objects + string_objects_size;
	    s++){

	    memset(&string_object, '\0', sizeof(struct objc_string_object));
	    left = string_objects_size - (s - string_objects); 
	    size = left < sizeof(struct objc_string_object) ?
		   left : sizeof(struct objc_string_object);
	    memcpy(&string_object, s, size);

	    if((char *)s + sizeof(struct objc_string_object) >
	       (char *)s + string_objects_size)
		printf("String Object extends past end of %s section\n",
		       sectname);
	    printf("String Object 0x%x\n", (unsigned int)
		   (string_objects_addr + (char *)s - (char *)string_objects));

	    if(swapped)
		swap_string_object((NXConstantString *)&string_object,
			           host_byte_sex);
	    printf("           isa 0x%x\n", (unsigned int)string_object.isa);
	    printf("    characters 0x%x",
		   (unsigned int)string_object.characters);
	    if(verbose){
		p = get_pointer(string_object.characters, &left,
			        &cstring_section, 1, &cstring_section);
		if(p != NULL)
		    printf(" %.*s\n", (int)left, p);
		else
		    printf(" (not in the (" SEG_TEXT ",__cstring) section)\n");
	    }
	    else
		printf("\n");
	    printf("       _length %u\n", string_object._length);
	}
}

/*
 * PHASH[SIZEHASHTABLE];
 * HASH[?]; variable sized (computed from size of section).
 */
void
print_objc_runtime_setup_section(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
enum bool verbose)
{

    enum byte_sex host_byte_sex;
    enum bool swapped;
    struct section_info *objc_sections, cstring_section;
    unsigned long i, nobjc_sections, left;
    struct _hashEntry **PHASH, *phash, *HASH, _hashEntry;
    char *sect, *p;
    unsigned long sect_addr, sect_size;

	printf("Contents of (" SEG_OBJC ",__runtime_setup) section\n");
	get_objc_sections(mh, load_commands, object_byte_sex, object_addr,
		object_size, &objc_sections, &nobjc_sections, "__runtime_setup",
		&sect, &sect_addr, &sect_size);

    if (verbose)
        get_cstring_section(mh, load_commands, object_byte_sex,
                            object_addr, object_size, &cstring_section);

    host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	PHASH = (struct _hashEntry **)sect;
	for(i = 0;
	    i < SIZEHASHTABLE &&
		(i + 1) * sizeof(struct _hashEntry *) < sect_size;
	    i++){

	    memcpy(&phash, PHASH + i, sizeof(struct _hashEntry *));
	    if(swapped)
		phash = (struct _hashEntry *)SWAP_LONG((long)phash);

	    if(phash == 0)
		continue;

	    printf("PHASH[%3lu] 0x%x", i, (unsigned int)phash);
	    if(print_PHASH(4, phash,
			   objc_sections, nobjc_sections, &cstring_section,
			   host_byte_sex, swapped, verbose) == FALSE)
		printf(" (not in an " SEG_OBJC " section)\n");
	}

	HASH = (struct _hashEntry *)(PHASH + SIZEHASHTABLE);
	for(i = 0; (char *)(HASH + i) < sect + sect_size; i++){
	    memcpy((char *)&_hashEntry, HASH + i, sizeof(struct _hashEntry));
	    if(swapped)
		swap_hashEntry(&_hashEntry, host_byte_sex);

	    printf("HASH at 0x%08x\n",
		   (unsigned int)(sect_addr + (char *)(HASH + i) - sect));
	    printf("     sel 0x%08x", (unsigned int)_hashEntry.sel);
	    if(verbose){
		p = get_pointer(_hashEntry.sel, &left,
				objc_sections, nobjc_sections, &cstring_section);
		if(p != NULL)
		    printf(" %.*s\n", (int)left, p);
		else
		    printf(" (not in an " SEG_OBJC " section)\n");
	    }
	    else
		printf("\n");

	    printf("    next 0x%08x", (unsigned int)_hashEntry.next);
	    if(_hashEntry.next == NULL){
		printf("\n");
	    }
	    else{
		if((unsigned long)_hashEntry.next < sect_addr || 
		   (unsigned long)_hashEntry.next >= sect_addr + sect_size)
		    printf(" (not in the ("SEG_OBJC ",__runtime_setup "
			   "section)\n");
		else
		    printf("\n");
	    }
	}
}

static
void
get_objc_sections(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
struct section_info **objc_sections,
unsigned long *nobjc_sections,
char *sectname,
char **sect,
unsigned long *sect_addr,
unsigned long *sect_size)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;

    unsigned long i, j, left, size;
    struct load_command lcmd, *lc;
    char *p;
    struct segment_command sg;
    struct section s;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	*objc_sections = NULL;
	*nobjc_sections = 0;
	*sect = NULL;
	*sect_addr = 0;
	*sect_size = 0;

	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&lcmd, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&lcmd, host_byte_sex);
	    if(lcmd.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + lcmd.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(lcmd.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		p = (char *)lc + sizeof(struct segment_command);
		for(j = 0 ; j < sg.nsects ; j++){
		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds){
			printf("section structure command extends past "
			       "end of load commands\n");
		    }
		    left = mh->sizeofcmds - (p - (char *)load_commands);
		    memset((char *)&s, '\0', sizeof(struct section));
		    size = left < sizeof(struct section) ?
			   left : sizeof(struct section);
		    memcpy((char *)&s, p, size);
		    if(swapped)
			swap_section(&s, 1, host_byte_sex);

		    if(strcmp(s.segname, SEG_OBJC) == 0){
			*objc_sections = reallocate(*objc_sections,
			   sizeof(struct section_info) * (*nobjc_sections + 1));
			(*objc_sections)[*nobjc_sections].s = s;
			(*objc_sections)[*nobjc_sections].contents = 
							 object_addr + s.offset;
			if(s.offset > object_size){
			    printf("section contents of: (%.16s,%.16s) is past "
				   "end of file\n", s.segname, s.sectname);
			    (*objc_sections)[*nobjc_sections].size =  0;
			}
			else if(s.offset + s.size > object_size){
			    printf("part of section contents of: (%.16s,%.16s) "
				   "is past end of file\n",
				   s.segname, s.sectname);
			    (*objc_sections)[*nobjc_sections].size =
				object_size - s.offset;
			}
			else
			    (*objc_sections)[*nobjc_sections].size = s.size;

			if(strncmp(s.sectname, sectname, 16) == 0){
			    if(*sect != NULL)
				printf("more than one (" SEG_OBJC ",%s) "
				       "section\n", sectname);
			    else{
				*sect = (object_addr + s.offset);
				*sect_size = s.size;
				*sect_addr = s.addr;
			    }
			}
			(*nobjc_sections)++;
		    }

		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds)
			break;
		    p += size;
		}
		break;
	    }
	    if(lcmd.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lcmd.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}

#ifdef notdef
	printf("*nobjc_sections %lu\n", *nobjc_sections);
	for(i = 0; i < *nobjc_sections; i++){
	    printf("(%.16s,%.16s) addr = 0x%x size = 0x%x\n",
		(*objc_sections)[i].s.segname,
		(*objc_sections)[i].s.sectname,
		(unsigned int)((*objc_sections)[i].s.addr),
		(unsigned int)((*objc_sections)[i].size));
	}
#endif /* notdef */
}

static
void
get_cstring_section(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
struct section_info *cstring_section)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;

    unsigned long i, j, left, size;
    struct load_command lcmd, *lc;
    char *p;
    struct segment_command sg;
    struct section s;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	memset(cstring_section, '\0', sizeof(struct section_info));

	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&lcmd, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&lcmd, host_byte_sex);
	    if(lcmd.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + lcmd.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(lcmd.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		p = (char *)lc + sizeof(struct segment_command);
		for(j = 0 ; j < sg.nsects ; j++){
		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds){
			printf("section structure command extends past "
			       "end of load commands\n");
		    }
		    left = mh->sizeofcmds - (p - (char *)load_commands);
		    memset((char *)&s, '\0', sizeof(struct section));
		    size = left < sizeof(struct section) ?
			   left : sizeof(struct section);
		    memcpy((char *)&s, p, size);
		    if(swapped)
			swap_section(&s, 1, host_byte_sex);

		    if(strcmp(s.segname, SEG_TEXT) == 0 &&
		       strcmp(s.sectname, "__cstring") == 0){
			cstring_section->s = s;
			cstring_section->contents = object_addr + s.offset;
			if(s.offset > object_size){
			    printf("section contents of: (%.16s,%.16s) is past "
				   "end of file\n", s.segname, s.sectname);
			    cstring_section->size = 0;
			}
			else if(s.offset + s.size > object_size){
			    printf("part of section contents of: (%.16s,%.16s) "
				   "is past end of file\n",
				   s.segname, s.sectname);
			    cstring_section->size = object_size - s.offset;
			}
			else
			    cstring_section->size = s.size;
			return;
		    }

		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds)
			break;
		    p += size;
		}
		break;
	    }
	    if(lcmd.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + lcmd.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
}

static
enum bool
print_method_list(
struct objc_method_list *addr,
struct section_info *objc_sections,
unsigned long nobjc_sections,
struct section_info *cstring_section_ptr,
enum byte_sex host_byte_sex,
enum bool swapped,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols,
enum bool verbose)
{
    struct objc_method *methods, method;
    struct objc_method_list method_list;
    enum bool trunc;
    unsigned long i, methods_left, left;
    char *p;

	if(get_method_list(addr, &method_list, &methods, &methods_left, &trunc,
			   objc_sections, nobjc_sections,
			   host_byte_sex, swapped) == FALSE)
	    return(FALSE);
	
	printf("\n");
	if(trunc == TRUE)
	    printf("\t\t objc_method_list extends past end of the section\n");

	printf("\t\t         obsolete 0x%08x\n",
	       (unsigned int)method_list.obsolete);
	printf("\t\t     method_count %d\n",
	       method_list.method_count);
	
	for(i = 0; i < method_list.method_count; i++){
	    if((i + 1) * sizeof(struct objc_method) > methods_left){
		printf("\t\t remaining method's extend past the of the "
		       "section\n");
		continue;
	    }
	    memcpy(&method, methods + i, sizeof(struct objc_method));
	    if(swapped)
		swap_objc_method(&method, host_byte_sex);

	    printf("\t\t      method_name 0x%08x",
		   (unsigned int)method.method_name);
	    if(verbose){
		p = get_pointer(method.method_name, &left,
			        objc_sections, nobjc_sections, cstring_section_ptr);
		if(p != NULL)
		    printf(" %.*s\n", (int)left, p);
		else
		    printf(" (not in an " SEG_OBJC " section)\n");
	    }
	    else
		printf("\n");

	    printf("\t\t     method_types 0x%08x",
		   (unsigned int)method.method_types);
	    if(verbose){
		p = get_pointer(method.method_types, &left,
			        objc_sections, nobjc_sections, cstring_section_ptr);
		if(p != NULL)
		    printf(" %.*s\n", (int)left, p);
		else
		    printf(" (not in an " SEG_OBJC " section)\n");
	    }
	    else
		printf("\n");
	    printf("\t\t       method_imp 0x%08x ",
		   (unsigned int)method.method_imp);
	    if(verbose)
		print_label((long)method.method_imp, FALSE, sorted_symbols,
    			    nsorted_symbols);
	    printf("\n");
	}
	return(TRUE);
}

static
enum bool
print_protocol_list(
unsigned long indent,
struct objc_protocol_list *addr,
struct section_info *objc_sections,
unsigned long nobjc_sections,
struct section_info *cstring_section_ptr,
enum byte_sex host_byte_sex,
enum bool swapped,
enum bool verbose)
{
    struct objc_protocol **list, *l, protocol;
    struct objc_protocol_list protocol_list;
    enum bool trunc;
    unsigned long i, list_left;

	if(get_protocol_list(addr, &protocol_list, &list, &list_left, &trunc,
			     objc_sections, nobjc_sections,
			     host_byte_sex, swapped) == FALSE)
	    return(FALSE);

	printf("\n");
	if(trunc == TRUE){
	    print_indent(indent);
	    printf(" objc_protocol_list extends past end of the section\n");
	}

	print_indent(indent);
	printf("         next 0x%08x\n",
	       (unsigned int)protocol_list.next);
	print_indent(indent);
	printf("        count %d\n",
	       protocol_list.count);
	
	for(i = 0; i < protocol_list.count; i++){
	    if((i + 1) * sizeof(struct objc_protocol *) > list_left){
		print_indent(indent);
		printf(" remaining list entries extend past the of the "
		       "section\n");
		continue;
	    }
	    memcpy(&l, list + i, sizeof(struct objc_protocol *));
	    if(swapped)
		l = (struct objc_protocol *)SWAP_LONG((long)l);

	    print_indent(indent);
	    printf("      list[%lu] 0x%08x", i, (unsigned int)l);
	    if(get_protocol((unsigned long)l, &protocol, &trunc,
			    objc_sections, nobjc_sections,
			    host_byte_sex, swapped) == FALSE){
		printf(" (not in an " SEG_OBJC " section)\n");
		continue;
	    }
	    printf("\n");
	    if(trunc == TRUE){
		print_indent(indent);
		printf("            Protocol extends past end of the "
		       "section\n");
	    }

	    if(swapped)
		swap_objc_protocol((Protocol *)&protocol, host_byte_sex);

	    print_protocol(indent, &protocol,
			   objc_sections, nobjc_sections, cstring_section_ptr,
			   host_byte_sex, swapped, verbose);
	}
	return(TRUE);
}

static
void
print_protocol(
unsigned long indent,
struct objc_protocol *protocol,
struct section_info *objc_sections,
unsigned long nobjc_sections,
struct section_info *cstring_section_ptr,
enum byte_sex host_byte_sex,
enum bool swapped,
enum bool verbose)
{
    unsigned long left;
    char *p;

	print_indent(indent);
	printf("              isa 0x%08x\n",
	       (unsigned int)protocol->isa);
	print_indent(indent);
	printf("    protocol_name 0x%08x",
	       (unsigned int)protocol->protocol_name);
	if(verbose){
	    p = get_pointer(protocol->protocol_name, &left,
			    objc_sections, nobjc_sections, cstring_section_ptr);
	    if(p != NULL)
		printf(" %.*s\n", (int)left, p);
	    else
		printf(" (not in an " SEG_OBJC " section)\n");
	}
	else
	    printf("\n");
	print_indent(indent);
	printf("    protocol_list 0x%08x",
	       (unsigned int)protocol->protocol_list);
	if(print_protocol_list(indent + 4, protocol->protocol_list,
		objc_sections, nobjc_sections, cstring_section_ptr,
		host_byte_sex, swapped, verbose) == FALSE)
	    printf(" (not in an " SEG_OBJC " section)\n");

	print_indent(indent);
	printf(" instance_methods 0x%08x",
	       (unsigned int)protocol->instance_methods);
	if(print_method_description_list(indent, protocol->instance_methods,
		objc_sections, nobjc_sections, cstring_section_ptr,
		host_byte_sex, swapped, verbose) == FALSE)
	    printf(" (not in an " SEG_OBJC " section)\n");

	print_indent(indent);
	printf("    class_methods 0x%08x",
	       (unsigned int)protocol->class_methods);
	if(print_method_description_list(indent, protocol->class_methods,
		objc_sections, nobjc_sections, cstring_section_ptr,
		host_byte_sex, swapped, verbose) == FALSE)
	    printf(" (not in an " SEG_OBJC " section)\n");
}

static
enum bool
print_method_description_list(
unsigned long indent,
struct objc_method_description_list *addr,
struct section_info *objc_sections,
unsigned long nobjc_sections,
struct section_info *cstring_section_ptr,
enum byte_sex host_byte_sex,
enum bool swapped,
enum bool verbose)
{
    struct objc_method_description_list mdl;
    struct objc_method_description *list, md;
    enum bool trunc;
    unsigned long i, list_left, left;
    char *p;

	if(get_method_description_list(addr, &mdl, &list, &list_left,
	    &trunc, objc_sections, nobjc_sections,
	    host_byte_sex, swapped) == FALSE)
	    return(FALSE);

	printf("\n");
	if(trunc == TRUE){
	    print_indent(indent);
	    printf(" objc_method_description_list extends past end of the "
		   "section\n");
	}

	print_indent(indent);
	printf("        count %d\n", mdl.count);
	
	for(i = 0; i < mdl.count; i++){
	    if((i + 1) * sizeof(struct objc_method_description) > list_left){
		print_indent(indent);
		printf(" remaining list entries extend past the of the "
		       "section\n");
		continue;
	    }
	    print_indent(indent);
	    printf("        list[%lu]\n", i);
	    memcpy(&md, list + i, sizeof(struct objc_method_description));
	    if(swapped)
		swap_objc_method_description(&md, host_byte_sex);

	    print_indent(indent);
	    printf("             name 0x%08x", (unsigned int)md.name);
	    if(verbose){
		p = get_pointer(md.name, &left,
		    objc_sections, nobjc_sections, cstring_section_ptr);
		if(p != NULL)
		    printf(" %.*s\n", (int)left, p);
		else
		    printf(" (not in an " SEG_OBJC " section)\n");
	    }
	    else
		printf("\n");

	    print_indent(indent);
	    printf("            types 0x%08x", (unsigned int)md.types);
	    if(verbose){
		p = get_pointer(md.types, &left,
		    objc_sections, nobjc_sections, cstring_section_ptr);
		if(p != NULL)
		    printf(" %.*s\n", (int)left, p);
		else
		    printf(" (not in an " SEG_OBJC " section)\n");
	    }
	    else
		printf("\n");
	}
	return(TRUE);
}

static enum bool
print_PHASH(
unsigned long indent,
struct _hashEntry *addr,
struct section_info *objc_sections,
unsigned long nobjc_sections,
struct section_info *cstring_section_ptr,
enum byte_sex host_byte_sex,
enum bool swapped,
enum bool verbose)
{
    struct _hashEntry _hashEntry;
    enum bool trunc;
    unsigned long left;
    char *p;

	if(get_hashEntry((unsigned long)addr, &_hashEntry, &trunc,
			 objc_sections, nobjc_sections,
			 host_byte_sex, swapped) == FALSE)
	    return(FALSE);

	printf("\n");
	if(trunc == TRUE){
	    print_indent(indent);
	    printf("_hashEntry extends past end of the section\n");
	}

	print_indent(indent);
	printf(" sel 0x%08x", (unsigned int)_hashEntry.sel);
	if(verbose){
	    p = get_pointer(_hashEntry.sel, &left,
    	    objc_sections, nobjc_sections, cstring_section_ptr);
	    if(p != NULL)
		printf(" %.*s\n", (int)left, p);
	    else
		printf(" (not in an " SEG_OBJC " section)\n");
	}
	else
	    printf("\n");

	print_indent(indent);
	printf("next 0x%08x", (unsigned int)_hashEntry.next);
	if(_hashEntry.next == NULL){
	    printf("\n");
	    return(TRUE);
	}
	if(print_PHASH(indent+4, _hashEntry.next,
	    objc_sections, nobjc_sections, cstring_section_ptr,
	    host_byte_sex, swapped, verbose) == FALSE)
	    printf(" (not in an " SEG_OBJC " section)\n");
	return(TRUE);
}

static
void
print_indent(
unsigned long indent)
{
     unsigned long i;
	
	for(i = 0; i < indent; ){
	    if(indent - i >= 8){
		printf("\t");
		i += 8;
	    }
	    else{
		printf("%.*s", (int)(indent - i), "        ");
		return;
	    }
	}
}

static
void *
get_pointer(
void *p,
unsigned long *left,
struct section_info *objc_sections,
unsigned long nobjc_sections,
struct section_info *cstring_section_ptr)
{
    void* returnValue = NULL;
    unsigned long i, addr;

    addr = (unsigned long)p;
    if(addr >= cstring_section_ptr->s.addr &&
       addr < cstring_section_ptr->s.addr + cstring_section_ptr->size){
        *left = cstring_section_ptr->size -
        (addr - cstring_section_ptr->s.addr);
        returnValue = (cstring_section_ptr->contents +
                       (addr - cstring_section_ptr->s.addr));
    }
	for(i = 0; !returnValue && i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){
		*left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		returnValue = (objc_sections[i].contents +
		       (addr - objc_sections[i].s.addr));
	    }
	}
	return returnValue;
}

static
enum bool
get_symtab(
void *p,
struct objc_symtab *symtab,
void ***defs,
unsigned long *left,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long addr, i;

	addr = (unsigned long)p;
	memset(symtab, '\0', sizeof(struct objc_symtab));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		*left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(*left >= sizeof(struct objc_symtab) - sizeof(void *)){
		    memcpy(symtab,
			   objc_sections[i].contents +
			       (addr - objc_sections[i].s.addr),
			   sizeof(struct objc_symtab) - sizeof(void *));
		    *left -= sizeof(struct objc_symtab) - sizeof(void *);
		    *defs = (void **)(objc_sections[i].contents +
				     (addr - objc_sections[i].s.addr) +
			   	     sizeof(struct objc_symtab)-sizeof(void *));
		    *trunc = FALSE;
		}
		else{
		    memcpy(symtab,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   *left);
		    *left = 0;
		    *defs = NULL;
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_symtab(symtab, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_objc_class(
unsigned long addr,
struct objc_class *objc_class,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long left, i;

	memset(objc_class, '\0', sizeof(struct objc_class));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(left >= sizeof(struct objc_class)){
		    memcpy(objc_class,
			   objc_sections[i].contents +
			       (addr - objc_sections[i].s.addr),
			   sizeof(struct objc_class));
		    *trunc = FALSE;
		}
		else{
		    memcpy(objc_class,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   left);
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_class(objc_class, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_objc_category(
unsigned long addr,
struct objc_category *objc_category,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long left, i;

	memset(objc_category, '\0', sizeof(struct objc_category));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(left >= sizeof(struct objc_category)){
		    memcpy(objc_category,
			   objc_sections[i].contents +
			       (addr - objc_sections[i].s.addr),
			   sizeof(struct objc_category));
		    *trunc = FALSE;
		}
		else{
		    memcpy(objc_category,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   left);
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_category(objc_category, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_ivar_list(
void *p,
struct objc_ivar_list *objc_ivar_list,
struct objc_ivar **ivar_list,
unsigned long *left,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long addr, i;

	addr = (unsigned long)p;
	memset(objc_ivar_list, '\0', sizeof(struct objc_ivar_list));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		*left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(*left >= sizeof(struct objc_ivar_list) -
			    sizeof(struct objc_ivar)){
		    memcpy(objc_ivar_list,
			   objc_sections[i].contents +
				(addr - objc_sections[i].s.addr),
			   sizeof(struct objc_ivar_list) -
				sizeof(struct objc_ivar));
		    *left -= sizeof(struct objc_ivar_list) -
			     sizeof(struct objc_ivar);
		    *ivar_list = (struct objc_ivar *)
				 (objc_sections[i].contents +
				     (addr - objc_sections[i].s.addr) +
				  sizeof(struct objc_ivar_list) -
				      sizeof(struct objc_ivar));
		    *trunc = FALSE;
		}
		else{
		    memcpy(objc_ivar_list,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   *left);
		    *left = 0;
		    *ivar_list = NULL;
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_ivar_list(objc_ivar_list, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_method_list(
void *p,
struct objc_method_list *method_list,
struct objc_method **methods,
unsigned long *left,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long addr, i;

	addr = (unsigned long)p;
	memset(method_list, '\0', sizeof(struct objc_method_list));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		*left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(*left >= sizeof(struct objc_method_list) -
			    sizeof(struct objc_method)){
		    memcpy(method_list,
			   objc_sections[i].contents +
				(addr - objc_sections[i].s.addr),
			   sizeof(struct objc_method_list) -
				sizeof(struct objc_method));
		    *left -= sizeof(struct objc_method_list) -
			     sizeof(struct objc_method);
		    *methods = (struct objc_method *)
				 (objc_sections[i].contents +
				     (addr - objc_sections[i].s.addr) +
				  sizeof(struct objc_method_list) -
				      sizeof(struct objc_method));
		    *trunc = FALSE;
		}
		else{
		    memcpy(method_list,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   *left);
		    *left = 0;
		    *methods = NULL;
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_method_list(method_list, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_protocol_list(
void *p,
struct objc_protocol_list *protocol_list,
struct objc_protocol ***list,
unsigned long *left,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long addr, i;

	addr = (unsigned long)p;
	memset(protocol_list, '\0', sizeof(struct objc_protocol_list));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		*left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(*left >= sizeof(struct objc_protocol_list) -
			    sizeof(struct objc_protocol *)){
		    memcpy(protocol_list,
			   objc_sections[i].contents +
				(addr - objc_sections[i].s.addr),
			   sizeof(struct objc_protocol_list) -
				sizeof(struct objc_protocol *));
		    *left -= sizeof(struct objc_protocol_list) -
			     sizeof(struct objc_protocol *);
		    *list = (struct objc_protocol **)
				 (objc_sections[i].contents +
				     (addr - objc_sections[i].s.addr) +
				  sizeof(struct objc_protocol_list) -
				      sizeof(struct objc_protocol **));
		    *trunc = FALSE;
		}
		else{
		    memcpy(protocol_list,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   *left);
		    *left = 0;
		    *list = NULL;
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_protocol_list(protocol_list, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_protocol(
unsigned long addr,
struct objc_protocol *protocol,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long left, i;

	memset(protocol, '\0', sizeof(struct objc_protocol));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(left >= sizeof(struct objc_protocol)){
		    memcpy(protocol,
			   objc_sections[i].contents +
			       (addr - objc_sections[i].s.addr),
			   sizeof(struct objc_protocol));
		    *trunc = FALSE;
		}
		else{
		    memcpy(protocol,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   left);
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_protocol((Protocol *)protocol, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_method_description_list(
void *p,
struct objc_method_description_list *mdl,
struct objc_method_description **list,
unsigned long *left,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long addr, i;

	addr = (unsigned long)p;
	memset(mdl, '\0', sizeof(struct objc_method_description_list));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		*left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(*left >= sizeof(struct objc_method_description_list) -
			    sizeof(struct objc_method_description)){
		    memcpy(mdl,
			   objc_sections[i].contents +
				(addr - objc_sections[i].s.addr),
			   sizeof(struct objc_method_description_list) -
				sizeof(struct objc_method_description));
		    *left -= sizeof(struct objc_method_description_list) -
			     sizeof(struct objc_method_description);
		    *list = (struct objc_method_description *)
				 (objc_sections[i].contents +
				     (addr - objc_sections[i].s.addr) +
				  sizeof(struct objc_method_description_list) -
				      sizeof(struct objc_method_description));
		    *trunc = FALSE;
		}
		else{
		    memcpy(mdl,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   *left);
		    *left = 0;
		    *list = NULL;
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_objc_method_description_list(mdl, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}

static
enum bool
get_hashEntry(
unsigned long addr,
struct _hashEntry *_hashEntry,
enum bool *trunc,
struct section_info *objc_sections,
unsigned long nobjc_sections,
enum byte_sex host_byte_sex,
enum bool swapped)
{
    unsigned long left, i;

	memset(_hashEntry, '\0', sizeof(struct _hashEntry));
	for(i = 0; i < nobjc_sections; i++){
	    if(addr >= objc_sections[i].s.addr &&
	       addr < objc_sections[i].s.addr + objc_sections[i].size){

		left = objc_sections[i].size -
		        (addr - objc_sections[i].s.addr);
		if(left >= sizeof(struct _hashEntry)){
		    memcpy(_hashEntry,
			   objc_sections[i].contents +
			       (addr - objc_sections[i].s.addr),
			   sizeof(struct _hashEntry));
		    *trunc = FALSE;
		}
		else{
		    memcpy(_hashEntry,
			   objc_sections[i].contents +
			        (addr - objc_sections[i].s.addr),
			   left);
		    *trunc = TRUE;
		}
		if(swapped)
		    swap_hashEntry(_hashEntry, host_byte_sex);
		return(TRUE);
	    }
	}
	return(FALSE);
}
