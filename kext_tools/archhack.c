// Stolen shamelessly from cctools.

#include <limits.h>
#include <string.h>
#include <mach-o/fat.h>
#include <mach-o/arch.h>

#ifndef CPU_TYPE_ARM
#define CPU_TYPE_ARM            ((cpu_type_t) 12)
#define CPU_SUBTYPE_ARM_V4T		((cpu_subtype_t) 5)
#define CPU_SUBTYPE_ARM_V6		((cpu_subtype_t) 6)
#endif

#ifndef CPU_SUBTYPE_ARM_V5TEJ
#define CPU_SUBTYPE_ARM_V5TEJ           ((cpu_subtype_t) 7)
#endif
#ifndef CPU_SUBTYPE_ARM_V7
#define CPU_SUBTYPE_ARM_V7		((cpu_subtype_t) 9)
#endif

#ifndef CPU_TYPE_VEO
#define CPU_TYPE_VEO            ((cpu_type_t) 255)
#define CPU_SUBTYPE_VEO_1	((cpu_subtype_t) 1)
#define CPU_SUBTYPE_VEO_2	((cpu_subtype_t) 2)
#define CPU_SUBTYPE_VEO_ALL	CPU_SUBTYPE_VEO_2
#endif

/* The array of all currently know architecture flags (terminated with an entry
 * with all zeros).  Pointer to this returned with NXGetAllArchInfos().
 */
static const NXArchInfo ArchInfoTable[] = {
    /* architecture families */
    {"hppa",   CPU_TYPE_HPPA,	 CPU_SUBTYPE_HPPA_ALL,	   NX_BigEndian,
	 "HP-PA"},
    {"i386",   CPU_TYPE_I386,    CPU_SUBTYPE_I386_ALL,	   NX_LittleEndian,
	 "Intel 80x86"},
    {"i860",   CPU_TYPE_I860,    CPU_SUBTYPE_I860_ALL,     NX_BigEndian,
	 "Intel 860"},
    {"m68k",   CPU_TYPE_MC680x0, CPU_SUBTYPE_MC680x0_ALL,  NX_BigEndian,
	 "Motorola 68K"},
    {"m88k",   CPU_TYPE_MC88000, CPU_SUBTYPE_MC88000_ALL,  NX_BigEndian,
	 "Motorola 88K"},
    {"ppc",    CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_ALL,  NX_BigEndian,
	 "PowerPC"},
    {"ppc64",  CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_ALL,  NX_BigEndian,
	 "PowerPC 64-bit"},
    {"sparc",  CPU_TYPE_SPARC,   CPU_SUBTYPE_SPARC_ALL,	   NX_BigEndian,
	 "SPARC"},
    {"arm",    CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V4T,	   NX_LittleEndian,
	 "arm v4t"},
    {"armv4t", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V4T,	   NX_LittleEndian,
	 "arm v4t"},
    {"armv5", CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V5TEJ,	   NX_LittleEndian,
	 "arm v5"},
    {"armv6",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V6,	   NX_LittleEndian,
	 "arm v6"},
    {"armv7",  CPU_TYPE_ARM,     CPU_SUBTYPE_ARM_V7,	   NX_LittleEndian,
	 "arm v7"},
    {"any",    CPU_TYPE_ANY,     CPU_SUBTYPE_MULTIPLE,     NX_UnknownByteOrder,
	 "Architecture Independent"},
    {"veo",    CPU_TYPE_VEO,	 CPU_SUBTYPE_VEO_ALL,  	   NX_BigEndian,
	 "veo"},
    /* specific architecture implementations */
    {"hppa7100LC", CPU_TYPE_HPPA, CPU_SUBTYPE_HPPA_7100LC, NX_BigEndian,
	 "HP-PA 7100LC"},
    {"m68030", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68030_ONLY, NX_BigEndian,
	 "Motorola 68030"},
    {"m68040", CPU_TYPE_MC680x0, CPU_SUBTYPE_MC68040,	   NX_BigEndian,
	 "Motorola 68040"},
    {"i486",   CPU_TYPE_I386,    CPU_SUBTYPE_486,	   NX_LittleEndian,
	 "Intel 80486"},
    {"i486SX", CPU_TYPE_I386,    CPU_SUBTYPE_486SX,	   NX_LittleEndian,
	 "Intel 80486SX"},
    {"pentium",CPU_TYPE_I386,    CPU_SUBTYPE_PENT,	   NX_LittleEndian,
	 "Intel Pentium"}, /* same as 586 */
    {"i586",   CPU_TYPE_I386,    CPU_SUBTYPE_586,	   NX_LittleEndian,
	 "Intel 80586"},
    {"pentpro", CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO,	   NX_LittleEndian,
	 "Intel Pentium Pro"}, /* same as 686 */
    {"i686",    CPU_TYPE_I386, CPU_SUBTYPE_PENTPRO,	   NX_LittleEndian,
	 "Intel Pentium Pro"},
    {"pentIIm3", CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M3, NX_LittleEndian,
	 "Intel Pentium II Model 3" },
    {"pentIIm5", CPU_TYPE_I386, CPU_SUBTYPE_PENTII_M5, NX_LittleEndian,
	 "Intel Pentium II Model 5" },
    {"pentium4", CPU_TYPE_I386, CPU_SUBTYPE_PENTIUM_4, NX_LittleEndian,
	 "Intel Pentium 4" },
    {"ppc601", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_601,  NX_BigEndian,
	 "PowerPC 601" },
    {"ppc603", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603,  NX_BigEndian,
	 "PowerPC 603" },
    {"ppc603e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_603e, NX_BigEndian,
	 "PowerPC 603e" },
    {"ppc603ev",CPU_TYPE_POWERPC,CPU_SUBTYPE_POWERPC_603ev,NX_BigEndian,
	 "PowerPC 603ev" },
    {"ppc604", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604,  NX_BigEndian,
	 "PowerPC 604" },
    {"ppc604e",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_604e, NX_BigEndian,
	 "PowerPC 604e" },
    {"ppc750", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_750,  NX_BigEndian,
	 "PowerPC 750" },
    {"ppc7400",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7400,  NX_BigEndian,
	 "PowerPC 7400" },
    {"ppc7450",CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_7450,  NX_BigEndian,
	 "PowerPC 7450" },
    {"ppc970", CPU_TYPE_POWERPC, CPU_SUBTYPE_POWERPC_970,  NX_BigEndian,
	 "PowerPC 970" },
    {"ppc970-64",  CPU_TYPE_POWERPC64, CPU_SUBTYPE_POWERPC_970,  NX_BigEndian,
	 "PowerPC 970 64-bit"},
    {"little", CPU_TYPE_ANY,     CPU_SUBTYPE_LITTLE_ENDIAN, NX_LittleEndian,
         "Little Endian"},
    {"big",    CPU_TYPE_ANY,     CPU_SUBTYPE_BIG_ENDIAN,   NX_BigEndian,
         "Big Endian"},
    {"veo1",CPU_TYPE_VEO,	 CPU_SUBTYPE_VEO_1,	   NX_BigEndian,
	 "veo 1" },
    {"veo2",CPU_TYPE_VEO,	 CPU_SUBTYPE_VEO_2,	   NX_BigEndian,
	 "veo 2" },
    {NULL,     0,		  0,			   0,
	 NULL}
};

