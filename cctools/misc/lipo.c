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
/*
 * The lipo(1) program.  This program creates, thins and operates on fat files.
 * This program takes the following options:
 *   <input_file>
 *   -output <filename>
 *   -create
 *   -info
 *   -detailed_info
 *   -arch <arch_type> <input_file>
 *   -thin <arch_type>
 *   -extract <arch_type>
 *   -remove <arch_type>
 *   -replace <arch_type> <file_name>
 *   -segalign <arch_type> <value>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ar.h>
#ifndef AR_EFMT1
#define	AR_EFMT1	"#1/"		/* extended format #1 */
#endif
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <libc.h>
#ifndef __OPENSTEP__
#include <utime.h>
#endif
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include "stuff/arch.h"
#include "stuff/errors.h"
#include "stuff/allocate.h"

/* The maximum section alignment allowed to be specified, as a power of two */
#define MAXSECTALIGN		15 /* 2**15 or 0x8000 */

/* These #undef's are because of the #define's in <mach.h> */
#undef TRUE
#undef FALSE

/* name of the program for error messages (argv[0]) */
char *progname = NULL;

/* names and types (if any) of input file specified on the commmand line */
struct input_file {
    char *name;
    struct arch_flag arch_flag;
    struct fat_header *fat_header;
    struct fat_arch *fat_arches;
    enum bool is_thin;
};
static struct input_file *input_files = NULL;
static unsigned long ninput_files = 0;

/* Thin files from the input files to operate on */
struct thin_file {
    char *name;
    char *addr;
    struct fat_arch fat_arch;
    enum bool from_fat;
    enum bool extract;
    enum bool remove;
    enum bool replace;
};
static struct thin_file *thin_files = NULL;
static unsigned long nthin_files = 0;

/* The specified output file */
static char *output_file = NULL;
static unsigned long output_filemode = 0;
#ifndef __OPENSTEP__
static struct utimbuf output_timep = { 0 };
#else
static time_t output_timep[2] = { 0 };
#endif
static enum bool archives_in_input = FALSE;

/* flags set from command line arguments to specifify the operation */
static enum bool create_flag = FALSE;
static enum bool info_flag = FALSE;
static enum bool detailed_info_flag = FALSE;

static enum bool thin_flag = FALSE;
static struct arch_flag thin_arch_flag = { 0 };

static enum bool remove_flag = FALSE;
static struct arch_flag *remove_arch_flags = NULL;
static unsigned long nremove_arch_flags = 0;

static enum bool extract_flag = FALSE;
static struct arch_flag *extract_arch_flags = NULL;
static unsigned long nextract_arch_flags = 0;
static enum bool extract_family_flag = FALSE;

static enum bool replace_flag = FALSE;
struct replace {
    struct arch_flag arch_flag;
    struct thin_file thin_file;
};
static struct replace *replaces = NULL;
static unsigned long nreplaces = 0;

struct segalign {
    struct arch_flag arch_flag;
    unsigned long align;
};
static struct segalign *segaligns = NULL;
static unsigned long nsegaligns = 0;


static struct fat_header fat_header = { 0 };

static void create_fat(
    void);
static void process_input_file(
    struct input_file *input);
static void process_replace_file(
    struct replace *replace);
static void check_archive(
    char *name,
    char *addr,
    unsigned long size,
    cpu_type_t *cputype,
    cpu_subtype_t *cpusubtype);
static void check_extend_format_1(
    char *name,
    struct ar_hdr *ar_hdr,
    unsigned long size_left,
    unsigned long *member_name_size);
static unsigned long get_align(
    struct mach_header *mhp,
    struct load_command *load_commands,
    unsigned long size,
    char *name,
    enum bool swapped);
static unsigned long guess_align(
    unsigned long vmaddr);
static void print_arch(
    struct fat_arch *fat_arch);
static void print_cputype(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype);
static int size_ar_name(
    char *ar_name);
static struct input_file *new_input(
    void);
static struct thin_file *new_thin(
    void);
static struct arch_flag *new_arch_flag(
    struct arch_flag **arch_flags,
    unsigned long *narch_flags);
static struct replace *new_replace(
    void);
static struct segalign *new_segalign(
    void);
static int cmp_qsort(
    const struct thin_file *thin1,
    const struct thin_file *thin2);
static unsigned long round(
    unsigned long v,
    unsigned long r);
static enum bool ispoweroftwo(
    unsigned long x);
static void check_arch(
    struct input_file *input,
    struct thin_file *thin);
static void usage(
    void);

