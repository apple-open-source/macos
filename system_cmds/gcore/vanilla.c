/*
 * Copyright (c) 2025 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "vm.h"
#include "region.h"
#include "utils.h"
#include "threads.h"
#include "vanilla.h"
#include "sparse.h"
#include "notes.h"

#include <os/overflow.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/mount.h>
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
#include <sysexits.h>

#include <mach/mach.h>

/*
 * (Paranoid validation + debugging assistance.)
 */
static void
validate_core_header(const native_mach_header_t *mh, off_t corefilesize, int fd)
{
    assert(NATIVE_MH_MAGIC == mh->magic);
    assert(MH_CORE == mh->filetype);

    if (OPTIONS_DEBUG(opt, 2)) {
        printf("%s: core file: mh %p ncmds %u sizeofcmds %u\n",
               __func__, mh, mh->ncmds, mh->sizeofcmds);
    }
    unsigned sizeofcmds = 0;
    off_t corefilemaxoff = 0;
    const struct load_command *lc = (const void *)(mh + 1);
    for (unsigned i = 0; i < mh->ncmds; i++) {

        if ((uintptr_t)lc < (uintptr_t)mh ||
            (uintptr_t)lc > (uintptr_t)mh + mh->sizeofcmds) {
            os_log_error(glog, "load command %p outside mach header range [%p, 0x%lx]?",
                  lc, mh, (uintptr_t)mh + mh->sizeofcmds);
            abort();
        }
        if (OPTIONS_DEBUG(opt, 2)) {
            printf("lc %p cmd %3u size %3u ", lc, lc->cmd, lc->cmdsize);
        }
        sizeofcmds += lc->cmdsize;

        switch (lc->cmd) {
            case NATIVE_LC_SEGMENT: {
                const native_segment_command_t *sc = (const void *)lc;
                if (OPTIONS_DEBUG(opt, 2)) {
                    printf("%8s: mem %llx-%llx file %lld-%lld %s/%s nsect %u flags %x\n",
                           "SEGMENT",
                           (mach_vm_offset_t)sc->vmaddr,
                           (mach_vm_offset_t)sc->vmaddr + sc->vmsize,
                           (off_t)sc->fileoff,
                           (off_t)sc->fileoff + (off_t)sc->filesize,
                           str_prot(sc->initprot), str_prot(sc->maxprot),
                           sc->nsects, sc->flags);
                }
                if ((off_t)sc->fileoff < mh->sizeofcmds ||
                    (off_t)sc->filesize < 0) {
                    os_log_error(glog, "bad segment command at 0x%llx",
                        (mach_vm_offset_t)sc->vmaddr);
                    abort();
                }
                const off_t endoff = (off_t)sc->fileoff + (off_t)sc->filesize;
                if ((off_t)sc->fileoff > corefilesize || endoff > corefilesize) {
                    /*
                     * Not necessarily a logic bug; may have run out of
                     * space to write the data
                     */
                    os_log_error(glog, "segment command points beyond end of file");
                }
                corefilemaxoff = MAX(corefilemaxoff, endoff);
                break;
            }

            case LC_THREAD: {
                const struct thread_command *tc = (const void *)lc;
                if (OPTIONS_DEBUG(opt, 2)) {
                    printf("%8s:\n", "THREAD");
                }
                uint32_t *wbuf = (void *)(tc + 1);
                do {
                    const uint32_t flavor = *wbuf++;
                    const uint32_t count = *wbuf++;

                    if (OPTIONS_DEBUG(opt, 2)) {
                        printf("  flavor %u count %u\n", flavor, count);
                        if (count) {
                            bool nl = false;
                            for (unsigned k = 0; k < count; k++) {
                                if (0 == (k & 7))
                                    printf("  [%3u] ", k);
                                printf("%08x ", *wbuf++);
                                if (7 == (k & 7)) {
                                    printf("\n");
                                    nl = true;
                                } else
                                    nl = false;
                            }
                            if (!nl)
                                printf("\n");
                        }
                    } else {
                        wbuf += count;
                    }
                    if (!VALID_THREAD_STATE_FLAVOR(flavor)) {
                        os_log_error(glog, "bad thread state flavor %u", flavor);
                        abort();
                    }
                } while ((caddr_t) wbuf < (caddr_t)tc + tc->cmdsize);
                break;
            }
            case LC_NOTE: {
                const struct note_command *nc = (const void *)lc;
                // Note: nc->data_owner[] bytes are not C strings
                // e.g. "process metadata" occupies all 16 bytes!
                char owner[sizeof(nc->data_owner) + 1];
                memcpy(owner, nc->data_owner, sizeof(nc->data_owner));
                owner[sizeof(nc->data_owner)] = 0;

                if (OPTIONS_DEBUG(opt, 2)) {
                    printf("%8s: data at %lld for %llu, owner '%s'\n",
                        "NOTE", nc->offset, nc->size, owner);
                }
                uint64_t endoff = UINT64_MAX;
                if (os_add_overflow(nc->offset, nc->size, &endoff) ||
                    endoff > (uint64_t)corefilesize) {
                    os_log_error(glog, "'%{public}s' LC_NOTE data does not fit in the file", owner);
                    abort();
                }
                if (fd >= 0) {
                    /* Only works if the fd is readable and mmapable! */
                    const int flags = fcntl(fd, F_GETFL);
                    if (flags > 0 && (flags & O_RDWR) == O_RDWR) {
                        validate_note_content(nc, owner, fd);
                    }
                }
                break;
            }
            default:
                os_log_error(glog, "unknown cmd %u in header", lc->cmd);
                abort();
        }
        if (lc->cmdsize)  {
            lc = (const void *)((caddr_t)lc + lc->cmdsize);
        } else {
            break;
        }
    }
    if (corefilemaxoff < corefilesize) {
        os_log_error(glog, "unused data after corefile offset %lld",
            corefilemaxoff);
    }
    if (sizeofcmds != mh->sizeofcmds) {
        os_log_error(glog, "inconsistent mach header %u vs. %u",
            sizeofcmds, mh->sizeofcmds);
        abort();
    }
}

