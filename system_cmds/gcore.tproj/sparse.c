/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "vm.h"
#include "region.h"
#include "utils.h"
#include "dyld.h"
#include "threads.h"
#include "sparse.h"
#include "vanilla.h"
#include "corefile.h"

#include <sys/types.h>
#include <sys/sysctl.h>
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

static struct subregion *
new_subregion(
    const mach_vm_offset_t vmaddr,
    const mach_vm_offset_t vmsize,
    const native_segment_command_t *sc,
    const struct libent *le)
{
    struct subregion *s = malloc(sizeof (*s));

    assert(vmaddr != 0 && vmsize != 0);
    assert(vmaddr < vmaddr + vmsize);
    s->s_segcmd = *sc;

    S_SETADDR(s, vmaddr);
    S_SETSIZE(s, vmsize);

    s->s_libent = le;
    s->s_isfileref = false;
    return s;
}

static void
del_subregion(struct subregion *s)
{
    poison(s, 0xfacefac1, sizeof (*s));
    free(s);
}

static walk_return_t
clean_subregions(struct region *r)
{
    for (unsigned i = 0; i < r->r_nsubregions; i++)
        del_subregion(r->r_subregions[i]);
    poison(r->r_subregions, 0xfac1fac1, sizeof (r->r_subregions[0]) * r->r_nsubregions);
    free(r->r_subregions);
    r->r_nsubregions = 0;
    return WALK_CONTINUE;
}

void
del_sparse_region(struct region *r)
{
    clean_subregions(r);
    poison(r, 0xcafecaff, sizeof (*r));
    free(r);
}

#define NULLsc  ((native_segment_command_t *)0)

static bool
issamesubregiontype(const struct subregion *s0, const struct subregion *s1) {
    return 0 == strncmp(S_MACHO_TYPE(s0), S_MACHO_TYPE(s1), sizeof (NULLsc->segname));
}

bool
issubregiontype(const struct subregion *s, const char *sctype) {
    return 0 == strncmp(S_MACHO_TYPE(s), sctype, sizeof (NULLsc->segname));
}

static void
elide_subregion(struct region *r, unsigned ind)
{
    del_subregion(r->r_subregions[ind]);
    for (unsigned j = ind; j < r->r_nsubregions - 1; j++)
        r->r_subregions[j] = r->r_subregions[j+1];
    assert(r->r_nsubregions != 0);
    r->r_subregions[--r->r_nsubregions] = NULL;
}

struct subregionlist {
    STAILQ_ENTRY(subregionlist) srl_linkage;
    struct subregion *srl_s;
};
typedef STAILQ_HEAD(, subregionlist) subregionlisthead_t;