int
main(
int argc,
char *argv[],
char *envp[])
{
    int fd;
    unsigned long i, j, k, value;
    char *p, *endp;
    struct input_file *input;
    struct arch_flag *arch_flag;
    struct replace *replace;
    struct segalign *segalign;
    const struct arch_flag *arch_flags;
    enum bool found;

	input = NULL;
	/*
	 * Process the command line arguments.
	 */
	progname = argv[0];
	for(i = 1; i < argc; i++){
	    if(argv[i][0] == '-'){
		p = &(argv[i][1]);
		switch(*p){
		case 'a':
		    if(strcmp(p, "arch") == 0 || strcmp(p, "a") == 0){
			if(i + 2 >= argc){
			    error("missing argument(s) to %s option", argv[i]);
			    usage();
			}
			input = new_input();
			if(get_arch_from_flag(argv[i+1],
					      &(input->arch_flag)) == 0){
			    error("unknown architecture specification flag: %s "
				  "in specifing input file %s %s %s", argv[i+1],
				  argv[i], argv[i+1], argv[i+2]);
			    arch_usage();
			    usage();
			}
			input->name = argv[i+2];
			i += 2;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'c':
		    if(strcmp(p, "create") == 0 || strcmp(p, "c") == 0){
			create_flag = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'd':
		    if(strcmp(p, "detailed_info") == 0 || strcmp(p, "d") == 0){
			detailed_info_flag = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'e':
		    if(strcmp(p, "extract") == 0 ||
		       strcmp(p, "extract_family") == 0 ||
		       strcmp(p, "e") == 0){
			extract_flag = TRUE;
			if(strcmp(p, "extract_family") == 0)
			    extract_family_flag = TRUE;
			if(i + 1 >= argc){
			    error("missing argument to %s option", argv[i]);
			    usage();
			}
			arch_flag = new_arch_flag(&extract_arch_flags,
						&nextract_arch_flags);
			if(get_arch_from_flag(argv[i+1], arch_flag) == 0){
			    error("unknown architecture specification flag: "
				  "%s in specifing extract operation: %s %s",
				  argv[i+1], argv[i], argv[i+1]);
			    arch_usage();
			    usage();
			}
			i++;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'i':
		    if(strcmp(p, "info") == 0 || strcmp(p, "i") == 0){
			info_flag = TRUE;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'o':
		    if(strcmp(p, "output") == 0 || strcmp(p, "o") == 0){
			if(i + 1 >= argc){
			    error("missing argument to %s option", argv[i]);
			    usage();
			}
			if(output_file != NULL)
			    fatal("more than one %s option specified", argv[i]);
			output_file = argv[i + 1];
			i++;
		    }
		    else
			goto unknown_flag;
		    break;
		case 'r':
		    if(strcmp(p, "remove") == 0 || strcmp(p, "rem") == 0){
			remove_flag = TRUE;
			if(i + 1 >= argc){
			    error("missing argument to %s option", argv[i]);
			    usage();
			}
			arch_flag = new_arch_flag(&remove_arch_flags,
						&nremove_arch_flags);
			if(get_arch_from_flag(argv[i+1], arch_flag) == 0){
			    error("unknown architecture specification flag: "
				  "%s in specifing remove operation: %s %s",
				  argv[i+1], argv[i], argv[i+1]);
			    arch_usage();
			    usage();
			}
			i++;
		    }
		    else if(strcmp(p, "replace") == 0 || strcmp(p, "rep") == 0){
			replace_flag = TRUE;
			if(i + 2 >= argc){
			    error("missing argument(s) to %s option", argv[i]);
			    usage();
			}
			replace = new_replace();
			if(get_arch_from_flag(argv[i+1],
					      &(replace->arch_flag)) == 0){
			    error("unknown architecture specification flag: "
				  "%s in specifing replace operation: %s %s %s",
				  argv[i+1], argv[i], argv[i+1], argv[i+2]);
			    arch_usage();
			    usage();
			}
			replace->thin_file.name = argv[i+2];
			i += 2;
		    }
		    else
			goto unknown_flag;
		    break;
		case 's':
		    if(strcmp(p, "segalign") == 0 || strcmp(p, "s") == 0){
			if(i + 2 >= argc){
			    error("missing argument(s) to %s option", argv[i]);
			    usage();
			}
			segalign = new_segalign();
			if(get_arch_from_flag(argv[i+1],
					      &(segalign->arch_flag)) == 0){
			    error("unknown architecture specification flag: "
				  "%s in specifing segment alignment: %s %s %s",
				  argv[i+1], argv[i], argv[i+1], argv[i+2]);
			    arch_usage();
			    usage();
			}
			value = strtoul(argv[i+2], &endp, 16);
			if(*endp != '\0')
			    fatal("argument for -segalign <arch_type> %s not a "
				  "proper hexadecimal number", argv[i+2]);
			if(!ispoweroftwo(value) || value == 0)
			    fatal("argument to -segalign <arch_type> %lx (hex) "
				  "must be a non-zero power of two", value);
			if(value > (1 << MAXSECTALIGN))
			    fatal("argument to -segalign <arch_type> %lx (hex) "
				  "must equal to or less than %x (hex)",
				  value, (unsigned int)(1 << MAXSECTALIGN));
			segalign->align = 0;
			while((value & 0x1) != 1){
			    value >>= 1;
			    segalign->align++;
			}
			i += 2;
		    }
		    else
			goto unknown_flag;
		    break;
		case 't':
		    if(strcmp(p, "thin") == 0 || strcmp(p, "t") == 0){
			if(thin_flag == TRUE)
			    fatal("more than one %s option specified", argv[i]);
			thin_flag = TRUE;
			if(i + 1 >= argc){
			    error("missing argument to %s option", argv[i]);
			    usage();
			}
			if(get_arch_from_flag(argv[i+1], &thin_arch_flag) == 0){
			    error("unknown architecture specification flag: "
				  "%s in specifing thin operation: %s %s",
				  argv[i+1], argv[i], argv[i+1]);
			    arch_usage();
			    usage();
			}
			i++;
		    }
		    else
			goto unknown_flag;
		    break;
		default:
unknown_flag:
		    fatal("unknown flag: %s", argv[i]);
		}
	    }
	    else{
		input = new_input();
		input->name = argv[i];
	    }
	}

	/*
	 * Check to see the specified arguments are valid.
	 */
	if(info_flag == FALSE && detailed_info_flag == FALSE &&
	   create_flag == FALSE && thin_flag == FALSE &&
	   extract_flag == FALSE && remove_flag == FALSE &&
	   replace_flag == FALSE){
	    error("one of -create, -thin <arch_type>, -extract <arch_type>, "
		  "-remove <arch_type>, -replace <arch_type> <file_name>, "
		  "-info or -detailed_info must be specified");
	    usage();
	}
	if((create_flag == TRUE || thin_flag == TRUE || extract_flag == TRUE ||
	    remove_flag == TRUE || replace_flag == TRUE) &&
	   output_file == NULL){
	    error("no output file specified");
	    usage();
	}
	if(ninput_files == 0){
	    error("no input files specified");
	    usage();
	}
	if(create_flag + thin_flag + extract_flag + remove_flag + replace_flag +
	   info_flag + detailed_info_flag > 1){
	    error("only one of -create, -thin <arch_type>, -extract <arch_type>"
		  ", -remove <arch_type>, -replace <arch_type> <file_name>, "
		  "-info or -detailed_info can be specified");
	    usage();
	}

	/*
	 * Determine the types of the input files.
	 */
	for(i = 0; i < ninput_files; i++)
	    process_input_file(input_files + i);

	/*
	 * Do the specified operation.
	 */

	if(create_flag){
	    /* check to make sure no two files have the same architectures */
	    for(i = 0; i < nthin_files; i++)
		for(j = i + 1; j < nthin_files; j++)
		    if(thin_files[i].fat_arch.cputype == 
		       thin_files[j].fat_arch.cputype && 
		       thin_files[i].fat_arch.cpusubtype == 
		       thin_files[j].fat_arch.cpusubtype){
			arch_flags = get_arch_flags();
			for(k = 0; arch_flags[k].name != NULL; k++){
			    if(arch_flags[k].cputype ==
			       thin_files[j].fat_arch.cputype && 
			       arch_flags[k].cpusubtype ==
			       thin_files[j].fat_arch.cpusubtype)
			    fatal("%s and %s have the same architectures (%s) "
			      "and can't be in the same fat output file",
			      thin_files[i].name, thin_files[j].name,
			      arch_flags[k].name);
			}
			fatal("%s and %s have the same architectures (cputype "
			      "(%d) and cpusubtype (%d)) and can't be in the "
			      "same fat output file", thin_files[i].name,
			      thin_files[j].name,thin_files[i].fat_arch.cputype,
			      thin_files[i].fat_arch.cpusubtype);
		    }
	    create_fat();
	}

	if(thin_flag){
	    if(ninput_files != 1)
		fatal("only one input file can be specified with the -thin "
		      "option");
	    if(input_files[0].fat_header == NULL)
		fatal("input file (%s) must be a fat file when the -thin "
		      "option is specified", input_files[0].name);
	    for(i = 0; i < nthin_files; i++){
		if(thin_files[i].fat_arch.cputype == thin_arch_flag.cputype &&
		   thin_files[i].fat_arch.cpusubtype ==
						     thin_arch_flag.cpusubtype){
		    (void)unlink(output_file);
		    if((fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC,
				  output_filemode)) == -1)
			system_fatal("can't create output file: %s",
				     output_file);

		    if(write(fd, thin_files[i].addr,thin_files[i].fat_arch.size)
		       != thin_files[i].fat_arch.size)
			system_fatal("can't write thin file to output file: %s",
				     output_file);
		    if(close(fd) == -1)
			system_fatal("can't close output file: %s",output_file);
		    if(utime(output_file,
#ifndef __OPENSTEP__
		       &output_timep) == -1)
#else
		       output_timep) == -1)
#endif
			system_fatal("can't set the modifiy times for "
				     "output file: %s", output_file);
		    break;
		}
	    }
	    if(i == nthin_files)
		fatal("fat input file (%s) does not contain the specified "
		      "architecture (%s) to thin it to", input->name,
		      thin_arch_flag.name);
	}

	if(extract_flag){
	    if(ninput_files != 1)
		fatal("only one input file can be specified with the -extract "
		      "option");
	    if(input_files[0].fat_header == NULL)
		fatal("input file (%s) must be a fat file when the -extract "
		      "option is specified", input_files[0].name);
	    for(i = 0; i < nextract_arch_flags; i++){
		for(j = i + 1; j < nextract_arch_flags; j++){
		if(extract_arch_flags[i].cputype ==
		       extract_arch_flags[j].cputype &&
		   extract_arch_flags[i].cpusubtype ==
		       thin_files[j].fat_arch.cpusubtype)
		    fatal("-extract %s specified multiple times", 
			  extract_arch_flags[i].name);
		}
	    }
	    /* mark those thin files for extraction */
	    for(i = 0; i < nextract_arch_flags; i++){
		found = FALSE;
		for(j = 0; j < nthin_files; j++){
		    if(extract_arch_flags[i].cputype ==
			   thin_files[j].fat_arch.cputype &&
		       (extract_arch_flags[i].cpusubtype ==
			   thin_files[j].fat_arch.cpusubtype ||
			extract_family_flag == TRUE)){
			thin_files[j].extract = TRUE;
			found = TRUE;
		    }
		}
		if(found == FALSE)
		    fatal("-extract %s specified but fat file: %s does not "
			  "contain that architecture",
			  extract_arch_flags[i].name, input_files[0].name);
	    }
	    /* remove those thin files not marked for extraction */
	    for(i = 0; i < nthin_files; ){
		if(thin_files[i].extract == FALSE){
		    for(j = i; j < nthin_files; j++)
			thin_files[j] = thin_files[j + 1];
		    nthin_files--;
		}
		else
		    i++;
	    }
	    create_fat();
	}

	if(remove_flag){
	    if(ninput_files != 1)
		fatal("only one input file can be specified with the -remove "
		      "option");
	    if(input_files[0].fat_header == NULL)
		fatal("input file (%s) must be a fat file when the -remove "
		      "option is specified", input_files[0].name);
	    for(i = 0; i < nremove_arch_flags; i++){
		for(j = i + 1; j < nremove_arch_flags; j++){
		if(remove_arch_flags[i].cputype ==
		       remove_arch_flags[j].cputype &&
		   remove_arch_flags[i].cpusubtype ==
		       remove_arch_flags[j].cpusubtype)
		    fatal("-remove %s specified multiple times", 
			  remove_arch_flags[i].name);
		}
	    }
	    /* mark those thin files for removal */
	    for(i = 0; i < nremove_arch_flags; i++){
		for(j = 0; j < nthin_files; j++){
		    if(remove_arch_flags[i].cputype ==
			   thin_files[j].fat_arch.cputype &&
		       remove_arch_flags[i].cpusubtype ==
			   thin_files[j].fat_arch.cpusubtype){
			thin_files[j].remove = TRUE;
			break;
		    }
		}
		if(j == nthin_files)
		    fatal("-remove %s specified but fat file: %s does not "
			  "contain that architecture",
			  remove_arch_flags[i].name, input_files[0].name);
	    }
	    /* remove those thin files marked for removal */
	    for(i = 0; i < nthin_files; ){
		if(thin_files[i].remove == TRUE){
		    for(j = i; j < nthin_files; j++)
			thin_files[j] = thin_files[j + 1];
		    nthin_files--;
		}
		else
		    i++;
	    }
	    if(nthin_files == 0)
		fatal("-remove's specified would result in an empty fat file");
	    create_fat();
	}

	if(replace_flag){
	    if(ninput_files != 1)
		fatal("only one input file can be specified with the -replace "
		      "option");
	    if(input_files[0].fat_header == NULL)
		fatal("input file (%s) must be a fat file when the -replace "
		      "option is specified", input_files[0].name);
	    for(i = 0; i < nreplaces; i++){
		for(j = i + 1; j < nreplaces; j++){
		if(replaces[i].arch_flag.cputype ==
		       replaces[j].arch_flag.cputype &&
		   replaces[i].arch_flag.cpusubtype ==
		       replaces[j].arch_flag.cpusubtype)
		    fatal("-replace %s <file_name> specified multiple times", 
			  replaces[j].arch_flag.name);
		}
	    }
	    for(i = 0; i < nreplaces; i++){
		process_replace_file(replaces + i);
		for(j = 0; j < nthin_files; j++){
		    if(replaces[i].arch_flag.cputype ==
			   thin_files[j].fat_arch.cputype &&
		       replaces[i].arch_flag.cpusubtype ==
			   thin_files[j].fat_arch.cpusubtype){
			thin_files[j] = replaces[i].thin_file;
			break;
		    }
		}
		if(j == nthin_files)
		    fatal("-replace %s <file_name> specified but fat file: %s "
			  "does not contain that architecture",
			  replaces[i].arch_flag.name, input_files[0].name);
	    }
	    create_fat();
	}

	if(info_flag){
	    for(i = 0; i < ninput_files; i++){
		if(input_files[i].fat_header != NULL){
		    printf("Architectures in the fat file: %s are: ",
			   input_files[i].name);
		    for(j = 0; j < input_files[i].fat_header->nfat_arch; j++){
		        print_arch(&(input_files[i].fat_arches[j]));
			printf(" ");
		    }
		    printf("\n");
		}
		else{
		    if(input_files[i].is_thin == FALSE)
			printf("input file %s is not a fat file\n",
			       input_files[i].name);
		}
	    }
	    for(i = 0; i < nthin_files; i++){
		if(thin_files[i].from_fat == TRUE)
		    continue;
		printf("Non-fat file: %s is architecture: %s\n",
		       thin_files[i].name,
		       get_arch_name_from_types(thin_files[i].fat_arch.cputype,
					thin_files[i].fat_arch.cpusubtype));
	    }
	}

	if(detailed_info_flag){
	    for(i = 0; i < ninput_files; i++){
		if(input_files[i].fat_header != NULL){
		    printf("Fat header in: %s\n", input_files[i].name);
		    printf("fat_magic 0x%x\n",
			  (unsigned int)(input_files[i].fat_header->magic));
		    printf("nfat_arch %lu\n",
			   input_files[i].fat_header->nfat_arch);
		    for(j = 0; j < input_files[i].fat_header->nfat_arch; j++){
			printf("architecture ");
		        print_arch(&(input_files[i].fat_arches[j]));
			printf("\n");
			print_cputype(input_files[i].fat_arches[j].cputype,
				      input_files[i].fat_arches[j].cpusubtype);
			printf("    offset %lu\n",
			       input_files[i].fat_arches[j].offset);
			printf("    size %lu\n",
			       input_files[i].fat_arches[j].size);
			printf("    align 2^%lu (%d)\n",
			       input_files[i].fat_arches[j].align,
			       1 << input_files[i].fat_arches[j].align);
		    }
		}
		else{
		    printf("input file %s is not a fat file\n",
			   input_files[i].name);
		}
	    }
	    for(i = 0; i < nthin_files; i++){
		if(thin_files[i].from_fat == TRUE)
		    continue;
		printf("Non-fat file: %s is architecture: %s\n",
		       thin_files[i].name,
		       get_arch_name_from_types(thin_files[i].fat_arch.cputype,
					thin_files[i].fat_arch.cpusubtype));
	    }
	}

	return(0);
}

/*
 * create_fat() creates a fat output file from the thin files.
 */
static
void
create_fat(void)
{
    unsigned long i, j, offset;
    int fd;

	/* fold in specified segment alignments */
	for(i = 0; i < nsegaligns; i++){
	    for(j = i + 1; j < nsegaligns; j++){
	    if(segaligns[i].arch_flag.cputype ==
		   segaligns[j].arch_flag.cputype &&
	       segaligns[i].arch_flag.cpusubtype ==
		   segaligns[j].arch_flag.cpusubtype)
		fatal("-segalign %s <value> specified multiple times", 
		      segaligns[j].arch_flag.name);
	    }
	}
	for(i = 0; i < nsegaligns; i++){
	    for(j = 0; j < nthin_files; j++){
		if(segaligns[i].arch_flag.cputype ==
		       thin_files[j].fat_arch.cputype &&
		   segaligns[i].arch_flag.cpusubtype ==
		       thin_files[j].fat_arch.cpusubtype){
/*
 Since this program has to guess at alignments and guesses high when unsure this
 check shouldn't be used so the the correct alignment can be specified by the
 the user.
		    if(thin_files[j].fat_arch.align > segaligns[i].align)
			fatal("specified segment alignment: %d for "
			      "architecture %s is less than the alignment: %d "
			      "required from the input file",
			      1 << segaligns[i].align,
			      segaligns[i].arch_flag.name,
			      1 << thin_files[j].fat_arch.align);
*/
		    thin_files[j].fat_arch.align = segaligns[i].align;
		    break;
		}
	    }
	    if(j == nthin_files)
		fatal("-segalign %s <value> specified but resulting fat "
		      "file does not contain that architecture",
		      segaligns[i].arch_flag.name);
	}

	/* sort the files by alignment to save space in the output file */
	qsort(thin_files, nthin_files, sizeof(struct thin_file),
	      (int (*)(const void *, const void *))cmp_qsort);

	/* Fill in the fat header and the fat_arch's offsets. */
	fat_header.magic = FAT_MAGIC;
	fat_header.nfat_arch = nthin_files;
	offset = sizeof(struct fat_header) +
		 nthin_files * sizeof(struct fat_arch);
	for(i = 0; i < nthin_files; i++){
	    offset = round(offset, 1 << thin_files[i].fat_arch.align);
	    thin_files[i].fat_arch.offset = offset;
	    offset += thin_files[i].fat_arch.size;
	}

	/*
	 * Create the output file.  The unlink() is done to handle the
	 * problem when the outputfile is not writable but the directory
	 * allows the file to be removed and thus created (since the file
	 * may not be there the return code of the unlink() is ignored).
	 */
	(void)unlink(output_file);
	if((fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC,
		      output_filemode)) == -1)
	    system_fatal("can't create output file: %s", output_file);

	/*
	 * If this is an extract_family_flag operation and the is just one
	 * thin file on the list don't create a fat file.
	 */
	if(extract_family_flag != TRUE || nthin_files != 1){
#ifdef __LITTLE_ENDIAN__
	    swap_fat_header(&fat_header, BIG_ENDIAN_BYTE_SEX);
#endif /* __LITTLE_ENDIAN__ */
	    if(write(fd, &fat_header, sizeof(struct fat_header)) !=
	       sizeof(struct fat_header))
		system_fatal("can't wtite fat header to output file: %s",
			     output_file);
#ifdef __LITTLE_ENDIAN__
	    swap_fat_header(&fat_header, LITTLE_ENDIAN_BYTE_SEX);
#endif /* __LITTLE_ENDIAN__ */
	    for(i = 0; i < nthin_files; i++){
#ifdef __LITTLE_ENDIAN__
		swap_fat_arch(&(thin_files[i].fat_arch), 1,BIG_ENDIAN_BYTE_SEX);
#endif /* __LITTLE_ENDIAN__ */
		if(write(fd, &(thin_files[i].fat_arch),
			 sizeof(struct fat_arch)) != sizeof(struct fat_arch))
		    system_fatal("can't write fat arch to output file: %s",
				 output_file);
#ifdef __LITTLE_ENDIAN__
		swap_fat_arch(&(thin_files[i].fat_arch), 1,
			      LITTLE_ENDIAN_BYTE_SEX);
    #endif /* __LITTLE_ENDIAN__ */
	    }
	}
	for(i = 0; i < nthin_files; i++){
	    if(extract_family_flag == FALSE)
		if(lseek(fd, thin_files[i].fat_arch.offset, L_SET) == -1)
		    system_fatal("can't lseek in output file: %s", output_file);
	    if(write(fd, thin_files[i].addr, thin_files[i].fat_arch.size)
	       != thin_files[i].fat_arch.size)
		system_fatal("can't write to output file: %s", output_file);
	}
	if(close(fd) == -1)
	    system_fatal("can't close output file: %s", output_file);
	if(archives_in_input == TRUE){
	    if(utime(output_file,
#ifndef __OPENSTEP__
		     &output_timep) == -1)
#else
		     output_timep) == -1)
#endif
		system_fatal("can't set the modifiy times for output file: %s",
			     output_file);
	}
}