static void
print_dump_stats(
    const struct write_segment_data *wsda,
    const struct size_core *vanilla,
    const struct size_core *sparse)
{
    char *dtype;
    switch (opt->content) {
        case CONTENT_FULL:
            dtype = "";
            break;
        case CONTENT_STACK:
            dtype = "stack-only ";
            break;
        case CONTENT_COMPACT:
            dtype = "modified-memory ";
            break;
    }
    hsize_str_t hsz;
    printf("Wrote %sB to %score file ", str_hsize(hsz, wsda->wsd_nwritten),
        dtype);
    printf("(%lu entries", vanilla->count + sparse->count);
    const mach_vm_offset_t memsize = vanilla->memsize + sparse->memsize;
    if (memsize) {
        printf(", %sB data", str_hsize(hsz, memsize));
    }
    if (sparse->zfodsize) {
        printf(", %sB zfod", str_hsize(hsz, sparse->zfodsize));
    }
    printf(")\n");
}

/*
 * The vanilla Mach-O core file consists of:
 *
 * - A Mach-O header of type MH_CORE
 *
 * - A set of LC_NOTE commands of various flavors, with some pointing
 *   at sections of the file after the following load commands
 *   but before their data.
 *
 * - A set of load commands of the following types:
 *
 *   LC_SEGMENT{,_64} pointing at memory content in the file,
 *   each chunk consisting of a contiguous region. Segments may be zfod,
 *   i.e. filesize == 0, although rdar://139678104 needs to be
 *   addressed for such segments to be treated as all-zero by lldb.
 *
 *   One or more LC_THREAD commands with state for each thread
 *
 * These load commands are followed by some LC_NOTE content, then
 * the relevant contents of memory, pointed to by the load commands.
 */

