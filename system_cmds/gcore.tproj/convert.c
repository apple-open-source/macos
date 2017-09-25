/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "convert.h"
#include "corefile.h"
#include "vanilla.h"
#include "threads.h"
#include "vm.h"
#include "dyld_shared_cache.h"
#include "utils.h"

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/queue.h>
#include <sys/param.h>
#include <mach-o/fat.h>
#include <uuid/uuid.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <err.h>
#include <sysexits.h>
#include <time.h>

#if defined(CONFIG_GCORE_MAP) || defined(CONFIG_GCORE_CONV) || defined(CONFIG_GCORE_FREF)

static const void *
mmapfile(int fd, off_t off, off_t *filesize)
{
	struct stat st;
	if (-1 == fstat(fd, &st))
		errc(EX_OSERR, errno, "can't stat input file");

	const size_t size = (size_t)(st.st_size - off);
	if ((off_t)size != (st.st_size - off))
		errc(EX_OSERR, EOVERFLOW, "input file too large?");

	const void *addr = mmap(0, size, PROT_READ, MAP_PRIVATE, fd, off);
	if ((void *)-1 == addr)
		errc(EX_OSERR, errno, "can't mmap input file");
	*filesize = st.st_size;
	return addr;
}

static void
walkcore(
	const native_mach_header_t *mh,
	void (^coreinfo)(const struct proto_coreinfo_command *),
	void (^frefdata)(const struct proto_fileref_command *),
	void (^coredata)(const struct proto_coredata_command *),
	void (^segdata)(const native_segment_command_t *),
	void (^thrdata)(const struct thread_command *))
{
	const struct load_command *lc = (const void *)(mh + 1);
	for (unsigned i = 0; i < mh->ncmds; i++) {
		switch (lc->cmd) {
			case proto_LC_COREINFO:
				if (coreinfo)
					coreinfo((const void *)lc);
				break;
			case proto_LC_FILEREF:
				if (frefdata)
					frefdata((const void *)lc);
				break;
			case proto_LC_COREDATA:
				if (coredata)
					coredata((const void *)lc);
				break;
			case NATIVE_LC_SEGMENT:
				if (segdata)
					segdata((const void *)lc);
				break;
			case LC_THREAD:
				if (thrdata)
					thrdata((const void *)lc);
				break;
			default:
				break;
		}
		if (NULL == (lc = next_lc(lc)))
			break;
	}
}

#endif

#ifdef CONFIG_GCORE_FREF

int
gcore_fref(int fd)
{
	off_t filesize;
	const void *corebase = mmapfile(fd, 0, &filesize);

	close(fd);
	struct flist {
		STAILQ_ENTRY(flist) f_linkage;
		const char *f_nm;
		unsigned long f_nmhash;
	};
	STAILQ_HEAD(flisthead, flist) __flh, *flh = &__flh;
	STAILQ_INIT(flh);

	walkcore(corebase, NULL, ^(const struct proto_fileref_command *fc) {
		const char *nm = fc->filename.offset + (const char *)fc;
		const unsigned long nmhash = simple_namehash(nm);
		struct flist *f;
		STAILQ_FOREACH(f, flh, f_linkage) {
			if (nmhash == f->f_nmhash && 0 == strcmp(f->f_nm, nm))
				return;	/* skip duplicates */
		}
		struct flist *nf = calloc(1, sizeof (*nf));
		nf->f_nm = nm;
		nf->f_nmhash = nmhash;
		STAILQ_INSERT_TAIL(flh, nf, f_linkage);
	}, NULL, NULL, NULL);

	struct flist *f, *tf;
	STAILQ_FOREACH_SAFE(f, flh, f_linkage, tf) {
		printf("%s\n", f->f_nm);
		free(f);
		f = NULL;
	}

	munmap((void *)corebase, (size_t)filesize);
	return 0;
}

#endif /* CONFIG_GCORE_FREF */

#ifdef CONFIG_GCORE_MAP

