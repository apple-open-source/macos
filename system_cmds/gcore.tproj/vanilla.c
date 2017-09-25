/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "vm.h"
#include "region.h"
#include "utils.h"
#include "dyld.h"
#include "threads.h"
#include "vanilla.h"
#include "sparse.h"

#include <sys/types.h>
#include <sys/sysctl.h>
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
 * (Another optimization to consider is merging adjacent regions with
 * the same properties.)
 */

static walk_return_t
simple_region_optimization(struct region *r, __unused void *arg)
{
    assert(0 != R_SIZE(r));

    /*
     * Elide unreadable regions
     */
    if ((r->r_info.max_protection & VM_PROT_READ) != VM_PROT_READ) {
        if (OPTIONS_DEBUG(opt, 2))
            printr(r, "eliding unreadable region\n");
        return WALK_DELETE_REGION;
    }
#ifdef CONFIG_SUBMAP
    /*
     * Elide submaps (here for debugging purposes?)
     */
    if (r->r_info.is_submap) {
        if (OPTIONS_DEBUG(opt))
            printr(r, "eliding submap\n");
        return WALK_DELETE_REGION;
    }
#endif
    /*
     * Elide guard regions
     */
    if (r->r_info.protection == VM_PROT_NONE &&
        (VM_MEMORY_STACK == r->r_info.user_tag ||
         VM_MEMORY_MALLOC == r->r_info.user_tag)) {
            if (OPTIONS_DEBUG(opt, 2)) {
                hsize_str_t hstr;
                tag_str_t tstr;
                printr(r, "elide %s - %s\n", str_hsize(hstr, R_SIZE(r)), str_tagr(tstr, r));
            }
            return WALK_DELETE_REGION;
        }

    /*
     * Regions full of clean zfod data e.g. VM_MEMORY_MALLOC_LARGE can be recorded as zfod
     */
    if (r->r_info.share_mode == SM_PRIVATE &&
        0 == r->r_info.external_pager &&
        0 == r->r_info.pages_dirtied) {
        if (OPTIONS_DEBUG(opt, 2)) {
            hsize_str_t hstr;
            tag_str_t tstr;
            printr(r, "convert to zfod %s - %s\n", str_hsize(hstr, R_SIZE(r)), str_tagr(tstr, r));
        }
        r->r_inzfodregion = true;
        r->r_op = &zfod_ops;
    }

    return WALK_CONTINUE;
}

/*
 * (Paranoid validation + debugging assistance.)
 */
