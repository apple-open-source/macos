#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <mach/machine.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <architecture/byte_order.h>

/*
 * The array of all currently know architecture flags (terminated with an entry
 * with all zeros).  Pointer to this returned with get_arch_flags().
 */
struct arch_flag {
    const char *name;
    cpu_type_t cputype;
    cpu_subtype_t cpusubtype;
};

static const struct arch_flag arch_flags[] = {
    { "any",    CPU_TYPE_ANY,       CPU_SUBTYPE_MULTIPLE },
    { "little", CPU_TYPE_ANY,       CPU_SUBTYPE_LITTLE_ENDIAN },
    { "big",    CPU_TYPE_ANY,       CPU_SUBTYPE_BIG_ENDIAN },
    /* architecture families */
    { "ppc",    CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_ALL },
    { "i386",   CPU_TYPE_I386,      CPU_SUBTYPE_I386_ALL },
    { "m68k",   CPU_TYPE_MC680x0,   CPU_SUBTYPE_MC680x0_ALL },
    { "hppa",   CPU_TYPE_HPPA,      CPU_SUBTYPE_HPPA_ALL },
    { "sparc",  CPU_TYPE_SPARC,     CPU_SUBTYPE_SPARC_ALL },
    { "m88k",   CPU_TYPE_MC88000,   CPU_SUBTYPE_MC88000_ALL },
    { "i860",   CPU_TYPE_I860,      CPU_SUBTYPE_I860_ALL },
    /* specific architecture implementations */
    { "ppc601", CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_601 },
    { "ppc603", CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_603 },
    { "ppc603e",CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_603e },
    { "ppc603ev",CPU_TYPE_POWERPC,  CPU_SUBTYPE_POWERPC_603ev },
    { "ppc604", CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_604 },
    { "ppc604e",CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_604e },
    { "ppc750", CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_750 },
    { "ppc7400",CPU_TYPE_POWERPC,   CPU_SUBTYPE_POWERPC_7400 },
    { "i486",   CPU_TYPE_I386,      CPU_SUBTYPE_486 },
    { "i486SX", CPU_TYPE_I386,      CPU_SUBTYPE_486SX },
    { "pentium",CPU_TYPE_I386,      CPU_SUBTYPE_PENT }, /* same as i586 */
    { "i586",   CPU_TYPE_I386,      CPU_SUBTYPE_586 },
    { "pentpro", CPU_TYPE_I386,     CPU_SUBTYPE_PENTPRO },
    { "pentIIm3",CPU_TYPE_I386,     CPU_SUBTYPE_PENTII_M3 },
    { "pentIIm5",CPU_TYPE_I386,     CPU_SUBTYPE_PENTII_M5 },
    { "m68030", CPU_TYPE_MC680x0,   CPU_SUBTYPE_MC68030_ONLY },
    { "m68040", CPU_TYPE_MC680x0,   CPU_SUBTYPE_MC68040 },
    { "hppa7100LC", CPU_TYPE_HPPA,  CPU_SUBTYPE_HPPA_7100LC },
    { NULL,     0,                  0 }
};


static int
check_fatarch(
    const struct fat_arch *arch,
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype)
{
    return (arch->cputype == cputype && arch->cpusubtype == cpusubtype);
}

