/*
 * Copyright (c) 2025 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "vm.h"
#include "region.h"
#include "utils.h"
#include "threads.h"
#include "sparse.h"
#include "vanilla.h"
#include "corefile.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libproc.h>

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <assert.h>

#include <mach/mach.h>

#pragma mark Simple Region Optimization

static walk_return_t
simple_region_optimization(struct region *r, __unused void *arg)
{
    assert(0 != R_SIZE(r));
    
    hsize_str_t hstr;
    tag_str_t tstr;

    /*
     * Elide always unreadable regions
     */
    if ((r->r_info.max_protection & VM_PROT_READ) != VM_PROT_READ) {
        if (OPTIONS_DEBUG(opt, 2)) {
            printr(r, "elided %s %s (%s/%s inaccessible)\n",
                str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r),
                str_prot(r->r_info.protection),
                str_prot(r->r_info.max_protection));
        }
        return WALK_DELETE_REGION;
    }
#ifdef CONFIG_SUBMAP
    /*
     * Elide submaps (here for debugging purposes?)
     */
    if (r->r_info.is_submap) {
        if (OPTIONS_DEBUG(opt, 2)) {
            printr(r, "elided %s %s (submap)\n",
                str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r));
        }
        return WALK_DELETE_REGION;
    }
#endif
    /*
     * Elide currently unreadable regions for certain tags
     */
    if (r->r_info.protection == VM_PROT_NONE) {
        switch (r->r_info.user_tag) {
            case VM_MEMORY_STACK:
            case VM_MEMORY_MALLOC:
            case VM_MEMORY_MALLOC_SMALL:
                if (OPTIONS_DEBUG(opt, 2)) {
                    printr(r, "elided %s %s (%s/%s guard)\n",
                        str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r),
                        str_prot(r->r_info.protection),
                        str_prot(r->r_info.max_protection));
                }
                return WALK_DELETE_REGION;
            default:
//                if (r->r_info.share_mode == SM_EMPTY) {
//                    if (OPTIONS_DEBUG(opt, 2)) {
//                        printr(r, "elided %s %s (%s/%s empty)\n",
//                            str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r),
//                            str_prot(r->r_info.protection),
//                            str_prot(r->r_info.max_protection));
//                    }
//                    return WALK_DELETE_REGION;
//                }
                break;
        }
    }
    
    /*
     * Elide "Owned unmapped memory". These fake regions are useful for
     * accounting purposes but don't contain any backing memory.
     */
    const unsigned owned_unmapped_memory_tag = (unsigned)-1;
    if (r->r_info.user_tag == owned_unmapped_memory_tag) {
        if (OPTIONS_DEBUG(opt, 2)) {
            printr(r, "elided %s %s (accounting)\n",
                str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r));
        }
        return WALK_DELETE_REGION;
    }

    /*
     * Elide magic unmapped rosetta pages that cannot be remapped readable
     */
    if (!r->r_insharedregion && r->r_info.share_mode == SM_EMPTY &&
        0 == (r->r_info.protection & VM_PROT_READ)) {
        switch (r->r_info.user_tag) {
            case VM_MEMORY_ROSETTA_THREAD_CONTEXT:
            case VM_MEMORY_ROSETTA_RETURN_STACK:
            case VM_MEMORY_ROSETTA:
                if (OPTIONS_DEBUG(opt, 2)) {
                    printr(r, "elided %s %s (%s/%s inaccessible)\n",
                        str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r),
                        str_prot(r->r_info.protection),
                        str_prot(r->r_info.max_protection));
                }
                return WALK_DELETE_REGION;
        }
    }

    return WALK_CONTINUE;
}

void
simple_regionlist_optimization(struct regionhead *rh)
{
    walk_region_list(rh, simple_region_optimization, NULL);
}

#pragma mark Stack-only Optimization

/*
 * Only keep regions that are tagged as "stack"
 */
static walk_return_t
stackonly_content(struct region *r, void *__unused arg)
{
    if (in_stack_tagged_region(r)) {
        return WALK_CONTINUE;
    }
    if (OPTIONS_DEBUG(opt, 2)) {
        hsize_str_t hstr;
        tag_str_t tstr;
        printr(r, "elided %s %s (not stack)\n",
            str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r));
    }
    return WALK_DELETE_REGION;
}

void
retain_only_stack(struct regionhead *rh)
{
    walk_region_list(rh, stackonly_content, NULL);
}

#pragma mark Modified-only Content Optimization

/*
 * Sometimes only part(s) of a region contain interesting content, where
 * "interesting" means modified pages. That's what subregions are used for.
 */
static struct subregion *
new_subregion(struct vm_range range, subregiontype type)
{
    assert(range.addr != 0 && range.size != 0);
    assert(range.addr < range.addr + range.size);
    struct subregion *s = malloc(sizeof(*s));
    if (s) {
        S_SETADDR(s, range.addr);
        S_SETSIZE(s, range.size);
        S_SETTYPE(s, type);
    }
    return s;
}

