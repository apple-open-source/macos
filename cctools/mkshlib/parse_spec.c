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
#undef SPEC_DEBUG
/*
 * This file contains the routines to parse a shared library specification
 * file and builds up the data structures to represent it.  See parse_spec.h
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <strings.h>
#include <libc.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/reloc.h>
#include "stuff/errors.h"
#include "stuff/allocate.h"
#include "stuff/hash_string.h"
#include "stuff/bool.h"
#include "stuff/bytesex.h"

#include "libgcc.h"
#include "mkshlib.h"

#define BRANCH_LIST_SIZE 1301 /* initial branch list table size */
#define OBJECT_HASH_SIZE 251 /* object table hash size (fixed, prime number) */
#define OBJECT_LIST_SIZE 256 /* initial object list table size */
#define ODDBALL_LIST_SIZE 256 /* initial oddball symbol list table size */

/*
 * These globals are set after parse_spec() returns with the values read from
 * the library shared library specification file.
 */
char *target_name = (char *)0;	/* shared library target name */
long minor_version = 0;		/* shared library minor version number */
long image_version = 0;		/* shared library image version number */
long text_addr = BAD_ADDRESS;	/* shared library __TEXT segment addr */
long data_addr = BAD_ADDRESS;	/* shared library __DATA segment addr */

/*
 * Hash table for branch slots, filled in here used in creating the host library
 * in changing symbol values to their branch table entries.
 */
struct branch *branch_hash[BRANCH_HASH_SIZE];

struct branch **branch_list; 	  /* linear list of branch slots */
static long branch_list_size;	  /* size of branch_list */
long nbranch_list;		  /* number of branches in branch_list */

/*
 * Hash table for oddball symbols, filled in here and used in creating
 * the host library by not including priviate symbols.  And is used in creating
 * the target library in not complaining about external symbols that don't
 * have branch table entries.
 */
struct oddball *oddball_hash[ODDBALL_HASH_SIZE];

static struct oddball **oddball_list;	/* linear list of oddball symbols */
static long oddball_list_size;	  	/* size of oddball_list */
static long noddball_list;		/* number of oddballs in oddball_list */

/* hash table for objects, fixed size */
static struct object **object_hash;

struct object **object_list; /* list of objects in order to be loaded */
static long object_list_size;     /* size of object_list */
long nobject_list;		  /* number of objects in object_list */

/* The list of alias structures */
struct alias *aliases;

/* the current line number for error messages */
static long line_num;

/*
 * spec_error is incremented for each error encountered and if non-zero at the
 * end of parse_spec() a non-zero exit is performed.
 */
static long spec_error;

/* the current object for the last #init directive seen */
static struct object *init_object;

/* The maximum number slot seen in the spec file, for error reporting only */
static int max_slotnum_seen = 0;

/*
 * functions used only for parsing the spec file.
 */
static long newstate(char *p, long oldstate);
static void branch(char *p);
static void object(char *p);
static void filelist(
    char *file_name,
    char *directory_name,
    enum libgcc libgcc_state);
static char *new_object(
    char *object_name,
    enum bool filelist,
    enum libgcc libgcc_state);
static void init(char *p);
static void alias(char *p);
static void oddball(char *p, long state);
static char *next(char *p);

/* parsing states returned by newstate() */
#define	STATE_NONE	0
#define	STATE_BRANCH	1
#define	STATE_OBJECT	2
#define	STATE_INIT	3
#define	STATE_ALIAS	4
#define	STATE_PRIVATE	5
#define	STATE_NOBRANCH	6
#define	STATE_UNDEFINED	7

/*
 * parse_spec() parses the shared library specification file, spec_filename, and
 * build data structures from it to drive the process to build the shared
 * library files.
 */
