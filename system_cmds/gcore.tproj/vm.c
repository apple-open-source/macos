/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "vm.h"
#include "utils.h"
#include "region.h"
#include "sparse.h"

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/queue.h>

/*
 * There should be better APIs to describe the shared region
 * For now, some hackery.
 */

#include <mach/shared_region.h>

static __inline boolean_t
in_shared_region(mach_vm_address_t addr)
{
    const mach_vm_address_t base = SHARED_REGION_BASE;
    const mach_vm_address_t size = SHARED_REGION_SIZE;
    return addr >= base && addr < (base + size);
}

/*
 * On both x64 and arm, there's a globallly-shared
 * read-only page at _COMM_PAGE_START_ADDRESS
 * which low-level library routines reference.
 *
 * On x64, somewhere randomly chosen between _COMM_PAGE_TEXT_ADDRESS
 * and the top of the user address space, there's the
 * pre-emption-free-zone read-execute page.
 */

#include <System/machine/cpu_capabilities.h>

static __inline boolean_t
in_comm_region(const mach_vm_address_t addr, const vm_region_submap_info_data_64_t *info)
{
    return addr >= _COMM_PAGE_START_ADDRESS &&
        SM_TRUESHARED == info->share_mode &&
        VM_INHERIT_SHARE == info->inheritance &&
        !info->external_pager && (info->max_protection & VM_PROT_WRITE) == 0;
}

static __inline boolean_t
in_zfod_region(const vm_region_submap_info_data_64_t *info)
{
    return info->share_mode == SM_EMPTY && !info->is_submap &&
        0 == info->object_id && !info->external_pager &&
        0 == info->pages_dirtied + info->pages_resident + info->pages_swapped_out;
}

static struct region *
new_region(mach_vm_offset_t vmaddr, mach_vm_size_t vmsize, const vm_region_submap_info_data_64_t *infop)
{
    struct region *r = calloc(1, sizeof (*r));
    assert(vmaddr != 0 && vmsize != 0);
    R_SETADDR(r, vmaddr);
    R_SETSIZE(r, vmsize);
    r->r_info = *infop;
    r->r_purgable = VM_PURGABLE_DENY;
    r->r_insharedregion = in_shared_region(vmaddr);
    r->r_incommregion = in_comm_region(vmaddr, &r->r_info);
    r->r_inzfodregion = in_zfod_region(&r->r_info);

    if (r->r_inzfodregion)
        r->r_op = &zfod_ops;
    else
        r->r_op = &vanilla_ops;
    return r;
}

void
del_fileref_region(struct region *r)
{
    assert(&fileref_ops == r->r_op);
    /* r->r_fileref->fr_libent is a reference into the name table */
    poison(r->r_fileref, 0xdeadbee9, sizeof (*r->r_fileref));
    free(r->r_fileref);
    poison(r, 0xdeadbeeb, sizeof (*r));
    free(r);
}

void
del_zfod_region(struct region *r)
{
    assert(&zfod_ops == r->r_op);
    assert(r->r_inzfodregion && 0 == r->r_nsubregions);
    assert(NULL == r->r_fileref);
    poison(r, 0xdeadbeed, sizeof (*r));
    free(r);
}

void
del_vanilla_region(struct region *r)
{
    assert(&vanilla_ops == r->r_op);
    assert(!r->r_inzfodregion && 0 == r->r_nsubregions);
    assert(NULL == r->r_fileref);
    poison(r, 0xdeadbeef, sizeof (*r));
    free(r);
}

/*
 * "does any part of this address range match the tag?"
 */
