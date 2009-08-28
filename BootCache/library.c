/*
 * Common functions related to playlist handling and the management of
 * the BootCache module.
 */


/* XXX trim includes */
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "BootCache.h"

/*
 * The blocksize is initialised from the first playlist read, the statistics
 * structure, or it can be pre-set by the caller.  Once set, only playlists with
 * matching sizes can be read/written.
 */
int	BC_blocksize = 0;

#define	BLOCK_ROUNDDOWN(x)	(((x) / BC_blocksize) * BC_blocksize)
#define BLOCK_ROUNDUP(x)	((((x) + (BC_blocksize - 1)) / BC_blocksize) * BC_blocksize)
/*
 * optional for power-of-two blocksize roundup:
 * (((x) + (BC_blocksize - 1)) & (~(BC_blocksize - 1)))
 */


/*
 * Read the named playlist from disk into an allocated buffer.
 */
int
BC_read_playlist(const char *pfname, struct BC_playlist_entry **ppc, int *pnentries)
{
	struct BC_playlist_entry *pc;
	struct BC_playlist_header ph;
	int error, fd;

	error = 0;
	fd = -1;
	pc = NULL;

	if (pfname == NULL)
		return(ENOENT);
	
	if ((fd = open(pfname, O_RDONLY)) == -1) {
		error = errno;
		goto out;
	}
	if (read(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		warnx("could not read header from %s", pfname);
		error = EINVAL;
		goto out;
	}
	if (ph.ph_magic != PH_MAGIC) {
		warnx("bad playlist magic");
		error = EINVAL;
		goto out;
	}
	if (BC_blocksize == 0) {
		BC_blocksize = ph.ph_blocksize;
	} else if (BC_blocksize != ph.ph_blocksize) {
		warnx("bad playlist blocksize %d (should be %d)",
		    ph.ph_blocksize, BC_blocksize);
		error = EINVAL;
	}
	if ((pc = malloc(sizeof(*pc) * ph.ph_entries)) == NULL) {
		warnx("could not allocate memory for playlist");
		error = errno;
		goto out;
	}

	if (read(fd, pc, sizeof(*pc) * ph.ph_entries) != sizeof(*pc) * ph.ph_entries) {
		warnx("could not read playlist data");
		error = EINVAL;
		goto out;
	}
	*ppc = pc;
	*pnentries = ph.ph_entries;

out:
	if (fd != -1)
		close(fd);
	if (error && (pc != NULL))
		free(pc);

	return(error);
}

/*
 * Write the playlist to the named file, securely.
 */
int
BC_write_playlist(const char *pfname, const struct BC_playlist_entry *pc, int nentries)
{
	struct BC_playlist_header ph;
	char *tfname;
	int error, fd;

	error = 0;
	tfname = NULL;
	fd = -1;

	if (BC_blocksize == 0) {
		warnx("can't write playlist without setting block size");
		error = EINVAL;
		goto out;
	}
	
	/*
	 * Prepare the output file.
	 *
	 * Create a secure temporary file and write an invalid header.
	 */
	if (strlen(pfname) > (MAXPATHLEN - 8)) {
		warnx("playlist filename too long");
		error = ENAMETOOLONG;
		goto out;
	}
	if ((tfname = malloc(strlen(pfname) + 8)) == NULL) {
		warn("could not allocate %d bytes for playlist filename", strlen(pfname));
		error = errno;
		goto out;
	}
	sprintf(tfname, "%s.XXXXXX", pfname);
	if ((fd = mkstemp(tfname)) < 0) {
		warn("could not create temporary playlist file");
		error = errno;
		goto out;
	}
	ph.ph_magic = 0;
	ph.ph_entries = 0;
	ph.ph_blocksize = 0;
	if (write(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		warn("could not write initial header to temporary playlist file");
		error = errno;
		goto out;
	}

	/*
	 * Write the playlist entries.
	 */
	if (nentries && write(fd, pc, sizeof(*pc) * nentries) != (sizeof(*pc) * nentries)) {
		warn("could not write entry to temporary playlist file");
		error = errno;
		goto out;
	}

	/*
	 * Write an updated (valid) header to the playlist file.
	 */
	ph.ph_magic = PH_MAGIC;
	ph.ph_entries = nentries;
	ph.ph_blocksize = BC_blocksize;
	if (lseek(fd, 0, SEEK_SET) != 0) {
		warn("could not seek on temporary playlist file");
		error = errno;
		goto out;
	}
	if (write(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		warn("could not write header to temporary playlist file");
		error = errno;
		goto out;
	}
	close(fd);
	fd = -1;

	/*
	 * Rename the temporary playlist file over the original.
	 */
	if (rename((const char *)tfname, pfname) != 0) {
		warn("could not save playlist file %s->%s", tfname, pfname);
		error = errno;
		goto out;
	}
	/* free here to avoid exploitable race with unlink below */
	free(tfname);
	tfname = NULL;

out:
	if (tfname != NULL) {
		unlink(tfname);
		free(tfname);
	}
	if (fd != -1)
		close(fd);
	return(error);
	
}

/*
 * Merge two playlists 'a' and 'b' into 'a's buffer.
 *
 * Does not sort or coalesce the lists.  Robust in the case
 * where any list pointer is NULL or length is zero.
 */
int
BC_merge_playlists(struct BC_playlist_entry **ppa, int *pna,
    const struct BC_playlist_entry *pb, int nb)
{
	struct BC_playlist_entry *pc;
	int nentries;

	pc = *ppa;
	nentries = *pna + nb;
	if ((pc = realloc(pc, sizeof(*pc) * nentries)) == NULL)
		return(ENOMEM);
	bcopy(pb, pc + *pna, nb * sizeof(*pc));
	*ppa = pc;
	*pna = nentries;
	return(0);
}

/*
 * Sort a playlist.
 */
static int
compare_playlist(const void *vfirst, const void *vsecond)
{
	const struct BC_playlist_entry	*first, *second;

	first = (const struct BC_playlist_entry *)vfirst;
	second = (const struct BC_playlist_entry *)vsecond;

	if (first->pce_offset == second->pce_offset)
		return(0);
	return((first->pce_offset < second->pce_offset) ? -1 : 1);
}

void
BC_sort_playlist(struct BC_playlist_entry *pc, int nentries)
{
	if ((pc != NULL) && (nentries > 0))
		qsort((void *)pc, nentries, sizeof(*pc), compare_playlist);
}

/*
 * Coalesece a sorted playlist into the smallest set of contiguous
 * extents.  Sets the new size of the playlist and realloc's the buffer.
 */
int
BC_coalesce_playlist(struct BC_playlist_entry **ppc, int *pnentries)
{
	struct BC_playlist_entry *pc, *dpc;
	int i, j, nentries, oentries;

	if ((*ppc == NULL) || (*pnentries <= 0))
		return(0);
	
	/*
	 * Scan the sorted list and emit coalesced playlist entries.
	 */
	pc = *ppc;
	nentries = *pnentries;
	oentries = 0;
	i = 0;
	dpc = pc;
	while (i < nentries) {
		/* entry is the first in a possible set */
		
		/* scan following entries to see if they can be coalesced */
		for (j = 1; (i + j) < nentries; j++) {
			/* entry is not inside or adjacent to preceeding */
			if ((pc + j)->pce_offset > (pc->pce_offset + pc->pce_length))
				break;
			/* adjust length if required */
			pc->pce_length = MAX((pc->pce_offset + pc->pce_length),
			    ((pc + j)->pce_offset + (pc + j)->pce_length)) - pc->pce_offset;
			pc->pce_batch = MIN(pc->pce_batch, (pc + j)->pce_batch);
		}

		/* save entry */
		*(dpc++) = *pc;
		oentries++;
		i += j;
		pc += j;
	}

	/*
	 * Shrink the alloction if possible.  If realloc fails, handle it
	 * gracefully.
	 */
	pc = *ppc;
	*ppc = realloc(pc, sizeof(*pc) * oentries);
	if (*ppc == NULL)
		*ppc = pc;
	*pnentries = oentries;
	return(0);
}

/*
 * Fetch cache statistics.
 */
int
BC_fetch_statistics(struct BC_statistics **pss)
{
	struct BC_command bc;
	static struct BC_statistics ss;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_STATS;
	bc.bc_data = (uintptr_t) &ss;
	bc.bc_length = (unsigned int) sizeof(ss);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		/* ENOENT means the kext is unloaded, that's OK */
		if (errno != ENOENT)
			warn("could not fetch cache statistics");
		return(error);
	}
	*pss = &ss;
	if (BC_blocksize == 0)
		BC_blocksize = ss.ss_blocksize;
	return(0);
}

/*
 * Convert a list of history entries into a smaller list of
 * playlist entries, rounding them to match the current blocksize.
 *
 * Detects the presence of a prefetch tag and marks playlist entries prior
 * to the tag as requiring prefetch.
 *
 * Returns the playlist in an allocated buffer.
 */
int
BC_convert_history(const struct BC_history_entry *he, struct BC_playlist_entry **ppc, int *pnentries)
{
	struct BC_playlist_entry *pc, *pcp;
	u_int64_t end;
	int oentries, nentries, ent, curbatch;

	if (he == NULL)
		return(0);
	oentries = *pnentries;

	if ((pc = malloc(sizeof(*pc) * oentries)) == NULL)
		return(ENOMEM);

	/* scan history and convert */
	curbatch = 0;
	pcp = pc;
	nentries = 0;
	for (ent = 0; ent < oentries; ent++, he++) {

		/* if we find a tag, mark the next batch */
		if ((he->he_flags == BC_HE_TAG) && (curbatch < CE_BATCH_MASK)) {
			curbatch++;
			continue;
		}

		/* if we find a writethrough, discard it (debugging use only) */
		if (he->he_flags == BC_HE_WRITE)
			continue;

		/* convert history entry across */
		pcp->pce_offset = BLOCK_ROUNDDOWN(he->he_offset);
		end = he->he_offset + he->he_length;
		pcp->pce_length = BLOCK_ROUNDUP(end) - pcp->pce_offset;
		pcp->pce_batch = curbatch;
		pcp++;
		nentries++;
	}

	*ppc = pc;
	*pnentries = nentries;
	return(0);
}

/*
 * Start the cache, feed it the playlist if provided.
 */
int
BC_start(struct BC_playlist_entry *pc, int nentries)
{
	struct BC_command bc;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_START;
	bc.bc_param = BC_blocksize;
	bc.bc_data = (uintptr_t) pc;
	bc.bc_length = (unsigned int) (nentries * sizeof(*pc));
	return(sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc)) ? errno : 0);
}

