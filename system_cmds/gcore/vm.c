/*
 * Copyright (c) 2024 Apple Inc.  All rights reserved.
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
#include <sys/param.h>

#include <mach-o/dyld_introspection.h>

static __inline boolean_t
in_shared_region(dyld_shared_cache_t sc, mach_vm_address_t addr)
{
    if (sc) {
        const uint64_t base = dyld_shared_cache_get_base_address(sc);
        const uint64_t size = dyld_shared_cache_get_mapped_size(sc);
        return addr >= base && addr < (base + size);
    }
    return false;
}

boolean_t
in_stack_tagged_region(const struct region *r)
{
    return r->r_info.user_tag == VM_MEMORY_STACK;
}

/*
 * On both x64 and arm, there's a globally-shared, read-only area of
 * _COMM_PAGE_AREA_LENGTH at _COMM_PAGE_START_ADDRESS which various
 * low-level library routines use directly i.e. they have this address
 * and offsets from it literally compiled into them.
 *
 * The purpose of forcing the commpage into a dump is to allow someone
 * debugging an application's use of those libraries to know what data was
 * present there when the dump was taken.
 *
 * (While it might be tempting to try and identify the commpage "automatically"
 * via the attributes of the region that contains it, this doesn't work
 * well on arm64 because of the fun and games the platform code plays in order
 * to map the commpage globally. See also hack in new_region())
 */

#include <System/machine/cpu_capabilities.h>

static __inline boolean_t
in_comm_region(const mach_vm_address_t addr)
{
    return addr >= _COMM_PAGE_START_ADDRESS &&
        addr < _COMM_PAGE_START_ADDRESS + _COMM_PAGE_AREA_LENGTH;
}

static struct region *
new_region(mach_vm_offset_t vmaddr, mach_vm_size_t vmsize, const vm_region_submap_info_data_64_t *infop, dyld_shared_cache_t sc)
{
    struct region *r = calloc(1, sizeof (*r));
    r->r_info = *infop;
    r->r_purgable = VM_PURGABLE_DENY;

#if defined(__arm64__)
    // At the time of writing, the commpage lives at 0xf_ffff_c000 at
    // the end of a 1GiB read-none segment that starts at 0xf_c000_0000
    if (vmaddr <= _COMM_PAGE_START_ADDRESS &&
        vmaddr + vmsize >= _COMM_PAGE_START_ADDRESS + _COMM_PAGE_AREA_LENGTH) {
        // Rewrite the region to describe only the globally shared page
        // to be dumped. Note that we map the contents of the commpage of
        // *this* process in map_memory_range(), since it's, er.. common.
        vmaddr = _COMM_PAGE_START_ADDRESS;
        vmsize = roundup(_COMM_PAGE_AREA_LENGTH, (1u << pageshift_host));
        r->r_info.protection = r->r_info.max_protection = VM_PROT_READ;
    }
#endif

    assert(vmaddr != 0 && vmsize != 0);
    R_SETADDR(r, vmaddr);
    R_SETSIZE(r, vmsize);

    r->r_insharedregion = in_shared_region(sc, vmaddr);
    r->r_incommregion = in_comm_region(vmaddr);
    r->r_op = &vanilla_ops;

    return r;
}

void
rop_vanilla_delete(struct region *r)
{
    assert(&vanilla_ops == r->r_op);
    assert(0 == r->r_nsubregions);
    poison(r, 0xdeadbeef, sizeof (*r));
    free(r);
}

STAILQ_HEAD(regionhead, region);

/*
 * XXX Need something like mach_vm_shared_region_recurse()
 * to properly identify the shared region address ranges as
 * we go.
 */