void
parse_spec(void)
{
    FILE *spec_stream;
    long state, i, hash_key;
    char line[BUFSIZ], *p;
    struct object *op;
    struct oddball *obp;
    struct branch *bp;

	/* open the specification file to read it */
	if((spec_stream = fopen(spec_filename, "r")) == NULL)
	    system_fatal("can't open: %s", spec_filename);

	line_num = 0;
	state = STATE_NONE;
	while(fgets(line, BUFSIZ, spec_stream) != NULL){
	    line_num++;
	    if(strlen(line) == BUFSIZ - 1 && line[BUFSIZ - 1] != '\n')
		fatal("line %ld in: %s too long (maximum %d)", line_num,
		      spec_filename, BUFSIZ);
	    p = line;
	    while(isspace(*p))
		p++;
	    if(*p == '\0')
		continue;
	    if(*p == '#'){
		state = newstate(p, state);
	    }
	    else{
		switch(state){
		case STATE_BRANCH:
		    branch(p);
		    break;
		case STATE_OBJECT:
		    object(p);
		    break;
		case STATE_INIT:
		    init(p);
		    break;
		case STATE_ALIAS:
		    alias(p);
		    break;
		case STATE_PRIVATE:
		case STATE_NOBRANCH:
		case STATE_UNDEFINED:
		    oddball(p, state);
		    break;
		default:
		    fatal("unrecognized specification in: %s on line %ld",
			  spec_filename, line_num);
		}
	    }
	}
	/*
	 * Make sure the NULL returned from fgets() was the end of file not an
	 * error.  If it is report it and quit otherwise close the file.
	 */
	if(ferror(spec_stream))
	    system_fatal("read error on: %s", spec_filename);
	fclose(spec_stream);

	/*
	 * Now check that everything was specified correctly in spec file.
	 */
	if(target_name == (char *)0){
	    error("shared library target name not specified in: %s (use "
		  "#target <name>)", spec_filename);
	    spec_error++;
	}
	if(minor_version == 0){
	    error("shared library minor version not specified in: %s (use "
		  "#minor_version <decimal number>)", spec_filename);
	    spec_error++;
	}
	if(text_addr == BAD_ADDRESS){
	    error("shared library %s segment address not specified in: %s "
		  "(use #address %s <hex address>)", SEG_TEXT, spec_filename,
		  SEG_TEXT);
	    spec_error++;
	}
	if(data_addr == BAD_ADDRESS){
	    error("shared library %s segment address not specified in: %s "
		  "(use #address %s <hex address>)", SEG_DATA, spec_filename,
		  SEG_DATA);
	    spec_error++;
	}
	if(nbranch_list == 0){
	    error("no #branch table entries specified in: %s ", spec_filename);
	    spec_error++;
	}
	/*
	 * Check the branch table for slots that weren't specified.
	 */
	for(i = 0; i <= max_slotnum_seen; i++){
	    if(branch_list[i] == (struct branch *)0){
		error("branch table entry %ld not specified in: %s ",
		      i, spec_filename);
		spec_error++;
	    }
	}
	if(nobject_list == 0){
	    error("no #objects entries specified in: %s ", spec_filename);
	    spec_error++;
	}
	for(i = 0; i < OBJECT_HASH_SIZE; i++){
	    op = object_hash[i];
	    while(op != (struct object *)0){
		if(op->init_only){
		    error("object: %s has #init specfication in: %s but not "
			  "listed in #objects specification section", op->name,
			   spec_filename);
		    spec_error++;
		}
		op = op->next;
	    }
	}
	free(object_hash);

	for(i = 0; i < ODDBALL_HASH_SIZE; i++){
	    obp = oddball_hash[i];
	    while(obp != (struct oddball *)0){
		hash_key = hash_string(obp->name) % BRANCH_HASH_SIZE;
		bp = branch_hash[hash_key];
		while(bp != (struct branch *)0){
		    if(strcmp(bp->name, obp->name) == 0){
			if(obp->nobranch){
			    error("nobranch_text symbol: %s has a branch slot "
				  "(%ld) in: %s", bp->name, bp->max_slotnum,
				  spec_filename);
			    spec_error++;
			}
			else if(obp->private){
			    error("private_extern symbol: %s has a branch slot "
				  "(%ld) in: %s", bp->name, bp->max_slotnum,
				  spec_filename);
			    spec_error++;
			}
			else if(obp->undefined){
			    error("undefined symbol: %s has a branch slot "
				  "(%ld) in: %s", bp->name, bp->max_slotnum,
				  spec_filename);
			    spec_error++;
			}
		    }
		    bp = bp->next;
		}
		obp = obp->next;
	    }
	}

#ifdef SPEC_DEBUG
	printf("Data structures built from the specification file: %s\n",
	       spec_filename);
	printf("\ttarget_name = %s\n", target_name);
	printf("\tminor_version = %d\n", minor_version);
	printf("\ttext_addr = 0x%x\n", text_addr);
	printf("\tdata_addr = 0x%x\n", data_addr);
	for(i = 0; i < nobject_list; i++){
	    printf("\tobject_list[%d] = 0x%x { %s, %s, 0x%x, %d, 0x%x }\n", i,
		    object_list[i],
		    object_list[i]->name,
		    object_list[i]->base_name,
		    object_list[i]->inits,
		    object_list[i]->init_only,
		    object_list[i]->next);
	    ip = object_list[i]->inits;
	    while(ip){
		printf("\t\tpinit = %s sinit = %s\n", ip->pinit, ip->sinit);
		ip = ip->next;
	    }
	}
	for(i = 0; i < branch_list_size; i++){
	    if(branch_list[i] != (struct branch *)0)
	    printf("\tbranch_list[%d] = 0x%x { %s, %s, %d, 0x%x }\n", i,
		    branch_list[i],
		    branch_list[i]->name,
		    branch_list[i]->old_name,
		    branch_list[i]->max_slotnum,
		    branch_list[i]->next);
	}
	ap = aliases;
	while(ap != (struct alias *)0){
	    printf("\talias list: %s %s\n", ap->alias_name, ap->real_name);
	    ap = ap->next;
	}
	for(i = 0; i < noddball_list; i++){
	    printf("\toddball_list[%d] = 0x%x { %s, private = %d nobranch = %d "
		   "undefined = %d }\n", i, oddball_list[i],
		   oddball_list[i]->name, oddball_list[i]->private,
		   oddball_list[i]->nobranch, oddball_list[i]->undefined);
	}
#endif SPEC_DEBUG
	free(oddball_list);

	/*
	 * If there were any errors in the spec file exit (no files have been
	 * created so no cleanup is needed).  Else return.
	 */
	if(spec_error)
	    exit(1);
}