int
BC_stop(struct BC_history_entry **phe, int *pnentries)
{
	struct BC_command bc;
	struct BC_history_entry *he;
	int error;
	size_t nsize;

	/*
	 * Stop the cache and get the history buffer size.
	 */
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_STOP;
	nsize = sizeof(bc);
	error = sysctlbyname(BC_SYSCTL, &bc, &nsize, &bc, nsize);
	if (error != 0) {
		/* if cache was not running, not really an error */
		if (errno != ENXIO)
			warn("could not stop cache");
		return(errno);
	}
	if (nsize != sizeof(bc)) {
		warnx("control structure wrong size, version mismatch?");
		return(EINVAL);
	}

	/*
	 * Fetch and clear the history buffer.
	 */
	bc.bc_opcode = BC_OP_HISTORY;
	he = NULL;
	if (bc.bc_length == 0) {
		bc.bc_data = 0;
	} else {
		if ((he = malloc(bc.bc_length)) == NULL) {
			warnx("could not allocate history buffer memory");
			return(ENOMEM);
		}
		bc.bc_data = (uintptr_t) he;
	}
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		warn("could not fetch %llu bytes of history", bc.bc_length);
		if (he != NULL)
			free(he);
		return(errno);
	}
	
	*phe = (struct BC_history_entry *) ((uintptr_t) bc.bc_data);
	*pnentries = (unsigned int) bc.bc_length / (unsigned int) sizeof(struct BC_history_entry);
	return(0);
}

