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
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/xattr.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#include "BootCache.h"

/*
 * Return a user-readable string for a given uuid
 *
 * Returns a pointer to a static buffer, which is
 * racy, so this should only be used for debugging purposes
 */
static inline const char* uuid_string(uuid_t uuid)
{
	/* Racy, but used for debug output so who cares */
	static uuid_string_t uuidString;
	uuid_unparse(uuid, uuidString);
	return (char*)uuidString;
}

void BC_free_playlist(struct BC_playlist *pc) {
	if (pc) {
		if (pc->p_mounts)
			free(pc->p_mounts);
		if (pc->p_entries)
			free(pc->p_entries);
		free(pc);
	}
}

void BC_free_history(struct BC_history *hc) {
	if (hc) {
		if (hc->h_mounts)
			free(hc->h_mounts);
		if (hc->h_entries)
			free(hc->h_entries);
		free(hc);
	}
}

/*
 * Read the named playlist from disk into an allocated buffer.
 */
int
BC_read_playlist(const char *pfname, struct BC_playlist **ppc)
{
	struct BC_playlist *pc;
	struct BC_playlist_header ph;
	int error, fd;

	fd = -1;
	pc = NULL;
	*ppc = NULL;

	
	if (pfname == NULL) {
		warnx("No playlist path provided");
		error = EINVAL;
		goto out;
	}
	if ((fd = open(pfname, O_RDONLY)) == -1) {
		warnx("Unable to open %s: %d %s", pfname, errno, strerror(errno));
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

	if ((pc = calloc(1, sizeof(*pc))) == NULL) {
		warnx("Unable to allocate playlist structure");
		error = errno;
		goto out;
	}

	pc->p_nmounts  = ph.ph_nmounts;
	pc->p_nentries = ph.ph_nentries;
	
	if ((pc->p_mounts = malloc(sizeof(*pc->p_mounts) * pc->p_nmounts)) == NULL) {
		warnx("could not allocate memory for playlist mounts");
		error = errno;
		goto out;
	}
	if (read(fd, pc->p_mounts, sizeof(*pc->p_mounts) * pc->p_nmounts) != (sizeof(*pc->p_mounts) * pc->p_nmounts)) {
		warnx("could not read playlist mounts");
		error = EINVAL;
		goto out;
	}
	
	if ((pc->p_entries = malloc(sizeof(*pc->p_entries) * pc->p_nentries)) == NULL) {
		warnx("could not allocate memory for playlist data");
		error = errno;
		goto out;
	}
	if (read(fd, pc->p_entries, (sizeof(*pc->p_entries) * pc->p_nentries)) != (sizeof(*pc->p_entries) * pc->p_nentries)) {
		warnx("could not read playlist data");
		error = EINVAL;
		goto out;
	}
	
	if ((error = BC_verify_playlist(pc)) != 0) {
		goto out;
	}
		
	*ppc = pc;
out:
	if (fd != -1)
		close(fd);
	if (error) {
		PC_FREE_ZERO(pc);
	}
	return(error);
}

/*
 * Write the playlist to the named file, securely.
 */
int
BC_write_playlist(const char *pfname, const struct BC_playlist *pc)
{
	struct BC_playlist_header ph;
	char *tfname;
	int error, fd;

	tfname = NULL;
	fd = -1;
	
	if ((error = BC_verify_playlist(pc)) != 0) {
		goto out;
	}
	
	/*
	 * Prepare the output file.
	 *
	 * Create a secure temporary file and write an invalid header.
	 */
	size_t tpathlen = strlen(pfname) + 8;
	if (tpathlen > MAXPATHLEN) {
		warnx("playlist filename too long");
		error = ENAMETOOLONG;
		goto out;
	}
	if ((tfname = malloc(tpathlen)) == NULL) {
		warnx("could not allocate %lu bytes for playlist filename", strlen(pfname));
		error = errno;
		goto out;
	}
	snprintf(tfname, tpathlen, "%s.XXXXXX", pfname);
	if ((fd = mkstemp(tfname)) < 0) {
		warnx("could not create temporary playlist file");
		error = errno;
		goto out;
	}
	ph.ph_magic   = 0;
	ph.ph_nmounts  = 0;
	ph.ph_nentries = 0;
	if (write(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		warnx("could not write initial header to temporary playlist file");
		error = errno;
		goto out;
	}
		
	/*
	 * Write the playlist mounts.
	 */
	if (write(fd, pc->p_mounts, pc->p_nmounts * sizeof(*pc->p_mounts)) != (pc->p_nmounts * sizeof(*pc->p_mounts))) {
		warnx("could not write mounts to temporary playlist file: %d %s", errno, strerror(errno));
		error = errno;
		goto out;
	}	
	
	/*
	 * Write the playlist entries.
	 */
	if (write(fd, pc->p_entries, pc->p_nentries * sizeof(*pc->p_entries)) != (pc->p_nentries * sizeof(*pc->p_entries))) {
		warnx("could not write entries to temporary playlist file");
		error = errno;
		goto out;
	}
	
	/*
	 * Write an updated (valid) header to the playlist file.
	 */
	ph.ph_magic   = PH_MAGIC;
	ph.ph_nmounts  = pc->p_nmounts;
	ph.ph_nentries = pc->p_nentries;
	if (lseek(fd, 0, SEEK_SET) != 0) {
		warnx("could not seek on temporary playlist file");
		error = errno;
		goto out;
	}
	if (write(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		warnx("could not write header to temporary playlist file");
		error = errno;
		goto out;
	}
	close(fd);
	fd = -1;

	/*
	 * Rename the temporary playlist file over the original.
	 */
	if (rename((const char *)tfname, pfname) != 0) {
		warnx("could not save playlist file %s->%s", tfname, pfname);
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
BC_merge_playlists(struct BC_playlist* pa, const struct BC_playlist* pb)
{	
	int mount_idx_b, mount_idx_a, entry_idx;
	if ((pa->p_mounts = reallocf(pa->p_mounts, sizeof(*pa->p_mounts) * (pa->p_nmounts + pb->p_nmounts))) == NULL)
		return(ENOMEM);

	/* merge the mount list */
	for (mount_idx_b = 0; mount_idx_b < pb->p_nmounts; mount_idx_b++) {
		for (mount_idx_a = 0; mount_idx_a < pa->p_nmounts; mount_idx_a++) {
			if (0 == uuid_compare(pa->p_mounts[mount_idx_a].pm_uuid, pb->p_mounts[mount_idx_b].pm_uuid)) {
				break;
			}
		}
		
		if (mount_idx_a == pa->p_nmounts) {
			pa->p_nmounts++;
			
			pa->p_mounts[mount_idx_a].pm_nentries = 0;
			uuid_copy(pa->p_mounts[mount_idx_a].pm_uuid, pb->p_mounts[mount_idx_b].pm_uuid);
		}
		
		pa->p_mounts[mount_idx_a].pm_nentries += pb->p_mounts[mount_idx_b].pm_nentries;
	}
	
	if ((pa->p_entries = reallocf(pa->p_entries, sizeof(*pa->p_entries) * (pa->p_nentries + pb->p_nentries))) == NULL)
		return(ENOMEM);
	memcpy(pa->p_entries + pa->p_nentries, pb->p_entries, pb->p_nentries * sizeof(*pb->p_entries));
	
	/* fixup mount indexes for the new entries */
	for (entry_idx = pa->p_nentries; entry_idx < (pa->p_nentries + pb->p_nentries); entry_idx++) {
		mount_idx_b = pa->p_entries[entry_idx].pe_mount_idx;
		for (mount_idx_a = 0; mount_idx_a < pa->p_nmounts; mount_idx_a++) {
			if (0 == uuid_compare(pa->p_mounts[mount_idx_a].pm_uuid, pb->p_mounts[mount_idx_b].pm_uuid)) {
				pa->p_entries[entry_idx].pe_mount_idx = mount_idx_a;
				break;
			}
		}
		if (mount_idx_a == pa->p_nmounts) {
			warnx("Mounts not merged properly for entry %d (mount %d)", entry_idx, mount_idx_b);
			return(1);
		}
	}

	pa->p_nentries += pb->p_nentries;
	
#ifdef BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET
    BC_sort_playlist(pa);
#endif
    BC_coalesce_playlist(pa);
	
	return BC_verify_playlist(pa);
}

/*
 * Sort a playlist.
 */
static int
compare_playlist_entry(const void *vfirst, const void *vsecond)
{
	const struct BC_playlist_entry	*first, *second;

	first = (const struct BC_playlist_entry *)vfirst;
	second = (const struct BC_playlist_entry *)vsecond;

	// Sort by volume first, then by logical block address
	int uuid_comparison = first->pe_mount_idx - second->pe_mount_idx;
	if (uuid_comparison != 0)
		return((uuid_comparison < 0) ? -1 : 1);

	if (first->pe_offset == second->pe_offset)
		return(0);
	return((first->pe_offset < second->pe_offset) ? -1 : 1);
}

void
BC_sort_playlist(struct BC_playlist *pc)
{
	if ((pc == NULL) || (pc->p_mounts == NULL) || (pc->p_entries == NULL) || (pc->p_nmounts == 0) || (pc->p_nentries == 0))
		return;
		
	if (pc->p_nentries > 0)
		qsort((void *)pc->p_entries, pc->p_nentries, sizeof(*pc->p_entries), compare_playlist_entry);
}

/*
 * Coalesece a sorted playlist into the smallest set of contiguous
 * extents.  Sets the new size of the playlist and realloc's the buffer.
 */
int
BC_coalesce_playlist(struct BC_playlist *pc)
{
	struct BC_playlist_entry *pe, *dpe;
	int i, oentries, mount_idx;
	
	/*
	 * Scan the sorted list and emit coalesced playlist entries.
	 */
	pe = pc->p_entries;
	oentries = 0;
	dpe = pe;
	
	/* clear the number of entries per mount to be recalculated below */
	for (mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++)
		pc->p_mounts[mount_idx].pm_nentries = 0;
	
	while (pe < pc->p_entries + pc->p_nentries) {
		/* entry is the first in a possible set */
		
		if (pe->pe_length > 0) { /* skip entries we've marked as empty */
			/* scan following entries to see if they can be coalesced */
			for (i = 1; (pe + i) < (pc->p_entries + pc->p_nentries); i++) {
				
				/* not the same mount, ignore */
				if (pe->pe_mount_idx != (pe + i)->pe_mount_idx)
					continue;
				
				/* entry is not inside or adjacent to preceeding */
				if ((pe + i)->pe_offset > (pe->pe_offset + pe->pe_length))
					break;
				
#define MAX_MERGE_SIZE (8*1024*1024)
				
				/* If these extents are both low priority, or both not low-priority but in the same batch, or combined they only grow the earlier extent by at most 8MB, merge them together
				 * Or, if we'd be subtracting out the middle of the first extent and so would require a new extent in order to keep the top half, go ahead and merge since we can't handle that case */
				if (((pe->pe_flags & BC_PE_LOWPRIORITY) && ((pe + i)->pe_flags & BC_PE_LOWPRIORITY)) ||
					(!(pe->pe_flags & BC_PE_LOWPRIORITY) && !((pe + i)->pe_flags & BC_PE_LOWPRIORITY) && pe->pe_batch == (pe + i)->pe_batch) ||
					(pe->pe_batch < (pe + i)->pe_batch && (int64_t)((pe + i)->pe_offset + (pe + i)->pe_length) - (pe->pe_offset + pe->pe_length) <= MAX_MERGE_SIZE) ||
					(pe->pe_batch > (pe + i)->pe_batch && (int64_t)(pe + i)->pe_offset - pe->pe_offset <= MAX_MERGE_SIZE) ||
					((pe->pe_offset + pe->pe_length) > ((pe + i)->pe_offset + (pe + i)->pe_length) && (pe->pe_batch > (pe + i)->pe_batch || pe->pe_flags & BC_PE_LOWPRIORITY))
					) {
					/* merge the two extents together */
					
					/* adjust length if required */
					pe->pe_length = MAX((pe->pe_offset + pe->pe_length), ((pe + i)->pe_offset + (pe + i)->pe_length)) - pe->pe_offset;
					pe->pe_batch = MIN(pe->pe_batch, (pe + i)->pe_batch);
					if ( !((pe + i)->pe_flags & BC_PE_LOWPRIORITY)) {
						/* If any are high priority, the merged extent is high priority */
						pe->pe_flags &= (~BC_PE_LOWPRIORITY);
					}
					if ((pe + i)->pe_flags & BC_PE_SHARED) {
						/* If any are shared, the merged extent is shared */
						pe->pe_flags |= BC_PE_SHARED;
					}
					(pe + i)->pe_length = 0; /* mark this entry as empty */
				} else {
					/* subtract the intersection from the later extent */
					
					if (pe->pe_batch < (pe + i)->pe_batch ||
						(pe + i)->pe_flags & BC_PE_LOWPRIORITY) {
						/* subtract intersection from pe+i */
						
						u_int64_t endpoint = MAX((pe->pe_offset + pe->pe_length), ((pe + i)->pe_offset + (pe + i)->pe_length));
						(pe + i)->pe_offset = pe->pe_offset + pe->pe_length;
						(pe + i)->pe_length = endpoint - (pe + i)->pe_offset;
					} else {
						/* subtract intersection from pe */
						/* We made sure that ((pe + i)->pe_offset + (pe + i)->pe_length) is greater than (pe->pe_offset + pe->pe_length) above */
						pe->pe_length = (pe + i)->pe_offset - pe->pe_offset;
					}
				}
			
			}
			
			/* save entry */
			*(dpe++) = *pe;
			oentries++;		
			pc->p_mounts[pe->pe_mount_idx].pm_nentries++;
		}
		pe++;
	}

	/*
	 * Shrink the alloction if possible.  If realloc fails, handle it
	 * gracefully.
	 */
	pc->p_nentries = oentries;
	pe = pc->p_entries;
	pc->p_entries = realloc(pc->p_entries, sizeof(*pc->p_entries) * pc->p_nentries);
	if (pc->p_entries == NULL)
		pc->p_entries = pe;
	return(0);
}

/*
 * Verify the consistency of a playlist
 *
 * Returns 0 if the playlist is consistent, non-0 otherwise
 */
int
BC_verify_playlist(const struct BC_playlist *pc)
{
	int i, error = 0;
	int* mount_counts = NULL;
	
	if (pc == NULL) {
		warnx("playlist is null");
		error = EINVAL;
		goto out;
	}
	if ((pc->p_mounts == NULL) && (pc->p_nmounts > 0)) {
		warnx("mounts is null");
		error = EINVAL;
		goto out;
	}
	if ((pc->p_entries == NULL) && (pc->p_nentries > 0)) {
		warnx("entries is null");
		error = EINVAL;
		goto out;
	}
	if (pc->p_nentries > 0 && pc->p_nmounts == 0) {
		warnx("entries, but no mounts");
		error = EINVAL;
		goto out;
	}
	
	/* A playlist with nothing in it is technically valid */
	if (pc->p_nmounts == 0) {
		error = 0;
		goto out;
	}
	
	mount_counts = calloc(pc->p_nmounts, sizeof(int));
	
	for (i = 0; i < pc->p_nentries; i++) {
		if (pc->p_entries[i].pe_mount_idx >= pc->p_nmounts) {
			warnx("entry %d referenced unknown mount %d", i, pc->p_entries[i].pe_mount_idx);
			error = EINVAL;
			goto out;
		}
		mount_counts[pc->p_entries[i].pe_mount_idx]++;
		
		if (pc->p_entries[i].pe_offset + pc->p_entries[i].pe_length <= pc->p_entries[i].pe_offset) {
			warnx("entry %d has a bad range %lld,%lld", i, pc->p_entries[i].pe_offset, pc->p_entries[i].pe_length);
			error = EINVAL;
			goto out;
		}
		
	}
	
	for (i = 0; i < pc->p_nmounts; i++) {
		if (pc->p_mounts[i].pm_nentries == 0) {
			warnx("mount %d has 0 entries", i);
			error = EINVAL;
			goto out;
		}
		if (mount_counts[i] != pc->p_mounts[i].pm_nentries) {
			warnx("%d entries exist for mount %d, vs %d claimed", mount_counts[i], i, pc->p_mounts[i].pm_nentries);
			error = EINVAL;
			goto out;
		}
	}
	
out:
	if (mount_counts)
		free(mount_counts);
	
	return (error);
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
	bc.bc_data1 = (uintptr_t) &ss;
	bc.bc_data1_size = (unsigned int) sizeof(ss);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	*pss = &ss;
	return(0);
}

/*
 * Convert a list of history entries into a smaller list of
 * playlist entries.
 *
 * Detects the presence of a prefetch tag and marks playlist entries prior
 * to the tag as requiring prefetch.
 *
 * Returns the playlist in an allocated buffer.
 */
int
BC_convert_history(const struct BC_history *hc, struct BC_playlist **ppc)
{
	struct BC_playlist *pc;
	struct BC_playlist_mount *pm;
	struct BC_playlist_entry *pe;
	struct BC_history_mount *hm;
	struct BC_history_entry *he;
	int error, mount_idx, curbatch;
	
	*ppc = NULL;

	if (hc == NULL) {
		error = EINVAL;
		goto out;
	}

	if ((pc = calloc(1, sizeof(*pc))) == NULL) {
		error = errno;
		goto out;
	}

	/* We won't convert all the history mounts into playlist mounts (no uuid) */
	if ((pc->p_mounts = malloc(sizeof(*pc->p_mounts) * hc->h_nmounts)) == NULL) {
		error = errno;
		goto out;
	}
	pc->p_nmounts = 0;
	for (hm = hc->h_mounts; hm < (hc->h_mounts + hc->h_nmounts); hm++) {
		if (! uuid_is_null(hm->hm_uuid)) {
			pm = pc->p_mounts + pc->p_nmounts;
			uuid_copy(pm->pm_uuid, hm->hm_uuid);
			pm->pm_nentries = 0;
			pc->p_nmounts++;
		}
	}	

	/* We won't convert all the history entries into playlist entries (writes, tags) */
	if ((pc->p_entries = malloc(sizeof(*pc->p_entries) * hc->h_nentries)) == NULL) {
		error = errno;
		goto out;
	}
	/* scan history and convert */
	curbatch = 0;
	pc->p_nentries = 0;
	for (he = hc->h_entries; he < (hc->h_entries + hc->h_nentries); he++) {
		
		/* if we find a tag, mark the next batch */
		if (he->he_flags & BC_HE_TAG) {
			if (curbatch < BC_MAXBATCHES) {
				curbatch++;
			}
			continue;
		}
		
		/* if we find a writethrough, discard it (debugging use only) */
		if (he->he_flags & BC_HE_WRITE)
			continue;

		if (he->he_length == 0)
			continue;

		if (he->he_mount_idx >= hc->h_nmounts) {
			warnx("History entry %ld referenced mount index %d, but there are only %d mounts", (long)(he - hc->h_entries), he->he_mount_idx, hc->h_nmounts);
			error = EINVAL;
			goto out;
		}
		
		hm = hc->h_mounts + he->he_mount_idx;
		pe = pc->p_entries + pc->p_nentries;
		
		/* convert history entry across */
		for (mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
			if (0 == uuid_compare(pc->p_mounts[mount_idx].pm_uuid, hm->hm_uuid)) {
				break;
			}
		}
		if (mount_idx >= pc->p_nmounts) {
			/* Entry matched a mount we filtered out */
			continue;
		}
		
		pm = pc->p_mounts + mount_idx;
		
		pe->pe_mount_idx = mount_idx;
		pe->pe_offset = he->he_offset;
		pe->pe_length = he->he_length;
		pe->pe_batch = curbatch;
		pe->pe_flags = 0;
		
		if (pm->pm_nentries >= hm->hm_nentries) {
			warnx("Bad playlist: more entries existed than mount %s claimed (%d)", uuid_string(pm->pm_uuid), hm->hm_nentries);
			error = EINVAL;
			goto out;
		}
		
		pm->pm_nentries++;
		pc->p_nentries++;
	}
	
	/* Remove any mounts with no entries */
	for (mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
		pm = pc->p_mounts + mount_idx;
		if (pm->pm_nentries == 0) {
			pc->p_nmounts--;
			if (mount_idx < pc->p_nmounts) {
				memcpy(pm, pm + 1, sizeof(*pm) * (pc->p_nmounts - mount_idx));
				for (pe = pc->p_entries; pe < (pc->p_entries + pc->p_nentries); pe++)
					if (pe->pe_mount_idx > mount_idx)
						pe->pe_mount_idx--;
				mount_idx--;
			}
		}
	}
	
	if ((error = BC_verify_playlist(pc)) != 0) {
		goto out;
	}
	
	*ppc = pc;
	
out:
	if (error) {
		PC_FREE_ZERO(pc);
	}
	return(error);
}

/*
 * Start the cache, feed it the playlist if provided.
 */
int
BC_start(struct BC_playlist *pc)
{
	struct BC_command bc;
	
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_START;
	if (pc) {
		bc.bc_data1      = (uintptr_t)     pc->p_mounts;
		bc.bc_data1_size = (unsigned int) (pc->p_nmounts * sizeof(*pc->p_mounts));
		bc.bc_data2      = (uintptr_t)     pc->p_entries;
		bc.bc_data2_size = (unsigned int) (pc->p_nentries * sizeof(*pc->p_entries));
	} else {
		bc.bc_data1      = 0;
		bc.bc_data1_size = 0;
		bc.bc_data2      = 0;
		bc.bc_data2_size = 0;
	}
	return(sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc)) ? errno : 0);
}

int
BC_stop(struct BC_history **phc)
{
	struct BC_command bc;
	struct BC_history *hc;
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
			warnx("could not stop cache: %d %s", errno, strerror(errno));
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
	if ((hc = calloc(1, sizeof(*hc))) == NULL) {
		warnx("could not allocate history struct memory");
		return(ENOMEM);
	}
	if (bc.bc_data1_size == 0 || bc.bc_data2_size == 0) {
		bc.bc_data1      = 0;
		bc.bc_data2      = 0;
		bc.bc_data1_size = 0;
		bc.bc_data2_size = 0;
	} else {
		if ((hc->h_mounts = malloc(bc.bc_data1_size)) == NULL) {
			warnx("could not allocate history mounts memory");
			HC_FREE_ZERO(hc);
			return(ENOMEM);
		}
		if ((hc->h_entries = malloc(bc.bc_data2_size)) == NULL) {
			warnx("could not allocate history mounts memory");
			HC_FREE_ZERO(hc);
			return(ENOMEM);
		}
		hc->h_nmounts  = bc.bc_data1_size / sizeof(struct BC_history_mount);
		hc->h_nentries = bc.bc_data2_size / sizeof(struct BC_history_entry);

		bc.bc_data1 = (uintptr_t) hc->h_mounts;
		bc.bc_data2 = (uintptr_t) hc->h_entries;
	}
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		warnx("could not fetch %u and %u bytes of history: %d %s", bc.bc_data1_size, bc.bc_data2_size, errno, strerror(errno));
		HC_FREE_ZERO(hc);
		return(errno);
	}
	
	if (hc->h_mounts || hc->h_entries) {
		hc->h_nmounts  = bc.bc_data1_size / sizeof(struct BC_history_mount);
		hc->h_nentries = bc.bc_data2_size / sizeof(struct BC_history_entry);
	}
		
	*phc = hc;
	return(0);
}

/*
 * Create a bootcache playlist for the given file's disk blocks.
 * If an error occurs, as much of the playlist as could be calculated is returned.
 * The caller is responsible for freeing the returned BC_playlist, if non-NULL.
 */
int
BC_playlist_for_file(int fd, struct BC_playlist** ppc) {
    return BC_playlist_for_filename(fd, NULL, 0, ppc);
}

/*
 * Create a bootcache playlist for the given file's disk blocks.
 * If an error occurs, as much of the playlist as could be calculated is returned.
 * The caller is responsible for freeing the returned BC_playlist, if non-NULL.
 *
 * Compressed files are handled appropriately if the filename is provided.
 */
int
BC_playlist_for_filename(int fd, const char *fname, off_t maxsize, struct BC_playlist** ppc) {
	struct BC_playlist* pc = NULL;
	int error = 0;
    int compressed_fd = -1;
    off_t filesize = 0;

	*ppc = NULL;

	struct stat fd_stats;
	if (0 != fstat(fd, &fd_stats)) {
		warnx("Unable to stat %d: %d %s", fd, errno, strerror(errno));
		error = errno;
        goto out;
	}
    
    filesize = fd_stats.st_size;
    
    if (fd_stats.st_flags & UF_COMPRESSED) {
        if (fname) {
            // If the file is compressed, figure out the size of the (hidden)
            // resource fork, if present
            size_t sz = getxattr(fname, XATTR_RESOURCEFORK_NAME, NULL, 0, 0,
                                 XATTR_SHOWCOMPRESSION);
            if (sz > 0){
                filesize = sz;
                
                // We want to do the fcntl on the resource fork, not the data fork
                char rsrcpath[PATH_MAX];
                sprintf(rsrcpath, "%s/..namedfork/rsrc", fname);
                compressed_fd = open(rsrcpath, O_RDONLY);
                if (compressed_fd < 0){
                    warnx("Unable to open compressed resource fork %s", rsrcpath);
                    error = errno;
                    goto out;
                }
                fd = compressed_fd;
            } else {
                /* This is a compressed file, but it has no resource fork. Nothing to do */
                error = EINVAL;
                goto out;                
            }
        } else {
            /* This is a compressed file, but we weren't provided a filename so can't grab the resource fork */
            error = EINVAL;
            goto out;
        }
    }

	struct {
		uint32_t size;
		uuid_t   uuid;
	} attrBuf = {0, {0}};

	// Get the mount's UUID where the file lives
	struct statfs statfs_buf;
	if (0 != fstatfs(fd, &statfs_buf)) {
		warnx("Unable to stafs %d: %d %s", fd, errno, strerror(errno));
		// Assume it's on the root volume
		uuid_clear(attrBuf.uuid);
        // Round up to the block size
        filesize = (((filesize + (512 - 1)) / 512) * 512);
	} else {
        // Round up to the block size
        filesize = (((filesize + (statfs_buf.f_bsize - 1)) / statfs_buf.f_bsize) * statfs_buf.f_bsize);
        
		struct attrlist list = {
			.bitmapcount = ATTR_BIT_MAP_COUNT,
			.volattr = ATTR_VOL_INFO | ATTR_VOL_UUID,
		};	
		
		if (0 != getattrlist(statfs_buf.f_mntonname,  &list, &attrBuf, sizeof(attrBuf), 0)) {
			warnx("Unable to determine uuid for volume %s", statfs_buf.f_mntonname);
			// Assume it's on the root volume
			uuid_clear(attrBuf.uuid);
		}
	}
    
    if (maxsize != 0 && maxsize < filesize) {
        filesize = maxsize;
    }
	
	// Allocate the playlist and inialize with the given mount
	pc = calloc(1, sizeof(*pc));
	if(!pc) {
		warnx("Unable to allocate playlist structure");
		error = errno;
        goto out;
	}
	pc->p_nmounts = 1;
	pc->p_mounts = malloc(sizeof(*pc->p_mounts));
	if(!pc->p_mounts) {
		warnx("could not allocate memory for playlist mounts");
		PC_FREE_ZERO(pc);
		error = errno;
        goto out;
	}
	pc->p_mounts[0].pm_nentries = 0;
	uuid_copy(pc->p_mounts[0].pm_uuid, attrBuf.uuid);
	pc->p_nentries = 0;
	pc->p_entries = NULL;
	
	off_t ra_off;
	for (ra_off = 0; (ra_off < filesize); ) {
		off_t remaining = (filesize - ra_off);
		struct log2phys block_range = {
			.l2p_flags       = 0,
			.l2p_devoffset   = ra_off,    //As an IN parameter to F_LOG2PHYS_EXT, this is the offset into the file
			.l2p_contigbytes = remaining, //As an IN parameter to F_LOG2PHYS_EXT, this is the number of bytes to be queried
		};
		
#if __LP64__
		int ret = syscall(SYS_fcntl, fd, F_LOG2PHYS_EXT, &block_range);
#else
		int ret = fcntl(fd, F_LOG2PHYS_EXT, &block_range);
#endif
		if (ret != 0) {
			//RLOG(NOTICE, "fcntl(%d, F_LOG2PHYS_EXT, &{.offset: %"PRIdoff", .bytes: %"PRIdoff"}) => %d (errno: %d %s)", fd, block_range.l2p_devoffset, block_range.l2p_contigbytes, ret, errno, strerror(errno));
			break;
		}
		
		// block_range.l2p_devoffset;   as an OUT parameter from F_LOG2PHYS_EXT, this is the offset in bytes on the disk
		// block_range.l2p_contigbytes; as an OUT parameter from F_LOG2PHYS_EXT, this is the number of bytes in the range
		
		if (remaining < block_range.l2p_contigbytes ) {
			warnx("Invalid size returned for %d from disk (%lld bytes requested, %lld bytes returned)", fd, remaining, block_range.l2p_contigbytes);
			break;
		}
		
		//RLOG(DEBUG, "%"PRIdoff":%"PRIdoff" is %"PRIdoff":%"PRIdoff" on disk\n", ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
				
		if (block_range.l2p_contigbytes == 0) {
			//RLOG(INFO, "%"PRIdoff":%"PRIdoff" returned %"PRIdoff":%"PRIdoff"\n", ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
			break;
		}
		
		if (block_range.l2p_devoffset < 0) {
			warnx("Invalid block range return for %d from disk (%lld:%lld returned %lld:%lld)\n", fd, ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
			break;
		}
		
		ra_off += block_range.l2p_contigbytes;
		
		// Add this range to our array
		// TODO: reduce the number of realloc calls
		pc->p_entries = reallocf(pc->p_entries, (pc->p_nentries + 1) * sizeof(*pc->p_entries));
		if(! pc->p_entries) {
			warnx("could not allocate memory for playlist data");
			error = errno;
            goto out;
		}
		pc->p_entries[pc->p_nentries].pe_offset    = block_range.l2p_devoffset;
		pc->p_entries[pc->p_nentries].pe_length    = block_range.l2p_contigbytes;
		pc->p_entries[pc->p_nentries].pe_batch     = 0;
		pc->p_entries[pc->p_nentries].pe_flags     = 0;
		pc->p_entries[pc->p_nentries].pe_mount_idx = 0;
		pc->p_mounts[0].pm_nentries++;
		pc->p_nentries++;
	}
	
    if (pc->p_nentries == 0) {
        error = ENOENT;
        goto out;
    }
    
	if ((error = BC_verify_playlist(pc)) != 0) {
        goto out;
	}
	
	*ppc = pc;
    error = 0;
out:
    if (compressed_fd >= 0) {
        close(compressed_fd);
    }
    if (error != 0) {
        PC_FREE_ZERO(pc);
    }
	return (error);
}

/*
 * Notify the kext that a new mount has appeared.
 */
int
BC_notify_mount(void)
{
	struct BC_command bc;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_MOUNT;
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		/* if cache was not running, not really an error */
		if (errno != ENXIO)
			warnx("could not notify cache of new mount: %d %s", errno, strerror(errno));
		return(errno);
	}

	return(0);
}

/*
 * Check if the boot cache is supported on this device.
 * Returns 0 if it is supported, non-0 if it isn't
 */
int
BC_test(void)
{
	struct BC_command bc;
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_TEST;
	return(sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc)) ? errno : 0);
}