/*
 * Newstate returns the new state for the directive passed to it and sets up to
 * handle it.  The pointer, p, is assumed to point at a '#' and the name of a
 * directive is to follow it.
 */
static
long
newstate(
char *p,
long oldstate)
{
    char *directive, *minor_version_string, *segment_name, *address_string, *q,
	 *object_name, *base_name, *file_name, *directory_name;
    long hash_key;
    unsigned long address;
    enum libgcc libgcc_state;

	/* step over the '#' */
	p++;
	/* if end of line just return */
	if(*p == '\0')
	    return(oldstate);
	/* skip any leading white space */
	while(isspace(*p))
	    p++;
	/* if end of line or comment return */
	if(*p == '\0' || *p == '#')
	    return(oldstate);
	/* now step over the directive */
	directive = p;
	p = next(p);

	/*
	 * Find out what directive this is and set up to operate in that state.
	 */
	if(strcmp(directive, "branch") == 0){
	    /*
	     * A #branch directive is simply a #branch on it's own line.
	     * This puts us into branch state where the following lines
	     * are entries in the branch table.  See branch() for their
	     * format.  When this is found the data structures to hold
	     * the branch table information is created.
	     */
	    if(*p != '\0')
		fatal("junk after #branch directive in: %s on line %ld",
		      spec_filename, line_num);
	    if(branch_list != (struct branch **)0)
		fatal("#branch directive appears more that once in: %s on "
		      "line %ld", spec_filename, line_num);
	    branch_list = allocate(BRANCH_LIST_SIZE *
			   sizeof(struct branch *));
	    bzero(branch_list, BRANCH_LIST_SIZE * sizeof(struct branch *));
	    branch_list_size = BRANCH_LIST_SIZE;
	    return(STATE_BRANCH);

	} else if(strcmp(directive, "objects") == 0){
	    /*
	     * A #objects directive is simply a #objects on it's own line.
	     * This puts us into object state where the following lines
	     * are the objects that are to make up the shared library.
	     * See object() for their format.  When this is found the
	     * data structures to hold the object information is created.
	     * Since #init's reference objects and needs the object hash
	     * table it may have already been created by a previous #init.
	     */
	    if(*p != '\0')
		fatal("junk after #objects directive in: %s on line %ld",
		      spec_filename, line_num);
	    if(object_list != (struct object **)0)
		fatal("#objects directive appears more that once in: %s on "
		      "line %ld", spec_filename, line_num);
	    object_list = allocate(OBJECT_LIST_SIZE *
			   sizeof(struct object *));
	    object_list_size = OBJECT_LIST_SIZE;
	    nobject_list = 0;
	    if(object_hash == (struct object **)0){
		object_hash = allocate(OBJECT_HASH_SIZE *
			       sizeof(struct object *));
		bzero(object_hash,
		      OBJECT_HASH_SIZE * sizeof(struct object *));
	    }
	    return(STATE_OBJECT);

	} else if(strcmp(directive, "filelist") == 0){
	    /*
	     * A #filelist directive is an #filelist followed by a file name
	     * and an optional directory name.
	     */
	    if(oldstate != STATE_OBJECT)
		fatal("#filelist directive in: %s on line %ld seen when a "
		      "#object directive is not in effect", spec_filename,
		      line_num);
	    if(*p == '\0')
		fatal("no file name specified for #filelist directive in: "
		      "%s on line %ld", spec_filename, line_num);
	    file_name = p;
	    p = next(p);
	    libgcc_state = NOT_LIBGCC;
	    if(*p != '\0'){
		directory_name = p;
		p = next(p);
		if(*p == 'n'){
		    if(strncmp(p, "new_libgcc", sizeof("new_libgcc")-1) != 0)
			fatal("junk after #filelist directive in: %s on line "
			      "%ld", spec_filename, line_num);
		    p += sizeof("new_libgcc") - 1;
		    p = next(p);
		    libgcc_state = NEW_LIBGCC;
		}
		else if(*p == 'o'){
		    if(strncmp(p, "old_libgcc", sizeof("old_libgcc")-1) != 0)
			fatal("junk after #filelist directive in: %s on line "
			      "%ld", spec_filename, line_num);
		    p += sizeof("old_libgcc") - 1;
		    p = next(p);
		    libgcc_state = OLD_LIBGCC;
		}
		if(*p != '\0')
		    fatal("junk after #filelist directive in: %s on line %ld",
			  spec_filename, line_num);
	    }
	    else{
		directory_name = NULL;
	    }
	    filelist(file_name, directory_name, libgcc_state);
	    return(STATE_OBJECT);

	} else if(strcmp(directive, "init") == 0){
	    /*
	     * An #init directive is an #init followed by an object file name
	     * on the same line.  This puts us into init state where the
	     * following lines are pairs of symbols for initialization.
	     * See init() for their format.  When this is found the
	     * data structures to hold the init information is created.
	     * and a pointer to the object structure, init_object,
	     * is set to chain the following init pairs off of.  If no
	     * object structure exist or the the object hash table
	     * doesn't exist it is created.
	     */
	    if(*p == '\0')
		fatal("no object file name specified for #init directive in: "
		      "%s on line %ld", spec_filename, line_num);
	    object_name = p;
	    p = next(p);
	    if(*p != '\0')
		fatal("junk after #init directive in: %s on line %ld",
		      spec_filename, line_num);
	    if(object_hash == (struct object **)0){
		object_hash = allocate(OBJECT_HASH_SIZE *
			       sizeof(struct object *));
		bzero(object_hash,
		      OBJECT_HASH_SIZE * sizeof(struct object *));
	    }

	    base_name = rindex(object_name, '/');
	    if(base_name == (char *)0)
		base_name = object_name;
	    else
		base_name++;
	    hash_key = hash_string(base_name) % OBJECT_HASH_SIZE;
	    init_object = object_hash[hash_key];
	    while(init_object != (struct object *)0){
		if(strcmp(init_object->base_name, base_name) == 0){
		    if(strcmp(init_object->name, object_name) != 0){
			fatal("object files: %s and %s in: %s must have unique "
			      "base file names (%s)", init_object->name,
			      object_name, spec_filename, base_name);
		    }
		    break;
		}
		init_object = init_object->next;
	    }
	    if(init_object == (struct object *)0){
		init_object = allocate(sizeof(struct object));
		init_object->name = savestr(object_name);
		init_object->base_name = rindex(init_object->name, '/');
		if(init_object->base_name == (char *)0)
		    init_object->base_name = init_object->name;
		else
		    init_object->base_name++;
		init_object->inits = (struct init *)0;
		init_object->init_only = 1;
		init_object->next = object_hash[hash_key];
		object_hash[hash_key] = init_object;
	    }
	    return(STATE_INIT);

	} else if(strcmp(directive, "target") == 0){
	    /*
	     * A #target directive is an #target followed by a name on the
	     * same line.  This is the target name the shared library is known
	     * as at runtime.
	     */
	    if(target_name != (char *)0)
		fatal("#target directive appears more that once in: %s on "
		      "line %ld", spec_filename, line_num);
	    if(*p == '\0')
		fatal("no file name specified for #target directive in: %s on "
		      "line %ld", spec_filename, line_num);
	    target_name = p;
	    p = next(p);
	    if(*p != '\0')
		fatal("junk after #target directive in: %s on line %ld",
		      spec_filename, line_num);
	    p = rindex(target_name, '/');
	    if(p != (char *)0)
		p++;
	    else
		p = target_name;
	    while(*p != '\0'){
		if(!isalnum(*p) && *p != '_' && *p != '.' && *p != '-')
		    fatal("base name of target: %s in: %s on line %ld must have"
			  " only alphanumeric, '_', '-', or '.' characters in "
			  "it", target_name, spec_filename, line_num);
		p++;
	    }
	    target_name = savestr(target_name);
	    return(STATE_NONE);

	} else if(strcmp(directive, "address") == 0){
	    /*
	     * An #address directive is an #address followed by a segment name
	     * and a hexidecimal value all on the same line.  The segment name
	     * currently can be either __TEXT or __DATA.  The address values
	     * must be on page boundaries.
	     */
	    if(*p == '\0')
		fatal("no segment name and address specified for #address "
		      "directive in: %s on line %ld", spec_filename, line_num);
	    segment_name = p; 
	    p = next(p);
	    if(*p == '\0')
		fatal("missing address for #address directive in: %s on line "
		      "%ld", spec_filename, line_num);
	    address_string = p;
	    p = next(p);
	    if(*p != '\0')
		fatal("junk after #address directive in: %s on line %ld",
		      spec_filename, line_num);
	    address = strtoul(address_string, &q, 16);
	    if(*q != '\0')
		fatal("bad #address value: %s in: %s on line %ld",
		      address_string, spec_filename, line_num);
	    if(address % getpagesize() != 0)
		fatal("#address value: 0x%x in: %s on line %ld must be a "
		      "multiple of the system pagesize (0x%x)",
		      (unsigned int)address, spec_filename, line_num,
		      (unsigned int)getpagesize());
	    if(strcmp(segment_name, SEG_TEXT) == 0){
		if(text_addr != BAD_ADDRESS && text_addr != address)
		    fatal("#address directive for: %s segment appears more "
			  "that once in: %s on line %ld", SEG_TEXT,
			   spec_filename, line_num);
		text_addr = address;
	    } else if(strcmp(segment_name, SEG_DATA) == 0){
		if(data_addr != BAD_ADDRESS && data_addr != address)
		    fatal("#address directive for: %s segment appears more "
			  "that once in: %s on line %ld", SEG_DATA,
			   spec_filename, line_num);
		data_addr = address;
	    } else {
		fatal("segment name: %s for #address directive in: %s on "
		      "line %ld must be either %s or %s", segment_name,
		      spec_filename, line_num, SEG_TEXT, SEG_DATA);
	    }
	    return(STATE_NONE);

	} else if(strcmp(directive, "minor_version") == 0){
	    /*
	     * A #minor_version directive is an #minor_version followed by a
	     * decimal number on the same line.  The version number must be
	     * greater than zero.
	     */
	    if(minor_version != 0)
		fatal("minor_version specified more than once (in the file: %s "
		      "on the line %ld and on the command line or previouly in "
		      "the specification file)", spec_filename, line_num);
	    if(*p == '\0')
		fatal("missing value for #minor_version directive in: %s on "
		      "line %ld", spec_filename, line_num);
	    minor_version_string = p;
	    p = next(p);
	    if(*p != '\0')
		fatal("junk after #minor_version directive in: %s on line %ld",
		      spec_filename, line_num);
	    minor_version = atol(minor_version_string);
	    if(minor_version <= 0)
		fatal("#minor_version value: %ld in: %s on line %ld must be "
		      "greater than 0", minor_version, spec_filename, line_num);
	    return(STATE_NONE);
	
	} else if(strcmp(directive, "alias") == 0){
	    /*
	     * A #alias directive is simply a #alias on it's own line.
	     * This puts us into alias state where the following lines
	     * lines are pairs of symbols for aliasing.  See alias() for
	     * their format.
	     */
	    if(*p != '\0')
		fatal("junk after #alias directive in: %s on line %ld",
		      spec_filename, line_num);
	    if(oddball_list == (struct oddball **)0){
		oddball_list = allocate(ODDBALL_LIST_SIZE *
			       sizeof(struct oddball *));
		oddball_list_size = OBJECT_LIST_SIZE;
		noddball_list = 0;
	    }
	    return(STATE_ALIAS);

	} else if(strcmp(directive, "private_externs") == 0){
	    /*
	     * A #private_externs directive is simply a #private_externs on
	     * it's own line.  This puts us into private state where the
	     * following lines are the names of the private externs symbols
	     * that are not to appear in the host shared library.
	     * See oddball() for their format.  When this is found the
	     * data structures to hold the oddball symbols are created.
	     */
	    if(*p != '\0')
		fatal("junk after #private_externs directive in: %s on line "
		      "%ld", spec_filename, line_num);
	    if(oddball_list == (struct oddball **)0){
		oddball_list = allocate(ODDBALL_LIST_SIZE *
			       sizeof(struct oddball *));
		oddball_list_size = OBJECT_LIST_SIZE;
		noddball_list = 0;
	    }
	    return(STATE_PRIVATE);

	} else if(strcmp(directive, "nobranch_text") == 0){
	    /*
	     * A #nobranch_text directive is simply a #nobranch_text on it's
	     * own line.  This puts us into nobranch state where the following
	     * lines are the names of the extern text symbols that will not
	     * have branch table entries (for now const data is in this class).
	     * See oddball() for their format.  When this is found the
	     * data structures to hold the oddball symbols are created.
	     */
	    if(*p != '\0')
		fatal("junk after #nobranch_text directive in: %s on line "
		      "%ld", spec_filename, line_num);
	    if(oddball_list == (struct oddball **)0){
		oddball_list = allocate(ODDBALL_LIST_SIZE *
			       sizeof(struct oddball *));
		oddball_list_size = OBJECT_LIST_SIZE;
		noddball_list = 0;
	    }
	    return(STATE_NOBRANCH);
	} else if(strcmp(directive, "undefined") == 0){
	    /*
	     * A #undefined directive is simply a #undefined on it's
	     * own line.  This puts us into undefined state where the following
	     * lines are the names of the extern text symbols that will be
	     * undefined.  See oddball() for their format.  When this is found
	     * the data structures to hold the oddball symbols are created.
	     */
	    if(*p != '\0')
		fatal("junk after #undefined directive in: %s on line "
		      "%ld", spec_filename, line_num);
	    if(oddball_list == (struct oddball **)0){
		oddball_list = allocate(ODDBALL_LIST_SIZE *
			       sizeof(struct oddball *));
		oddball_list_size = OBJECT_LIST_SIZE;
		noddball_list = 0;
	    }
	    return(STATE_UNDEFINED);
	}

	fatal("unknown directive: #%s in: %s on line: %ld", directive,
	      spec_filename, line_num);
	return(STATE_NONE);
}