/*
 * A pale imitation of vmmap, but for core files
 */
int
gcore_map(int fd)
{
	off_t filesize;
	const void *corebase = mmapfile(fd, 0, &filesize);

	__block int coreversion = 0;

	walkcore(corebase, ^(const struct proto_coreinfo_command *ci) {
			coreversion = ci->version;
		}, NULL, NULL, NULL, NULL);

	if (0 == coreversion) {
		const char titlfmt[] = "%16s-%-16s [%7s] %3s/%3s\n";
		const char *segcfmt = "%016llx-%016llx [%7s] %3s/%3s\n";

		printf(titlfmt, "start ", " end", "vsize", "prt", "max");
		walkcore(corebase, NULL, NULL, NULL, ^(const native_segment_command_t *sc) {
			hsize_str_t vstr;
			printf(segcfmt, (mach_vm_offset_t)sc->vmaddr, (mach_vm_offset_t)sc->vmaddr + sc->vmsize, str_hsize(vstr, sc->vmsize), str_prot(sc->initprot), str_prot(sc->maxprot));
		}, NULL);
	} else {
		const char titlfmt[] = "%-23s %16s-%-16s [%7s] %3s/%3s %6s %4s  %-14s\n";
		const char *freffmt = "%-23s %016llx-%016llx [%7s] %3s/%3s %6s %4s  %-14s @%lld\n";
		const char *datafmt = "%-23s %016llx-%016llx [%7s] %3s/%3s %6s %4s  %-14s\n";

		printf(titlfmt, "region type", "start ", " end", "vsize", "prt", "max", "shrmod", "purge", "region detail");
		walkcore(corebase, NULL, ^(const struct proto_fileref_command *fc) {
			const char *nm = fc->filename.offset + (const char *)fc;
            tag_str_t tstr;
			hsize_str_t vstr;
			printf(freffmt, str_tag(tstr, fc->tag, fc->share_mode, fc->prot, fc->extp),
				   fc->vmaddr, fc->vmaddr + fc->vmsize,
				   str_hsize(vstr, fc->vmsize), str_prot(fc->prot),
				   str_prot(fc->maxprot), str_shared(fc->share_mode),
				   str_purgable(fc->purgable, fc->share_mode), nm, fc->fileoff);
		}, ^(const struct proto_coredata_command *cc) {
            tag_str_t tstr;
			hsize_str_t vstr;
			printf(datafmt, str_tag(tstr, cc->tag, cc->share_mode, cc->prot, cc->extp),
				   cc->vmaddr, cc->vmaddr + cc->vmsize,
				   str_hsize(vstr, cc->vmsize), str_prot(cc->prot),
				   str_prot(cc->maxprot), str_shared(cc->share_mode),
				   str_purgable(cc->purgable, cc->share_mode),
				   cc->vmsize && 0 == cc->filesize ? "(zfod)" : "");
		}, ^(const native_segment_command_t *sc) {
			hsize_str_t vstr;
			printf(datafmt, "", (mach_vm_offset_t)sc->vmaddr,
				   (mach_vm_offset_t)sc->vmaddr + sc->vmsize,
				   str_hsize(vstr, sc->vmsize), str_prot(sc->initprot),
				   str_prot(sc->maxprot), "", "",
				   sc->vmsize && 0 == sc->filesize ? "(zfod)" : "");
		}, NULL);
	}
	close(fd);
	munmap((void *)corebase, (size_t)filesize);
	return 0;
}

#endif

#ifdef CONFIG_GCORE_CONV

/*
 * Convert an input core file into an "old" format core file
 * (a) convert all fileref segments into regular segments
 * (b) uncompress anything we find compressed.
 * This should be equivalent to a copy for an "old" format core file.
 */