static walk_return_t
add_subregions_for_libent(
    subregionlisthead_t *srlh,
    const struct region *r,
    const native_mach_header_t *mh,
    const mach_vm_offset_t mh_taddr,
    const struct libent *le)
{
    const struct load_command *lc = (const void *)(mh + 1);
    mach_vm_offset_t scoffset = MACH_VM_MAX_ADDRESS;

    for (unsigned n = 0; n < mh->ncmds; n++) {

        const native_segment_command_t *sc;

        switch (lc->cmd) {
            case NATIVE_LC_SEGMENT:
                sc = (const void *)lc;

                char scsegname[17];
                strlcpy(scsegname, sc->segname, sizeof (scsegname));

                if (0 == sc->vmaddr &&
                    strcmp(scsegname, SEG_PAGEZERO) == 0)
                    break;

                /* -Depends- on finding a __TEXT segment first! */

                if (MACH_VM_MAX_ADDRESS == scoffset) {
                    if (strcmp(scsegname, SEG_TEXT) == 0)
                        scoffset = mh_taddr - sc->vmaddr;
                    else {
                        /*
                         * Treat as error - don't want a partial description
                         * to cause something to be omitted from the dump.
                         */
                        printr(r, "expected %s segment, found %s segment\n", SEG_TEXT, scsegname);
                        return WALK_ERROR;
                    }
                }

                /* Eliminate non-overlapping sections first */

                if (R_ENDADDR(r) - 1 < sc->vmaddr + scoffset)
                    break;
                if (sc->vmaddr + scoffset + sc->vmsize - 1 < R_ADDR(r))
                    break;
                /*
                 * Some part of this segment is in the region.
                 * Trim the edges in the case where we span regions.
                 */
                mach_vm_offset_t loaddr = sc->vmaddr + scoffset;
                mach_vm_offset_t hiaddr = loaddr + sc->vmsize;
                if (loaddr < R_ADDR(r))
                    loaddr = R_ADDR(r);
                if (hiaddr > R_ENDADDR(r))
                    hiaddr = R_ENDADDR(r);

                struct subregionlist *srl = calloc(1, sizeof (*srl));
                struct subregion *s = new_subregion(loaddr, hiaddr - loaddr, sc, le);
                assert(sc->fileoff >= 0);
                srl->srl_s = s;
                STAILQ_INSERT_HEAD(srlh, srl, srl_linkage);

                if (opt->debug > 3) {
                    hsize_str_t hstr;
                    printr(r, "subregion %llx-%llx %7s %12s\t%s [%x/%x off %zd for %zd nsects %u flags %x]\n",
                           S_ADDR(s), S_ENDADDR(s),
                           str_hsize(hstr, S_SIZE(s)),
                           scsegname,
                           S_FILENAME(s),
                           sc->initprot, sc->maxprot,
                           sc->fileoff, sc->filesize,
                           sc->nsects, sc->flags);
                }
                break;
            default:
                break;
        }
        if (lc->cmdsize)
            lc = (const void *)((caddr_t)lc + lc->cmdsize);
        else
            break;
    }
    return WALK_CONTINUE;
}

/*
 * Because we aggregate information from multiple sources, there may
 * be duplicate subregions.  Eliminate them here.
 *
 * Note that the each library in the shared cache points
 * separately at a single, unified (large!) __LINKEDIT section; these
 * get removed here too.
 *
 * Assumes the subregion array is sorted by address!
 */
static void
eliminate_duplicate_subregions(struct region *r)
{
    unsigned i = 1;
    while (i < r->r_nsubregions) {
        struct subregion *s0 = r->r_subregions[i-1];
        struct subregion *s1 = r->r_subregions[i];

        if (S_ADDR(s0) != S_ADDR(s1) || S_SIZE(s0) != S_SIZE(s1)) {
            i++;
            continue;
        }
        if (memcmp(&s0->s_segcmd, &s1->s_segcmd, sizeof (s0->s_segcmd)) != 0) {
            i++;
            continue;
        }
        if (opt->debug)
            printr(r, "eliding duplicate %s subregion (%llx-%llx) file %s\n",
                   S_MACHO_TYPE(s1), S_ADDR(s1), S_ENDADDR(s1), S_FILENAME(s1));
        /* If the duplicate subregions aren't mapping the same file (?), forget the name */
        if (s0->s_libent != s1->s_libent)
            s0->s_libent = s1->s_libent = NULL;
        elide_subregion(r, i);
    }
}

/*
 * See if any of the dyld information we have can better describe this
 * region of the target address space.
 */