/*
 * branch is called after a #branch directive has been seen and we are in branch
 * state.  The name of a symbol for the branch table and it possition as a
 * decimal number (or range of possitions) separated by white space appears on
 * its own line.  For each of them a branch structure is created and added
 * to the branch_list in each possition it appears.  Also it is entered in the
 * branch hash table.
 */
static
void
branch(
char *p)
{
    char *branch_name, *old_name, *q;
    long min_slotnum, max_slotnum, hash_key, i;
    struct branch *bp;

	old_name = NULL;

	/* if end of line just return */
	if(*p == '\0')
	    return;
	/* skip any leading white space */
	while(isspace(*p))
	    p++;
	/* if end of line or comment return */
	if(*p == '\0' || *p == '#')
	    return;
	/* now step over the branch symbol name */
	branch_name = p;
	p = next(p);
	if(*p == '\0')
	    fatal("missing branch table possition number in: %s on line %ld",
		  spec_filename, line_num);
	min_slotnum = strtol(p, &q, 10);
	if(q == p)
	    fatal("bad branch table possition number in: %s on line %ld",
		  spec_filename, line_num);
	if(min_slotnum < 0)
	    fatal("branch table possition number in: %s on line %ld must be "
		  "greater than zero", spec_filename, line_num);
	p = q;
	while(isspace(*p))
	    p++;
	if(*p != '\0' && *p != '#' && *p != '-' && *p != 'o')
	    fatal("bad branch table specification in: %s on line %ld",
		  spec_filename, line_num);
	if(*p == '-'){
	    p++; /* skip the '-' */
	    while(isspace(*p))
		p++;
	    if(*p == '\0')
		fatal("missing upper branch table possition range in: %s on "
		      "line %ld", spec_filename, line_num);
	    max_slotnum = strtol(p, &q, 10);
	    if(q == p)
		fatal("bad upper branch table possition range in: %s on line "
		      "%ld", spec_filename, line_num);
	    if(max_slotnum < min_slotnum)
		fatal("bad upper branch table possition range (%ld) in: %s on "
		      "line %ld must be greater than lower (%ld)", max_slotnum,
		       spec_filename, line_num, min_slotnum);
	    while(isspace(*q))
		q++;
	    if(*q != '\0' && *q != '#')
		fatal("junk after #branch specification in: %s on line %ld",
		      spec_filename, line_num);
	}
	else if(*p == 'o'){
	    if(strncmp(p, "old_name", sizeof("old_name")-1) != 0)
		fatal("junk after #branch specification in: %s on line %ld",
		      spec_filename, line_num);
	    p += sizeof("old_name") - 1;
	    while(isspace(*p))
		p++;
	    if(*p == '\0' || *p == '#')
		fatal("missing symbol name after \"old_name\" for #branch "
		      "specification in: %s on line %ld", spec_filename,
		      line_num);
	    /* now step over the old symbol name */
	    old_name = p;
	    p = next(p);
	    if(*p != '\0' && *p != '#')
		fatal("junk after #branch specification in: %s on line %ld",
		      spec_filename, line_num);
	    max_slotnum = min_slotnum;
	}
	else{
	    max_slotnum = min_slotnum;
	}
	/*
	 * put this branch name in the hash table if not in there already.
	 */
	hash_key = hash_string(branch_name) % BRANCH_HASH_SIZE;
	bp = branch_hash[hash_key];
	while(bp != (struct branch *)0){
	    if(strcmp(bp->name, branch_name) == 0){
		/* adjust the slot used for the value of this symbol */
		if(bp->max_slotnum < max_slotnum)
		    bp->max_slotnum = max_slotnum;
		break;
	    }
	    bp = bp->next;
	}
	if(bp == (struct branch *)0){
	    bp = allocate(sizeof(struct branch));
	    bp->name = savestr(branch_name);
	    if(old_name != NULL)
		bp->old_name = savestr(old_name);
	    if(strcmp(bp->name, EMPTY_SLOT_NAME) == 0)
		bp->empty_slot = 1;
	    else
		bp->empty_slot = 0;
	    bp->max_slotnum = max_slotnum;
	    bp->next = branch_hash[hash_key];
	    branch_hash[hash_key] = bp;
	}
	/*
	 * put this branch spec in the branch list in all of it's slots.
	 */
	if(max_slotnum >= branch_list_size){
	    branch_list = reallocate(branch_list, max_slotnum * 2 *
						  sizeof(struct branch *));
	    bzero(branch_list + branch_list_size,
		  branch_list_size * sizeof(struct branch *));
	    branch_list_size = max_slotnum * 2;
	}
	for(i = min_slotnum; i <= max_slotnum; i++){
	    if(branch_list[i] != (struct branch *)0 &&
	       branch_list[i] != bp){
		error("#branch position %ld multiply specified in: %s on line "
		      "%ld", i, spec_filename, line_num);
		spec_error++;
	    }
	    else{
		if(branch_list[i] != bp)
		    nbranch_list++;
		branch_list[i] = bp;
	    }
	}
	if(max_slotnum > max_slotnum_seen)
	    max_slotnum_seen = max_slotnum;
}