static int
machocmp(const native_mach_header_t *tmh, const native_mach_header_t *mh, const struct proto_fileref_command *fr)
{
	if (tmh->magic == mh->magic) {
		const struct load_command *lc = (const void *)(tmh + 1);
		for (unsigned i = 0; i < tmh->ncmds; i++) {
			if (LC_UUID == lc->cmd && lc->cmdsize >= sizeof (struct uuid_command)) {
				const struct uuid_command *uc = (const void *)lc;
				return uuid_compare(uc->uuid, fr->id);
			}
			if (NULL == (lc = next_lc(lc)))
				break;
		}
	}
	return -1;
}

static int
fat_machocmp(const struct fat_header *fh, const native_mach_header_t *mh, const struct proto_fileref_command *fr, off_t *reloff)
{
	const uint32_t (^get32)(uint32_t);

	if (FAT_MAGIC == fh->magic) {
		get32 = ^(uint32_t val) {
			return val;
		};
	} else {
		get32 = ^(uint32_t val) {
			uint32_t result = 0;
			for (unsigned i = 0; i < sizeof (uint32_t); i++)
				((uint8_t *)&result)[i] = ((uint8_t *)&val)[3-i];
			return result;
		};
	}

	assert(FAT_MAGIC == get32(fh->magic));
	assert(kFREF_ID_UUID == FREF_ID_TYPE(fr->flags) && !uuid_is_null(fr->id));

	const struct fat_arch *fa = (const struct fat_arch *)(fh + 1);
	uint32_t narch = get32(fh->nfat_arch);
	for (unsigned n = 0; n < narch; n++, fa++) {
		const native_mach_header_t *tmh = (const void *)(((const char *)fh) + get32(fa->offset));
		if (tmh->magic == mh->magic && 0 == machocmp(tmh, mh, fr)) {
			*reloff = get32(fa->offset);
			return 0;
		}
	}
	return -1;
}

struct output_info {
	int oi_fd;
	off_t oi_foffset;
	bool oi_nocache;
};

static struct convstats {
	int64_t copied;
	int64_t added;
	int64_t compressed;
	int64_t uncompressed;
} cstat, *cstats = &cstat;

/*
 * A fileref segment references a read-only file that contains pages from
 * the image.  The file may be a Mach binary or dylib identified with a uuid.
 */