walk_return_t
decorate_memory_region(struct region *r, void *arg)
{
    const dyld_process_info dpi = arg;

    __block walk_return_t retval = WALK_CONTINUE;
    __block subregionlisthead_t srlhead = STAILQ_HEAD_INITIALIZER(srlhead);

    _dyld_process_info_for_each_image(dpi, ^(uint64_t mhaddr, const uuid_t uuid, __unused const char *path) {
        if (WALK_CONTINUE == retval) {
            const struct libent *le = libent_lookup_byuuid(uuid);
            assert(le->le_mhaddr == mhaddr);
            /*
             * Core dumps conventionally contain the whole executable, but we're trying
             * to elide everything that can't be found in a file elsewhere.
             */
#if 0
            if (MH_EXECUTE == le->le_mh->filetype)
                return; // cause the whole a.out to be emitted
#endif
            retval = add_subregions_for_libent(&srlhead, r, le->le_mh, le->le_mhaddr, le);
        }
    });
    if (WALK_CONTINUE != retval)
        goto done;

    /*
     * Take the unsorted list of subregions, if any,
     * and hang a sorted array of ranges on the region structure.
     */
    if (!STAILQ_EMPTY(&srlhead)) {
        struct subregionlist *srl;
        STAILQ_FOREACH(srl, &srlhead, srl_linkage) {
            r->r_nsubregions++;
        }
        assert(r->r_nsubregions);

        r->r_subregions = calloc(r->r_nsubregions, sizeof (void *));
        unsigned i = 0;
        STAILQ_FOREACH(srl, &srlhead, srl_linkage) {
            r->r_subregions[i++] = srl->srl_s;
        }
        qsort_b(r->r_subregions, r->r_nsubregions, sizeof (void *),
                ^(const void *a, const void *b) {
                    const struct subregion *lhs = *(struct subregion **)a;
                    const struct subregion *rhs = *(struct subregion **)b;
                    if (S_ADDR(lhs) > S_ADDR(rhs))
                        return 1;
                    if (S_ADDR(lhs) < S_ADDR(rhs))
                        return -1;
                    return 0;
                });

        eliminate_duplicate_subregions(r);

        const struct libent *lesc = NULL;   /* libent ref for shared cache */
        if (r->r_insharedregion) {
            uuid_t uusc;
            if (get_sc_uuid(dpi, uusc)) {
                lesc = libent_lookup_byuuid(uusc);
                assert(NULL == lesc->le_mh && 0 == lesc->le_mhaddr);
            }
        }

        /*
         * Only very specific segment types get to be filerefs
         */
        for (i = 0; i < r->r_nsubregions; i++) {
            struct subregion *s = r->r_subregions[i];
            /*
             * Anything writable is trivially disqualified
             */
            if (s->s_segcmd.initprot & VM_PROT_WRITE)
                continue;
            /*
             * As long as there's a filename, __TEXT and __LINKEDIT
             * end up as a file reference.
             *
             * __LINKEDIT is more complicated: the segment commands point
             * at a unified segment in the shared cache mapping.
             * Ditto for __UNICODE(?)
             */
            if (issubregiontype(s, SEG_TEXT)) {
                /* fall through */;
            } else if (issubregiontype(s, SEG_LINKEDIT)) {
                if (r->r_insharedregion)
                    s->s_libent = lesc;
             } else if (issubregiontype(s, "__UNICODE")) {
                if (r->r_insharedregion)
                    s->s_libent = lesc;
            } else
                continue;

            if (s->s_libent)
                s->s_isfileref = true;
        }
    }
    assert(WALK_CONTINUE == retval);

done:
    if (!STAILQ_EMPTY(&srlhead)) {
        struct subregionlist *srl, *trl;
        STAILQ_FOREACH_SAFE(srl, &srlhead, srl_linkage, trl) {
            free(srl);
        }
    }
    return retval;
}

/*
 * Strip region of all decoration
 *
 * Invoked (on every region!) after an error during the initial
 * 'decoration' phase to discard to discard potentially incomplete
 * information.
 */
walk_return_t
undecorate_memory_region(struct region *r, __unused void *arg)
{
    assert(&sparse_ops != r->r_op);
    return r->r_nsubregions ? clean_subregions(r) : WALK_CONTINUE;
}

/*
 * This optimization occurs -after- the vanilla_region_optimizations(),
 * and -after- we've tagged zfod and first-pass fileref's.
 */