/*
 * object is called after a #objects directive has been seen and we are in
 * object state.  The names of object files then appear separated by white
 * space.
 */
static
void
object(
char *p)
{
    char *object_name;

	/* if end of line just return */
	if(*p == '\0')
	    return;
	/* skip any leading white space */
	while(isspace(*p))
	    p++;
	/* if end of line or comment return */
	if(*p == '\0' || *p == '#')
	    return;
	/* now step over the object name */
	object_name = p;
	p = next(p);
	while(*object_name != '\0'){
	    p = new_object(object_name, FALSE, NOT_LIBGCC);
	    /* now step over the object name and see if there is another */
	    object_name = p;
	    p = next(p);
	}
}

static
void
filelist(
char *file_name,
char *directory_name,
enum libgcc libgcc_state)
{
    FILE *filelist_stream;
    long n;
    char line[BUFSIZ], *p, *object_name;

	if(directory_name != NULL){
	    if(directory_name[strlen(directory_name) - 1] != '/')
		directory_name = makestr(directory_name, "/", NULL);
	}
	/* open the filelist file to read it */
	if((filelist_stream = fopen(file_name, "r")) == NULL)
	    system_fatal("can't open: %s from #filelist directive in: %s on "
			 "line %ld", file_name, spec_filename, line_num);