/*
 * process_input_file() checks input file and breaks it down into thin files
 * for later operations.
 */
static
void
process_input_file(
struct input_file *input)
{
    int fd;
    struct stat stat_buf;
    unsigned long size, i, j;
    kern_return_t r;
    char *addr;
    struct thin_file *thin;
    struct mach_header *mhp, mh;
    struct load_command *lcp;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    enum bool swapped;

	/* Open the input file and map it in */
	if((fd = open(input->name, O_RDONLY)) == -1)
	    system_fatal("can't open input file: %s", input->name);
	if(fstat(fd, &stat_buf) == -1)
	    system_fatal("Can't stat input file: %s", input->name);
	size = stat_buf.st_size;
	/* pick up set uid, set gid and sticky text bits */
	output_filemode = stat_buf.st_mode & 07777;
	/*
	 * Select the eariliest modifiy time so that if the output file
	 * contains archives with table of contents lipo will not make them
	 * out of date.  This logic however could make an out of date table of
	 * contents appear up todate if another file is combined with it that
	 * has a date early enough.
	 */
#ifndef __OPENSTEP__
	if(output_timep.modtime == 0 ||
	   output_timep.modtime > stat_buf.st_mtime){
	    output_timep.actime = stat_buf.st_atime;
	    output_timep.modtime = stat_buf.st_mtime;
	}
#else
	if(output_timep[1] == 0 || output_timep[1] > stat_buf.st_mtime){
	    output_timep[0] = stat_buf.st_atime;
	    output_timep[1] = stat_buf.st_mtime;
	}
#endif
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&addr,
		       (boolean_t)TRUE, (vm_size_t)size)) != KERN_SUCCESS)
	    mach_fatal(r, "Can't map input file: %s", input->name);
	close(fd);

	/* Try to figure out what kind of file this is */

	/* see if this file is a fat file */
	if(size >= sizeof(struct fat_header) &&
#ifdef __BIG_ENDIAN__
	   *((unsigned long *)addr) == FAT_MAGIC)
