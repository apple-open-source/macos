/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "utils.h"
#include "region.h"

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
    va_list ap;
    va_start(ap, fmt);
    if (0 != kr)
        printf("%s: ", pgm);
    if (NULL != r)
        printf("%016llx-%016llx ", R_ADDR(r), R_ENDADDR(r));
    vprintf(fmt, ap);
    va_end(ap);

    if (0 != kr) {
        printf(": failed: %s (0x%x)", mach_error_string(kr), kr);
        switch (err_get_system(kr)) {
            case err_get_system(err_mach_ipc):
                /* 0x10000000  == (4 << 26) */
                printf(" => fatal\n");
                exit(127);
            default:
                putchar('\n');
                break;
        }
    } else
        putchar('\n');
}

static void
vprintvr(const struct vm_range *vr, const char *restrict fmt, va_list ap)
{
	if (NULL != vr)
		printf("%016llx-%016llx ", V_ADDR(vr), V_ENDADDR(vr));
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
	if ((unsigned)sm < sizeof (sstr) / sizeof (sstr[0]))
		return sstr[sm];
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
	if ((unsigned)pu < sizeof (pstr) / sizeof (pstr[0]))
		return pstr[pu];
	return "p=?";
}

/*
 * c.f. VMURegionTypeDescriptionForTagShareProtAndPager.
 */
const char *
str_tag(tag_str_t tstr, int tag, int share_mode, vm_prot_t curprot, int external_pager)
{
	const char *rtype;

	switch (tag) {
		case 0:
			if (external_pager)
				rtype = "mapped file";
			else if (SM_TRUESHARED == share_mode)
				rtype = "shared memory";
			else
				rtype = "VM_allocate";
			break;
		case VM_MEMORY_MALLOC:
			if (VM_PROT_NONE == curprot)
				rtype = "MALLOC guard page";
			else if (SM_EMPTY == share_mode)
				rtype = "MALLOC";
			else
				rtype = "MALLOC metadata";
			break;
        case VM_MEMORY_STACK:
            if (VM_PROT_NONE == curprot)
                rtype = "Stack guard";
            else
                rtype = "Stack";
            break;
#if defined(CONFIG_DEBUG) || defined(CONFIG_GCORE_MAP)
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
		case VM_MEMORY_SKYWALK:
			rtype = "Skywalk Networking";
			break;
#endif
		default:
            rtype = NULL;
            break;
    }
    if (rtype)
        snprintf(tstr, sizeof (tag_str_t), "%s", rtype);
    else
        snprintf(tstr, sizeof (tag_str_t), "tag #%d", tag);
    return tstr;
}

const char *
str_tagr(tag_str_t tstr, const struct region *r) {
    return str_tag(tstr, r->r_info.user_tag, r->r_info.share_mode, r->r_info.protection, r->r_info.external_pager);
}

/*
 * Put two strings together separated by a '+' sign
 * If the string gets too long, then add an ellipsis and
 * stop concatenating it.
 */
char *
strconcat(const char *s0, const char *s1, size_t maxlen)
{
    const char ellipsis[] = "...";
    const char junction[] = ", ";
    const size_t s0len = strlen(s0);
    size_t nmlen = s0len + strlen(s1) + strlen(junction) + 1;
    if (maxlen > strlen(ellipsis) && nmlen > maxlen) {
        if (strcmp(s0 + s0len - strlen(ellipsis), ellipsis) == 0)
            return strdup(s0);
        s1 = ellipsis;
        nmlen = s0len + strlen(s1) + strlen(junction) + 1;
    }
    char *p = malloc(nmlen);
    if (p) {
        strlcpy(p, s0, nmlen);
        strlcat(p, junction, nmlen);
        strlcat(p, s1, nmlen);
    }
    return p;
}

unsigned long
simple_namehash(const char *nm)
{
	unsigned long result = 5381;
	int c;
	while (0 != (c = *nm++))
		result = (result * 33) ^ c;
	return result;  /* modified djb2 */
}

int
bounded_pwrite(int fd, const void *addr, size_t size, off_t off, bool *nocache, ssize_t *nwrittenp)
{
	if (opt->sizebound && off + (off_t)size > opt->sizebound)
		return EFBIG;

	bool oldnocache = *nocache;
	if (size >= opt->ncthresh && !oldnocache)
		*nocache = 0 == fcntl(fd, F_NOCACHE, 1);
	else if (size < opt->ncthresh && oldnocache)
		*nocache = 0 != fcntl(fd, F_NOCACHE, 0);
	if (OPTIONS_DEBUG(opt, 3) && oldnocache ^ *nocache)
		printf("F_NOCACHE now %sabled on fd %d\n", *nocache ? "en" : "dis", fd);

	const ssize_t nwritten = pwrite(fd, addr, size, off);
	if (-1 == nwritten)
		return errno;
	if (nwrittenp)
		*nwrittenp = nwritten;
	return 0;
}