	n = 0;
	while(fgets(line, BUFSIZ, filelist_stream) != NULL){
	    n++;
	    if(strlen(line) == BUFSIZ - 1 && line[BUFSIZ - 1] != '\n')
		fatal("line %ld in: %s too long (maximum %d)", n, file_name,
		      BUFSIZ);
	    p = strchr(line, '\n');
	    *p = '\0';
	    if(directory_name != NULL)
		object_name = makestr(directory_name, line, NULL);
	    else
		object_name = line;
	    new_object(object_name, TRUE, libgcc_state);
	}
	fclose(filelist_stream);
}

/*
 * new_object() is passed an object_name and an object structure is found
 * (as previouly entered in the hash table) or created and added to the
 * object_list.  Also it is entered in hash table if it was not found there.
 * If this is a duplicate name and filelist is TRUE it is ignored.
 */
static
char *
new_object(
char *object_name,
enum bool filelist,
enum libgcc libgcc_state)
{
    char *p, *base_name;
    long hash_key;
    struct object *op;

	if(nobject_list + 1 > object_list_size){
	    object_list = reallocate(object_list, object_list_size * 2 *
						  sizeof(struct object *));
	    object_list_size *= 2;
	}
	base_name = rindex(object_name, '/');
	if(base_name == (char *)0)
	    base_name = object_name;
	else
	    base_name++;
	hash_key = hash_string(base_name) % OBJECT_HASH_SIZE;
	op = object_hash[hash_key];
	while(op != (struct object *)0){
	    if(strcmp(op->base_name, base_name) == 0){
		if(strcmp(op->name, object_name) != 0){
		    fatal("object files: %s and %s must have unique base "
			  "file names (%s)", op->name, object_name, base_name);
		}
		/*
		 * If this duplicate object name comes from a filelist then
		 * ignore it and use the previous one.
		 */
		if(filelist == TRUE)
		    return(NULL);
		if(op->init_only)
		    break;
		fatal("object file name: %s appears more than once in: %s "
		      "on line %ld", object_name, spec_filename, line_num);
	    }
	    op = op->next;
	}
	p = base_name;
	while(*p != '\0'){
	    if(!isalnum(*p) && *p != '_' && *p != '.' && *p != '-')
		fatal("base name of object: %s in: %s on line %ld must have"
		      " only alphanumeric, '_', '-', or '.' characters in "
		      "it", object_name, spec_filename, line_num);
	    p++;
	}
	if(op == (struct object *)0){
	    op = allocate(sizeof(struct object));
	    bzero(op, sizeof(struct object));
	    op->name = savestr(object_name);
	    op->base_name = rindex(op->name, '/');
	    if(op->base_name == (char *)0)
		op->base_name = op->name;
	    else
		op->base_name++;
	    op->libgcc_state = libgcc_state;
	    op->next = object_hash[hash_key];
	    object_hash[hash_key] = op;
	}
	else{
	    op->init_only = 0;
	}