static int
convert_fileref_with_file(const char *filename, const native_mach_header_t *inmh, const struct proto_fileref_command *infr, const struct vm_range *invr, struct load_command *lc, struct output_info *oi)
{
	assert(invr->addr == infr->vmaddr && invr->size == infr->vmsize);

	struct stat st;
	const int rfd = open(filename, O_RDONLY);
	if (-1 == rfd || -1 == fstat(rfd, &st)) {
		warnc(errno, "%s: open", filename);
		return EX_IOERR;
	}
	const size_t rlen = (size_t)st.st_size;
	void *raddr = mmap(NULL, rlen, PROT_READ, MAP_PRIVATE, rfd, 0);
	if ((void *)-1 == raddr) {
		warnc(errno, "%s: mmap", filename);
		close(rfd);
		return EX_IOERR;
	}
	close(rfd);

	off_t fatoff = 0;	/* for FAT objects */
	int ecode = EX_DATAERR;

	switch (FREF_ID_TYPE(infr->flags)) {
		case kFREF_ID_UUID: {
			/* file should be a mach binary: check that uuid matches */
			const uint32_t magic = *(uint32_t *)raddr;
			switch (magic) {
				case FAT_MAGIC:
				case FAT_CIGAM:
					if (0 == fat_machocmp(raddr, inmh, infr, &fatoff))
						ecode = 0;
					break;
				case NATIVE_MH_MAGIC:
					if (0 == machocmp(raddr, inmh, infr))
						ecode = 0;
					break;
				default: {
					/*
					 * Maybe this is the shared cache?
					 */
					uuid_t uu;
					if (get_uuid_from_shared_cache_mapping(raddr, rlen, uu) && uuid_compare(uu, infr->id) == 0)
						ecode = 0;
					break;
				}
			}
			break;
		}
		case kFREF_ID_MTIMESPEC_LE:
			/* file should have the same mtime */
			if (0 == memcmp(&st.st_mtimespec, infr->id, sizeof (infr->id)))
				ecode = 0;
			break;
		case kFREF_ID_NONE:
			/* file has no uniquifier, copy it anyway */
			break;
	}

	if (0 != ecode) {
		munmap(raddr, rlen);
		warnx("%s doesn't match corefile content", filename);
		return ecode;
	}

	const off_t fileoff = fatoff + infr->fileoff;
	const void *start = (const char *)raddr + fileoff;
	const size_t len = (size_t)infr->filesize;
	void *zaddr = NULL;
	size_t zlen = 0;

	if (fileoff + (off_t)infr->filesize > (off_t)rlen) {
		/*
		 * the file content needed (as described on machine with
		 * larger pagesize) extends beyond the end of the mapped
		 * file using our smaller pagesize.  Zero pad it.
		 */
		const size_t pagesize_host = 1ul << pageshift_host;
		void *endaddr = (caddr_t)raddr + roundup(rlen, pagesize_host);
		zlen = (size_t)(fileoff + infr->filesize - rlen);
		zaddr = mmap(endaddr, zlen, PROT_READ, MAP_FIXED | MAP_PRIVATE | MAP_ANON, -1, 0);
		if ((void *)-1 == zaddr) {
			hsize_str_t hstr;
			warnc(errno, "cannot zero-pad %s mapping for %s", str_hsize(hstr, zlen),filename);
			munmap(raddr, rlen);
			return EX_IOERR;
		}
	}

	if (-1 == madvise((void *)start, len, MADV_SEQUENTIAL))
		warnc(errno, "%s: madvise", filename);

	const int error = bounded_pwrite(oi->oi_fd, start, len, oi->oi_foffset, &oi->oi_nocache, NULL);

	if (zlen) {
		if (-1 == munmap(zaddr, zlen))
			warnc(errno, "%s: munmap zero pad", filename);
	}
	if (-1 == munmap(raddr, rlen))
		warnc(errno, "%s: munmap", filename);
	if (error) {
		warnc(error, "while copying %s to core file", filename);
		return EX_IOERR;
	}

	const struct file_range fr = {
		.off = oi->oi_foffset,
		.size = infr->filesize,
	};
	make_native_segment_command(lc, invr, &fr, infr->maxprot, infr->prot);
	oi->oi_foffset += fr.size;
	cstats->added += infr->filesize;
	return 0;
}

/*
 * bind the file reference into the output core file.
 * filename optionally prefixed with names from a ':'-separated PATH variable
 */
