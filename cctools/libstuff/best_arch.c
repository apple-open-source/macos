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
#include <limits.h>
#include <mach-o/fat.h>
#include <stuff/best_arch.h>

/*
 * cpusubtype_findbestarch() is passed a cputype and cpusubtype and a set of
 * fat_arch structs and selects the best one that matches (if any) and returns
 * a pointer to that fat_arch struct (or NULL).  The fat_arch structs must be
 * in the host byte sex and correct such that the fat_archs really points to
 * enough memory for nfat_arch structs.  It is possible that this routine could
 * fail if new cputypes or cpusubtypes are added and an old version of this
 * routine is used.  But if there is an exact match between the cputype and
 * cpusubtype and one of the fat_arch structs this routine will always succeed.
 */
__private_extern__
struct fat_arch *
cpusubtype_findbestarch(
cpu_type_t cputype,
cpu_subtype_t cpusubtype,
struct fat_arch *fat_archs,
unsigned long nfat_archs)
{
    unsigned long i;
    long lowest_family, lowest_model, lowest_index;

	/*
	 * Look for the first exact match.
	 */
	for(i = 0; i < nfat_archs; i++){
	    if(fat_archs[i].cputype == cputype &&
	       fat_archs[i].cpusubtype == cpusubtype)
		return(fat_archs + i);
	}

	/*
	 * An exact match was not found so find the next best match which is
	 * cputype dependent.
	 */
	switch(cputype){
	case CPU_TYPE_I386:
	    switch(cpusubtype){
	    default:
		/*
		 * Intel cpusubtypes after the pentium (same as 586) are handled
		 * such that they require an exact match or they can use the
		 * pentium.  If that is not found call into the loop for the
		 * earilier subtypes.
		 */
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_PENT)
			return(fat_archs + i);
		}
	    case CPU_SUBTYPE_PENT:
	    case CPU_SUBTYPE_486SX:
		/*
		 * Since an exact match as not found look for the i486 else
		 * break into the loop to look for the i386_ALL.
		 */
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_486)
			return(fat_archs + i);
		}
		break;
	    case CPU_SUBTYPE_I386_ALL:
	    /* case CPU_SUBTYPE_I386: same as above */
	    case CPU_SUBTYPE_486:
		break;
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_I386_ALL)
		    return(fat_archs + i);
	    }

	    /*
	     * A match failed, promote as little as possible.
	     */
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_486)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_486SX)
		    return(fat_archs + i);
	    }
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_586)
		    return(fat_archs + i);
	    }
	    /*
	     * Now look for the lowest family and in that the lowest model.
	     */
	    lowest_family = CPU_SUBTYPE_INTEL_FAMILY_MAX + 1;
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(CPU_SUBTYPE_INTEL_FAMILY(fat_archs[i].cpusubtype) <
		   lowest_family)
		    lowest_family = CPU_SUBTYPE_INTEL_FAMILY(
					fat_archs[i].cpusubtype);
	    }
	    /* if no intel cputypes found return NULL */
	    if(lowest_family == CPU_SUBTYPE_INTEL_FAMILY_MAX + 1)
		return(NULL);
	    lowest_model = ULONG_MAX;
	    lowest_index = -1;
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(CPU_SUBTYPE_INTEL_FAMILY(fat_archs[i].cpusubtype) ==
		   lowest_family){
		    if(CPU_SUBTYPE_INTEL_MODEL(fat_archs[i].cpusubtype) <
		       lowest_model){
		        lowest_model = CPU_SUBTYPE_INTEL_MODEL(
					fat_archs[i].cpusubtype);
			lowest_index = i;
		    }
		}
	    }
	    return(fat_archs + lowest_index);
	case CPU_TYPE_MC680x0:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC680x0_ALL)
		    return(fat_archs + i);
	    }
	    /*
	     * Try to promote if starting from CPU_SUBTYPE_MC680x0_ALL and
	     * favor the CPU_SUBTYPE_MC68040 over the CPU_SUBTYPE_MC68030_ONLY.
	     */
	    if(cpusubtype == CPU_SUBTYPE_MC680x0_ALL){
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC68040)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC68030_ONLY)
			return(fat_archs + i);
		}
	    }
	    break;
	case CPU_TYPE_POWERPC:
	    /*
	     * An exact match as not found.  So for all the PowerPC subtypes
	     * pick the subtype from the following order starting from a subtype
	     * that will work (contains 64-bit instructions or altivec if
	     * needed):
	     *	970, 7450, 7400, 750, 604e, 604, 603ev, 603e, 603, ALL
	     * Note the 601 is NOT in the list above.  It is only picked via
	     * an exact match.  For an unknown subtype pick only the ALL type if
	     * it exists.
	     */
	    switch(cpusubtype){
	    case CPU_SUBTYPE_POWERPC_ALL:
		/*
		 * The CPU_SUBTYPE_POWERPC_ALL is only used by the development
		 * environment tools when building a generic ALL type binary.
		 * In the case of a non-exact match we pick the most current
		 * processor.
		 */
	    case CPU_SUBTYPE_POWERPC_970:
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_970)
			return(fat_archs + i);
		}
	    case CPU_SUBTYPE_POWERPC_7450:
	    case CPU_SUBTYPE_POWERPC_7400:
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_7450)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_7400)
			return(fat_archs + i);
		}
	    case CPU_SUBTYPE_POWERPC_750:
	    case CPU_SUBTYPE_POWERPC_604e:
	    case CPU_SUBTYPE_POWERPC_604:
	    case CPU_SUBTYPE_POWERPC_603ev:
	    case CPU_SUBTYPE_POWERPC_603e:
	    case CPU_SUBTYPE_POWERPC_603:
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_750)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_604e)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_604)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_603ev)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_603e)
			return(fat_archs + i);
		}
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_603)
			return(fat_archs + i);
		}
	    default:
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_ALL)
			return(fat_archs + i);
		}
	    }
	    break;
	case CPU_TYPE_VEO:
	    /*
	     * An exact match was not found.  So for the VEO subtypes if VEO1
	     * is wanted then VEO2 can be used.  But if VEO2 is wanted only
	     * VEO2 can be used.  Any unknown values don't match.
	     */
	    switch(cpusubtype){
	    case CPU_SUBTYPE_VEO_1:
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_VEO_2)
			return(fat_archs + i);
		}
	    }
	    break;
	case CPU_TYPE_MC88000:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_MC88000_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_I860:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_I860_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_HPPA:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_HPPA_ALL)
		    return(fat_archs + i);
	    }
	    break;
	case CPU_TYPE_SPARC:
	    for(i = 0; i < nfat_archs; i++){
		if(fat_archs[i].cputype != cputype)
		    continue;
		if(fat_archs[i].cpusubtype == CPU_SUBTYPE_SPARC_ALL)
		    return(fat_archs + i);
	    }
	    break;
	default:
	    return(NULL);
	}
	return(NULL);
}