struct notebook {
    struct task_crashinfo_note_data *task_crashinfo_note_data;
    struct region_infos_note_data *region_infos_note_data;
    struct note_addrable_bits *addrable_bits_note_data;
    struct all_image_infos_note_data *all_image_infos_note_data;
    struct process_metadata_note_data *process_metadata_note_data;
};

static int
coredump_pwrite(
    const task_t task,
    const mach_port_t *threads,
    const unsigned thread_count,
    const int fd,
    struct regionhead *rhead,
    const struct notebook *nb)
{
    struct size_segment_data ssda;
    bzero(&ssda, sizeof (ssda));

    if (walk_region_list(rhead, size_memory_region, &ssda) < 0) {
        os_log_error(glog, "cannot count memory regions");
        return EX_OSERR;
    }

	if (OPTIONS_DEBUG(opt, 3)) {
		print_memory_region_header();
		walk_region_list(rhead, region_print_memory, NULL);
		printf("\nmach header %lu\n", sizeof (native_mach_header_t));
		printf("thread #%u threadsize %lu\n",
            thread_count, thread_count * sizeof_LC_THREAD());
		printf("vanilla #%lu hdr %lu mem %llu\n", ssda.ssd_vanilla.count,
            ssda.ssd_vanilla.headersize, ssda.ssd_vanilla.memsize);
		printf("sparse #%lu hdr %lu mem %llu zfod %llu\n",
            ssda.ssd_sparse.count, ssda.ssd_sparse.headersize,
            ssda.ssd_sparse.memsize, ssda.ssd_sparse.zfodsize);
	}

    size_t headersize = sizeof (native_mach_header_t) +
        thread_count * sizeof_LC_THREAD() +
        ssda.ssd_vanilla.headersize +
        ssda.ssd_sparse.headersize;

    off_t datasize = (off_t)ssda.ssd_vanilla.memsize +
        (off_t)ssda.ssd_sparse.memsize;

    if (nb->addrable_bits_note_data) {
        headersize += sizeof(struct note_command);
        datasize += size_addrable_bits_note(nb->addrable_bits_note_data);
    }
    if (nb->all_image_infos_note_data) {
        headersize += sizeof(struct note_command);
        datasize += size_all_image_infos_note(nb->all_image_infos_note_data);
    }
    if (nb->process_metadata_note_data) {
        headersize += sizeof(struct note_command);
        datasize += size_process_metadata_note(nb->process_metadata_note_data);
    }
    if (nb->task_crashinfo_note_data) {
        headersize += sizeof(struct note_command);
        datasize += size_task_crashinfo_note(nb->task_crashinfo_note_data);
    }
    if (nb->region_infos_note_data) {
        headersize += sizeof(struct note_command);
        datasize += size_region_infos_note(nb->region_infos_note_data);
    }

    /*
     * headersize is the number of bytes actually needed in the mach header,
     * while headersize_aligned is the number of bytes occupied by it in
     * the file, computed by rounding up to the nearest host page boundary.
     */
    const mach_vm_offset_t pagesize = ((mach_vm_offset_t)1 << pageshift_host);
    const mach_vm_offset_t pagemask = pagesize - 1;
    const size_t headersize_aligned = (size_t)
        (((mach_vm_offset_t)headersize + pagemask) & ~pagemask);

    if (opt->sizebound) {
        /*
         * Will it fit?
         */
        if (OPTIONS_DEBUG(opt, 1)) {
            printf("Core file size %lu+%lld limit %lld\n",
                headersize_aligned, datasize, opt->sizebound);
        }
        off_t filesize = OFF_MAX;
        if (os_add_overflow((off_t)headersize_aligned, datasize, &filesize) ||
            filesize > opt->sizebound) {
            hsize_str_t hstr;
            os_log_error(glog, "Core file size (%{public}s) will exceed specified bound",
                str_hsize(hstr, headersize_aligned + datasize));
            return EX_IOERR;
        }
    }

    void *header = calloc(1, headersize);
    if (NULL == header) {
        os_log_error(glog, "out of memory for header");
        return EX_OSERR;
    }

    native_mach_header_t *mh = make_corefile_mach_header(header,
        get_aout_mach_header(task));
    struct load_command *lc = (void *)(mh + 1);

    if (OPTIONS_DEBUG(opt, 1)) {
        const unsigned long segment_count =
            ssda.ssd_vanilla.count + ssda.ssd_sparse.count;
        printf("Writing %lu segments\n", segment_count);
    }

    struct write_segment_data wsda = {
        .wsd_task = task,
        .wsd_mh = mh,
        .wsd_lc = lc,
        .wsd_fd = fd,
		.wsd_nocache = false,
        .wsd_foffset = headersize_aligned,
        .wsd_nwritten = 0,
    };

    if (nb->addrable_bits_note_data) {
        const struct note_command *nc = make_addrable_bits_note(mh, lc, &wsda, nb->addrable_bits_note_data);
        if (nc) {
            lc = (void *)nc + nc->cmdsize;
            wsda.wsd_lc = lc; /* make sure the lc is always kept up-to-date */
        }
    }

    if (nb->process_metadata_note_data) {
        const struct note_command *nc = make_process_metadata_note(mh, lc, &wsda,
            nb->process_metadata_note_data);
        if (nc) {
            lc = (void *)nc + nc->cmdsize;
            wsda.wsd_lc = lc;
        }
    }
    
    if (nb->all_image_infos_note_data) {
        const struct note_command *nc = make_all_image_infos_note(mh, lc, &wsda,
            nb->all_image_infos_note_data);
        if (nc) {
            lc = (void *)nc + nc->cmdsize;
            wsda.wsd_lc = lc;
        }
    }

    if (nb->task_crashinfo_note_data) {
        assert(NULL != nb->region_infos_note_data);
        
        /* cast to void* first to avoid alignment warnings */
        const struct note_command *nc = make_task_crashinfo_note(mh, (struct note_command*)(void*)lc, &wsda, nb->task_crashinfo_note_data);
        if (nc) {
            lc = (void *)((caddr_t)nc + nc->cmdsize);
            wsda.wsd_lc = lc; /* make sure the lc is always kept up-to-date */
        }
    }
    
    if (nb->region_infos_note_data) {
        assert(NULL != nb->task_crashinfo_note_data);
        
        const struct note_command *nc = make_region_infos_note(mh, (struct note_command*)(void*)lc, &wsda, nb->region_infos_note_data);
        if (nc) {
            lc = (void *)((caddr_t)nc + nc->cmdsize);
            wsda.wsd_lc = lc; /* make sure the lc is always kept up-to-date */
        }
    }
    
    int ecode = 0;
    if (0 != walk_region_list(rhead, pwrite_memory_region, &wsda)) {
        ecode = EX_IOERR;
    }
    del_region_list(rhead);

    struct thread_command *tc = (void *)wsda.wsd_lc;

    for (unsigned t = 0; t < thread_count; t++) {
        dump_thread_state(mh, tc, threads[t]);
        tc = (void *)((caddr_t)tc + tc->cmdsize);
    }

    /*
     * Even if we've run out of space, try our best to
     * write out the header at the beginning of the dump.
     */
    if (0 != bounded_pwrite(fd, header, headersize, 0, &wsda.wsd_nocache, NULL)) {
        ecode = EX_IOERR;
    }
    if (0 == ecode && headersize != sizeof (*mh) + mh->sizeofcmds) {
        ecode = EX_SOFTWARE;
    }
    if (0 == ecode) {
        /*
         * The filesystem fills [headersize, headersize_aligned)
         * with zeroes - either implicitly (as a hole after the first
         * notes and segments were written beyond the header region)
         * or explicitly. Account for that here.
         */
        wsda.wsd_nwritten += headersize_aligned;
    }
    validate_core_header(mh, wsda.wsd_foffset, ecode == 0 ? fd : -1);

    if (ecode) {
        os_log_error(glog, "failed to write core file correctly");
    } else if (opt->verbose) {
        print_dump_stats(&wsda, &ssda.ssd_vanilla, &ssda.ssd_sparse);
    }
    free(header);
    return ecode;
}

