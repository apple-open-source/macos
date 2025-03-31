/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "dyld.h"
#include "utils.h"
#include "corefile.h"
#include "vm.h"
#include "vanilla.h"
#include "sparse.h"
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/dyld_process_info.h>
#include <mach-o/dyld_introspection.h>
#include <mach/mach.h>
#include <mach/task.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <sys/param.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <libgen.h>
#include <sys/stat.h>

/*
 * WARNING WARNING WARNING
 *
 * Do not trust any of the data from the target task.
 *
 * A broken program may have damaged it, or a malicious
 * program may have deliberately constructed something to
 * cause us harm.
 */

/*
 * List of memory regions that cannot be optmized,
 * because they are critical for analysis
 */
#define DYLD_PRIVATE_MEMORY_NAME "dyld private memory"

static const char *protected_regions [] = {
    DYLD_PRIVATE_MEMORY_NAME,
    NULL
};

struct memory_region_preservation
{
    dyld_shared_cache_t sc;
    dyld_process_t process;
    dyld_process_snapshot_t snapshot;
    struct regionhead *rhead;

};
/* Create the dyld headers coredump sections */

static void get_dyld_ranges(task_t task);

/*
 * Check if a region should not be removed to protect analysis,
 * returns true if that region is NOT protected
 */
bool 
can_remove_region(struct region *r) {
    if (r->r_info.user_tag == VM_MEMORY_DYLD) {
        return false;
    }
    if (r->r_nsubregions) {
        for (unsigned i = 0; i < r->r_nsubregions; i++) {
            struct subregion *s = r->r_subregions[i];
            const char *subregion_name = S_FILENAME(s);
            for (unsigned j=0;j<sizeof(protected_regions)/sizeof(protected_regions[0]);j++) {
                if (OPTIONS_DEBUG(opt, 3)) {
                    printr(r,"comparing name %s with %s\n",subregion_name,protected_regions[j]);
                }

                if (!strcmp(subregion_name,protected_regions[j])) {
                    if (OPTIONS_DEBUG(opt, 3)) {
                        printr(r,"have to be protected\n");
                    }
                    return false;
                }
            }
        }
    }
    printr(r,"can be removed\n");
    return true;
}

static const char warn_dyld_info[] = "dyld information is incomplete or damaged";

dyld_process_info
get_task_dyld_info(const task_t task)
{
    kern_return_t kret;
    dyld_process_info dpi = _dyld_process_info_create(task, 0, &kret);
    if (NULL == dpi) {
        err_mach(kret, NULL, "_dlyd_process_info_create");
    } else {
        dyld_process_state_info stateInfo;

        _dyld_process_info_get_state(dpi, &stateInfo);
        switch (stateInfo.dyldState) {
            case dyld_process_state_not_started:
                warnx("%s: dyld state %d", warn_dyld_info, stateInfo.dyldState);
                _dyld_process_info_release(dpi);
                dpi = NULL;
                break;
            default:
                break;
        }
    }
    return dpi;
}

/*
 * Get the shared cache UUID iff it's in use and is the system one
 */
bool
get_sc_uuid(dyld_process_info dpi, uuid_t uu)
{
    dyld_process_cache_info cacheInfo;

    _dyld_process_info_get_cache(dpi, &cacheInfo);
    if (!cacheInfo.noCache && !cacheInfo.privateCache) {
        uuid_copy(uu, cacheInfo.cacheUUID);
        return true;
    }
    return false;
}

void
free_task_dyld_info(dyld_process_info dpi)
{
    _dyld_process_info_release(dpi);
}

/*
 * This routine collects both the Mach-O header and the commands
 * "below" it, assuming they're in contiguous memory.
 */
