/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "dyld.h"
#include "utils.h"
#include "corefile.h"
#include "vm.h"

#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <mach-o/dyld_process_info.h>

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