static int
convert_fileref(const char *path, bool zf, const native_mach_header_t *inmh, const struct proto_fileref_command *infr, struct load_command *lc, struct output_info *oi)
{
	const char *nm = infr->filename.offset + (const char *)infr;
	uuid_string_t uustr;
	const struct vm_range invr = {
		.addr = infr->vmaddr,
		.size = infr->vmsize,
	};

	if (opt->verbose) {
		hsize_str_t hstr;
		printvr(&invr, "adding %s from '%s'",
			   str_hsize(hstr, (off_t)infr->filesize), nm);
		switch (FREF_ID_TYPE(infr->flags)) {
			case kFREF_ID_NONE:
				break;
			case kFREF_ID_UUID:
				uuid_unparse_lower(infr->id, uustr);
				printf(" (%s)", uustr);
				break;
			case kFREF_ID_MTIMESPEC_LE: {
				struct timespec mts;
				struct tm tm;
				char tbuf[4 + 2 + 2 + 2 + 2 + 1 + 2 + 1];	/* touch -t */
				memcpy(&mts, &infr->id, sizeof (mts));
				localtime_r(&mts.tv_sec, &tm);
				strftime(tbuf, sizeof (tbuf), "%Y%m%d%H%M.%S", &tm);
				printf(" (%s)", tbuf);
			}	break;
		}
		printf("\n");
	}

	const size_t pathsize = path ? strlen(path) : 0;
	int ecode = EX_DATAERR;
	if (0 == pathsize)
		ecode = convert_fileref_with_file(nm, inmh, infr, &invr, lc, oi);
	else {
		/* search the : separated path (-L) for possible matches */
		char *pathcopy = strdup(path);
		char *searchpath = pathcopy;
		const char *token;

		while ((token = strsep(&searchpath, ":")) != NULL) {
			const size_t buflen = strlen(token) + 1 + strlen(nm) + 1;
			char *buf = malloc(buflen);
			snprintf(buf, buflen, "%s%s%s", token, '/' == nm[0] ? "" : "/", nm);
			if (opt->verbose)
				printf("\tTrying '%s'", buf);
			if (0 == access(buf, R_OK)) {
				if (opt->verbose)
					printf("\n");
				ecode = convert_fileref_with_file(buf, inmh, infr, &invr, lc, oi);
				if (0 == ecode) {
					free(buf);
					break;
				}
			} else if (opt->verbose)
				printf(": %s.\n",
					0 == access(buf, F_OK) ? "Unreadable" : "Not present");
			free(buf);
		}
		free(pathcopy);
	}

	if (0 != ecode && zf) {
		/*
		 * Failed to find the file reference.  If this was a fileref that uses
		 * a file metadata tagging method (e.g. mtime), allow the user to subsitute a
		 * zfod region: assumes that it's better to have something to debug
		 * vs. nothing.  UUID-tagged filerefs are Mach-O tags, and are
		 * assumed to be never substitutable.
		 */
		switch (FREF_ID_TYPE(infr->flags)) {
			case kFREF_ID_NONE:
			case kFREF_ID_MTIMESPEC_LE: {	// weak tagging, allow zfod substitution
				const struct file_range outfr = {
					.off = oi->oi_foffset,
					.size = 0,
				};
				if (opt->verbose)
					printf("\tWARNING: no file matched. Missing content is now zfod\n");
				else
					printvr(&invr, "WARNING: missing content (%s) now zfod\n", nm);
				make_native_segment_command(lc, &invr, &outfr, infr->maxprot, infr->prot);
				ecode = 0;
			}	break;
			default:
				break;
		}
	}

	return (ecode);
}

static int
segment_uncompflags(unsigned algnum, compression_algorithm *ca)
{
	switch (algnum) {
		case kCOMP_LZ4:
			*ca = COMPRESSION_LZ4;
			break;
		case kCOMP_ZLIB:
			*ca = COMPRESSION_ZLIB;
			break;
		case kCOMP_LZMA:
			*ca = COMPRESSION_LZMA;
			break;
		case kCOMP_LZFSE:
			*ca = COMPRESSION_LZFSE;
			break;
		default:
			warnx("unknown compression flavor %d", algnum);
			return EX_DATAERR;
	}
	return 0;
}