static int
check_fatarchlist(
    struct fat_arch *first_arch,
    int nfat_archs,
    int nsubtypes,
    cpu_type_t cputype,
    const cpu_subtype_t *cpusubtype)
{
    struct fat_arch *arch;
    int archi, j;

    for (j = 0; j < nsubtypes; j++) {
        arch = first_arch;
        for (archi = 0; archi < nfat_archs; archi++, arch++) {
            if (check_fatarch(arch, cputype, cpusubtype[j]))
                return archi;
        }
    }

    return nfat_archs;
}

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
static
struct fat_arch *
cpusubtype_findbestarch(
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype,
    struct fat_header *fat,
    off_t size)
{
    unsigned long archi, lowest_family, lowest_model, lowest_index;
    struct fat_arch *first_arch, *arch;
    int nfat_archs;

    nfat_archs = NXSwapBigLongToHost(fat->nfat_arch);
    arch = (struct fat_arch *)((char *)fat + sizeof(struct fat_header));
    if ( (u_int8_t *) fat + size < (u_int8_t *)  &arch[nfat_archs])
        return NULL;

    first_arch = malloc(nfat_archs * sizeof(struct fat_arch));
    memcpy(first_arch, arch, nfat_archs * sizeof(struct fat_arch));

    /*
     * Look for the first exact match.
     * Also convert to host endianness as we go through the list
     */
    for (archi = 0, arch = first_arch; archi < nfat_archs; archi++, arch++) {
        arch->cputype = NXSwapBigIntToHost(arch->cputype);
        arch->cpusubtype = NXSwapBigIntToHost(arch->cpusubtype);
        if (check_fatarch(arch, cputype, cpusubtype))
            goto foundArch;
    }

    /*
     * An exact match was not found so find the next best match which is
     * cputype dependent.
     */
    switch(cputype) {
    case CPU_TYPE_I386:
        switch(cpusubtype) {
        default:
            /*
             * Intel cpusubtypes after the pentium (same as 586) are handled
             * such that they require an exact match or they can use the
             * pentium.  If that is not found call into the loop for the
             * earlier subtypes.
             */
            arch = first_arch;
            for (archi = 0; archi < nfat_archs; archi++, arch++) {
                if (check_fatarch(arch, cputype, CPU_SUBTYPE_PENT))
                    goto foundArch;
            }
            /* No Break */
        case CPU_SUBTYPE_PENT:
        case CPU_SUBTYPE_486SX:
            /*
             * Since an exact match as not found look for the i486 else
             * break into the loop to look for the i386_ALL.
             */
            arch = first_arch;
            for (archi = 0; archi < nfat_archs; archi++, arch++) {
                if (check_fatarch(arch, cputype, CPU_SUBTYPE_486))
                    goto foundArch;
            }
            break;

        case CPU_SUBTYPE_I386_ALL:
        /* case CPU_SUBTYPE_I386: same as above */
        case CPU_SUBTYPE_486:
            break;
        }

        /*
         * A match failed, promote as little as possible.
         */
        {
            static cpu_subtype_t sub_list[] = {
                CPU_SUBTYPE_I386_ALL,
                CPU_SUBTYPE_486,
                CPU_SUBTYPE_486SX,
                CPU_SUBTYPE_586,
            };

            archi = check_fatarchlist(first_arch,
                                     nfat_archs,
                                     sizeof(sub_list)/sizeof(sub_list[0]),
                                     cputype,
                                     sub_list);
            if (archi != nfat_archs)
                goto foundArch;
        }

        /*
         * Now look for the lowest family and in that the lowest model.
         */
        lowest_family = CPU_SUBTYPE_INTEL_FAMILY_MAX + 1;
        arch = first_arch;
        for (archi = 0; archi < nfat_archs; archi++, arch++) {
            if (arch->cputype == cputype
            &&  CPU_SUBTYPE_INTEL_FAMILY(arch->cpusubtype) < lowest_family) {
                lowest_family = CPU_SUBTYPE_INTEL_FAMILY(arch->cpusubtype);
            }
        }

        /* if no intel cputypes found return NULL */
        if (lowest_family == CPU_SUBTYPE_INTEL_FAMILY_MAX + 1) {
            archi = nfat_archs;
            goto foundArch;
        }
    
        lowest_model = ULONG_MAX;
        lowest_index = -1;
        arch = first_arch;
        for (archi = 0; archi < nfat_archs; archi++, arch++) {
            if (arch->cputype == cputype
            &&  CPU_SUBTYPE_INTEL_FAMILY(arch->cpusubtype) == lowest_family
            &&  CPU_SUBTYPE_INTEL_MODEL( arch->cpusubtype) <  lowest_model) {
                lowest_model = CPU_SUBTYPE_INTEL_MODEL(arch->cpusubtype);
                lowest_index = archi;
            }
        }
        if (lowest_index == -1)
            archi = nfat_archs;
        else
            archi = lowest_index;
        goto foundArch;

    case CPU_TYPE_MC680x0:
        arch = first_arch;
        for (archi = 0; archi < nfat_archs; archi++, arch++) {
            if (check_fatarch(arch, cputype, CPU_SUBTYPE_MC680x0_ALL))
                goto foundArch;
        }

        /*
         * Try to promote if starting from CPU_SUBTYPE_MC680x0_ALL and
         * favor the CPU_SUBTYPE_MC68040 over the CPU_SUBTYPE_MC68030_ONLY.
         */
        archi = nfat_archs;
        if (cpusubtype == CPU_SUBTYPE_MC680x0_ALL) {
            static cpu_subtype_t sub_list[] = {
                CPU_SUBTYPE_MC68040,
                CPU_SUBTYPE_MC68030_ONLY,
            };

            archi = check_fatarchlist(first_arch,
                                     nfat_archs,
                                     sizeof(sub_list)/sizeof(sub_list[0]),
                                     cputype,
                                     sub_list);
        }
        goto foundArch;

    case CPU_TYPE_POWERPC:
    {
        /*
         * An exact match as not found.  So for all the PowerPC subtypes
         * pick the subtype from the following order:
         * 7400, 750, 604e, 604, 603ev, 603e, 603, ALL
         * Note the 601 is NOT in the list above.  It is only picked via
         * an exact match.
         */
        static cpu_subtype_t sub_list[] = {
            CPU_SUBTYPE_POWERPC_7400,
            CPU_SUBTYPE_POWERPC_750,
            CPU_SUBTYPE_POWERPC_604e,
            CPU_SUBTYPE_POWERPC_604,
            CPU_SUBTYPE_POWERPC_603ev,
            CPU_SUBTYPE_POWERPC_603e,
            CPU_SUBTYPE_POWERPC_603,
            CPU_SUBTYPE_POWERPC_ALL,
        };

        archi = check_fatarchlist(first_arch,
                                 nfat_archs,
                                 sizeof(sub_list)/sizeof(sub_list[0]),
                                 cputype,
                                 sub_list);
        goto foundArch;
    }

    case CPU_TYPE_MC88000:
        cpusubtype = CPU_SUBTYPE_MC88000_ALL; break;
    case CPU_TYPE_I860:
        cpusubtype = CPU_SUBTYPE_I860_ALL; break;
    case CPU_TYPE_HPPA:
        cpusubtype = CPU_SUBTYPE_HPPA_ALL; break;
    case CPU_TYPE_SPARC:
        cpusubtype = CPU_SUBTYPE_SPARC_ALL; break;
    default:
        goto foundArch;
    }

    // Check for one of the 'ALL' subtypes
    arch = first_arch;
    for (archi = 0; archi < nfat_archs; archi++, arch++) {
        if (check_fatarch(arch, cputype, cpusubtype))
            goto foundArch;
    }
    archi = nfat_archs;

    // Well that's all folks no options left;
foundArch:
    free(first_arch);

    if (archi >= nfat_archs) return NULL;
    arch = (struct fat_arch *)((char *)fat + sizeof(struct fat_header));
    return &arch[archi];
}