void
validate_core_header(const native_mach_header_t *mh, off_t corefilesize)
{
    assert(NATIVE_MH_MAGIC == mh->magic);
    assert(MH_CORE == mh->filetype);

    if (OPTIONS_DEBUG(opt, 2))
        printf("%s: core file: mh %p ncmds %u sizeofcmds %u\n",
               __func__, mh, mh->ncmds, mh->sizeofcmds);

    unsigned sizeofcmds = 0;
    off_t corefilemaxoff = 0;
    const struct load_command *lc = (const void *)(mh + 1);
    for (unsigned i = 0; i < mh->ncmds; i++) {

        if ((uintptr_t)lc < (uintptr_t)mh ||
            (uintptr_t)lc > (uintptr_t)mh + mh->sizeofcmds) {
            warnx("load command %p outside mach header range [%p, 0x%lx]?",
                  lc, mh, (uintptr_t)mh + mh->sizeofcmds);
            abort();
        }
        if (OPTIONS_DEBUG(opt, 2))
            printf("lc %p cmd %3u size %3u ", lc, lc->cmd, lc->cmdsize);
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
                    warnx("bad segment command");
                    abort();
                }
                const off_t endoff = (off_t)sc->fileoff + (off_t)sc->filesize;
                if ((off_t)sc->fileoff > corefilesize || endoff > corefilesize) {
                    /*
                     * We may have run out of space to write the data
                     */
                    warnx("segment command points beyond end of file");
                }
                corefilemaxoff = MAX(corefilemaxoff, endoff);
                break;
            }
            case proto_LC_COREINFO: {
                const struct proto_coreinfo_command *cic = (const void *)lc;
                if (OPTIONS_DEBUG(opt, 2)) {
                    uuid_string_t uustr;
                    uuid_unparse_lower(cic->uuid, uustr);
                    printf("%8s: version %d type %d uuid %s addr %llx dyninfo %llx\n",
                           "COREINFO", cic->version, cic->type, uustr, cic->address, cic->dyninfo);
                }
                if (cic->version < 1 ||
                    cic->type != proto_CORETYPE_USER) {
                    warnx("bad coreinfo command");
                    abort();
                }
                break;
            }
            case proto_LC_FILEREF: {
                const struct proto_fileref_command *frc = (const void *)lc;
                const char *nm = frc->filename.offset + (char *)lc;
                if (OPTIONS_DEBUG(opt, 2)) {
                    printf("%8s: mem %llx-%llx file %lld-%lld %s/%s '%s'\n",
                           "FREF",
                           frc->vmaddr, frc->vmaddr + frc->vmsize,
                           (off_t)frc->fileoff,
                           (off_t)frc->fileoff + (off_t)frc->filesize,
                           str_prot(frc->prot), str_prot(frc->maxprot), nm);
                }
				switch (FREF_ID_TYPE(frc->flags)) {
					case kFREF_ID_UUID:
					case kFREF_ID_MTIMESPEC_LE:
					case kFREF_ID_NONE:
						break;
					default:
						warnx("unknown fref id type: flags %x", frc->flags);
						abort();
				}
                if (nm <= (caddr_t)lc ||
                    nm > (caddr_t)lc + lc->cmdsize ||
                    (off_t)frc->fileoff < 0 || (off_t)frc->filesize < 0) {
                    warnx("bad fileref command");
                    abort();
                }
                break;
           }
           case proto_LC_COREDATA: {
                const struct proto_coredata_command *cc = (const void *)lc;
                if (OPTIONS_DEBUG(opt, 2)) {
                    printf("%8s: mem %llx-%llx file %lld-%lld %s/%s flags %x\n",
                        "COREDATA",
                        cc->vmaddr, cc->vmaddr + cc->vmsize,
                        (off_t)cc->fileoff,
                        (off_t)cc->fileoff + (off_t)cc->filesize,
                        str_prot(cc->prot), str_prot(cc->maxprot), cc->flags);
                }
                if ((off_t)cc->fileoff < mh->sizeofcmds ||
                    (off_t)cc->filesize < 0) {
					warnx("bad COREDATA command");
                    abort();
                }
                const off_t endoff = (off_t)cc->fileoff + (off_t)cc->filesize;
                if ((off_t)cc->fileoff > corefilesize || endoff > corefilesize) {
                    /*
                     * We may have run out of space to write the data
                     */
                    warnx("segment command points beyond end of file");
                }
                corefilemaxoff = MAX(corefilemaxoff, endoff);
                break;
           }
           case LC_THREAD: {
                const struct thread_command *tc = (const void *)lc;
                if (OPTIONS_DEBUG(opt, 2))
                    printf("%8s:\n", "THREAD");
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
                    } else
                        wbuf += count;

                    if (!VALID_THREAD_STATE_FLAVOR(flavor)) {
                        warnx("bad thread state flavor");
                        abort();
                    }
                } while ((caddr_t) wbuf < (caddr_t)tc + tc->cmdsize);
                break;
            }
            default:
                warnx("unknown cmd %u in header", lc->cmd);
                abort();
        }
        if (lc->cmdsize)
            lc = (const void *)((caddr_t)lc + lc->cmdsize);
        else
            break;
    }
    if (corefilemaxoff < corefilesize)
        warnx("unused data after corefile offset %lld", corefilemaxoff);
    if (sizeofcmds != mh->sizeofcmds) {
        warnx("inconsistent mach header %u vs. %u", sizeofcmds, mh->sizeofcmds);
        abort();
    }
}

