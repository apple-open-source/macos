/*
 * Copyright (c) 2025 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "utils.h"
#include "region.h"

#include <mach/mach.h>
#include <mach/mach_vm.h>

#include <sys/sysctl.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <libutil.h>
#include <errno.h>

void
err_mach(kern_return_t kr, const struct region *r, const char *fmt, ...)
{
    const size_t size = 1024;
    char buf[size];
    int count = 0;

    va_list ap;
    va_start(ap, fmt);
    if (0 != kr) {
        count += snprintf(buf + count, size - count, "%s: ", pgm);
    }
    if (NULL != r) {
        count += snprintf(buf + count, size - count,
            "%016llx-%016llx ", R_ADDR(r), R_ENDADDR(r));
    }
    count += vsnprintf(buf + count, size - count, fmt, ap);
    va_end(ap);

    if (0 != kr) {
        count += snprintf(buf + count, size - count,
            ": failed: %s (0x%x)", mach_error_string(kr), kr);
        switch (err_get_system(kr)) {
            case err_get_system(err_mach_ipc):
                /* 0x10000000  == (4 << 26) */
                snprintf(buf + count, size - count, " => fatal\n");
                os_log_error(glog, "%{public}s", buf);
                exit(EX_OSERR);
            default:
                break;
        }
    }
    os_log_error(glog, "%{public}s", buf);
}

static void
vprintvr(const struct vm_range *vr, const char *restrict fmt, va_list ap)
{
    if (NULL != vr) {
        printf("%016llx-%016llx ", V_ADDR(vr), V_ENDADDR(vr));
    }
	vprintf(fmt, ap);
}

void
printvr(const struct vm_range *vr, const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vprintvr(vr, fmt, ap);
	va_end(ap);
}

void
printr(const struct region *r, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
	vprintvr(R_RANGE(r), fmt, ap);
    va_end(ap);
}

/*
 * Print power-of-1024 sizes in human-readable form
 */
const char *
str_hsize(hsize_str_t hstr, uint64_t size)
{
    humanize_number(hstr, sizeof (hsize_str_t) - 1, size, "",
        HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL | HN_IEC_PREFIXES);
    return hstr;
}

/*
 * Print VM protections in human-readable form
 */
const char *
str_prot(const vm_prot_t prot)
{
	static const char *pstr[] = {
		[0]												= "---",
		[VM_PROT_READ]									= "r--",
		[VM_PROT_WRITE]									= "-w-",
		[VM_PROT_READ|VM_PROT_WRITE]					= "rw-",
		[VM_PROT_EXECUTE]								= "--x",
		[VM_PROT_READ|VM_PROT_EXECUTE]					= "r-x",
		[VM_PROT_WRITE|VM_PROT_EXECUTE]					= "-wx",
		[VM_PROT_READ|VM_PROT_WRITE|VM_PROT_EXECUTE]	= "rwx"
	};
	return pstr[prot & 7];
}

// c.f. VMUVMRegion.m

const char *
str_shared(int sm)
{
	static const char *sstr[] = {
		[0]						= "      ",
		[SM_COW]				= "sm=cow",
		[SM_PRIVATE]			= "sm=prv",
		[SM_EMPTY]				= "sm=nul",
		[SM_SHARED]				= "sm=ali",
		[SM_TRUESHARED]			= "sm=shm",
		[SM_PRIVATE_ALIASED]	= "sm=zer",
		[SM_SHARED_ALIASED]		= "sm=s/a",
		[SM_LARGE_PAGE]			= "sm=lpg",
	};
    if ((unsigned)sm < sizeof (sstr) / sizeof (sstr[0])) {
        return sstr[sm];
    }
	return "sm=???";
}

const char *
str_purgable(int pu, int sm)
{
	if (SM_EMPTY == sm)
		return "   ";
	static const char *pstr[] = {
		[VM_PURGABLE_NONVOLATILE]	= "p=n",
		[VM_PURGABLE_VOLATILE]		= "p=v",
		[VM_PURGABLE_EMPTY]			= "p=e",
		[VM_PURGABLE_DENY]			= "   ",
	};
    if ((unsigned)pu < sizeof (pstr) / sizeof (pstr[0])) {
        return pstr[pu];
    }
	return "p=?";
}

/*
 * c.f. VMURegionTypeDescriptionForTagShareProtAndPager.
 */