__private_extern__ void
find_arch(
    u_int8_t **dataP,
    off_t *sizeP,
    cpu_type_t cputype,
    cpu_subtype_t cpusubtype,
    u_int8_t *data_ptr,
    off_t filesize)
{
    struct fat_header *fat_hdr;
    struct mach_header *mach_hdr;
    struct fat_arch *best_arch;
    struct {
        struct fat_header hdr;
        struct fat_arch arch;
    } fakeHeader;
    int is_fat;
    int is_mh;

    fat_hdr = (struct fat_header *) data_ptr;
    is_fat = (FAT_MAGIC == fat_hdr->magic || FAT_CIGAM == fat_hdr->magic);

    mach_hdr = (struct mach_header *) data_ptr;
    is_mh = (MH_MAGIC == mach_hdr->magic || MH_CIGAM == mach_hdr->magic);

    // If it is full fat or not an executable then return unchanged.
    if (cputype == CPU_TYPE_ANY || !(is_mh || is_fat)) {
        if (sizeP) *sizeP = filesize;
        if (dataP) *dataP = data_ptr;
        return;
    }

    if (is_mh) {
        fakeHeader.hdr.magic = FAT_MAGIC;
        fakeHeader.hdr.nfat_arch = NXSwapHostLongToBig(1);
        fakeHeader.arch.cputype = NXSwapHostIntToBig(mach_hdr->cputype);
        fakeHeader.arch.cpusubtype = NXSwapHostIntToBig(mach_hdr->cpusubtype);
        fakeHeader.arch.offset = NXSwapHostIntToBig(0);
        fakeHeader.arch.size = NXSwapHostLongToBig((long) filesize);
        fat_hdr = &fakeHeader.hdr;
    }

    /*
     *  Map portion that must be accessible directly into
     *  kernel's map.
     */
    best_arch = cpusubtype_findbestarch(cputype, cpusubtype, fat_hdr, filesize);

    /* Return our results. */
    if (best_arch) {
        if (sizeP) *sizeP = NXSwapBigLongToHost(best_arch->size);
        if (dataP) *dataP = data_ptr + NXSwapBigLongToHost(best_arch->offset);
    } else {
        if (sizeP) *sizeP = 0;
        if (dataP) *dataP = 0;
    }
}

/*
 * get_arch_from_flag() is passed a name of an architecture flag and returns
 * zero if that flag is not known and non-zero if the flag is known.
 * If the pointer to the arch_flag is not NULL it is filled in with the
 * arch_flag struct that matches the name.
 */
__private_extern__ int
get_arch_from_flag(char *name, cpu_type_t *cpuP, cpu_subtype_t *subcpuP)
{
    const struct arch_flag *arch;

    for (arch = arch_flags; arch->name != NULL; arch++) {
        if (strcmp(arch->name, name) == 0) {
            if (cpuP)
                *cpuP = arch->cputype;
            if (subcpuP)
                *subcpuP = arch->cpusubtype;
            return 1;
        }
    }
    return 0;
}