int
is_tagged(task_t task, mach_vm_offset_t addr, mach_vm_offset_t size, unsigned tag)
{
    mach_vm_offset_t vm_addr = addr;
    mach_vm_offset_t vm_size = 0;
    natural_t depth = 0;
    size_t pgsize = (1u << pageshift_host);

    do {
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        vm_region_submap_info_data_64_t info;

        kern_return_t ret = mach_vm_region_recurse(task, &vm_addr, &vm_size, &depth, (vm_region_recurse_info_t)&info, &count);

        if (KERN_FAILURE == ret) {
            err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
            return -1;
        } else if (KERN_INVALID_ADDRESS == ret) {
            err_mach(ret, NULL, "invalid address at %llx", vm_addr);
            return -1;
        } else if (KERN_SUCCESS != ret) {
            err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
            return -1;
        }
        if (info.is_submap) {
            depth++;
            continue;
        }
        if (info.user_tag == tag)
            return 1;
        if (vm_addr + vm_size > addr + size)
            return 0;
        vm_addr += pgsize;
    } while (1);
}

STAILQ_HEAD(regionhead, region);

/*
 * XXX Need something like mach_vm_shared_region_recurse()
 * to properly identify the shared region address ranges as
 * we go.
 */

static int
walk_regions(task_t task, struct regionhead *rhead)
{
    mach_vm_offset_t vm_addr = MACH_VM_MIN_ADDRESS;
    natural_t depth = 0;

	if (OPTIONS_DEBUG(opt, 3)) {
		printf("Building raw region list\n");
        print_memory_region_header();
	}
    while (1) {
        vm_region_submap_info_data_64_t info;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        mach_vm_size_t vm_size;

        kern_return_t ret = mach_vm_region_recurse(task, &vm_addr, &vm_size, &depth, (vm_region_recurse_info_t)&info, &count);

        if (KERN_FAILURE == ret) {
            err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
            goto bad;
        } else if (KERN_INVALID_ADDRESS == ret) {
            break;  /* loop termination */
        } else if (KERN_SUCCESS != ret) {
            err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
            goto bad;
        }

        if (OPTIONS_DEBUG(opt, 3)) {
            struct region *d = new_region(vm_addr, vm_size, &info);
            ROP_PRINT(d);
            ROP_DELETE(d);
        }

        if (info.is_submap) {
#ifdef CONFIG_SUBMAP
            /* We also want to see submaps -- for debugging purposes. */
            struct region *r = new_region(vm_addr, vm_size, &info);
            r->r_depth = depth;
            STAILQ_INSERT_TAIL(rhead, r, r_linkage);
#endif
            depth++;
            continue;
        }

        if (VM_MEMORY_IOKIT == info.user_tag) {
            vm_addr += vm_size;
            continue; // ignore immediately: IO memory has side-effects
        }

        struct region *r = new_region(vm_addr, vm_size, &info);
#ifdef CONFIG_SUBMAP
        r->r_depth = depth;
#endif
        /* grab the page info of the first page in the mapping */

        mach_msg_type_number_t pageinfoCount = VM_PAGE_INFO_BASIC_COUNT;
        ret = mach_vm_page_info(task, R_ADDR(r), VM_PAGE_INFO_BASIC, (vm_page_info_t)&r->r_pageinfo, &pageinfoCount);
        if (KERN_SUCCESS != ret)
            err_mach(ret, r, "getting pageinfo at %llx", R_ADDR(r));

        /* record the purgability */

        ret = mach_vm_purgable_control(task, vm_addr, VM_PURGABLE_GET_STATE, &r->r_purgable);
        if (KERN_SUCCESS != ret)
            r->r_purgable = VM_PURGABLE_DENY;

		STAILQ_INSERT_TAIL(rhead, r, r_linkage);

        vm_addr += vm_size;
    }

    return 0;
bad:
    return EX_OSERR;
}

void
del_region_list(struct regionhead *rhead)
{
    struct region *r, *t;

    STAILQ_FOREACH_SAFE(r, rhead, r_linkage, t) {
        STAILQ_REMOVE(rhead, r, region, r_linkage);
        ROP_DELETE(r);
    }
    free(rhead);
}

struct regionhead *
build_region_list(task_t task)
{
    struct regionhead *rhead = malloc(sizeof (*rhead));
    STAILQ_INIT(rhead);
    if (0 != walk_regions(task, rhead)) {
        del_region_list(rhead);
        return NULL;
    }
    return rhead;
}