int
BC_print_statistics(char *fname, struct BC_statistics *ss)
{
	FILE *fp;
	int msec = 0, b;

	if (ss == NULL)
		return(0);
	
	errno = 0;
	if (fname != NULL) {
		fp = fopen(fname, "w");
	} else {
		fp = stdout;
	}
	if (fp == NULL)
		return(errno);

	/* readahead */
	fprintf(fp, "initiated reads           %u\n", ss->ss_initiated_reads);
	fprintf(fp, "blocks read               %u\n", ss->ss_read_blocks);
	fprintf(fp, "read errors               %u\n", ss->ss_read_errors);
	fprintf(fp, "blocks discarded by error %u\n", ss->ss_error_discards);
	for(b = 0; b < STAT_BATCHMAX; b++) {
		if (ss->ss_batch_time[b] > 0) {
			fprintf(fp, "batch %d time              %d.%03ds\n",
					b, 
					ss->ss_batch_time[b] / 1000, 
					ss->ss_batch_time[b] % 1000);
			msec += ss->ss_batch_time[b];
		}
	}

	for(b = 0; b < STAT_BATCHMAX - 1; b++) {
		fprintf(fp, "blocks read in batch %d:   %u\n", b, ss->ss_batch_size[b]);
	}

	if (msec > 0) {
		fprintf(fp, "reader thread rate        %ukB/s, %utps\n",
				(u_int)(((unsigned long long)ss->ss_read_blocks * ss->ss_blocksize * 1000) / (msec * 1024)),
				(ss->ss_initiated_reads * 1000) / msec);
	}

	/* inbound strategy */
	fprintf(fp, "total strategy calls      %u\n", ss->ss_strategy_calls);
	fprintf(fp, "non-read strategy calls   %u\n", ss->ss_strategy_nonread);
	fprintf(fp, "throttled strategy calls  %u\n", ss->ss_strategy_throttled);
	fprintf(fp, "bypassed strategy calls   %u\n", ss->ss_strategy_bypassed);
	fprintf(fp, "bypasses while active     %u\n", ss->ss_strategy_bypass_active);
	fprintf(fp, "filled strategy calls     %u\n", ss->ss_strategy_calls - ss->ss_strategy_bypassed);
	fprintf(fp, "filled during active I/O  %u\n", ss->ss_strategy_duringio);
	fprintf(fp, "preload time              %d.%03ds\n",
			ss->ss_preload_time / 1000, ss->ss_preload_time % 1000);
	fprintf(fp, "active time               %d.%03ds\n",
			ss->ss_active_time / 1000, ss->ss_active_time % 1000);
	if (ss->ss_active_time != 0)
		fprintf(fp, "read/write strategy rate  %u/%utps\n",
				((ss->ss_strategy_calls - ss->ss_strategy_nonread) * 1000) / ss->ss_active_time,
				(ss->ss_strategy_nonread * 1000) / ss->ss_active_time);
	if (ss->ss_strategy_blocked > 0)
		fprintf(fp, "callers blocked           %u\n", ss->ss_strategy_blocked);
	if (ss->ss_strategy_stolen > 0)
		fprintf(fp, "stolen page bypasses      %u\n", ss->ss_strategy_stolen);
	
	/* extents */
	fprintf(fp, "extents in cache          %u\n", ss->ss_total_extents);
	fprintf(fp, "extent lookups            %u\n", ss->ss_extent_lookups);
	fprintf(fp, "extent hits               %u\n", ss->ss_extent_hits);
	if (ss->ss_extent_lookups > 0)
		fprintf(fp, "extent hit ratio          %.2f%%\n",
		    ((float)ss->ss_extent_hits * 100) / ss->ss_extent_lookups);
	fprintf(fp, "hits not fulfilled        %u\n", ss->ss_hit_blkmissing);

	/* block/page activity */
	fprintf(fp, "blocks discarded by write %u\n", ss->ss_write_discards);
	fprintf(fp, "blocks requested          %u\n", ss->ss_requested_blocks);
	fprintf(fp, "blocks hit                %u\n", ss->ss_hit_blocks);
	fprintf(fp, "blocks lost from cache    %u\n", ss->ss_lost_blocks);
	if (ss->ss_requested_blocks > 0)
		fprintf(fp, "block hit ratio           %.2f%%\n",
		    ((float)(ss->ss_hit_blocks * 100) / ss->ss_requested_blocks));
	fprintf(fp, "leftover blocks           %u\n", ss->ss_spurious_blocks);
	if (ss->ss_read_blocks > 0)
		fprintf(fp, "block wastage             %.2f%%\n",
		    ((float)ss->ss_spurious_blocks * 100) / ss->ss_read_blocks);

	/* history */
	fprintf(fp, "history clusters          %u\n", ss->ss_history_clusters);

	if (fp == stdout) {
		fflush(fp);
	} else {
		fclose(fp);
	}
	return(0);
}