const char *
str_tag(tag_str_t tstr, const struct region *r)
{
	const char *rtype;

    switch (r->r_info.user_tag) {
		case 0:
            if (r->r_info.external_pager) {
                rtype = "mapped file";
            } else if (r->r_incommregion) {
                rtype = "commpage";
            } else if (SM_TRUESHARED == r->r_info.share_mode) {
                rtype = "shared memory";
            } else {
                rtype = "VM_allocate";
            }
			break;
		case VM_MEMORY_MALLOC:
            rtype = "MALLOC";
			break;
        case VM_MEMORY_STACK:
            rtype = "Stack";
            break;
        case (uint32_t)-1:
            rtype = "Owned unmapped";
            break;
#if defined(CONFIG_DEBUG)
        case VM_MEMORY_MALLOC_SMALL:
            rtype = "MALLOC_SMALL";
			break;
		case VM_MEMORY_MALLOC_LARGE:
			rtype = "MALLOC_LARGE";
			break;
		case VM_MEMORY_MALLOC_HUGE:
			rtype = "MALLOC_HUGE";
			break;
		case VM_MEMORY_SBRK:
			rtype = "SBRK";
			break;
		case VM_MEMORY_REALLOC:
			rtype = "MALLOC_REALLOC";
			break;
		case VM_MEMORY_MALLOC_TINY:
			rtype = "MALLOC_TINY";
			break;
		case VM_MEMORY_MALLOC_LARGE_REUSABLE:
			rtype = "MALLOC_LARGE_REUSABLE";
			break;
		case VM_MEMORY_MALLOC_LARGE_REUSED:
			rtype = "MALLOC_LARGE";
			break;
		case VM_MEMORY_ANALYSIS_TOOL:
			rtype = "Performance tool data";
			break;
		case VM_MEMORY_MALLOC_NANO:
			rtype = "MALLOC_NANO";
			break;
        case VM_MEMORY_MALLOC_MEDIUM:
            rtype = "MALLOC_MEDIUM";
            break;
        case VM_MEMORY_MALLOC_PROB_GUARD:
            rtype = "MALLOC_PROB_GUARD";
            break;
		case VM_MEMORY_MACH_MSG:
			rtype = "Mach message";
			break;
		case VM_MEMORY_IOKIT:
			rtype = "IOKit";
			break;
		case VM_MEMORY_GUARD:
			rtype = "Guard";
			break;
		case VM_MEMORY_SHARED_PMAP:
			rtype = "shared pmap";
			break;
		case VM_MEMORY_DYLIB:
			rtype = "dylib";
			break;
		case VM_MEMORY_OBJC_DISPATCHERS:
			rtype = "ObjC dispatching code";
			break;
		case VM_MEMORY_UNSHARED_PMAP:
			rtype = "unshared pmap";
			break;
		case VM_MEMORY_APPKIT:
			rtype = "AppKit";
			break;
		case VM_MEMORY_FOUNDATION:
			rtype = "Foundation";
			break;
		case VM_MEMORY_COREGRAPHICS:
			rtype = "CoreGraphics";
			break;
		case VM_MEMORY_CORESERVICES:
			rtype = "CoreServices";
			break;
		case VM_MEMORY_JAVA:
			rtype = "Java";
			break;
		case VM_MEMORY_COREDATA:
			rtype = "CoreData";
			break;
		case VM_MEMORY_COREDATA_OBJECTIDS:
			rtype = "CoreData Object IDs";
			break;
		case VM_MEMORY_ATS:
			rtype = "ATS (font support)";
			break;
		case VM_MEMORY_LAYERKIT:
			rtype = "CoreAnimation";
			break;
		case VM_MEMORY_CGIMAGE:
			rtype = "CG image";
			break;
		case VM_MEMORY_TCMALLOC:
			rtype = "WebKit Malloc";
			break;
		case VM_MEMORY_COREGRAPHICS_DATA:
			rtype = "CG raster data";
			break;
		case VM_MEMORY_COREGRAPHICS_SHARED:
			rtype = "CG shared images";
			break;
		case VM_MEMORY_COREGRAPHICS_FRAMEBUFFERS:
			rtype = "CG framebuffers";
			break;
		case VM_MEMORY_COREGRAPHICS_BACKINGSTORES:
			rtype = "CG backingstores";
			break;
		case VM_MEMORY_DYLD:
			rtype = "dyld private memory";
			break;
		case VM_MEMORY_DYLD_MALLOC:
			rtype = "dyld malloc memory";
			break;
		case VM_MEMORY_SQLITE:
			rtype = "SQlite page cache";
			break;
		case VM_MEMORY_JAVASCRIPT_CORE:
			rtype = "WebAssembly memory";
			break;
		case VM_MEMORY_JAVASCRIPT_JIT_EXECUTABLE_ALLOCATOR:
			rtype = "JS JIT generated code";
			break;
		case VM_MEMORY_JAVASCRIPT_JIT_REGISTER_FILE:
			rtype = "JS VM register file";
			break;
		case VM_MEMORY_GLSL:
			rtype = "OpenGL GLSL";
			break;
		case VM_MEMORY_OPENCL:
			rtype = "OpenCL";
			break;
		case VM_MEMORY_COREIMAGE:
			rtype = "CoreImage";
			break;
		case VM_MEMORY_WEBCORE_PURGEABLE_BUFFERS:
			rtype = "WebCore purgable data";
			break;
		case VM_MEMORY_IMAGEIO:
			rtype = "Image IO";
			break;
		case VM_MEMORY_COREPROFILE:
			rtype = "CoreProfile";
			break;
		case VM_MEMORY_ASSETSD:
			rtype = "Assets Library";
			break;
		case VM_MEMORY_OS_ALLOC_ONCE:
            // libplatform and libpthread, zfod
			rtype = "OS Alloc Once";
			break;
		case VM_MEMORY_LIBDISPATCH:
			rtype = "Dispatch continuations";
			break;
		case VM_MEMORY_ACCELERATE:
			rtype = "Accelerate framework";
			break;
		case VM_MEMORY_COREUI:
			rtype = "CoreUI image data";
			break;
		case VM_MEMORY_COREUIFILE:
			rtype = "CoreUI image file";
			break;
		case VM_MEMORY_GENEALOGY:
			rtype = "Activity Tracing";
			break;
		case VM_MEMORY_RAWCAMERA:
			rtype = "RawCamera";
			break;
		case VM_MEMORY_CORPSEINFO:
			rtype = "Process Corpse Info";
			break;
		case VM_MEMORY_ASL:
			rtype = "Apple System Log";
			break;
		case VM_MEMORY_SWIFT_RUNTIME:
			rtype = "Swift runtime";
			break;
		case VM_MEMORY_SWIFT_METADATA:
			rtype = "Swift metadata";
			break;
		case VM_MEMORY_DHMM:
			rtype = "DHMM";
			break;
		case VM_MEMORY_SCENEKIT:
			rtype = "SceneKit";
			break;
        case VM_MEMORY_DFR:
            rtype = "DFR";
            break;
		case VM_MEMORY_SKYWALK:
			rtype = "Skywalk Networking";
			break;
        case VM_MEMORY_IOSURFACE:
            rtype = "IO Surface";
            break;
        case VM_MEMORY_LIBNETWORK:
            rtype = "libnetwork";
            break;
        case VM_MEMORY_AUDIO:
            rtype = "Audio";
            break;
        case VM_MEMORY_VIDEOBITSTREAM:
            rtype = "Video Bitstream";
            break;
        case VM_MEMORY_CM_XPC:
            rtype = "CoreMedia XPC";
            break;
        case VM_MEMORY_CM_RPC:
            rtype = "CoreMedia RPC";
            break;
        case VM_MEMORY_CM_MEMORYPOOL:
            rtype = "CoreMedia Memory Pool";
            break;
        case VM_MEMORY_CM_READCACHE:
            rtype = "CoreMedia Readcache";
            break;
        case VM_MEMORY_CM_CRABS:
            rtype = "CoreMedia Crabs";
            break;
        case VM_MEMORY_QUICKLOOK_THUMBNAILS:
            rtype = "Quicklook Thumbnails";
            break;
        case VM_MEMORY_ACCOUNTS:
            rtype = "Accounts";
            break;
        case VM_MEMORY_SANITIZER:
            rtype = "Sanitizer";
            break;
        case VM_MEMORY_IOACCELERATOR:
            rtype = "IO Accelerator";
            break;
        case VM_MEMORY_CM_REGWARP:
            rtype = "CoreMedia Regwarp";
            break;
        case VM_MEMORY_EAR_DECODER:
            rtype = "Embedded Acoustic Recognition Decoder";
            break;
        case VM_MEMORY_COREUI_CACHED_IMAGE_DATA:
            rtype = "CoreUI Cached Image Data";
            break;
        case VM_MEMORY_COLORSYNC:
            rtype = "ColorSync";
            break;
        case VM_MEMORY_BTINFO:
            rtype = "BT Info";
            break;
        case VM_MEMORY_CM_HLS:
            rtype = "CoreMedia HLS";
            break;
        case VM_MEMORY_ROSETTA:
            rtype = "Rosetta";
            break;
        case VM_MEMORY_ROSETTA_THREAD_CONTEXT:
            rtype = "Rosetta Thread Context";
            break;
        case VM_MEMORY_ROSETTA_INDIRECT_BRANCH_MAP:
            rtype = "Rosetta Indirect Branch Map";
            break;
        case VM_MEMORY_ROSETTA_RETURN_STACK:
            rtype = "Rosetta Return Stack";
            break;
        case VM_MEMORY_ROSETTA_EXECUTABLE_HEAP:
            rtype = "Rosetta Executable Heap";
            break;
        case VM_MEMORY_ROSETTA_USER_LDT:
            rtype = "Rosetta User LDT";
            break;
        case VM_MEMORY_ROSETTA_ARENA:
            rtype = "Rosetta Arena";
            break;
        case VM_MEMORY_ROSETTA_10:
            rtype = "Rosetta 10";
            break;
        case VM_MEMORY_APPLICATION_SPECIFIC_1 ... VM_MEMORY_APPLICATION_SPECIFIC_16:
            snprintf(tstr, sizeof(tag_str_t), "App Specific %u",
                r->r_info.user_tag - VM_MEMORY_APPLICATION_SPECIFIC_1 + 1);
            return tstr;
#endif
		default:
            // VM_MAKE_TAG() squeezes the tag into the top 8 bits.
            // However, user_tag is a uint32_t, so it can ultimately
            // have more bits
            snprintf(tstr, sizeof (tag_str_t), "tag #%u/0x%x",
                (uint8_t)r->r_info.user_tag, r->r_info.user_tag);
            return tstr;
    }
    snprintf(tstr, sizeof (tag_str_t), "%s", rtype);
    return tstr;
}