static native_mach_header_t *
copy_dyld_image_mh(task_t task, mach_vm_address_t targetmh, const char *path)
{
    vm_offset_t mhaddr = 0;
    mach_msg_type_number_t mhlen = sizeof (native_mach_header_t);

    for (int attempts = 0; attempts < 2; attempts++) {

        const kern_return_t ret = mach_vm_read(task, targetmh, mhlen, &mhaddr, &mhlen);
        if (KERN_SUCCESS != ret) {
            err_mach(ret, NULL, "mach_vm_read() at 0x%llx for image %s\n", targetmh, path);
            mhaddr = 0;
            break;
        }
        const native_mach_header_t *mh = (void *)mhaddr;
        if (mhlen < mh->sizeofcmds + sizeof (*mh)) {
            const mach_msg_type_number_t newmhlen = sizeof (*mh) + mh->sizeofcmds;
            mach_vm_deallocate(mach_task_self(), mhaddr, mhlen);
            mhlen = newmhlen;
        } else
            break;
    }

    native_mach_header_t *result = NULL;

    if (mhaddr) {
        if (NULL != (result = malloc(mhlen)))
            memcpy(result, (void *)mhaddr, mhlen);
        mach_vm_deallocate(mach_task_self(), mhaddr, mhlen);
    }
    return result;
}

/*
 * This table (list) describes libraries and the executable in the address space
 */
struct liblist {
    STAILQ_ENTRY(liblist) ll_linkage;
    unsigned long ll_namehash;
    struct libent ll_entry;
};
static STAILQ_HEAD(, liblist) libhead = STAILQ_HEAD_INITIALIZER(libhead);

static const struct libent *
libent_lookup_bypathname_withhash(const char *nm, const unsigned long hash)
{
    struct liblist *ll;
    STAILQ_FOREACH(ll, &libhead, ll_linkage) {
        if (hash != ll->ll_namehash)
            continue;
        struct libent *le = &ll->ll_entry;
        if (strcmp(nm, le->le_pathname) == 0)
            return le;
    }
    return NULL;
}

const struct libent *
libent_lookup_byuuid(const uuid_t uuid)
{
    struct liblist *ll;
    STAILQ_FOREACH(ll, &libhead, ll_linkage) {
        struct libent *le = &ll->ll_entry;
        if (uuid_compare(uuid, le->le_uuid) == 0)
            return le;
    }
    return NULL;
}

const struct libent *
libent_lookup_first_bytype(uint32_t mhtype)
{
    struct liblist *ll;
    STAILQ_FOREACH(ll, &libhead, ll_linkage) {
        struct libent *le = &ll->ll_entry;
        if (mhtype == le->le_mh->filetype)
            return le;
    }
    return NULL;
}

const struct libent *
libent_insert(const char *rawnm, const uuid_t uuid, uint64_t mhaddr, const native_mach_header_t *mh, const struct vm_range *vr, mach_vm_offset_t objoff)
{
    const struct libent *le = libent_lookup_byuuid(uuid);
    if (NULL != le)
        return le;  // disallow multiple names for the same uuid

    char *nm = realpath(rawnm, NULL);
    if (NULL == nm)
        nm = strdup(rawnm);
    const unsigned long nmhash = simple_namehash(nm);
    le = libent_lookup_bypathname_withhash(nm, nmhash);
	if (NULL != le) {
		free(nm);
        return le;
	}

    if (OPTIONS_DEBUG(opt, 3)) {
        uuid_string_t uustr;
        uuid_unparse_lower(uuid, uustr);
        printf("[adding <'%s', %s, 0x%llx, %p", nm, uustr, mhaddr, mh);
		if (vr)
			printf(" (%llx-%llx)", V_ADDR(vr), V_ENDADDR(vr));
		printf(">]\n");
    }
    struct liblist *ll = malloc(sizeof (*ll));
    ll->ll_namehash = nmhash;
    ll->ll_entry.le_pathname = nm;
    ll->ll_entry.le_filename = strrchr(ll->ll_entry.le_pathname, '/');
    if (NULL == ll->ll_entry.le_filename)
        ll->ll_entry.le_filename = ll->ll_entry.le_pathname;
    else
        ll->ll_entry.le_filename++;
    uuid_copy(ll->ll_entry.le_uuid, uuid);
    ll->ll_entry.le_mhaddr = mhaddr;
    ll->ll_entry.le_mh = mh;
	if (vr)
		ll->ll_entry.le_vr = *vr;
	else {
		V_SETADDR(&ll->ll_entry.le_vr, MACH_VM_MAX_ADDRESS);
		V_SETSIZE(&ll->ll_entry.le_vr, 0);
	}
	ll->ll_entry.le_objoff = objoff;
    STAILQ_INSERT_HEAD(&libhead, ll, ll_linkage);

    return &ll->ll_entry;
}