/*
 * Jettison the cache.
 */
int
BC_jettison(void)
{
	struct BC_command bc;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_JETTISON;
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		/* if cache was not running, not really an error */
		if (errno != ENXIO)
			warnx("could not jettison cache: %d %s", errno, strerror(errno));
		return(errno);
	}

	return(0);
}

int
BC_print_statistics(char *fname, struct BC_statistics *ss)
{
	FILE *fp;
	uint disk_msecs, disk_bytes, d, b, m, bytes_remaining, other;

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
fprintf(fp, "initiated reads            %u\n", ss->ss_initiated_reads);
fprintf(fp, "blocks read                %u\n", ss->ss_read_blocks);
fprintf(fp, "bytes read                 %u\n", ss->ss_read_bytes);
	if (ss->ss_read_bytes > 0) {
		bytes_remaining = ss->ss_read_bytes - ss->ss_hit_bytes - ss->ss_read_errors_bytes - ss->ss_write_discards - ss->ss_read_discards - ss->ss_stolen_bytes - ss->ss_spurious_bytes - ss->ss_hit_bytes_afterhistory - ss->ss_lost_bytes_afterhistory - ss->ss_bypass_nocache_discards;
fprintf(fp, " bytes used                %-10u  %3.0f%%\n", ss->ss_hit_bytes, ((float)ss->ss_hit_bytes / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes remaining           %-10u  %3.0f%%\n", bytes_remaining, ((float)bytes_remaining / ss->ss_read_bytes) * 100);
fprintf(fp, " nonshared bytes read      %-10u  %3.0f%%\n", (ss->ss_read_bytes - ss->ss_shared_bytes), ((float)(ss->ss_read_bytes - ss->ss_shared_bytes) / ss->ss_read_bytes) * 100);
		if ((ss->ss_read_bytes - ss->ss_shared_bytes) > 0) {
fprintf(fp, "  nonshared bytes used     %-10u  %3.0f%%\n", (ss->ss_hit_bytes - ss->ss_hit_shared_bytes), ((float)(ss->ss_hit_bytes - ss->ss_hit_shared_bytes) / (ss->ss_read_bytes - ss->ss_shared_bytes)) * 100);
		}
fprintf(fp, " shared cache bytes read   %-10u  %3.0f%%\n", ss->ss_shared_bytes, ((float)ss->ss_shared_bytes / ss->ss_read_bytes) * 100);
		if (ss->ss_shared_bytes > 0) {
fprintf(fp, "  shared cache bytes used  %-10u  %3.0f%%\n", ss->ss_hit_shared_bytes, ((float)ss->ss_hit_shared_bytes / ss->ss_shared_bytes) * 100);
		}
fprintf(fp, " low priority bytes        %-10u  %3.0f%%\n", ss->ss_read_bytes_lowpri, ((float)ss->ss_read_bytes_lowpri / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes failed to read      %-10u  %3.0f%%\n", ss->ss_read_errors_bytes, ((float)ss->ss_read_errors_bytes / ss->ss_read_bytes) * 100);
		if (ss->ss_bypass_nocache_discards > 0) {
fprintf(fp, " bytes discarded by noncac %-10u  %3.0f%%\n", ss->ss_bypass_nocache_discards, ((float)ss->ss_bypass_nocache_discards / ss->ss_read_bytes) * 100);
		}
fprintf(fp, " bytes discarded by write  %-10u  %3.0f%%\n", ss->ss_write_discards, ((float)ss->ss_write_discards / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes discarded by read   %-10u  %3.0f%%\n", ss->ss_read_discards, ((float)ss->ss_read_discards / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes discarded by error  %-10u  %3.0f%%\n", ss->ss_error_discards, ((float)ss->ss_error_discards / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes stolen from cache   %-10u  %3.0f%%\n", ss->ss_stolen_bytes, ((float)ss->ss_stolen_bytes / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes used after boot     %-10u  %3.0f%%\n", ss->ss_hit_bytes_afterhistory, ((float)ss->ss_hit_bytes_afterhistory / ss->ss_read_bytes) * 100);
fprintf(fp, " bytes lost after boot     %-10u  %3.0f%%\n", ss->ss_lost_bytes_afterhistory, ((float)ss->ss_lost_bytes_afterhistory / ss->ss_read_bytes) * 100);
		if (bytes_remaining == 0 || 0 != ss->ss_spurious_bytes) {
fprintf(fp, " bytes wasted              %-10u  %3.0f%%\n", ss->ss_spurious_bytes, ((float)ss->ss_spurious_bytes / ss->ss_read_bytes) * 100);
		}
	}
fprintf(fp, "bytes requested            %u\n", ss->ss_requested_bytes);
	if (ss->ss_requested_bytes > 0) {
fprintf(fp, " bytes hit                 %-10u  %3.0f%%\n", ss->ss_hit_bytes, ((float)ss->ss_hit_bytes / ss->ss_requested_bytes) * 100);
		for (m = 0; m < STAT_MOUNTMAX; m++) {
			if (ss->ss_requested_bytes_m[m] > 0 && ss->ss_requested_bytes_m[m] != ss->ss_requested_bytes) {
fprintf(fp, "  mount %d bytes hit        %-10u  %3.0f%% of %u\n", m, ss->ss_hit_bytes_m[m], ((float)ss->ss_hit_bytes_m[m] / ss->ss_requested_bytes_m[m]) * 100, ss->ss_requested_bytes_m[m]);
			}
		}
fprintf(fp, " noncached bytes requested %-10u  %3.0f%%\n", ss->ss_bypass_nocache_bytes + ss->ss_hit_nocache_bytes, ((float)(ss->ss_bypass_nocache_bytes + ss->ss_hit_nocache_bytes) / ss->ss_requested_bytes) * 100);
		if (ss->ss_bypass_nocache_bytes + ss->ss_hit_nocache_bytes > 0) {
fprintf(fp, "  noncached bytes hit      %-10u  %3.0f%%\n", ss->ss_hit_nocache_bytes, ((float)ss->ss_hit_nocache_bytes / (ss->ss_bypass_nocache_bytes + ss->ss_hit_nocache_bytes)) * 100);
fprintf(fp, "  noncached bytes missed   %-10u  %3.0f%%\n", ss->ss_bypass_nocache_bytes, ((float)ss->ss_bypass_nocache_bytes / (ss->ss_bypass_nocache_bytes + ss->ss_hit_nocache_bytes)) * 100);
		}
	}

	if (ss->ss_initiated_reads > 0 && ss->ss_read_errors > 0) {
fprintf(fp, " read errors               %-10u  %3.0f%%\n", ss->ss_read_errors, ((float)ss->ss_read_errors / ss->ss_initiated_reads) * 100);
	}
	if (ss->ss_cache_time > 0 || ss->ss_read_bytes == 0) {
fprintf(fp, "cache active time          %d.%03ds\n", ss->ss_cache_time / 1000, ss->ss_cache_time % 1000);
	} else {
fprintf(fp, "cache active time          (still active)\n");
	}
	
	/* inbound strategy */
fprintf(fp, "\n");
fprintf(fp, "total strategy calls       %u\n", ss->ss_strategy_calls);
	if (ss->ss_strategy_calls > 0) {
fprintf(fp, " filled calls              %-10u  %3.0f%%\n", ss->ss_strategy_calls - ss->ss_strategy_bypassed, ((float)(ss->ss_strategy_calls - ss->ss_strategy_bypassed) / ss->ss_strategy_calls) * 100);
fprintf(fp, " bypassed calls            %-10u  %3.0f%%\n", ss->ss_strategy_bypassed, ((float)ss->ss_strategy_bypassed / ss->ss_strategy_calls) * 100);
		if (ss->ss_strategy_bypassed > 0) {
			other = ss->ss_strategy_bypassed - ss->ss_strategy_nonread - ss->ss_strategy_noncached_mount - ss->ss_strategy_nonblocksize - (ss->ss_extent_lookups - ss->ss_extent_hits) - ss->ss_strategy_bypass_duringio_unfilled - ss->ss_strategy_unfilled_lowpri;
fprintf(fp, "  nonread calls            %-10u  %3.0f%%\n", ss->ss_strategy_nonread, ((float)ss->ss_strategy_nonread / ss->ss_strategy_bypassed) * 100);
fprintf(fp, "  noncached mount calls    %-10u  %3.0f%%\n", ss->ss_strategy_noncached_mount, ((float)ss->ss_strategy_noncached_mount / ss->ss_strategy_bypassed) * 100);
			if (ss->ss_strategy_nonblocksize > 0) {
fprintf(fp, "  nonblocksize calls       %-10u  %3.0f%%\n", ss->ss_strategy_nonblocksize, ((float)ss->ss_strategy_nonblocksize / ss->ss_strategy_bypassed) * 100);
			}
fprintf(fp, "  cache misses             %-10u  %3.0f%%\n", ss->ss_extent_lookups - ss->ss_extent_hits, ((float)(ss->ss_extent_lookups - ss->ss_extent_hits) / ss->ss_strategy_bypassed) * 100);
fprintf(fp, "  cache hit failures       %-10u  %3.0f%%\n", other, ((float)other / ss->ss_strategy_bypassed) * 100);
			if (ss->ss_strategy_bypass_duringio_unfilled > 0) {
fprintf(fp, "  unfilled extent          %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio_unfilled, ((float)ss->ss_strategy_bypass_duringio_unfilled / ss->ss_strategy_bypassed) * 100);
			}
			if (ss->ss_strategy_unfilled_lowpri > 0) {
fprintf(fp, "  unfilled lowpri extent   %-10u  %3.0f%%\n", ss->ss_strategy_unfilled_lowpri, ((float)ss->ss_strategy_unfilled_lowpri / ss->ss_strategy_bypassed) * 100);
			}
		}
fprintf(fp, " noncached calls           %-10u  %3.0f%%\n", ss->ss_strategy_bypass_nocache + ss->ss_strategy_hit_nocache, ((float)(ss->ss_strategy_bypass_nocache + ss->ss_strategy_hit_nocache) / ss->ss_strategy_calls) * 100);
		if (ss->ss_strategy_bypass_nocache + ss->ss_strategy_hit_nocache > 0) {
fprintf(fp, "  noncached calls filled   %-10u  %3.0f%%\n", ss->ss_strategy_hit_nocache, ((float)ss->ss_strategy_hit_nocache / (ss->ss_strategy_bypass_nocache + ss->ss_strategy_hit_nocache)) * 100);
fprintf(fp, "  noncached calls bypassed %-10u  %3.0f%%\n", ss->ss_strategy_bypass_nocache, ((float)ss->ss_strategy_bypass_nocache / (ss->ss_strategy_bypass_nocache + ss->ss_strategy_hit_nocache)) * 100);
		}
fprintf(fp, " throttled calls           %-10u  %3.0f%%\n", ss->ss_strategy_throttled, ((float)ss->ss_strategy_throttled / ss->ss_strategy_calls) * 100);
fprintf(fp, " calls during readahead    %-10u  %3.0f%%\n", ss->ss_strategy_duringio, ((float)ss->ss_strategy_duringio / ss->ss_strategy_calls) * 100);
		if (ss->ss_strategy_duringio > 0) {
fprintf(fp, "  filled during readahead  %-10u  %3.0f%%\n", ss->ss_strategy_duringio - ss->ss_strategy_bypass_duringio, ((float)(ss->ss_strategy_duringio - ss->ss_strategy_bypass_duringio) / ss->ss_strategy_duringio) * 100);
fprintf(fp, "  bypassed during readahea %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio, ((float)ss->ss_strategy_bypass_duringio / ss->ss_strategy_duringio) * 100);
			if (ss->ss_strategy_bypass_duringio > 0) {
				other = ss->ss_strategy_bypass_duringio - ss->ss_strategy_bypass_duringio_rootdisk_nonread - ss->ss_strategy_bypass_duringio_rootdisk_read - ss->ss_strategy_bypass_duringio_rootdisk_failure - ss->ss_strategy_bypass_duringio_unfilled - ss->ss_strategy_bypass_duringio_nocache;
				if (ss->ss_strategy_bypass_duringio_unfilled > 0) {
fprintf(fp, "   unfilled extent         %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio_unfilled, ((float)ss->ss_strategy_bypass_duringio_unfilled / ss->ss_strategy_bypass_duringio) * 100);
				}
				if (ss->ss_strategy_bypass_duringio_nocache > 0) {
fprintf(fp, "   noncached IO            %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio_nocache, ((float)ss->ss_strategy_bypass_duringio_nocache / ss->ss_strategy_bypass_duringio) * 100);
				}
fprintf(fp, "   root disk nonread       %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio_rootdisk_nonread, ((float)ss->ss_strategy_bypass_duringio_rootdisk_nonread / ss->ss_strategy_bypass_duringio) * 100);
fprintf(fp, "   root disk cache miss    %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio_rootdisk_read, ((float)ss->ss_strategy_bypass_duringio_rootdisk_read / ss->ss_strategy_bypass_duringio) * 100);
fprintf(fp, "   root disk hit failure   %-10u  %3.0f%%\n", ss->ss_strategy_bypass_duringio_rootdisk_failure, ((float)ss->ss_strategy_bypass_duringio_rootdisk_failure / ss->ss_strategy_bypass_duringio) * 100);
fprintf(fp, "   non-root                %-10u  %3.0f%%\n", other, ((float)other / ss->ss_strategy_bypass_duringio) * 100);
fprintf(fp, "   forced throttled        %-10u  %3.0f%%\n", ss->ss_strategy_forced_throttled, ((float)ss->ss_strategy_forced_throttled / ss->ss_strategy_bypass_duringio) * 100);
			}
fprintf(fp, "  extent hits during reada %-10u  %3.0f%%\n", ss->ss_hit_duringio, ((float)ss->ss_hit_duringio / ss->ss_strategy_duringio) * 100);
			if (ss->ss_hit_duringio > 0) {
fprintf(fp, "   blocked during readahea %-10u  %3.1f%%\n", ss->ss_strategy_blocked, ((float)ss->ss_strategy_blocked / ss->ss_hit_duringio) * 100);
fprintf(fp, "   block calls timed out   %-10u  %3.1f%%\n", ss->ss_strategy_timedout, ((float)ss->ss_strategy_timedout / ss->ss_hit_duringio) * 100);
fprintf(fp, "   longest blocked call    %ums\n", ss->ss_strategy_time_longest_blocked);
fprintf(fp, "   total blocked duration  %ums\n", ss->ss_strategy_time_blocked);
			}
		}
	}
	if (ss->ss_strategy_unknown > 0) {
fprintf(fp, "unknown strategy calls     %u\n", ss->ss_strategy_unknown);
fprintf(fp, "unknown strategy bytes     %u\n", ss->ss_strategy_unknown_bytes);
	}
	
	if (ss->ss_read_bytes - ss->ss_read_bytes_lowpri > 0) {
fprintf(fp, "\n");
fprintf(fp, "total readahead threads    %u\n", ss->ss_readahead_threads);
		for (d = 0; d < STAT_DISKMAX; d++) {
			disk_bytes = 0;
			disk_msecs = 0;
			
			for(b = 0; b < STAT_BATCHMAX; b++) {
				disk_bytes += ss->ss_batch_bytes[d][b];
				disk_msecs += ss->ss_batch_time[d][b];
			}
			if (0 == disk_bytes) continue; /* no reads for this disk */
			
			if (disk_msecs > 0) {
fprintf(fp, "Disk %d reader rate:        %ukB/s, %utps\n",
						d,
						(u_int)(((unsigned long long)disk_bytes * 1000) / (disk_msecs * 1024)),
						(ss->ss_disk_initiated_reads[d] * 1000) / disk_msecs);
			}
			
fprintf(fp, "Disk %d time                %d.%03ds\n",
					d, 
					disk_msecs / 1000, 
					disk_msecs % 1000);
fprintf(fp, "Disk %d bytes read:         %u\n", d, disk_bytes);
			
			for(b = 0; b < STAT_BATCHMAX; b++) {
				if (ss->ss_batch_time[d][b] > 0) {
fprintf(fp, "  batch %d time             %d.%03ds\n",
							b, 
							ss->ss_batch_time[d][b] / 1000, 
							ss->ss_batch_time[d][b] % 1000);
				}
			}
			
			for(b = 0; b < STAT_BATCHMAX; b++) {
				if (ss->ss_batch_bytes[d][b] > 0) {
fprintf(fp, "  batch %d bytes read:      %-10u", b, ss->ss_batch_bytes[d][b]);
					if (ss->ss_batch_late_bytes[d][b] > 0) {
fprintf(fp, " (%u late)", ss->ss_batch_late_bytes[d][b]);
					}
fprintf(fp, "\n");
				}
			}
		}
	}

	if (ss->ss_read_bytes_lowpri > 0) {
		for (d = 0; d < STAT_DISKMAX; d++) {
			if (0 == ss->ss_batch_bytes_lowpri[d]) continue; /* no reads for this disk */
			
			if (ss->ss_batch_time_lowpri[d] > 0) {
fprintf(fp, "Disk %d lowpri reader rate: %ukB/s, %u.%utps\n",
						d,
						(u_int)(((unsigned long long)ss->ss_batch_bytes_lowpri[d] * 1000) / (ss->ss_batch_time_lowpri[d] * 1024)),
						(ss->ss_disk_initiated_reads_lowpri[d] * 1000) / ss->ss_batch_time_lowpri[d], (10 * (ss->ss_disk_initiated_reads_lowpri[d] * 1000) / ss->ss_batch_time_lowpri[d]) % 10);
			}
			
fprintf(fp, "Disk %d lowpri bytes read:  %u\n", d, ss->ss_batch_bytes_lowpri[d]);
fprintf(fp, "Disk %d lowpri time:        %d.%03ds\n",
					d, 
					ss->ss_batch_time_lowpri[d] / 1000, 
					ss->ss_batch_time_lowpri[d] % 1000);
		}
	}
	
	/* extents */
fprintf(fp, "\n");
fprintf(fp, "mounts in cache            %u\n", ss->ss_total_mounts);
fprintf(fp, "extents in cache           %u\n", ss->ss_total_extents);
	if (ss->ss_extents_clipped > 0 && ss->ss_total_extents > 0) {
fprintf(fp, " extents clipped           %-10u  %3.0f%%\n", ss->ss_extents_clipped, ((float)ss->ss_extents_clipped / ss->ss_total_extents) * 100);
	}
fprintf(fp, "extent lookups             %u\n", ss->ss_extent_lookups);
	if (ss->ss_extent_lookups > 0) {
fprintf(fp, "extent hits                %-10u  %3.0f%%\n", ss->ss_extent_hits, ((float)ss->ss_extent_hits / ss->ss_extent_lookups) * 100);
		if (ss->ss_extent_hits > 0) {
fprintf(fp, " multiple extent hits      %-10u  %3.0f%%\n", ss->ss_hit_multiple, ((float)ss->ss_hit_multiple / ss->ss_extent_hits) * 100);
fprintf(fp, " hits aborted              %-10u  %3.0f%%\n", ss->ss_hit_aborted, ((float)ss->ss_hit_aborted / ss->ss_extent_hits) * 100);
fprintf(fp, " hits missing blocks       %-10u  %3.0f%%\n", ss->ss_hit_blkmissing, ((float)ss->ss_hit_blkmissing / ss->ss_extent_hits) * 100);
fprintf(fp, " hits stolen               %-10u  %3.0f%%\n", ss->ss_hit_stolen, ((float)ss->ss_hit_stolen / ss->ss_extent_hits) * 100);
fprintf(fp, " hits failed               %-10u  %3.0f%%\n", ss->ss_hit_failure, ((float)ss->ss_hit_failure / ss->ss_extent_hits) * 100);
		}
	}

	/* history */
fprintf(fp, "\n");
fprintf(fp, "preload time               %d.%03ds\n", ss->ss_preload_time / 1000, ss->ss_preload_time % 1000);
	if (ss->ss_history_time > 0 || ss->ss_history_entries == 0) {
fprintf(fp, "history active time        %d.%03ds\n", ss->ss_history_time / 1000, ss->ss_history_time % 1000);
		if (ss->ss_history_time > 0) {
fprintf(fp, "read/write IO rate         %u/%utps\n", (ss->ss_history_reads * 1000) / ss->ss_history_time, (ss->ss_history_writes * 1000) / ss->ss_history_time);
		}
	} else {
fprintf(fp, "history active time        (still active)\n");
	}
fprintf(fp, "history entries            %u\n", ss->ss_history_entries);
fprintf(fp, "history bytes              %u\n", ss->ss_history_bytes);
fprintf(fp, "history mounts             %u\n", ss->ss_history_mounts);
fprintf(fp, "unidentifiable mounts      %u\n", ss->ss_history_mount_no_uuid);
	if (ss->ss_history_unknown > 0) {
fprintf(fp, "history unknown calls      %u\n", ss->ss_history_unknown);
fprintf(fp, "history unknown bytes      %u\n", ss->ss_history_unknown_bytes);
	}
fprintf(fp, "number history recordings  %u\n", ss->ss_history_num_recordings);

	/* flags */
fprintf(fp, "\n");
fprintf(fp, "current flags              0x%x\n", ss->ss_cache_flags);
	
	if (fp == stdout) {
		fflush(fp);
	} else {
		fclose(fp);
	}
	return(0);
}

int
BC_print_history(char *fname, struct BC_history *hc)
{
	FILE *fp;
	int i;

	errno = 0;
	if (fname != NULL) {
		fp = fopen(fname, "w");
	} else {
		fp = stdout;
	}
	if (fp == NULL)
		return(errno);

	for (i = 0; i < hc->h_nentries; i++) {
		fprintf(fp, "%s %-12llu %-8llu %5u%s%s\n",
				uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_uuid),
				hc->h_entries[i].he_offset, hc->h_entries[i].he_length,
				hc->h_entries[i].he_pid,
				hc->h_entries[i].he_flags & BC_HE_HIT    ? " hit"    :
				hc->h_entries[i].he_flags & BC_HE_WRITE  ? " write"  :
				hc->h_entries[i].he_flags & BC_HE_TAG    ? " tag"    : " miss",
				hc->h_entries[i].he_flags & BC_HE_SHARED ? " shared" : "");
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
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		warnx("could not insert prefetch tag: %d %s", errno, strerror(errno));
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