/*
 * The vanilla Mach-O core file consists of:
 *
 * - A Mach-O header of type MH_CORE
 *
 * A set of load commands of the following types:
 *
 * - LC_SEGMENT{,_64} pointing at memory content in the file,
 *   each chunk consisting of a contiguous region.  Regions may be zfod
 *   (no file content present).
 *
 * - proto_LC_COREDATA pointing at memory content in the file,
 *   each chunk consisting of a contiguous region.  Regions may be zfod
 *   (no file content present) or content may be compressed (experimental)
 *
 * - proto_LC_COREINFO (experimental), pointing at dyld (10.12 onwards)
 *
 * - proto_LC_FILEREF (experimental) pointing at memory
 *   content to be mapped in from another uuid-tagged file at various offsets
 *
 * - LC_THREAD commands with state for each thread
 *
 * These load commands are followed by the relevant contents of memory,
 * pointed to by the various commands.
 */

int
coredump_write(
    const task_t task,
    const int fd,
    struct regionhead *rhead,
    const uuid_t aout_uuid,
    mach_vm_offset_t aout_load_addr,
    mach_vm_offset_t dyld_aii_addr)
{
    struct size_segment_data ssda;
    bzero(&ssda, sizeof (ssda));

    if (walk_region_list(rhead, region_size_memory, &ssda) < 0) {
        warnx(0, "cannot count segments");
        return EX_OSERR;
    }

    unsigned thread_count = 0;
    mach_port_t *threads = NULL;
    kern_return_t ret = task_threads(task, &threads, &thread_count);
    if (KERN_SUCCESS != ret || thread_count < 1) {
        err_mach(ret, NULL, "cannot retrieve threads");
        thread_count = 0;
    }

	if (OPTIONS_DEBUG(opt, 3)) {
		print_memory_region_header();
		walk_region_list(rhead, region_print_memory, NULL);
		printf("\nmach header %lu\n", sizeof (native_mach_header_t));
		printf("threadcount %u threadsize %lu\n", thread_count, thread_count * sizeof_LC_THREAD());
		printf("fileref %lu %lu %llu\n", ssda.ssd_fileref.count, ssda.ssd_fileref.headersize, ssda.ssd_fileref.memsize);
		printf("zfod %lu %lu %llu\n", ssda.ssd_zfod.count, ssda.ssd_zfod.headersize, ssda.ssd_zfod.memsize);
		printf("vanilla %lu %lu %llu\n", ssda.ssd_vanilla.count, ssda.ssd_vanilla.headersize, ssda.ssd_vanilla.memsize);
		printf("sparse %lu %lu %llu\n", ssda.ssd_sparse.count, ssda.ssd_sparse.headersize, ssda.ssd_sparse.memsize);
	}

    size_t headersize = sizeof (native_mach_header_t) +
        thread_count * sizeof_LC_THREAD() +
        ssda.ssd_fileref.headersize +
        ssda.ssd_zfod.headersize +
        ssda.ssd_vanilla.headersize +
        ssda.ssd_sparse.headersize;
    if (opt->extended)
        headersize += sizeof (struct proto_coreinfo_command);

    void *header = calloc(1, headersize);
    if (NULL == header)
        errx(EX_OSERR, "out of memory for header");

    native_mach_header_t *mh = make_corefile_mach_header(header);
    struct load_command *lc = (void *)(mh + 1);

    if (opt->extended) {
        const struct proto_coreinfo_command *cc =
            make_coreinfo_command(mh, lc, aout_uuid, aout_load_addr, dyld_aii_addr);
        lc = (void *)((caddr_t)cc + cc->cmdsize);
    }

    if (opt->verbose) {
        const unsigned long fileref_count = ssda.ssd_fileref.count;
        const unsigned long segment_count = fileref_count +
            ssda.ssd_zfod.count + ssda.ssd_vanilla.count + ssda.ssd_sparse.count;
        printf("Writing %lu segments", segment_count);
        if (0 != fileref_count)
            printf(" (including %lu file reference%s (%lu bytes))",
                   fileref_count, 1 == fileref_count ? "" : "s",
                   ssda.ssd_fileref.headersize);
        printf("\n");
    }

    mach_vm_offset_t pagesize = ((mach_vm_offset_t)1 << pageshift_host);
    mach_vm_offset_t pagemask = pagesize - 1;

    struct write_segment_data wsda = {
        .wsd_task = task,
        .wsd_mh = mh,
        .wsd_lc = lc,
        .wsd_fd = fd,
		.wsd_nocache = false,
        .wsd_foffset = ((mach_vm_offset_t)headersize + pagemask) & ~pagemask,
        .wsd_nwritten = 0,
    };

	int ecode = 0;
    if (0 != walk_region_list(rhead, region_write_memory, &wsda))
        ecode = EX_IOERR;

    del_region_list(rhead);

    struct thread_command *tc = (void *)wsda.wsd_lc;

    for (unsigned t = 0; t < thread_count; t++) {
        dump_thread_state(mh, tc, threads[t]);
        mach_port_deallocate(mach_task_self(), threads[t]);
        tc = (void *)((caddr_t)tc + tc->cmdsize);
    }

    /*
     * Even if we've run out of space, try our best to
     * write out the header.
     */
    if (0 != bounded_pwrite(fd, header, headersize, 0, &wsda.wsd_nocache, NULL))
        ecode = EX_IOERR;
    if (0 == ecode && headersize != sizeof (*mh) + mh->sizeofcmds)
       ecode = EX_SOFTWARE;
    if (0 == ecode)
        wsda.wsd_nwritten += headersize;

    validate_core_header(mh, wsda.wsd_foffset);

    if (ecode)
        warnx("failed to write core file correctly");
    else if (opt->verbose) {
        hsize_str_t hsz;
        printf("Wrote %s to corefile ", str_hsize(hsz, wsda.wsd_nwritten));
        printf("(memory image %s", str_hsize(hsz, ssda.ssd_vanilla.memsize));
        if (ssda.ssd_sparse.memsize)
            printf("+%s", str_hsize(hsz, ssda.ssd_sparse.memsize));
        if (ssda.ssd_fileref.memsize)
            printf(", referenced %s", str_hsize(hsz, ssda.ssd_fileref.memsize));
        if (ssda.ssd_zfod.memsize)
            printf(", zfod %s", str_hsize(hsz, ssda.ssd_zfod.memsize));
        printf(")\n");
    }
    free(header);
    return ecode;
}

