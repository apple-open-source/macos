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
/*
 * This file intention is to beable to print the structures in an object file
 * and handle problems with reguard to alignment and bytesex.  The goal is to
 * print as much as possible even when things are truncated or trashed.  Both
 * a verbose (symbolic) and non-verbose modes are supported to aid in seeing
 * the values even if they are not correct.  As much as possible strict checks
 * on values of fields for correctness should be done (such as proper alignment)
 * and notations on errors should be printed.
 */
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <limits.h>
#include <ar.h>
#include <libc.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/reloc.h>
#include <mach-o/i860/reloc.h>
#include <mach-o/m88k/reloc.h>
#include <mach-o/ppc/reloc.h>
#include <mach-o/hppa/reloc.h>
#include <mach-o/sparc/reloc.h>
#include "stuff/ofile.h"
#include "stuff/allocate.h"
#include "stuff/errors.h"
#include "stuff/guess_short_name.h"
#include "ofile_print.h"

/* <mach/loader.h> */
/* The maximum section alignment allowed to be specified, as a power of two */
#define MAXSECTALIGN		15 /* 2**15 or 0x8000 */

static void print_arch(
    struct fat_arch *fat_arch);
static void print_cputype(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype);
static void print_unknown_state(
    char *begin,
    char *end,
    unsigned int count,
    enum bool swapped);
static void print_relocs(
    unsigned reloff,
    unsigned nreloc,
    struct section *sections,
    unsigned long nsects,
    enum bool swapped,
    struct mach_header *mh,
    char *object_addr,
    unsigned long object_size,
    struct nlist *symbols,
    unsigned long nsymbols,
    char *strings,
    unsigned long strings_size,
    enum bool verbose);
static void print_r_type(
    cpu_type_t cputype,
    unsigned long r_type,
    enum bool predicted);
static void print_cstring_char(
    char c);
static void print_literal4(
    unsigned long l,
    float f);
static void print_literal8(
    unsigned long l0,
    unsigned long l1,
    double d);
static int rel_bsearch(
    unsigned long *address,
    struct relocation_info *rel);

/*
 * Print the fat header and the fat_archs.  The caller is responsible for making
 * sure the structures are properly aligned and that the fat_archs is of the
 * size fat_header->nfat_arch * sizeof(struct fat_arch).
 */
void
print_fat_headers(
struct fat_header *fat_header,
struct fat_arch *fat_archs,
unsigned long size,
enum bool verbose)
{
    unsigned long i, j;

	if(verbose){
	    if(fat_header->magic == FAT_MAGIC)
		printf("fat_magic FAT_MAGIC\n");
	    else
		printf("fat_magic 0x%x\n", (unsigned int)(fat_header->magic));
	}
	else
	    printf("fat_magic 0x%x\n", (unsigned int)(fat_header->magic));
	printf("nfat_arch %lu", fat_header->nfat_arch);
	if(fat_header->nfat_arch == 0)
	    printf(" (malformed, contains zero architecture types)\n");
	else
	    printf("\n");

	for(i = 0; i < fat_header->nfat_arch; i++){
	    printf("architecture ");
	    for(j = 0; j < i - 1; j++){
		if(fat_archs[i].cputype != 0 && fat_archs[i].cpusubtype != 0 &&
		   fat_archs[i].cputype == fat_archs[j].cputype &&
		   fat_archs[i].cpusubtype == fat_archs[j].cpusubtype){
		    printf(" (illegal duplicate architecture) ");
		    break;
		}
	    }
	    if(verbose){
		print_arch(fat_archs + i);
		print_cputype(fat_archs[i].cputype, fat_archs[i].cpusubtype);
	    }
	    else{
		printf("%ld\n", i);
		printf("    cputype %d\n", fat_archs[i].cputype);
		printf("    cpusubtype %d\n", fat_archs[i].cpusubtype);
	    }
	    printf("    offset %lu", fat_archs[i].offset);
	    if(fat_archs[i].offset > size)
		printf(" (past end of file)");
	    if(fat_archs[i].offset % (1 << fat_archs[i].align) != 0)
		printf(" (not aligned on it's alignment (2^%lu))\n",
		       fat_archs[i].align);
	    else
		printf("\n");

	    printf("    size %lu", fat_archs[i].size);
	    if(fat_archs[i].offset + fat_archs[i].size > size)
		printf(" (past end of file)\n");
	    else
		printf("\n");

	    printf("    align 2^%lu (%d)", fat_archs[i].align,
		   1 << fat_archs[i].align);
	    if(fat_archs[i].align > MAXSECTALIGN)
		printf("( too large, maximum 2^%d)\n", MAXSECTALIGN);
	    else
		printf("\n");
	}
}

