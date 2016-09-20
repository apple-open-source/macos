/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "corefile.h"
#include "sparse.h"
#include "utils.h"
#include "vm.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <compression.h>
#include <sys/param.h>
#include <libgen.h>

native_mach_header_t *
make_corefile_mach_header(void *data)
{
    native_mach_header_t *mh = data;
    mh->magic = NATIVE_MH_MAGIC;
    mh->filetype = MH_CORE;
#if defined(__LP64__)
    const int is64 = 1;
#else
    const int is64 = 0;
#endif
#if defined(__i386__) || defined(__x86_64__)
    mh->cputype = is64 ? CPU_TYPE_X86_64 : CPU_TYPE_I386;
    mh->cpusubtype = is64 ? CPU_SUBTYPE_X86_64_ALL : CPU_SUBTYPE_I386_ALL;
#elif defined(__arm__) || defined(__arm64__)
    mh->cputype = is64 ? CPU_TYPE_ARM64 : CPU_TYPE_ARM;
    mh->cpusubtype = is64 ? CPU_SUBTYPE_ARM64_ALL : CPU_SUBTYPE_ARM_ALL;
#else
#error undefined
#endif
    return mh;
}

struct proto_coreinfo_command *
make_coreinfo_command(native_mach_header_t *mh, void *data, const uuid_t aoutid, uint64_t address, uint64_t dyninfo)
{
    struct proto_coreinfo_command *cc = data;
    cc->cmd = proto_LC_COREINFO;
    cc->cmdsize = sizeof (*cc);
    cc->version = 1;
    cc->type = proto_CORETYPE_USER;
    cc->address = address;
    uuid_copy(cc->uuid, aoutid);
    cc->dyninfo = dyninfo;
    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, cc->cmdsize);
    return cc;
}

static native_segment_command_t *
make_native_segment_command(void *data, mach_vm_offset_t vmaddr, mach_vm_size_t vmsize, off_t fileoff, size_t filesize, unsigned maxprot, unsigned initprot, unsigned comptype)
{
    native_segment_command_t *sc = data;
    sc->cmd = NATIVE_LC_SEGMENT;
    sc->cmdsize = sizeof (*sc);
    assert(vmsize);
#if defined(__LP64__)
    sc->vmaddr = vmaddr;
    sc->vmsize = vmsize;
    sc->fileoff = fileoff;
#else
    sc->vmaddr = (uintptr_t)vmaddr;
    sc->vmsize = (size_t)vmsize;
    sc->fileoff = (long)fileoff;
#endif
    sc->filesize = filesize;
    sc->maxprot = maxprot;
    sc->initprot = initprot;
    sc->nsects = 0;
    sc->flags = proto_SG_COMP_MAKE_FLAGS(comptype);
    return sc;
}

/*
 * Increment the mach-o header data when we succeed
 */
static void
commit_load_command(struct write_segment_data *wsd, const struct load_command *lc)
{
    wsd->wsd_lc = (caddr_t)lc + lc->cmdsize;
    native_mach_header_t *mh = wsd->wsd_mh;
    mach_header_inc_ncmds(mh, 1);
    mach_header_inc_sizeofcmds(mh, lc->cmdsize);
}

#pragma mark -- Regions written as "file references" --

static size_t
cmdsize_fileref_command(const char *nm)
{
    size_t cmdsize = sizeof (struct proto_fileref_command);
    size_t len;
    if (0 != (len = strlen(nm))) {
        len++; // NUL-terminated for mmap sanity
        cmdsize += roundup(len, sizeof (long));
    }
    return cmdsize;
}

static void
size_fileref_subregion(const struct subregion *s, struct size_core *sc)
{
    assert(S_LIBENT(s));

    size_t cmdsize = cmdsize_fileref_command(S_PATHNAME(s));
    sc->headersize += cmdsize;
    sc->count++;
    sc->memsize += S_SIZE(s);
}

#ifdef CONFIG_REFSC
static void
size_fileref_region(const struct region *r, struct size_core *sc)
{
    assert(0 == r->r_nsubregions);
    assert(!r->r_inzfodregion);

    size_t cmdsize = cmdsize_fileref_command(r->r_fileref->fr_libent->le_pathname);
    sc->headersize += cmdsize;
    sc->count++;
    sc->memsize += R_SIZE(r);
}
#endif