#endif /* __BIG_ENDIAN__ */
#ifdef __LITTLE_ENDIAN__
	   *((unsigned long *)addr) == SWAP_LONG(FAT_MAGIC))
#endif /* __LITTLE_ENDIAN__ */
	{

	    if(input->arch_flag.name != NULL)
		fatal("architecture specifed for fat input file: %s "
		      "(architectures can't be specifed for fat input files)",
		      input->name);

	    input->fat_header = (struct fat_header *)addr;
#ifdef __LITTLE_ENDIAN__
	    swap_fat_header(input->fat_header, LITTLE_ENDIAN_BYTE_SEX);
#endif /* __LITTLE_ENDIAN__ */
	    if(sizeof(struct fat_header) + input->fat_header->nfat_arch *
	       sizeof(struct fat_arch) > size)
		fatal("truncated or malformed fat file (fat_arch structs would "
		      "extend past the end of the file) %s", input->name);
	    input->fat_arches = (struct fat_arch *)(addr +
						    sizeof(struct fat_header));
#ifdef __LITTLE_ENDIAN__
	    swap_fat_arch(input->fat_arches, input->fat_header->nfat_arch,
			  LITTLE_ENDIAN_BYTE_SEX);
#endif /* __LITTLE_ENDIAN__ */
	    for(i = 0; i < input->fat_header->nfat_arch; i++){
		if(input->fat_arches[i].offset + input->fat_arches[i].size >
		   size)
		    fatal("truncated or malformed fat file (offset plus size "
			  "of cputype (%d) cpusubtype (%d) extends past the "
			  "end of the file) %s", input->fat_arches[i].cputype,
			  input->fat_arches[i].cpusubtype, input->name);
		if(input->fat_arches[i].align > MAXSECTALIGN)
		    fatal("align (2^%lu) too large of fat file %s (cputype (%d)"
			  " cpusubtype (%d)) (maximum 2^%d)",
			  input->fat_arches[i].align, input->name,
			  input->fat_arches[i].cputype,
			  input->fat_arches[i].cpusubtype, MAXSECTALIGN);
		if(input->fat_arches[i].offset %
		   (1 << input->fat_arches[i].align) != 0)
		    fatal("offset %lu of fat file %s (cputype (%d) cpusubtype "
			  "(%d)) not aligned on it's alignment (2^%lu)",
			  input->fat_arches[i].offset, input->name,
			  input->fat_arches[i].cputype,
			  input->fat_arches[i].cpusubtype,
			  input->fat_arches[i].align);
	    }
	    for(i = 0; i < input->fat_header->nfat_arch; i++){
		for(j = i + 1; j < input->fat_header->nfat_arch; j++){
		    if(input->fat_arches[i].cputype ==
		         input->fat_arches[j].cputype &&
		       input->fat_arches[i].cpusubtype ==
		         input->fat_arches[j].cpusubtype)
		    fatal("fat file %s contains two of the same architecture "
			  "(cputype (%d) cpusubtype (%d))", input->name,
			  input->fat_arches[i].cputype,
			  input->fat_arches[i].cpusubtype);
		}
	    }

	    /* create a thin file struct for each arch in the fat file */
	    for(i = 0; i < input->fat_header->nfat_arch; i++){
		thin = new_thin();
		thin->name = input->name;
		thin->addr = addr + input->fat_arches[i].offset;
		thin->fat_arch = input->fat_arches[i];
		thin->from_fat = TRUE;
		if(input->fat_arches[i].size >= SARMAG &&
		   strncmp(thin->addr, ARMAG, SARMAG) == 0)
		    archives_in_input = TRUE;
	    }
	}
	/* see if this file is Mach-O file */
	else if(size >= sizeof(struct mach_header) &&
	        (*((unsigned long *)addr) == MH_MAGIC ||
	         *((unsigned long *)addr) == SWAP_LONG(MH_MAGIC))){

	    /* this is a Mach-O file so create a thin file struct for it */
	    thin = new_thin();
	    input->is_thin = TRUE;
	    thin->name = input->name;
	    thin->addr = addr;
	    mhp = (struct mach_header *)addr;
	    lcp = (struct load_command *)((char *)mhp +
					  sizeof(struct mach_header));
	    if(mhp->magic == SWAP_LONG(MH_MAGIC)){
		swapped = TRUE;
		mh = *mhp;
		swap_mach_header(&mh, get_host_byte_sex());
		mhp = &mh;
	    }
	    else
		swapped = FALSE;
	    thin->fat_arch.cputype = mhp->cputype;
	    thin->fat_arch.cpusubtype = mhp->cpusubtype;
	    thin->fat_arch.offset = 0;
	    thin->fat_arch.size = size;
	    thin->fat_arch.align = get_align(mhp, lcp, size, input->name,
					     swapped);

	    /* if the arch type is specified make sure it matches the object */
	    if(input->arch_flag.name != NULL)
		check_arch(input, thin);
	}
	/* see if this file is an archive file */
	else if(size >= SARMAG && strncmp(addr, ARMAG, SARMAG) == 0){

	    check_archive(input->name, addr, size, &cputype, &cpusubtype);
	    archives_in_input = TRUE;

	    /* create a thin file struct for this archive */
	    thin = new_thin();
	    thin->name = input->name;
	    thin->addr = addr;
	    thin->fat_arch.cputype = cputype;
	    thin->fat_arch.cpusubtype = cpusubtype;
	    thin->fat_arch.offset = 0;
	    thin->fat_arch.size = size;
	    thin->fat_arch.align = 2; /* 2^2, sizeof(long) */

	    /* if the arch type is specified make sure it matches the object */
	    if(input->arch_flag.name != NULL){
		if(cputype == 0){
		    thin->fat_arch.cputype = input->arch_flag.cputype;
		    thin->fat_arch.cpusubtype = input->arch_flag.cpusubtype;
		}
		else
		    check_arch(input, thin);
	    }
	    else{
		if(cputype == 0)
		    fatal("archive with no architecture specification: %s "
			  "(can't determine architecture for it)", input->name);
	    }
	}
	/* this file type is now known to be unknown to this program */
	else{

	    if(input->arch_flag.name != NULL){
		/* create a thin file struct for it */
		thin = new_thin();
		thin->name = input->name;
		thin->addr = addr;
		thin->fat_arch.cputype = input->arch_flag.cputype;
		thin->fat_arch.cpusubtype = input->arch_flag.cpusubtype;
		thin->fat_arch.offset = 0;
		thin->fat_arch.size = size;
		thin->fat_arch.align = 0;
	    }
	    else{
		fatal("can't figure out the architecture type of: %s",
		      input->name);
	    }
	}
}