static void
addfileref(struct region *r, const struct libent *le, const char *nm)
{
	r->r_fileref = calloc(1, sizeof (*r->r_fileref));
	if (r->r_fileref) {
		if (le) {
			assert(NULL == nm);
			r->r_fileref->fr_libent = le;
			r->r_fileref->fr_pathname = le->le_pathname;
		} else {
			assert(NULL == le);
			r->r_fileref->fr_pathname = strdup(nm);
		}
		r->r_fileref->fr_offset = r->r_pageinfo.offset;
		r->r_op = &fileref_ops;
	}
}

/*
 * Once all the info about the shared cache (filerefs) and the information from
 * dyld (filerefs and subregions), take one last look for mappings
 * of filesystem content to convert to additional filerefs.
 *
 * By default we are pessimistic: read-only mappings on read-only root.
 */
static walk_return_t
label_mapped_files(struct region *r, void *arg)
{
	const struct proc_bsdinfo *pbi = arg;

	if (r->r_fileref || r->r_insharedregion || r->r_incommregion || r->r_inzfodregion)
		return WALK_CONTINUE;
	if (r->r_nsubregions)
		return WALK_CONTINUE;
	if (!r->r_info.external_pager)
		return WALK_CONTINUE;
    if (!opt->allfilerefs) {
        /* must be mapped without write permission */
        if (0 != (r->r_info.protection & VM_PROT_WRITE))
            return WALK_CONTINUE;
    }

	char pathbuf[MAXPATHLEN+1];
	pathbuf[0] = '\0';
	int len = proc_regionfilename(pbi->pbi_pid, R_ADDR(r), pathbuf, sizeof (pathbuf)-1);
	if (len <= 0 || len > MAXPATHLEN)
		return WALK_CONTINUE;
	pathbuf[len] = 0;

#if 0
	/*
	 * On the desktop, only refer to files beginning with particular
	 * prefixes to increase the likelihood that we'll be able to
	 * find the content later on.
	 *
	 * XXX Not practical with a writable root, but helpful for testing.
	 */
	static const char *white[] = {
		"/System",
		"/Library",
		"/usr",
	};
	const unsigned nwhite = sizeof (white) / sizeof (white[0]);
	bool skip = true;
	for (unsigned i = 0; skip && i < nwhite; i++)
		skip = 0 != strncmp(white[i], pathbuf, strlen(white[i]));
	if (skip) {
		if (OPTIONS_DEBUG(opt, 3))
			printf("\t(%s not included)\n", pathbuf);
		return WALK_CONTINUE;
	}
	static const char *black[] = {
		"/System/Library/Caches",
		"/Library/Caches",
		"/usr/local",
	};
	const unsigned nblack = sizeof (black) / sizeof (black[0]);
	for (unsigned i = 0; !skip && i < nblack; i++)
		skip = 0 == strncmp(black[i], pathbuf, strlen(black[i]));
	if (skip) {
		if (OPTIONS_DEBUG(opt, 3))
			printf("\t(%s excluded)\n", pathbuf);
		return WALK_CONTINUE;
	}
#endif

	struct statfs stfs;
	if (-1 == statfs(pathbuf, &stfs)) {
		switch (errno) {
			case EACCES:
			case EPERM:
			case ENOENT:
				break;
			default:
				warnc(errno, "statfs: %s", pathbuf);
				break;
		}
		return WALK_CONTINUE;
	}

	do {
		if (OPTIONS_DEBUG(opt, 2))
			printr(r, "found mapped file %s\n", pathbuf);
        if (!opt->allfilerefs) {
            if ((stfs.f_flags & MNT_ROOTFS) != MNT_ROOTFS)
                break;	// must be on the root filesystem
            if ((stfs.f_flags & MNT_RDONLY) != MNT_RDONLY)
                break;	// must be on a read-only filesystem
        }
		if (OPTIONS_DEBUG(opt, 2))
			print_memory_region(r);
		addfileref(r, NULL, pathbuf);
	} while (0);

	return WALK_CONTINUE;
}