static int 
recursively_walk_regions(task_t task, struct regionhead *rhead, const natural_t current_depth, mach_vm_address_t container_start, mach_vm_size_t container_size, dyld_shared_cache_t sc, dyld_process_snapshot_t snapshot, bool force_all_regions) {
    mach_vm_address_t vm_addr = container_start;

    while (1) {
        mach_vm_size_t vm_size = 0;
        vm_region_submap_info_data_64_t info = {0};
        natural_t depth = current_depth;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;

        kern_return_t ret = mach_vm_region_recurse(task, &vm_addr, &vm_size, &depth, (vm_region_recurse_info_t)&info, &count);
        if (KERN_SUCCESS != ret) {
            /*
             * ret is KERN_INVALID_ADDRESS when we finish iterating regions.
             * Anything else (other than KERN_SUCCESS) is an error.
             */
            if (KERN_INVALID_ADDRESS == ret) {
                return EX_OK;
            } else {
                err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
                return EX_OSERR;
            }
        }

        if (!(container_start <= vm_addr && (vm_addr + vm_size) <= (container_start + container_size)) || depth != current_depth) {
            /* Done recursing for this container. */
            return EX_OK;
        }

        if (OPTIONS_DEBUG(opt, 3)) {
            struct region *d = new_region(vm_addr, vm_size, &info, sc);
            print_memory_region(d);
            ROP_DELETE(d);
        }

        if (info.is_submap) {
#ifdef CONFIG_SUBMAP
            /* We also want to see submaps -- for debugging purposes. */
            struct region *r = new_region(vm_addr, vm_size, &info, sc);
            r->r_depth = current_depth;
            STAILQ_INSERT_TAIL(rhead, r, r_linkage);
#endif

            ret = recursively_walk_regions(task, rhead, current_depth + 1, vm_addr, vm_size, sc, snapshot, force_all_regions);
            if (EX_OK != ret) {
                return ret;
            }
            vm_addr += vm_size;
            continue;
        }

        if (VM_MEMORY_IOKIT == info.user_tag && !force_all_regions) {
            vm_addr += vm_size;
            continue; // ignore immediately: IO memory has side-effects
        }

        struct region *r = new_region(vm_addr, vm_size, &info, sc);
#ifdef CONFIG_SUBMAP
        r->r_depth = current_depth;
#endif

        /* record the purgability */

        ret = mach_vm_purgable_control(task, vm_addr, VM_PURGABLE_GET_STATE, &r->r_purgable);
        if (KERN_SUCCESS != ret) {
            r->r_purgable = VM_PURGABLE_DENY;
        }
        STAILQ_INSERT_TAIL(rhead, r, r_linkage);

        vm_addr += vm_size;
    }
}

static int
walk_regions(task_t task, struct regionhead *rhead, bool force_all_regions)
{
    dyld_process_snapshot_t snapshot = NULL;
    kern_return_t ret = KERN_SUCCESS;
    int retval = EX_OSERR;

	if (OPTIONS_DEBUG(opt, 3)) {
		printf("Building raw region list\n");
        print_memory_region_header();
	}

    dyld_process_t process = dyld_process_create_for_task(task, &ret);
    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "dyld_process_create_for_task()");
        goto done;
    }

    snapshot = dyld_process_snapshot_create_for_process(process, &ret);
    dyld_process_dispose(process);
    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "dyld_process_snapshot_create_for_process()");
        goto done;
    }

    /*
     * Processes that have not yet loaded dyld will fail to create a
     * snapshot but have no error, which we treat as success - we just
     * pass along a null shared cache handle to e.g. in_shared_region()
     * which will report all regions as not-the-shared-cache as a result.
     */
    dyld_shared_cache_t sc = NULL;
    if (snapshot &&
        (sc = dyld_process_snapshot_get_shared_cache(snapshot)) == NULL) {
        /*
         * But if there is a snapshot, but (seemingly) no shared cache, bail
         * for now, since there seem to be circumstances with corpses
         * where the shared cache is fully present in the target, but not
         * returned (via this API) when the target is a corpse -- seemingly
         * depending on who's asking e.g. the GCoreFramework XCTest (fails)
         * vs. a standalone C programs (works).
         *
         * XXX Investigate further!
         */
        os_log_error(glog, "fatal: dyld_process_snapshot_get_shared_cache() NULL");
        goto done;
    }

    /* use phys footprint accounting when collecting region info so we find the
     * "owned unmapped memory"
     */
    set_collect_phys_footprint(true);

    retval = recursively_walk_regions(task, rhead, (natural_t)0, 0x0, UINT64_MAX, sc, snapshot, force_all_regions);

    /* set back to default `false` */
    set_collect_phys_footprint(false);