/*
 * print_arch() helps print_fat_headers by printing the
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
		printf("m68k\n");
		break;
	    case CPU_SUBTYPE_MC68030_ONLY:
		printf("m68030\n");
		break;
	    case CPU_SUBTYPE_MC68040:
		printf("m68040\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_MC88000:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_MC88000_ALL:
	    case CPU_SUBTYPE_MC88110:
		printf("m88k\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_I386:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_I386_ALL:
	    /* case CPU_SUBTYPE_386: same as above */
		printf("i386\n");
		break;
	    case CPU_SUBTYPE_486:
		printf("i486\n");
		break;
	    case CPU_SUBTYPE_486SX:
		printf("i486SX\n");
		break;
	    case CPU_SUBTYPE_PENT: /* same as 586 */
		printf("pentium\n");
		break;
	    case CPU_SUBTYPE_PENTPRO:
		printf("pentpro\n");
		break;
	    case CPU_SUBTYPE_PENTII_M3:
		printf("pentIIm3\n");
		break;
	    case CPU_SUBTYPE_PENTII_M5:
		printf("pentIIm5\n");
		break;
	    default:
		printf("intel x86 family %d model %d\n",
		       CPU_SUBTYPE_INTEL_FAMILY(fat_arch->cpusubtype),
		       CPU_SUBTYPE_INTEL_MODEL(fat_arch->cpusubtype));
		break;
	    }
	    break;
	case CPU_TYPE_I860:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_I860_ALL:
	    case CPU_SUBTYPE_I860_860:
		printf("i860\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_POWERPC:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_POWERPC_ALL:
		printf("ppc\n");
		break;
	    case CPU_SUBTYPE_POWERPC_601:
		printf("ppc601\n");
		break;
	    case CPU_SUBTYPE_POWERPC_602:
		printf("ppc602\n");
		break;
	    case CPU_SUBTYPE_POWERPC_603:
		printf("ppc603\n");
		break;
	    case CPU_SUBTYPE_POWERPC_603e:
		printf("ppc603e\n");
		break;
	    case CPU_SUBTYPE_POWERPC_603ev:
		printf("ppc603ev\n");
		break;
	    case CPU_SUBTYPE_POWERPC_604:
		printf("ppc604\n");
		break;
	    case CPU_SUBTYPE_POWERPC_604e:
		printf("ppc604e\n");
		break;
	    case CPU_SUBTYPE_POWERPC_620:
		printf("ppc620\n");
		break;
	    case CPU_SUBTYPE_POWERPC_750:
		printf("ppc750\n");
		break;
	    case CPU_SUBTYPE_POWERPC_7400:
		printf("ppc7400\n");
		break;
	    case CPU_SUBTYPE_POWERPC_7450:
		printf("ppc7450\n");
		break;
	    case CPU_SUBTYPE_POWERPC_970:
		printf("ppc970\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_VEO:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_VEO_1:
		printf("veo1\n");
		break;
	    case CPU_SUBTYPE_VEO_2:
		printf("veo2\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_HPPA:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_HPPA_ALL:
	    case CPU_SUBTYPE_HPPA_7100LC:
		printf("hppa\n");
	    break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_SPARC:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_SPARC_ALL:
		printf("sparc\n");
	    break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_ANY:
	    switch(fat_arch->cpusubtype){
	    case CPU_SUBTYPE_MULTIPLE:
		printf("any\n");
		break;
	    case CPU_SUBTYPE_LITTLE_ENDIAN:
		printf("little\n");
		break;
	    case CPU_SUBTYPE_BIG_ENDIAN:
		printf("big\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
print_arch_unknown:
	default:
	    printf("cputype (%d) cpusubtype (%d)\n", fat_arch->cputype,
		   fat_arch->cpusubtype);
	    break;
	}
}

/*
 * print_cputype() helps print_fat_headers by printing the cputype and
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
	    case CPU_SUBTYPE_POWERPC_602:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_602\n");
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
	    case CPU_SUBTYPE_POWERPC_620:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_620\n");
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
	    case CPU_SUBTYPE_POWERPC_970:
		printf("    cputype CPU_TYPE_POWERPC\n"
		       "    cpusubtype CPU_SUBTYPE_POWERPC_970\n");
		break;
	    default:
		goto print_arch_unknown;
	    }
	    break;
	case CPU_TYPE_VEO:
	    switch(cpusubtype){
	    case CPU_SUBTYPE_VEO_1:
		printf("    cputype CPU_TYPE_VEO\n"
		       "    cpusubtype CPU_SUBTYPE_VEO_1\n");
		break;
	    case CPU_SUBTYPE_VEO_2:
		printf("    cputype CPU_TYPE_VEO\n"
		       "    cpusubtype CPU_SUBTYPE_VEO_2\n");
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
		   "    cpusubtype (%d)\n", cputype, cpusubtype);
	    break;
	}
}

/*
 * Print the archive header.  The format is constant width character fields
 * blank padded.  So the trailing blanks are stripped and full field widths
 * are handled correctly.
 */
void
print_ar_hdr(
struct ar_hdr *ar_hdr,
char *member_name,
unsigned long member_name_size,
enum bool verbose)
{
    long i;
    unsigned long j, mode, date;
    char *p, *endp;

    char date_buf[sizeof(ar_hdr->ar_date) + 1];
    char  uid_buf[sizeof(ar_hdr->ar_uid)  + 1];
    char  gid_buf[sizeof(ar_hdr->ar_gid)  + 1];
    char mode_buf[sizeof(ar_hdr->ar_mode) + 1];
    char size_buf[sizeof(ar_hdr->ar_size) + 1];

	memcpy(date_buf, ar_hdr->ar_date, sizeof(ar_hdr->ar_date));
	for(i = sizeof(ar_hdr->ar_date) - 1; i >= 0 && date_buf[i] == ' '; i--)
	    date_buf[i] = '\0';
	date_buf[sizeof(ar_hdr->ar_date)] = '\0';

	memcpy(uid_buf, ar_hdr->ar_uid, sizeof(ar_hdr->ar_uid));
	for(i = sizeof(ar_hdr->ar_uid) - 1; i >= 0 && uid_buf[i] == ' '; i--)
	    uid_buf[i] = '\0';
	uid_buf[sizeof(ar_hdr->ar_uid)] = '\0';

	memcpy(gid_buf, ar_hdr->ar_gid, sizeof(ar_hdr->ar_gid));
	for(i = sizeof(ar_hdr->ar_gid) - 1; i >= 0 && gid_buf[i] == ' '; i--)
	    gid_buf[i] = '\0';
	gid_buf[sizeof(ar_hdr->ar_gid)] = '\0';

	memcpy(mode_buf, ar_hdr->ar_mode, sizeof(ar_hdr->ar_mode));
	for(i = sizeof(ar_hdr->ar_mode) - 1; i >= 0 && mode_buf[i] == ' '; i--)
	    mode_buf[i] = '\0';
	mode_buf[sizeof(ar_hdr->ar_mode)] = '\0';

	memcpy(size_buf, ar_hdr->ar_size, sizeof(ar_hdr->ar_size));
	for(i = sizeof(ar_hdr->ar_size) - 1; i >= 0 && size_buf[i] == ' '; i--)
	    size_buf[i] = '\0';
	size_buf[sizeof(ar_hdr->ar_size)] = '\0';


	if(verbose == TRUE){
	    mode = strtoul(mode_buf, &endp, 8);
	    if(*endp != '\0')
		printf("(mode: \"%s\" contains non-octal chars) ", mode_buf);
	    switch(mode & S_IFMT){
	    case S_IFDIR:
		printf("d");
		break;
	    case S_IFCHR:
		printf("c");
		break;
	    case S_IFBLK:
		printf("b");
		break;
	    case S_IFREG:
		printf("-");
		break;
	    case S_IFLNK:
		printf("l");
		break;
	    case S_IFSOCK:
		printf("s");
		break;
	    default:
		printf("?");
		break;
	    }

	    /* owner permissions */
	    if(mode & S_IREAD)
		printf("r");
	    else
		printf("-");
	    if(mode & S_IWRITE)
		printf("w");
	    else
		printf("-");
	    if(mode & S_ISUID)
		printf("s");
	    else if(mode & S_IEXEC)
		printf("x");
	    else
		printf("-");

	    /* group permissions */
	    if(mode & (S_IREAD >> 3))
		printf("r");
	    else
		printf("-");
	    if(mode & (S_IWRITE >> 3))
		printf("w");
	    else
		printf("-");
	    if(mode & S_ISGID)
		printf("s");
	    else if(mode & (S_IEXEC >> 3))
		printf("x");
	    else
		printf("-");

	    /* other permissions */
	    if(mode & (S_IREAD >> 6))
		printf("r");
	    else
		printf("-");
	    if(mode & (S_IWRITE >> 6))
		printf("w");
	    else
		printf("-");
	    if(mode & S_ISVTX)
		printf("t");
	    else if(mode & (S_IEXEC >> 6))
		printf("x");
	    else
		printf("-");
	}
	else
	    /* printf("0%03o ", mode & 0777); */
	    printf("0%s ", mode_buf);

	printf("%3s/%-3s %5s ", uid_buf, gid_buf, size_buf);

	/*
	 * Since cime(3) returns a 26 character string of the form:
	 * "Sun Sep 16 01:03:52 1973\n\0"
	 * and the new line is not wanted a '\0' is placed there.
	 */
	if(verbose){
	    date = strtoul(date_buf, &endp, 10);
	    if(*endp != '\0')
		printf("(date: \"%s\" contains non-decimal chars) ", date_buf);
	    p = ctime((const long *)&date);
	    p[24] = '\0';
	    printf("%s ", p);
	}
	else
	    printf("%s ", date_buf);

	if(verbose){
	    printf("%.*s", (int)member_name_size, member_name);
	}
	else{
	    j = size_ar_name(ar_hdr);
	    printf("%.*s", (int)j, ar_hdr->ar_name);
	}

	if(memcmp(ar_hdr->ar_fmag, ARFMAG, sizeof(ARFMAG) - 1) == 0)
	    printf("\n");
	else
	    printf(" (ar_fmag not ARFMAG)\n");
}

/*
 * print_library_toc prints the table of contents of the a library.  It is
 * converted to the host byte sex if toc_byte_sex is not the host byte sex.
 * The problem with determing the byte sex of the table of contents is left
 * to the caller.  The determination is based on the byte sex of the object
 * files contained in the library (this can still present a problem since the
 * object files could be of differing byte sex in an erroneous library).  There
 * is no problem of a library containing no objects with respect to the byte
 * sex of the table of contents since the table of contents would be made up
 * of two binary unsigned long zeros which are the same in either byte sex.
 */
void
print_library_toc(
struct ar_hdr *toc_ar_hdr,
char *toc_name,
unsigned long toc_name_size,
char *toc_addr,
unsigned long toc_size,
enum byte_sex toc_byte_sex,
char *library_name,
char *library_addr,
unsigned long library_size,
char *arch_name,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    unsigned long ran_size, nranlibs, str_size, i, toc_offset, member_name_size;
    struct ranlib *ranlibs;
    char *strings, *member_name;
    struct ar_hdr *ar_hdr;
    int n;
    char buf[20];

	host_byte_sex = get_host_byte_sex();
	toc_offset = 0;
	strings = NULL;

	if(toc_offset + sizeof(unsigned long) > toc_size){
	    error_with_arch(arch_name, "truncated table of contents in: "
		"%s(%.*s) (size of ranlib structs extends past the end of the "
		"table of contents member)", library_name, (int)toc_name_size,
		toc_name);
	    return;
	}
	memcpy((char *)&ran_size, toc_addr + toc_offset, sizeof(unsigned long));
	if(toc_byte_sex != host_byte_sex)
	    ran_size = SWAP_LONG(ran_size);
	toc_offset += sizeof(unsigned long);

	if(toc_offset + ran_size > toc_size){
	    error_with_arch(arch_name, "truncated table of contents in: "
		"%s(%.*s) (ranlib structures extends past the end of the "
		"table of contents member)", library_name, (int)toc_name_size,
		toc_name);
	    return;
	}
	ranlibs = allocate(ran_size);
	memcpy((char *)ranlibs, toc_addr + toc_offset, ran_size);
	nranlibs = ran_size / sizeof(struct ranlib);
	if(toc_byte_sex != host_byte_sex)
	    swap_ranlib(ranlibs, nranlibs, host_byte_sex);
	toc_offset += ran_size;

	if(verbose){
	    if(toc_offset + sizeof(unsigned long) > toc_size){
		error_with_arch(arch_name, "truncated table of contents in: "
		    "%s(%.*s) (size of ranlib strings extends past the end of "
		    "the table of contents member)", library_name,
		    (int)toc_name_size, toc_name);
		free(ranlibs);
		return;
	    }
	    memcpy((char *)&str_size, toc_addr + toc_offset,
		   sizeof(unsigned long));
	    if(toc_byte_sex != host_byte_sex)
		str_size = SWAP_LONG(str_size);
	    toc_offset += sizeof(unsigned long);

	    if(toc_offset + str_size > toc_size){
		error_with_arch(arch_name, "truncated table of contents in: "
		    "%s(%.*s) (ranlib strings extends past the end of the "
		    "table of contents member)", library_name,
		    (int)toc_name_size, toc_name);
		free(ranlibs);
		return;
	    }
	    strings = toc_addr + toc_offset;
	}

	printf("Table of contents from: %s(%.*s)", library_name,
	       (int)toc_name_size, toc_name);
	if(arch_name != NULL)
	    printf(" (for architecture %s)\n", arch_name);
	else
	    printf("\n");
	printf("size of ranlib structures: %lu (number %lu)\n", ran_size,
	       nranlibs);
	if(verbose){
	    printf("size of strings: %lu", str_size);
	    if(str_size % sizeof(long) != 0)
		printf(" (not multiple of sizeof(long))\n");
	    else
		printf("\n");
	}
	if(verbose)
	    printf("object           symbol name\n");
	else
	    printf("object offset  string index\n");

	for(i = 0; i < nranlibs; i++){
	    if(verbose){
		if(ranlibs[i].ran_off + sizeof(struct ar_hdr) <= library_size){
		    ar_hdr = (struct ar_hdr *)
			     (library_addr + ranlibs[i].ran_off);
		    if(strncmp(ar_hdr->ar_name, AR_EFMT1,
			       sizeof(AR_EFMT1) - 1) == 0){
			member_name = ar_hdr->ar_name + sizeof(struct ar_hdr);
			member_name_size = strtoul(ar_hdr->ar_name +
				sizeof(AR_EFMT1) - 1, NULL, 10);
			while(member_name_size > 0 &&
			      member_name[member_name_size - 1] == '\0')
			    member_name_size--;
			printf("%-.*s ", (int)member_name_size, member_name);
			if(member_name_size < 16)
			    printf("%-.*s", (int)(16 - member_name_size),
				   "                ");
		    }
		    else{
			printf("%-.16s ", ar_hdr->ar_name);
		    }
		}
		else{
		    n = sprintf(buf, "?(%ld) ", (long int)ranlibs[i].ran_off);
		    printf("%s%.*s", buf, 17 - n, "              ");
		}
		if(ranlibs[i].ran_un.ran_strx < str_size)
		    printf("%s\n", strings + ranlibs[i].ran_un.ran_strx);
		else
		    printf("?(%ld)\n", (long int)ranlibs[i].ran_un.ran_strx);
	    }
	    else{
		printf("%-14ld %ld\n", (long int)ranlibs[i].ran_off,
			(long int)ranlibs[i].ran_un.ran_strx);
	    }
	}

	free(ranlibs);
}

/*
 * Print the mach header.  It is assumed that the structure pointed to by mh
 * is aligned correctly and in the host byte sex.  In this way it is up to the
 * caller to determine he has a mach_header and what byte sex it is and get it
 * aligned in the host byte sex for this routine.
 */
void
print_mach_header(
struct mach_header *mh,
enum bool verbose)
{
    unsigned long flags;

	printf("Mach header\n");
	printf("      magic cputype cpusubtype   filetype ncmds sizeofcmds "
	       "     flags\n");
	if(verbose){
	    if(mh->magic == MH_MAGIC)
		printf("   MH_MAGIC");
	    else
		printf(" 0x%08x", (unsigned int)mh->magic);
	    switch(mh->cputype){
	    case CPU_TYPE_VAX:
		printf("     VAX");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_VAX780:
		    printf("     VAX780");
		    break;
		case CPU_SUBTYPE_VAX785:
		    printf("     VAX785");
		    break;
		case CPU_SUBTYPE_VAX750:
		    printf("     VAX750");
		    break;
		case CPU_SUBTYPE_VAX730:
		    printf("     VAX730");
		    break;
		case CPU_SUBTYPE_UVAXI:
		    printf("     UVAXI");
		    break;
		case CPU_SUBTYPE_UVAXII:
		    printf("     UVAXII");
		    break;
		case CPU_SUBTYPE_VAX8200:
		    printf("    VAX8200");
		    break;
		case CPU_SUBTYPE_VAX8500:
		    printf("    VAX8500");
		    break;
		case CPU_SUBTYPE_VAX8600:
		    printf("    VAX8600");
		    break;
		case CPU_SUBTYPE_VAX8650:
		    printf("    VAX8650");
		    break;
		case CPU_SUBTYPE_VAX8800:
		    printf("    VAX8800");
		    break;
		case CPU_SUBTYPE_UVAXIII:
		    printf("    UVAXIII");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_ROMP:
		printf("    ROMP");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_RT_PC:
		    printf("      RT_PC");
		    break;
		case CPU_SUBTYPE_RT_APC:
		    printf("     RT_APC");
		    break;
		case CPU_SUBTYPE_RT_135:
		    printf("     RT_135");
		    break;

		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_NS32032:
		printf(" NS32032");
		goto NS32;
	    case CPU_TYPE_NS32332:
		printf(" NS32332");
NS32:
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_MMAX_DPC:
		    printf("   MMAX_DPC");
		    break;
		case CPU_SUBTYPE_SQT:
		    printf("        SQT");
		    break;
		case CPU_SUBTYPE_MMAX_APC_FPU:
		    printf(" MMAX_APC_FPC");
		    break;
		case CPU_SUBTYPE_MMAX_APC_FPA:
		    printf(" MMAX_APC_FPA");
		    break;
		case CPU_SUBTYPE_MMAX_XPC:
		    printf("   MMAX_XPC");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_MC680x0:
		printf(" MC680x0");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_MC680x0_ALL:
		    printf("        ALL");
		    break;
		case CPU_SUBTYPE_MC68030_ONLY:
		    printf("    MC68030");
		    break;
		case CPU_SUBTYPE_MC68040:
		    printf("    MC68040");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_MC88000:
		printf(" MC88000");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_MC88000_ALL:
		    printf("        ALL");
		    break;
		case CPU_SUBTYPE_MC88100:
		    printf("    MC88100");
		    break;
		case CPU_SUBTYPE_MC88110:
		    printf("    MC88110");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_I860:
		printf("    I860");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_I860_ALL:
		    printf("        ALL");
		    break;
		case CPU_SUBTYPE_I860_860:
		    printf("        860");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_I386:
		printf("    I386");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_I386_ALL:
		/* case CPU_SUBTYPE_386: same as above */
		    printf("        ALL");
		    break;
		case CPU_SUBTYPE_486:
		    printf("        486");
		    break;
		case CPU_SUBTYPE_486SX:
		    printf("      486SX");
		    break;
		case CPU_SUBTYPE_PENT: /* same as 586 */
		    printf("       PENT");
		    break;
		case CPU_SUBTYPE_PENTPRO:
		    printf("    PENTPRO");
		    break;
		case CPU_SUBTYPE_PENTII_M3:
		    printf("  PENTII_M3");
		    break;
		case CPU_SUBTYPE_PENTII_M5:
		    printf("  PENTII_M5");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_POWERPC:
		printf("     PPC");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_POWERPC_ALL:
		    printf("        ALL");
		    break;
		case CPU_SUBTYPE_POWERPC_601:
		    printf("     ppc601");
		    break;
		case CPU_SUBTYPE_POWERPC_602:
		    printf("     ppc602");
		    break;
		case CPU_SUBTYPE_POWERPC_603:
		    printf("     ppc603");
		    break;
		case CPU_SUBTYPE_POWERPC_603e:
		    printf("    ppc603e");
		    break;
		case CPU_SUBTYPE_POWERPC_603ev:
		    printf("   ppc603ev");
		    break;
		case CPU_SUBTYPE_POWERPC_604:
		    printf("     ppc604");
		    break;
		case CPU_SUBTYPE_POWERPC_604e:
		    printf("    ppc604e");
		    break;
		case CPU_SUBTYPE_POWERPC_620:
		    printf("     ppc620");
		    break;
		case CPU_SUBTYPE_POWERPC_750:
		    printf("     ppc750");
		    break;
		case CPU_SUBTYPE_POWERPC_7400:
		    printf("    ppc7400");
		    break;
		case CPU_SUBTYPE_POWERPC_7450:
		    printf("    ppc7450");
		    break;
		case CPU_SUBTYPE_POWERPC_970:
		    printf("     ppc970");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_VEO:
		printf("     VEO");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_VEO_1:
		    printf("       veo1");
		    break;
		case CPU_SUBTYPE_VEO_2:
		    printf("       veo2");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_HPPA:
		printf("    HPPA");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_HPPA_ALL:
		    printf("        ALL");
		    break;
		case CPU_SUBTYPE_HPPA_7100LC:
		    printf("  HPPA_7100LC");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    case CPU_TYPE_SPARC:
		printf("   SPARC");
		switch(mh->cpusubtype){
		case CPU_SUBTYPE_SPARC_ALL:
		    printf("        ALL");
		    break;
		default:
		    printf(" %10d", mh->cpusubtype);
		    break;
		}
		break;
	    default:
		printf(" %7d %10d", mh->cputype, mh->cpusubtype);
		break;
	    }
	    switch(mh->filetype){
	    case MH_OBJECT:
		printf("     OBJECT");
		break;
	    case MH_EXECUTE:
		printf("    EXECUTE");
		break;
	    case MH_FVMLIB:
		printf("     FVMLIB");
		break;
	    case MH_CORE:
		printf("       CORE");
		break;
	    case MH_PRELOAD:
		printf("    PRELOAD");
		break;
	    case MH_DYLIB:
		printf("      DYLIB");
		break;
	    case MH_DYLIB_STUB:
		printf(" DYLIB_STUB");
		break;
	    case MH_DYLINKER:
		printf("   DYLINKER");
		break;
	    case MH_BUNDLE:
		printf("     BUNDLE");
		break;
	    default:
		printf(" %10lu", mh->filetype);
		break;
	    }
	    printf(" %5lu %10lu", mh->ncmds, mh->sizeofcmds);
	    flags = mh->flags;
	    if(flags & MH_NOUNDEFS){
		printf("   NOUNDEFS");
		flags &= ~MH_NOUNDEFS;
	    }
	    if(flags & MH_INCRLINK){
		printf(" INCRLINK");
		flags &= ~MH_INCRLINK;
	    }
	    if(flags & MH_DYLDLINK){
		printf(" DYLDLINK");
		flags &= ~MH_DYLDLINK;
	    }
	    if(flags & MH_BINDATLOAD){
		printf(" BINDATLOAD");
		flags &= ~MH_BINDATLOAD;
	    }
	    if(flags & MH_PREBOUND){
		printf(" PREBOUND");
		flags &= ~MH_PREBOUND;
	    }
	    if(flags & MH_SPLIT_SEGS){
		printf(" SPLIT_SEGS");
		flags &= ~MH_SPLIT_SEGS;
	    }
	    if(flags & MH_LAZY_INIT){
		printf(" LAZY_INIT");
		flags &= ~MH_LAZY_INIT;
	    }
	    if(flags & MH_TWOLEVEL){
		printf(" TWOLEVEL");
		flags &= ~MH_TWOLEVEL;
	    }
	    if(flags & MH_FORCE_FLAT){
		printf(" FORCE_FLAT");
		flags &= ~MH_FORCE_FLAT;
	    }
	    if(flags & MH_NOMULTIDEFS){
		printf(" NOMULTIDEFS");
		flags &= ~MH_NOMULTIDEFS;
	    }
	    if(flags & MH_NOFIXPREBINDING){
		printf(" NOFIXPREBINDING");
		flags &= ~MH_NOFIXPREBINDING;
	    }
	    if(flags != 0 || mh->flags == 0)
		printf(" 0x%08x", (unsigned int)flags);
	    printf("\n");
	}
	else{
	    printf(" 0x%08x %7d %10d %10lu %5lu %10lu 0x%08x\n",
		   (unsigned int)mh->magic, mh->cputype, mh->cpusubtype,
		   mh->filetype, mh->ncmds, mh->sizeofcmds,
		   (unsigned int)mh->flags);
	}
}

/*
 * Print the load commands.  It is assumed that the structure pointed to by mh
 * is aligned correctly and in the host byte sex.  The load commands pointed to
 * by load_commands can have any alignment, are in the specified byte_sex, and
 * must be at least mh->sizeofcmds in length.
 */
void
print_loadcmds(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
unsigned long object_size,
enum bool verbose,
enum bool very_verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, k, left, size, *unknown, nsyms;
    char *p, *begin, *end;
    struct load_command *lc, l;
    struct segment_command sg;
    struct section s;
    struct symtab_command st;
    struct dysymtab_command dyst;
    struct symseg_command ss;
    struct fvmlib_command fl;
    struct dylib_command dl;
    struct prebound_dylib_command pbdylib;
    struct sub_framework_command sub;
    struct sub_umbrella_command usub;
    struct sub_library_command lsub;
    struct sub_client_command csub;
    struct fvmfile_command ff;
    struct dylinker_command dyld;
    struct routines_command rc;
    struct twolevel_hints_command hints;
    struct prebind_cksum_command cs;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	nsyms = ULONG_MAX;
	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    printf("Load command %lu\n", i);

	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of sizeof(long)\n",
		       i);
	    if((char *)lc + l.cmdsize > (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load commands\n",
		       i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);
		print_segment_command(&sg, object_size, verbose);

		p = (char *)lc + sizeof(struct segment_command);
		for(j = 0 ; j < sg.nsects ; j++){
		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds){
			printf("section structure command extends past end of "
			       "load commands\n");
		    }
		    left = mh->sizeofcmds - (p - (char *)load_commands);
		    memset((char *)&s, '\0', sizeof(struct section));
		    size = left < sizeof(struct section) ?
			   left : sizeof(struct section);
		    memcpy((char *)&s, p, size);
		    if(swapped)
			swap_section(&s, 1, host_byte_sex);
		    print_section(&s, &sg, mh, object_size, verbose);

		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds)
			return;
		    p += size;
		}
		break;

	    case LC_SYMTAB:
		memset((char *)&st, '\0', sizeof(struct symtab_command));
		size = left < sizeof(struct symtab_command) ?
		       left : sizeof(struct symtab_command);
		memcpy((char *)&st, (char *)lc, size);
		if(swapped)
		    swap_symtab_command(&st, host_byte_sex);
		nsyms = st.nsyms;
		print_symtab_command(&st, object_size);
		break;

	    case LC_DYSYMTAB:
		memset((char *)&dyst, '\0', sizeof(struct dysymtab_command));
		size = left < sizeof(struct dysymtab_command) ?
		       left : sizeof(struct dysymtab_command);
		memcpy((char *)&dyst, (char *)lc, size);
		if(swapped)
		    swap_dysymtab_command(&dyst, host_byte_sex);
		print_dysymtab_command(&dyst, nsyms, object_size);
		break;

	    case LC_SYMSEG:
		memset((char *)&ss, '\0', sizeof(struct symseg_command));
		size = left < sizeof(struct symseg_command) ?
		       left : sizeof(struct symseg_command);
		memcpy((char *)&ss, (char *)lc, size);
		if(swapped)
		    swap_symseg_command(&ss, host_byte_sex);
		print_symseg_command(&ss, object_size);
		break;

	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
		memset((char *)&fl, '\0', sizeof(struct fvmlib_command));
		size = left < sizeof(struct fvmlib_command) ?
		       left : sizeof(struct fvmlib_command);
		memcpy((char *)&fl, (char *)lc, size);
		if(swapped)
		    swap_fvmlib_command(&fl, host_byte_sex);
		print_fvmlib_command(&fl, lc);
		break;

	    case LC_ID_DYLIB:
	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		memset((char *)&dl, '\0', sizeof(struct dylib_command));
		size = left < sizeof(struct dylib_command) ?
		       left : sizeof(struct dylib_command);
		memcpy((char *)&dl, (char *)lc, size);
		if(swapped)
		    swap_dylib_command(&dl, host_byte_sex);
		print_dylib_command(&dl, lc);
		break;

	    case LC_SUB_FRAMEWORK:
		memset((char *)&sub, '\0',sizeof(struct sub_framework_command));
		size = left < sizeof(struct sub_framework_command) ?
		       left : sizeof(struct sub_framework_command);
		memcpy((char *)&sub, (char *)lc, size);
		if(swapped)
		    swap_sub_framework_command(&sub, host_byte_sex);
		print_sub_framework_command(&sub, lc);
		break;

	    case LC_SUB_UMBRELLA:
		memset((char *)&usub, '\0',sizeof(struct sub_umbrella_command));
		size = left < sizeof(struct sub_umbrella_command) ?
		       left : sizeof(struct sub_umbrella_command);
		memcpy((char *)&usub, (char *)lc, size);
		if(swapped)
		    swap_sub_umbrella_command(&usub, host_byte_sex);
		print_sub_umbrella_command(&usub, lc);
		break;

	    case LC_SUB_LIBRARY:
		memset((char *)&lsub, '\0',sizeof(struct sub_library_command));
		size = left < sizeof(struct sub_library_command) ?
		       left : sizeof(struct sub_library_command);
		memcpy((char *)&lsub, (char *)lc, size);
		if(swapped)
		    swap_sub_library_command(&lsub, host_byte_sex);
		print_sub_library_command(&lsub, lc);
		break;

	    case LC_SUB_CLIENT:
		memset((char *)&csub, '\0',sizeof(struct sub_client_command));
		size = left < sizeof(struct sub_client_command) ?
		       left : sizeof(struct sub_client_command);
		memcpy((char *)&csub, (char *)lc, size);
		if(swapped)
		    swap_sub_client_command(&csub, host_byte_sex);
		print_sub_client_command(&csub, lc);
		break;

	    case LC_PREBOUND_DYLIB:
		memset((char *)&pbdylib, '\0',
			sizeof(struct prebound_dylib_command));
		size = left < sizeof(struct prebound_dylib_command) ?
		       left : sizeof(struct prebound_dylib_command);
		memcpy((char *)&pbdylib, (char *)lc, size);
		if(swapped)
		    swap_prebound_dylib_command(&pbdylib, host_byte_sex);
		print_prebound_dylib_command(&pbdylib, lc, very_verbose);
		break;

	    case LC_ID_DYLINKER:
	    case LC_LOAD_DYLINKER:
		memset((char *)&dyld, '\0', sizeof(struct dylinker_command));
		size = left < sizeof(struct dylinker_command) ?
		       left : sizeof(struct dylinker_command);
		memcpy((char *)&dyld, (char *)lc, size);
		if(swapped)
		    swap_dylinker_command(&dyld, host_byte_sex);
		print_dylinker_command(&dyld, lc);
		break;

	    case LC_FVMFILE:
		memset((char *)&ff, '\0', sizeof(struct fvmfile_command));
		size = left < sizeof(struct fvmfile_command) ?
		       left : sizeof(struct fvmfile_command);
		memcpy((char *)&ff, (char *)lc, size);
		if(swapped)
		    swap_fvmfile_command(&ff, host_byte_sex);
		print_fvmfile_command(&ff, lc);
		break;

	    case LC_UNIXTHREAD:
	    case LC_THREAD:
	        if(l.cmd == LC_UNIXTHREAD)
		    printf("        cmd LC_UNIXTHREAD\n");
		else
		    printf("        cmd LC_THREAD\n");
		printf("    cmdsize %lu\n", l.cmdsize);

		if(left <= sizeof(struct thread_command))
		    break;
		begin = (char *)lc + sizeof(struct thread_command);
		if(left >= l.cmdsize)
		    end = (char *)lc + l.cmdsize;
		else
		    end = (char *)lc + left;
		print_thread_states(begin, end, mh, load_commands_byte_sex);
		break;

	    case LC_IDENT:
		printf("          cmd LC_IDENT\n");
		printf("      cmdsize %lu", l.cmdsize);
		if(l.cmdsize < sizeof(struct ident_command))
		    printf(" Incorrect size\n");
		else
		    printf("\n");
		begin = (char *)lc + sizeof(struct ident_command);
		left -= sizeof(struct ident_command);
		if(left >= l.cmdsize)
		    end = (char *)lc + l.cmdsize;
		else
		    end = (char *)lc + left;

		p = ((char *)lc) + sizeof(struct ident_command);
		while(begin < end){
		    if(*begin == '\0'){
			begin++;
			continue;
		    }
		    for(j = 0; begin + j < end && begin[j] != '\0'; j++)
			;
		    printf(" ident string %.*s\n", (int)j, begin);
		    begin += j;
		}
		break;

	    case LC_ROUTINES:
		memset((char *)&rc, '\0', sizeof(struct routines_command));
		size = left < sizeof(struct routines_command) ?
		       left : sizeof(struct routines_command);
		memcpy((char *)&rc, (char *)lc, size);
		if(swapped)
		    swap_routines_command(&rc, host_byte_sex);
		print_routines_command(&rc);
		break;

	    case LC_TWOLEVEL_HINTS:
		memset((char *)&hints, '\0',
		       sizeof(struct twolevel_hints_command));
		size = left < sizeof(struct twolevel_hints_command) ?
		       left : sizeof(struct twolevel_hints_command);
		memcpy((char *)&hints, (char *)lc, size);
		if(swapped)
		    swap_twolevel_hints_command(&hints, host_byte_sex);
		print_twolevel_hints_command(&hints, object_size);
		break;

	    case LC_PREBIND_CKSUM:
		memset((char *)&cs, '\0', sizeof(struct prebind_cksum_command));
		size = left < sizeof(struct prebind_cksum_command) ?
		       left : sizeof(struct prebind_cksum_command);
		memcpy((char *)&cs, (char *)lc, size);
		if(swapped)
		    swap_prebind_cksum_command(&cs, host_byte_sex);
		print_prebind_cksum_command(&cs);
		break;

	    default:
		printf("      cmd ?(0x%08x) Unknown load command\n",
		       (unsigned int)l.cmd);
		printf("  cmdsize %lu\n", l.cmdsize);
		if(left < sizeof(struct load_command))
		    return;
		left -= sizeof(struct load_command);
		size = left < l.cmdsize - sizeof(struct load_command) ?
		       left : l.cmdsize - sizeof(struct load_command);
		unknown = allocate(size);
		memcpy((char *)unknown,
		       ((char *)lc) + sizeof(struct load_command), size);
		if(swapped)
		    for(j = 0; j < size / sizeof(unsigned long); j++)
			unknown[j] = SWAP_LONG(unknown[j]);
		for(j = 0; j < size / sizeof(unsigned long); j += k){
		    for(k = 0;
			k < 8 && j + k < size / sizeof(unsigned long);
			k++)
			printf("%08x ", (unsigned int)unknown[j + k]);
		    printf("\n");
		}
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		return;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");
}

void
print_libraries(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
enum bool just_id,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, left, size;
    struct load_command *lc, l;
    struct fvmlib_command fl;
    struct dylib_command dl;
    char *p;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of sizeof(long)\n",
		       i);
	    if((char *)lc + l.cmdsize > (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load commands\n",
		       i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_IDFVMLIB:
	    case LC_LOADFVMLIB:
		if(just_id == TRUE)
		    break;
		memset((char *)&fl, '\0', sizeof(struct fvmlib_command));
		size = left < sizeof(struct fvmlib_command) ?
		       left : sizeof(struct fvmlib_command);
		memcpy((char *)&fl, (char *)lc, size);
		if(swapped)
		    swap_fvmlib_command(&fl, host_byte_sex);
		if(fl.fvmlib.name.offset < fl.cmdsize){
		    p = (char *)lc + fl.fvmlib.name.offset;
		    printf("\t%s (minor version %lu)\n", p,
			   fl.fvmlib.minor_version);
		}
		else{
		    printf("\tBad offset (%lu) for name of %s command %lu\n",
			   fl.fvmlib.name.offset, l.cmd == LC_IDFVMLIB ?
			   "LC_IDFVMLIB" : "LC_LOADFVMLIB" , i);
		}
		break;

	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		if(just_id == TRUE)
		    break;
	    case LC_ID_DYLIB:
		memset((char *)&dl, '\0', sizeof(struct dylib_command));
		size = left < sizeof(struct dylib_command) ?
		       left : sizeof(struct dylib_command);
		memcpy((char *)&dl, (char *)lc, size);
		if(swapped)
		    swap_dylib_command(&dl, host_byte_sex);
		if(dl.dylib.name.offset < dl.cmdsize){
		    p = (char *)lc + dl.dylib.name.offset;
		    if(just_id == TRUE)
			printf("%s\n", p);
		    else
			printf("\t%s (compatibility version %lu.%lu.%lu, "
			   "current version %lu.%lu.%lu)\n", p,
			   dl.dylib.compatibility_version >> 16,
			   (dl.dylib.compatibility_version >> 8) & 0xff,
			   dl.dylib.compatibility_version & 0xff,
			   dl.dylib.current_version >> 16,
			   (dl.dylib.current_version >> 8) & 0xff,
			   dl.dylib.current_version & 0xff);
		    if(verbose){
			printf("\ttime stamp %lu ", dl.dylib.timestamp);
			printf("%s",ctime((const long *)&(dl.dylib.timestamp)));
		    }
		}
		else{
		    printf("\tBad offset (%lu) for name of %s command %lu\n",
			   dl.dylib.name.offset, l.cmd == LC_ID_DYLIB ?
			   "LC_ID_DYLIB" :
			   (l.cmd == LC_LOAD_DYLIB ? "LC_LOAD_DYLIB" :
			    "LC_LOAD_WEAK_DYLIB") , i);
		}
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		return;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");
}

/*
 * print an LC_SEGMENT command.  The segment_command structure specified must
 * be aligned correctly and in the host byte sex.
 */
void
print_segment_command(
struct segment_command *sg,
unsigned long object_size,
enum bool verbose)
{
	printf("      cmd LC_SEGMENT\n");
	printf("  cmdsize %lu", sg->cmdsize);
	if(sg->cmdsize != sizeof(struct segment_command) +
			  sg->nsects * sizeof(struct section))
	    printf(" Inconsistent size\n");
	else
	    printf("\n");
	printf("  segname %.16s\n", sg->segname);
	printf("   vmaddr 0x%08x\n", (unsigned int)sg->vmaddr);
	printf("   vmsize 0x%08x\n", (unsigned int)sg->vmsize);
	printf("  fileoff %lu", sg->fileoff);
	if(sg->fileoff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf(" filesize %lu", sg->filesize);
	if(sg->fileoff + sg->filesize > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	if(verbose){
	    if((sg->maxprot &
	      ~(VM_PROT_READ  | VM_PROT_WRITE  | VM_PROT_EXECUTE)) != 0)
		printf("  maxprot ?(0x%08x)\n", (unsigned int)sg->maxprot);
	    else{
		if(sg->maxprot & VM_PROT_READ)
		    printf("  maxprot r");
		else
		    printf("  maxprot -");
		if(sg->maxprot & VM_PROT_WRITE)
		    printf("w");
		else
		    printf("-");
		if(sg->maxprot & VM_PROT_EXECUTE)
		    printf("x\n");
		else
		    printf("-\n");
	    }
	    if((sg->initprot &
	      ~(VM_PROT_READ  | VM_PROT_WRITE  | VM_PROT_EXECUTE)) != 0)
		printf(" initprot ?(0x%08x)\n", (unsigned int)sg->initprot);
	    else{
		if(sg->initprot & VM_PROT_READ)
		    printf(" initprot r");
		else
		    printf(" initprot -");
		if(sg->initprot & VM_PROT_WRITE)
		    printf("w");
		else
		    printf("-");
		if(sg->initprot & VM_PROT_EXECUTE)
		    printf("x\n");
		else
		    printf("-\n");
	    }
	}
	else{
	    printf("  maxprot 0x%08x\n", (unsigned int)sg->maxprot);
	    printf(" initprot 0x%08x\n", (unsigned int)sg->initprot);
	}
	printf("   nsects %lu\n", sg->nsects);
	if(verbose){
	    printf("    flags");
	    if(sg->flags == 0)
		printf(" (none)\n");
	    else{
		if(sg->flags & SG_HIGHVM){
		    printf(" HIGHVM");
		    sg->flags &= ~SG_HIGHVM;
		}
		if(sg->flags & SG_FVMLIB){
		    printf(" FVMLIB");
		    sg->flags &= ~SG_FVMLIB;
		}
		if(sg->flags & SG_NORELOC){
		    printf(" NORELOC");
		    sg->flags &= ~SG_NORELOC;
		}
		if(sg->flags)
		    printf(" 0x%x (unknown flags)\n", (unsigned int)sg->flags);
		else
		    printf("\n");
	    }
	}
	else{
	    printf("    flags 0x%x\n", (unsigned int)sg->flags);
	}
}

/*
 * print a section structure.  All specified structures must
 * be aligned correctly and in the host byte sex.
 */
void
print_section(
struct section *s,
struct segment_command *sg,
struct mach_header *mh,
unsigned long object_size,
enum bool verbose)
{
    unsigned long section_type, section_attributes;

	printf("Section\n");
	printf("  sectname %.16s\n", s->sectname);
	printf("   segname %.16s", s->segname);
	if(mh->filetype != MH_OBJECT &&
	   strcmp(sg->segname, s->segname) != 0)
	    printf(" (does not match segment)\n");
	else
	    printf("\n");
	printf("      addr 0x%08x\n", (unsigned int)s->addr);
	printf("      size 0x%08x", (unsigned int)s->size);
	if((s->flags & S_ZEROFILL) != 0 && s->offset + s->size > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("    offset %lu", s->offset);
	if(s->offset > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("     align 2^%lu (%d)\n", s->align, 1 << s->align);
	printf("    reloff %lu", s->reloff);
	if(s->reloff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("    nreloc %lu", s->nreloc);
	if(s->reloff + s->nreloc * sizeof(struct relocation_info) > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	section_type = s->flags & SECTION_TYPE;
	if(verbose){
	    printf("      type");
	    if(section_type == S_REGULAR)
		printf(" S_REGULAR\n");
	    else if(section_type == S_ZEROFILL)
		printf(" S_ZEROFILL\n");
	    else if(section_type == S_CSTRING_LITERALS)
		printf(" S_CSTRING_LITERALS\n");
	    else if(section_type == S_4BYTE_LITERALS)
		printf(" S_4BYTE_LITERALS\n");
	    else if(section_type == S_8BYTE_LITERALS)
		printf(" S_8BYTE_LITERALS\n");
	    else if(section_type == S_LITERAL_POINTERS)
		printf(" S_LITERAL_POINTERS\n");
	    else if(section_type == S_NON_LAZY_SYMBOL_POINTERS)
		printf(" S_NON_LAZY_SYMBOL_POINTERS\n");
	    else if(section_type == S_LAZY_SYMBOL_POINTERS)
		printf(" S_LAZY_SYMBOL_POINTERS\n");
	    else if(section_type == S_SYMBOL_STUBS)
		printf(" S_SYMBOL_STUBS\n");
	    else if(section_type == S_MOD_INIT_FUNC_POINTERS)
		printf(" S_MOD_INIT_FUNC_POINTERS\n");
	    else if(section_type == S_MOD_TERM_FUNC_POINTERS)
		printf(" S_MOD_TERM_FUNC_POINTERS\n");
	    else if(section_type == S_COALESCED)
		printf(" S_COALESCED\n");
	    else
		printf(" 0x%08x\n", (unsigned int)section_type);

	    printf("attributes");
	    section_attributes = s->flags & SECTION_ATTRIBUTES;
	    if(section_attributes & S_ATTR_PURE_INSTRUCTIONS)
		printf(" PURE_INSTRUCTIONS");
	    if(section_attributes & S_ATTR_NO_TOC)
		printf(" NO_TOC");
	    if(section_attributes & S_ATTR_STRIP_STATIC_SYMS)
		printf(" STRIP_STATIC_SYMS");
	    if(section_attributes & S_ATTR_SOME_INSTRUCTIONS)
		printf(" SOME_INSTRUCTIONS");
	    if(section_attributes & S_ATTR_EXT_RELOC)
		printf(" EXT_RELOC");
	    if(section_attributes & S_ATTR_LOC_RELOC)
		printf(" LOC_RELOC");
	    if(section_attributes == 0)
		printf(" (none)");
	    printf("\n");
	}
	else
	    printf("     flags 0x%08x\n", (unsigned int)s->flags);
	printf(" reserved1 %lu", s->reserved1);
	if(section_type == S_SYMBOL_STUBS ||
	   section_type == S_LAZY_SYMBOL_POINTERS ||
	   section_type == S_NON_LAZY_SYMBOL_POINTERS)
	    printf(" (index into indirect symbol table)\n");
	else
	    printf("\n");
	printf(" reserved2 %lu", s->reserved2);
	if(section_type == S_SYMBOL_STUBS)
	    printf(" (size of stubs)\n");
	else
	    printf("\n");
}

/*
 * print an LC_SYMTAB command.  The symtab_command structure specified must
 * be aligned correctly and in the host byte sex.
 */
void
print_symtab_command(
struct symtab_command *st,
unsigned long object_size)
{
	printf("     cmd LC_SYMTAB\n");
	printf(" cmdsize %lu", st->cmdsize);
	if(st->cmdsize != sizeof(struct symtab_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	printf("  symoff %lu", st->symoff);
	if(st->symoff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("   nsyms %lu", st->nsyms);
	if(st->symoff + st->nsyms * sizeof(struct nlist) > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("  stroff %lu", st->stroff);
	if(st->stroff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf(" strsize %lu", st->strsize);
	if(st->stroff + st->strsize > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
}

/*
 * print an LC_DYSYMTAB command.  The dysymtab_command structure specified must
 * be aligned correctly and in the host byte sex.
 */
void
print_dysymtab_command(
struct dysymtab_command *dyst,
unsigned long nsyms,
unsigned long object_size)
{
	printf("            cmd LC_DYSYMTAB\n");
	printf("        cmdsize %lu", dyst->cmdsize);
	if(dyst->cmdsize != sizeof(struct dysymtab_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");

	printf("      ilocalsym %lu", dyst->ilocalsym);
	if(dyst->ilocalsym > nsyms)
	    printf(" (greater than the number of symbols)\n");
	else
	    printf("\n");
	printf("      nlocalsym %lu", dyst->nlocalsym);
	if(dyst->ilocalsym + dyst->nlocalsym > nsyms)
	    printf(" (past the end of the symbol table)\n");
	else
	    printf("\n");
	printf("     iextdefsym %lu", dyst->iextdefsym);
	if(dyst->iextdefsym > nsyms)
	    printf(" (greater than the number of symbols)\n");
	else
	    printf("\n");
	printf("     nextdefsym %lu", dyst->nextdefsym);
	if(dyst->iextdefsym + dyst->nextdefsym > nsyms)
	    printf(" (past the end of the symbol table)\n");
	else
	    printf("\n");
	printf("      iundefsym %lu", dyst->iundefsym);
	if(dyst->iundefsym > nsyms)
	    printf(" (greater than the number of symbols)\n");
	else
	    printf("\n");
	printf("      nundefsym %lu", dyst->nundefsym);
	if(dyst->iundefsym + dyst->nundefsym > nsyms)
	    printf(" (past the end of the symbol table)\n");
	else
	    printf("\n");
	printf("         tocoff %lu", dyst->tocoff);
	if(dyst->tocoff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("           ntoc %lu", dyst->ntoc);
	if(dyst->tocoff + dyst->ntoc * sizeof(struct dylib_table_of_contents) >
	   object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("      modtaboff %lu", dyst->modtaboff);
	if(dyst->modtaboff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("        nmodtab %lu", dyst->nmodtab);
	if(dyst->modtaboff + dyst->nmodtab * sizeof(struct dylib_module) >
	   object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("   extrefsymoff %lu", dyst->extrefsymoff);
	if(dyst->extrefsymoff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("    nextrefsyms %lu", dyst->nextrefsyms);
	if(dyst->extrefsymoff +
	   dyst->nextrefsyms * sizeof(struct dylib_reference) > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf(" indirectsymoff %lu", dyst->indirectsymoff);
	if(dyst->indirectsymoff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("  nindirectsyms %lu", dyst->nindirectsyms);
	if(dyst->indirectsymoff + dyst->nindirectsyms * sizeof(unsigned long) >
	   object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("      extreloff %lu", dyst->extreloff);
	if(dyst->extreloff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("        nextrel %lu", dyst->nextrel);
	if(dyst->extreloff + dyst->nextrel * sizeof(struct relocation_info) >
	   object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("      locreloff %lu", dyst->locreloff);
	if(dyst->locreloff > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("        nlocrel %lu", dyst->nlocrel);
	if(dyst->locreloff + dyst->nlocrel * sizeof(struct relocation_info) >
	   object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
}

/*
 * print an LC_SYMSEG command.  The symseg_command structure specified must
 * be aligned correctly and in the host byte sex.
 */
void
print_symseg_command(
struct symseg_command *ss,
unsigned long object_size)
{
	printf("     cmd LC_SYMSEG (obsolete)\n");
	printf(" cmdsize %lu", ss->cmdsize);
	if(ss->cmdsize != sizeof(struct symseg_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	printf("  offset %lu", ss->offset);
	if(ss->offset > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("    size %lu", ss->size);
	if(ss->offset + ss->size > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
}

/*
 * print an LC_IDFVMLIB or LC_LOADFVMLIB command.  The fvmlib_command structure
 * specified must be aligned correctly and in the host byte sex.
 */
void
print_fvmlib_command(
struct fvmlib_command *fl,
struct load_command *lc)
{
    char *p;

	if(fl->cmd == LC_IDFVMLIB)
	    printf("           cmd LC_IDFVMLIB\n");
	else
	    printf("           cmd LC_LOADFVMLIB\n");
	printf("       cmdsize %lu", fl->cmdsize);
	if(fl->cmdsize < sizeof(struct fvmlib_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(fl->fvmlib.name.offset < fl->cmdsize){
	    p = (char *)lc + fl->fvmlib.name.offset;
	    printf("          name %s (offset %lu)\n",
		   p, fl->fvmlib.name.offset);
	}
	else{
	    printf("          name ?(bad offset %lu)\n",
		   fl->fvmlib.name.offset);
	}
	printf(" minor version %lu\n", fl->fvmlib.minor_version);
	printf("   header addr 0x%08x\n", (unsigned int)fl->fvmlib.header_addr);
}

/*
 * print an LC_ID_DYLIB, LC_LOAD_DYLIB or LC_LOAD_WEAK_DYLIB command.  The
 * dylib_command structure specified must be aligned correctly and in the host
 * byte sex.
 */
void
print_dylib_command(
struct dylib_command *dl,
struct load_command *lc)
{
    char *p;

	if(dl->cmd == LC_ID_DYLIB)
	    printf("          cmd LC_ID_DYLIB\n");
	else if(dl->cmd == LC_LOAD_DYLIB)
	    printf("          cmd LC_LOAD_DYLIB\n");
	else
	    printf("          cmd LC_LOAD_WEAK_DYLIB\n");
	printf("      cmdsize %lu", dl->cmdsize);
	if(dl->cmdsize < sizeof(struct dylib_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(dl->dylib.name.offset < dl->cmdsize){
	    p = (char *)lc + dl->dylib.name.offset;
	    printf("         name %s (offset %lu)\n",
		   p, dl->dylib.name.offset);
	}
	else{
	    printf("         name ?(bad offset %lu)\n",
		   dl->dylib.name.offset);
	}
	printf("   time stamp %lu ", dl->dylib.timestamp);
	printf("%s", ctime((const long *)&(dl->dylib.timestamp)));
	printf("      current version %lu.%lu.%lu\n",
	       dl->dylib.current_version >> 16,
	       (dl->dylib.current_version >> 8) & 0xff,
	       dl->dylib.current_version & 0xff);
	printf("compatibility version %lu.%lu.%lu\n",
	       dl->dylib.compatibility_version >> 16,
	       (dl->dylib.compatibility_version >> 8) & 0xff,
	       dl->dylib.compatibility_version & 0xff);
}

/*
 * print an LC_SUB_FRAMEWORK command.  The sub_framework_command
 * structure specified must be aligned correctly and in the host byte sex.
 */
void
print_sub_framework_command(
struct sub_framework_command *sub,
struct load_command *lc)
{
    char *p;

	printf("          cmd LC_SUB_FRAMEWORK\n");
	printf("      cmdsize %lu", sub->cmdsize);
	if(sub->cmdsize < sizeof(struct sub_framework_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(sub->umbrella.offset < sub->cmdsize){
	    p = (char *)lc + sub->umbrella.offset;
	    printf("         umbrella %s (offset %lu)\n",
		   p, sub->umbrella.offset);
	}
	else{
	    printf("         umbrella ?(bad offset %lu)\n",
		   sub->umbrella.offset);
	}
}

/*
 * print an LC_SUB_UMBRELLA command.  The sub_umbrella_command
 * structure specified must be aligned correctly and in the host byte sex.
 */
void
print_sub_umbrella_command(
struct sub_umbrella_command *usub,
struct load_command *lc)
{
    char *p;

	printf("          cmd LC_SUB_UMBRELLA\n");
	printf("      cmdsize %lu", usub->cmdsize);
	if(usub->cmdsize < sizeof(struct sub_umbrella_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(usub->sub_umbrella.offset < usub->cmdsize){
	    p = (char *)lc + usub->sub_umbrella.offset;
	    printf("         sub_umbrella %s (offset %lu)\n",
		   p, usub->sub_umbrella.offset);
	}
	else{
	    printf("         sub_umbrella ?(bad offset %lu)\n",
		   usub->sub_umbrella.offset);
	}
}

/*
 * print an LC_SUB_LIBRARY command.  The sub_library_command
 * structure specified must be aligned correctly and in the host byte sex.
 */
void
print_sub_library_command(
struct sub_library_command *lsub,
struct load_command *lc)
{
    char *p;

	printf("          cmd LC_SUB_LIBRARY\n");
	printf("      cmdsize %lu", lsub->cmdsize);
	if(lsub->cmdsize < sizeof(struct sub_library_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(lsub->sub_library.offset < lsub->cmdsize){
	    p = (char *)lc + lsub->sub_library.offset;
	    printf("         sub_library %s (offset %lu)\n",
		   p, lsub->sub_library.offset);
	}
	else{
	    printf("         sub_library ?(bad offset %lu)\n",
		   lsub->sub_library.offset);
	}
}

/*
 * print an LC_SUB_CLIENT command.  The sub_client_command
 * structure specified must be aligned correctly and in the host byte sex.
 */
void
print_sub_client_command(
struct sub_client_command *csub,
struct load_command *lc)
{
    char *p;

	printf("          cmd LC_SUB_CLIENT\n");
	printf("      cmdsize %lu", csub->cmdsize);
	if(csub->cmdsize < sizeof(struct sub_client_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(csub->client.offset < csub->cmdsize){
	    p = (char *)lc + csub->client.offset;
	    printf("         client %s (offset %lu)\n",
		   p, csub->client.offset);
	}
	else{
	    printf("         client ?(bad offset %lu)\n",
		   csub->client.offset);
	}
}

/*
 * print an LC_PREBOUND_DYLIB command.  The prebound_dylib_command structure
 * specified must be aligned correctly and in the host byte sex.
 */
void
print_prebound_dylib_command(
struct prebound_dylib_command *pbdylib,
struct load_command *lc,
enum bool verbose)
{
    char *p;
    unsigned long i;

	printf("            cmd LC_PREBOUND_DYLIB\n");
	printf("        cmdsize %lu", pbdylib->cmdsize);
	if(pbdylib->cmdsize < sizeof(struct prebound_dylib_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(pbdylib->name.offset < pbdylib->cmdsize){
	    p = (char *)lc + pbdylib->name.offset;
	    printf("           name %s (offset %lu)\n",
		   p, pbdylib->name.offset);
	}
	else{
	    printf("           name ?(bad offset %lu)\n",
		   pbdylib->name.offset);
	}
	printf("       nmodules %lu\n", pbdylib->nmodules);

	if(pbdylib->linked_modules.offset < pbdylib->cmdsize){
	    p = (char *)lc + pbdylib->linked_modules.offset;
	    if(verbose == TRUE){
		printf(" linked_modules (offset %lu)\n",
			pbdylib->linked_modules.offset);
		for(i = 0; i < pbdylib->nmodules; i++){
		    if(((p[i/8] >> (i%8)) & 1) == 1)
			printf("%lu\n", i);
		}
	    }
	    else{
		printf(" linked_modules ");
		for(i = 0; i < pbdylib->nmodules && i < 8; i++){
		    if(((*p >> i) & 1) == 0)
			printf("0");
		    else
			printf("1");
		}
		if(i <= pbdylib->nmodules)
		    printf("...");
		printf(" (offset %lu)\n", pbdylib->linked_modules.offset);
	    }
	}
	else{
	    printf(" linked_modules ?(bad offset %lu)\n",
		   pbdylib->linked_modules.offset);
	}
}

/*
 * print an LC_ID_DYLINKER or LC_LOAD_DYLINKER command.  The dylinker_command
 * structure specified must be aligned correctly and in the host byte sex.
 */
void
print_dylinker_command(
struct dylinker_command *dyld,
struct load_command *lc)
{
    char *p;

	if(dyld->cmd == LC_ID_DYLINKER)
	    printf("          cmd LC_ID_DYLINKER\n");
	else
	    printf("          cmd LC_LOAD_DYLINKER\n");
	printf("      cmdsize %lu", dyld->cmdsize);
	if(dyld->cmdsize < sizeof(struct dylinker_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(dyld->name.offset < dyld->cmdsize){
	    p = (char *)lc + dyld->name.offset;
	    printf("         name %s (offset %lu)\n", p, dyld->name.offset);
	}
	else{
	    printf("         name ?(bad offset %lu)\n", dyld->name.offset);
	}
}

/*
 * print an LC_FVMFILE command.  The fvmfile_command structure specified must
 * be aligned correctly and in the host byte sex.
 */
void
print_fvmfile_command(
struct fvmfile_command *ff,
struct load_command *lc)
{
    char *p;

	printf("           cmd LC_FVMFILE\n");
	printf("       cmdsize %lu", ff->cmdsize);
	if(ff->cmdsize < sizeof(struct fvmfile_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	if(ff->name.offset < ff->cmdsize){
	    p = (char *)lc + ff->name.offset;
	    printf("          name %s (offset %lu)\n", p, ff->name.offset);
	}
	else{
	    printf("          name ?(bad offset %lu)\n", ff->name.offset);
	}
	printf("   header addr 0x%08x\n", (unsigned int)ff->header_addr);
}

/*
 * print an LC_ROUTINES command.  The routines_command structure specified must
 * be aligned correctly and in the host byte sex.
 */
void
print_routines_command(
struct routines_command *rc)
{
	printf("          cmd LC_ROUTINES\n");
	printf("      cmdsize %lu", rc->cmdsize);
	if(rc->cmdsize != sizeof(struct routines_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	printf(" init_address 0x%08x\n", (unsigned int)rc->init_address);
	printf("  init_module %lu\n", rc->init_module);
	printf("    reserved1 %lu\n", rc->reserved1);
	printf("    reserved2 %lu\n", rc->reserved2);
	printf("    reserved3 %lu\n", rc->reserved3);
	printf("    reserved4 %lu\n", rc->reserved4);
	printf("    reserved5 %lu\n", rc->reserved5);
	printf("    reserved6 %lu\n", rc->reserved6);
}

/*
 * print an LC_TWOLEVEL_HINTS command.  The twolevel_hints_command structure
 * specified must be aligned correctly and in the host byte sex.
 */
void
print_twolevel_hints_command(
struct twolevel_hints_command *hints,
unsigned long object_size)
{
	printf("     cmd LC_TWOLEVEL_HINTS\n");
	printf(" cmdsize %lu", hints->cmdsize);
	if(hints->cmdsize != sizeof(struct twolevel_hints_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	printf("  offset %lu", hints->offset);
	if(hints->offset > object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
	printf("  nhints %lu", hints->nhints);
	if(hints->offset + hints->nhints * sizeof(struct twolevel_hint) >
	   object_size)
	    printf(" (past end of file)\n");
	else
	    printf("\n");
}

/*
 * print an LC_PREBIND_CKSUM command.  The prebind_cksum_command structure
 * specified must be aligned correctly and in the host byte sex.
 */
void
print_prebind_cksum_command(
struct prebind_cksum_command *cksum)
{
	printf("     cmd LC_PREBIND_CKSUM\n");
	printf(" cmdsize %lu", cksum->cmdsize);
	if(cksum->cmdsize != sizeof(struct prebind_cksum_command))
	    printf(" Incorrect size\n");
	else
	    printf("\n");
	printf("   cksum 0x%08x\n", (unsigned int)cksum->cksum);
}

/*
 * print the thread states from an LC_THREAD or LC_UNIXTHREAD command.  The
 * thread state triples (flavor, count, state) are in memory between begin and
 * and end values specified, and in the specified byte sex.  The mach_header
 * structure specified must be aligned correctly and in the host byte sex.
 */
void
print_thread_states(
char *begin, 
char *end,
struct mach_header *mh,
enum byte_sex thread_states_byte_sex)
{
    unsigned long i, j, k, flavor, count, left;
    enum byte_sex host_byte_sex;
    enum bool swapped;

	i = 0;
	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != thread_states_byte_sex;

	if(mh->cputype == CPU_TYPE_MC680x0){
	    struct m68k_thread_state_regs cpu;
	    struct m68k_thread_state_68882 fpu;
	    struct m68k_thread_state_user_reg user_reg;

	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case M68K_THREAD_STATE_REGS:
		    printf("     flavor M68K_THREAD_STATE_REGS\n");
		    if(count == M68K_THREAD_STATE_REGS_COUNT)
			printf("      count M68K_THREAD_STATE_"
			       "REGS_COUNT\n");
		    else
			printf("      count %lu (not M68K_THREAD_STATE_"
			       "REGS_COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(struct m68k_thread_state_regs)){
		        memcpy((char *)&cpu, begin,
			       sizeof(struct m68k_thread_state_regs));
		        begin += sizeof(struct m68k_thread_state_regs);
		    }
		    else{
		        memset((char *)&cpu, '\0',
			       sizeof(struct m68k_thread_state_regs));
		        memcpy((char *)&cpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m68k_thread_state_regs(&cpu, host_byte_sex);
		    printf(" dregs ");
		    for(j = 0 ; j < 8 ; j++)
			printf(" %08x", (unsigned int)cpu.dreg[j]);
		    printf("\n");
		    printf(" aregs ");
		    for(j = 0 ; j < 8 ; j++)
			printf(" %08x", (unsigned int)cpu.areg[j]);
		    printf("\n");
		    printf(" pad 0x%04x sr 0x%04x pc 0x%08x\n", 
			    (unsigned int)(cpu.pad0 & 0x0000ffff),
			    (unsigned int)(cpu.sr & 0x0000ffff),
			    (unsigned int)cpu.pc);
		    break;

		case M68K_THREAD_STATE_68882:
		    printf("     flavor M68K_THREAD_STATE_68882\n");
		    if(count == M68K_THREAD_STATE_68882_COUNT)
			printf("      count M68K_THREAD_STATE_"
			       "68882_COUNT\n");
		    else
			printf("      count %lu (not M68K_THREAD_STATE_"
			       "68882_COUNT\n", count);
		    left = end - begin;
		    if(left >= sizeof(struct m68k_thread_state_68882)){
		        memcpy((char *)&fpu, begin,
			       sizeof(struct m68k_thread_state_68882));
		        begin += sizeof(struct m68k_thread_state_68882);
		    }
		    else{
		        memset((char *)&fpu, '\0',
			       sizeof(struct m68k_thread_state_68882));
		        memcpy((char *)&fpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m68k_thread_state_68882(&fpu, host_byte_sex);
		    for(j = 0 ; j < 8 ; j++)
			printf(" fp reg %lu %08x %08x %08x\n", j,
			       (unsigned int)fpu.regs[j].fp[0],
			       (unsigned int)fpu.regs[j].fp[1],
			       (unsigned int)fpu.regs[j].fp[2]);
		    printf(" cr 0x%08x sr 0x%08x state 0x%08x\n", 
			   (unsigned int)fpu.cr, (unsigned int)fpu.sr,
			   (unsigned int)fpu.state);
		    break;

		case M68K_THREAD_STATE_USER_REG:
		    printf("     flavor M68K_THREAD_STATE_USER_REG\n");
		    if(count == M68K_THREAD_STATE_USER_REG_COUNT)
			printf("      count M68K_THREAD_STATE_"
			       "USER_REG_COUNT\n");
		    else
			printf("      count %lu (not M68K_THREAD_STATE_"
			       "USER_REG_COUNT", count);
		    left = end - begin;
		    if(left >= sizeof(struct m68k_thread_state_user_reg)){
		        memcpy((char *)&user_reg, begin,
			       sizeof(struct m68k_thread_state_user_reg));
		        begin += sizeof(struct m68k_thread_state_user_reg);
		    }
		    else{
		        memset((char *)&user_reg, '\0',
			       sizeof(struct m68k_thread_state_user_reg));
		        memcpy((char *)&user_reg, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m68k_thread_state_user_reg(&user_reg,
							host_byte_sex);
		    printf(" user_reg 0x%08x\n",
			   (unsigned int)user_reg.user_reg);
		    break;

		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	if(mh->cputype == CPU_TYPE_HPPA){
	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case HPPA_INTEGER_THREAD_STATE:
			{ struct hp_pa_integer_thread_state frame;
			
		    printf("      flavor HPPA_INTEGER_THREAD_STATE\n");
		    if(count == HPPA_INTEGER_THREAD_STATE_COUNT)
			printf("      count HPPA_INTEGER_THREAD_STATE_COUNT\n");
		    else
			printf("      count %lu (not HPPA_INTEGER_THREAD_STATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(struct hp_pa_integer_thread_state)){
		        memcpy((char *)&frame, begin,
			       sizeof(struct hp_pa_integer_thread_state));
		        begin += sizeof(struct hp_pa_integer_thread_state);
		    }
		    else{
		        memset((char *)&frame, '\0',
			       sizeof(struct hp_pa_integer_thread_state));
		        memcpy((char *)&frame, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_hppa_integer_thread_state(&frame, host_byte_sex);
			printf(
		         "r1  0x%08lx  r2  0x%08lx  r3  0x%08lx  r4  0x%08lx\n"
		         "r5  0x%08lx  r6  0x%08lx  r7  0x%08lx  r8  0x%08lx\n"
		         "r9  0x%08lx  r10 0x%08lx  r11 0x%08lx  r12 0x%08lx\n"
		         "r13 0x%08lx  r14 0x%08lx  r15 0x%08lx  r16 0x%08lx\n"
		         "r17 0x%08lx  r18 0x%08lx  r19 0x%08lx  r20 0x%08lx\n"
		         "r21 0x%08lx  r22 0x%08lx  r23 0x%08lx  r24 0x%08lx\n"
		         "r25 0x%08lx  r26 0x%08lx  r27 0x%08lx  r28 0x%08lx\n"
		         "r29 0x%08lx  r30 0x%08lx  r31 0x%08lx\n"
			 "sr0 0x%08lx  sr1 0x%08lx  sr2 0x%08lx  sar 0x%08lx\n",
		   frame.ts_gr1,  frame.ts_gr2,  frame.ts_gr3,  frame.ts_gr4,
		   frame.ts_gr5,  frame.ts_gr6,  frame.ts_gr7,  frame.ts_gr8,
		   frame.ts_gr9,  frame.ts_gr10, frame.ts_gr11, frame.ts_gr12,
		   frame.ts_gr13, frame.ts_gr14, frame.ts_gr15, frame.ts_gr16,
		   frame.ts_gr17, frame.ts_gr18, frame.ts_gr19, frame.ts_gr20,
		   frame.ts_gr21, frame.ts_gr22, frame.ts_gr23, frame.ts_gr24,
		   frame.ts_gr25, frame.ts_gr26, frame.ts_gr27, frame.ts_gr28,
		   frame.ts_gr29, frame.ts_gr30, frame.ts_gr31,
		   frame.ts_sr0,  frame.ts_sr1,  frame.ts_sr2,  frame.ts_sar);
			}
		    break;
		case HPPA_FRAME_THREAD_STATE: {
			struct hp_pa_frame_thread_state frame;
		    printf("      flavor HPPA_FRAME_THREAD_STATE\n");
		    if(count == HPPA_FRAME_THREAD_STATE_COUNT)
			printf("      count HPPA_FRAME_THREAD_STATE_COUNT\n");
		    else
			printf("      count %lu (not HPPA_FRAME_THREAD_STATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(struct hp_pa_frame_thread_state)){
		        memcpy((char *)&frame, begin,
			       sizeof(struct hp_pa_frame_thread_state));
		        begin += sizeof(struct hp_pa_frame_thread_state);
		    }
		    else{
		        memset((char *)&frame, '\0',
			       sizeof(struct hp_pa_frame_thread_state));
		        memcpy((char *)&frame, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_hppa_frame_thread_state(&frame, host_byte_sex);
		    printf("pcsq_front  0x%08lx pcsq_back  0x%08lx\n"
		           "pcoq_front  0x%08lx pcoq_back  0x%08lx\n"
			   "       psw  0x%08lx\n",
			   frame.ts_pcsq_front, frame.ts_pcsq_back,
			   frame.ts_pcoq_front, frame.ts_pcoq_back,
			   frame.ts_psw);
		    break;
		}
		case HPPA_FP_THREAD_STATE: {
			struct hp_pa_fp_thread_state frame;
		    printf("      flavor HPPA_FP_THREAD_STATE\n");
		    if(count == HPPA_FP_THREAD_STATE_COUNT)
			printf("      count HPPA_FP_THREAD_STATE_COUNT\n");
		    else
			printf("      count %lu (not HPPA_FP_THREAD_STATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(struct hp_pa_fp_thread_state)){
		        memcpy((char *)&frame, begin,
			       sizeof(struct hp_pa_fp_thread_state));
		        begin += sizeof(struct hp_pa_fp_thread_state);
		    }
		    else{
		        memset((char *)&frame, '\0',
			       sizeof(struct hp_pa_fp_thread_state));
		        memcpy((char *)&frame, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_hppa_fp_thread_state(&frame, host_byte_sex);
		    printf("fp0  %f    fp1  %f\nfp2  %f    fp3  %f\n"
			   "fp4  %f    fp5  %f\nfp6  %f    fp7  %f\n"
			   "fp8  %f    fp9  %f\nfp10 %f    fp11 %f\n"
			   "fp12 %f    fp13 %f\nfp14 %f    fp15 %f\n"
			   "fp16 %f    fp17 %f\nfp18 %f    fp19 %f\n"
			   "fp20 %f    fp21 %f\nfp22 %f    fp23 %f\n"
			   "fp24 %f    fp25 %f\nfp26 %f    fp27 %f\n"
			   "fp28 %f    fp29 %f\nfp30 %f    fp31 %f\n",
		    frame.ts_fp0,  frame.ts_fp1,  frame.ts_fp2,  frame.ts_fp3,
		    frame.ts_fp4,  frame.ts_fp5,  frame.ts_fp6,  frame.ts_fp7,
		    frame.ts_fp8,  frame.ts_fp9,  frame.ts_fp10, frame.ts_fp11,
		    frame.ts_fp12, frame.ts_fp13, frame.ts_fp14, frame.ts_fp15,
		    frame.ts_fp16, frame.ts_fp17, frame.ts_fp18, frame.ts_fp19,
		    frame.ts_fp20, frame.ts_fp21, frame.ts_fp22, frame.ts_fp23,
		    frame.ts_fp24, frame.ts_fp25, frame.ts_fp26, frame.ts_fp27,
		    frame.ts_fp28, frame.ts_fp29, frame.ts_fp30, frame.ts_fp31);
		    break;
		}
		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	if(mh->cputype == CPU_TYPE_SPARC){
	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case SPARC_THREAD_STATE_REGS:
		  { struct sparc_thread_state_regs cpu;
		    printf("     flavor SPARC_THREAD_STATE_REGS\n");
		    if (count == SPARC_THREAD_STATE_REGS_COUNT)
		      printf("      count SPARC_THREAD_STATE_REGS_COUNT\n");
		    else
		      printf("      count %lu (not SPARC_THREAD_STATE_REGS_COUNT)\n",
			     count);
		    left = begin - end;
		    if (left >= sizeof(struct sparc_thread_state_regs)) {
		      memcpy((char *) &cpu, begin,
			     sizeof(struct sparc_thread_state_regs));
		      begin += sizeof(struct sparc_thread_state_regs);
		    } else {
		      memset((char *) &cpu, '\0',
			     sizeof(struct sparc_thread_state_regs));
		      begin += left;
		    }
		    if (swapped)
		      swap_sparc_thread_state_regs(&cpu, host_byte_sex);
		    printf(
			   "psr 0x%08x  pc  0x%08x  npc 0x%08x  y   0x%08x\n"
			   "g0  0x%08x  g1  0x%08x  g2  0x%08x  g3  0x%08x\n"
			   "g4  0x%08x  g5  0x%08x  g6  0x%08x  g7  0x%08x\n"
			   "o0  0x%08x  o1  0x%08x  o2  0x%08x  o3  0x%08x\n"
			   "o4  0x%08x  o5  0x%08x  o6  0x%08x  o7  0x%08x\n",
			   cpu.regs.r_psr, cpu.regs.r_pc, cpu.regs.r_npc, 
			   cpu.regs.r_y, 0, cpu.regs.r_g1, 
			   cpu.regs.r_g2, cpu.regs.r_g3,
			   cpu.regs.r_g4, cpu.regs.r_g5, 
			   cpu.regs.r_g6, cpu.regs.r_g7,
			   cpu.regs.r_o0, cpu.regs.r_o1, 
			   cpu.regs.r_o2, cpu.regs.r_o3,
			   cpu.regs.r_o4, cpu.regs.r_o5, 
			   cpu.regs.r_o6, cpu.regs.r_o7);
		    break;
		  }
		case SPARC_THREAD_STATE_FPU:
		  { struct sparc_thread_state_fpu fpu;

		    printf("     flavor SPARC_THREAD_STATE_FPU\n");
		    if (count == SPARC_THREAD_STATE_FPU_COUNT)
		      printf("      count SPARC_THREAD_STATE_FPU_COUNT\n");
		    else
		      printf("      count %lu (not SPARC_THREAD_STATE_FPU_COUNT)\n",
			     count);
		    left = begin - end;
		    if (left >= sizeof(struct sparc_thread_state_fpu)) {
		      memcpy((char *) &fpu, begin,
			     sizeof(struct sparc_thread_state_fpu));
		      begin += sizeof(struct sparc_thread_state_fpu);
		    } else {
		      memset((char *) &fpu, '\0',
			     sizeof(struct sparc_thread_state_fpu));
		      begin += left;
		    }
		    if (swapped)
		      swap_sparc_thread_state_fpu(&fpu, host_byte_sex);
		    printf(
			   "f0  0x%08x  f1  0x%08x  f2  0x%08x  f3  0x%08x\n"
			   "f4  0x%08x  f5  0x%08x  f6  0x%08x  f7  0x%08x\n"
			   "f8  0x%08x  f9  0x%08x  f10 0x%08x  f11 0x%08x\n"
			   "f12 0x%08x  f13 0x%08x  f14 0x%08x  f15 0x%08x\n"
			   "f16 0x%08x  f17 0x%08x  f18 0x%08x  f19 0x%08x\n"
			   "f20 0x%08x  f21 0x%08x  f22 0x%08x  f23 0x%08x\n"
			   "f24 0x%08x  f25 0x%08x  f26 0x%08x  f27 0x%08x\n"
			   "f28 0x%08x  f29 0x%08x  f30 0x%08x  f31 0x%08x\n"
			   "fsr 0x%08x\n",
			   fpu.fpu.fpu_fr.Fpu_regs[0], fpu.fpu.fpu_fr.Fpu_regs[1],
			   fpu.fpu.fpu_fr.Fpu_regs[2], fpu.fpu.fpu_fr.Fpu_regs[3],
			   fpu.fpu.fpu_fr.Fpu_regs[4], fpu.fpu.fpu_fr.Fpu_regs[5],
			   fpu.fpu.fpu_fr.Fpu_regs[6], fpu.fpu.fpu_fr.Fpu_regs[7],
			   fpu.fpu.fpu_fr.Fpu_regs[8], fpu.fpu.fpu_fr.Fpu_regs[9],
			   fpu.fpu.fpu_fr.Fpu_regs[10], fpu.fpu.fpu_fr.Fpu_regs[11],
			   fpu.fpu.fpu_fr.Fpu_regs[12], fpu.fpu.fpu_fr.Fpu_regs[13],
			   fpu.fpu.fpu_fr.Fpu_regs[14], fpu.fpu.fpu_fr.Fpu_regs[15],
			   fpu.fpu.fpu_fr.Fpu_regs[16], fpu.fpu.fpu_fr.Fpu_regs[17],
			   fpu.fpu.fpu_fr.Fpu_regs[18], fpu.fpu.fpu_fr.Fpu_regs[19],
			   fpu.fpu.fpu_fr.Fpu_regs[20], fpu.fpu.fpu_fr.Fpu_regs[21],
			   fpu.fpu.fpu_fr.Fpu_regs[22], fpu.fpu.fpu_fr.Fpu_regs[23],
			   fpu.fpu.fpu_fr.Fpu_regs[24], fpu.fpu.fpu_fr.Fpu_regs[25],
			   fpu.fpu.fpu_fr.Fpu_regs[26], fpu.fpu.fpu_fr.Fpu_regs[27],
			   fpu.fpu.fpu_fr.Fpu_regs[28], fpu.fpu.fpu_fr.Fpu_regs[29],
			   fpu.fpu.fpu_fr.Fpu_regs[30], fpu.fpu.fpu_fr.Fpu_regs[31],
			   fpu.fpu.Fpu_fsr);
		    break;
		  }
		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	if(mh->cputype == CPU_TYPE_POWERPC ||
	   mh->cputype == CPU_TYPE_VEO){
	    ppc_thread_state_t cpu;
	    ppc_float_state_t fpu;
	    ppc_exception_state_t except;

	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case PPC_THREAD_STATE:
		    printf("     flavor PPC_THREAD_STATE\n");
		    if(count == PPC_THREAD_STATE_COUNT)
			printf("      count PPC_THREAD_STATE_COUNT\n");
		    else
			printf("      count %lu (not PPC_THREAD_STATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(ppc_thread_state_t)){
		        memcpy((char *)&cpu, begin,
			       sizeof(ppc_thread_state_t));
		        begin += sizeof(ppc_thread_state_t);
		    }
		    else{
		        memset((char *)&cpu, '\0',
			       sizeof(ppc_thread_state_t));
		        memcpy((char *)&cpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_ppc_thread_state_t(&cpu, host_byte_sex);
		    printf("    r0  0x%08x r1  0x%08x r2  0x%08x r3   0x%08x "
			   "r4   0x%08x\n"
			   "    r5  0x%08x r6  0x%08x r7  0x%08x r8   0x%08x "
			   "r9   0x%08x\n"
			   "    r10 0x%08x r11 0x%08x r12 0x%08x r13  0x%08x "
			   "r14  0x%08x\n"
			   "    r15 0x%08x r16 0x%08x r17 0x%08x r18  0x%08x "
			   "r19  0x%08x\n"
			   "    r20 0x%08x r21 0x%08x r22 0x%08x r23  0x%08x "
			   "r24  0x%08x\n"
			   "    r25 0x%08x r26 0x%08x r27 0x%08x r28  0x%08x "
			   "r29  0x%08x\n"
			   "    r30 0x%08x r31 0x%08x cr  0x%08x xer  0x%08x "
			   "lr   0x%08x\n"
			   "    ctr 0x%08x mq  0x%08x pad 0x%08x srr0 0x%08x "
			   "srr1 0x%08x\n",
			   cpu.r0, cpu.r1, cpu.r2, cpu.r3, cpu.r4, cpu.r5,
			   cpu.r6, cpu.r7, cpu.r8, cpu.r9, cpu.r10, cpu.r11,
			   cpu.r12, cpu.r13, cpu.r14, cpu.r15, cpu.r16, cpu.r17,
			   cpu.r18, cpu.r19, cpu.r20, cpu.r21, cpu.r22, cpu.r23,
			   cpu.r24, cpu.r25, cpu.r26, cpu.r27, cpu.r28, cpu.r29,
			   cpu.r30, cpu.r31, cpu.cr,  cpu.xer, cpu.lr, cpu.ctr,
			   cpu.mq, cpu.pad, cpu.srr0, cpu.srr1);
		    break;
		case PPC_FLOAT_STATE:
		    printf("      flavor PPC_FLOAT_STATE\n");
		    if(count == PPC_FLOAT_STATE_COUNT)
			printf("      count PPC_FLOAT_STATE_COUNT\n");
		    else
			printf("      count %lu (not PPC_FLOAT_STATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(ppc_float_state_t)){
		        memcpy((char *)&fpu, begin,
			       sizeof(ppc_float_state_t));
		        begin += sizeof(ppc_float_state_t);
		    }
		    else{
		        memset((char *)&fpu, '\0',
			       sizeof(ppc_float_state_t));
		        memcpy((char *)&fpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_ppc_float_state_t(&fpu, host_byte_sex);
		    printf("       f0  %f    f1  %f\n       f2  %f    f3  %f\n"
			   "       f4  %f    f5  %f\n       f6  %f    f7  %f\n"
			   "       f8  %f    f9  %f\n       f10 %f    f11 %f\n"
			   "       f12 %f    f13 %f\n       f14 %f    f15 %f\n"
			   "       f16 %f    f17 %f\n       f18 %f    f19 %f\n"
			   "       f20 %f    f21 %f\n       f22 %f    f23 %f\n"
			   "       f24 %f    f25 %f\n       f26 %f    f27 %f\n"
			   "       f28 %f    f29 %f\n       f30 %f    f31 %f\n",
			   fpu.fpregs[0],  fpu.fpregs[1],  fpu.fpregs[2],
			   fpu.fpregs[3],  fpu.fpregs[4],  fpu.fpregs[5],
			   fpu.fpregs[6],  fpu.fpregs[7],  fpu.fpregs[8],
			   fpu.fpregs[9],  fpu.fpregs[10], fpu.fpregs[11],
			   fpu.fpregs[12], fpu.fpregs[13], fpu.fpregs[14],
			   fpu.fpregs[15], fpu.fpregs[16], fpu.fpregs[17],
			   fpu.fpregs[18], fpu.fpregs[19], fpu.fpregs[20],
			   fpu.fpregs[21], fpu.fpregs[22], fpu.fpregs[23],
			   fpu.fpregs[24], fpu.fpregs[25], fpu.fpregs[26],
			   fpu.fpregs[27], fpu.fpregs[28], fpu.fpregs[29],
			   fpu.fpregs[30], fpu.fpregs[31]);
		    printf("       fpscr_pad 0x%x fpscr 0x%x\n", fpu.fpscr_pad,
			   fpu.fpscr);
		    break;
		case PPC_EXCEPTION_STATE:
		    printf("      flavor PPC_EXCEPTION_STATE\n");
		    if(count == PPC_EXCEPTION_STATE_COUNT)
			printf("      count PPC_EXCEPTION_STATE_COUNT\n");
		    else
			printf("      count %lu (not PPC_EXCEPTION_STATE_COUNT"
			       ")\n", count);
		    left = end - begin;
		    if(left >= sizeof(ppc_exception_state_t)){
		        memcpy((char *)&except, begin,
			       sizeof(ppc_exception_state_t));
		        begin += sizeof(ppc_exception_state_t);
		    }
		    else{
		        memset((char *)&except, '\0',
			       sizeof(ppc_exception_state_t));
		        memcpy((char *)&except, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_ppc_exception_state_t(&except, host_byte_sex);
		    printf("      dar 0x%x dsisr 0x%x exception 0x%x pad0 "
			   "0x%x\n", (unsigned int)except.dar,
			   (unsigned int)except.dsisr,
			   (unsigned int)except.exception,
			   (unsigned int)except.pad0);
		    printf("      pad1[0] 0x%x pad1[1] 0x%x pad1[2] "
			   "0x%x pad1[3] 0x%x\n", (unsigned int)except.pad1[0],
			   (unsigned int)except.pad1[0],
			   (unsigned int)except.pad1[0],
			   (unsigned int)except.pad1[0]);
		    break;
		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	if(mh->cputype == CPU_TYPE_MC88000){
	    m88k_thread_state_grf_t cpu;
	    m88k_thread_state_xrf_t fpu;
	    m88k_thread_state_user_t user;
	    m88110_thread_state_impl_t spu;

	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case M88K_THREAD_STATE_GRF:
		    printf("      flavor M88K_THREAD_STATE_GRF\n");
		    if(count == M88K_THREAD_STATE_GRF_COUNT)
			printf("      count M88K_THREAD_STATE_GRF_COUNT\n");
		    else
			printf("      count %lu (not M88K_THREAD_STATE_GRF_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(m88k_thread_state_grf_t)){
		        memcpy((char *)&cpu, begin,
			       sizeof(m88k_thread_state_grf_t));
		        begin += sizeof(m88k_thread_state_grf_t);
		    }
		    else{
		        memset((char *)&cpu, '\0',
			       sizeof(m88k_thread_state_grf_t));
		        memcpy((char *)&cpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m88k_thread_state_grf_t(&cpu, host_byte_sex);
		    printf("      r1  0x%08x r2  0x%08x r3  0x%08x r4  0x%08x "
			   "r5  0x%08x\n"
			   "      r6  0x%08x r7  0x%08x r8  0x%08x r9  0x%08x "
			   "r10 0x%08x\n"
			   "      r11 0x%08x r12 0x%08x r13 0x%08x r14 0x%08x "
			   "r15 0x%08x\n"
			   "      r16 0x%08x r17 0x%08x r18 0x%08x r19 0x%08x "
			   "r20 0x%08x\n"
			   "      r21 0x%08x r22 0x%08x r23 0x%08x r24 0x%08x "
			   "r25 0x%08x\n"
			   "      r26 0x%08x r27 0x%08x r28 0x%08x r29 0x%08x "
			   "r30 0x%08x\n"
			   "      r31 0x%08x xip 0x%08x xip_in_bd 0x%08x nip "
			   "0x%08x\n",
			   cpu.r1,  cpu.r2,  cpu.r3,  cpu.r4,  cpu.r5,
			   cpu.r6,  cpu.r7,  cpu.r8,  cpu.r9,  cpu.r10,
			   cpu.r11, cpu.r12, cpu.r13, cpu.r14, cpu.r15,
			   cpu.r16, cpu.r17, cpu.r18, cpu.r19, cpu.r20,
			   cpu.r21, cpu.r22, cpu.r23, cpu.r24, cpu.r25,
			   cpu.r26, cpu.r27, cpu.r28, cpu.r29, cpu.r30,
			   cpu.r31, cpu.xip, cpu.xip_in_bd, cpu.nip);
		    break;
		case M88K_THREAD_STATE_XRF:
		    printf("      flavor M88K_THREAD_STATE_XRF\n");
		    if(count == M88K_THREAD_STATE_XRF_COUNT)
			printf("      count M88K_THREAD_STATE_XRF_COUNT\n");
		    else
			printf("      count %lu (not M88K_THREAD_STATE_XRF_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(m88k_thread_state_xrf_t)){
		        memcpy((char *)&fpu, begin,
			       sizeof(m88k_thread_state_xrf_t));
		        begin += sizeof(m88k_thread_state_xrf_t);
		    }
		    else{
		        memset((char *)&fpu, '\0',
			       sizeof(m88k_thread_state_xrf_t));
		        memcpy((char *)&fpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m88k_thread_state_xrf_t(&fpu, host_byte_sex);
		    printf("      x1  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x2  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x3  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x4  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x5  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x6  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x7  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x8  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x9  0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x10 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x11 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x12 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x13 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x14 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x15 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x16 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x17 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x18 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x19 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x20 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x21 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x22 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x23 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x24 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x25 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x26 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x27 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x28 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x29 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x30 0x%08x 0x%08x 0x%08x 0x%08x\n"
			   "      x31 0x%08x 0x%08x 0x%08x 0x%08x\n",
			   fpu.x1.x[0],fpu.x1.x[1],fpu.x1.x[2],fpu.x1.x[3],
			   fpu.x2.x[0],fpu.x2.x[1],fpu.x2.x[2],fpu.x2.x[3],
			   fpu.x3.x[0],fpu.x3.x[1],fpu.x3.x[2],fpu.x3.x[3],
			   fpu.x4.x[0],fpu.x4.x[1],fpu.x4.x[2],fpu.x4.x[3],
			   fpu.x5.x[0],fpu.x5.x[1],fpu.x5.x[2],fpu.x5.x[3],
			   fpu.x6.x[0],fpu.x6.x[1],fpu.x6.x[2],fpu.x6.x[3],
			   fpu.x7.x[0],fpu.x7.x[1],fpu.x7.x[2],fpu.x7.x[3],
			   fpu.x8.x[0],fpu.x8.x[1],fpu.x8.x[2],fpu.x8.x[3],
			   fpu.x9.x[0],fpu.x9.x[1],fpu.x9.x[2],fpu.x9.x[3],
			   fpu.x10.x[0],fpu.x10.x[1],fpu.x10.x[2],fpu.x10.x[3],
			   fpu.x11.x[0],fpu.x11.x[1],fpu.x11.x[2],fpu.x11.x[3],
			   fpu.x12.x[0],fpu.x12.x[1],fpu.x12.x[2],fpu.x12.x[3],
			   fpu.x13.x[0],fpu.x13.x[1],fpu.x13.x[2],fpu.x13.x[3],
			   fpu.x14.x[0],fpu.x14.x[1],fpu.x14.x[2],fpu.x14.x[3],
			   fpu.x15.x[0],fpu.x15.x[1],fpu.x15.x[2],fpu.x15.x[3],
			   fpu.x16.x[0],fpu.x16.x[1],fpu.x16.x[2],fpu.x16.x[3],
			   fpu.x17.x[0],fpu.x17.x[1],fpu.x17.x[2],fpu.x17.x[3],
			   fpu.x18.x[0],fpu.x18.x[1],fpu.x18.x[2],fpu.x18.x[3],
			   fpu.x19.x[0],fpu.x19.x[1],fpu.x19.x[2],fpu.x19.x[3],
			   fpu.x20.x[0],fpu.x20.x[1],fpu.x20.x[2],fpu.x20.x[3],
			   fpu.x21.x[0],fpu.x21.x[1],fpu.x21.x[2],fpu.x21.x[3],
			   fpu.x22.x[0],fpu.x22.x[1],fpu.x22.x[2],fpu.x22.x[3],
			   fpu.x23.x[0],fpu.x23.x[1],fpu.x23.x[2],fpu.x23.x[3],
			   fpu.x24.x[0],fpu.x24.x[1],fpu.x24.x[2],fpu.x24.x[3],
			   fpu.x25.x[0],fpu.x25.x[1],fpu.x25.x[2],fpu.x25.x[3],
			   fpu.x26.x[0],fpu.x26.x[1],fpu.x26.x[2],fpu.x26.x[3],
			   fpu.x27.x[0],fpu.x27.x[1],fpu.x27.x[2],fpu.x27.x[3],
			   fpu.x28.x[0],fpu.x28.x[1],fpu.x28.x[2],fpu.x28.x[3],
			   fpu.x29.x[0],fpu.x29.x[1],fpu.x29.x[2],fpu.x29.x[3],
			   fpu.x30.x[0],fpu.x30.x[1],fpu.x30.x[2],fpu.x30.x[3],
			   fpu.x31.x[0],fpu.x31.x[1],fpu.x31.x[2],fpu.x31.x[3]);
		    printf("      fpsr xmod %d afinv %d afdvz %d afunf %d "
			   "afovf %d afinx %d\n", fpu.fpsr.xmod, fpu.fpsr.afinv,
			   fpu.fpsr.afdvz, fpu.fpsr.afunf,
			   fpu.fpsr.afovf, fpu.fpsr.afinx);
		    printf("      fpcr rm ");
			switch(fpu.fpcr.rm){
			case M88K_RM_NEAREST:
			    printf("RM_NEAREST ");
			    break;
			case M88K_RM_ZERO:
			    printf("RM_ZERO ");
			    break;
			case M88K_RM_NEGINF:
			    printf("RM_NEGINF ");
			    break;
			case M88K_RM_POSINF:
			    printf("RM_POSINF ");
			    break;
			}
		    printf("efinv %d efdvz %d efunf %d efovf %d efinx %d\n",
			   fpu.fpcr.efinv, fpu.fpcr.efdvz, fpu.fpcr.efunf,
			   fpu.fpcr.efovf, fpu.fpcr.efinx);
		    break;
		case M88K_THREAD_STATE_USER:
		    printf("      flavor M88K_THREAD_STATE_USER\n");
		    if(count == M88K_THREAD_STATE_USER_COUNT)
			printf("      count M88K_THREAD_STATE_USER_COUNT\n");
		    else
			printf("      count %lu (not M88K_THREAD_STATE_USER_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(m88k_thread_state_user_t)){
		        memcpy((char *)&user, begin,
			       sizeof(m88k_thread_state_user_t));
		        begin += sizeof(m88k_thread_state_user_t);
		    }
		    else{
		        memset((char *)&user, '\0',
			       sizeof(m88k_thread_state_user_t));
		        memcpy((char *)&user, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m88k_thread_state_user_t(&user, host_byte_sex);
		    printf("      user 0x%08x\n", (unsigned int)user.user);
		    break;

		case M88110_THREAD_STATE_IMPL:
		    printf("      flavor M88110_THREAD_STATE_IMPL\n");
		    if(count == M88110_THREAD_STATE_IMPL_COUNT)
			printf("      count M88110_THREAD_STATE_IMPL_COUNT\n");
		    else
			printf("      count %lu (not M88110_THREAD_STATE_IMPL_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(m88110_thread_state_impl_t)){
		        memcpy((char *)&spu, begin,
			       sizeof(m88110_thread_state_impl_t));
		        begin += sizeof(m88110_thread_state_impl_t);
		    }
		    else{
		        memset((char *)&spu, '\0',
			       sizeof(m88110_thread_state_impl_t));
		        memcpy((char *)&spu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_m88110_thread_state_impl_t(&spu, host_byte_sex);
		    for(j = 0; j < M88110_N_DATA_BP; j++){
			printf("      data_bp[%lu] addr 0x%08x\n",
			       j, (unsigned int)spu.data_bp[i].addr);
			printf("                  cntl rw %d rwm %d "
			       "addr_match ", spu.data_bp[j].ctrl.rw,
			       spu.data_bp[j].ctrl.rwm);
			switch(spu.data_bp[j].ctrl.addr_match){
			case M88110_MATCH_BYTE:
			    printf("MATCH_BYTE ");
			    break;
			case M88110_MATCH_SHORT:
			    printf("MATCH_SHORT ");
			    break;
			case M88110_MATCH_WORD:
			    printf("MATCH_WORD ");
			    break;
			case M88110_MATCH_DOUBLE:
			    printf("MATCH_DOUBLE ");
			    break;
			case M88110_MATCH_QUAD:
			    printf("MATCH_QUAD ");
			    break;
			case M88110_MATCH_32:
			    printf("MATCH_32 ");
			    break;
			case M88110_MATCH_64:
			    printf("MATCH_64 ");
			    break;
			case M88110_MATCH_128:
			    printf("MATCH_128 ");
			    break;
			case M88110_MATCH_256:
			    printf("MATCH_256 ");
			    break;
			case M88110_MATCH_512:
			    printf("MATCH_512 ");
			    break;
			case M88110_MATCH_1024:
			    printf("MATCH_1024 ");
			    break;
			case M88110_MATCH_2048:
			    printf("MATCH_2048 ");
			    break;
			case M88110_MATCH_4096:
			    printf("MATCH_4096 ");
			    break;
			default:
			    printf("%d (?)", spu.data_bp[j].ctrl.addr_match);
			    break;
			}
			printf("v %d\n", spu.data_bp[j].ctrl.v);
		    }
		    printf("      psr le %d se %d c %d sgn_imd %d sm %d "
			   "sfu1dis %d mxm_dis %d\n" , spu.psr.le, spu.psr.se,
			   spu.psr.c, spu.psr.sgn_imd, spu.psr.sm,
			   spu.psr.sfu1dis, spu.psr.mxm_dis);
		    break;

		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	if(mh->cputype == CPU_TYPE_I860){
	    struct i860_thread_state_regs cpu;

	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case I860_THREAD_STATE_REGS:
		    printf("      flavor I860_THREAD_STATE_REGS\n");
		    if(count == I860_THREAD_STATE_REGS_COUNT)
			printf("      count I860_THREAD_STATE_REGS_COUNT\n");
		    else
			printf("      count %lu (not I860_THREAD_STATE_REGS_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(struct i860_thread_state_regs)){
		        memcpy((char *)&cpu, begin,
			       sizeof(struct i860_thread_state_regs));
		        begin += sizeof(struct i860_thread_state_regs);
		    }
		    else{
		        memset((char *)&cpu, '\0',
			       sizeof(struct i860_thread_state_regs));
		        memcpy((char *)&cpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_i860_thread_state_regs(&cpu, host_byte_sex);
		    printf(" iregs\n");
		    for(j = 0 ; j < 31 ; j += k){
			for(k = 0 ; k < 5 && j + k < 31 ; k++)
			    printf(" i%-2lu 0x%08x", j + k,
				   (unsigned int)cpu.ireg[j + k]);
			printf("\n");
		    }
		    printf(" fregs\n");
		    for(j = 0 ; j < 30 ; j += k){
			for(k = 0 ; k < 5 && j + k < 30 ; k++)
			    printf(" f%-2lu 0x%08x", j + k,
				   (unsigned int)cpu.freg[j + k]);
			printf("\n");
		    }
		    printf(" psr 0x%08x epsr 0x%08x db 0x%08x pc 0x%08x\n",
			   (unsigned int)cpu.psr, (unsigned int)cpu.epsr,
			   (unsigned int)cpu.db, (unsigned int)cpu.pc);
		    printf(" Mres3 %e Ares3 %e\n", cpu.Mres3, cpu.Ares3);
		    printf(" Mres2 %e Ares2 %e\n", cpu.Mres2, cpu.Ares2);
		    printf(" Mres1 %e Ares1 %e\n", cpu.Mres1, cpu.Ares1);
		    printf(" Ires1 %e\n", cpu.Ires1);
		    printf(" Lres3m %e Lres2m %e Lres1m %e\n", cpu.Lres3m,
			   cpu.Lres2m, cpu.Lres1m);
		    printf(" KR %e KI %e T %e\n", cpu.KR, cpu.KI, cpu.T);
		    printf(" Fsr3 0x%08x Fsr2 0x%08x Fsr1 0x%08x\n",
			   (unsigned int)cpu.Fsr3, (unsigned int)cpu.Fsr2,
			   (unsigned int)cpu.Fsr1);
		    printf(" Mergelo32 0x%08x Mergehi32 0x%08x\n",
			   (unsigned int)cpu.Mergelo32,
			   (unsigned int)cpu.Mergehi32);
		    break;

		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	if(mh->cputype == CPU_TYPE_I386){
	    i386_thread_state_t cpu;
	    i386_thread_fpstate_t fpu;
	    i386_thread_exceptstate_t exc;
	    i386_thread_cthreadstate_t user;
	    const char *tags[] = { "VALID", "ZERO", "SPEC", "EMPTY" };

	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);

		switch(flavor){
		case i386_THREAD_STATE:
		    printf("     flavor i386_THREAD_STATE\n");
		    if(count == i386_THREAD_STATE_COUNT)
			printf("      count i386_THREAD_STATE_COUNT\n");
		    else
			printf("      count %lu (not i386_THREAD_STATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(i386_thread_state_t)){
		        memcpy((char *)&cpu, begin,
			       sizeof(i386_thread_state_t));
		        begin += sizeof(i386_thread_state_t);
		    }
		    else{
		        memset((char *)&cpu, '\0',
			       sizeof(i386_thread_state_t));
		        memcpy((char *)&cpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_i386_thread_state(&cpu, host_byte_sex);
		    printf(
		       "\t    eax 0x%08x ebx    0x%08x ecx 0x%08x edx 0x%08x\n"
		       "\t    edi 0x%08x esi    0x%08x ebp 0x%08x esp 0x%08x\n"
		       "\t    ss  0x%08x eflags 0x%08x eip 0x%08x cs  0x%08x\n"
		       "\t    ds  0x%08x es     0x%08x fs  0x%08x gs  0x%08x\n",
			cpu.eax, cpu.ebx, cpu.ecx, cpu.edx, cpu.edi, cpu.esi,
			cpu.ebp, cpu.esp, cpu.ss, cpu.eflags, cpu.eip, cpu.cs,
			cpu.ds, cpu.es, cpu.fs, cpu.gs);
		    break;

		case i386_THREAD_FPSTATE:
		    printf("     flavor i386_THREAD_FPSTATE\n");
		    if(count == i386_THREAD_FPSTATE_COUNT)
			printf("      count i386_THREAD_FPSTATE_COUNT\n");
		    else
			printf("      count %lu (not i386_THREAD_FPSTATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(i386_thread_fpstate_t)){
		        memcpy((char *)&fpu, begin,
			       sizeof(i386_thread_fpstate_t));
		        begin += sizeof(i386_thread_fpstate_t);
		    }
		    else{
		        memset((char *)&fpu, '\0',
			       sizeof(i386_thread_fpstate_t));
		        memcpy((char *)&fpu, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_i386_thread_fpstate(&fpu, host_byte_sex);
		    printf("\t    control: invalid %d denorm %d zdiv %d ovrfl "
			   "%d undfl %d precis %d\n",
			   fpu.environ.control.invalid,
			   fpu.environ.control.denorm,
			   fpu.environ.control.zdiv,
			   fpu.environ.control.ovrfl,
			   fpu.environ.control.undfl,
			   fpu.environ.control.precis);
		    printf("\t\t     pc ");
		    switch(fpu.environ.control.pc){
		    case FP_PREC_24B:
			printf("FP_PREC_24B ");
			break;
		    case FP_PREC_53B:
			printf("FP_PREC_53B ");
			break;
		    case FP_PREC_64B:
			printf("FP_PREC_64B ");
			break;
		    default:
			printf("%d ", fpu.environ.control.pc);
			break;
		    }
		    printf("rc ");
		    switch(fpu.environ.control.rc){
		    case FP_RND_NEAR:
			printf("FP_RND_NEAR ");
			break;
		    case FP_RND_DOWN:
			printf("FP_RND_DOWN ");
			break;
		    case FP_RND_UP:
			printf("FP_RND_UP ");
			break;
		    case FP_CHOP:
			printf("FP_CHOP ");
			break;
		    }
		    printf("\n");

		    printf("\t    status: invalid %d denorm %d zdiv %d ovrfl "
			   "%d undfl %d precis %d stkflt %d\n",
			   fpu.environ.status.invalid,
			   fpu.environ.status.denorm,
			   fpu.environ.status.zdiv,
			   fpu.environ.status.ovrfl,
			   fpu.environ.status.undfl,
			   fpu.environ.status.precis,
			   fpu.environ.status.stkflt);
		    printf("\t\t    errsumm %d c0 %d c1 %d c2 %d tos %d c3 %d "
			   "busy %d\n", fpu.environ.status.errsumm,
			   fpu.environ.status.c0, fpu.environ.status.c1,
			   fpu.environ.status.c2, fpu.environ.status.tos,
			   fpu.environ.status.c3, fpu.environ.status.busy);
		    printf("\t    tags: tag0 %s tag1 %s tag2 %s tag3 %s\n"
			   "\t          tag4 %s tag5 %s tag6 %s tag7 %s\n",
			   tags[fpu.environ.tag.tag0],
			   tags[fpu.environ.tag.tag1],
			   tags[fpu.environ.tag.tag2],
			   tags[fpu.environ.tag.tag3],
			   tags[fpu.environ.tag.tag4],
			   tags[fpu.environ.tag.tag5],
			   tags[fpu.environ.tag.tag6],
			   tags[fpu.environ.tag.tag7]);
		    printf("\t    ip 0x%08x\n", fpu.environ.ip);
		    printf("\t    cs: rpl ");
		    switch(fpu.environ.cs.rpl){
		    case KERN_PRIV:
			printf("KERN_PRIV ");
			break;
		    case USER_PRIV:
			printf("USER_PRIV ");
			break;
		    default:
			printf("%d ", fpu.environ.cs.rpl);
			break;
		    }
		    printf("ti ");
		    switch(fpu.environ.cs.ti){
		    case SEL_GDT:
			printf("SEL_GDT ");
			break;
		    case SEL_LDT:
			printf("SEL_LDT ");
			break;
		    }
		    printf("index %d\n", fpu.environ.cs.index);
		    printf("\t    opcode 0x%04x\n",
			   (unsigned int)fpu.environ.opcode);
		    printf("\t    dp 0x%08x\n", fpu.environ.dp);
		    printf("\t    ds: rpl ");
		    switch(fpu.environ.ds.rpl){
		    case KERN_PRIV:
			printf("KERN_PRIV ");
			break;
		    case USER_PRIV:
			printf("USER_PRIV ");
			break;
		    default:
			printf("%d ", fpu.environ.ds.rpl);
			break;
		    }
		    printf("ti ");
		    switch(fpu.environ.ds.ti){
		    case SEL_GDT:
			printf("SEL_GDT ");
			break;
		    case SEL_LDT:
			printf("SEL_LDT ");
			break;
		    }
		    printf("index %d\n", fpu.environ.ds.index);
		    printf("\t    stack:\n");
		    for(i = 0; i < 8; i++){
			printf("\t\tST[%lu] mant 0x%04x 0x%04x 0x%04x 0x%04x "
			       "exp 0x%04x sign %d\n", i,
			       (unsigned int)fpu.stack.ST[i].mant,
			       (unsigned int)fpu.stack.ST[i].mant1,
			       (unsigned int)fpu.stack.ST[i].mant2,
			       (unsigned int)fpu.stack.ST[i].mant3,
			       (unsigned int)fpu.stack.ST[i].exp,
			       fpu.stack.ST[i].sign);
		    }
		    break;
		case i386_THREAD_EXCEPTSTATE:
		    printf("     flavor i386_THREAD_EXCEPTSTATE\n");
		    if(count == i386_THREAD_EXCEPTSTATE_COUNT)
			printf("      count i386_THREAD_EXCEPTSTATE_COUNT\n");
		    else
			printf("      count %lu (not i386_THREAD_EXCEPTSTATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(i386_thread_exceptstate_t)){
		        memcpy((char *)&exc, begin,
			       sizeof(i386_thread_exceptstate_t));
		        begin += sizeof(i386_thread_exceptstate_t);
		    }
		    else{
		        memset((char *)&exc, '\0',
			       sizeof(i386_thread_exceptstate_t));
		        memcpy((char *)&exc, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_i386_thread_exceptstate(&exc, host_byte_sex);
		    printf("\t    trapno 0x%08x\n", exc.trapno);
		    if(exc.trapno == 14){
			printf("\t    err.pgfault: prot %d wrtflt %d user %d\n",
			       exc.err.pgfault.prot, exc.err.pgfault.wrtflt,
			       exc.err.pgfault.user);
		    }
		    else{
			printf("\t    err.normal: ext %d ", exc.err.normal.ext);
			printf("tbl ");
			switch(exc.err.normal.tbl){
		        case ERR_GDT:
			    printf("ERR_GDT ");
			    break;
		        case ERR_IDT:
			    printf("ERR_IDT ");
			    break;
		        case ERR_LDT:
			    printf("ERR_LDT ");
			    break;
			default:
			    printf("%d ", exc.err.normal.tbl);
			    break;
			}
			printf("index %d\n", exc.err.normal.index);
		    }
		    break;

		case i386_THREAD_CTHREADSTATE:
		    printf("     flavor i386_THREAD_CTHREADSTATE\n");
		    if(count == i386_THREAD_CTHREADSTATE_COUNT)
			printf("      count i386_THREAD_CTHREADSTATE_COUNT\n");
		    else
			printf("      count %lu (not i386_THREAD_CTHREADSTATE_"
			       "COUNT)\n", count);
		    left = end - begin;
		    if(left >= sizeof(i386_thread_cthreadstate_t)){
		        memcpy((char *)&user, begin,
			       sizeof(i386_thread_cthreadstate_t));
		        begin += sizeof(i386_thread_cthreadstate_t);
		    }
		    else{
		        memset((char *)&user, '\0',
			       sizeof(i386_thread_cthreadstate_t));
		        memcpy((char *)&user, begin, left);
		        begin += left;
		    }
		    if(swapped)
			swap_i386_thread_cthreadstate(&user, host_byte_sex);
		    printf("\t    self 0x%08x\n", user.self);
		    break;
		default:
		    printf("     flavor %lu (unknown)\n", flavor);
		    printf("      count %lu\n", count);
		    printf("      state:\n");
		    print_unknown_state(begin, end, count, swapped);
		    begin += count * sizeof(unsigned long);
		    break;
		}
	    }
	}
	else{
	    while(begin < end){
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&flavor, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    flavor = 0;
		    begin = end;
		}
		if(swapped)
		    flavor = SWAP_LONG(flavor);
		if(end - begin > (ptrdiff_t)sizeof(unsigned long)){
		    memcpy((char *)&count, begin, sizeof(unsigned long));
		    begin += sizeof(unsigned long);
		}
		else{
		    count = 0;
		    begin = end;
		}
		if(swapped)
		    count = SWAP_LONG(count);
		printf("     flavor %lu\n", flavor);
		printf("      count %lu\n", count);
		printf("      state (Unknown cputype/cpusubtype):\n");
		print_unknown_state(begin, end, count, swapped);
		begin += count * sizeof(unsigned long);
	    }
	}
}

static
void
print_unknown_state(
char *begin,
char *end,
unsigned int count,
enum bool swapped)
{
    unsigned long left, *state, i, j;

	left = end - begin;
	if(left * sizeof(unsigned long) >= count){
	    state = allocate(count * sizeof(unsigned long));
	    memcpy((char *)state, begin, count * sizeof(unsigned long));
	    begin += count * sizeof(unsigned long);
	}
	else{
	    state = allocate(left);
	    memset((char *)state, '\0', left);
	    memcpy((char *)state, begin, left);
	    count = left / sizeof(unsigned long);
	    begin += left;
	}
	if(swapped)
	    for(i = 0 ; i < count; i++)
		state[i] = SWAP_LONG(state[i]);
	for(i = 0 ; i < count; i += j){
	    for(j = 0 ; j < 8 && i + j < count; j++)
		printf("%08x ", (unsigned int)state[i + j]);
	    printf("\n");
	}
	free(state);
}

void
walk_loadcmds(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
unsigned long object_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, left, size;
    char *p;
    struct load_command *lc, l;
    struct segment_command sg;
    struct section s;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of sizeof(long)\n",
		       i);
	    if((char *)lc + l.cmdsize > (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load commands\n",
		       i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
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
			printf("section structure command extends past end of "
			       "load commands\n");
		    }
		    left = mh->sizeofcmds - (p - (char *)load_commands);
		    memset((char *)&s, '\0', sizeof(struct section));
		    size = left < sizeof(struct section) ?
			   left : sizeof(struct section);
		    memcpy((char *)&s, p, size);
		    if(swapped)
			swap_section(&s, 1, host_byte_sex);

		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds)
			return;
		    p += size;
		}
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		return;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		return;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");
}

/*
 * Print the relocation information.
 */
void
print_reloc(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct nlist *symbols,
long nsymbols,
char *strings,
long strings_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, k, left, size, nsects;
    char *p;
    struct load_command *lc, l;
    struct segment_command sg;
    struct section *sections;
    struct dysymtab_command dyst;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	/*
	 * Create an array of section structures in the host byte sex so it
	 * can be processed and indexed into directly.
	 */
	k = 0;
	nsects = 0;
	sections = NULL;
	lc = load_commands;
	dyst.cmd = 0;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		nsects += sg.nsects;
		sections = reallocate(sections,
				      nsects * sizeof(struct section));
		memset((char *)(sections + (nsects - sg.nsects)), '\0',
		       sizeof(struct section) * sg.nsects);
		p = (char *)lc + sizeof(struct segment_command);
		for(j = 0 ; j < sg.nsects ; j++){
		    left = mh->sizeofcmds - (p - (char *)load_commands);
		    size = left < sizeof(struct section) ?
			   left : sizeof(struct section);
		    memcpy((char *)(sections + k), p, size);
		    if(swapped)
			swap_section(sections + k, 1, host_byte_sex);

		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds)
			break;
		    p += size;
		    k++;
		}
		break;
	    case LC_DYSYMTAB:
		memset((char *)&dyst, '\0', sizeof(struct dysymtab_command));
		size = left < sizeof(struct dysymtab_command) ?
		       left : sizeof(struct dysymtab_command);
		memcpy((char *)&dyst, (char *)lc, size);
		if(swapped)
		    swap_dysymtab_command(&dyst, host_byte_sex);
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");

	if(dyst.cmd != 0){
	    printf("External relocation information %lu entries", dyst.nextrel);
	    if(dyst.extreloff > object_size){
		printf(" (offset to relocation entries extends past the end of "
		       " the file)\n");
	    }
	    else{
		printf("\naddress  pcrel length extern type    scattered "
		       "symbolnum/value\n");

		print_relocs(dyst.extreloff, dyst.nextrel, sections, nsects,
			     swapped, mh, object_addr, object_size, symbols,
			     nsymbols, strings, strings_size, verbose);
	    }
	    printf("Local relocation information %lu entries", dyst.nlocrel);
	    if(dyst.locreloff > object_size){
		printf(" (offset to relocation entries extends past the end of "
		       " the file)\n");
	    }
	    else{
		printf("\naddress  pcrel length extern type    scattered "
		       "symbolnum/value\n");

		print_relocs(dyst.locreloff, dyst.nlocrel, sections, nsects,
			     swapped, mh, object_addr, object_size, symbols,
			     nsymbols, strings, strings_size, verbose);
	    }
	}

	for(i = 0 ; i < nsects ; i++){
	    if(sections[i].nreloc == 0)
		continue;
	    printf("Relocation information (%.16s,%.16s) %lu entries",
		   sections[i].segname, sections[i].sectname,
		   sections[i].nreloc);
	    if(sections[i].reloff > object_size){
		printf(" (offset to relocation entries extends past the end of "
		       " the file)\n");
		continue;
	    }
	    printf("\naddress  pcrel length extern type    scattered "
		   "symbolnum/value\n");

	    print_relocs(sections[i].reloff, sections[i].nreloc, sections,
			 nsects, swapped, mh, object_addr, object_size, symbols,
			 nsymbols, strings, strings_size, verbose);
	}
}

static
void
print_relocs(
unsigned reloff,
unsigned nreloc,
struct section *sections,
unsigned long nsects,
enum bool swapped,
struct mach_header *mh,
char *object_addr,
unsigned long object_size,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    unsigned long j;
    struct relocation_info *r, reloc;
    struct scattered_relocation_info *sr;
    enum bool previous_sectdiff, previous_ppc_jbsr, predicted;
    unsigned long sectdiff_r_type;

	host_byte_sex = get_host_byte_sex();

	previous_sectdiff = FALSE;
	previous_ppc_jbsr = FALSE;
	sectdiff_r_type = 0;
	for(j = 0 ;
	    j < nreloc &&
	    reloff + (j + 1) * sizeof(struct relocation_info) <= object_size;
	    j++){
	    predicted = FALSE;
	    r = (struct relocation_info *)
		 (object_addr + reloff +
		  j * sizeof(struct relocation_info));
	    memcpy((char *)&reloc, (char *)r,
		   sizeof(struct relocation_info));
	    if(swapped)
		swap_relocation_info(&reloc, 1, host_byte_sex);
	    
	    if((reloc.r_address & R_SCATTERED) != 0){
		sr = (struct scattered_relocation_info *)&reloc; 
		if(verbose){
		    if((mh->cputype == CPU_TYPE_MC680x0 &&
			sr->r_type == GENERIC_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_I386 &&
			sr->r_type == GENERIC_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_MC88000 &&
			sr->r_type == M88K_RELOC_PAIR) ||
		       ((mh->cputype == CPU_TYPE_POWERPC ||
		         mh->cputype == CPU_TYPE_VEO) &&
			sr->r_type == PPC_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_HPPA &&
			sr->r_type == HPPA_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_SPARC &&
			sr->r_type == SPARC_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_I860 &&
			sr->r_type == I860_RELOC_PAIR))
			    printf("         ");
		    else
			printf("%08x ", (unsigned int)sr->r_address);
		    if(sr->r_pcrel)
			printf("True  ");
		    else
			printf("False ");
		    switch(sr->r_length){
		    case 0:
			printf("byte   ");
			break;
		    case 1:
			printf("word   ");
			break;
		    case 2:
			printf("long   ");
			break;
		    case 3:
			/*
			 * The value of 3 for r_length for PowerPC is to encode
			 * that a conditional branch using the Y-bit for static
			 * branch prediction was predicted in the assembly
			 * source.
			 */
			if(mh->cputype == CPU_TYPE_POWERPC ||
			   mh->cputype == CPU_TYPE_VEO){
			    printf("long   ");
			    predicted = TRUE;
			}
			else
			    printf("?(%2d)  ", sr->r_length);
			break;
		    default:
			printf("?(%2d)  ", sr->r_length);
			break;
		    }
		    printf("n/a    ");
		    print_r_type(mh->cputype, sr->r_type, predicted);
		    printf("True      0x%08x", (unsigned int)sr->r_value);
		    if(previous_sectdiff == FALSE){
			if((mh->cputype == CPU_TYPE_MC88000 &&
			    sr->r_type == M88K_RELOC_PAIR) ||
			   (mh->cputype == CPU_TYPE_SPARC &&
			    sr->r_type == SPARC_RELOC_PAIR) ||
			   (mh->cputype == CPU_TYPE_I860 &&
			    sr->r_type == I860_RELOC_PAIR))
			    printf(" half = 0x%04x ",
				   (unsigned int)sr->r_address);
			else if(mh->cputype == CPU_TYPE_HPPA &&
				 sr->r_type == HPPA_RELOC_PAIR)
			    printf(" other_part = 0x%06x ",
				   (unsigned int)sr->r_address);
			else if(((mh->cputype == CPU_TYPE_POWERPC ||
				  mh->cputype == CPU_TYPE_VEO) &&
				 sr->r_type == PPC_RELOC_PAIR)){
			    if(previous_ppc_jbsr == FALSE)
				printf(" half = 0x%04x ",
				       (unsigned int)reloc.r_address);
			    else{
				printf(" <- other_part ");
			    }
			}
		    }
		    else if(mh->cputype == CPU_TYPE_HPPA &&
			    (sectdiff_r_type == HPPA_RELOC_HI21_SECTDIFF ||
			     sectdiff_r_type == HPPA_RELOC_LO14_SECTDIFF)){
			    printf(" other_part = 0x%06x ",
				   (unsigned int)sr->r_address);
		    }
		    else if(mh->cputype == CPU_TYPE_SPARC &&
			    (sectdiff_r_type == SPARC_RELOC_HI22_SECTDIFF ||
			     sectdiff_r_type == SPARC_RELOC_LO10_SECTDIFF)){
			    printf(" other_part = 0x%06x ",
				   (unsigned int)sr->r_address);
		    }
		    else if((mh->cputype == CPU_TYPE_POWERPC ||
			     mh->cputype == CPU_TYPE_VEO) &&
			    (sectdiff_r_type == PPC_RELOC_HI16_SECTDIFF ||
			     sectdiff_r_type == PPC_RELOC_LO16_SECTDIFF ||
			     sectdiff_r_type == PPC_RELOC_LO14_SECTDIFF ||
			     sectdiff_r_type == PPC_RELOC_HA16_SECTDIFF)){
			    printf(" other_half = 0x%04x ",
				   (unsigned int)sr->r_address);
		    }
		    if((mh->cputype == CPU_TYPE_MC680x0 &&
			sr->r_type == GENERIC_RELOC_SECTDIFF) ||
		       (mh->cputype == CPU_TYPE_I386 &&
			sr->r_type == GENERIC_RELOC_SECTDIFF) ||
		       (mh->cputype == CPU_TYPE_MC88000 &&
			sr->r_type == M88K_RELOC_SECTDIFF) ||
		       ((mh->cputype == CPU_TYPE_POWERPC ||
		         mh->cputype == CPU_TYPE_VEO) &&
			(sr->r_type == PPC_RELOC_SECTDIFF ||
			 sr->r_type == PPC_RELOC_HI16_SECTDIFF ||
			 sr->r_type == PPC_RELOC_LO16_SECTDIFF ||
			 sr->r_type == PPC_RELOC_LO14_SECTDIFF ||
			 sr->r_type == PPC_RELOC_HA16_SECTDIFF)) ||
		       (mh->cputype == CPU_TYPE_I860 &&
			sr->r_type == I860_RELOC_SECTDIFF) ||
		       (mh->cputype == CPU_TYPE_HPPA &&
			(sr->r_type == HPPA_RELOC_SECTDIFF ||
			 sr->r_type == HPPA_RELOC_HI21_SECTDIFF ||
			 sr->r_type == HPPA_RELOC_LO14_SECTDIFF)) ||
		       (mh->cputype == CPU_TYPE_SPARC &&
			(sr->r_type == SPARC_RELOC_SECTDIFF ||
			 sr->r_type == SPARC_RELOC_HI22_SECTDIFF ||
			 sr->r_type == SPARC_RELOC_LO10_SECTDIFF))){
			previous_sectdiff = TRUE;
			sectdiff_r_type = sr->r_type;
		    }
		    else
			previous_sectdiff = FALSE;
		    if(((mh->cputype == CPU_TYPE_POWERPC ||
		         mh->cputype == CPU_TYPE_VEO) &&
			 sr->r_type == PPC_RELOC_JBSR))
			previous_ppc_jbsr = TRUE;
		    else
			previous_ppc_jbsr = FALSE;
		    printf("\n");
		}
		else{
		    printf("%08x %1d     %-2d     n/a    %-7d 1         "
			   "0x%08x\n", (unsigned int)sr->r_address,
			   sr->r_pcrel, sr->r_length, sr->r_type,
			   (unsigned int)sr->r_value);
		}
	    }
	    else{
		if(verbose){
		    if((mh->cputype == CPU_TYPE_MC88000 &&
			reloc.r_type == M88K_RELOC_PAIR) ||
		       ((mh->cputype == CPU_TYPE_POWERPC ||
		         mh->cputype == CPU_TYPE_VEO) &&
			reloc.r_type == PPC_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_HPPA &&
			reloc.r_type == HPPA_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_SPARC &&
			reloc.r_type == SPARC_RELOC_PAIR) ||
		       (mh->cputype == CPU_TYPE_I860 &&
			reloc.r_type == I860_RELOC_PAIR))
			    printf("         ");
		    else
			printf("%08x ", (unsigned int)reloc.r_address);
		    if(reloc.r_pcrel)
			printf("True  ");
		    else
			printf("False ");
		    switch(reloc.r_length){
		    case 0:
			printf("byte   ");
			break;
		    case 1:
			printf("word   ");
			break;
		    case 2:
			printf("long   ");
			break;
		    case 3:
			/*
			 * The value of 3 for r_length for PowerPC is to encode
			 * that a conditional branch using the Y-bit for static
			 * branch prediction was predicted in the assembly
			 * source.
			 */
			if(mh->cputype == CPU_TYPE_POWERPC ||
			   mh->cputype == CPU_TYPE_VEO){
			    printf("long   ");
			    predicted = TRUE;
			}
			else
			    printf("?(%2d)  ", reloc.r_length);
			break;
		    default:
			printf("?(%2d)  ", reloc.r_length);
			break;
		    }
		    if(reloc.r_extern){
			printf("True   ");
			print_r_type(mh->cputype, reloc.r_type, predicted);
			printf("False     ");
			if(symbols == NULL || strings == NULL ||
			   reloc.r_symbolnum > nsymbols ||
			   (unsigned long)symbols[reloc.r_symbolnum].
				n_un.n_strx > strings_size)
			    printf("?(%d)\n", reloc.r_symbolnum);
			else{
			    printf("%s\n", strings +
				   symbols[reloc.r_symbolnum].n_un.n_strx);
			}
		    }
		    else{
			printf("False  ");
			print_r_type(mh->cputype, reloc.r_type, predicted);
			printf("False     ");
			if((mh->cputype == CPU_TYPE_I860 &&
			    reloc.r_type == I860_RELOC_PAIR) ||
			   (mh->cputype == CPU_TYPE_MC88000 &&
			    reloc.r_type == M88K_RELOC_PAIR) ){
			    printf("half = 0x%04x\n",
				   (unsigned int)reloc.r_address);
			}
			else if((mh->cputype == CPU_TYPE_HPPA &&
				 reloc.r_type == HPPA_RELOC_PAIR) ||
				(mh->cputype == CPU_TYPE_SPARC &&
				 reloc.r_type == SPARC_RELOC_PAIR)){
			    printf(" other_part = 0x%06x\n",
				   (unsigned int)reloc.r_address);
			}
			else if(((mh->cputype == CPU_TYPE_POWERPC ||
				  mh->cputype == CPU_TYPE_VEO) &&
				 reloc.r_type == PPC_RELOC_PAIR)){
			    if(previous_ppc_jbsr == FALSE)
				printf("half = 0x%04x\n",
				       (unsigned int)reloc.r_address);
			    else
				printf("other_part = 0x%08x\n",
				       (unsigned int)reloc.r_address);
			}
			else{
			    printf("%d ", reloc.r_symbolnum);
			    if(reloc.r_symbolnum > nsects + 1)
				printf("(?,?)\n");
			    else{
				if(reloc.r_symbolnum == R_ABS)
				    printf("R_ABS\n");
				else
				    printf("(%.16s,%.16s)\n",
				    sections[reloc.r_symbolnum-1].segname,
				    sections[reloc.r_symbolnum-1].sectname);
			    }
			}
		    }
		    if(((mh->cputype == CPU_TYPE_POWERPC ||
		         mh->cputype == CPU_TYPE_VEO) &&
			 reloc.r_type == PPC_RELOC_JBSR))
			previous_ppc_jbsr = TRUE;
		    else
			previous_ppc_jbsr = FALSE;
		}
		else{
		    printf("%08x %1d     %-2d     %1d      %-7d 0"
			   "         %d\n", (unsigned int)reloc.r_address,
			   reloc.r_pcrel, reloc.r_length, reloc.r_extern,
			   reloc.r_type, reloc.r_symbolnum);
		}
	    }
	}
}


static char *generic_r_types[] = {
    "VANILLA ", "PAIR    ", "SECTDIF ", "PBLAPTR ", "  4 (?) ", "  5 (?) ",
    "  6 (?) ", "  7 (?) ", "  8 (?) ", "  9 (?) ", " 10 (?) ", " 11 (?) ",
    " 12 (?) ", " 13 (?) ", " 14 (?) ", " 15 (?) "
};
static char *m88k_r_types[] = {
    "VANILLA ", "PAIR    ", "PC16    ", "PC26    ", "HI16    ", "LO16    ",
    "SECTDIF ", "PBLAPTR ", "  8 (?) ", "  9 (?) ", " 10 (?) ", " 11 (?) ",
    " 12 (?) ", " 13 (?) ", " 14 (?) ", " 15 (?) "
};
static char *i860_r_types[] = {
    "VANILLA ", "PAIR    ", "HIGH    ", "LOW0    ", "LOW1    ", "LOW2    ",
    "LOW3    ", "LOW4    ", "SPLIT0  ", "SPLIT1  ", "SPLIT2  ", "HIGHADJ ",
    "BRADDR  ", "SECTDIF ", " 14 (?) ", " 15 (?) "
};
static char *ppc_r_types[] = {
    "VANILLA ", "PAIR    ", "BR14",     "BR24    ", "HI16    ", "LO16    ",
    "HA16    ", "LO14    ", "SECTDIF ", "PBLAPTR ", "HI16DIF ", "LO16DIF ",
    "HA16DIF ", "JBSR    ", "LO14DIF ", " 15 (?) "
};
static char *hppa_r_types[] = {
	"VANILLA ", "PAIR    ", "HI21    ", "LO14    ", "BR17    ",
	"BL17    ", "JBSR    ", "SECTDIF ", "HI21DIF ", "LO14DIF ",
	"PBLAPTR ", " 11 (?) ", " 12 (?) ", " 13 (?) ", " 14 (?) ", " 15 (?) "
};

static char *sparc_r_types[] = {
	"VANILLA ", "PAIR    ", "HI22    ", "LO10    ", "DISP22  ",
	"DISP30  ", "SECTDIFF", "HI22DIFF", "LO10DIFF", "PBLAPTR ", 
	" 10 (?) ", " 11 (?) ", " 12 (?) ", " 13 (?) ", " 14 (?) ", " 15 (?) "
};

static
void
print_r_type(
cpu_type_t cputype,
unsigned long r_type,
enum bool predicted)
{
	if(r_type > 0xf){
	    printf("%-7lu ", r_type);
	    return;
	}
	switch(cputype){
	case CPU_TYPE_MC680x0:
	case CPU_TYPE_I386:
	    printf("%s", generic_r_types[r_type]);
	    break;
	case CPU_TYPE_MC88000:
	    printf("%s", m88k_r_types[r_type]);
	    break;
	case CPU_TYPE_I860:
	    printf("%s", i860_r_types[r_type]);
	    break;
	case CPU_TYPE_POWERPC:
	case CPU_TYPE_VEO:
	    printf("%s", ppc_r_types[r_type]);
	    if(r_type == PPC_RELOC_BR14){
		if(predicted == TRUE)
		    printf("+/- ");
		else
		    printf("    ");
	    }
	    break;
	case CPU_TYPE_HPPA:
	    printf("%s", hppa_r_types[r_type]);
	    break;
	case CPU_TYPE_SPARC:
	    printf("%s", sparc_r_types[r_type]);
	    break;
	default:
	    printf("%-7lu ", r_type);
	}
}

/*
 * Print the table of contents.
 */
void
print_toc(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct dylib_table_of_contents *tocs,
unsigned long ntocs,
struct dylib_module *mods,
unsigned long nmods,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
   unsigned long i;

	printf("Table of contents (%lu entries)\n", ntocs);
	if(verbose)
	    printf("module name      symbol name\n");
	else
	    printf("module index symbol index\n");
	for(i = 0; i < ntocs; i++){
	    if(verbose){
		if(tocs[i].module_index > nmods)
		    printf("%-16lu (past the end of the module table) ",
			   tocs[i].module_index);
		else if(mods[tocs[i].module_index].module_name > strings_size)
		    printf("%-16lu (string index past the end of string "
			   "table) ", tocs[i].module_index);
		else
		    printf("%-16s ", strings +
			   mods[tocs[i].module_index].module_name);

		if(tocs[i].symbol_index > nsymbols)
		    printf("%lu (past the end of the symbol table)\n",
			   tocs[i].symbol_index);
		else if((unsigned long)symbols[tocs[i].symbol_index].
			n_un.n_strx > strings_size)
		    printf("%lu (string index past the end of the string "
			   "table)\n", tocs[i].symbol_index);
		else{
		    printf("%s", strings +
			   symbols[tocs[i].symbol_index].n_un.n_strx);
		    if(symbols[tocs[i].symbol_index].n_type & N_EXT)
			printf("\n");
		    else
			printf(" [private]\n");
		}
	    }
	    else{
		printf("%-12lu %lu\n", tocs[i].module_index,
		       tocs[i].symbol_index);
	    }
	}
}

/*
 * Print the module table.
 */
void
print_module_table(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct dylib_module *mods,
unsigned long nmods,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
   unsigned long i;

	printf("Module table (%lu entries)\n", nmods);
	for(i = 0; i < nmods; i++){
	    printf("module %lu\n", i);
	    if(verbose){
		if(mods[i].module_name > strings_size)
		    printf("    module_name = %lu (past end of string table)\n",
			   mods[i].module_name);
		else
		    printf("    module_name = %s\n",
			   strings + mods[i].module_name);
	    }
	    else{
		if(mods[i].module_name > strings_size)
		    printf("    module_name = %lu (past end of string table)\n",
			   mods[i].module_name);
		else
		    printf("    module_name = %lu\n", mods[i].module_name);
	    }
	    printf("     iextdefsym = %lu\n", mods[i].iextdefsym);
	    printf("     nextdefsym = %lu\n", mods[i].nextdefsym);
	    printf("        irefsym = %lu\n", mods[i].irefsym);
	    printf("        nrefsym = %lu\n", mods[i].nrefsym);
	    printf("      ilocalsym = %lu\n", mods[i].ilocalsym);
	    printf("      nlocalsym = %lu\n", mods[i].nlocalsym);
	    printf("        iextrel = %lu\n", mods[i].iextrel);
	    printf("        nextrel = %lu\n", mods[i].nextrel);
	    printf("    iinit_iterm = %lu %lu\n",
		mods[i].iinit_iterm & 0xffff,
		(mods[i].iinit_iterm >> 16) & 0xffff);
	    printf("    ninit_nterm = %lu %lu\n",
		mods[i].ninit_nterm & 0xffff,
		(mods[i].ninit_nterm >> 16) & 0xffff);
	    printf("      objc_addr = 0x%x\n",
		(unsigned int)mods[i].objc_module_info_addr);
	    printf("      objc_size = %lu\n", mods[i].objc_module_info_size);
	}
}

/*
 * Print the reference table.
 */
void
print_refs(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct dylib_reference *refs,
unsigned long nrefs,
struct dylib_module *mods,
unsigned long nmods,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
   unsigned long i, j;

	printf("Reference table (%lu entries)\n", nrefs);
	for(i = 0; i < nmods; i++){
	    if(verbose){
		if(mods[i].module_name > strings_size)
		    printf("    module %lu (past end of string table)",
			   mods[i].module_name);
		else
		    printf("    module %s", strings + mods[i].module_name);
	    }
	    else{
		printf("    module %lu", mods[i].module_name);
	    }
	    if(mods[i].irefsym > nrefs){
		printf(" %lu entries, at index %lu (past end of reference "
		       "table)\n", mods[i].nrefsym, mods[i].irefsym);
		continue;
	    }
	    if(mods[i].irefsym + mods[i].nrefsym > nrefs)
		printf(" %lu entries (extends past the end of the reference "
		       "table), at index %lu\n", mods[i].nrefsym,
		       mods[i].irefsym);
	    else
		printf(" %lu entries, at index %lu\n",
		       mods[i].nrefsym, mods[i].irefsym);
	    for(j = mods[i].irefsym;
		j - mods[i].irefsym < mods[i].nrefsym && j < nrefs;
		j++){
		if(refs[j].isym > nsymbols)
		    printf("\t%u (past the end of the symbol table) ",
			   refs[j].isym);
		else{
		    if(verbose){
			if(refs[j].isym > nsymbols)
			    printf("\t%u (past the end of the symbol table) ",
				   refs[j].isym);
			else if((unsigned long)symbols[refs[j].isym].
				n_un.n_strx > strings_size)
			    printf("\t%u (string index past the end of the "
				   "string table) ", refs[j].isym);
			else
			    printf("\t%s ", strings +
				   symbols[refs[j].isym].n_un.n_strx);
		    }
		    else
			printf("\tisym %u ", refs[j].isym);
		}
		if(verbose){
		    switch(refs[j].flags){
		    case REFERENCE_FLAG_UNDEFINED_NON_LAZY:
			printf("undefined [non-lazy]\n");
			break;
		    case REFERENCE_FLAG_UNDEFINED_LAZY:
			printf("undefined [lazy]\n");
			break;
		    case REFERENCE_FLAG_DEFINED:
			printf("defined\n");
			break;
		    case REFERENCE_FLAG_PRIVATE_DEFINED:
			printf("private defined\n");
			break;
		    case REFERENCE_FLAG_PRIVATE_UNDEFINED_NON_LAZY:
			printf("private undefined [non-lazy]\n");
			break;
		    case REFERENCE_FLAG_PRIVATE_UNDEFINED_LAZY:
			printf("private undefined [lazy]\n");
			break;
		    default:
			printf("%u\n", (unsigned int)refs[j].flags);
			break;
		    }
		}
		else
		    printf("flags %u\n", (unsigned int)refs[j].flags);
	    }
	}
}

/*
 * Print the indirect symbol table.
 */
void
print_indirect_symbols(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
unsigned long *indirect_symbols,
unsigned long nindirect_symbols,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, k, left, size, nsects, n, count, stride, section_type;
    char *p;
    struct load_command *lc, l;
    struct segment_command sg;
    struct section *sections;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	/*
	 * Create an array of section structures in the host byte sex so it
	 * can be processed and indexed into directly.
	 */
	k = 0;
	nsects = 0;
	sections = NULL;
	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_SEGMENT:
		memset((char *)&sg, '\0', sizeof(struct segment_command));
		size = left < sizeof(struct segment_command) ?
		       left : sizeof(struct segment_command);
		memcpy((char *)&sg, (char *)lc, size);
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);

		nsects += sg.nsects;
		sections = reallocate(sections,
				      nsects * sizeof(struct section));
		memset((char *)(sections + (nsects - sg.nsects)), '\0',
		       sizeof(struct section) * sg.nsects);
		p = (char *)lc + sizeof(struct segment_command);
		for(j = 0 ; j < sg.nsects ; j++){
		    left = mh->sizeofcmds - (p - (char *)load_commands);
		    size = left < sizeof(struct section) ?
			   left : sizeof(struct section);
		    memcpy((char *)(sections + k), p, size);
		    if(swapped)
			swap_section(sections + k, 1, host_byte_sex);

		    if(p + sizeof(struct section) >
		       (char *)load_commands + mh->sizeofcmds)
			break;
		    p += size;
		    k++;
		}
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");

	for(i = 0 ; i < nsects ; i++){
	    section_type = sections[i].flags & SECTION_TYPE;
	    if(section_type == S_SYMBOL_STUBS){
		stride = sections[i].reserved2;
		if(stride == 0){
		    printf("Can't print indirect symbols for (%.16s,%.16s) "
			   "(size of stubs in reserved2 field is zero)\n",
			   sections[i].segname, sections[i].sectname);
		    continue;
		}
	    }
	    else if(section_type == S_LAZY_SYMBOL_POINTERS ||
	            section_type == S_NON_LAZY_SYMBOL_POINTERS)
		stride = sizeof(unsigned long);
	    else
		continue;
	
	    count = sections[i].size / stride;
	    printf("Indirect symbols for (%.16s,%.16s) %lu entries",
		   sections[i].segname, sections[i].sectname,
		   count);

	    n = sections[i].reserved1;
	    if(n > nindirect_symbols)
		printf(" (entries start past the end of the indirect symbol "
		       "table) (reserved1 field greater than the table size)");
	    else if(n + count > nindirect_symbols)
		printf(" (entries extends past the end of the indirect symbol "
		       "table)");
	    printf("\naddress    index");
	    if(verbose)
		printf(" name\n");
	    else
		printf("\n");

	    for(j = 0 ; j < count && n + j < nindirect_symbols; j++){
		printf("0x%08x ",(unsigned int)(sections[i].addr + j * stride));
		if(indirect_symbols[j + n] == INDIRECT_SYMBOL_LOCAL){
		    printf("LOCAL\n");
		    continue;
		}
		if(indirect_symbols[j + n] ==
		   (INDIRECT_SYMBOL_LOCAL | INDIRECT_SYMBOL_ABS)){
		    printf("LOCAL ABSOLUTE\n");
		    continue;
		}
		printf("%5lu ", indirect_symbols[j + n]);
		if(verbose){
		    if(symbols == NULL || strings == NULL ||
		       indirect_symbols[j + n] >= nsymbols ||
		       (unsigned long)symbols[indirect_symbols[j+n]].
				n_un.n_strx >= strings_size)
			printf("?\n");
		    else
			printf("%s\n", strings +
			   symbols[indirect_symbols[j+n]].n_un.n_strx);
		}
		else
		    printf("\n");
	    }
	    n += count;
	}
}

void print_hints(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex load_commands_byte_sex,
char *object_addr,
unsigned long object_size,
struct twolevel_hint *hints,
unsigned long nhints,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped, is_framework;
    unsigned long i, left, size, nlibs, dyst_cmd, lib_ord;
    char *p, **libs, *short_name, *has_suffix;
    struct load_command *lc, l;
    struct dysymtab_command dyst;
    struct dylib_command dl;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	/*
	 * If verbose is TRUE create an array of load dylibs names so it
	 * indexed into directly.
	 */
	nlibs = 0;
	libs = NULL;
	dyst_cmd = ULONG_MAX;
	lc = load_commands;
	for(i = 0 ; verbose == TRUE && i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		printf("load command %lu size not a multiple of "
		       "sizeof(long)\n", i);
	    if((char *)lc + l.cmdsize >
	       (char *)load_commands + mh->sizeofcmds)
		printf("load command %lu extends past end of load "
		       "commands\n", i);
	    left = mh->sizeofcmds - ((char *)lc - (char *)load_commands);

	    switch(l.cmd){
	    case LC_DYSYMTAB:
		if(dyst_cmd != ULONG_MAX){
		    printf("more than one LC_DYSYMTAB command (using command "
			   "%lu)\n", dyst_cmd);
		    break;
		}
		memset((char *)&dyst, '\0', sizeof(struct dysymtab_command));
		size = left < sizeof(struct dysymtab_command) ?
		       left : sizeof(struct dysymtab_command);
		memcpy((char *)&dyst, (char *)lc, size);
		if(swapped)
		    swap_dysymtab_command(&dyst, host_byte_sex);
		dyst_cmd = i;
		break;

	    case LC_LOAD_DYLIB:
	    case LC_LOAD_WEAK_DYLIB:
		memset((char *)&dl, '\0', sizeof(struct dylib_command));
		size = left < sizeof(struct dylib_command) ?
		       left : sizeof(struct dylib_command);
		memcpy((char *)&dl, (char *)lc, size);
		if(swapped)
		    swap_dylib_command(&dl, host_byte_sex);
		if(dl.dylib.name.offset < dl.cmdsize){
		    p = (char *)lc + dl.dylib.name.offset;
		    short_name = guess_short_name(p, &is_framework,
						  &has_suffix);
		    if(short_name != NULL)
			p = short_name;
		}
		else{
		    p = "bad dylib.name.offset";
		}
		libs = reallocate(libs, (nlibs+1) * sizeof(char *));
		libs[nlibs] = p;
		nlibs++;
		break;
	    }
	    if(l.cmdsize == 0){
		printf("load command %lu size zero (can't advance to other "
		       "load commands)\n", i);
		break;
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		break;
	}
	if((char *)load_commands + mh->sizeofcmds != (char *)lc)
	    printf("Inconsistent mh_sizeofcmds\n");

	printf("Two-level namespace hints table (%lu hints)\n", nhints);
	printf("index  isub  itoc\n");
	for(i = 0; i < nhints; i++){
	    printf("%5lu %5d %5u", i, (int)hints[i].isub_image, hints[i].itoc);
	    if(verbose){
		if(dyst_cmd != ULONG_MAX &&
		   dyst.iundefsym + i < nsymbols){
		    if((unsigned long)symbols[dyst.iundefsym + i].n_un.n_strx >
		       strings_size)
			printf(" (bad string index in symbol %lu)\n",
			       dyst.iundefsym + i);
		    else{
			printf(" %s", strings +
				      symbols[dyst.iundefsym + i].n_un.n_strx);
			lib_ord = GET_LIBRARY_ORDINAL(symbols[
						  dyst.iundefsym + i].n_desc);
			if(lib_ord != SELF_LIBRARY_ORDINAL &&
			   lib_ord - 1 < nlibs)
			    printf(" (from %s)\n", libs[lib_ord - 1]);
			else
			    printf("\n");
		    }
		}
		else
		    printf("\n");
	    }
	    else
		printf("\n");
	}
}

void
print_cstring_section(
char *sect,
unsigned long sect_size,
unsigned long sect_addr,
enum bool print_addresses)
{
    unsigned long i;

	for(i = 0; i < sect_size ; i++){
	    if(print_addresses == TRUE)
		printf("%08x  ", (unsigned int)(sect_addr + i));

	    for( ; i < sect_size && sect[i] != '\0'; i++)
		print_cstring_char(sect[i]);
	    if(i < sect_size && sect[i] == '\0')
		printf("\n");
	}
}

static
void
print_cstring_char(
char c)
{
	if(isprint(c)){
	    if(c == '\\')	/* backslash */
		printf("\\\\");
	    else		/* all other printable characters */
		printf("%c", c);
	}
	else{
	    switch(c){
	    case '\n':		/* newline */
		printf("\\n");
		break;
	    case '\t':		/* tab */
		printf("\\t");
		break;
	    case '\v':		/* vertical tab */
		printf("\\v");
		break;
	    case '\b':		/* backspace */
		printf("\\b");
		break;
	    case '\r':		/* carriage return */
		printf("\\r");
		break;
	    case '\f':		/* formfeed */
		printf("\\f");
		break;
	    case '\a':		/* audiable alert */
		printf("\\a");
		break;
	    default:
		printf("\\%03o", (unsigned int)c);
	    }
	}
}

void
print_literal4_section(
char *sect,
unsigned long sect_size,
unsigned long sect_addr,
enum byte_sex literal_byte_sex,
enum bool print_addresses)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, l;
    float f;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != literal_byte_sex;

	for(i = 0; i < sect_size ; i += sizeof(float)){
	    if(print_addresses == TRUE)
		printf("%08x  ", (unsigned int)(sect_addr + i));
	    memcpy((char *)&f, sect + i, sizeof(float));
	    memcpy((char *)&l, sect + i, sizeof(unsigned long));
	    if(swapped){
		f = SWAP_FLOAT(f);
		l = SWAP_LONG(l);
	    }
	    print_literal4(l, f);
	}
}

static
void
print_literal4(
unsigned long l,
float f)
{
	printf("0x%08x", (unsigned int)l);
	if((l & 0x7f800000) != 0x7f800000){
	    printf(" (%.16e)\n", f);
	}
	else{
	    if(l == 0x7f800000)
		printf(" (+Infinity)\n");
	    else if(l == 0xff800000)
		printf(" (-Infinity)\n");
	    else if((l & 0x00400000) == 0x00400000)
		printf(" (non-signaling Not-a-Number)\n");
	    else
		printf(" (signaling Not-a-Number)\n");
	}
}

void
print_literal8_section(
char *sect,
unsigned long sect_size,
unsigned long sect_addr,
enum byte_sex literal_byte_sex,
enum bool print_addresses)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, l0, l1;
    double d;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != literal_byte_sex;

	for(i = 0; i < sect_size ; i += sizeof(double)){
	    if(print_addresses == TRUE)
		printf("%08x  ", (unsigned int)(sect_addr + i));
	    memcpy((char *)&d, sect + i, sizeof(double));
	    memcpy((char *)&l0, sect + i, sizeof(unsigned long));
	    memcpy((char *)&l1, sect + i + sizeof(unsigned long),
		   sizeof(unsigned long));
	    if(swapped){
		d = SWAP_DOUBLE(d);
		l0 = SWAP_LONG(l0);
		l1 = SWAP_LONG(l1);
	    }
	    print_literal8(l0, l1, d);
	}
}

static
void
print_literal8(
unsigned long l0,
unsigned long l1,
double d)
{
	printf("0x%08x 0x%08x", (unsigned int)l0, (unsigned int)l1);
	/* l0 is the high word, so this is equivalent to if(isfinite(d)) */
	if((l0 & 0x7ff00000) != 0x7ff00000)
	    printf(" (%.16e)\n", d);
	else{
	    if(l0 == 0x7ff00000 && l1 == 0)
		printf(" (+Infinity)\n");
	    else if(l0 == 0xfff00000 && l1 == 0)
		printf(" (-Infinity)\n");
	    else if((l0 & 0x00080000) == 0x00080000)
		printf(" (non-signaling Not-a-Number)\n");
	    else
		printf(" (signaling Not-a-Number)\n");
	}
}

void
print_literal_pointer_section(
struct mach_header *mh,
struct load_command *load_commands,
enum byte_sex object_byte_sex,
char *object_addr,
unsigned long object_size,
char *sect,
unsigned long sect_size,
unsigned long sect_addr,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
struct relocation_info *relocs,
unsigned long nrelocs,
enum bool print_addresses)
{
    enum byte_sex host_byte_sex;
    enum bool swapped, found;
    unsigned long i, j, k, l, l0, l1, left, size;
    struct load_command lcmd, *lc;
    struct segment_command sg;
    struct section s;
    struct literal_section {
	struct section s;
	char *contents;
	unsigned long size;
    } *literal_sections;
    char *p;
    unsigned long nliteral_sections;
    float f;
    double d;
    struct relocation_info *reloc;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	literal_sections = NULL;
	nliteral_sections = 0;

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

		    if(s.flags == S_CSTRING_LITERALS ||
		       s.flags == S_4BYTE_LITERALS ||
		       s.flags == S_8BYTE_LITERALS){
			literal_sections = reallocate(literal_sections,
						sizeof(struct literal_section) *
						(nliteral_sections + 1));
			literal_sections[nliteral_sections].s = s;
			literal_sections[nliteral_sections].contents = 
							object_addr + s.offset;
			if(s.offset > object_size){
			    printf("section contents of: (%.16s,%.16s) is past "
				   "end of file\n", s.segname, s.sectname);
			    literal_sections[nliteral_sections].size =  0;
			}
			else if(s.offset + s.size > object_size){
			    printf("part of section contents of: (%.16s,%.16s) "
				   "is past end of file\n",
				   s.segname, s.sectname);
			    literal_sections[nliteral_sections].size =
				object_size - s.offset;
			}
			else
			    literal_sections[nliteral_sections].size = s.size;
			nliteral_sections++;
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

	/* loop through the literal pointer section and print the pointers */
	for(i = 0; i < sect_size ; i += sizeof(long)){
	    if(print_addresses == TRUE)
		printf("%08x  ", (unsigned int)(sect_addr + i));
	    l = (long)*((long *)(sect + i));
	    memcpy((char *)&l, sect + i, sizeof(unsigned long));
	    if(swapped)
		l = SWAP_LONG(l);
	    /*
	     * If there is an external relocation entry for this pointer then
	     * print the symbol and any offset.
	     */
	    reloc = bsearch(&i, relocs, nrelocs, sizeof(struct relocation_info),
			    (int (*)(const void *, const void *))rel_bsearch);
	    if(reloc != NULL && (reloc->r_address & R_SCATTERED) == 0 &&
	       reloc->r_extern == 1){
		printf("external relocation entry for symbol:");
		if(reloc->r_symbolnum < nsymbols &&
		   symbols[reloc->r_symbolnum].n_un.n_strx > 0 &&
		   (unsigned long)symbols[reloc->r_symbolnum].n_un.n_strx <
			strings_size){
		    if(l != 0)
			printf("%s+0x%x\n", strings +
				symbols[reloc->r_symbolnum].n_un.n_strx,
				(unsigned int)l);
		    else
			printf("%s\n", strings +
				symbols[reloc->r_symbolnum].n_un.n_strx);
		}
		else{
		    printf("bad relocation entry\n");
		}
		continue;
	    }
	    found = FALSE;
	    for(j = 0; j < nliteral_sections; j++){
		if(l >= literal_sections[j].s.addr &&
		   l < literal_sections[j].s.addr +
		       literal_sections[j].size){
		    printf("%.16s:%.16s:", literal_sections[j].s.segname,
			   literal_sections[j].s.sectname);
		    switch(literal_sections[j].s.flags){
		    case S_CSTRING_LITERALS:
			for(k = l - literal_sections[j].s.addr;
			    k < literal_sections[j].size &&
					literal_sections[j].contents[k] != '\0';
			    k++)
			    print_cstring_char(literal_sections[j].contents[k]);
			printf("\n");
			break;
		    case S_4BYTE_LITERALS:
			memcpy((char *)&f,
			       (char *)(literal_sections[j].contents +
					l - literal_sections[j].s.addr),
				sizeof(float));
			memcpy((char *)&l0,
			       (char *)(literal_sections[j].contents +
					l - literal_sections[j].s.addr),
				sizeof(unsigned long));
			if(swapped){
			    d = SWAP_DOUBLE(d);
			    l0 = SWAP_LONG(l0);
			}
			print_literal4(l0, f);
			break;
		    case S_8BYTE_LITERALS:
			memcpy((char *)&d,
			       (char *)(literal_sections[j].contents +
					l - literal_sections[j].s.addr),
				sizeof(double));
			memcpy((char *)&l0,
			       (char *)(literal_sections[j].contents +
					l - literal_sections[j].s.addr),
				sizeof(unsigned long));
			memcpy((char *)&l1,
			       (char *)(literal_sections[j].contents +
					l - literal_sections[j].s.addr +
					sizeof(unsigned long)),
			       sizeof(unsigned long));
			if(swapped){
			    d = SWAP_DOUBLE(d);
			    l0 = SWAP_LONG(l0);
			    l1 = SWAP_LONG(l1);
			}
			print_literal8(l0, l1, d);
			break;
		    }
		    found = TRUE;
		    break;
		}
	    }
	    if(found == FALSE)
		printf("0x%x (not in a literal section)\n", (unsigned int)l);
	}

	if(literal_sections != NULL)
	    free(literal_sections);
}

/*
 * Function for bsearch for searching relocation entries.
 */
static
int
rel_bsearch(
unsigned long *address,
struct relocation_info *rel)
{
    struct scattered_relocation_info *srel;
    unsigned long r_address;

	if((rel->r_address & R_SCATTERED) != 0){
	    srel = (struct scattered_relocation_info *)rel;
	    r_address = srel->r_address;
	}
	else
	    r_address = rel->r_address;

	if(*address == r_address)
	    return(0);
	if(*address < r_address)
	    return(-1);
	else
	    return(1);
}

/*
 * Print the shared library initialization table.
 */
void
print_shlib_init(
enum byte_sex object_byte_sex,
char *sect,
unsigned long sect_size,
unsigned long sect_addr,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols,
struct nlist *symbols,
unsigned long nsymbols,
char *strings,
unsigned long strings_size,
struct relocation_info *relocs,
unsigned long nrelocs,
enum bool verbose)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i;
    struct shlib_init {
	long value;		/* the value to be stored at the address */
	long address;	/* the address to store the value */
    } shlib_init;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	for(i = 0; i < sect_size; i += sizeof(struct shlib_init)){
	    memcpy((char *)&shlib_init, sect + i, sizeof(struct shlib_init));
	    if(swapped){
		shlib_init.value = SWAP_LONG(shlib_init.value);
		shlib_init.address = SWAP_LONG(shlib_init.address);
	    }
	    printf("\tvalue   0x%08x ", (unsigned int)shlib_init.value);
	    (void)print_symbol(shlib_init.value, sect_addr + i, 0, relocs,
			       nrelocs, symbols, nsymbols, sorted_symbols,
			       nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	    printf("\taddress 0x%08x ", (unsigned int)shlib_init.address);
	    (void)print_symbol(shlib_init.address, sect_addr+i+sizeof(long), 0,
			       relocs,nrelocs,symbols, nsymbols, sorted_symbols,
			       nsorted_symbols, strings, strings_size, verbose);
	    printf("\n");
	}
}

/*
 * Print_symbol prints a symbol name for the addr if a symbol exist with the
 * same address.  Nothing else is printed, no whitespace, no newline.  If it
 * prints something then it returns TRUE, else it returns FALSE.
 */
enum bool
print_symbol(
unsigned long value,
unsigned long r_address,
unsigned long dot_value,
struct relocation_info *relocs,
unsigned long nrelocs,
struct nlist *symbols,
unsigned long nsymbols,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols,
char *strings,
unsigned long strings_size,
enum bool verbose)
{
    unsigned long i, offset;
    struct scattered_relocation_info *sreloc, *pair;
    unsigned int r_symbolnum;
    unsigned long n_strx;
    const char *name, *add, *sub;

	if(verbose == FALSE)
	    return(FALSE);

	for(i = 0; i < nrelocs; i++){
	    if(((relocs[i].r_address) & R_SCATTERED) != 0){
		sreloc = (struct scattered_relocation_info *)(relocs + i);
		if(sreloc->r_type == GENERIC_RELOC_PAIR){
		    fprintf(stderr, "Stray GENERIC_RELOC_PAIR relocation entry "
			    "%lu\n", i);
		    continue;
		}
		if(sreloc->r_type == GENERIC_RELOC_VANILLA){
		    if(sreloc->r_address == r_address){
			name = guess_symbol(sreloc->r_value, sorted_symbols,
					    nsorted_symbols, verbose);
			offset = value - sreloc->r_value;
			if(name != NULL){
			    printf("%s+0x%x", name, (unsigned int)offset);
			    return(TRUE);
			}
		    }
		    continue;
		}
		if(sreloc->r_type != GENERIC_RELOC_SECTDIFF){
		    fprintf(stderr, "Unknown relocation r_type for entry "
			    "%lu\n", i);
		    continue;
		}
		if(i + 1 < nrelocs){
		    pair = (struct scattered_relocation_info *)(relocs + i + 1);
		    if(pair->r_scattered == 0 ||
		       pair->r_type != GENERIC_RELOC_PAIR){
			fprintf(stderr, "No GENERIC_RELOC_PAIR relocation "
				"entry after entry %lu\n", i);
			continue;
		    }
		}
		else{
		    fprintf(stderr, "No GENERIC_RELOC_PAIR relocation entry "
			    "after entry %lu\n", i);
		    continue;
		}
		i++; /* skip the pair reloc */

		if(sreloc->r_address == r_address){
		    add = guess_symbol(sreloc->r_value, sorted_symbols,
				       nsorted_symbols, verbose);
		    sub = guess_symbol(pair->r_value, sorted_symbols,
				       nsorted_symbols, verbose);
		    offset = value - (sreloc->r_value - pair->r_value);
		    if(add != NULL)
			printf("%s", add);
		    else
			printf("0x%x", (unsigned int)sreloc->r_value);
		    if(sub != NULL)
			printf("-%s", sub);
		    else{
			if((unsigned long)pair->r_value == dot_value)
			    printf("-.");
			else
			    printf("-0x%x", (unsigned int)pair->r_value);
		    }
		    if(offset != 0)
			printf("+0x%x", (unsigned int)offset);
		    return(TRUE);
		}
	    }
	    else{
		if((unsigned long)relocs[i].r_address == r_address){
		    r_symbolnum = relocs[i].r_symbolnum;
		    if(relocs[i].r_extern){
		        if(r_symbolnum >= nsymbols)
			    return(FALSE);
			n_strx = symbols[r_symbolnum].n_un.n_strx;
			if(n_strx <= 0 || n_strx >= strings_size)
			    return(FALSE);
			if(value != 0)
			    printf("%s+0x%x", strings + n_strx,
				   (unsigned int)value);
			else
			    printf("%s", strings + n_strx);
			return(TRUE);
		    }
		    break;
		}
	    }
	}

	name = guess_symbol(value, sorted_symbols, nsorted_symbols, verbose);
	if(name != NULL){
	    printf("%s", name);
	    return(TRUE);
	}
	return(FALSE);
}

/*
 * guess_symbol() guesses the name for a symbol based on the specified value.
 * It returns the name of symbol or NULL.  It only returns a symbol name if
 *  a symbol with that exact value exists.
 */
const char *
guess_symbol(
const unsigned long value,	/* the value of this symbol (in) */
const struct nlist *sorted_symbols,
const unsigned long nsorted_symbols,
const enum bool verbose)
{
    long high, low, mid;

	if(verbose == FALSE)
	    return(NULL);

	low = 0;
	high = nsorted_symbols - 1;
	mid = (high - low) / 2;
	while(high >= low){
	    if(sorted_symbols[mid].n_value == value){
		return(sorted_symbols[mid].n_un.n_name);
	    }
	    if(sorted_symbols[mid].n_value > value){
		high = mid - 1;
		mid = (high + low) / 2;
	    }
	    else{
		low = mid + 1;
		mid = (high + low) / 2;
	    }
	}
	return(NULL);
}

/*
 * guess_indirect_symbol() returns the name of the indirect symbol for the
 * value passed in or NULL.
 */
const char *
guess_indirect_symbol(
const unsigned long value,	/* the value of this symbol (in) */
const struct mach_header *mh,
const struct load_command *load_commands,
const enum byte_sex load_commands_byte_sex,
const unsigned long *indirect_symbols,
const unsigned long nindirect_symbols,
const struct nlist *symbols,
const unsigned long nsymbols,
const char *strings,
const unsigned long strings_size)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, section_type, index, stride;
    const struct load_command *lc;
    struct load_command l;
    struct segment_command sg;
    struct section s;
    char *p;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != load_commands_byte_sex;

	lc = load_commands;
	for(i = 0 ; i < mh->ncmds; i++){
	    memcpy((char *)&l, (char *)lc, sizeof(struct load_command));
	    if(swapped)
		swap_load_command(&l, host_byte_sex);
	    if(l.cmdsize % sizeof(long) != 0)
		return(NULL);
	    if((char *)lc + l.cmdsize > (char *)load_commands + mh->sizeofcmds)
		return(NULL);
	    switch(l.cmd){
	    case LC_SEGMENT:
		memcpy((char *)&sg, (char *)lc, sizeof(struct segment_command));
		if(swapped)
		    swap_segment_command(&sg, host_byte_sex);
		p = (char *)lc + sizeof(struct segment_command);
		for(j = 0 ; j < sg.nsects ; j++){
		    memcpy((char *)&s, p, sizeof(struct section));
		    p += sizeof(struct section);
		    if(swapped)
			swap_section(&s, 1, host_byte_sex);
		    section_type = s.flags & SECTION_TYPE;
		    if((section_type == S_NON_LAZY_SYMBOL_POINTERS ||
		        section_type == S_LAZY_SYMBOL_POINTERS ||
		        section_type == S_SYMBOL_STUBS) &&
		        value >= s.addr && value < s.addr + s.size){
			if(section_type == S_SYMBOL_STUBS)
			    stride = s.reserved2;
			else
			    stride = sizeof(unsigned long);
			index = s.reserved1 + (value - s.addr) / stride;
			if(index < nindirect_symbols &&
		    	   symbols != NULL && strings != NULL &&
		           indirect_symbols[index] < nsymbols &&
		           (unsigned long)symbols[indirect_symbols[index]].
				n_un.n_strx < strings_size)
			    return(strings +
				symbols[indirect_symbols[index]].n_un.n_strx);
			else
			    return(NULL);
		    }
		}
		break;
	    }
	    if(l.cmdsize == 0){
		return(NULL);
	    }
	    lc = (struct load_command *)((char *)lc + l.cmdsize);
	    if((char *)lc > (char *)load_commands + mh->sizeofcmds)
		return(NULL);
	}
	return(NULL);
}

void
print_sect(
cpu_type_t cputype,
enum byte_sex object_byte_sex,
char *sect,
unsigned long size,
unsigned long addr)
{
    enum byte_sex host_byte_sex;
    enum bool swapped;
    unsigned long i, j, long_word;
    unsigned short short_word;
    unsigned char byte_word;

	host_byte_sex = get_host_byte_sex();
	swapped = host_byte_sex != object_byte_sex;

	if(cputype == CPU_TYPE_I386){
	    for(i = 0 ; i < size ; i += j , addr += j){
		printf("%08x ", (unsigned int)addr);
		for(j = 0;
		    j < 16 * sizeof(char) && i + j < size;
		    j += sizeof(char)){
		    byte_word = *(sect + i + j);
		    printf("%02x ", (unsigned int)byte_word);
		}
		printf("\n");
	    }
	}
	else if(cputype == CPU_TYPE_MC680x0){
	    for(i = 0 ; i < size ; i += j , addr += j){
		printf("%08x ", (unsigned int)addr);
		for(j = 0;
		    j < 8 * sizeof(short) && i + j < size;
		    j += sizeof(short)){
		    memcpy(&short_word, sect + i + j, sizeof(short));
		    if(swapped)
			short_word = SWAP_SHORT(short_word);
		    printf("%04x ", (unsigned int)short_word);
		}
		printf("\n");
	    }
	}
	else{
	    for(i = 0 ; i < size ; i += j , addr += j){
		printf("%08x ", (unsigned int)addr);
		for(j = 0;
		    j < 4 * sizeof(long) && i + j < size;
		    j += sizeof(long)){
		    memcpy(&long_word, sect + i + j, sizeof(long));
		    if(swapped)
			long_word = SWAP_LONG(long_word);
		    printf("%08x ", (unsigned int)long_word);
		}
		printf("\n");
	    }
	}
}

/*
 * Print_label prints a symbol name for the addr if a symbol exist with the
 * same address in label form, namely:.
 *
 * <symbol name>:\n
 *
 * The colon and the newline are printed if colon_and_newline is TRUE.
 */
void
print_label(
unsigned long addr,
enum bool colon_and_newline,
struct nlist *sorted_symbols,
unsigned long nsorted_symbols)
{
    long high, low, mid;

	low = 0;
	high = nsorted_symbols - 1;
	mid = (high - low) / 2;
	while(high >= low){
	    if(sorted_symbols[mid].n_value == addr){
		printf("%s", sorted_symbols[mid].n_un.n_name);
		if(colon_and_newline == TRUE)
		    printf(":\n");
		return;
	    }
	    if(sorted_symbols[mid].n_value > addr){
		high = mid - 1;
		mid = (high + low) / 2;
	    }
	    else{
		low = mid + 1;
		mid = (high + low) / 2;
	    }
	}
}