int
coredump(task_t task, int fd, const struct proc_bsdinfo *__unused pbi)
{
    /* this is the shared cache id, if any */
    uuid_t sc_uuid;
    uuid_clear(sc_uuid);

	dyld_process_info dpi = NULL;
	if (opt->extended) {
		dpi = get_task_dyld_info(task);
		if (dpi) {
			get_sc_uuid(dpi, sc_uuid);
		}
	}

    /* this group is for LC_COREINFO */
    mach_vm_offset_t dyld_addr = 0;     // all_image_infos -or- dyld mach header
    mach_vm_offset_t aout_load_addr = 0;
    uuid_t aout_uuid;
    uuid_clear(aout_uuid);

    /*
     * Walk the address space
     */
    int ecode = 0;
    struct regionhead *rhead = coredump_prepare(task, sc_uuid);
    if (NULL == rhead) {
        ecode = EX_OSERR;
        goto done;
    }

    if (OPTIONS_DEBUG(opt, 1))
        printf("Optimizing dump content\n");
    walk_region_list(rhead, simple_region_optimization, NULL);

	if (dpi) {
		/*
		 * Snapshot dyld's info ..
		 */
		if (!libent_build_nametable(task, dpi))
			warnx("error parsing dyld data => ignored");
		else {
			/*
			 * Find the a.out load address and uuid, and the dyld mach header for the coreinfo
			 */
			const struct libent *le;
			if (NULL != (le = libent_lookup_first_bytype(MH_EXECUTE))) {
				aout_load_addr = le->le_mhaddr;
				uuid_copy(aout_uuid, le->le_uuid);
			}
			if (NULL != (le = libent_lookup_first_bytype(MH_DYLINKER))) {
				dyld_addr = le->le_mhaddr;
			}

			/*
			 * Use dyld's view of what's being used in the address
			 * space to shrink the dump.
			 */
			if (OPTIONS_DEBUG(opt, 1))
				printf("Decorating dump with dyld-derived data\n");
			if (0 == walk_region_list(rhead, decorate_memory_region, (void *)dpi)) {
				if (OPTIONS_DEBUG(opt, 1))
					printf("Sparse dump optimization(s)\n");
				walk_region_list(rhead, sparse_region_optimization, NULL);
			} else {
				walk_region_list(rhead, undecorate_memory_region, NULL);
				warnx("error parsing dyld data => ignored");
			}
		}
		free_task_dyld_info(dpi);
	}

	/*
	 * Hunt for any memory mapped files that we can include by reference
	 * Depending on whether the bsd part of the task is still present
	 * we might be able to determine filenames of other regions mapping
	 * them here - this allows fonts, images, and other read-only content
	 * to be converted into file references, further reducing the size
	 * of the dump.
	 *
	 * NOTE: Even though the corpse snapshots the VM, the filesystem is
	 * not correspondingly snapshotted and thus may mutate while the dump
     * proceeds - so be pessimistic about inclusion.
	 */
	if (opt->extended && NULL != pbi) {
		if (OPTIONS_DEBUG(opt, 1))
			printf("Mapped file optimization\n");
		walk_region_list(rhead, label_mapped_files, (void *)pbi);
	}

    if (OPTIONS_DEBUG(opt, 1))
        printf("Optimization(s) done\n");

done:
    if (0 == ecode)
        ecode = coredump_write(task, fd, rhead, aout_uuid, aout_load_addr, dyld_addr);
    return ecode;
}