static int
convert_region(const void *inbase, const struct vm_range *invr, const struct file_range *infr, const vm_prot_t prot, const vm_prot_t maxprot, const int flavor, struct load_command *lc, struct output_info *oi)
{
	int ecode = 0;

	if (F_SIZE(infr)) {
		void *input = (const caddr_t)inbase + F_OFF(infr);
		void *buf;

		if (0 == flavor) {
			buf = input;
			if (opt->verbose) {
				hsize_str_t hstr;
				printvr(invr, "copying %s\n", str_hsize(hstr, F_SIZE(infr)));
			}
		} else {
			compression_algorithm ca;

			if (0 != (ecode = segment_uncompflags(flavor, &ca)))
				return ecode;
			if (opt->verbose) {
				hsize_str_t hstr1, hstr2;
				printvr(invr, "uncompressing %s to %s\n",
					str_hsize(hstr1, F_SIZE(infr)), str_hsize(hstr2, V_SIZE(invr)));
			}
			const size_t buflen = V_SIZEOF(invr);
			buf = malloc(buflen);
			const size_t dstsize = compression_decode_buffer(buf, buflen, input, (size_t)F_SIZE(infr), NULL, ca);
			if (buflen != dstsize) {
				warnx("failed to uncompress segment");
				free(buf);
				return EX_DATAERR;
			}
			cstats->compressed += F_SIZE(infr);
		}
		const int error = bounded_pwrite(oi->oi_fd, buf, V_SIZEOF(invr), oi->oi_foffset, &oi->oi_nocache, NULL);
		if (error) {
			warnc(error, "failed to write data to core file");
			ecode = EX_IOERR;
		}
		if (buf != input)
			free(buf);
		if (ecode)
			return ecode;

		const struct file_range outfr = {
			.off = oi->oi_foffset,
			.size = V_SIZE(invr),
		};
		make_native_segment_command(lc, invr, &outfr, maxprot, prot);
		oi->oi_foffset += outfr.size;

		if (0 == flavor)
			cstats->copied += outfr.size;
		else
			cstats->uncompressed += outfr.size;
	} else {
		if (opt->verbose) {
			hsize_str_t hstr;
			printvr(invr, "%s remains zfod\n", str_hsize(hstr, V_SIZE(invr)));
		}
		const struct file_range outfr = {
			.off = oi->oi_foffset,
			.size = 0,
		};
		make_native_segment_command(lc, invr, &outfr, maxprot, prot);
	}
	return ecode;
}

static int
convert_coredata(const void *inbase, const native_mach_header_t *__unused inmh, const struct proto_coredata_command *cc, struct load_command *lc, struct output_info *oi)
{
	const struct vm_range vr = {
		.addr = cc->vmaddr,
		.size = cc->vmsize,
	};
	const struct file_range fr = {
		.off = cc->fileoff,
		.size = cc->filesize,
	};
	return convert_region(inbase, &vr, &fr, cc->prot, cc->maxprot, COMP_ALG_TYPE(cc->flags), lc, oi);
}

static int
convert_segment(const void *inbase, const native_mach_header_t *__unused inmh, const native_segment_command_t *sc, struct load_command *lc, struct output_info *oi)
{
	const struct vm_range vr = {
		.addr = sc->vmaddr,
		.size = sc->vmsize,
	};
	const struct file_range fr = {
		.off = sc->fileoff,
		.size = sc->filesize,
	};
	return convert_region(inbase, &vr, &fr, sc->initprot, sc->maxprot, 0, lc, oi);
}

/* pass-through - content is all in the header */

static int
convert_thread(struct thread_command *dst, const struct thread_command *src)
{
	assert(LC_THREAD == src->cmd);
	memcpy(dst, src, src->cmdsize);
	cstats->copied += src->cmdsize;
	return 0;
}