int
walk_region_list(struct regionhead *rhead, walk_region_cbfn_t cbfn, void *arg)
{
    struct region *r, *t;

    STAILQ_FOREACH_SAFE(r, rhead, r_linkage, t) {
        switch (cbfn(r, arg)) {
            case WALK_CONTINUE:
                break;
            case WALK_DELETE_REGION:
                STAILQ_REMOVE(rhead, r, region, r_linkage);
                ROP_DELETE(r);
                break;
            case WALK_TERMINATE:
                goto done;
            case WALK_ERROR:
                return -1;
        }
    }
done:
    return 0;
}

int pageshift_host;
int pageshift_app;

void
setpageshift(void)
{
    if (0 == pageshift_host) {
        vm_size_t hps = 0;
        kern_return_t ret = host_page_size(MACH_PORT_NULL, &hps);
        if (KERN_SUCCESS != ret || hps == 0)
            err_mach(ret, NULL, "host page size");
        int pshift = 0;
        while (((vm_offset_t)1 << pshift) != hps)
            pshift++;
        pageshift_host = pshift;
    }
    if (OPTIONS_DEBUG(opt, 3))
        printf("host page size: %lu\n", 1ul << pageshift_host);

    if (0 == pageshift_app) {
        size_t psz = getpagesize();
        int pshift = 0;
        while ((1ul << pshift) != psz)
            pshift++;
        pageshift_app = pshift;
    }
    if (OPTIONS_DEBUG(opt, 3) && pageshift_app != pageshift_host)
        printf("app page size: %lu\n", 1ul << pageshift_app);
}

void
print_memory_region_header(void)
{
    printf("%-33s %c %-7s %-7s %8s %16s ",
           "Address Range", 'S', "Size", "Cur/Max", "Obj32", "FirstPgObjectID");
    printf("%9s %-3s %-11s %5s ",
           "Offset", "Tag", "Mode", "Refc");
#ifdef CONFIG_SUBMAP
    printf("%5s ", "Depth");
#endif
    printf("%5s %5s %5s %3s ",
           "Res", "SNP", "Dirty", "Pgr");
    printf("\n");
}

static __inline char
region_type(const struct region *r)
{
    if (r->r_fileref)
        return 'f';
    if (r->r_inzfodregion)
        return 'z';
    if (r->r_incommregion)
        return 'c';
    if (r->r_insharedregion)
        return 's';
    return ' ';
}

void
print_memory_region(const struct region *r)
{
    hsize_str_t hstr;
    tag_str_t tstr;

    printf("%016llx-%016llx %c %-7s %s/%s %8x %16llx ",
           R_ADDR(r), R_ENDADDR(r), region_type(r),
           str_hsize(hstr, R_SIZE(r)),
           str_prot(r->r_info.protection),
           str_prot(r->r_info.max_protection),
           r->r_info.object_id, r->r_pageinfo.object_id
           );

    printf("%9lld %3d %-11s %5u ",
           r->r_info.external_pager ?
           r->r_pageinfo.offset : r->r_info.offset,
           r->r_info.user_tag,
           str_shared(r->r_info.share_mode),
           r->r_info.ref_count
           );
#ifdef CONFIG_SUBMAP
    printf("%5u ", r->r_depth);
#endif

    if (!r->r_info.is_submap) {
        printf("%5u %5u %5u %3s ",
               r->r_info.pages_resident,
               r->r_info.pages_shared_now_private,
               r->r_info.pages_dirtied,
               r->r_info.external_pager ? "ext" : "");
		if (r->r_fileref)
            printf("\n    %s at %lld ",
                   r->r_fileref->fr_pathname,
                   r->r_fileref->fr_offset);
		else
			printf("%s", str_tagr(tstr, r));
        printf("\n");
        if (r->r_nsubregions) {
            printf("    %-33s %7s %12s\t%s\n",
                   "Address Range", "Size", "Type(s)", "Filename(s)");
            for (unsigned i = 0; i < r->r_nsubregions; i++) {
                struct subregion *s = r->r_subregions[i];
                printf("    %016llx-%016llx %7s %12s\t%s\n",
                       S_ADDR(s), S_ENDADDR(s),
                       str_hsize(hstr, S_SIZE(s)),
                       S_MACHO_TYPE(s),
                       S_FILENAME(s));
            }
        }
	} else {
		printf("%5s %5s %5s %3s %s\n", "", "", "", "", str_tagr(tstr, r));
    }
}