static struct proto_fileref_command *
make_fileref_command(void *data, const struct libent *le, mach_vm_offset_t vmaddr, mach_vm_size_t vmsize, off_t fileoff, off_t filesize, unsigned maxprot, unsigned initprot)
{
    struct proto_fileref_command *fr = data;
    size_t len;

    fr->cmd = proto_LC_FILEREF;
    fr->cmdsize = sizeof (*fr);
    if (0 != (len = strlen(le->le_pathname))) {
        /*
         * Strings live immediately after the
         * command, and are included in the cmdsize
         */
        fr->filename.offset = sizeof (*fr);
        void *s = fr + 1;
        strlcpy(s, le->le_pathname, ++len); // NUL-terminated for mmap sanity
        fr->cmdsize += roundup(len, sizeof (long));
        assert(cmdsize_fileref_command(le->le_pathname) == fr->cmdsize);
    }
    uuid_copy(fr->uuid, le->le_uuid);

    fr->vmaddr = vmaddr;

    assert(vmsize);
    fr->vmsize = vmsize;
    assert(fileoff >= 0);
    fr->fileoff = fileoff;
    fr->filesize = filesize;

    assert(maxprot & VM_PROT_READ);
    fr->maxprot = maxprot;
    fr->initprot = initprot;
    return fr;
}

/*
 * It's almost always more efficient to write out a reference to the
 * data than write out the data itself.
 */
static walk_return_t
write_fileref_subregion(const struct region *r, const struct subregion *s, struct write_segment_data *wsd)
{
    assert(S_LIBENT(s));
    if (opt->debug && !issubregiontype(s, SEG_TEXT) && !issubregiontype(s, SEG_LINKEDIT))
        printf("%s: unusual segment type %s from %s\n", __func__, S_MACHO_TYPE(s), S_FILENAME(s));
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    assert((r->r_info.protection & VM_PROT_WRITE) == 0);

    const struct libent *le = S_LIBENT(s);
    const struct proto_fileref_command *fc = make_fileref_command(wsd->wsd_lc, le, S_ADDR(s), S_SIZE(s), S_MACHO_FILEOFF(s), S_SIZE(s), r->r_info.max_protection, r->r_info.protection);
    commit_load_command(wsd, (const void *)fc);
    if (opt->debug > 1) {
        hsize_str_t hstr;
        printr(r, "ref '%s' %s (vm %llx-%llx, file offset %lld for %s)\n", S_FILENAME(s), S_MACHO_TYPE(s), (uint64_t)fc->vmaddr, (uint64_t)fc->vmaddr + fc->vmsize, (int64_t)fc->fileoff, str_hsize(hstr, fc->filesize));
    }
    return WALK_CONTINUE;
}

#ifdef CONFIG_REFSC

/*
 * Note that we may be asked to write reference segments whose protections
 * are rw- -- this -should- be ok as we don't convert the region to a file
 * reference unless we know it hasn't been modified.
 */
static walk_return_t
write_fileref_region(const struct region *r, struct write_segment_data *wsd)
{
    assert(0 == r->r_nsubregions);
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT);
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    assert(!r->r_inzfodregion);

    const struct libent *le = r->r_fileref->fr_libent;
    const struct proto_fileref_command *fc = make_fileref_command(wsd->wsd_lc, le, R_ADDR(r), R_SIZE(r), r->r_fileref->fr_offset, (size_t)R_SIZE(r), r->r_info.max_protection, r->r_info.protection);
    commit_load_command(wsd, (const void *)fc);
    if (opt->debug > 1) {
        hsize_str_t hstr;
        printr(r, "ref '%s' %s (vm %llx-%llx, file offset %lld for %s)\n", le->le_filename, "(type?)", (uint64_t)fc->vmaddr, (uint64_t)fc->vmaddr + fc->vmsize, (int64_t)fc->fileoff, str_hsize(hstr, fc->filesize));
    }
    return WALK_CONTINUE;
}

const struct regionop fileref_ops = {
    print_memory_region,
    write_fileref_region,
    del_fileref_region,
};

#endif /* CONFIG_REFSC */

#pragma mark -- ZFOD segments written only to the header --

static void
size_zfod_region(const struct region *r, struct size_core *sc)
{
    assert(0 == r->r_nsubregions);
    assert(r->r_inzfodregion);
    sc->headersize += sizeof (native_segment_command_t);
    sc->count++;
    sc->memsize += R_SIZE(r);
}