/*
 * cpusubtype_combine() returns the resulting cpusubtype when combining two
 * differnet cpusubtypes for the specified cputype.  If the two cpusubtypes
 * can't be combined (the specific subtypes are mutually exclusive) -1 is
 * returned indicating it is an error to combine them.  This can also fail and
 * return -1 if new cputypes or cpusubtypes are added and an old version of
 * this routine is used.  But if the cpusubtypes are the same they can always
 * be combined and this routine will return the cpusubtype pass in.
 */
__private_extern__
cpu_subtype_t
cpusubtype_combine(
cpu_type_t cputype,
cpu_subtype_t cpusubtype1,
cpu_subtype_t cpusubtype2)
{
	if(cpusubtype1 == cpusubtype2)
	    return(cpusubtype1);

	switch(cputype){
	case CPU_TYPE_I386:
	    /*
	     * For compatiblity with pre-Rhapsody CR1 systems the subtypes that
	     * previously existed are handled the same as before.  So going in
	     * we know we don't have an exact match because of the test done
	     * at the beginning of the routine.
	     */
	    if((cpusubtype1 == CPU_SUBTYPE_I386_ALL ||
	        /* cpusubtype1 == CPU_SUBTYPE_386 || same as above */
	        cpusubtype1 == CPU_SUBTYPE_486 ||
	        cpusubtype1 == CPU_SUBTYPE_486SX ||
	        cpusubtype1 == CPU_SUBTYPE_586) &&
	       (cpusubtype2 == CPU_SUBTYPE_I386_ALL ||
	        /* cpusubtype2 == CPU_SUBTYPE_386 || same as above */
	        cpusubtype2 == CPU_SUBTYPE_486 ||
	        cpusubtype2 == CPU_SUBTYPE_486SX ||
	        cpusubtype2 == CPU_SUBTYPE_586)){
		/* return the highest subtype of the two */
		if(cpusubtype1 == CPU_SUBTYPE_586 ||
		   cpusubtype2 == CPU_SUBTYPE_586)
		    return(CPU_SUBTYPE_586);
		if(cpusubtype1 == CPU_SUBTYPE_486SX ||
		   cpusubtype2 == CPU_SUBTYPE_486SX)
		    return(CPU_SUBTYPE_486SX);
		if(cpusubtype1 == CPU_SUBTYPE_486 ||
		   cpusubtype2 == CPU_SUBTYPE_486)
		    return(CPU_SUBTYPE_486);
		break; /* logically can't get here */
	    }
	    /*
	     * If either is a MODEL_ALL type select the one with the highest
	     * family type.  Note that only the old group of subtypes (ALL, 386,
	     * 486, 486SX, 586) can ever have MODEL_ALL (a model number of
	     * zero).  Since the cases both subtypes being in the old group are
	     * handled above this makes the the highest family type the one that
	     * is not MODEL_ALL.
	     */
	    if(CPU_SUBTYPE_INTEL_MODEL(cpusubtype1) ==
	       CPU_SUBTYPE_INTEL_MODEL_ALL)
		return(cpusubtype2);
	    if(CPU_SUBTYPE_INTEL_MODEL(cpusubtype2) ==
	       CPU_SUBTYPE_INTEL_MODEL_ALL)
		return(cpusubtype1);
	    /*
	     * For all other families and models they must match exactly to
	     * combine and since that test was done at the start of this routine
	     * we know these do not match and do not combine.
	     */
	    return((cpu_subtype_t)-1);
	    break; /* logically can't get here */

	case CPU_TYPE_MC680x0:
	    if(cpusubtype1 != CPU_SUBTYPE_MC680x0_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_MC68030_ONLY &&
	       cpusubtype1 != CPU_SUBTYPE_MC68040)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_MC680x0_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_MC68030_ONLY &&
	       cpusubtype2 != CPU_SUBTYPE_MC68040)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_MC68030_ONLY &&
	       cpusubtype2 == CPU_SUBTYPE_MC68040)
		return((cpu_subtype_t)-1);
	    if(cpusubtype1 == CPU_SUBTYPE_MC68040 &&
	       cpusubtype2 == CPU_SUBTYPE_MC68030_ONLY)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_MC68030_ONLY ||
	       cpusubtype2 == CPU_SUBTYPE_MC68030_ONLY)
		return(CPU_SUBTYPE_MC68030_ONLY);

	    if(cpusubtype1 == CPU_SUBTYPE_MC68040 ||
	       cpusubtype2 == CPU_SUBTYPE_MC68040)
		return(CPU_SUBTYPE_MC68040);
	    break; /* logically can't get here */

	case CPU_TYPE_POWERPC:
	    /*
	     * Combining with the ALL type becomes the other type. Combining
	     * anything with the 601 becomes 601.  All other non exact matches
	     * combine to the higher value subtype.
	     */
	    if(cpusubtype1 == CPU_SUBTYPE_POWERPC_ALL)
		return(cpusubtype2);
	    if(cpusubtype2 == CPU_SUBTYPE_POWERPC_ALL)
		return(cpusubtype1);

	    if(cpusubtype1 == CPU_SUBTYPE_POWERPC_601 ||
	       cpusubtype2 == CPU_SUBTYPE_POWERPC_601)
		return(CPU_SUBTYPE_POWERPC_601);

	    if(cpusubtype1 > cpusubtype2)
		return(cpusubtype1);
	    else
		return(cpusubtype2);
	    break; /* logically can't get here */

	case CPU_TYPE_VEO:
	    /*
	     * Combining VEO1 with VEO2 returns VEO1.  Any unknown values don't
	     * combine.
	     */
	    if(cpusubtype1 == CPU_SUBTYPE_VEO_1 &&
	       cpusubtype2 == CPU_SUBTYPE_VEO_2)
		return(CPU_SUBTYPE_VEO_1);
	    if(cpusubtype1 == CPU_SUBTYPE_VEO_2 &&
	       cpusubtype2 == CPU_SUBTYPE_VEO_1)
		return(CPU_SUBTYPE_VEO_1);
	    return((cpu_subtype_t)-1);
	    break; /* logically can't get here */

	case CPU_TYPE_MC88000:
	    if(cpusubtype1 != CPU_SUBTYPE_MC88000_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_MC88110)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_MC88000_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_MC88110)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_MC88110 ||
	       cpusubtype2 == CPU_SUBTYPE_MC88110)
		return(CPU_SUBTYPE_MC88110);

	    break; /* logically can't get here */

	case CPU_TYPE_I860:
	    if(cpusubtype1 != CPU_SUBTYPE_I860_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_I860_860)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_I860_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_I860_860)
		return((cpu_subtype_t)-1);

	    if(cpusubtype1 == CPU_SUBTYPE_I860_860 ||
	       cpusubtype2 == CPU_SUBTYPE_I860_860)
		return(CPU_SUBTYPE_I860_860);
	    break; /* logically can't get here */

	case CPU_TYPE_HPPA:
	    if(cpusubtype1 != CPU_SUBTYPE_HPPA_ALL &&
	       cpusubtype1 != CPU_SUBTYPE_HPPA_7100LC)
		return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_HPPA_ALL &&
	       cpusubtype2 != CPU_SUBTYPE_HPPA_7100LC)
		return((cpu_subtype_t)-1);

	    return(CPU_SUBTYPE_HPPA_7100LC);
	    break; /* logically can't get here */

	case CPU_TYPE_SPARC:
	    if(cpusubtype1 != CPU_SUBTYPE_SPARC_ALL)
			return((cpu_subtype_t)-1);
	    if(cpusubtype2 != CPU_SUBTYPE_SPARC_ALL)
			return((cpu_subtype_t)-1);
	    break; /* logically can't get here */

	default:
	    return((cpu_subtype_t)-1);
	}
	return((cpu_subtype_t)-1); /* logically can't get here */
}