/*
 * process_replace_file() checks the replacement file and maps it in for later
 * processing.
 */
static
void
process_replace_file(
struct replace *replace)
{
    int fd;
    struct stat stat_buf;
    unsigned long size;
    kern_return_t r;
    char *addr;
    struct mach_header *mhp, mh;
    struct load_command *lcp;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
    enum bool swapped;

	/* Open the replacement file and map it in */
	if((fd = open(replace->thin_file.name, O_RDONLY)) == -1)
	    system_fatal("can't open replacement file: %s",
			 replace->thin_file.name);
	if(fstat(fd, &stat_buf) == -1)
	    system_fatal("Can't stat replacement file: %s",
			 replace->thin_file.name);
	size = stat_buf.st_size;
	if((r = map_fd((int)fd, (vm_offset_t)0, (vm_offset_t *)&addr,
		       (boolean_t)TRUE, (vm_size_t)size)) != KERN_SUCCESS)
	    mach_fatal(r, "Can't map replacement file: %s",
		       replace->thin_file.name);
	close(fd);

	/* Try to figure out what kind of file this is */

	/* see if this file is a fat file */
	if(size >= sizeof(struct fat_header) &&
	   *((unsigned long *)addr) == FAT_MAGIC){

	    fatal("replacement file: %s is a fat file (must be a thin file)",
		  replace->thin_file.name);
	}
	/* see if this file is Mach-O file */
	else if(size >= sizeof(struct mach_header) &&
	        (*((unsigned long *)addr) == MH_MAGIC ||
	         *((unsigned long *)addr) == SWAP_LONG(MH_MAGIC))){

	    /* this is a Mach-O file so fill in the thin file struct for it */
	    replace->thin_file.addr = addr;
	    mhp = (struct mach_header *)addr;
	    lcp = (struct load_command *)((char *)mhp +
					  sizeof(struct mach_header));
	    if(mhp->magic == SWAP_LONG(MH_MAGIC)){
		swapped = TRUE;
		mh = *mhp;
		swap_mach_header(&mh, get_host_byte_sex());
		mhp = &mh;
	    }
	    else
		swapped = FALSE;
	    replace->thin_file.fat_arch.cputype = mhp->cputype;
	    replace->thin_file.fat_arch.cpusubtype = mhp->cpusubtype;
	    replace->thin_file.fat_arch.offset = 0;
	    replace->thin_file.fat_arch.size = size;
	    replace->thin_file.fat_arch.align =
		    get_align(mhp, lcp, size, replace->thin_file.name, swapped);
	}
	/* see if this file is an archive file */
	else if(size >= SARMAG && strncmp(addr, ARMAG, SARMAG) == 0){

	    check_archive(replace->thin_file.name, addr, size,
			  &cputype, &cpusubtype);

	    /* fill in the thin file struct for this archive */
	    replace->thin_file.addr = addr;
	    replace->thin_file.fat_arch.cputype = cputype;
	    replace->thin_file.fat_arch.cpusubtype = cpusubtype;
	    replace->thin_file.fat_arch.offset = 0;
	    replace->thin_file.fat_arch.size = size;
	    replace->thin_file.fat_arch.align = 2; /* 2^2, sizeof(long) */
	}
	else{
	    /* fill in the thin file struct for it */
	    replace->thin_file.addr = addr;
	    replace->thin_file.fat_arch.cputype = replace->arch_flag.cputype;
	    replace->thin_file.fat_arch.cpusubtype =
				    replace->arch_flag.cpusubtype;
	    replace->thin_file.fat_arch.offset = 0;
	    replace->thin_file.fat_arch.size = size;
	    replace->thin_file.fat_arch.align = 0;
	}