static void
del_subregion(struct subregion *s)
{
    assert(S_ADDR(s) != 0);
    assert(S_SIZE(s) != 0);
    assert(S_TYPE(s) == SR_DIRTY || S_TYPE(s) == SR_CLEAN);
    poison(s, 0xfacefac1, sizeof(*s));
    free(s);
}

static walk_return_t
clean_subregions(struct region *r)
{
	if (r->r_nsubregions) {
		assert(r->r_subregions);
        for (unsigned i = 0; i < r->r_nsubregions; i++) {
            del_subregion(r->r_subregions[i]);
        }
		poison(r->r_subregions, 0xfac1fac1, sizeof(r->r_subregions[0]) * r->r_nsubregions);
		free(r->r_subregions);
		r->r_nsubregions = 0;
		r->r_subregions = NULL;
	} else {
		assert(NULL == r->r_subregions);
	}
    return WALK_CONTINUE;
}

void
rop_sparse_delete(struct region *r)
{
    clean_subregions(r);
    poison(r, 0xcafecaff, sizeof (*r));
    free(r);
}

struct modified_content_args {
    task_t mca_task;
};

// footprint does
// (disp & VM_PAGE_QUERY_PAGE_DIRTY) ||
//  ((disp & VM_PAGE_QUERY_PRESENT) && !(disp && VM_PAGE_QUERY_PAGE_EXTERNAL)

// libmalloc and PerfUtils does
//if (disp & VM_PAGE_QUERY_PAGE_PRESENT) {
//    if (disp & (VM_PAGE_QUERY_PAGE_COPIED|VM_PAGE_QUERY_PAGE_DIRTY)) {
//        magazine.pages_dirty++;
//    }
//} else if (disp & VM_PAGE_QUERY_PAGE_PAGED_OUT) {
//    magazine.pages_dirty++;
//}

// OrderFiles does
// if (disposition & (VM_PAGE_QUERY_PAGE_DIRTY | VM_PAGE_QUERY_PAGE_PAGED_OUT))

static subregiontype
page_type(int disp)
{
    if (disp & VM_PAGE_QUERY_PAGE_PRESENT) {
        if (disp & (VM_PAGE_QUERY_PAGE_COPIED|VM_PAGE_QUERY_PAGE_DIRTY)) {
            return SR_DIRTY;
        }
    } else if (disp & VM_PAGE_QUERY_PAGE_PAGED_OUT) {
        return SR_DIRTY;
    }
    return SR_CLEAN;
}

struct subregionlist {
    STAILQ_ENTRY(subregionlist) srl_link;
    struct subregion *srl_s;
};

/*
 * Two separate allocations. Start with the subregions on a
 * subregionlist tail queue, then repack if needed into an array
 * of subregions hanging off a struct region.
 */
static struct subregionlist *
new_subregionlist(struct vm_range range, subregiontype t)
{
    struct subregion *s = new_subregion(range, t);
    if (s) {
        struct subregionlist *srl = calloc(1, sizeof(*srl));
        if (srl) {
            srl->srl_s = s;
            return srl;
        }
        del_subregion(s);
    }
    return NULL;
}