/*
 * Like coredump_write() above, but arranges to stream the core dump
 * sequentially so that it can be e.g. compressed on the fly via a pipe.
 */
static int
coredump_stream(
    const task_t task,
    const mach_port_t *threads,
    const unsigned thread_count,
    const int fd,
    struct regionhead *rhead,
    const struct notebook *__unused nb)
{
    struct size_segment_data ssda;
    bzero(&ssda, sizeof (ssda));

    if (walk_region_list(rhead, size_memory_region, &ssda) < 0) {
        os_log_error(glog, "cannot count memory regions");
        return EX_OSERR;
    }

    if (OPTIONS_DEBUG(opt, 3)) {
        print_memory_region_header();
        walk_region_list(rhead, region_print_memory, NULL);
        printf("\nmach header %lu\n", sizeof (native_mach_header_t));
        printf("thread #%u threadsize %lu\n",
            thread_count, thread_count * sizeof_LC_THREAD());
        printf("vanilla #%lu hdr %lu mem %llu\n", ssda.ssd_vanilla.count,
            ssda.ssd_vanilla.headersize, ssda.ssd_vanilla.memsize);
        printf("sparse #%lu hdr %lu mem %llu zfod %llu\n",
            ssda.ssd_sparse.count, ssda.ssd_sparse.headersize,
            ssda.ssd_sparse.memsize, ssda.ssd_sparse.zfodsize);
    }

    const size_t headersize = sizeof (native_mach_header_t) +
        thread_count * sizeof_LC_THREAD() +
        ssda.ssd_vanilla.headersize + ssda.ssd_sparse.headersize;

    void *header = calloc(1, headersize);
    if (NULL == header) {
        os_log_error(glog, "out of memory for header");
        return EX_OSERR;
    }

    native_mach_header_t *mh = make_corefile_mach_header(header,
        get_aout_mach_header(task));
    struct load_command *lc = (void *)(mh + 1);

    if (opt->verbose) {
        const unsigned long segment_count =
            ssda.ssd_vanilla.count + ssda.ssd_sparse.count;
        printf("Writing %lu segments\n", segment_count);
    }

    const mach_vm_offset_t pagesize = ((mach_vm_offset_t)1 << pageshift_host);
    const mach_vm_offset_t pagemask = pagesize - 1;
    const mach_vm_offset_t data_offset = ((mach_vm_offset_t)headersize + pagemask) & ~pagemask;

    struct write_segment_data wsda = {
        .wsd_task = task,
        .wsd_mh = mh,
        .wsd_lc = lc,
        .wsd_fd = fd,
        .wsd_nocache = false,
        .wsd_foffset = data_offset,
        .wsd_nwritten = 0,
    };

    int ecode = 0;
    do {
        if (0 != walk_region_list(rhead, makeheader_memory_region, &wsda)) {
            ecode = EX_IOERR;
            break;
        }

        struct thread_command *tc = (void *)wsda.wsd_lc;

        for (unsigned t = 0; t < thread_count; t++) {
            dump_thread_state(mh, tc, threads[t]);
            tc = (void *)((caddr_t)tc + tc->cmdsize);
        }

        if (headersize != sizeof (*mh) + mh->sizeofcmds) {
            ecode = EX_SOFTWARE;
            break;
        }
        if (0 != bounded_write(fd, header, headersize, NULL)) {
            ecode = EX_IOERR;
            break;
        }

        // Write zeroes beyond the header to the next page boundary
        if (0 != bounded_write_zero(fd, (size_t)(data_offset - headersize), NULL)) {
            ecode = EX_IOERR;
            break;
        }
        wsda.wsd_foffset = data_offset;
        wsda.wsd_nwritten = data_offset;

        if (0 != walk_region_list(rhead, stream_memory_region, &wsda)) {
            ecode = EX_IOERR;
            break;
        }
        validate_core_header(mh, wsda.wsd_foffset, -1);
        del_region_list(rhead);
    } while (0);

    if (ecode) {
        os_log_error(glog, "failed to write core file correctly");
    } else if (opt->verbose) {
        print_dump_stats(&wsda, &ssda.ssd_vanilla, &ssda.ssd_sparse);
    }
    free(header);
    return ecode;
}