	if(replace->thin_file.fat_arch.cputype != replace->arch_flag.cputype ||
	   replace->thin_file.fat_arch.cpusubtype !=
						replace->arch_flag.cpusubtype)
	    fatal("specified architecture: %s for replacement file: %s does "
		  "not match the file's architecture", replace->arch_flag.name,
		  replace->thin_file.name);
}

/*
 * check_archive() checks an archive (mapped in at 'addr' of size 'size') to
 * make sure it can be included in a fat file (all members are the same
 * architecture, no fat files, etc).  It returns the cputype and cpusubtype.
 */
static
void
check_archive(
char *name,
char *addr,
unsigned long size,
cpu_type_t *cputype,
cpu_subtype_t *cpusubtype)
{
    unsigned long offset, magic, i, ar_name_size;
    struct mach_header mh;
    struct ar_hdr *ar_hdr;
    char *ar_name;

	/*
	 * Check this archive out to make sure that it does not contain
	 * any fat files and that all object files it contains have the
	 * same cputype and subsubtype.
	 */
	*cputype = 0;
	*cpusubtype = 0;
	offset = SARMAG;
	if(offset == size)
	    fatal("empty archive with no architecture specification: %s "
		  "(can't determine architecture for it)", name);
	if(offset != size && offset + sizeof(struct ar_hdr) > size)
	    fatal("truncated or malformed archive: %s (archive header of "
		  "first member extends past the end of the file)", name);
	while(size > offset){
	    ar_hdr = (struct ar_hdr *)(addr + offset);
	    offset += sizeof(struct ar_hdr);
	    if(strncmp(ar_hdr->ar_name, AR_EFMT1, sizeof(AR_EFMT1) - 1) == 0){
		check_extend_format_1(name, ar_hdr, size-offset, &ar_name_size);
		i = ar_name_size;
		ar_name = ar_hdr->ar_name + sizeof(struct ar_hdr);
	    }
	    else{
		i = size_ar_name(ar_hdr->ar_name);
		ar_name = ar_hdr->ar_name;
		ar_name_size = 0;
	    }
	    if(size + ar_name_size - offset > sizeof(unsigned long)){
		memcpy(&magic, addr + offset + ar_name_size,
		       sizeof(unsigned long));
		if(magic == FAT_MAGIC)
		    fatal("archive member %s(%.*s) is a fat file (not "
			  "allowed in an archive)", name, (int)i, ar_name);
		if((size - ar_name_size) - offset >=
		    sizeof(struct mach_header) &&
		   (magic == MH_MAGIC || magic == SWAP_LONG(MH_MAGIC))){
		    memcpy(&mh, addr + offset + ar_name_size,
			   sizeof(struct mach_header));
		    if(mh.magic == SWAP_LONG(MH_MAGIC))
			swap_mach_header(&mh, get_host_byte_sex());
		    if(*cputype == 0){
			*cputype = mh.cputype;
			*cpusubtype = mh.cpusubtype;
		    }
		    else if(*cputype != mh.cputype){
			fatal("archive member %s(%.*s) cputype (%d) and "
			      "cpusubtype (%d) does not match previous "
			      "archive members cputype (%d) and cpusubtype"
			      " (%d) (all members must match)", name,
			      (int)i, ar_name, mh.cputype, mh.cpusubtype,
			      *cputype, *cpusubtype);
		    }
		}
	    }
	    offset += round(strtoul(ar_hdr->ar_size, NULL, 10),
			    sizeof(short));
	}
}
/*
 * check_extend_format_1() checks the archive header for extened format1.
 */
static
void
check_extend_format_1(
char *name,
struct ar_hdr *ar_hdr,
unsigned long size_left,
unsigned long *member_name_size)
{
    char *p, *endp, buf[sizeof(ar_hdr->ar_name)+1];
    unsigned long ar_name_size;

	*member_name_size = 0;

	buf[sizeof(ar_hdr->ar_name)] = '\0';
	memcpy(buf, ar_hdr->ar_name, sizeof(ar_hdr->ar_name));
	p = buf + sizeof(AR_EFMT1) - 1;
	if(isdigit(*p) == 0)
	    fatal("archive: %s malformed (ar_name: %.*s for archive extend "
		  "format #1 starts with non-digit)", name,
		  (int)sizeof(ar_hdr->ar_name), ar_hdr->ar_name);
	ar_name_size = strtoul(p, &endp, 10);
	if(ar_name_size == ULONG_MAX && errno == ERANGE)
	    fatal("archive: %s malformed (size in ar_name: %.*s for archive "
		  "extend format #1 overflows unsigned long)", name,
		  (int)sizeof(ar_hdr->ar_name), ar_hdr->ar_name);
	while(*endp == ' ' && *endp != '\0')
	    endp++;
	if(*endp != '\0')
	    fatal("archive: %s malformed (size in ar_name: %.*s for archive "
		  "extend format #1 contains non-digit and non-space "
		  "characters)", name, (int)sizeof(ar_hdr->ar_name),
		  ar_hdr->ar_name);
	if(ar_name_size > size_left)
	    fatal("archive: %s truncated or malformed (archive name "
		"of member extends past the end of the file)", name);
	*member_name_size = ar_name_size;
}

/*
 * get_align is passed a pointer to a mach header and size of the object.  It
 * returns the segment alignment the object was created with.  It guesses but
 * it is conservative.  The maximum alignment is that the link editor will allow
 * MAXSECTALIGN and the minimum is the conserative alignment for a long which
 * appears in a mach object files (2^2 worst case for all current machines).
 */