bool
libent_build_nametable(task_t task, dyld_process_info dpi)
{
    __block bool valid = true;
    /*
     * Lets prepare the range of DYLD shared for latter use
     */
    get_dyld_ranges(task);
	_dyld_process_info_for_each_image(dpi, ^(uint64_t mhaddr, const uuid_t uuid, const char *path) {
        if (valid) {
            native_mach_header_t *mh = copy_dyld_image_mh(task, mhaddr, path);
            if (mh) {
                /*
                 * Validate the rest of the mach information in the header before attempting optimizations
                 */
                const size_t mhlen = sizeof (*mh) + mh->sizeofcmds;
                const struct load_command *lc = (const void *)(mh + 1);
		struct vm_range vr = {
			.addr = MACH_VM_MAX_ADDRESS,
			.size = 0
		};
		mach_vm_offset_t objoff = MACH_VM_MAX_ADDRESS;

                for (unsigned n = 0; n < mh->ncmds; n++) {
                    if (((uintptr_t)lc & 0x3) != 0 ||
                        (uintptr_t)lc < (uintptr_t)mh || (uintptr_t)lc > (uintptr_t)mh + mhlen) {
                        warnx("%s, %d", warn_dyld_info, __LINE__);
                        valid = false;
                        break;
                    }
					switch (lc->cmd) {
						case NATIVE_LC_SEGMENT: {
							const native_segment_command_t *sc = (const void *)lc;

							char scsegname[17];
							strlcpy(scsegname, sc->segname, sizeof (scsegname));

							if (0 == sc->vmaddr &&
								strcmp(scsegname, SEG_PAGEZERO) == 0)
								break;

							/*
							 * -Depends- on finding a __TEXT segment first
							 * which implicitly maps the mach header too
							 */

							if (MACH_VM_MAX_ADDRESS == objoff) {
								if (strcmp(scsegname, SEG_TEXT) == 0) {
									objoff = mhaddr - sc->vmaddr;
									V_SETADDR(&vr, mhaddr);
									V_SETSIZE(&vr, sc->vmsize);
								} else {
									printf("%s: expected %s segment, found %s\n", path, SEG_TEXT, scsegname);
									valid = false;
									break;
								}
							}

							mach_vm_offset_t lo = sc->vmaddr + objoff;
							mach_vm_offset_t hi = lo + sc->vmsize;

							if (V_SIZE(&vr)) {
								if (lo < V_ADDR(&vr)) {
									mach_vm_offset_t newsize = V_SIZE(&vr) + (V_ADDR(&vr) - lo);
									V_SETSIZE(&vr, newsize);
									V_SETADDR(&vr, lo);
								}
								if (hi > V_ENDADDR(&vr)) {
									V_SETSIZE(&vr, (hi - V_ADDR(&vr)));
								}
							} else {
								V_SETADDR(&vr, lo);
								V_SETSIZE(&vr, hi - lo);
							}
							assert(lo >= V_ADDR(&vr) && hi <= V_ENDADDR(&vr));
						}	break;
#if defined(RDAR_28040018)
						case LC_ID_DYLINKER:
							if (MH_DYLINKER == mh->filetype) {
								/* workaround: the API doesn't always return the right name */
								const struct dylinker_command *dc = (const void *)lc;
								path = dc->name.offset + (const char *)dc;
							}
							break;
#endif
						default:
							break;
					}
                    if (NULL == (lc = next_lc(lc)))
                        break;
                }
				if (valid)
                    (void) libent_insert(path, uuid, mhaddr, mh, &vr, objoff);
            }
        }
    });
    if (OPTIONS_DEBUG(opt, 3))
        printf("nametable %sconstructed\n", valid ? "" : "NOT ");
    return valid;
}