int
coredump(task_t task, int fd, const struct proc_bsdinfo *pbi)
{
    struct notebook notebook = { 0 }, *nb = &notebook;

    nb->addrable_bits_note_data = prepare_addrable_bits_note();
    nb->all_image_infos_note_data = prepare_all_image_infos_note(task);
    if (opt->notes) {
        nb->task_crashinfo_note_data = prepare_task_crashinfo_note(task);
        nb->region_infos_note_data = prepare_region_infos_note(task);
    }

    /*
     * Get the thread ports
     */
    unsigned thread_count = 0;
    mach_port_t *threads = NULL;
    kern_return_t ret = task_threads(task, &threads, &thread_count);
    if (KERN_SUCCESS != ret || thread_count < 1) {
        err_mach(ret, NULL, "cannot retrieve threads");
        thread_count = 0;
    }

    if (pbi && !task_port_is_corpse(task)) {
        /*
         * Corpse threads are assigned different unique thread IDs than the
         * original process. These are thus different from the ones the
         * process might've introspected about itself, so only add the
         * "process metadata" note when a live process is being dumped.
         */
        nb->process_metadata_note_data =
            prepare_process_metadata_note(threads, thread_count);
    }

    /*
     * Walk the address space
     */
    int ecode = 0;
    struct regionhead *rhead = build_region_list(task, false);
    if (OPTIONS_DEBUG(opt, 2)) {
        printf("Region list built\n");
        print_memory_region_header();
        walk_region_list(rhead, region_print_memory, NULL);
    }
    if (NULL == rhead) {
        ecode = EX_OSERR;
        goto done;
    }

    if (OPTIONS_DEBUG(opt, 2)) {
        printf("Optimizing dump content\n");
    }
    
    /*
     * elide unreadable page, submaps, guard pages,
     * and "owned unmapped" regions
     */
    simple_regionlist_optimization(rhead);

    switch (opt->content) {
        case CONTENT_STACK:
            retain_only_stack(rhead);
            retain_only_modified(rhead, task);
            break;
        case CONTENT_COMPACT:
            retain_only_modified(rhead, task);
            break;
        case CONTENT_FULL:
            break;
    }

    if (OPTIONS_DEBUG(opt, 2)) {
        printf("Optimization(s) done\n");
    }
done:
    if (0 == ecode) {
        if (opt->stream) {
            ecode = coredump_stream(task, threads, thread_count, fd, rhead, nb);
        } else {
            ecode = coredump_pwrite(task, threads, thread_count, fd, rhead, nb);
        }
    }
    
    for (unsigned t = 0; t < thread_count; t++) {
        mach_port_deallocate(mach_task_self(), threads[t]);
    }

    if (nb->addrable_bits_note_data) {
        destroy_addrable_bits_note_data(nb->addrable_bits_note_data);
    }

    if (nb->all_image_infos_note_data) {
        destroy_all_image_infos_note_data(nb->all_image_infos_note_data);
    }

    if (nb->process_metadata_note_data) {
        destroy_process_metadata(nb->process_metadata_note_data);
    }

    if (nb->task_crashinfo_note_data) {
        destroy_task_crash_info_note_data(nb->task_crashinfo_note_data);
    }

    if (nb->region_infos_note_data) {
        destroy_region_infos_note_data(nb->region_infos_note_data);
    }

    return ecode;
}