struct find_shared_cache_args {
    task_t fsc_task;
    vm_object_id_t fsc_object_id;
    vm32_object_id_t fsc_region_object_id;
    uuid_t fsc_uuid;
    const struct libent *fsc_le;
    int fsc_fd;
};

/*
 * This is "find the objid of the first shared cache" in the shared region.
 */
static walk_return_t
find_shared_cache(struct region *r, void *arg)
{
    struct find_shared_cache_args *fsc = arg;

    if (!r->r_insharedregion)
        return WALK_CONTINUE; /* wrong address range! */
    if (0 != r->r_info.user_tag)
        return WALK_CONTINUE; /* must be tag zero */
    if ((VM_PROT_READ | VM_PROT_EXECUTE) != r->r_info.protection ||
        r->r_info.protection != r->r_info.max_protection)
        return WALK_CONTINUE; /* must be r-x / r-x */
    if (r->r_pageinfo.offset != 0)
        return WALK_CONTINUE; /* must map beginning of file */

    if (OPTIONS_DEBUG(opt, 1)) {
        hsize_str_t hstr;
        printr(r, "examining %s shared cache candidate\n", str_hsize(hstr, R_SIZE(r)));
    }

    struct copied_dyld_cache_header *ch;
    mach_msg_type_number_t chlen = sizeof (*ch);
    kern_return_t ret = mach_vm_read(fsc->fsc_task, R_ADDR(r), sizeof (*ch), (vm_offset_t *)&ch, &chlen);

    if (KERN_SUCCESS != ret) {
        err_mach(ret, NULL, "mach_vm_read() candidate shared region header");
        return WALK_CONTINUE;
    }

    uuid_t scuuid;
    if (get_uuid_from_shared_cache_mapping(ch, chlen, scuuid) &&
        uuid_compare(scuuid, fsc->fsc_uuid) == 0) {
        if (OPTIONS_DEBUG(opt, 1)) {
            uuid_string_t uustr;
            uuid_unparse_lower(fsc->fsc_uuid, uustr);
            printr(r, "found shared cache %s here\n", uustr);
        }
        if (!r->r_info.external_pager) {
            if (OPTIONS_DEBUG(opt, 1))
                printf("Hmm. Found shared cache magic# + uuid, but not externally paged?\n");
#if 0
            return WALK_CONTINUE; /* should be "paged" from a file */
#endif
        }
        // This is the ID associated with the first page of the mapping
        fsc->fsc_object_id = r->r_pageinfo.object_id;
        // This is the ID associated with the region
        fsc->fsc_region_object_id = r->r_info.object_id;
    }
    mach_vm_deallocate(mach_task_self(), (vm_offset_t)ch, chlen);
    if (fsc->fsc_object_id) {
        if (OPTIONS_DEBUG(opt, 3)) {
            uuid_string_t uu;
            uuid_unparse_lower(fsc->fsc_uuid, uu);
            printf("Shared cache objid %llx uuid %s\n",
                   fsc->fsc_object_id, uu);
        }
        return WALK_TERMINATE;
    }
    return WALK_CONTINUE;
}