walk_return_t
sparse_region_optimization(struct region *r, __unused void *arg)
{
    assert(&sparse_ops != r->r_op);

    if (r->r_inzfodregion) {
        /*
         * Pure zfod region: almost certainly a more compact
         * representation - keep it that way.
         */
        assert(&zfod_ops == r->r_op);
        return clean_subregions(r);
    }

#ifdef CONFIG_REFSC
    if (r->r_fileref) {
        /*
         * Already have a fileref for the whole region: almost
         * certainly a more compact representation - keep
         * it that way.
         */
        assert(&fileref_ops == r->r_op);
        return clean_subregions(r);
    }
#endif

    if (r->r_insharedregion && 0 == r->r_nsubregions) {
        /*
         * A segment in the shared region needs to be
         * identified with an LC_SEGMENT that dyld claims,
         * otherwise (we assert) it's not useful to the dump.
         */
        if (opt->debug) {
            hsize_str_t hstr;
            printr(r, "not referenced in dyld info => "
                   "eliding %s range in shared region\n",
                   str_hsize(hstr, R_SIZE(r)));
        }
        return WALK_DELETE_REGION;
    }

    if (r->r_nsubregions > 1) {
        /*
         * Merge adjacent or identical subregions that have no file reference
         * (Reducing the number of subregions reduces header overhead and
         * improves compressability)
         */
        unsigned i = 1;
        while (i < r->r_nsubregions) {
            struct subregion *s0 = r->r_subregions[i-1];
            struct subregion *s1 = r->r_subregions[i];

            if (s0->s_isfileref) {
                i++;
                continue; /* => destined to be a fileref */
            }
            if (!issamesubregiontype(s0, s1)) {
                i++;
                continue; /* merge-able subregions must have same "type" */
            }

            if (S_ENDADDR(s0) == S_ADDR(s1)) {
                /* directly adjacent subregions */
#if 1
                if (opt->debug)
                    printr(r, "merging subregions (%llx-%llx + %llx-%llx) -- adjacent\n",
                           S_ADDR(s0), S_ENDADDR(s0), S_ADDR(s1), S_ENDADDR(s1));
#endif
                S_SETSIZE(s0, S_ENDADDR(s1) - S_ADDR(s0));
                elide_subregion(r, i);
                continue;
            }

            const mach_vm_size_t pfn[2] = {
                S_ADDR(s0) >> pageshift_host,
                S_ADDR(s1) >> pageshift_host
            };
            const mach_vm_size_t endpfn[2] = {
                (S_ENDADDR(s0) - 1) >> pageshift_host,
                (S_ENDADDR(s1) - 1) >> pageshift_host
            };

            if (pfn[0] == pfn[1] && pfn[0] == endpfn[0] && pfn[0] == endpfn[1]) {
                /* two small subregions share a host page */
#if 1
                if (opt->debug)
                    printr(r, "merging subregions (%llx-%llx + %llx-%llx) -- same page\n",
                           S_ADDR(s0), S_ENDADDR(s0), S_ADDR(s1), S_ENDADDR(s1));
#endif
                S_SETSIZE(s0, S_ENDADDR(s1) - S_ADDR(s0));
                elide_subregion(r, i);
                continue;
            }

            if (pfn[1] == 1 + endpfn[0]) {
                /* subregions are pagewise-adjacent: bigger chunks to compress */
#if 1
                if (opt->debug)
                    printr(r, "merging subregions (%llx-%llx + %llx-%llx) -- adjacent pages\n",
                           S_ADDR(s0), S_ENDADDR(s0), S_ADDR(s1), S_ENDADDR(s1));
#endif
                S_SETSIZE(s0, S_ENDADDR(s1) - S_ADDR(s0));
                elide_subregion(r, i);
                continue;
            }

            i++;    /* this isn't the subregion we're looking for */
        }
    }

    if (r->r_nsubregions)
        r->r_op = &sparse_ops;

    return WALK_CONTINUE;
}