	object_list[nobject_list++] = op;
	return(p);
}

/*
 * init is called after a #init directive has been seen and we are in init
 * state.  The pairs of symbol names then appear separated by white space each
 * on their own line.  A init structure is created for it and then it is
 * put on the chain of such structures in the object pointed to by
 * init_object.
 */
static
void
init(
char *p)
{
    char *pinit, *sinit;
    struct init *ip;

	/* if end of line just return */
	if(*p == '\0')
	    return;
	/* skip any leading white space */
	while(isspace(*p))
	    p++;
	/* if end of line or comment return */
	if(*p == '\0' || *p == '#')
	    return;
	/* now step over the pointer symbol name */
	pinit = p;
	p = next(p);
	if(*p == '\0')
	    fatal("missing symbol name for #init specification in: %s on line "
		  "%ld", spec_filename, line_num);
	sinit = p;
	p = next(p);
	if(*p != '\0')
	    fatal("junk after #init specification in: %s on line %ld",
		  spec_filename, line_num);
	ip = allocate(sizeof(struct init));
	ip->pinit = savestr(pinit);
	ip->sinit = savestr(sinit);
	ip->next = init_object->inits;
	init_object->inits = ip;
}

/*
 * alias is called after a #alias directive has been seen and we are in alias
 * state.  The pairs of symbol names then appear separated by white space each
 * on their own line.  An alias structure is created for it and then it is
 * put on the chain of such structures.
 */