int
BC_print_history(char *fname, struct BC_history_entry *he, int nentries)
{
	FILE *fp;

	errno = 0;
	if (fname != NULL) {
		fp = fopen(fname, "w");
	} else {
		fp = stdout;
	}
	if (fp == NULL)
		return(errno);

	while (nentries--) {
		fprintf(fp, "%-10llu %-5llu %s%s%s\n",
		    he->he_offset, he->he_length,
		    he->he_flags == BC_HE_HIT  ? "hit"  : "",
		    he->he_flags == BC_HE_MISS ? "miss" : "",
		    he->he_flags == BC_HE_TAG  ? "tag"  : "");
		he++;
	}

	if (fp == stdout) {
		fflush(fp);
	} else {
		fclose(fp);
	}
	return(0);
}

int
BC_tag_history(void)
{
	struct BC_command bc;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_TAG;
	bc.bc_data = 0;
	bc.bc_length = 0;
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		warn("could not insert prefetch tag");
		return(ENOENT);
	}
	return(0);
}

/*
 * Unload the BootCache kext.
 */
int
BC_unload(void)
{
#if 1
	return(0);
#else
	pid_t	child;
	char	*argv[4];
	int	result;

	child = fork();
	switch (child) {
	case -1:
		/* fork failed, bail with error */
		return(errno);
	case 0:
		/* we are the child, do our work */
		argv[0] = BC_KEXTUNLOAD;
		argv[1] = "-b";
		argv[2] = BC_BUNDLE_ID;
		argv[3] = NULL;
		result = execve(BC_KEXTUNLOAD, argv, NULL);
		exit((result != 0) ? 1 : 0);
	default:
		/* we are the parent, wait for the child */
		waitpid(child, &result, 0);
		break;
	}
	/* EBUSY is not a good error */
	return((result != 0) ? EBUSY : 0);
#endif
}