static walk_return_t
write_zfod_region(const struct region *r, struct write_segment_data *wsd)
{
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT);
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);

    const void *sc = make_native_segment_command(wsd->wsd_lc, R_ADDR(r), R_SIZE(r), wsd->wsd_foffset, 0, r->r_info.max_protection, r->r_info.protection, 0);
    commit_load_command(wsd, sc);
    return WALK_CONTINUE;
}

const struct regionop zfod_ops = {
    print_memory_region,
    write_zfod_region,
    del_zfod_region,
};

#pragma mark -- Regions containing data --

static walk_return_t
pwrite_memory(struct write_segment_data *wsd, const void *addr, size_t size, mach_vm_offset_t memaddr, size_t memsize)
{
    assert(size);

    int error = 0;
    ssize_t nwritten = 0;

    if (opt->sizebound > 0 &&
        wsd->wsd_foffset + (off_t)size > opt->sizebound) {
        error = EFBIG;
    } else {
        nwritten = pwrite(wsd->wsd_fd, addr, size, wsd->wsd_foffset);
        if (nwritten < 0)
            error = errno;
    }

    if (error || opt->debug > 1) {
        hsize_str_t hsz;
        printf("%llx-%llx writing %ld bytes at offset %lld -> ",
               memaddr, memaddr+memsize, size, wsd->wsd_foffset);
        if (error)
            printf("err #%d - %s ", error, strerror(error));
        else {
            printf("%s ", str_hsize(hsz, nwritten));
            if (size != (size_t)nwritten)
                printf("[%zd - incomplete write!] ", nwritten);
            else if (size != memsize)
                printf("(%s in memory) ",
                       str_hsize(hsz, memsize));
        }
        printf("\n");
    }

    walk_return_t step = WALK_CONTINUE;
    switch (error) {
        case 0:
            if (size != (size_t)nwritten)
                step = WALK_ERROR;
            else {
                wsd->wsd_foffset += nwritten;
                wsd->wsd_nwritten += nwritten;
            }
            break;
        case EFAULT:	// transient mapping failure?
            break;
        default:        // EROFS, ENOSPC, EFBIG etc. */
            step = WALK_ERROR;
            break;
    }
    return step;
}


/*
 * Write a contiguous range of memory into the core file.
 * Apply compression, and chunk if necessary.
 */
static int
segment_compflags(compression_algorithm ca, unsigned *algnum)
{
    switch (ca) {
        case COMPRESSION_LZ4:
            *algnum = proto_SG_COMP_LZ4;
            break;
        case COMPRESSION_ZLIB:
            *algnum = proto_SG_COMP_ZLIB;
            break;
        case COMPRESSION_LZMA:
            *algnum = proto_SG_COMP_LZMA;
            break;
        case COMPRESSION_LZFSE:
            *algnum = proto_SG_COMP_LZFSE;
            break;
        default:
            err(EX_SOFTWARE, "unsupported compression algorithm %x", ca);
    }
    return 0;
}