static
unsigned long
get_align(
struct mach_header *mhp,
struct load_command *load_commands,
unsigned long size,
char *name,
enum bool swapped)
{
    unsigned long i, j, cur_align, align;
    struct load_command *lcp, l;
    struct segment_command *sgp, sg;
    struct section *sp, s;
    enum byte_sex host_byte_sex;

	host_byte_sex = get_host_byte_sex();

	/* set worst case the link editor uses first */
	cur_align = MAXSECTALIGN;
	if(mhp->sizeofcmds + sizeof(struct mach_header) > size)
	    fatal("truncated or malformed object (load commands would "
		  "extend past the end of the file) in: %s", name);
	lcp = load_commands;
	for(i = 0; i < mhp->ncmds; i++){
	    l = *lcp;
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		error("load command %lu size not a multiple of "
		      "sizeof(long) in: %s", i, name);
	    if(l.cmdsize <= 0)
		fatal("load command %lu size is less than or equal to zero "
		      "in: %s", i, name);
	    if((char *)lcp + l.cmdsize >
	       (char *)load_commands + mhp->sizeofcmds)
		fatal("load command %lu extends past end of all load "
		      "commands in: %s", i, name);
	    if(l.cmd == LC_SEGMENT){
		sgp = (struct segment_command *)lcp;
		sg = *sgp;
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);
		if(mhp->filetype == MH_OBJECT){
		    /* this is the minimum alignment, then take largest */
		    align = 2; /* 2^2 sizeof(long) */
		    sp = (struct section *)((char *)sgp +
					    sizeof(struct segment_command));
		    for(j = 0; j < sg.nsects; j++){
			s = *sp;
			if(swapped)
			    swap_section(&s, 1, host_byte_sex);
			if(s.align > align)
			    align = s.align;
			sp++;
		    }
		    if(align < cur_align)
			cur_align = align;
		}
		else{
		    /* guess the smallest alignment and use that */
		    align = guess_align(sg.vmaddr);
		    if(align < cur_align)
			cur_align = align;
		}
	    }
	    lcp = (struct load_command *)((char *)lcp + l.cmdsize);
	}
	return(cur_align);
}

/*
 * guess_align is passed a vmaddr of a segment and guesses what the segment
 * alignment was.  It uses the most conservative guess up to the maximum
 * alignment that the link editor uses.
 */
static
unsigned long
guess_align(
unsigned long vmaddr)
{
    unsigned long align, segalign;

	if(vmaddr == 0)
	    return(MAXSECTALIGN);

	align = 0;
	segalign = 1;
	while((segalign & vmaddr) == 0){
	    segalign = segalign << 1;
	    align++;
	}
	
	if(align < 2)
	    return(2);
	if(align > MAXSECTALIGN)
	    return(MAXSECTALIGN);

	return(align);
}

/*
 * print_arch() helps implement -info and -detailed_info by printing the
 * architecture name for the cputype and cpusubtype.
 */
static
void
print_arch(
struct fat_arch *fat_arch)
{
	switch(fat_arch->cputype){
	case CPU_TYPE_MC680x0:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_MC680x0_ALL:
		printf("m68k");
		break;
	    case CPU_SUBTYPE_MC68030_ONLY:
		printf("m68030");
		break;
	    case CPU_SUBTYPE_MC68040:
		printf("m68040");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_POWERPC:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_POWERPC_ALL:
		printf("ppc");
		break;
	    case CPU_SUBTYPE_POWERPC_601:
		printf("ppc601");
		break;
	    case CPU_SUBTYPE_POWERPC_603:
		printf("ppc603");
		break;
	    case CPU_SUBTYPE_POWERPC_603e:
		printf("ppc603e");
		break;
	    case CPU_SUBTYPE_POWERPC_603ev:
		printf("ppc603ev");
		break;
	    case CPU_SUBTYPE_POWERPC_604:
		printf("ppc604");
		break;
	    case CPU_SUBTYPE_POWERPC_604e:
		printf("ppc604e");
		break;
	    case CPU_SUBTYPE_POWERPC_750:
		printf("ppc750");
		break;
	    case CPU_SUBTYPE_POWERPC_7400:
		printf("ppc7400");
		break;
	    case CPU_SUBTYPE_POWERPC_7450:
		printf("ppc7450");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_MC88000:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_MC88000_ALL:
	    case CPU_SUBTYPE_MC88110:
		printf("m88k");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_I386:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_I386_ALL:
	    /* case CPU_SUBTYPE_386: same as above */
		printf("i386");
		break;
	    case CPU_SUBTYPE_486:
		printf("i486");
		break;
	    case CPU_SUBTYPE_486SX:
		printf("i486SX");
		break;
	    case CPU_SUBTYPE_PENT: /* same as 586 */
		printf("pentium");
		break;
	    case CPU_SUBTYPE_PENTPRO:
		printf("pentpro");
		break;
	    case CPU_SUBTYPE_PENTII_M3:
		printf("pentIIm3");
		break;
	    case CPU_SUBTYPE_PENTII_M5:
		printf("pentIIm5");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_I860:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_I860_ALL:
	    case CPU_SUBTYPE_I860_860:
		printf("i860");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_HPPA:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_HPPA_ALL:
	    case CPU_SUBTYPE_HPPA_7100LC:
		printf("hppa");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_SPARC:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_SPARC_ALL:
		printf("sparc");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_ANY:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_MULTIPLE:
		printf("any");
		break;
	    case CPU_SUBTYPE_LITTLE_ENDIAN:
		printf("little");
		break;
	    case CPU_SUBTYPE_BIG_ENDIAN:
		printf("big");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
print_arch_unknown:
	default:
	    printf("(cputype (%d) cpusubtype (%d))", fat_arch->cputype,
		   fat_arch->cpusubtype);
	    break;
	}
}

/*
 * print_cputype() helps implement -detailed_info by printing the cputype and
 * cpusubtype (symbolicly for the one's it knows about).
 */