walk_return_t
region_print_memory(struct region *r, __unused void *arg)
{
    ROP_PRINT(r);
    return WALK_CONTINUE;
}

void
print_one_memory_region(const struct region *r)
{
    print_memory_region_header();
    ROP_PRINT(r);
}

#ifdef RDAR_23744374
/*
 * The reported size of a mapping to a file object gleaned from
 * mach_vm_region_recurse() can exceed the underlying size of the file.
 * If we attempt to write out the full reported size, we find that we
 * error (EFAULT) or if we compress it, we die with the SIGBUS.
 *
 * See rdar://23744374
 *
 * Figure out what the "non-faulting" size of the object is to
 * *host* page size resolution.
 */
bool
is_actual_size(const task_t task, const struct region *r, mach_vm_size_t *hostvmsize)
{
    if (!r->r_info.external_pager ||
        (r->r_info.max_protection & VM_PROT_READ) == VM_PROT_NONE)
        return true;

    const size_t pagesize_host = 1ul << pageshift_host;
    const unsigned filepages = r->r_info.pages_resident +
    r->r_info.pages_swapped_out;

    if (pagesize_host * filepages == R_SIZE(r))
        return true;

    /*
     * Verify that the last couple of host-pagesize pages
     * of a file backed mapping are actually pageable in the
     * underlying object by walking backwards from the end
     * of the application-pagesize mapping.
     */
    *hostvmsize = R_SIZE(r);

    const long npagemax = 1ul << (pageshift_app - pageshift_host);
    for (long npage = 0; npage < npagemax; npage++) {

        const mach_vm_address_t taddress =
        R_ENDADDR(r) - pagesize_host * (npage + 1);
        if (taddress < R_ADDR(r) || taddress >= R_ENDADDR(r))
            break;

        mach_msg_type_number_t pCount = VM_PAGE_INFO_BASIC_COUNT;
        vm_page_info_basic_data_t pInfo;

        kern_return_t ret = mach_vm_page_info(task, taddress, VM_PAGE_INFO_BASIC, (vm_page_info_t)&pInfo, &pCount);
        if (KERN_SUCCESS != ret) {
            err_mach(ret, NULL, "getting pageinfo at %llx", taddress);
            break;	/* bail */
        }

        /*
         * If this page has been in memory before, assume it can
         * be brought back again
         */
        if (pInfo.disposition & (VM_PAGE_QUERY_PAGE_PRESENT | VM_PAGE_QUERY_PAGE_REF | VM_PAGE_QUERY_PAGE_DIRTY | VM_PAGE_QUERY_PAGE_PAGED_OUT))
            continue;

        /*
         * Force the page to be fetched to see if it faults
         */
        mach_vm_size_t tsize = 1ul << pageshift_host;
        void *tmp = valloc((size_t)tsize);
        const mach_vm_address_t vtmp = (mach_vm_address_t)tmp;

        switch (ret = mach_vm_read_overwrite(task,
                                             taddress, tsize, vtmp, &tsize)) {
            case KERN_INVALID_ADDRESS:
                *hostvmsize = taddress - R_ADDR(r);
                break;
            case KERN_SUCCESS:
                break;
            default:
                err_mach(ret, NULL, "mach_vm_overwrite()");
                break;
        }
        free(tmp);
    }
    return R_SIZE(r) == *hostvmsize;
}
#endif