static bool
compare_region_with_shared_cache(const struct region *r, struct find_shared_cache_args *fsc)
{
    struct stat st;
    if (-1 == fstat(fsc->fsc_fd, &st)) {
        if (OPTIONS_DEBUG(opt, 1))
            printr(r, "cannot fstat %s - %s\n",
                   fsc->fsc_le->le_filename, strerror(errno));
        return false;
    }
    void *file = mmap(NULL, R_SIZEOF(r), PROT_READ, MAP_PRIVATE, fsc->fsc_fd, r->r_pageinfo.offset);
    if ((void *)-1L == file) {
        if (OPTIONS_DEBUG(opt, 1))
            printr(r, "mmap %s - %s\n", fsc->fsc_le->le_filename, strerror(errno));
        return false;
    }
    madvise(file, R_SIZEOF(r), MADV_SEQUENTIAL);

    vm_offset_t data = 0;
    mach_msg_type_number_t data_count;
    const kern_return_t kr = mach_vm_read(fsc->fsc_task, R_ADDR(r), R_SIZE(r), &data, &data_count);

    if (KERN_SUCCESS != kr || data_count < R_SIZE(r)) {
        err_mach(kr, r, "mach_vm_read()");
        munmap(file, R_SIZEOF(r));
        return false;
    }

    mach_vm_size_t cmpsize = data_count;

#ifdef RDAR_23744374
    /*
     * Now we have the corresponding regions mapped, we should be
     * able to compare them.  There's just one last twist that relates
     * to heterogenous pagesize systems: rdar://23744374
     */
    if (st.st_size < (off_t)(r->r_pageinfo.offset + cmpsize) &&
        pageshift_host < pageshift_app) {
        /*
         * Looks like we're about to map close to the end of the object.
         * Check what's really mapped there and reduce the size accordingly.
         */
        if (!is_actual_size(fsc->fsc_task, r, &cmpsize)) {
            if (OPTIONS_DEBUG(opt, 3))
                printr(r, "narrowing the comparison (%llu "
                       "-> %llu)\n", R_SIZE(r), cmpsize);
        }
    }
#endif

    mach_vm_behavior_set(mach_task_self(), data, data_count, VM_BEHAVIOR_SEQUENTIAL);

    const bool thesame = memcmp(file, (void *)data, (size_t)cmpsize) == 0;
#if 0
    if (!thesame) {
        int diffcount = 0;
        int samecount = 0;
        const char *f = file;
        const char *d = (void *)data;
        for (mach_vm_size_t off = 0; off < cmpsize; off += 4096) {
            if (memcmp(f, d, 4096) != 0) {
                diffcount++;
            } else samecount++;
            f += 4096;
            d += 4096;
        }
        if (diffcount)
            printr(r, "%d of %d pages different\n", diffcount, diffcount + samecount);
    }
#endif
    mach_vm_deallocate(mach_task_self(), data, data_count);
    munmap(file, R_SIZEOF(r));

    if (!thesame && OPTIONS_DEBUG(opt, 3))
        printr(r, "mapped file (%s) region is modified\n", fsc->fsc_le->le_filename);
    return thesame;
}