static walk_return_t
modified_content(struct region *r, void *arg)
{
    hsize_str_t hstr;
    tag_str_t tstr;

    if (r->r_incommregion) {
        // preserve the commpage
        return WALK_CONTINUE;
    }

    /*
     * Seems like a simple test on r->r_info.pages_dirtied + swapped_out
     * would let you identify unmodified segments quickly, but experimentally
     * it appears that some regions still have dirty pages despite what
     * r_info says.
     */
    
    /*
     * Eliminate r-x and r-- regions of the shared cache.
     */
    if (r->r_insharedregion && r->r_info.external_pager &&
        ((r->r_info.max_protection | r->r_info.protection) & VM_PROT_WRITE) == 0 &&
        r->r_info.pages_dirtied == 0 && r->r_info.pages_swapped_out == 0) {
        if (OPTIONS_DEBUG(opt, 2)) {
            printr(r, "elided %s %s (shared cache, %s)\n",
                str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r),
                str_prot(r->r_info.max_protection));
        }
        return WALK_DELETE_REGION;
    }

    const struct modified_content_args *mca = arg;
    const size_t pagesize_app = 1ul << pageshift_app;
    const mach_vm_address_t starta = R_ADDR(r);
    const mach_vm_address_t enda = R_ENDADDR(r);
    const mach_vm_size_t npages = (enda + pagesize_app - 1 - starta) / pagesize_app;
    int *disp = calloc(1, (size_t)(npages * sizeof(*disp)));
    if (disp == NULL) {
        if (OPTIONS_DEBUG(opt, 1)) {
            printr(r, "failed to allocate memory for page attributes\n");
        }
        return WALK_CONTINUE;
    }
    
    mach_vm_size_t page_count = npages;
    kern_return_t kr = mach_vm_page_range_query(mca->mca_task,
        starta, npages * pagesize_app, (mach_vm_address_t)disp, &page_count);
    if (kr != KERN_SUCCESS) {
        if (OPTIONS_DEBUG(opt, 1)) {
            err_mach(kr, r, "fetch page attributes");
        }
        free(disp);
        return WALK_CONTINUE;
    }

    struct subregionlisthead {
        STAILQ_HEAD(, subregionlist) head;
        unsigned nelem;
    } data = {
        .head = STAILQ_HEAD_INITIALIZER(data.head),
        .nelem = 0,
    };

    /*
     * Some regions are copy-on-write with a few dirty pages. We want the
     * debugger to see the clean pages in the region as present (and zeroed),
     * so in those cases, we also record the clean pages and write them
     * out using a "zfod" segment in the core header i.e. with filesize == 0.
     */
    const bool record_clean = r->r_info.external_pager == 0 &&
        (r->r_info.share_mode == SM_COW || r->r_info.share_mode == SM_PRIVATE);

    struct subregionlist * (^listelem)(ppnum_t, ppnum_t) = ^(ppnum_t s, ppnum_t l) {
        struct vm_range vr = {
            .addr = starta + s * pagesize_app,
            .size = l * pagesize_app,
        };
        return new_subregionlist(vr, page_type(disp[s]));
    };

    ppnum_t start = 0;
    ppnum_t run_length = 1;
    for (ppnum_t pgno = 1; pgno < page_count; pgno++) {

        const subregiontype now = page_type(disp[pgno]);
        const subregiontype prev = page_type(disp[pgno-1]);

        if (now == prev) {
            // clean -> clean || dirty -> dirty
            run_length++;
            continue;
        }
        // clean -> dirty || dirty -> clean
        if (prev == SR_DIRTY || record_clean) {
            struct subregionlist *srl = listelem(start, run_length);
            if (srl == NULL) {
                goto done;
            }
            STAILQ_INSERT_TAIL(&data.head, srl, srl_link);
            data.nelem += 1;
        }
        start = pgno;
        run_length = 1;
    }
    
    // Handle the trailing edge

    if (page_count &&
        (page_type(disp[start]) == SR_DIRTY || record_clean)) {
        struct subregionlist *srl = listelem(start, run_length);
        if (srl == NULL) {
            goto done;
        }
        STAILQ_INSERT_TAIL(&data.head, srl, srl_link);
        data.nelem += 1;
    }

    switch (data.nelem) {
        case 0:
            /*
             * No subregions implicitly means we only looked for dirty pages,
             * (and there weren't any), so just discard the region
             * altogether e.g. this is an unmodified anon region or
             * unmodified mapped file
             */
            assert(!record_clean);
            if (OPTIONS_DEBUG(opt, 2)) {
                printr(r, "elided %s %s (no modified pages)\n",
                    str_hsize(hstr, R_SIZE(r)), str_tag(tstr, r));
            }
            free(disp);
            return WALK_DELETE_REGION;

        case 1: {
            const struct subregion *s = STAILQ_FIRST(&data.head)->srl_s;
            if (R_ADDR(r) == S_ADDR(s) && R_ENDADDR(r) == S_ENDADDR(s)) {
                /*
                 * Only one subregion == the region!
                 * If it was all dirty, just use the existing vanilla
                 * region representation.
                 */
                if (S_TYPE(s) == SR_DIRTY) {
                    break;
                }
            }
        }
            /* FALLTHROUGH */
        default:
            // Move subregions from the tailq to an array
            r->r_subregions = calloc(data.nelem, sizeof(void *));
            if (r->r_subregions == NULL) {
                break;  // stick with a region description
            }
            r->r_nsubregions = 0;
            struct subregionlist *srl;
            STAILQ_FOREACH(srl, &data.head, srl_link) {
                r->r_subregions[r->r_nsubregions++] = srl->srl_s;
                srl->srl_s = NULL;  // ownership transferred!
            }
            assert(data.nelem == r->r_nsubregions);
            r->r_op = &sparse_ops;
    }
    
done:
    if (data.nelem) {
        // Destroy the temporary subregion list (and any lurking subregions)
        struct subregionlist *srl, *trl;
        STAILQ_FOREACH_SAFE(srl, &data.head, srl_link, trl) {
            if (srl->srl_s) {
                del_subregion(srl->srl_s);
            }
            free(srl);
        }
    }
    free(disp);
    return WALK_CONTINUE;
}

void
retain_only_modified(struct regionhead *rh, task_t task)
{
    struct modified_content_args mca = {
        .mca_task = task,
    };
    walk_region_list(rh, modified_content, &mca);
}