static
void
alias(
char *p)
{
    char *alias_name, *real_name;
    struct alias *ap;

	/* if end of line just return */
	if(*p == '\0')
	    return;
	/* skip any leading white space */
	while(isspace(*p))
	    p++;
	/* if end of line or comment return */
	if(*p == '\0' || *p == '#')
	    return;
	/* now step over the pointer symbol name */
	alias_name = p;
	p = next(p);
	if(*p == '\0')
	    fatal("missing symbol name for #alias specification in: %s on line "
		  "%ld", spec_filename, line_num);
	real_name = p;
	p = next(p);
	if(*p != '\0')
	    fatal("junk after #alias specification in: %s on line %ld",
		  spec_filename, line_num);
	ap = allocate(sizeof(struct alias));
	ap->alias_name = savestr(alias_name);
	ap->real_name = savestr(real_name);
	ap->next = aliases;
	aliases = ap;

	/*
	 * The old alias feature of the link editor has been replace with the
	 * indirect feature.  So to get the same behavior of having the alias
	 * symbol not appear in the output file the alias name is made a private
	 * extern.  Note this does not change the state we are in.
	 */
	oddball(alias_name, STATE_PRIVATE);
}

/*
 * oddball is called after a #private_externs directive has been seen and we
 * are in private state, after a #nobranch_text directive has been seen and we
 * are in nobranch state, or after a #undefined directive has been seen and we
 * are in undefined state.  The names of external symbols then appear separated
 * by white space.  For each of them an oddball structure is found (as previouly
 * entered in the hash table) or created and added to the oddball_list.  Also
 * it is entered in hash table if it was not found there.  Then the oddball
 * structure is marked with the state we are in.
 */
static
void
oddball(
char *p,
long state)
{
    char *name;
    long hash_key;
    struct oddball *obp;

	/* if end of line just return */
	if(*p == '\0')
	    return;
	/* skip any leading white space */
	while(isspace(*p))
	    p++;
	/* if end of line or comment return */
	if(*p == '\0' || *p == '#')
	    return;
	/* now step over the symbol name */
	name = p;
	p = next(p);
	while(*name != '\0'){
	    if(noddball_list + 1 > oddball_list_size){
		oddball_list = reallocate(oddball_list, oddball_list_size * 2 *
			       			      sizeof(struct oddball *));
		oddball_list_size *= 2;
	    }
	    hash_key = hash_string(name) % ODDBALL_HASH_SIZE;
	    obp = oddball_hash[hash_key];
	    while(obp != (struct oddball *)0){
		if(strcmp(obp->name, name) == 0)
		    break;
		obp = obp->next;
	    }
	    if(obp == (struct oddball *)0){
		obp = allocate(sizeof(struct oddball));
		bzero(obp, sizeof(struct oddball));
		obp->name = savestr(name);
		obp->next = oddball_hash[hash_key];
		oddball_hash[hash_key] = obp;
		oddball_list[noddball_list++] = obp;
	    }
	    switch(state){
	    case STATE_PRIVATE:
		obp->private = 1;
		break;
	    case STATE_NOBRANCH:
		obp->nobranch = 1;
		break;
	    case STATE_UNDEFINED:
		obp->undefined = 1;
		break;
	    }

	    /* now step over the symbol name and see if there is another */
	    name = p;
	    p = next(p);
	}
}

/*
 * next() is passed a pointer to string.  It is assumed to point to a word
 * (a non isspace character).  It advances over the word and places a null
 * after it.  Then it advances to the next word and returns a pointer to
 * it.  If there are no more words or the rest of the line is a comment
 * (starts with a #) then the returned pointer points at '\0'.  This routine
 * assumes the line ends with a '\0' like fgets() reads.
 */
static
char *
next(
char *p)
{
	/* advance past the current word */
	while(!isspace(*p) && *p != '\0')
	    p++;
	/*
	 * put a null at the end of the current word and advance p if not at end
	 * of line so to point to the next thing.
	 */
	if(*p != '\0'){
	    *p = '\0';
	    p++;
	    while(isspace(*p))
		p++;
	    /*
	     * If the next thing is a # then treat the rest of the line as a
	     * comment and nothing else is left on the line.
	     */
	    if(*p == '#')
		*p = '\0';
	}
	return(p);
}