static
void
print_cputype(
cpu_type_t cputype,
cpu_subtype_t cpusubtype)
{
	switch(cputype){
	case CPU_TYPE_MC680x0:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_MC680x0_ALL:
		printf("    cputype CPU_TYPE_MC680x0\n"
		       "    cpusubtype CPU_SUBTYPE_MC680x0_ALL\n");
		break;
	    case CPU_SUBTYPE_MC68030_ONLY:
		printf("    cputype CPU_TYPE_MC680x0\n"
		       "    cpusubtype CPU_SUBTYPE_MC68030_ONLY\n");
		break;
	    case CPU_SUBTYPE_MC68040:
		printf("    cputype CPU_TYPE_MC680x0\n"
		       "    cpusubtype CPU_SUBTYPE_MC68040\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_POWERPC:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_POWERPC_ALL:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_ALL\n");
		break;
	    case CPU_SUBTYPE_POWERPC_601:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_601\n");
		break;
	    case CPU_SUBTYPE_POWERPC_603:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_603\n");
		break;
	    case CPU_SUBTYPE_POWERPC_603e:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_603e\n");
		break;
	    case CPU_SUBTYPE_POWERPC_603ev:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_603ev\n");
		break;
	    case CPU_SUBTYPE_POWERPC_604:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_604\n");
		break;
	    case CPU_SUBTYPE_POWERPC_604e:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_604e\n");
		break;
	    case CPU_SUBTYPE_POWERPC_750:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_750\n");
		break;
	    case CPU_SUBTYPE_POWERPC_7400:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_7400\n");
		break;
	    case CPU_SUBTYPE_POWERPC_7450:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_7450\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_MC88000:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_MC88000_ALL:
		printf("    cputype CPU_TYPE_MC88000\n"
		       "    cpusubtype CPU_SUBTYPE_MC88000_ALL\n");
		break;
	    case CPU_SUBTYPE_MC88110:
		printf("    cputype CPU_TYPE_MC88000\n"
		       "    cpusubtype CPU_SUBTYPE_MC88110\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_I386:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_I386_ALL:
	    /* case CPU_SUBTYPE_386: same as above */
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_I386_ALL\n");
		break;
	    case CPU_SUBTYPE_486:
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_486\n");
		break;
	    case CPU_SUBTYPE_486SX:
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_486SX\n");
		break;
	    case CPU_SUBTYPE_PENT: /* same as 586 */
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_PENT\n");
		break;
	    case CPU_SUBTYPE_PENTPRO:
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_PENTPRO\n");
		break;
	    case CPU_SUBTYPE_PENTII_M3:
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_PENTII_M3\n");
		break;
	    case CPU_SUBTYPE_PENTII_M5:
		printf("    cputype CPU_TYPE_I386\n"
		       "    cpusubtype CPU_SUBTYPE_PENTII_M5\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_I860:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_I860_ALL:
		printf("    cputype CPU_TYPE_I860\n"
		       "    cpusubtype CPU_SUBTYPE_I860_ALL\n");
		break;
	    case CPU_SUBTYPE_I860_860:
		printf("    cputype CPU_TYPE_I860\n"
		       "    cpusubtype CPU_SUBTYPE_I860_860\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_HPPA:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_HPPA_ALL:
		printf("    cputype CPU_TYPE_HPPA\n"
		       "    cpusubtype CPU_SUBTYPE_HPPA_ALL\n");
		break;
	    case CPU_SUBTYPE_HPPA_7100LC:
		printf("    cputype CPU_TYPE_HPPA\n"
		       "    cpusubtype CPU_SUBTYPE_HPPA_7100LC\n");
	    	break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_SPARC:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_SPARC_ALL:
		printf("    cputype CPU_TYPE_SPARC\n"
		       "    cpusubtype CPU_SUBTYPE_SPARC_ALL\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_ANY:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_MULTIPLE:
		printf("    cputype CPU_TYPE_ANY\n"
		       "    cpusubtype CPU_SUBTYPE_MULTIPLE\n");
		break;
	    case CPU_SUBTYPE_LITTLE_ENDIAN:
		printf("    cputype CPU_TYPE_ANY\n"
		       "    cpusubtype CPU_SUBTYPE_LITTLE_ENDIAN\n");
		break;
	    case CPU_SUBTYPE_BIG_ENDIAN:
		printf("    cputype CPU_TYPE_ANY\n"
		       "    cpusubtype CPU_SUBTYPE_BIG_ENDIAN\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
print_arch_unknown:
	default:
	    printf("    cputype (%d)\n"
		   "    cpusubtype cpusubtype (%d)\n", cputype, cpusubtype);
	    break;
	}
}

/*
 * size_ar_name is used to return the size of the name of an archive member
 * for printing it without blanks (with printf string "%.*s").
 */
static
int
size_ar_name(
char *ar_name)
{
    int j;
    struct ar_hdr ar_hdr;

	for(j = 0; j < sizeof(ar_hdr.ar_name); j++){
	    if(ar_name[j] == ' ')
		break;
	}
	return(j);
}

/*
 * Create a new input file struct, clear it and return it.
 */
static
struct input_file *
new_input(void)
{
    struct input_file *input;

	input_files = reallocate(input_files,
		      (ninput_files + 1) * sizeof(struct input_file));
	input = input_files + ninput_files;
	ninput_files++;
	memset(input, '\0', sizeof(struct input_file));
	return(input);
}

/*
 * Create a new thin file struct, clear it and return it.
 */
static
struct thin_file *
new_thin(void)
{
    struct thin_file *thin;

	thin_files = reallocate(thin_files,
		      (nthin_files + 1) * sizeof(struct thin_file));
	thin = thin_files + nthin_files;
	nthin_files++;
	memset(thin, '\0', sizeof(struct thin_file));
	return(thin);
}

/*
 * Create a new arch_flag struct on the specified list, clear it and return it.
 */
static
struct arch_flag *
new_arch_flag(
struct arch_flag **arch_flags,
unsigned long *narch_flags)
{
    struct arch_flag *arch_flag;

	*arch_flags = reallocate(*arch_flags,
		      (*narch_flags + 1) * sizeof(struct arch_flag));
	arch_flag = *arch_flags + *narch_flags;
	*narch_flags = *narch_flags + 1;
	memset(arch_flag, '\0', sizeof(struct arch_flag));
	return(arch_flag);
}

/*
 * Create a new replace struct, clear it and return it.
 */
static
struct replace *
new_replace(void)
{
    struct replace *replace;

	replaces = reallocate(replaces,
		      (nreplaces + 1) * sizeof(struct replace));
	replace = replaces + nreplaces;
	nreplaces++;
	memset(replace, '\0', sizeof(struct replace));
	return(replace);
}

/*
 * Create a new segalign struct, clear it and return it.
 */
static
struct segalign *
new_segalign(void)
{
    struct segalign *segalign;

	segaligns = reallocate(segaligns,
		      (nsegaligns + 1) * sizeof(struct segalign));
	segalign = segaligns + nsegaligns;
	nsegaligns++;
	memset(segalign, '\0', sizeof(struct segalign));
	return(segalign);
}

/*
 * Function for qsort for comparing thin file's alignment
 */
static
int
cmp_qsort(
const struct thin_file *thin1,
const struct thin_file *thin2)
{
	return(thin1->fat_arch.align - thin2->fat_arch.align);
}

/*
 * round() rounds v to a multiple of r.
 */
static
unsigned long
round(
unsigned long v,
unsigned long r)
{
	r--;
	v += r;
	v &= ~(long)r;
	return(v);
}

/*
 * ispoweroftwo() returns TRUE or FALSE depending if x is a power of two.
 */
static
enum
bool
ispoweroftwo(
unsigned long x)
{
	if(x == 0)
	    return(TRUE);
	while((x & 0x1) != 0x1){
	    x >>= 1;
	}
	if((x & ~0x1) != 0)
	    return(FALSE);
	else
	    return(TRUE);
}

/*
 * check_arch is called when an input file is specified with a -arch flag input
 * and that the architecture can be determined from the input to check that
 * both architectures match.
 */
static
void
check_arch(
struct input_file *input,
struct thin_file *thin)
{
	if(input->arch_flag.cputype != thin->fat_arch.cputype)
	    fatal("specifed architecture type (%s) for file (%s) does "
		  "not match it's cputype (%d) and cpusubtype (%d) "
		  "(should be cputype (%d) and cpusubtype (%d))",
		  input->arch_flag.name, input->name,
		  thin->fat_arch.cputype, thin->fat_arch.cpusubtype,
		  input->arch_flag.cputype, input->arch_flag.cpusubtype);
}

/*
 * Print the current usage line and exit (by calling fatal).
 */
static
void
usage(void)
{
	fatal("Usage: %s [input_file] ... [-arch <arch_type> input_file] ... "
	      "[-info] [-detailed_info] [-output output_file] [-create] "
	      "[-thin <arch_type>] [-remove <arch_type>] ... "
	      "[-extract <arch_type>] ... [-extract_family <arch_type>] ..."
	      "[-replace <arch_type> <file_name>] ...", progname);
}