int
gcore_conv(int infd, const char *searchpath, bool zf, int fd)
{
	off_t filesize;
	const void *corebase = mmapfile(infd, 0, &filesize);
	close(infd);
	/*
	 * Check to see if the input file is "sane" as far as we're concerned.
	 * XXX	Note that this -won't- necessarily work for other ISAs than
	 *		our own!
	 */
	const native_mach_header_t *inmh = corebase;
	validate_core_header(inmh, filesize);

	/*
	 * The sparse file may have created many more segments, but there's no
	 * attempt to change their numbers here.  Just count all the segment
	 * types needed to figure out the size of the output file header.
	 *
	 * (Size assertions to be deleted once data structures stable!)
	 */
	__block size_t headersize = sizeof (native_mach_header_t);
	__block unsigned pageshift_target = pageshift_host;

	walkcore(inmh, ^(const struct proto_coreinfo_command *ci) {
		assert(sizeof (*ci) == ci->cmdsize);
		if (opt->verbose)
			printf("Converting version %d core file to pre-versioned format\n", ci->version);
		if (0 < ci->pageshift && ci->pageshift < 31)
			pageshift_target = ci->pageshift;
		else if (CPU_TYPE_ARM64 == inmh->cputype)
			pageshift_target = 14;	// compatibility hack, should go soon
	}, ^(const struct proto_fileref_command *__unused fc) {
		const char *nm = fc->filename.offset + (const char *)fc;
		size_t nmlen = strlen(nm) + 1;
		size_t cmdsize = sizeof (*fc) + roundup(nmlen, sizeof (long));
		assert(cmdsize == fc->cmdsize);

		headersize += sizeof (native_segment_command_t);
	}, ^(const struct proto_coredata_command *__unused cc) {
		assert(sizeof (*cc) == cc->cmdsize);
		headersize += sizeof (native_segment_command_t);
	}, ^(const native_segment_command_t *sc) {
		headersize += sc->cmdsize;
	}, ^(const struct thread_command *tc) {
		headersize += tc->cmdsize;
	});

	void *header = calloc(1, headersize);
	if (NULL == header)
		errx(EX_OSERR, "out of memory for header");

	native_mach_header_t *mh = memcpy(header, inmh, sizeof (*mh));
	mh->ncmds = 0;
	mh->sizeofcmds = 0;

	assert(0 < pageshift_target && pageshift_target < 31);
	const vm_offset_t pagesize_target = ((vm_offset_t)1 << pageshift_target);
	const vm_offset_t pagemask_target = pagesize_target - 1;

	const struct load_command *inlc = (const void *)(inmh + 1);
	struct load_command *lc = (void *)(mh + 1);
	int ecode = 0;

	struct output_info oi = {
		.oi_fd = fd,
		.oi_foffset = ((vm_offset_t)headersize + pagemask_target) & ~pagemask_target,
		.oi_nocache = false,
	};

	for (unsigned i = 0; i < inmh->ncmds; i++) {
		switch (inlc->cmd) {
			case proto_LC_FILEREF:
				ecode = convert_fileref(searchpath, zf, inmh, (const void *)inlc, lc, &oi);
				break;
			case proto_LC_COREDATA:
				ecode = convert_coredata(corebase, inmh, (const void *)inlc, lc, &oi);
				break;
			case NATIVE_LC_SEGMENT:
				ecode = convert_segment(corebase, inmh, (const void *)inlc, lc, &oi);
				break;
			case LC_THREAD:
				ecode = convert_thread((void *)lc, (const void *)inlc);
				break;
			default:
				if (OPTIONS_DEBUG(opt, 1))
					printf("discarding load command %d\n", inlc->cmd);
				break;
		}
		if (0 != ecode)
			break;
		if (NATIVE_LC_SEGMENT == lc->cmd || LC_THREAD == lc->cmd) {
			mach_header_inc_ncmds(mh, 1);
			mach_header_inc_sizeofcmds(mh, lc->cmdsize);
			lc = (void *)next_lc(lc);
		}
		if (NULL == (inlc = next_lc(inlc)))
			break;
	}

	/*
	 * Even if we've encountered an error, try and write out the header
	 */
	if (0 != bounded_pwrite(fd, header, headersize, 0, &oi.oi_nocache, NULL))
		ecode = EX_IOERR;
	if (0 == ecode && sizeof (*mh) + mh->sizeofcmds != headersize)
		ecode = EX_SOFTWARE;
	validate_core_header(mh, oi.oi_foffset);
	if (ecode)
		warnx("failed to write new core file correctly");
	else if (opt->verbose) {
		hsize_str_t hstr;
		printf("Conversion complete: %s copied", str_hsize(hstr, cstats->copied));
		const int64_t delta = cstats->uncompressed - cstats->compressed;
		if (delta > 0)
			printf(", %s uncompressed", str_hsize(hstr, delta));
		const int64_t added = cstats->added + ((int)mh->sizeofcmds - (int)inmh->sizeofcmds);
		if (added > 0)
			printf(", %s added", str_hsize(hstr, added));
		printf("\n");
	}
	free(header);
	munmap((void *)corebase, (size_t)filesize);
	return ecode;
}
#endif