int
bounded_pwrite(int fd, const void *addr, size_t size, off_t off, bool *nocache, ssize_t *nwrittenp)
{
    if (opt->sizebound && off + (off_t)size > opt->sizebound) {
        if (OPTIONS_DEBUG(opt, 1)) {
            // Really shouldn't happen unless dump size estimate is wrong!
            printf("offset %lld will exceed bound %lld\n",
                off + (off_t)size, opt->sizebound);
        }
        return EFBIG;
    }

	bool oldnocache = *nocache;
    if (size >= opt->ncthresh && !oldnocache) {
        *nocache = 0 == fcntl(fd, F_NOCACHE, 1);
    } else if (size < opt->ncthresh && oldnocache) {
        *nocache = 0 != fcntl(fd, F_NOCACHE, 0);
    }
    if (OPTIONS_DEBUG(opt, 3) && oldnocache ^ *nocache) {
        printf("F_NOCACHE now %sabled on fd %d\n", *nocache ? "en" : "dis", fd);
    }
	const ssize_t nwritten = pwrite(fd, addr, size, off);
    if (-1 == nwritten) {
        return errno;
    }
    if (nwrittenp) {
        *nwrittenp = nwritten;
    }
	return 0;
}

int
bounded_write(int fd, const void *addr, size_t size, ssize_t *nwrittenp)
{
    const ssize_t nwritten = write(fd, addr, size);
    if (-1 == nwritten) {
        return errno;
    }
    if (nwrittenp) {
        *nwrittenp = nwritten;
    }
    return 0;
}