static walk_return_t
write_memory_range(struct write_segment_data *wsd, const struct region *r, mach_vm_offset_t vmaddr, mach_vm_offset_t vmsize)
{
    assert(R_ADDR(r) <= vmaddr && R_ENDADDR(r) >= vmaddr + vmsize);

    mach_vm_offset_t resid = vmsize;
    walk_return_t step = WALK_CONTINUE;

    do {
        unsigned algorithm = 0;
        void *dstbuf = NULL;
        size_t filesize;

        vmsize = resid;

        /*
         * Since some regions can be inconveniently large,
         * chop them into multiple chunks as we compress them.
         * (mach_vm_read has 32-bit limitations too).
         */
        vmsize = vmsize > INT32_MAX ? INT32_MAX : vmsize;
        if (opt->chunksize > 0 && vmsize > opt->chunksize)
            vmsize = opt->chunksize;
        assert(vmsize <= INT32_MAX);

        mach_vm_offset_t data;
        mach_vm_offset_t data_count;

        kern_return_t kr;
        const void *srcaddr;

        if (r->r_incommregion) {
            /*
             * For commpage access, we just copy from our own address space.
             */
            data = 0;
            data_count = vmsize;
            kr = mach_vm_allocate(mach_task_self(), &data, data_count, VM_FLAGS_ANYWHERE);
            if (KERN_SUCCESS != kr || data == 0) {
                err_mach(kr, r, "subregion %llx-%llx, mach_vm_allocate()", vmaddr, vmaddr + vmsize);
                if (opt->debug) {
                    print_memory_region_header();
                    ROP_PRINT(r);
                }
                break;
            }
            if (opt->debug)
                printr(r, "subregion %llx-%llx, copying from self\n", vmaddr, vmaddr+vmsize);
            srcaddr = (const void *)memcpy((void *)data, (void *)vmaddr, vmsize);
        } else {
            /*
             * Most segments with data are mapped here
             */
            vm_offset_t data32 = 0;
            mach_msg_type_number_t data32_count;
            kr = mach_vm_read(wsd->wsd_task, vmaddr, vmsize, &data32, &data32_count);
            if (KERN_SUCCESS != kr || data32 == 0 || data32_count < vmsize) {
                err_mach(kr, r, "subregion %llx-%llx, mach_vm_read()", vmaddr, vmaddr + vmsize);
                if (opt->debug) {
                    print_memory_region_header();
                    ROP_PRINT(r);
                }
                break;
            }
            data = data32;
            data_count = data32_count;
            mach_vm_behavior_set(mach_task_self(), data, data_count, VM_BEHAVIOR_SEQUENTIAL);
            srcaddr = (const void *)data;
        }

        assert(vmsize);

        if (opt->compress) {
            dstbuf = malloc((size_t)vmsize);
            if (dstbuf) {

                filesize = compression_encode_buffer(dstbuf, (size_t)vmsize, srcaddr, (size_t)vmsize, NULL, opt->calgorithm);

                if (filesize > 0 && filesize < vmsize) {
                    srcaddr = dstbuf;
                    if (segment_compflags(opt->calgorithm, &algorithm) != 0) {
                        free(dstbuf);
                        mach_vm_deallocate(mach_task_self(), data, data_count);
                        return WALK_ERROR;
                    }
                } else {
                    free(dstbuf);
                    dstbuf = NULL;
                    filesize = (size_t)vmsize;
                }
            } else
                filesize = (size_t)vmsize;
        } else
            filesize = (size_t)vmsize;

        assert(filesize);

        native_segment_command_t *sc = make_native_segment_command(wsd->wsd_lc, vmaddr, vmsize, wsd->wsd_foffset, filesize, r->r_info.max_protection, r->r_info.protection, algorithm);

        assert((sc->flags == 0) ^ (sc->filesize < sc->vmsize));

        step = pwrite_memory(wsd, srcaddr, sc->filesize, vmaddr, sc->vmsize);
        if (dstbuf)
            free(dstbuf);
        mach_vm_deallocate(mach_task_self(), data, data_count);

        if (WALK_ERROR == step)
            break;
        commit_load_command(wsd, (const void *)sc);
        resid -= vmsize;
        vmaddr += vmsize;
    } while (resid);

    return step;
}

#ifdef RDAR_23744374
/*
 * Sigh. This is a workaround.
 * Find the vmsize as if the VM system manages ranges in host pagesize units
 * rather than application pagesize units.
 */
static mach_vm_size_t
getvmsize_host(const task_t task, const struct region *r)
{
    mach_vm_size_t vmsize_host = R_SIZE(r);

    if (pageshift_host != pageshift_app) {
        is_actual_size(task, r, &vmsize_host);
        if (opt->debug && R_SIZE(r) != vmsize_host)
            printr(r, "(region size tweak: was %llx, is %llx)\n", R_SIZE(r), vmsize_host);
    }
    return vmsize_host;
}
#else
static __inline mach_vm_size_t
getvmsize_host(__unused const task_t task, const struct region *r)
{
    return R_SIZE(r);
}
#endif

static walk_return_t
write_sparse_region(const struct region *r, struct write_segment_data *wsd)
{
    assert(r->r_nsubregions);
    assert(!r->r_inzfodregion);
#ifdef CONFIG_REFSC
    assert(NULL == r->r_fileref);
#endif

    const mach_vm_size_t vmsize_host = getvmsize_host(wsd->wsd_task, r);
    walk_return_t step = WALK_CONTINUE;

    for (unsigned i = 0; i < r->r_nsubregions; i++) {
        const struct subregion *s = r->r_subregions[i];

        if (s->s_isfileref)
            step = write_fileref_subregion(r, s, wsd);
        else {
            /* Write this one out as real data */
            mach_vm_size_t vmsize = S_SIZE(s);
            if (R_SIZE(r) != vmsize_host) {
                if (S_ADDR(s) + vmsize > R_ADDR(r) + vmsize_host) {
                    vmsize = R_ADDR(r) + vmsize_host - S_ADDR(s);
                    if (opt->debug)
                        printr(r, "(subregion size tweak: was %llx, is %llx)\n",
                               S_SIZE(s), vmsize);
                }
            }
            step = write_memory_range(wsd, r, S_ADDR(s), vmsize);
        }
        if (WALK_ERROR == step)
            break;
    }
    return step;
}