static walk_return_t
label_shared_cache(struct region *r, void *arg)
{
    struct find_shared_cache_args *fsc = arg;

    if (!r->r_insharedregion)
        return WALK_CONTINUE;
    if (!r->r_info.external_pager)
        return WALK_CONTINUE;
    if (r->r_pageinfo.object_id != fsc->fsc_object_id) {
        /* wrong object, or first page already modified */
        return WALK_CONTINUE;
    }
    if (((r->r_info.protection | r->r_info.max_protection) & VM_PROT_WRITE) != 0) {
        /* potentially writable, but was it written? */
        if (0 != r->r_info.pages_dirtied)
            return WALK_CONTINUE;
        if (0 != r->r_info.pages_swapped_out)
            return WALK_CONTINUE;
        if (0 != r->r_info.pages_resident && !r->r_info.external_pager)
            return WALK_CONTINUE;
		if (OPTIONS_DEBUG(opt, 1))
			printr(r, "verifying shared cache content against memory image\n");
        if (!compare_region_with_shared_cache(r, fsc)) {
            /* bits don't match */
			if (OPTIONS_DEBUG(opt, 1))
				printr(r, "hmm .. mismatch: using memory image\n");
            return WALK_CONTINUE;
        }
	}

    /*
     * This mapped file segment will be represented as a reference
     * to the file, rather than as a copy of the mapped file.
     */
	addfileref(r, libent_lookup_byuuid(fsc->fsc_uuid), NULL);
	return WALK_CONTINUE;
}

struct regionhead *
coredump_prepare(task_t task, uuid_t sc_uuid)
{
    struct regionhead *rhead = build_region_list(task);

    if (OPTIONS_DEBUG(opt, 2)) {
		printf("Region list built\n");
        print_memory_region_header();
        walk_region_list(rhead, region_print_memory, NULL);
    }

    if (uuid_is_null(sc_uuid))
        return rhead;

    /*
     * Name the shared cache, if we can
     */
    char *nm = shared_cache_filename(sc_uuid);
    const struct libent *le;

    if (NULL != nm)
        le = libent_insert(nm, sc_uuid, 0, NULL, NULL, 0);
    else {
        libent_insert("(anonymous shared cache)", sc_uuid, 0, NULL, NULL, 0);
        if (opt->verbose){
			printf("Warning: cannot name the shared cache ");
			if (OPTIONS_DEBUG(opt, 1)) {
				uuid_string_t uustr;
				uuid_unparse_lower(sc_uuid, uustr);
				printf("(%s) ", uustr);
			}
			printf("- dump may be large!\n");
        }
        return rhead;
    }

    if (opt->extended) {
        /*
         * See if we can replace entire regions with references to the shared cache
         * by looking at the VM meta-data about those regions.
         */
        if (OPTIONS_DEBUG(opt, 1)) {
            uuid_string_t uustr;
            uuid_unparse_lower(sc_uuid, uustr);
            printf("Searching for shared cache with uuid %s\n", uustr);
        }

        /*
         * Identify the regions mapping the shared cache by comparing the UUID via
         * dyld with the UUID of likely-looking mappings in the right address range
         */
        struct find_shared_cache_args fsca;
        bzero(&fsca, sizeof (fsca));
        fsca.fsc_task = task;
        uuid_copy(fsca.fsc_uuid, sc_uuid);
        fsca.fsc_fd = -1;

        walk_region_list(rhead, find_shared_cache, &fsca);

        if (0 == fsca.fsc_object_id) {
            printf("Cannot identify the shared cache region(s) => ignored\n");
        } else {
            if (opt->verbose)
                printf("Referenced %s\n", nm);
            fsca.fsc_le = le;
            fsca.fsc_fd = open(fsca.fsc_le->le_pathname, O_RDONLY);
            if (-1 == fsca.fsc_fd)
                errc(EX_SOFTWARE, errno, "open %s", fsca.fsc_le->le_pathname);
            else {
                walk_region_list(rhead, label_shared_cache, &fsca);
                close(fsca.fsc_fd);
            }
            free(nm);
        }
    }

    return rhead;
}