int
bounded_write_zero(int fd, size_t size, ssize_t *nwrittenp)
{
    void *zero = calloc(1, size);
    if (zero) {
        const ssize_t nwritten = write(fd, zero, size);
        if (-1 == nwritten) {
            const int error = errno;
            free(zero);
            return error;
        }
        if (nwrittenp) {
            *nwrittenp = nwritten;
        }
        free(zero);
        return 0;
    }
    return ENOMEM;
}

bool
task_port_is_corpse(mach_port_t task_port)
{
    mach_vm_address_t kcd_addr_begin;
    mach_vm_size_t kcd_size;
    
    kern_return_t kr = task_map_corpse_info_64(mach_task_self(),
        task_port, &kcd_addr_begin, &kcd_size);
    if (kr != KERN_SUCCESS) {
        return false;
    }
    mach_vm_deallocate(mach_task_self(), kcd_addr_begin, kcd_size);
    return true;
}

int
virtual_address_size(uint32_t *abitsp)
{
    static uint32_t abits = 0;
    if (abits == 0) {
        size_t abits_size = sizeof(abits);
        if (sysctlbyname("machdep.virtual_address_size",
            &abits, &abits_size, NULL, 0) != 0) {
            return errno;
        }
        assert(sizeof(abits) == abits_size);
        assert(abits > 0);
    }
    *abitsp = abits;
    return 0;
}