/*
 * cpusubtype_execute() returns TRUE if the exec_cpusubtype can be used for
 * execution on the host_cpusubtype for the specified cputype (this routine is
 * used by the dynamic linker and should match the kernel's exec(2) code).  If
 * the exec_cpusubtype can't be run on the host_cpusubtype FALSE is returned
 * indicating it can't be run on that cpu.  This can also return FALSE and
 * if new cputypes or cpusubtypes are added and an old version of this routine
 * is used.  But if the cpusubtypes are the same they can always be executed
 * and this routine will return TRUE.  And ALL subtypes are always allowed to be
 * executed on unknown host_cpusubtype's.
 */
__private_extern__
enum bool
cpusubtype_execute(
cpu_type_t host_cputype,
cpu_subtype_t host_cpusubtype, /* can NOT be the ALL type */
cpu_subtype_t exec_cpusubtype) /* can be the ALL type */
{
	if(host_cpusubtype == exec_cpusubtype)
	    return(TRUE);

	switch(host_cputype){
	case CPU_TYPE_POWERPC:
	    /*
	     * The 970 has 64-bit and altivec instructions
	     * The 7450 and 7400 have altivec instructions
	     * The 601 has Power instructions (can only execute on a 601)
	     * other known subtypes can execute anywhere
	     * unknown hosts will only be allowed to execute the ALL subtype
	     */
	    switch(host_cpusubtype){
	    case CPU_SUBTYPE_POWERPC_970:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_POWERPC_970:
		case CPU_SUBTYPE_POWERPC_7450:
		case CPU_SUBTYPE_POWERPC_7400:
		case CPU_SUBTYPE_POWERPC_750:
		case CPU_SUBTYPE_POWERPC_620:
		case CPU_SUBTYPE_POWERPC_604e:
		case CPU_SUBTYPE_POWERPC_604:
		case CPU_SUBTYPE_POWERPC_603ev:
		case CPU_SUBTYPE_POWERPC_603e:
		case CPU_SUBTYPE_POWERPC_603:
		case CPU_SUBTYPE_POWERPC_602:
		case CPU_SUBTYPE_POWERPC_ALL:
		    return(TRUE);
		case CPU_SUBTYPE_POWERPC_601:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    case CPU_SUBTYPE_POWERPC_7450:
	    case CPU_SUBTYPE_POWERPC_7400:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_POWERPC_7450:
		case CPU_SUBTYPE_POWERPC_7400:
		case CPU_SUBTYPE_POWERPC_750:
		case CPU_SUBTYPE_POWERPC_620:
		case CPU_SUBTYPE_POWERPC_604e:
		case CPU_SUBTYPE_POWERPC_604:
		case CPU_SUBTYPE_POWERPC_603ev:
		case CPU_SUBTYPE_POWERPC_603e:
		case CPU_SUBTYPE_POWERPC_603:
		case CPU_SUBTYPE_POWERPC_602:
		case CPU_SUBTYPE_POWERPC_ALL:
		    return(TRUE);
		case CPU_SUBTYPE_POWERPC_970:
		case CPU_SUBTYPE_POWERPC_601:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    case CPU_SUBTYPE_POWERPC_750:
	    case CPU_SUBTYPE_POWERPC_620:
	    case CPU_SUBTYPE_POWERPC_604e:
	    case CPU_SUBTYPE_POWERPC_604:
	    case CPU_SUBTYPE_POWERPC_603ev:
	    case CPU_SUBTYPE_POWERPC_603e:
	    case CPU_SUBTYPE_POWERPC_603:
	    case CPU_SUBTYPE_POWERPC_602:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_POWERPC_750:
		case CPU_SUBTYPE_POWERPC_620:
		case CPU_SUBTYPE_POWERPC_604e:
		case CPU_SUBTYPE_POWERPC_604:
		case CPU_SUBTYPE_POWERPC_603ev:
		case CPU_SUBTYPE_POWERPC_603e:
		case CPU_SUBTYPE_POWERPC_603:
		case CPU_SUBTYPE_POWERPC_602:
		case CPU_SUBTYPE_POWERPC_ALL:
		    return(TRUE);
		case CPU_SUBTYPE_POWERPC_970:
		case CPU_SUBTYPE_POWERPC_7450:
		case CPU_SUBTYPE_POWERPC_7400:
		case CPU_SUBTYPE_POWERPC_601:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    case CPU_SUBTYPE_POWERPC_601:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_POWERPC_750:
		case CPU_SUBTYPE_POWERPC_620:
		case CPU_SUBTYPE_POWERPC_604e:
		case CPU_SUBTYPE_POWERPC_604:
		case CPU_SUBTYPE_POWERPC_603ev:
		case CPU_SUBTYPE_POWERPC_603e:
		case CPU_SUBTYPE_POWERPC_603:
		case CPU_SUBTYPE_POWERPC_602:
		case CPU_SUBTYPE_POWERPC_601:
		case CPU_SUBTYPE_POWERPC_ALL:
		    return(TRUE);
		case CPU_SUBTYPE_POWERPC_970:
		case CPU_SUBTYPE_POWERPC_7450:
		case CPU_SUBTYPE_POWERPC_7400:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    default: /* unknown host */
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_POWERPC_ALL:
		    return(TRUE);
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */
	    }
	    break; /* logically can't get here */

	case CPU_TYPE_I386:
	    /*
	     * On i386 if it is any known subtype it is allowed to execute on
	     * any host (even unknown hosts).  And the binary is expected to
	     * have code to avoid instuctions that will not execute on the
	     * host cpu.
	     */
	    switch(exec_cpusubtype){
	    case CPU_SUBTYPE_I386_ALL: /* same as CPU_SUBTYPE_386 */
	    case CPU_SUBTYPE_486:
	    case CPU_SUBTYPE_486SX:
	    case CPU_SUBTYPE_586: /* same as CPU_SUBTYPE_PENT */
	    case CPU_SUBTYPE_PENTPRO:
	    case CPU_SUBTYPE_PENTII_M3:
	    case CPU_SUBTYPE_PENTII_M5:
		return(TRUE);
	    default:
		return(FALSE);
	    }
	    break; /* logically can't get here */

	case CPU_TYPE_MC680x0:
	    switch(host_cpusubtype){
	    case CPU_SUBTYPE_MC68040:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_MC68040:
		case CPU_SUBTYPE_MC680x0_ALL: /* same as CPU_SUBTYPE_MC68030 */
		    return(TRUE);
		case CPU_SUBTYPE_MC68030_ONLY:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    case CPU_SUBTYPE_MC68030:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_MC680x0_ALL: /* same as CPU_SUBTYPE_MC68030 */
		case CPU_SUBTYPE_MC68030_ONLY:
		    return(TRUE);
		case CPU_SUBTYPE_MC68040:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */
		
	    default: /* unknown host */
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_MC680x0_ALL: /* same as CPU_SUBTYPE_MC68030 */
		    return(TRUE);
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */
	    }
	    break; /* logically can't get here */

	case CPU_TYPE_MC88000:
	    switch(host_cpusubtype){
	    case CPU_SUBTYPE_MC88110:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_MC88110:
		case CPU_SUBTYPE_MC88000_ALL:
		    return(TRUE);
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    default: /* unknown host */
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_MC88000_ALL:
		    return(TRUE);
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */
	    }
	    break; /* logically can't get here */
	    
	case CPU_TYPE_HPPA:
	    switch(host_cpusubtype){
	    case CPU_SUBTYPE_HPPA_7100LC:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_HPPA_ALL: /* same as CPU_SUBTYPE_HPPA_7100 */
		case CPU_SUBTYPE_HPPA_7100LC:
		    return(TRUE);
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    case CPU_SUBTYPE_HPPA_7100:
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_HPPA_ALL: /* same as CPU_SUBTYPE_HPPA_7100 */
		    return(TRUE);
		case CPU_SUBTYPE_HPPA_7100LC:
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */

	    default: /* unknown host */
		switch(exec_cpusubtype){
		case CPU_SUBTYPE_HPPA_ALL: /* same as CPU_SUBTYPE_HPPA_7100 */
		    return(TRUE);
		default:
		    return(FALSE);
		}
		break; /* logically can't get here */
	    }
	    break; /* logically can't get here */

	case CPU_TYPE_SPARC:
	    /*
	     * For Sparc we only have the ALL subtype defined.
	     */
	    switch(exec_cpusubtype){
	    case CPU_SUBTYPE_SPARC_ALL:
		return(TRUE);
	    default:
		return(FALSE);
	    }
	    break; /* logically can't get here */

	case CPU_TYPE_VEO:  /* not used with the dynamic linker */
	case CPU_TYPE_I860: /* not used with the dynamic linker */
	default:
	    return(FALSE);
	}
	return(FALSE); /* logically can't get here */
}
