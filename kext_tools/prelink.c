#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <libc.h>
#include <Kernel/libsa/mkext.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fts.h>
#include <architecture/byte_order.h>
#include <mach-o/arch.h>
#include <mach-o/fat.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>
#include <mach-o/swap.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/machine/vm_param.h>
#include <mach/kmod.h>
#include <mach/mach_types.h>

#include <IOKit/kext/KXKextManager.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOKitLib.h>
#include <IOKit/IOCFUnserialize.h>
#include <IOKit/IOCFSerialize.h>
#include <libkern/OSByteOrder.h>

/*******************************************************************************
*******************************************************************************/

// In compression.c
__private_extern__ u_int32_t
local_adler32(u_int8_t *buffer, int32_t length);

// In arch.c
__private_extern__ void
find_arch(u_int8_t **dataP, off_t *sizeP, cpu_type_t in_cpu,
    cpu_subtype_t in_cpu_subtype, u_int8_t *data_ptr, off_t filesize);
__private_extern__ int
get_arch_from_flag(char *name, cpu_type_t *cpuP, cpu_subtype_t *subcpuP);

__private_extern__ KXKextManagerError
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize);
__private_extern__ KXKextManagerError
writeFile(int fd, const void * data, vm_size_t length);

__private_extern__ void verbose_log(const char * format, ...);

/*******************************************************************************
*******************************************************************************/

extern KXKextManagerError _KXKextManagerPrepareKextForLoading(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    Boolean check_loaded_for_dependencies,
    Boolean do_load,
    CFMutableArrayRef inauthenticKexts);

extern KXKextManagerError _KXKextManagerLoadKextUsingOptions(
    KXKextManagerRef aKextManager,
    KXKextRef aKext,
    const char * kext_name,
    const char * kernel_file,
    const char * patch_dir,
    const char * symbol_dir,
    IOOptionBits load_options,
    Boolean do_start_kext,
    int     interactive_level,
    Boolean ask_overwrite_symbols,
    Boolean overwrite_symbols,
    Boolean get_addrs_from_kernel,
    unsigned int num_addresses,
    char ** addresses);

// load_options
enum 
{
    kKXKextManagerLoadNone	= false,
    kKXKextManagerLoadKernel	= true,
    kKXKextManagerLoadPrelink	= 2,
    kKXKextManagerLoadKextd	= 3
};

extern const char * _KXKextCopyCanonicalPathnameAsCString(KXKextRef aKext);
static CFMutableDictionaryRef copyInfoDict(CFBundleRef bundle);

/*******************************************************************************
*******************************************************************************/
extern void
kld_set_architecture(const NXArchInfo * arch);
extern Boolean
kld_macho_swap(struct mach_header * mh);
extern void
kld_macho_unswap(struct mach_header * mh, Boolean didSwap, int symbols);

/*******************************************************************************
*******************************************************************************/

#define TEMP_DIR	"/com.apple.iokit.kextcache.XX"