void create_dyld_header_regions(task_t task, struct regionhead *rhead) {
    struct liblist *ll;
    dyld_shared_cache_t sc = NULL;
    dyld_process_t process = NULL;
    dyld_process_snapshot_t snapshot = NULL;
    kern_return_t ret = KERN_SUCCESS;

    if (OPTIONS_DEBUG(opt, 1))
        printf("creating shared libraries macho headers section\n");

    process = dyld_process_create_for_task(task, &ret);
    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "dyld_process_create_for_task()");
        goto done;
    }
    snapshot = dyld_process_snapshot_create_for_process(process, &ret);
    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "dyld_process_snapshot_create_for_process()");
        goto done;
    }
    sc = dyld_process_snapshot_get_shared_cache(snapshot);
    

    STAILQ_FOREACH(ll, &libhead, ll_linkage) {
        vm_region_submap_info_data_64_t info = {0};
        natural_t depth = 0;
        mach_msg_type_number_t count = VM_REGION_SUBMAP_INFO_COUNT_64;
        mach_vm_address_t vm_addr = ll->ll_entry.le_mhaddr;
        if(vm_addr == 0) {
            continue;
        }
        mach_vm_size_t vm_size = ll->ll_entry.le_mh->sizeofcmds + sizeof(*ll->ll_entry.le_mh);
        /*
         * We want to process only DYLD headers, rest will be part of the
         * regular coredump.
         */
        if (!is_range_part_of_the_shared_library_address_space(vm_addr,vm_size)) {
            if (OPTIONS_DEBUG(opt, 3))
                printf("skipping shared library macho header section at address 0x%16.16llx size %llu name %s\n",vm_addr,vm_size,ll->ll_entry.le_filename);
            continue;
        }
        if (OPTIONS_DEBUG(opt, 3))
            printf("shared library macho header section at address 0x%16.16llx size %llu name %s\n",vm_addr,vm_size,ll->ll_entry.le_filename);
        do {
            mach_vm_address_t test_addr = vm_addr;
            mach_vm_size_t test_size;
            ret = mach_vm_region_recurse(task, &test_addr, &test_size, &depth, (vm_region_recurse_info_t)&info, &count);
            depth++;

            if (KERN_FAILURE == ret) {
                err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
                continue;
            } else if (KERN_INVALID_ADDRESS == ret) {
                err_mach(ret, NULL, "invalid address at %llx", vm_addr);
                continue;
            } else if (KERN_SUCCESS != ret) {
                err_mach(ret, NULL, "error inspecting task at %llx", vm_addr);
                continue;
            }
        } while(info.is_submap);
        
        if (OPTIONS_DEBUG(opt, 3))
            printf("inserting shared library macho header section at address 0x%16.16llx size %llu name %s\n",vm_addr,vm_size,ll->ll_entry.le_filename);
        (void) vm_insert_region(rhead,vm_addr, vm_size, &info, sc);
    }
done:
    if (snapshot)
        dyld_process_snapshot_dispose(snapshot);
    if (process)
        dyld_process_dispose(process);

}