done:
    if (snapshot) {
        dyld_process_snapshot_dispose(snapshot);
    }
    return retval;
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
build_region_list(task_t task, bool force_all_regions)
{
    struct regionhead *rhead = malloc(sizeof (*rhead));
    STAILQ_INIT(rhead);
    if (0 != walk_regions(task, rhead, force_all_regions)) {
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
    printf("%-33s %c %-7s %-7s %8s ",
           "Address Range", 'S', "Size", "Cur/Max", "Obj32");
    printf("%9s %-3s %-11s %5s ",
           "Offset", "Tag", "Mode", "Refc");
#ifdef CONFIG_SUBMAP
    printf("%5s ", "Depth");
#endif
    printf("%5s %5s %5s %5s %3s ",
           "Res", "SNP", "Swap", "Dirty", "Pgr");
    printf("\n");
}

static __inline char
region_type(const struct region *r)
{
    if (r->r_incommregion) {
        return 'c';
    }
    if (r->r_insharedregion) {
        return 's';
    }
    if (((r->r_info.protection | r->r_info.max_protection) & VM_PROT_READ) == 0) {
        return 'u';
    }
    return ' ';
}

void
print_memory_region(const struct region *r)
{
    hsize_str_t hstr;
    tag_str_t tstr;

    printf("%016llx-%016llx %c %-7s %s/%s %8x ",
           R_ADDR(r), R_ENDADDR(r), region_type(r),
           str_hsize(hstr, R_SIZE(r)),
           str_prot(r->r_info.protection),
           str_prot(r->r_info.max_protection),
           r->r_info.object_id
           );

    printf("%9lld %3d %-11s %5u ",
           r->r_info.offset,
           r->r_info.user_tag,
           str_shared(r->r_info.share_mode),
           r->r_info.ref_count
           );
#ifdef CONFIG_SUBMAP
    printf("%5u ", r->r_depth);
#endif

    if (!r->r_info.is_submap) {
        printf("%5u %5u %5u %5u %3s %s\n",
               r->r_info.pages_resident,
               r->r_info.pages_shared_now_private,
               r->r_info.pages_swapped_out,
               r->r_info.pages_dirtied,
               r->r_info.external_pager ? "ext" : "-  ", str_tag(tstr, r));
        if (r->r_nsubregions) {
            printf("    %-33s %7s %5s\n",
                   "Address Subrange", "Size", "Type");
            for (unsigned i = 0; i < r->r_nsubregions; i++) {
                struct subregion *s = r->r_subregions[i];
                printf("    %016llx-%016llx %7s %5s\n",
                       S_ADDR(s), S_ENDADDR(s),
                       str_hsize(hstr, S_SIZE(s)),
                       S_TYPE(s) == SR_CLEAN ? "clean" : "data");
            }
        }
    } else {
        printf("%5s %5s %5s %5s %3s %s\n", "", "", "", "", "", str_tag(tstr, r));
    }
}

walk_return_t
region_print_memory(struct region *r, __unused void *arg)
{
    print_memory_region(r);
    return WALK_CONTINUE;
}

void
print_one_memory_region(const struct region *r)
{
    print_memory_region_header();
    print_memory_region(r);
}

#ifdef RDAR_23744374
/*
 * The reported size of a mapping to a file object gleaned from
 * mach_vm_region_recurse() can exceed the underlying size of the file.
 * If we attempt to write out the full reported size, we find that we
 * error (EFAULT) or if we generate a reference to it, we die with SIGBUS.
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
        (r->r_info.max_protection & VM_PROT_READ) == VM_PROT_NONE) {
        return true;
    }

    const size_t pagesize_host = 1ul << pageshift_host;
    const unsigned filepages = r->r_info.pages_resident + r->r_info.pages_swapped_out;

    if (pagesize_host * filepages == R_SIZE(r)) {
        return true;
    }
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
        if (taddress < R_ADDR(r) || taddress >= R_ENDADDR(r)) {
            break;
        }
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
        if (pInfo.disposition & (VM_PAGE_QUERY_PAGE_PRESENT | VM_PAGE_QUERY_PAGE_REF | VM_PAGE_QUERY_PAGE_DIRTY | VM_PAGE_QUERY_PAGE_PAGED_OUT)) {
            continue;
        }

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

#endif /* RDAR_23744374 */