KXKextManagerError
PreLink(KXKextManagerRef theKextManager, CFDictionaryRef kextDict, 
        const char * kernelCacheFilename,
	const struct timeval *cacheFileTimes,
	const char * kernel_file_name, 
	const char * platform_name,
	const char * root_path,
	CFSetRef kernel_requests,
	Boolean all_plists,
	const NXArchInfo * arch,
	int verbose_level, Boolean debug_mode)
{
    KXKextManagerError	result = kKXKextManagerErrorFileAccess;
    CFIndex		kexts_count, i;
    KXKextRef *		kexts = NULL;   // must free
    KXKextRef 		theKext = NULL; // don't release

    char * symbol_dir = NULL;
    const char * temp_dir;
    const char * temp_file = "cache.out";
    int outfd = -1, curwd = -1;

    CFBundleRef kextBundle = NULL;    // don't release
    CFMutableArrayRef      moduleList;
    CFMutableDictionaryRef infoDict;
    vm_offset_t nextKernelVM;

    Boolean	use_existing, kernel_swapped, includeInfo;

    vm_offset_t kernel_file, kernel_map;
    off_t       kernel_size;
    vm_size_t   kernel_file_size, bytes, totalBytes;
    vm_size_t   totalModuleBytes, totalInfoBytes;
    vm_size_t	totalSymbolBytes, totalSymbolSetBytes, totalSymbolDiscardedBytes;
    vm_size_t   remainingModuleBytes, fileoffset, vmoffset, tailoffset;
    CFIndex     idx, ncmds, cmd, moduleCount;
    IOReturn    err;

    struct segment_command * seg;
    struct segment_command * prelinkseg;
    struct section *         prelinksects;
    struct PrelinkState
    {
        kmod_info_t modules[1];
    };
    struct PrelinkState   prelink_state_init;
    struct PrelinkState * prelink_state;
    vm_size_t 		  prelink_size;
    int 		* prelink_dependencies;
    vm_size_t 		  prelink_dependencies_size;
    kmod_info_t * 	  lastInfo;

    struct FileInfo
    {
	vm_offset_t mapped;
	vm_size_t   mappedSize;
	vm_offset_t symtaboffset;
	vm_offset_t symbolsetoffset;
	vm_size_t   symtabsize;
	vm_size_t   symtabdiscarded;
	CFStringRef key;
	KXKextRef   kext;
	Boolean	    code;
	Boolean	    swapped;
    };
    struct FileInfo * files = NULL;

    // --

    moduleList = CFArrayCreateMutable( kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks );

    if (kKXKextManagerErrorNone !=
        (err = readFile(kernel_file_name, &kernel_file, &kernel_file_size)))
        goto finish;

    find_arch( (u_int8_t **) &kernel_map, &kernel_size, arch->cputype, arch->cpusubtype, 
		(u_int8_t *) kernel_file, kernel_file_size);
    if (!kernel_size) {
	fprintf(stderr, "can't find architecture %s in %s\n",
                arch->name, kernel_file_name);
        err = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    if (arch->cputype != CPU_TYPE_ANY)
	kld_set_architecture(arch);
    kernel_swapped = kld_macho_swap((struct mach_header *)kernel_map);

    ncmds = ((struct mach_header *)kernel_map)->ncmds;
    seg = (struct segment_command *)(((struct mach_header *)kernel_map) + 1);
    for (cmd = 0;
            cmd < ncmds;
            cmd++, seg = (struct segment_command *)(((vm_offset_t)seg) + seg->cmdsize))
    {
        if (LC_SEGMENT != seg->cmd)
            continue;
        if (strcmp("__PRELINK", seg->segname))
            continue;
        break;
    }
    if (cmd >= ncmds)
    {
        fprintf(stderr, "no __PRELINK segment in %s\n", kernel_file_name);
        err = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    prelinkseg   = seg;
    prelinksects = (struct section *) (prelinkseg + 1);

    if ((prelinkseg->nsects != 3)
     || (strcmp("__text",   prelinksects[0].sectname))
     || (strcmp("__symtab", prelinksects[1].sectname))
     || (strcmp("__info",   prelinksects[2].sectname)))
    {
        fprintf(stderr, "unexpected __PRELINK sections in %s\n", kernel_file_name);
        err = kKXKextManagerErrorLinkLoad;
        goto finish;
    }

    if (!prelinkseg->fileoff)
        prelinkseg->fileoff = prelinksects[0].offset;

    temp_dir = getenv("TMPDIR");
    if (!temp_dir)
        temp_dir = "/tmp";
    symbol_dir = malloc(strlen(temp_dir) + strlen(TEMP_DIR) + 1);
    strcpy(symbol_dir, temp_dir);
    strcat(symbol_dir, TEMP_DIR);
    mktemp(symbol_dir);
    if (0 != mkdir(symbol_dir, 0755))
    {
        fprintf(stderr, "mkdir(%s) failed: %s\n", symbol_dir, strerror(errno));
        symbol_dir[0] = 0;
        err = kKXKextManagerErrorFileAccess;
        goto finish;
    }
    curwd = open(".", O_RDONLY, 0);
    if (0 != chdir(symbol_dir))
    {
        perror("chdir");
        err = kKXKextManagerErrorFileAccess;
        goto finish;
    }

    use_existing = false;
    if (!use_existing)
    {
	int fd;

        bzero(&prelink_state_init, sizeof(prelink_state_init));
        prelink_state_init.modules[0].address = prelinkseg->vmaddr;

	fd = open("prelinkstate", O_WRONLY|O_CREAT|O_TRUNC, 0755);
	if (-1 == fd)
	{
            perror("create prelinkstate failed");
            err = kKXKextManagerErrorFileAccess;
            goto finish;
        }
        err = writeFile(fd, &prelink_state_init, sizeof(prelink_state_init));
        close(fd);
        if (kKXKextManagerErrorNone != err)
            goto finish;
    }

   /* Prepare to iterate over the kexts in the dictionary.
    */
    kexts_count = CFDictionaryGetCount(kextDict);
    kexts       = (KXKextRef *) calloc(kexts_count, sizeof(KXKextRef));

    if (!kexts) {
        fprintf(stderr, "memory allocation failure\n");
        err = kKXKextManagerErrorNoMemory;
        goto finish;
    }
    CFDictionaryGetKeysAndValues(kextDict, (const void **)NULL, (const void **)kexts);

    for (i = 0; i < kexts_count; i++)
    {
	Boolean linkit;

        theKext = (KXKextRef) kexts[i];
	linkit = KXKextGetDeclaresExecutable(theKext)
		&& (kextBundle = KXKextGetBundle(theKext));
	if (linkit && kernel_requests)
	{
	    CFStringRef bundleIdentifier = CFBundleGetIdentifier(kextBundle);
	    linkit = (bundleIdentifier 
			&& CFSetContainsValue(kernel_requests, bundleIdentifier));
	}

        if (linkit)
	{
	    result = _KXKextManagerPrepareKextForLoading(
			    theKextManager, theKext, NULL /*kext_name*/,
			    FALSE /*check_loaded_for_dependencies*/,
			    FALSE /*do_load*/,
			    NULL  /*inauthenticKexts*/);

	    if (!use_existing && (result == kKXKextManagerErrorNone)) {

		result = _KXKextManagerLoadKextUsingOptions(
			    theKextManager, theKext, NULL /*kext_name*/, kernel_file_name,
			    NULL /*patch_dir*/,
			    symbol_dir,
			    kKXKextManagerLoadPrelink     /*load_options*/,
			    FALSE /*do_start_kmod*/,
			    0     /*interactive_level*/,
			    FALSE /*ask_overwrite_symbols*/,
			    FALSE /*overwrite_symbols*/,
			    FALSE /*get_addrs_from_kernel*/,
			    0     /*num_addresses*/, NULL /*addresses*/);
	    }

	    if ((result != kKXKextManagerErrorNone) && (verbose_level > 0))
	    {
                const char * kext_path = NULL; // must free

                kext_path = _KXKextCopyCanonicalPathnameAsCString(theKext);
                if (kext_path) {
                    fprintf(stderr, kext_path);
		    free((char *)kext_path);
                }
		fprintf(stderr, " error 0x%x\n", result);
	    }
	}
    }
    {
	struct module_header {
	    struct mach_header	   h;
	    struct segment_command seg[1];
	};
	struct module_header * header;
	int num_modules;

        if (kKXKextManagerErrorNone !=
            (err = readFile("prelinkstate", 
                            (vm_offset_t *) &prelink_state, &prelink_size)))
	    goto finish;

        if (kKXKextManagerErrorNone !=
            (err = readFile("prelinkdependencies", 
			(vm_offset_t *) &prelink_dependencies, &prelink_dependencies_size)))
	    goto finish;

	num_modules =  prelink_state->modules[0].id;
	nextKernelVM = prelink_state->modules[0].address;

	if (!num_modules || (prelink_size < ((num_modules + 1) * sizeof(kmod_info_t))))
	{
	    fprintf(stderr, "read prelinkstate failed\n");
            err = kKXKextManagerErrorFileAccess;
	    goto finish;
	}
    
	if (verbose_level > 0)
	{
	    verbose_log("%d modules - code VM 0x%lx - 0x%x (0x%lx, %.2f Mb)",
			num_modules, prelinkseg->vmaddr, nextKernelVM,
			nextKernelVM - prelinkseg->vmaddr,
			((float)(nextKernelVM - prelinkseg->vmaddr)) / 1024.0 / 1024.0 );
	}

	// map files, get sizes
        files = (struct FileInfo *) calloc(num_modules, sizeof(struct FileInfo));
	totalModuleBytes = 0;
	for (idx = 0; idx < num_modules; idx++)
	{
	    files[idx].key = CFStringCreateWithCString(kCFAllocatorDefault,
				prelink_state->modules[1+idx].name, kCFStringEncodingMacRoman);

	    files[idx].kext = KXKextManagerGetKextWithIdentifier(theKextManager, files[idx].key);

	    if (!prelink_state->modules[1+idx].size)
		continue;

            if (kKXKextManagerErrorNone !=
                (err = readFile(prelink_state->modules[1+idx].name, 
                                &files[idx].mapped, &files[idx].mappedSize)))
		goto finish;

	    header = (struct module_header *) files[idx].mapped;
	    files[idx].swapped = kld_macho_swap(&header->h);
	    files[idx].code    = (LC_SEGMENT == header->seg[0].cmd);

	    if (files[idx].code)
		totalModuleBytes += header->seg[0].vmaddr + round_page(header->seg[0].vmsize) 
				    - prelink_state->modules[1+idx].address;
	}

	totalSymbolBytes          = 0;
	totalSymbolDiscardedBytes = 0;
	totalSymbolSetBytes       = 0;
	remainingModuleBytes      = totalModuleBytes;

	// create prelinked kernel file

	outfd = open("mach_kernel.prelink", O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (-1 == outfd) {
	    fprintf(stderr, "can't create %s: %s\n", "mach_kernel.prelink",
		strerror(errno));
            err = kKXKextManagerErrorFileAccess;
	    goto finish;
        }

	// start writing at the prelink segs file offset
	if (-1 == lseek(outfd, prelinkseg->fileoff, SEEK_SET)) {
            perror("couldn't write output");
            err = kKXKextManagerErrorFileAccess;
	    goto finish;
        }

	for (idx = 0, lastInfo = NULL; idx < num_modules; idx++)
	{
	    kmod_info_t *           info;
	    unsigned long           modcmd;
	    struct symtab_command * symcmd;
	    struct section *        sect;
	    vm_size_t               headerOffset;

	    if (!files[idx].code)
	    {
		if (prelink_state->modules[1+idx].size)
		{
		    // for symbol sets the whole file ends up in the symbol sect
		    files[idx].symtaboffset    = 0;
		    files[idx].symtabsize      = prelink_state->modules[1+idx].size;
		    files[idx].symbolsetoffset = totalSymbolBytes;
		    totalSymbolBytes       += files[idx].symtabsize;
		    totalSymbolSetBytes    += files[idx].symtabsize;
		}
		continue;
	    }

	    header = (struct module_header *) files[idx].mapped;

	    // patch kmod_info 

	    info = (kmod_info_t *) (prelink_state->modules[1+idx].id - header->seg[0].vmaddr
					+ header->seg[0].fileoff + files[idx].mapped);
    
	    info->next       = lastInfo;
	    lastInfo         = info;
	    bcopy(prelink_state->modules[1+idx].name, info->name, sizeof(info->name));
	    bcopy(prelink_state->modules[1+idx].version, info->version, sizeof(info->version));
	    info->address    = prelink_state->modules[1+idx].address;
	    info->size       = prelink_state->modules[1+idx].size;
	    info->id         = prelink_state->modules[1+idx].id;
	    info->hdr_size   = header->seg[0].vmaddr - prelink_state->modules[1+idx].address;

	    // patch offsets
	    // how far back the header moves
	    headerOffset = info->hdr_size - header->seg[0].fileoff;

	    header->seg[0].fileoff  += headerOffset;
	    header->seg[0].filesize += headerOffset;

	    // module code size
	    tailoffset = round_page(header->seg[0].vmsize) + info->hdr_size - headerOffset;

	    for (modcmd = 0, sect = (struct section *) &header->seg[1];
		 modcmd < header->seg[0].nsects;
		 modcmd++, sect++)
		sect->offset += headerOffset;

	    for (modcmd = 0, seg = &header->seg[0];
		    (modcmd < header->h.ncmds) && (LC_SYMTAB != seg->cmd);
		    modcmd++, seg = (struct segment_command *)(((vm_offset_t)seg) + seg->cmdsize))
		    {}
	    if (modcmd >= header->h.ncmds)
	    {
		fprintf(stderr, "No LC_SYMTAB in %s\n", prelink_state->modules[1+idx].name);
                err = kKXKextManagerErrorLinkLoad;
		goto finish;
	    }
	    symcmd = (struct symtab_command *) seg;

	    theKext = files[idx].kext;
	    if (false
	     && theKext
	     && (kextBundle = KXKextGetBundle(theKext))
	     && CFBundleGetValueForInfoDictionaryKey(kextBundle, CFSTR("OSBundleCompatibleVersion")))
	    {
		// keeping symbols for this module
		struct nlist *		sym;
		long			align, lastUsedOffset = 0;
		const char *		lastUsedString;
		unsigned int 		strIdx;

		files[idx].symtaboffset = symcmd->symoff;
		files[idx].symtabsize   = symcmd->nsyms * sizeof(struct nlist);

		// .. but just the external strings
		sym = (struct nlist *) (symcmd->symoff + files[idx].mapped);
		for(strIdx = 0; strIdx < symcmd->nsyms; strIdx++, sym++)
		{
		    if (!lastUsedOffset)
			lastUsedOffset = sym->n_un.n_strx;
		    else if (!(sym->n_type & N_EXT))
			sym->n_un.n_strx = 0;
		    else if (sym->n_un.n_strx > lastUsedOffset)
			lastUsedOffset = sym->n_un.n_strx;
		}

		lastUsedString  = (const char *) (symcmd->stroff + lastUsedOffset + files[idx].mapped);
		lastUsedOffset += (1 + strlen(lastUsedString));
		align = lastUsedOffset % sizeof(long);
		if (align)
		{
		    align = sizeof(long) - align;
		    bzero((void *) (symcmd->stroff + lastUsedOffset + files[idx].mapped), align);
		    lastUsedOffset += align;
		}
		files[idx].symtabsize     += lastUsedOffset;
		files[idx].symtabdiscarded = symcmd->strsize - lastUsedOffset;
		symcmd->strsize            = lastUsedOffset;

		// unswap symbols
		kld_macho_unswap(&header->h, files[idx].swapped, 1);

		// patch offset to symbols
		// how far ahead the symtab moves
		bytes = remainingModuleBytes + totalSymbolBytes;

		symcmd->symoff    = bytes;
		symcmd->stroff   += bytes - files[idx].symtaboffset;
    
		totalSymbolBytes          += files[idx].symtabsize;
		totalSymbolDiscardedBytes += files[idx].symtabdiscarded;
	    }
	    else
	    {
		// ditching symbols for this module
		files[idx].symtaboffset = 0;
		files[idx].symtabsize   = 0;
		symcmd->nsyms           = 0;
		symcmd->symoff          = 0;
		symcmd->stroff          = 0;
	    }

	    remainingModuleBytes -= prelink_state->modules[1+idx].size;

	    // unswap rest of module
	    if (files[idx].swapped)
	    {
		info->next       = (void *) NXSwapLong((long) info->next);
		info->address    = NXSwapLong(info->address);
		info->size       = NXSwapLong(info->size);
		info->hdr_size   = NXSwapLong(info->hdr_size);
		info->id         = NXSwapLong(info->id);
	    }
	    kld_macho_unswap(&header->h, files[idx].swapped, -1);
	    files[idx].swapped = false;
	    header = 0;
	    info   = 0;

	    // copy header to start of VM allocation
	    if (kKXKextManagerErrorNone !=
                (err = writeFile(outfd, (const void *) files[idx].mapped, headerOffset)))
                goto finish;

	    // write the module
	    if (tailoffset <= files[idx].mappedSize)
	    {
		if (kKXKextManagerErrorNone !=
		    (err = writeFile(outfd, (const void *) files[idx].mapped, tailoffset )))
		    goto finish;
	    }
	    else
	    {
		if (kKXKextManagerErrorNone !=
		    (err = writeFile(outfd, (const void *) files[idx].mapped, files[idx].mappedSize )))
		    goto finish;

		if (-1 == lseek(outfd, tailoffset - files[idx].mappedSize, SEEK_CUR)) {
		    perror("couldn't write output");
		    err = kKXKextManagerErrorFileAccess;
		    goto finish;
		}
	    }
	}

	// write the symtabs, get info, unmap

	for (idx = 0; idx < num_modules; idx++)
	{
	    bytes = files[idx].symtabsize;
	    if (bytes && prelink_state->modules[1+idx].size)
	    {
		if (files[idx].mappedSize < (files[idx].symtaboffset + bytes))
		{
		    fprintf(stderr, "%s is truncated\n", prelink_state->modules[1+idx].name);
		    result = kKXKextManagerErrorFileAccess;
		    goto finish;
		}
		else if (files[idx].mappedSize >
			(files[idx].symtaboffset + bytes + files[idx].symtabdiscarded))
		    fprintf(stderr, "%s has extra data\n", prelink_state->modules[1+idx].name);
    
		// write symtab
                if (kKXKextManagerErrorNone !=
                    (err = writeFile(outfd, (const void *) files[idx].mapped + files[idx].symtaboffset, bytes)))
                    goto finish;
	    }
	    // unmap file
	    if (files[idx].mappedSize)
	    {
		vm_deallocate(mach_task_self(), files[idx].mapped, files[idx].mappedSize);
		files[idx].mapped     = 0;
		files[idx].mappedSize = 0;
	    }

	    // make info dict

	    theKext = files[idx].kext;
	    infoDict = NULL;

	    includeInfo = (theKext && (kextBundle = KXKextGetBundle(theKext)));
	    if (includeInfo && !all_plists)
	    {
		CFStringRef str;
		// check OSBundleRequired to see if safe for boot time matching
		str = CFBundleGetValueForInfoDictionaryKey(kextBundle, CFSTR("OSBundleRequired"));
		includeInfo = (str && (kCFCompareEqualTo != CFStringCompare(str, CFSTR("Safe Boot"), 0)));
	    }

	    if (includeInfo)
		infoDict = copyInfoDict(kextBundle);

	    if (!infoDict)
	    {
		if (debug_mode > 0)
		{
		    verbose_log("skip info for %s", prelink_state->modules[1+idx].name);
		}
		infoDict = CFDictionaryCreateMutable(
		    kCFAllocatorDefault, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		CFDictionarySetValue(infoDict, CFSTR("CFBundleIdentifier"), files[idx].key);
	    }
	    CFRelease(files[idx].key);
	    files[idx].key = 0;

	    if (prelink_state->modules[1+idx].address)
	    {
		CFMutableDataRef data;
		enum {		 kPrelinkReservedCount = 4 };
		UInt32		 prelinkInfo[kPrelinkReservedCount];

		prelinkInfo[0] = NXSwapHostIntToBig(prelink_state->modules[1+idx].id);
		prelinkInfo[1] = NXSwapHostIntToBig(0);
		prelinkInfo[2] = NXSwapHostIntToBig(sizeof(prelinkInfo));
		prelinkInfo[3] = NXSwapHostIntToBig(
			prelink_state->modules[1+idx].reference_count * sizeof(UInt32)
			+ sizeof(prelinkInfo));

		data = CFDataCreateMutable(kCFAllocatorDefault, 0);
		CFDataAppendBytes( data,
				    (const UInt8 *) prelinkInfo, 
				    sizeof(prelinkInfo) );

		if (prelink_state->modules[1+idx].reference_count)
		{
		    CFIndex start = (CFIndex) prelink_state->modules[1+idx].reference_list;
		    CFDataAppendBytes( data, 
					(const UInt8 *) &prelink_dependencies[start], 
					prelink_state->modules[1+idx].reference_count * sizeof(CFIndex) );
		}
		CFDictionarySetValue(infoDict, CFSTR("OSBundlePrelink"), data);
		CFRelease(data);
	    }
	    else if (files[idx].symtabsize)
	    {
		CFNumberRef num;
		
		num = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
					(const void *) &files[idx].symbolsetoffset);
		CFDictionarySetValue(infoDict, CFSTR("OSBundlePrelinkSymbols"), num);
		CFRelease(num);
	    }
	    CFArrayAppendValue(moduleList, infoDict);
	    CFRelease(infoDict);
	}

	bytes = round_page(totalSymbolBytes) - totalSymbolBytes;
	totalSymbolBytes += bytes;
	if (-1 == lseek(outfd, bytes, SEEK_CUR)) {
            perror("couldn't write output");
            err = kKXKextManagerErrorFileAccess;
	    goto finish;
        }

	// deferred load __info
	for (i = 0; i < kexts_count; i++)
	{
	    theKext = (KXKextRef) kexts[i];
	    for (idx = 0;
		(idx < num_modules) && (theKext != files[idx].kext);
		idx++)	{}
	    if (idx < num_modules)
		continue;

	    includeInfo = (theKext && (kextBundle = KXKextGetBundle(theKext)));
	    if (includeInfo)
	    {
		if (!all_plists)
		{
		    CFStringRef str;
		    // check OSBundleRequired to see if safe for boot time matching
		    str = CFBundleGetValueForInfoDictionaryKey(kextBundle, CFSTR("OSBundleRequired"));
		    includeInfo = (str && (kCFCompareEqualTo != CFStringCompare(str, CFSTR("Safe Boot"), 0)));
		}
		if (includeInfo)
		{
		    infoDict = copyInfoDict(kextBundle);
		    if (infoDict)
		    {
			if (!all_plists)
			{
			    Boolean linkit = KXKextGetDeclaresExecutable(theKext);
			    if (linkit && kernel_requests)
			    {
				CFStringRef bundleIdentifier = CFBundleGetIdentifier(kextBundle);
				linkit = (bundleIdentifier 
					    && CFSetContainsValue(kernel_requests, bundleIdentifier));
			    }
			    // if we got here, the link was attempted and failed; never defer this one
			    // so the same failure will happen for the cached boot.
			    if (!linkit)
				CFDictionarySetValue(infoDict, CFSTR("OSBundleDefer"), kCFBooleanTrue);
			}
			CFArrayAppendValue(moduleList, infoDict);
			CFRelease(infoDict);
		    }
		}
	    }
	}

	// patch __info
        moduleCount = CFArrayGetCount(moduleList);
        for (idx = 0; idx < moduleCount; idx++)
        {
            CFDictionaryRef	     personalities;
            CFMutableDictionaryRef * allPersonalities;
            CFStringRef 	     bundleIdentifier;
            CFIndex		     count, idx2;

            infoDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex(moduleList, idx);

            personalities  = CFDictionaryGetValue(infoDict, CFSTR("IOKitPersonalities"));
            if (!personalities)
                continue;
            bundleIdentifier  = CFDictionaryGetValue(infoDict, CFSTR("CFBundleIdentifier"));
            if (!bundleIdentifier)
                continue;
            count = CFDictionaryGetCount(personalities);
            allPersonalities = calloc(count, sizeof(CFMutableDictionaryRef));
            if (!allPersonalities) {
                fprintf(stderr, "memory allocation failure\n");
                err = kKXKextManagerErrorNoMemory;
                goto finish;
            }
            CFDictionaryGetKeysAndValues(personalities, (const void **)NULL, (const void **)allPersonalities);
            for (idx2 = 0; idx2 < count; idx2++)
                CFDictionaryAddValue(allPersonalities[idx2], CFSTR("CFBundleIdentifier"), bundleIdentifier);
            free(allPersonalities);
        }
	// write __info
	{
	    CFDataRef data;
	    data = IOCFSerialize(moduleList, kNilOptions);
            if (!data)
            {
                fprintf(stderr, "couldn't serialize property lists\n");
                for (idx = 0; idx < moduleCount; idx++)
                {
                    infoDict = (CFMutableDictionaryRef) CFArrayGetValueAtIndex(moduleList, idx);
                    data = IOCFSerialize(infoDict, kNilOptions);
                    if (data)
                        CFRelease(data);
                    else
                        CFShow(infoDict);
                }
                err = kKXKextManagerErrorSerialization;
                goto finish;
            }
	    totalInfoBytes = round_page(CFDataGetLength(data));
	    if (kKXKextManagerErrorNone !=
                (err = writeFile(outfd, CFDataGetBytePtr(data), CFDataGetLength(data))))
                goto finish;
	    if (-1 == lseek(outfd, totalInfoBytes - CFDataGetLength(data), SEEK_CUR)) {
		perror("couldn't write output");
		err = kKXKextManagerErrorFileAccess;
		goto finish;
	    }

	    CFRelease(data);
	}

	totalBytes = totalModuleBytes + totalSymbolBytes + totalInfoBytes;
	if (verbose_level > 0)
	{
	    verbose_log("added 0x%x bytes %.2f Mb (code 0x%x + symbol 0x%x + sets 0x%x - strings 0x%x + info 0x%x)",
			    totalBytes, ((float) totalBytes) / 1024.0 / 1024.0,
			    totalModuleBytes, 
			    totalSymbolBytes, totalSymbolSetBytes, totalSymbolDiscardedBytes,
			    totalInfoBytes);
	}
	fileoffset = totalBytes - prelinkseg->filesize;
	vmoffset   = totalBytes - round_page(prelinkseg->filesize);

	// unswap kernel symbols

	kld_macho_unswap((struct mach_header *)kernel_map, kernel_swapped, 1);

	// write tail of base kernel

	tailoffset = prelinkseg->fileoff + prelinkseg->filesize;

        if (kKXKextManagerErrorNone !=
            (err = writeFile(outfd, (const void *) (kernel_map + tailoffset),  kernel_size - tailoffset)))
            goto finish;

	// patch prelink sects sizes and offsets

	prelinkseg->vmsize     = totalBytes;
	prelinkseg->filesize   = totalBytes;

	prelinksects[0].size   = totalModuleBytes;

	prelinksects[1].addr   = prelinksects[0].addr + totalModuleBytes;
	prelinksects[1].size   = totalSymbolBytes;
	prelinksects[1].offset = prelinksects[0].offset + totalModuleBytes;

	prelinksects[2].addr   = prelinksects[1].addr + totalSymbolBytes;
	prelinksects[2].size   = totalInfoBytes;
	prelinksects[2].offset = prelinksects[1].offset + totalSymbolBytes;

	// patch following segs address & offsets

	seg = prelinkseg;
	for (; cmd < ncmds; cmd++)
	{
	    seg = (struct segment_command *)(((vm_offset_t)seg) + seg->cmdsize);
	    if (LC_SYMTAB == seg->cmd)
	    {
		((struct symtab_command *)seg)->symoff += fileoffset;
		((struct symtab_command *)seg)->stroff += fileoffset;
	    }
	    else if (LC_SEGMENT == seg->cmd)
	    {
		seg->fileoff += fileoffset;
		seg->vmaddr  += vmoffset;
	    }
	}

	bytes = prelinkseg->fileoff;
	
	// unswap kernel headers

	kld_macho_unswap((struct mach_header *)kernel_map, kernel_swapped, -1);
	kernel_swapped = false;
	prelinkseg = 0;

	// write head of base kernel, & free it

	if (-1 == lseek(outfd, 0, SEEK_SET)) {
            perror("couldn't write output");
            err = kKXKextManagerErrorFileAccess;
	    goto finish;
        }
        if (kKXKextManagerErrorNone !=
            (err = writeFile(outfd, (const void *) kernel_map, bytes)))
            goto finish;

	close(outfd);
	outfd = -1;

	vm_deallocate( mach_task_self(), kernel_file, kernel_file_size );
	kernel_file = 0;
	kernel_map  = 0;
    }

    // compresss
    {
	char *    buf;
	char *    bufend;
	vm_size_t bufsize;
	struct {
	    uint32_t  signature;
	    uint32_t  compress_type;
	    uint32_t  adler32;
	    vm_size_t uncompressed_size;
	    vm_size_t compressed_size;
	    uint32_t  reserved[11];
	    char      platform_name[64];
	    char      root_path[256];
	    char      data[0];
	} kernel_header = { 0 };

        if (kKXKextManagerErrorNone !=
            (err = readFile("mach_kernel.prelink", &kernel_file, &kernel_file_size)))
	    goto finish;

	bufsize = kernel_file_size;
	buf = malloc(bufsize);
    
	kernel_header.signature		= NXSwapHostIntToBig('comp');
	kernel_header.compress_type	= NXSwapHostIntToBig('lzss');
	kernel_header.adler32		= NXSwapHostIntToBig(local_adler32(
		    (u_int8_t *) kernel_file, kernel_file_size));
	kernel_header.uncompressed_size = NXSwapHostIntToBig(kernel_file_size);

	strcpy(kernel_header.platform_name, platform_name);
	strcpy(kernel_header.root_path, root_path);

	if (verbose_level > 0)
	    verbose_log("adler32 0x%08x, compressing...", NXSwapBigIntToHost(kernel_header.adler32));
	bufend = compress_lzss(buf, bufsize, (u_int8_t *) kernel_file, kernel_file_size);
	totalBytes = bufend - buf;
	kernel_header.compressed_size = NXSwapHostIntToBig(totalBytes);
	if (verbose_level > 0)
	    verbose_log("final size 0x%x bytes %.2f Mb", totalBytes, ((float) totalBytes) / 1024.0 / 1024.0);

	vm_deallocate( mach_task_self(), kernel_file, kernel_file_size );

	outfd = open(temp_file, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (-1 == outfd) {
	    fprintf(stderr, "can't create %s - %s\n", temp_file,
		strerror(errno));
            err = kKXKextManagerErrorFileAccess;
	    goto finish;
	}

	// write header
        if (kKXKextManagerErrorNone !=
            (err = writeFile(outfd, &kernel_header, sizeof(kernel_header))))
            goto finish;
	// write compressed data
        if (kKXKextManagerErrorNone !=
            (err = writeFile(outfd, buf, bufend - buf)))
            goto finish;

	close(outfd);
	outfd = -1;
    }

    // move it to the final destination
    if (-1 == rename(temp_file, kernelCacheFilename)) {
	fprintf(stderr, "can't create file %s: %s\n", kernelCacheFilename,
		strerror(errno));
	err = kKXKextManagerErrorFileAccess;
	goto finish;
    }

    // give the cache file the mod time of the source files when kextcache started
    if (cacheFileTimes && (-1 == utimes(kernelCacheFilename, cacheFileTimes))) {
	fprintf(stderr, "can't set file times %s: %s\n", kernelCacheFilename,
		strerror(errno));
	err = kKXKextManagerErrorFileAccess;
	goto finish;
    }

    if (verbose_level > 0)
	verbose_log("created cache: %s", kernelCacheFilename);

    result = kKXKextManagerErrorNone;

finish:

    if (-1 != outfd)
	close(outfd);

    if (debug_mode && symbol_dir)
	symbol_dir[0] = 0;

    if (symbol_dir && symbol_dir[0])
    {
        FTS *    fts;
        FTSENT * fts_entry;
        char *   paths[2];
    
        paths[0] = symbol_dir;
        paths[1] = 0;
        fts = fts_open(paths, FTS_NOCHDIR, NULL);
        if (fts) 
        {
            while ((fts_entry = fts_read(fts)))
            {
                if (fts_entry->fts_errno)
                    continue;
                if (FTS_F != fts_entry->fts_info)
                    continue;
        
                if (-1 == unlink(fts_entry->fts_path))
                    fprintf(stderr, "can't remove file %s: %s\n", 
                                fts_entry->fts_path, strerror(errno));
            }
            fts_close(fts);
        }
    }

    if (-1 != curwd)
        fchdir(curwd);
    if (symbol_dir && symbol_dir[0] && (-1 == rmdir(symbol_dir)))
        perror("rmdir");

    if (kexts)
	free(kexts);
    if (files)
	free(files);
    if (symbol_dir)
        free(symbol_dir);

    return result;
}

__private_extern__ KXKextManagerError
writeFile(int fd, const void * data, vm_size_t length)
{
    KXKextManagerError err;

    if (length != (vm_size_t) write(fd, data, length))
        err = kKXKextManagerErrorDiskFull;
    else
        err = kKXKextManagerErrorNone;

    if (kKXKextManagerErrorNone != err)
        perror("couldn't write output");

    return( err );
}

__private_extern__ KXKextManagerError
readFile(const char *path, vm_offset_t * objAddr, vm_size_t * objSize)
{
    KXKextManagerError err = kKXKextManagerErrorFileAccess;
    int fd;
    struct stat stat_buf;

    *objAddr = 0;
    *objSize = 0;

    do
    {
        if((fd = open(path, O_RDONLY)) == -1)
	    continue;

	if(fstat(fd, &stat_buf) == -1)
	    continue;

        if (0 == (stat_buf.st_mode & S_IFREG)) 
            continue;

	*objSize = stat_buf.st_size;

	if( KERN_SUCCESS != map_fd(fd, 0, objAddr, TRUE, *objSize)) {
            *objAddr = 0;
            *objSize = 0;
	    continue;
	}

	err = kKXKextManagerErrorNone;

    } while( false );

    if (-1 != fd)
    {
        close(fd);
    }
    if (kKXKextManagerErrorNone != err)
    {
        fprintf(stderr, "couldn't read %s: %s\n", path, strerror(errno));
    }

    return( err );
}

static CFMutableDictionaryRef copyInfoDict(CFBundleRef bundle)
{
    CFMutableDictionaryRef infoDict = NULL;
    CFURLRef		   infoDictURL;
    CFDataRef		   data;

    infoDictURL = _CFBundleCopyInfoPlistURL(bundle);
    if (infoDictURL)
    {
	if (CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, infoDictURL, &data,
		    NULL, NULL, NULL))
	{
	    infoDict = (CFMutableDictionaryRef) CFPropertyListCreateFromXMLData(kCFAllocatorDefault,
						data, kCFPropertyListMutableContainers, NULL);
	}
	CFRelease(infoDictURL);
    }
    return( infoDict );
}