static walk_return_t
check_if_region_must_be_preserved(struct region *r, void *arg)
{
    if (OPTIONS_DEBUG(opt, 3)) {
        printr(r, "Checking region\n");
    }
    struct memory_region_preservation * preservation_data = (struct memory_region_preservation *)arg;
    struct regionhead *rhead = preservation_data->rhead;
    if (!can_remove_region(r)) {

        (void) vm_insert_region(rhead,r->r_range.addr, r->r_range.size, &r->r_info, preservation_data->sc);
    }
    return WALK_DELETE_REGION;
}

/*
 * This function adds to the coredump the regions that even being discarded
 * by common criteria (read/only, not dirty, etc.) they have to be part of the
 * coredump or analysis would fail.
 * They are identified by its name
 * An example of them are the dyld private sections, that are unique
 * per process and cannot be read on host process in skinny analysis
 */
void add_forced_regions(task_t task,struct regionhead *rhead) {
    kern_return_t ret;
    struct memory_region_preservation preservation_data = {
        .process = NULL,
        .sc = NULL,
        .snapshot = NULL,
        .rhead = rhead,
    };
    preservation_data.process = dyld_process_create_for_task(task, &ret);
    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "dyld_process_create_for_task()");
        goto done;
    }
    preservation_data.snapshot = dyld_process_snapshot_create_for_process(preservation_data.process, &ret);
    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "dyld_process_snapshot_create_for_process()");
        goto done;
    }
    preservation_data.sc = dyld_process_snapshot_get_shared_cache(preservation_data.snapshot);

    struct regionhead *sys_rhead = build_region_list(task, true);
    if (OPTIONS_DEBUG(opt, 3)) {
        printf("Adding forced regions\n");
    }
    walk_region_list(sys_rhead, check_if_region_must_be_preserved, &preservation_data);
    del_region_list(sys_rhead);

done:
    if (preservation_data.snapshot)
        dyld_process_snapshot_dispose(preservation_data.snapshot);
    if (preservation_data.process)
        dyld_process_dispose(preservation_data.process);
}


static mach_vm_address_t dyld_shared_cache_base_addr = UINT64_MAX;
static mach_vm_size_t    dyld_shared_cache_size      = UINT64_MAX;
static void get_dyld_ranges(task_t task) {
    dyld_shared_cache_base_addr = UINT64_MAX;
    dyld_shared_cache_size = UINT64_MAX;
    
    kern_return_t dyld_err = KERN_FAILURE;
    
    dyld_process_t process = dyld_process_create_for_task(task, &dyld_err);
    if (!process) {
        errx(EX_OSERR, "Failed to create dyld process info with error %s", mach_error_string(dyld_err));
    }
    
    dyld_process_snapshot_t snapshot = dyld_process_snapshot_create_for_process(process, &dyld_err);
    if (!snapshot) {
        errx(EX_OSERR, "Failed to create dyld snapshot with error %s", mach_error_string(dyld_err));
    }
    
    dyld_shared_cache_t cache = dyld_process_snapshot_get_shared_cache(snapshot);
    if (!cache) {
        errx(EX_OSERR, "Failed to get dyld shared cache info");
    }
    
    if (!dyld_shared_cache_is_mapped_private(cache)) {
        /* ignore private shared caches */
        dyld_shared_cache_base_addr = dyld_shared_cache_get_base_address(cache);
        dyld_shared_cache_size = dyld_shared_cache_get_mapped_size(cache);
    }
    
    dyld_process_snapshot_dispose(snapshot);
    dyld_process_dispose(process);

}

/*
 * Checks if a range belongs to the DYLD memory cache, normally
 * used to avoid creating file references for executables / frameworks
 * that are not part of shared cache
 *
 * Returns true if the range is part of DYLD areas.
 */
bool is_range_part_of_the_shared_library_address_space(mach_vm_address_t address,mach_vm_size_t size) {
    if (dyld_shared_cache_base_addr <= address &&
        dyld_shared_cache_base_addr+dyld_shared_cache_size >= address + size)
    {
        return true;
    }
    return false;
}