/*
 * NXGetArchInfoFromName() is passed an architecture name (like "m68k")
 * and returns the matching NXArchInfo struct, or NULL if none is found.
 */
const
NXArchInfo *
NXGetArchInfoFromName(
const char *name)
{
    const NXArchInfo *ai;

	for(ai = ArchInfoTable; ai->name != NULL; ai++)
	    if(strcmp(ai->name, name) == 0)
		return(ai);

	return(NULL);
}

/*
 * NXFindBestFatArch() is passed a cputype and cpusubtype and a set of
 * fat_arch structs and selects the best one that matches (if any) and returns
 * a pointer to that fat_arch struct (or NULL).  The fat_arch structs must be
 * in the host byte order and correct such that the fat_archs really points to
 * enough memory for nfat_arch structs.  It is possible that this routine could
 * fail if new cputypes or cpusubtypes are added and an old version of this
 * routine is used.  But if there is an exact match between the cputype and
 * cpusubtype and one of the fat_arch structs this routine will always succeed.
 */
struct fat_arch *
NXFindBestFatArch(
cpu_type_t cputype,
cpu_subtype_t cpusubtype,
struct fat_arch *fat_archs,
uint32_t nfat_archs)
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
	    lowest_model = LONG_MAX;
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
	case CPU_TYPE_POWERPC64:
	    /*
	     * An exact match as not found.  So for all the PowerPC64 subtypes
	     * pick the subtype from the following order starting from a subtype
	     * that will work (contains 64-bit instructions or altivec if
	     * needed):
	     *	970 (currently only the one 64-bit subtype)
	     * For an unknown subtype pick only the ALL type if it exists.
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
	    default:
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype == CPU_SUBTYPE_POWERPC_ALL)
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
	case CPU_TYPE_ARM:
	    {
		/* 
		 * ARM is straightforward, since each architecture is backward
		 * compatible with previous architectures.  So, we just take the
		 * highest that is less than our target.
		 */
		int fat_match_found = 0;
		unsigned long best_fat_arch = 0;
		for(i = 0; i < nfat_archs; i++){
		    if(fat_archs[i].cputype != cputype)
			continue;
		    if(fat_archs[i].cpusubtype > cpusubtype)
			continue;
		    if(!fat_match_found){
			fat_match_found = 1;
			best_fat_arch = i;
			continue;
		    }
		    if(fat_archs[i].cpusubtype >
		       fat_archs[best_fat_arch].cpusubtype)
			best_fat_arch = i;
		}
		if(fat_match_found)
		  return fat_archs + best_fat_arch;
	    }
	    break;
	default:
	    return(NULL);
	}
	return(NULL);
}