static walk_return_t
write_vanilla_region(const struct region *r, struct write_segment_data *wsd)
{
    assert(0 == r->r_nsubregions);
    assert(!r->r_inzfodregion);
#ifdef CONFIG_REFSC
    assert(NULL == r->r_fileref);
#endif

    const mach_vm_size_t vmsize_host = getvmsize_host(wsd->wsd_task, r);
    return write_memory_range(wsd, r, R_ADDR(r), vmsize_host);
}

walk_return_t
region_write_memory(struct region *r, void *arg)
{
    assert(r->r_info.user_tag != VM_MEMORY_IOKIT); // elided in walk_regions()
    assert((r->r_info.max_protection & VM_PROT_READ) == VM_PROT_READ);
    return ROP_WRITE(r, arg);
}

/*
 * Handles the cases where segments are broken into chunks i.e. when
 * writing compressed segments.
 */
static unsigned long
count_memory_range(mach_vm_offset_t vmsize)
{
    unsigned long count;
    if (opt->compress && opt->chunksize > 0) {
        count = (size_t)vmsize / opt->chunksize;
        if (vmsize != (mach_vm_offset_t)count * opt->chunksize)
            count++;
    } else
        count = 1;
    return count;
}

/*
 * A sparse region is likely a writable data segment described by
 * native_segment_command_t somewhere in the address space.
 */
static void
size_sparse_subregion(const struct subregion *s, struct size_core *sc)
{
    const unsigned long count = count_memory_range(S_SIZE(s));
    sc->headersize += sizeof (native_segment_command_t) * count;
    sc->count += count;
    sc->memsize += S_SIZE(s);
}

static void
size_sparse_region(const struct region *r, struct size_core *sc_sparse, struct size_core *sc_fileref)
{
    assert(0 != r->r_nsubregions);

    unsigned long entry_total = sc_sparse->count + sc_fileref->count;
    for (unsigned i = 0; i < r->r_nsubregions; i++) {
        const struct subregion *s = r->r_subregions[i];
        if (s->s_isfileref)
            size_fileref_subregion(s, sc_fileref);
        else
            size_sparse_subregion(s, sc_sparse);
    }
    if (opt->debug) {
        /* caused by compression breaking a large region into chunks */
        entry_total = (sc_fileref->count + sc_sparse->count) - entry_total;
        if (entry_total > r->r_nsubregions)
            printr(r, "range contains %u subregions requires %lu segment commands\n",
               r->r_nsubregions, entry_total);
    }
}

const struct regionop sparse_ops = {
    print_memory_region,
    write_sparse_region,
    del_sparse_region,
};

static void
size_vanilla_region(const struct region *r, struct size_core *sc)
{
    assert(0 == r->r_nsubregions);

    const unsigned long count = count_memory_range(R_SIZE(r));
    sc->headersize += sizeof (native_segment_command_t) * count;
    sc->count += count;
    sc->memsize += R_SIZE(r);

    if (opt->debug && count > 1)
        printr(r, "range with 1 region, but requires %lu segment commands\n", count);
}

const struct regionop vanilla_ops = {
    print_memory_region,
    write_vanilla_region,
    del_vanilla_region,
};

walk_return_t
region_size_memory(struct region *r, void *arg)
{
    struct size_segment_data *ssd = arg;

    if (&zfod_ops == r->r_op)
        size_zfod_region(r, &ssd->ssd_zfod);
#ifdef CONFIG_REFSC
    else if (&fileref_ops == r->r_op)
        size_fileref_region(r, &ssd->ssd_fileref);
#endif
    else if (&sparse_ops == r->r_op)
        size_sparse_region(r, &ssd->ssd_sparse, &ssd->ssd_fileref);
    else if (&vanilla_ops == r->r_op)
        size_vanilla_region(r, &ssd->ssd_vanilla);
    else
        errx(EX_SOFTWARE, "%s: bad op", __func__);

    return WALK_CONTINUE;
}
