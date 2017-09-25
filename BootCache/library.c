/*
 * Common functions related to playlist handling and the management of
 * the BootCache module.
 */


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
#include <sys/ucred.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <mach/mach_time.h>
#include <dispatch/dispatch.h>
#include <libgen.h>
#include <os/log.h>
#include <libkern/OSAtomic.h>
#include <os/assumes.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <APFS/APFS.h>

#include "BootCache_private.h"

FILE* bc_log_stream = NULL;

#define LOGOSLOG(fmt, args...) os_log(OS_LOG_DEFAULT, "libBootCache: "fmt, ## args)
#define LOGPRINT(fmt, args...) fprintf(bc_log_stream, "libBootCache: "fmt"\n", ## args)

#ifdef DEBUG

#define LOG(fmt, args...) (bc_log_stream ? LOGPRINT("%32s:%-4d "fmt, __func__, __LINE__, ## args) : LOGOSLOG(fmt, ## args))
#define LOG_ERRNO(fmt, args...) (bc_log_stream ? LOGPRINT("%32s:%-4d "fmt": %d %s", __func__, __LINE__, ## args, errno, strerror(errno)) : LOGOSLOG(fmt": %{errno}d", ## args, errno))

#define DLOG(fmt, args...) LOG("Debug: "fmt, ## args)
#define DLOG_ERRNO(fmt, args...) LOG_ERRNO("Debug: "fmt, ## args)

#else

#define LOG(fmt, args...) (bc_log_stream ? LOGPRINT(fmt, ## args) : LOGOSLOG(fmt, ## args))
#define LOG_ERRNO(fmt, args...) (bc_log_stream ? LOGPRINT(fmt": %d %s", ## args, errno, strerror(errno)) : LOGOSLOG(fmt": %{errno}d", ## args, errno))

#define DLOG(fmt, args...)
#define DLOG_ERRNO(fmt, args...)

#endif

#define playlist_entry_format_str "%#-12llx %#-8llx %d 0x%x %#llx"
#define playlist_entry_format_args(pe) (pe)->pe_offset, (pe)->pe_length, (pe)->pe_batch, (pe)->pe_flags, (pe)->pe_crypto_offset

#define ASSERT(check) \
do { \
	if (__builtin_expect(!(check), 0)) { \
		LOG("Assertion failed: '%s'", # check); \
		if (bc_log_stream) fflush(bc_log_stream); \
		os_assert(check); \
	} \
} while (0)

static void clear_apfs_mount_cache(void);
static CFDictionaryRef get_apfs_mount_cache(void);
static const struct statfs* apfs_mount_for_uuid(uuid_t uuid);
static void get_volume_uuid(const char* volume, uuid_t uuid_out);
static int apfs_flags_for_volume(const char* bsddisk, uint* fs_flags_out, uuid_t* container_uuid_out);

static int BC_start_omap_recording_for_all_mounts(void);
static int BC_stop_omap_recording_for_all_mounts(void);
static int BC_fetch_omaps_for_all_mounts(struct BC_omap_history **);

static void BC_sort_playlist(struct BC_playlist *pc);
static int BC_coalesce_playlist(struct BC_playlist *pc);


/*
 * Return a user-readable string for a given uuid
 *
 * Returns a pointer to a static buffer, which is
 * racy, so this should only be used for debugging purposes
 */
static inline const char* uuid_string(const uuid_t uuid)
{
	static uuid_string_t string[4];
	static int index = 0;
	int i = OSAtomicIncrement32(&index) % 4;
	uuid_unparse(uuid, string[i]);
	return string[i];
}

void BC_free_playlist(struct BC_playlist *pc) {
	if (pc){
		if (pc->p_mounts)
			free(pc->p_mounts);
		if (pc->p_entries)
			free(pc->p_entries);
		if (pc->p_omaps)
			free(pc->p_omaps);
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

void BC_free_omap_history(struct BC_omap_history *oh) {
	if (oh) {
		if (oh->oh_mounts) {
			for (int i = 0; i < oh->oh_nmounts; i++) {
				if (oh->oh_mounts[i].ohm_omaps) {
					free(oh->oh_mounts[i].ohm_omaps);
				}
			}
			free(oh->oh_mounts);
		}
		free(oh);
	}
}

struct BC_playlist *
BC_allocate_playlist(uint nmounts, uint nentries, uint nomaps)
{
	struct BC_playlist *pc = NULL;

	pc = calloc(1, sizeof(*pc));
	if (!pc)
		goto out;


	if (nmounts > 0) {
		pc->p_mounts = calloc(nmounts, sizeof(*pc->p_mounts));
		if (pc->p_mounts == NULL)
			goto out;
		
		pc->p_nmounts = nmounts;
	}
	
	if (nentries > 0) {
		pc->p_entries = calloc(nentries, sizeof(*pc->p_entries));
		if (pc->p_entries == NULL)
			goto out;
		
		pc->p_nentries = nentries;
	}

	if (nomaps > 0) {
		pc->p_omaps = calloc(nomaps, sizeof(*pc->p_omaps));
		if (pc->p_omaps == NULL) {
			goto out;
		}

		pc->p_nomaps = nomaps;
	}

	return pc;

out:

	PC_FREE_ZERO(pc);
	return NULL;
}

void
BC_reset_playlist(struct BC_playlist *pc)
{
	if (pc == NULL) return;

	if (pc->p_mounts) {
		free(pc->p_mounts);
		pc->p_mounts = NULL;
	}
	pc->p_nmounts = 0;

	if (pc->p_entries) {
		free(pc->p_entries);
		pc->p_entries = NULL;
	}
	pc->p_nentries = 0;
	
	if (pc->p_omaps) {
		free(pc->p_omaps);
		pc->p_omaps = NULL;
	}
	pc->p_nomaps = 0;

}


struct BC_playlist *
BC_copy_playlist(const struct BC_playlist *pc)
{

	if (pc == NULL) return NULL;

	struct BC_playlist *npc = BC_allocate_playlist(pc->p_nmounts, pc->p_nentries, pc->p_nomaps);
	if (npc == NULL) return NULL;

	if (pc->p_nentries > 0)
		memcpy(npc->p_entries, pc->p_entries, sizeof(*pc->p_entries) * pc->p_nentries);

	if (pc->p_nmounts > 0)
		memcpy(npc->p_mounts, pc->p_mounts, sizeof(*pc->p_mounts) * pc->p_nmounts);

	if (pc->p_nomaps > 0)
		memcpy(npc->p_omaps, pc->p_omaps, sizeof(*pc->p_omaps) * pc->p_nomaps);

	return npc;
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
		LOG("No playlist path provided");
		error = EINVAL;
		goto out;
	}
	if ((fd = open(pfname, O_RDONLY)) == -1) {
		LOG_ERRNO("Unable to open %s", pfname);
		error = errno;
		goto out;
	}
	if (read(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		LOG_ERRNO("could not read header from %s", pfname);
		error = EINVAL;
		goto out;
	}
	if (ph.ph_magic != PH_MAGIC) {
		LOG("bad playlist magic in %s", pfname);
		error = EINVAL;
		goto out;
	}

	pc = BC_allocate_playlist(ph.ph_nmounts, ph.ph_nentries, ph.ph_nomaps);
	if (pc == NULL) {
		LOG("could not allocate playlist");
		error = ENOMEM;
		goto out;
	}

	if (read(fd, pc->p_mounts, sizeof(*pc->p_mounts) * pc->p_nmounts) != (sizeof(*pc->p_mounts) * pc->p_nmounts)) {
		LOG_ERRNO("could not read playlist mounts");
		error = EINVAL;
		goto out;
	}

	if (read(fd, pc->p_entries, (sizeof(*pc->p_entries) * pc->p_nentries)) != (sizeof(*pc->p_entries) * pc->p_nentries)) {
		LOG_ERRNO("could not read playlist data");
		error = EINVAL;
		goto out;
	}

	if (read(fd, pc->p_omaps, (sizeof(*pc->p_omaps) * pc->p_nomaps)) != (sizeof(*pc->p_omaps) * pc->p_nomaps)) {
		LOG_ERRNO("could not read omap data");
		error = EINVAL;
		goto out;
	}

	if ((error = BC_verify_playlist(pc)) != 0) {
		LOG("Failed to verify playlist");
		goto out;
	}

	*ppc = pc;
	pc = NULL;
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
		LOG("Failed to verify playlist");
		goto out;
	}

	/*
	 * Prepare the output file.
	 *
	 * Create a secure temporary file and write an invalid header.
	 */
	size_t tpathlen = strlen(pfname) + 8;
	if (tpathlen > MAXPATHLEN) {
		LOG("playlist filename too long");
		error = ENAMETOOLONG;
		goto out;
	}
	if ((tfname = malloc(tpathlen)) == NULL) {
		LOG("could not allocate %lu bytes for playlist filename", strlen(pfname));
		error = errno;
		goto out;
	}
	snprintf(tfname, tpathlen, "%s.XXXXXX", pfname);
	if ((fd = mkstemp(tfname)) < 0) {
		LOG("could not create temporary playlist file");
		error = errno;
		goto out;
	}
	ph.ph_magic    = 0;
	ph.ph_nmounts  = 0;
	ph.ph_nentries = 0;
	ph.ph_nomaps   = 0;
	if (write(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		LOG("could not write initial header to temporary playlist file");
		error = errno;
		goto out;
	}

	/*
	 * Write the playlist mounts.
	 */
	if (write(fd, pc->p_mounts, pc->p_nmounts * sizeof(*pc->p_mounts)) != (pc->p_nmounts * sizeof(*pc->p_mounts))) {
		LOG_ERRNO("could not write mounts to temporary playlist file");
		error = errno;
		goto out;
	}

	/*
	 * Write the playlist entries.
	 */
	if (write(fd, pc->p_entries, pc->p_nentries * sizeof(*pc->p_entries)) != (pc->p_nentries * sizeof(*pc->p_entries))) {
		LOG_ERRNO("could not write entries to temporary playlist file");
		error = errno;
		goto out;
	}

	/*
	 * Write the omap records.
	 */
	if (write(fd, pc->p_omaps, pc->p_nomaps * sizeof(*pc->p_omaps)) != pc->p_nomaps * sizeof(*pc->p_omaps)) {
		LOG_ERRNO("could not write omaps to temporary playlist file");
		error = errno;
		goto out;
	}

	/*
	 * Write an updated (valid) header to the playlist file.
	 */
	ph.ph_magic   = PH_MAGIC;
	ph.ph_nmounts  = pc->p_nmounts;
	ph.ph_nentries = pc->p_nentries;
	ph.ph_nomaps   = pc->p_nomaps;
	if (lseek(fd, 0, SEEK_SET) != 0) {
		LOG("could not seek on temporary playlist file");
		error = errno;
		goto out;
	}
	if (write(fd, &ph, sizeof(ph)) != sizeof(ph)) {
		LOG("could not write header to temporary playlist file");
		error = errno;
		goto out;
	}
	close(fd);
	fd = -1;

	/*
	 * Rename the temporary playlist file over the original.
	 */
	if (rename((const char *)tfname, pfname) != 0) {
		LOG("could not save playlist file %s->%s", tfname, pfname);
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
 * Sorts and coalesces the resulting playlist.  Robust in the case
 * where any list pointer is NULL or length is zero.
 */
int
BC_merge_playlists(struct BC_playlist* pa, const struct BC_playlist* pb)
{
	int mount_idx_b, mount_idx_a, entry_idx, omap_idx;
	if ((pa->p_mounts = reallocf(pa->p_mounts, sizeof(*pa->p_mounts) * (pa->p_nmounts + pb->p_nmounts))) == NULL)
		return(errno ?: ENOMEM);

	/* merge the mount list */
	for (mount_idx_b = 0; mount_idx_b < pb->p_nmounts; mount_idx_b++) {
		for (mount_idx_a = 0; mount_idx_a < pa->p_nmounts; mount_idx_a++) {
			if (0 == uuid_compare(pa->p_mounts[mount_idx_a].pm_uuid, pb->p_mounts[mount_idx_b].pm_uuid)) {
				break;
			}
		}

		if (mount_idx_a == pa->p_nmounts) {
			/* We're seeing a new mount we haven't seen before */
			pa->p_nmounts++;
			
			struct BC_playlist_mount *pm = &pa->p_mounts[mount_idx_a];
			struct BC_playlist_mount *new_mount = &pb->p_mounts[mount_idx_b];
			memcpy(pm, new_mount, sizeof(*pm));
		} else {
			
			// We found a matching mount in pa's existing list so we update the counts
			pa->p_mounts[mount_idx_a].pm_nentries += pb->p_mounts[mount_idx_b].pm_nentries;
			pa->p_mounts[mount_idx_a].pm_nomaps += pb->p_mounts[mount_idx_b].pm_nomaps;
		}
	}

	/* merge the entries list */
	if ((pa->p_entries = reallocf(pa->p_entries, sizeof(*pa->p_entries) * (pa->p_nentries + pb->p_nentries))) == NULL)
		return(errno ?: ENOMEM);
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
			LOG("Mounts not merged properly for entry %d (mount %d)", entry_idx, mount_idx_b);
			return(1);
		}
	}

	pa->p_nentries += pb->p_nentries;
	
	
	/* merge the omaps list */
	if ((pa->p_omaps = reallocf(pa->p_omaps, (pa->p_nomaps + pb->p_nomaps) * sizeof(*pa->p_omaps))) == NULL)
		return(errno ?: ENOMEM);
	memcpy(pa->p_omaps + pa->p_nomaps, pb->p_omaps, pb->p_nomaps * sizeof(*pb->p_omaps));
	
	/* fixup mount indices for the new omaps */
	for (omap_idx = pa->p_nomaps; omap_idx < (pa->p_nomaps + pb->p_nomaps); omap_idx++) {
		mount_idx_b = pa->p_omaps[omap_idx].po_mount_idx;
		for (mount_idx_a = 0; mount_idx_a < pa->p_nmounts; mount_idx_a++) {
			if (0 == uuid_compare(pa->p_mounts[mount_idx_a].pm_uuid, pb->p_mounts[mount_idx_b].pm_uuid)) {
				pa->p_omaps[omap_idx].po_mount_idx = mount_idx_a;
				break;
			}
		}
		if (mount_idx_a == pa->p_nmounts) {
			LOG("Mounts not merged properly for omap %d (mount %d)", omap_idx, mount_idx_b);
			return(1);
		}
	}
	
	pa->p_nomaps += pb->p_nomaps;


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

static void
BC_sort_playlist(struct BC_playlist *pc)
{
	if ((pc == NULL) || (pc->p_mounts == NULL) || (pc->p_entries == NULL) || (pc->p_nmounts == 0) || (pc->p_nentries == 0))
		return;
	
	if (pc->p_nentries > 0)
		qsort((void *)pc->p_entries, pc->p_nentries, sizeof(*pc->p_entries), compare_playlist_entry);
}


#pragma mark -
#pragma mark Coalescing
// Ways we may coalesce two entries, in increasing order of priority (so we don't end up with out-of-order entries)
typedef enum {
	BC_COALESCE_NOT_ADJACENT = 0,// Null case. Entries are not adjacent
	BC_COALESCE_NO_CHANGE,       // No merge and no size changes necessary, though the entries are adjacent
	BC_COALESCE_HIGHER_SHRINKS,  // Higher shrinks to not overlap lower
	BC_COALESCE_LOWER_SPLIT,     // Lower encompasses higher and will split into two entries to not overlap higher
	BC_COALESCE_LOWER_SHRINKS,   // Lower will shrink to not overlap higher
	BC_COALESCE_MERGE,           // Merge into one entry
	BC_COALESCE_LOWER_REMOVE,    // Lower to be removed and higher unmodified
	BC_COALESCE_HIGHER_REMOVE,   // Higher to be removed and higher unmodified
} coalesce_style_t;

#ifdef DEBUG
#define COALESCING_DEBUG_LOGGING_ENABLED 0
#else
#define COALESCING_DEBUG_LOGGING_ENABLED 0
#endif

#if COALESCING_DEBUG_LOGGING_ENABLED
#define DLOG_COALESCING(fmt, args...) LOG("Coalescing: "fmt, ## args)
#else
#define DLOG_COALESCING(fmt, args...)
#endif

// Given two adjacent/overlapping entries, how will they be coalesced?
static coalesce_style_t BC_coalesce_style(const struct BC_playlist_entry* lower_pe, const struct BC_playlist_entry* higher_pe, bool is_encrypted) {
	
	ASSERT(lower_pe->pe_offset <= higher_pe->pe_offset);
	ASSERT(lower_pe->pe_mount_idx == higher_pe->pe_mount_idx);
	
	if (lower_pe->pe_offset + lower_pe->pe_length < higher_pe->pe_offset) {
		DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_NOT_ADJACENT", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
		return BC_COALESCE_NOT_ADJACENT;
	}
	
	if (lower_pe->pe_length == 0 || higher_pe->pe_length == 0) {
		DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_NO_CHANGE", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
		return BC_COALESCE_NO_CHANGE;
	}

	if (is_encrypted) {
		
		/* if encrypted entries do not have the same delta between their crypto offset and their disk offset, then they cannot be combined */
		if ((higher_pe->pe_offset - lower_pe->pe_offset) != (higher_pe->pe_crypto_offset - lower_pe->pe_crypto_offset)) {
			// Crypto mismatch
			
			if (lower_pe->pe_offset + lower_pe->pe_length > higher_pe->pe_offset) {
				// Overlap with crypto mismatch
				
				// If we're going to throw out this entry due to a crypto mismatch, don't first merge it with a bunch of other entries that may not overlap the crypto mismatch one
				static_assert(BC_COALESCE_LOWER_REMOVE > BC_COALESCE_MERGE, "");
				
				// One of these entries is wrong because there cannot be a block with two different crypto offsets
				//
				// The only case where this situation should come up is if the BootCache generated a playlist from a file, and the recording from the previous boot also included I/O to that file, and that file has crypto offset != dev offset.
				// The BootCache when generating a playlist for a file, assumes offset == crypto_offset and fills in the crypto offsets in the playlist accordingly.
				// That assumption isn't necessarily true, though: entries of files that existed before upgrading to apfs (and were created on volume that had CoreStorage created before HFS was laid down) will have the crypto offset != dev offset, see rdar://33489229.
				// So, just remove the entry where crypto offset == offset, because that came from the (incorrect) file-generated playlist, and the one with crypto offset != dev offset is correct because that's what was recorded by the kext.
				
				if (lower_pe->pe_offset == lower_pe->pe_crypto_offset) {
					DLOG("Entry "playlist_entry_format_str" mismatch with "playlist_entry_format_str, playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
					DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_LOWER_REMOVE", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
					return BC_COALESCE_LOWER_REMOVE;
				} else if (higher_pe->pe_offset == higher_pe->pe_crypto_offset) {
					DLOG("Entry "playlist_entry_format_str" mismatch with "playlist_entry_format_str, playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
					DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_HIGHER_REMOVE", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
					return BC_COALESCE_HIGHER_REMOVE;
				} else {
					// Both entries have crypto offset != offset, but the deltas don't match?
					// That shouldn't happen... remove lower_pe, I guess, so in the case where it actually is wrong it doesn't knock out a bunch of correct higher extents.
					LOG("Entry "playlist_entry_format_str" mismatch with "playlist_entry_format_str, playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
					
					// Because we're unsure which entry is wrong but return BC_COALESCE_LOWER_REMOVE here, BC_COALESCE_LOWER_REMOVE doesn't necessarily mean lower_pe is actually wrong. We are sure that when we return BC_COALESCE_HIGHER_REMOVE that the higher_pe is wrong, so make sure we prefer to do BC_COALESCE_HIGHER_REMOVE before BC_COALESCE_LOWER_REMOVE
					static_assert(BC_COALESCE_HIGHER_REMOVE > BC_COALESCE_LOWER_REMOVE, "");
					
					DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_LOWER_REMOVE", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
					return BC_COALESCE_LOWER_REMOVE;
				}
			} else {
				// Adjacent. Not a problem, but they cannot be combined
				DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_NO_CHANGE due to adjacent nonmatching crypto", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
				return BC_COALESCE_NO_CHANGE;
			}
		} else {
			// Crypto match, go ahead and fall-through to coalesce like normal
		}
	}
	
#define MAX_HIGHPRI_ENTRY_GROWTH_DUE_TO_LOWPRI_COALESCING (128*1024) // 128KB
	
	// Length we would be if we merged
	uint64_t coalesced_length = MAX((lower_pe->pe_offset + lower_pe->pe_length), (higher_pe->pe_offset + higher_pe->pe_length)) - lower_pe->pe_offset;
	
	// Check for cases where we want to merge the entries together
	if ((lower_pe->pe_flags & BC_PE_LOWPRIORITY) && (higher_pe->pe_flags & BC_PE_LOWPRIORITY)) {
		// Both low priority
		DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_MERGE both low priority", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
		return BC_COALESCE_MERGE;
	} else if (!(lower_pe->pe_flags & BC_PE_LOWPRIORITY) && !(higher_pe->pe_flags & BC_PE_LOWPRIORITY)) {
		// Both high prioirty
		DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_MERGE both high priority", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
		return BC_COALESCE_MERGE;
	} else if (lower_pe->pe_flags & BC_PE_LOWPRIORITY) {
		// lower_pe is low priority, higher_pe is high priority
		
		if (coalesced_length - higher_pe->pe_length < MAX_HIGHPRI_ENTRY_GROWTH_DUE_TO_LOWPRI_COALESCING) {
			// Adding lower_pe to higher_pe will increase higher's size by less than the max growth size
			DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_MERGE higher high priority", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
			return BC_COALESCE_MERGE;
		} else {
			// Adding lower_pe (low priority) to higher_pe (high priority) will increase higher's size by more than the max growth size, don't coalesce them
		}
	} else {
		// lower_pe is high priority, higher_pe is low priority
		
		if (coalesced_length - lower_pe->pe_length < MAX_HIGHPRI_ENTRY_GROWTH_DUE_TO_LOWPRI_COALESCING) {
			// Adding higher_pe to lower_pe will increase lower's size by less than the max growth size
			DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_MERGE lower high priority", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
			return BC_COALESCE_MERGE;
		} else {
			// Adding higher_pe (low priority) to lower_pe (high priority) will increase lower's size by more than the max growth size, don't coalesce them
		}
	}

	// If we've gotten here, then we're not merging. Check for overlap and decide how to deal with it
	// Further, because of the merging logic above, we know either lower_pe or higher_pe is low priority, but not both
	
	if (lower_pe->pe_offset + lower_pe->pe_length == higher_pe->pe_offset) {
		// No overlap
		DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_NO_CHANGE adjacent, not overlapping", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
		return BC_COALESCE_NO_CHANGE;
	}
	
	if (higher_pe->pe_flags & BC_PE_LOWPRIORITY) {
		/* lower_pe is high priority and higher_pe is low priority, subtract intersection from higher_pe (because of ordering by offset, we know higher_pe cannot encompass lower_pe thus we don't need to split higher_pe into two entries) */
		DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_HIGHER_SHRINKS", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
		return BC_COALESCE_HIGHER_SHRINKS;
	} else {
		/* lower_pe is low priority, higher_pe is high priority, subtract intersection from lower_pe */
		if ((lower_pe->pe_offset + lower_pe->pe_length) > (higher_pe->pe_offset + higher_pe->pe_length)) {
			/* lower_pe encompasses higher_pe. In order to subtract the intersection we need to split lower_pe into two entries: one below higher_pe and one below higher_pe */
			DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_LOWER_SPLIT", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
			return BC_COALESCE_LOWER_SPLIT;
		} else {
			DLOG_COALESCING(playlist_entry_format_str" vs "playlist_entry_format_str" -> BC_COALESCE_LOWER_SHRINKS", playlist_entry_format_args(lower_pe), playlist_entry_format_args(higher_pe));
			return BC_COALESCE_LOWER_SHRINKS;
		}
	}

}


/*
 * Coalesece a sorted playlist into the smallest set of contiguous
 * entries.  Sets the new size of the playlist and realloc's the buffer.
 */
static int
BC_coalesce_playlist(struct BC_playlist *pc)
{
	/*
	 * Scan the sorted list and emit coalesced playlist entries.
	 *
	 * As we're scanning and coalescing pc->p_entries, we're saving the coalesced entries at the start of pc->p_entries
	 *
	 * C: already-coalesced entries
	 * D: The index of the next entry after the already-coalesced entries (dest_idx)
	 * -: Unused entries we've already coalesced into the C entries
	 * S: Entry we're currently considering for coalescing (scan_idx)
	 * *: Entries we've already coalesced with S (potentially non-0 length, but do not overlap S)
	 * B: Entry we're currently looking at coalescing with S (bestmatch_idx)
	 * |: Entries that are adjacent/overlapping S, but have not yet been coalesced
	 * +: Entries not yet coalesced and not overlapping S
	 *
	 *            --> higher offset -->
	 * CCCCCCCCCCD--------------S**|||*B*||*|*+++++++++
	 *
	 * In each turn of the scan_idx loop below, we coalesce S with all overlapping/adjacent entry (the entry immediately after it), then move S to D.
	 *
	 * In each turn of the do-while loop inside the scan_idx loop, we pick an adjacent/overlapping entry, B, and coalece it with S. Afterwards, S and B must not overlap and the order of the entire list must be preserved. To keep the ordering, all adjacent/overlapping entries that won't change their offset and will change S's length when coalesced with S, are picked before any that will change their offset based on S's length (this is what the bestmatch loop does).
	 */
	
	// There are several possibilities when coalescing two entries (see coalesce_style_t).
	// We want to make sure we don't break the invariant that all (non-0 length) entries are in sorted order.
	// No methods change scan_pe's offset, but BC_COALESCE_HIGHER_SHRINKS may affect the lookahead_pe's offset.
	// So, we want to make sure we do any BC_COALESCE_HIGHER_SHRINKS-style coalescing after anything else that may change the lower's endpoint. That way, all entries that overlap with scan_pe left after the other coalescing styles will be changed to have the same offset, thus ordering is preserved
	static_assert(BC_COALESCE_HIGHER_SHRINKS < BC_COALESCE_LOWER_SPLIT, "");
	static_assert(BC_COALESCE_HIGHER_SHRINKS < BC_COALESCE_LOWER_SHRINKS, "");
	static_assert(BC_COALESCE_HIGHER_SHRINKS < BC_COALESCE_MERGE, "");
	
#ifdef DEBUG
	uint nentries_original = pc->p_nentries;
#endif
	
#if COALESCING_DEBUG_LOGGING_ENABLED
//	DLOG_COALESCING("Coalescing %d entries", pc->p_nentries);
//    BC_print_playlist(pc);
#endif

	/* clear the number of entries per mount, to be recalculated as we're coalescing */
	for (int mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
		pc->p_mounts[mount_idx].pm_nentries = 0;
	}
	
	int dest_idx = 0;
	for (int scan_idx = 0; scan_idx < pc->p_nentries; scan_idx++) {
		struct BC_playlist_entry* scan_pe = pc->p_entries + scan_idx;
		
		if (scan_pe->pe_length <= 0) { /* skip entries we've marked as empty */
			continue;
		}

#if COALESCING_DEBUG_LOGGING_ENABLED
		bool did_anything = false;
#endif

		bool is_encrypted = (pc->p_mounts[scan_pe->pe_mount_idx].pm_fs_flags & BC_FS_ENCRYPTED);
		
		coalesce_style_t bestmatch_style; // Needs to be defined outside the while loop in order to be checked by the while condition
		do {
			
			// Look ahead at all adjacent/overlapping entries and find the one we want to coalesce first
			int bestmatch_idx = -1;
			bestmatch_style = BC_COALESCE_NOT_ADJACENT; // NULL case
			int lookahead_idx;
			for (lookahead_idx = scan_idx + 1; lookahead_idx < pc->p_nentries; lookahead_idx++) {
				struct BC_playlist_entry* lookahead_pe = pc->p_entries + lookahead_idx;
				
				if (lookahead_pe->pe_length <= 0) {
					/* skip entries we've marked as empty */
					continue;
				}

				if (scan_pe->pe_mount_idx != lookahead_pe->pe_mount_idx) {
					/* not the same mount, can't be coalesced */
					/* Further, since we sort by mount and then offset we know nothing past lookahead_pe will match by mount either, so stop looking ahead */
					break;
				}
				
				if (lookahead_pe->pe_offset > (scan_pe->pe_offset + scan_pe->pe_length)) {
					/* scan_pe is not overlapping nor adjacent to lookahead_pe */
					/* Further, since we sort by offset we know nothing past lookahead_pe will intersect scan_pe either, so stop looking ahead */
					break;
				}
				
				coalesce_style_t style = BC_coalesce_style(scan_pe, lookahead_pe, is_encrypted);
				
				if (style > bestmatch_style) {
					bestmatch_style = style;
					bestmatch_idx = lookahead_idx;
				}
			}
			
			struct BC_playlist_entry* bestmatch_pe = NULL;
			if (bestmatch_idx != -1) {
				bestmatch_pe = pc->p_entries + bestmatch_idx;
#if COALESCING_DEBUG_LOGGING_ENABLED
				did_anything = true;
#endif
			} else {
				ASSERT(bestmatch_style == BC_COALESCE_NOT_ADJACENT);
			}
			
			switch (bestmatch_style) {
					
				case BC_COALESCE_HIGHER_REMOVE:
				{
					DLOG_COALESCING("BC_COALESCE_HIGHER_REMOVE: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

					bestmatch_pe->pe_length = 0;
					
					DLOG_COALESCING("                        -> %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

				}
					break;
					

				case BC_COALESCE_LOWER_REMOVE:
				{
					DLOG_COALESCING("BC_COALESCE_LOWER_REMOVE: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

					scan_pe->pe_length = 0;
					
					DLOG_COALESCING("                       -> %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));
				}
					break;
					

				case BC_COALESCE_MERGE:
				{
					/* coalesce the two entries together */
					
					DLOG_COALESCING("BC_COALESCE_MERGE: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

					// scan_pe->pe_offset == coalesced_offset already, and thus pe_crypto_offset doesn't need to be changed since offset didn't change
					scan_pe->pe_length = MAX((scan_pe->pe_offset + scan_pe->pe_length), (bestmatch_pe->pe_offset + bestmatch_pe->pe_length)) - scan_pe->pe_offset;
					
					if ((scan_pe->pe_flags & BC_PE_LOWPRIORITY) && (bestmatch_pe->pe_flags & BC_PE_LOWPRIORITY)) {
						// Both already low priority, no need to change batch
					} else if (scan_pe->pe_flags & BC_PE_LOWPRIORITY) {
						// bestmatch_pe is high priority, scan_pe is low priority. Set to high priority and use bestmatch_pe's batch
						scan_pe->pe_batch = bestmatch_pe->pe_batch;
						scan_pe->pe_flags &= (~BC_PE_LOWPRIORITY);
					} else if (bestmatch_pe->pe_flags & BC_PE_LOWPRIORITY) {
						// scan_pe is high priority, bestmatch_pe is low priority. Keep scan_pe's batch
					} else {
						// Both high priority, use lower batch of the two
						scan_pe->pe_batch = MIN(scan_pe->pe_batch, bestmatch_pe->pe_batch);
					}
					
					if (bestmatch_pe->pe_flags & BC_PE_SHARED) {
						/* If either scan_pe or bestmatch_pe are shared, the coalesced entry is shared */
						scan_pe->pe_flags |= BC_PE_SHARED;
					}

					bestmatch_pe->pe_length = 0; /* mark the now-defunct entry as empty */
					
					DLOG_COALESCING("                -> %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));
				}
					break;
					
					
				case BC_COALESCE_LOWER_SHRINKS:
				{
					/* Shorten scan_pe to the range below bestmatch_pe */

					DLOG_COALESCING("BC_COALESCE_LOWER_SHRINKS: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

					scan_pe->pe_length = bestmatch_pe->pe_offset - scan_pe->pe_offset;
					
					DLOG_COALESCING("                        -> %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

				}
					break;
					
					
				case BC_COALESCE_LOWER_SPLIT:
				{
					/* scan_pe encompasses bestmatch_pe, so we need to split it into two halves: scan_pe will be the range below bestmatch_pe and we need a new entry for the range above bestmatch_pe */

					DLOG_COALESCING("BC_COALESCE_LOWER_SPLIT: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

					ASSERT((scan_pe->pe_offset + scan_pe->pe_length) > (bestmatch_pe->pe_offset + bestmatch_pe->pe_length));
					
					struct BC_playlist_entry new_entry = *scan_pe; // Start with a copy of scan_pe for mount index, flags, batch
					new_entry.pe_offset = (bestmatch_pe->pe_offset + bestmatch_pe->pe_length);
					new_entry.pe_length = (scan_pe->pe_offset + scan_pe->pe_length) - new_entry.pe_offset;
					if (is_encrypted) {
						new_entry.pe_crypto_offset = new_entry.pe_offset + scan_pe->pe_crypto_offset - scan_pe->pe_offset;
					}
					
					// Find the place in our entries list where we need to insert this new entry
					int new_entry_idx;
					for (new_entry_idx = bestmatch_idx + 1; new_entry_idx < pc->p_nentries; new_entry_idx++) {
						
						int compar = compare_playlist_entry(&new_entry, pc->p_entries + new_entry_idx);
						DLOG_COALESCING("compar %d: "playlist_entry_format_str" vs %d@"playlist_entry_format_str, compar, playlist_entry_format_args(&new_entry), new_entry_idx, playlist_entry_format_args(pc->p_entries + new_entry_idx));
						if (compar < 0) {
							break;
						}
					}
					// new_entry should be inserted at pc->p_entries + new_entry_idx
					
					// Grow p_entries to accomodate the new entry
					pc->p_nentries++;
					pc->p_entries = reallocf(pc->p_entries, sizeof(*pc->p_entries) * pc->p_nentries);
					if (pc->p_entries == NULL) {
						LOG_ERRNO("Unable to grow entry list for split entry");
						return errno ?: ENOMEM;
					}
					
					// Fix up all pointers into that realloc'ed buffer
					scan_pe = pc->p_entries + scan_idx;
					bestmatch_pe = pc->p_entries + bestmatch_idx;
					
					if (new_entry_idx < pc->p_nentries - 1) {
						// Move higher entries up one in the array to make room for the new entry
						memmove(pc->p_entries + new_entry_idx + 1, pc->p_entries + new_entry_idx, (pc->p_nentries - 1 - new_entry_idx) * sizeof(*pc->p_entries));
					}
					
					// Insert new_entry at new_entry_idx
					*(pc->p_entries + new_entry_idx) = new_entry;
					
					
					/* Shorten scan_pe to the range below bestmatch_pe */
					scan_pe->pe_length = bestmatch_pe->pe_offset - scan_pe->pe_offset;
					
					DLOG_COALESCING("                      -> %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str", nentries now %d", scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe), new_entry_idx, playlist_entry_format_args(pc->p_entries + new_entry_idx), pc->p_nentries);

				}
					break;
					
					
				case BC_COALESCE_HIGHER_SHRINKS:
				{
					DLOG_COALESCING("BC_COALESCE_HIGHER_SHRINKS: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));

					if ((scan_pe->pe_offset + scan_pe->pe_length) > (bestmatch_pe->pe_offset + bestmatch_pe->pe_length)) {
						// bestmatch entry is encompassed by scan_pe, so it shrinks to nothing
						bestmatch_pe->pe_length = 0;
					} else {
						
						uint64_t end = (bestmatch_pe->pe_offset + bestmatch_pe->pe_length);
						
						bestmatch_pe->pe_offset = scan_pe->pe_offset + scan_pe->pe_length;
						bestmatch_pe->pe_length = end - bestmatch_pe->pe_offset;
						if (is_encrypted) {
							bestmatch_pe->pe_crypto_offset = scan_pe->pe_crypto_offset + scan_pe->pe_length;
						}
					}
					
					DLOG_COALESCING("                         -> %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));
				}
					break;
					
				case BC_COALESCE_NO_CHANGE:
				{
					// Nothing to do

					DLOG_COALESCING("BC_COALESCE_NO_CHANGE: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str, scan_idx, playlist_entry_format_args(scan_pe), bestmatch_idx, playlist_entry_format_args(bestmatch_pe));
				}
					break;
					
				case BC_COALESCE_NOT_ADJACENT:
				{
					// Nothing to do

#if COALESCING_DEBUG_LOGGING_ENABLED
					if (did_anything) {
						if (pc->p_nentries > lookahead_idx) {
							DLOG_COALESCING("BC_COALESCE_NOT_ADJACENT: %d@"playlist_entry_format_str" + %d@"playlist_entry_format_str" nentries is %d", scan_idx, playlist_entry_format_args(scan_pe), lookahead_idx, playlist_entry_format_args(pc->p_entries + lookahead_idx), pc->p_nentries);
						} else {
							DLOG_COALESCING("BC_COALESCE_NOT_ADJACENT: %d@"playlist_entry_format_str" at end", scan_idx, playlist_entry_format_args(scan_pe));
						}
					}
#endif
				}
					break;
			}
			
			// Make sure we keep our invariant that scan_pe doesn't overlap bestmatch_pe once we're done coalescing them
			ASSERT(bestmatch_pe == NULL || scan_pe->pe_length == 0 || bestmatch_pe->pe_length == 0 || scan_pe->pe_offset + scan_pe->pe_length <= bestmatch_pe->pe_offset);
			
			// After each bestmatch coalesced, we need to loop back and re-scan all adjacent/overlapping entries because the coalescing may have changed the style of coalescing we will use for those entries (assuming there is still work to do)
		} while (bestmatch_style != BC_COALESCE_NOT_ADJACENT &&
				 bestmatch_style != BC_COALESCE_NO_CHANGE &&
				 scan_pe->pe_length > 0);
		
		if (scan_pe->pe_length > 0) {
			/* save entry */
			
			if (dest_idx != scan_idx) {
				*(pc->p_entries + dest_idx) = *scan_pe;
			}
			
#if COALESCING_DEBUG_LOGGING_ENABLED
			if (did_anything) {
				DLOG_COALESCING("Saved %d@"playlist_entry_format_str, dest_idx, playlist_entry_format_args(pc->p_entries + dest_idx));
			}
#endif
			
			dest_idx++;
			pc->p_mounts[scan_pe->pe_mount_idx].pm_nentries++;
		}
	}
	
	// Set new size based on how many entries we ended up with
	pc->p_nentries = dest_idx;
	
#if COALESCING_DEBUG_LOGGING_ENABLED
//DLOG_COALESCING("Coalesced to %d entries", pc->p_nentries);
//BC_print_playlist(pc);
#endif
	
#ifdef DEBUG
	if (pc->p_nentries != nentries_original) {
		DLOG("Coalesced %d entries into %d entries", nentries_original, pc->p_nentries);
	}
#endif
	
	/*
	 * Shrink the alloction if possible.  If realloc fails, handle it
	 * gracefully.
	 */
	if (pc->p_nentries > 0) {
		struct BC_playlist_entry* old_entries = pc->p_entries;
		pc->p_entries = realloc(pc->p_entries, sizeof(*pc->p_entries) * pc->p_nentries);
		if (pc->p_entries == NULL)
			pc->p_entries = old_entries;
	} else {
		if (pc->p_entries) {
			free(pc->p_entries);
			pc->p_entries = NULL;
		}
	}
	return(0);
}

#pragma mark -

/*
 * Verify the consistency of a playlist
 *
 * Returns 0 if the playlist is consistent, non-0 otherwise
 */
int
BC_verify_playlist(const struct BC_playlist *pc)
{
	int i, error = 0;
	int* mount_entry_counts = NULL;
	int* mount_entry_last_idx = NULL;
	int* mount_omap_counts = NULL;
	
	if (pc == NULL) {
		LOG("playlist is null");
		error = EINVAL;
		goto out;
	}
	
	
	if (pc->p_nmounts >= INT_MAX) {
		LOG("Number of mounts underflow");
		error = EINVAL;
		goto out;
	}
	if ((pc->p_mounts == NULL) && (pc->p_nmounts > 0)) {
		LOG("mounts is null");
		error = EINVAL;
		goto out;
	}
	
	
	if (pc->p_nentries >= INT_MAX) {
		LOG("Number of entries underflow");
		error = EINVAL;
		goto out;
	}
	if ((pc->p_entries == NULL) && (pc->p_nentries > 0)) {
		LOG("entries is null");
		error = EINVAL;
		goto out;
	}
	if (pc->p_nentries > 0 && pc->p_nmounts <= 0) {
		LOG("entries, but no mounts");
		error = EINVAL;
		goto out;
	}
	
	
	if (pc->p_nomaps >= INT_MAX) {
		LOG("Number of omaps underflow");
		error = EINVAL;
		goto out;
	}
	if ((pc->p_omaps == NULL) && (pc->p_nomaps > 0)) {
		LOG("omaps is null");
		error = EINVAL;
		goto out;
	}
	if (pc->p_nomaps > 0 && pc->p_nmounts <= 0) {
		LOG("omaps, but no mounts");
		error = EINVAL;
		goto out;
	}

	
	/* A playlist with nothing in it is technically valid */
	if (pc->p_nmounts == 0) {
		error = 0;
		goto out;
	}
	
	mount_entry_counts = calloc(pc->p_nmounts, sizeof(int));
	mount_entry_last_idx = calloc(pc->p_nmounts, sizeof(int));
	for (i = 0; i < pc->p_nmounts; i++) {
		mount_entry_last_idx[i] = -1;
	}
	for (i = 0; i < pc->p_nentries; i++) {
		if (pc->p_entries[i].pe_mount_idx >= pc->p_nmounts) {
			LOG("entry %d referenced unknown mount %d", i, pc->p_entries[i].pe_mount_idx);
			error = EINVAL;
			goto out;
		}
		
		if (pc->p_entries[i].pe_length == 0) {
			LOG("entry %d has 0 length %#llx,%#llx", i, pc->p_entries[i].pe_offset, pc->p_entries[i].pe_length);
			error = EINVAL;
			goto out;
		}
		
		if (mount_entry_last_idx[pc->p_entries[i].pe_mount_idx] >= 0 &&
			mount_entry_last_idx[pc->p_entries[i].pe_mount_idx] != i - 1) {
			
			LOG("entry %d referenced mount %d, but last entry to reference that mount was entry %d", i, pc->p_entries[i].pe_mount_idx, mount_entry_last_idx[pc->p_entries[i].pe_mount_idx]);
			error = EINVAL;
			goto out;
		}
		mount_entry_last_idx[pc->p_entries[i].pe_mount_idx] = i;
		
		mount_entry_counts[pc->p_entries[i].pe_mount_idx]++;
		
		if (pc->p_entries[i].pe_offset + pc->p_entries[i].pe_length <= pc->p_entries[i].pe_offset) {
			LOG("entry %d has a bad range %#llx,%#llx", i, pc->p_entries[i].pe_offset, pc->p_entries[i].pe_length);
			error = EINVAL;
			goto out;
		}
		
		// pe_crypto_offset is garbage if the volume is not encrypted, and any value is valid if it is encrypted, so can't check it
	}
	
	mount_omap_counts = calloc(pc->p_nmounts, sizeof(int));
	for (i = 0; i < pc->p_nomaps; i++) {
		if (pc->p_omaps[i].po_mount_idx >= pc->p_nmounts) {
			LOG("omap %d referenced unknown mount %d", i, pc->p_omaps[i].po_mount_idx );
			error = EINVAL;
			goto out;
		}
		
		mount_omap_counts[pc->p_omaps[i].po_mount_idx]++;
	}
	
	// Verify counts
	for (i = 0; i < pc->p_nmounts; i++) {
		if (pc->p_mounts[i].pm_nentries == 0 && pc->p_mounts[i].pm_nomaps == 0) {
			LOG("mount %d has 0 entries and 0 omaps", i);
			error = EINVAL;
			goto out;
		}
		
		if (mount_entry_counts[i] != pc->p_mounts[i].pm_nentries) {
			LOG("%d entries exist for mount %d, vs %d claimed", mount_entry_counts[i], i, pc->p_mounts[i].pm_nentries);
			error = EINVAL;
			goto out;
		}
		
		if (mount_omap_counts[i] != pc->p_mounts[i].pm_nomaps) {
			LOG("%d omaps exist for mount %d, vs %d claimed", mount_omap_counts[i], i, pc->p_mounts[i].pm_nomaps);
			error = EINVAL;
			goto out;
		}

	}

out:
	if (mount_entry_counts)
		free(mount_entry_counts);
	if (mount_omap_counts)
		free(mount_omap_counts);
	if (mount_entry_last_idx)
		free(mount_entry_last_idx);
	
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
 * Set cache userspace statistics.
 */
int
BC_set_userspace_statistics(const struct BC_userspace_statistics *statistics)
{
	struct BC_command bc;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_SET_USER_STATS;
	bc.bc_data1 = (uintptr_t) statistics;
	bc.bc_data1_size = (unsigned int) sizeof(*statistics);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	return(0);
}

/*
 * Lookup the extents for the givem OMAP records and add them to the entries in this playlist
 *
 * Returns non-0 on failure when the playlist is not usable
 * Returns 0 on success, and the replaylist is usable. An error will be logged
 *
 */
static int
BC_playlist_lookup_omaps(struct BC_playlist *pc)
{
	int ret = 0;

	if (pc == NULL) return 0;
	
	DLOG("Input playlist with %d mounts, %d entries, %d omaps",
		 pc->p_nmounts, pc->p_nentries, pc->p_nomaps);
	
	if ((ret = BC_verify_playlist(pc)) != 0) {
		LOG("Playlist with omaps failed to verify");
		return ret;
	}
	
	
	/*
	 * Iterate over all mounts.
	 * For each mount:
	 *
	 *	   Coalesce the omaps for each mount.
	 *	   Create a BC_playlist for each mount by converting the omaps to actual BC_playlist_entries using the fsctl.
	 *	   Merge it with the entries
	 *
	 */
	for (int mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
		int ol_record_idx = 0;
		struct BC_playlist *omap_pc = NULL;

		int num_omaps_in_mount = pc->p_mounts[mount_idx].pm_nomaps;
		if (num_omaps_in_mount == 0)
			continue;
		
		
		const struct statfs *mountbuf = apfs_mount_for_uuid(pc->p_mounts[mount_idx].pm_uuid);
		if (!mountbuf) {
			LOG("No mount for %s", uuid_string(pc->p_mounts[mount_idx].pm_uuid));
			continue;
		}
		
		const char *mount_name = mountbuf->f_mntonname;
		const char *bsd_disk = mountbuf->f_mntfromname;

		/* Do the translation of apfs_omap_record_ts to apfs_omap_lookup_result_ts */
		apfs_omap_lookup_t ol = {0};

		ol.ol_record_count = num_omaps_in_mount;

		/* Source buffer */
		ol.ol_records = calloc(num_omaps_in_mount, sizeof(*ol.ol_records));
		if (ol.ol_records == NULL) {
			ret = errno ?: ENOMEM;
			LOG("Unable to allocate memory for omap records");
			goto error_loop;
		}

		/* Dest buffer */
		ol.ol_results = calloc(num_omaps_in_mount, sizeof(*ol.ol_results));
		if (ol.ol_results == NULL) {
			ret = errno ?: ENOMEM;
			LOG("Unable to allocate memory for omap results");
			goto error_loop;
		}

		ol_record_idx = 0;
		/* Iterate through omaps and isolate only those belonging to mount_idx
		 */
		for (int global_omap_idx = 0; global_omap_idx < pc->p_nomaps; global_omap_idx++) {
			struct BC_playlist_omap *po = &pc->p_omaps[global_omap_idx];
			if (po->po_mount_idx == mount_idx) {
				memcpy(ol.ol_records + ol_record_idx, &(po->po_omap), sizeof(*ol.ol_records));
				ol_record_idx++;
			}
		}

		if (ol_record_idx != num_omaps_in_mount) {
			ret = EINVAL;
			LOG("Discrepency in number of omaps in mount");
			goto error_loop;
		}

		if ((ret = fsctl(mount_name, APFSIOC_OMAP_LOOKUP, &ol, 0)) != 0) {
			ret = 0; // Can't get extents for this mount, but other mounts may be fine
			LOG_ERRNO("Unable to lookup the records for mount %s", mount_name);
			goto error_loop;
		}

		/* Create a playlist for this mount's metadata */

		
		/* Each omap becomes a new entry */
		omap_pc = BC_allocate_playlist(1 /* nmounts */ , num_omaps_in_mount /* nentries */, 0 /* nomaps */);
		if (!omap_pc) {
			ret = ENOMEM;
			LOG("Unable to allocate playlist to convert omaps to entries");
			goto error_loop;
		}

		/* Fill up the single mount entry */
		memcpy(&omap_pc->p_mounts[0], &pc->p_mounts[mount_idx], sizeof(*omap_pc->p_mounts));
		omap_pc->p_mounts[0].pm_nentries = num_omaps_in_mount; /* Each omap becomes a new entry */
		omap_pc->p_mounts[0].pm_nomaps = 0; /* No omaps (we are converting the omaps to entries) */
		
		// <rdar://problem/33273791> Update BootCache to handle metadata for unencrypted volumes going to the container and metadata for encrypted APFS volumes going to the volume
		// For unencrypted apfs volumes, the OID cache needs to be applied to the container
		// For encrypted apfs volumes, the OID cache needs to be applied to the volume
		uint fs_flags;
		uuid_t container_uuid;
		ret = apfs_flags_for_volume(bsd_disk, &fs_flags, &container_uuid);
		if (ret != 0) {
			// Unable to get flags/container. Error is logged in fs_flags_for_volume
			ret = 0; // Can't get extents for this mount, but other mounts may be fine
			goto error_loop;
		} else if ((fs_flags & BC_FS_ENCRYPTED) != (omap_pc->p_mounts[0].pm_fs_flags & BC_FS_ENCRYPTED)) {
			// Volume has changed encryption state, give up
			LOG("APFS volume flags mismatch: %#x != %#x", fs_flags, omap_pc->p_mounts[0].pm_fs_flags);
			ret = 0; // Can't get extents for this mount, but other mounts may be fine
			goto error_loop;
		} else if (!(fs_flags & BC_FS_ENCRYPTED)) {
			// Volume is unencrypted, move OID metadata cache to the container
			DLOG("APFS volume %s is unencrypted, moving OID metadata cache to container %s", uuid_string(omap_pc->p_mounts[0].pm_uuid), uuid_string(container_uuid));
			uuid_copy(omap_pc->p_mounts[0].pm_uuid, container_uuid);
			omap_pc->p_mounts[0].pm_fs_flags |= BC_FS_APFS_CONTAINER;
		} else {
			// Volume is encrypted, keep OID metadata cache in the volume
		}
		
		//DLOG("OMAP conversion for mount %s", mount_name);
		//DLOG("(oid, oxid) --> (offset, length, crypto)");

		/* Fill up the playlist entries */
		for (int i = 0; i < num_omaps_in_mount; i++) {
			struct BC_playlist_entry *pentry = omap_pc->p_entries + i;
			apfs_omap_lookup_result_t result = ol.ol_results[i];

//#ifdef DEBUG
//			apfs_omap_track_record_t record = ol.ol_records[i];
//			DLOG("(0x%llx, 0x%llx) --> (0x%llx, 0x%llx, 0x%llx)",
//				  record.otr_oid, record.otr_oxid,
//				  result.ol_offset, result.ol_length, result.ol_cpoff);
//#endif
			
			pentry->pe_offset = result.ol_offset;
			pentry->pe_length = result.ol_length;
			if (fs_flags & BC_FS_ENCRYPTED) {
				pentry->pe_crypto_offset = result.ol_cpoff;
			} else {
				pentry->pe_crypto_offset = 0;
			}

#if BC_ADD_SHARED_CACHE_AT_HIGH_PRIORITY
			pentry->pe_batch = 1; /* All metadata is batch 1 (the one after the shared cache) */
#else
			pentry->pe_batch = 0; /* All metadata is batch 0 (the first batch) */
#endif
			pentry->pe_flags = 0; /* Should ideally copy over from the po_flags, but that doesn't exist yet */
			pentry->pe_mount_idx = 0; /* The only mount in this temp playlist, pc, we're creating */
			
		}

		/* Merge this mount's metadata playlist with the rest of the play list entries we're building up */
		if ((ret = BC_merge_playlists(pc, omap_pc)) != 0) {
			LOG("Unable to merge playlists");
			goto error_loop;
		}
		
error_loop:
		if (ol.ol_records)
			free(ol.ol_records);
		if (ol.ol_results)
			free(ol.ol_results);
		PC_FREE_ZERO(omap_pc);

		if (ret != 0)
			break;
	}
	
	if (ret == 0) {
		
		// We've converted all out omaps, throw them out now
		for (int mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
			pc->p_mounts[mount_idx].pm_nomaps = 0;
		}
		if (pc->p_omaps) {
			free(pc->p_omaps);
			pc->p_omaps = NULL;
		}
		pc->p_nomaps = 0;
		
		DLOG("Output playlist with %d mounts, %d entries",
			  pc->p_nmounts, pc->p_nentries);
	} else {
		BC_reset_playlist(pc);
	}
	
	return ret;
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
BC_convert_history_and_omaps(const struct BC_history *hc, const struct BC_omap_history *oh, struct BC_playlist **ppc)
{
	struct BC_playlist *pc = NULL;
	struct BC_playlist_mount *pm;
	struct BC_playlist_entry *pe;
	struct BC_playlist_omap *po;
	struct BC_history_mount *hm;
	struct BC_history_entry *he;
	int error, mount_idx, curbatch;

	*ppc = NULL;

	if (hc == NULL) {
		LOG_ERRNO("NULL hc");
		error = EINVAL;
		goto out;
	}

	uint32_t total_omaps = 0;
	if (oh) {
		for (int i = 0; i < oh->oh_nmounts; i++) {
			total_omaps += oh->oh_mounts[i].ohm_nomaps;
		}
	}

	pc = BC_allocate_playlist(hc->h_nmounts, hc->h_nentries, total_omaps);
	if (pc == NULL) {
		LOG_ERRNO("Unable to allocate a playlist of size (%d, %d, %d)", hc->h_nmounts,
						hc->h_nentries, total_omaps);
		error = ENOMEM;
		goto out;
	}

	pc->p_nmounts = 0;
	for (hm = hc->h_mounts; hm < (hc->h_mounts + hc->h_nmounts); hm++) {
		if (! uuid_is_null(hm->hm_uuid) && hm->hm_nentries > 0) {
			pm = pc->p_mounts + pc->p_nmounts;
			uuid_copy(pm->pm_uuid, hm->hm_uuid);
			pm->pm_fs_flags = hm->hm_fs_flags;
			pm->pm_nentries = 0;
			pm->pm_nomaps = 0;
			pc->p_nmounts++;
		}
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
			LOG("History entry %ld referenced mount index %d, but there are only %d mounts", (long)(he - hc->h_entries), he->he_mount_idx, hc->h_nmounts);
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
		pe->pe_crypto_offset = he->he_crypto_offset;
		pe->pe_batch = curbatch;
		pe->pe_flags = 0;
		if (he->he_flags & BC_HE_SHARED) {
			pe->pe_flags |= BC_PE_SHARED;
		}

		if (pm->pm_nentries >= hm->hm_nentries) {
			LOG("Bad playlist: more entries existed than mount %s claimed (%d)", uuid_string(pm->pm_uuid), hm->hm_nentries);
			error = EINVAL;
			goto out;
		}

		pm->pm_nentries++;
		pc->p_nentries++;
	}

	
	pc->p_nomaps = 0;
	if (oh != NULL) {

	   /* Iterate through each omap record, create a BC_playlist_omap for each of them and
		 * assign a mount index for it. */
		for (mount_idx = 0; mount_idx < oh->oh_nmounts; mount_idx++) {

			const struct BC_omap_history_mount *ohm = &oh->oh_mounts[mount_idx];
			
			uint pc_mount_idx;
			/* Iterate through mount list to figure out which mount we belong to */
			for (pc_mount_idx = 0; pc_mount_idx < pc->p_nmounts; pc_mount_idx++) {
				if (0 == uuid_compare(pc->p_mounts[pc_mount_idx].pm_uuid, ohm->ohm_uuid)) {
					break;
				}
			}
			if (pc_mount_idx >= pc->p_nmounts) {
				LOG("No recorded I/O for mount %s, ignoring %d omap records", uuid_string(ohm->ohm_uuid), ohm->ohm_nomaps);
				continue;
			}
			
			
			for (int omap_idx_in_mount = 0; omap_idx_in_mount < ohm->ohm_nomaps; omap_idx_in_mount++) {
				
				po = &(pc->p_omaps[pc->p_nomaps]);
				
				po->po_omap = ohm->ohm_omaps[omap_idx_in_mount];
				po->po_mount_idx = pc_mount_idx;
				
				//DLOG("omap record = {%s, (%#llx, %#llx)}", uuid_string(pc->p_mounts[pc_mount_idx].pm_uuid), po->po_omap.otr_oid, po->po_omap.otr_oxid);
				
				pc->p_nomaps++;
				/* Increment number of omaps pointing to this mount */
				pc->p_mounts[pc_mount_idx].pm_nomaps++;
			}
			
		}
	}


	/* Remove any mounts with no entries nor omaps */
	for (mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
		pm = pc->p_mounts + mount_idx;
		if (pm->pm_nentries == 0 && pm->pm_nomaps == 0) {
			pc->p_nmounts--;
			if (mount_idx < pc->p_nmounts) {
				memcpy(pm, pm + 1, sizeof(*pm) * (pc->p_nmounts - mount_idx));
				for (pe = pc->p_entries; pe < (pc->p_entries + pc->p_nentries); pe++)
					if (pe->pe_mount_idx > mount_idx)
						pe->pe_mount_idx--;
				for (po = pc->p_omaps; po < (pc->p_omaps + pc->p_nomaps); po++)
					if (po->po_mount_idx > mount_idx)
						po->po_mount_idx--;
				mount_idx--;
			}
		}
	}

	/*
	 * Sort the playlist into block order and coalesce into the smallest set
	 * of read operations.
	 */
#ifdef BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET
	BC_sort_playlist(pc);
#endif
	BC_coalesce_playlist(pc);

	/* Verify new playlist */
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
BC_start(const struct BC_playlist *pc)
{
	struct BC_playlist* new_pc = NULL;

	int ret = 0;
	
	if (pc) {
		if (pc->p_nmounts > 0) {
			
			new_pc = BC_copy_playlist(pc);
			ret = BC_playlist_lookup_omaps(new_pc);
			if (ret != 0) {
				errno = ret;
				LOG_ERRNO("Failed to convert playlist with omaps to playlist");
			} else {
				// Use the new playlist with the omaps converted into entries
				pc = new_pc;
				
			}
			
		} else {
			DLOG("Empty playlist");
			pc = NULL;
		}
	}

	struct BC_command bc;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_START;
	if (pc) {
		bc.bc_data1		 = (uintptr_t)	   pc->p_mounts;
		bc.bc_data1_size = (unsigned int) (pc->p_nmounts * sizeof(*pc->p_mounts));
		bc.bc_data2		 = (uintptr_t)	   pc->p_entries;
		bc.bc_data2_size = (unsigned int) (pc->p_nentries * sizeof(*pc->p_entries));
	} else {
		bc.bc_data1		 = 0;
		bc.bc_data1_size = 0;
		bc.bc_data2		 = 0;
		bc.bc_data2_size = 0;
	}
	
	ret = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	
	if (ret) {
		ret = errno;
		LOG_ERRNO("Unable to enable BootCache");
		PC_FREE_ZERO(new_pc);
		return ret;
	}
	
	ret = BC_start_omap_recording_for_all_mounts();

	PC_FREE_ZERO(new_pc);
	return ret;
}

int
BC_stop_and_fetch(struct BC_history **phc, struct BC_omap_history **poh)
{
	struct BC_command bc;
	struct BC_history *hc = NULL;
	int error;
	size_t nsize;
	
	if (phc) {
		*phc = NULL;
	}
	if (poh) {
		*poh  = NULL;
	}
	
	DLOG("Stopping and fetching boot cache");
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
			LOG_ERRNO("could not stop cache");
		
		error = errno;
		goto out;
	}
	if (nsize != sizeof(bc)) {
		LOG("control structure wrong size, version mismatch?");
		error = EINVAL;
		goto out;
	}

	/* Stop OMAP recording */
	BC_stop_omap_recording_for_all_mounts();

	/*
	 * Fetch and clear the history buffer.
	 */
	bc.bc_opcode = BC_OP_HISTORY;
	if ((hc = calloc(1, sizeof(*hc))) == NULL) {
		LOG("could not allocate history struct memory");
		error = errno ?: ENOMEM;
		goto out;
	}
	if (bc.bc_data1_size == 0 || bc.bc_data2_size == 0) {
		bc.bc_data1		 = 0;
		bc.bc_data2		 = 0;
		bc.bc_data1_size = 0;
		bc.bc_data2_size = 0;
	} else {
		if ((hc->h_mounts = malloc(bc.bc_data1_size)) == NULL) {
			LOG("could not allocate history mounts memory");
			error = errno ?: ENOMEM;
			goto out;
		}
		if ((hc->h_entries = malloc(bc.bc_data2_size)) == NULL) {
			LOG("could not allocate history mounts memory");
			error = errno ?: ENOMEM;
			goto out;
		}
		hc->h_nmounts  = bc.bc_data1_size / sizeof(struct BC_history_mount);
		hc->h_nentries = bc.bc_data2_size / sizeof(struct BC_history_entry);
		
		bc.bc_data1 = (uintptr_t) hc->h_mounts;
		bc.bc_data2 = (uintptr_t) hc->h_entries;
	}
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		LOG_ERRNO("could not fetch %u and %u bytes of history", bc.bc_data1_size, bc.bc_data2_size);
		error = errno ?: ENOMEM;
		goto out;
	}
	
	if (hc->h_mounts || hc->h_entries) {
		hc->h_nmounts  = bc.bc_data1_size / sizeof(struct BC_history_mount);
		hc->h_nentries = bc.bc_data2_size / sizeof(struct BC_history_entry);
	}
	
	if (phc != NULL) {
		*phc = hc;
		hc = NULL;
	} else {
		HC_FREE_ZERO(hc);
	}
	
	BC_fetch_omaps_for_all_mounts(poh);
	
#ifdef DEBUG
	uint32_t total_omaps = 0;
	if (poh && *poh) {
		for (int i = 0; i < (*poh)->oh_nmounts; i++) {
			total_omaps += (*poh)->oh_mounts[i].ohm_nomaps;
		}
	}
	
	DLOG("Collected %d mounts, %d entries and %d omaps", hc ? hc->h_nmounts : 0, hc ? hc->h_nentries : 0, total_omaps);
#endif
	
	return 0;
	
out:
	// On error, clear out OID tracking
	BC_stop_omap_recording_for_all_mounts();
	BC_fetch_omaps_for_all_mounts(NULL);
	HC_FREE_ZERO(hc);
	
	return(error);
}


static int
BC_start_omap_recording_for_mount(const char *mount)
{
	uint32_t enable = 1;
	int ret = fsctl(mount, APFSIOC_OMAP_TRACK_ENABLE, &enable, 0);
	if (ret != 0) {
		if (errno == ENOTTY) {
			DLOG("omap recording not supported for %s", mount);
			ret = 0;
		} else if (errno == EALREADY) {
			DLOG("omap recording already running for %s", mount);
			ret = 0;
		} else {
			LOG_ERRNO("Unable to start omap recording for %s", mount);
		}
	} else {
		DLOG("Started omap recording for %s", mount);
	}

	return ret;
}

#if 0
// Needs <rdar://problem/33557142> Use OID metadata cache for non-root mounts
static void
BC_start_omap_recording_for_mount_cache_entry(const void *key __unused, const void * __nonnull value, void * __nonnull context)
{
	// key is UUID of this mount
	// value is struct statfs* for this mount
	// context is a pointer to an error value
	const struct statfs* statfsbuf = (const struct statfs*)value;
	int* perror = (int*)context;
	
	int error = BC_start_omap_recording_for_mount(statfsbuf->f_mntonname);
	
	if (error != 0) {
		*perror = error;
	}
}
#endif

static int
BC_start_omap_recording_for_all_mounts(void)
{
	
#if 0
	// Needs <rdar://problem/33557142> Use OID metadata cache for non-root mounts

	// Clear cache so we get whatever is current
	clear_apfs_mount_cache();
	CFDictionaryRef cache = get_apfs_mount_cache();
	
	int error = 0;
	
	CFDictionaryApplyFunction(cache, BC_start_omap_recording_for_mount_cache_entry, &error);
	
	return error;
#else
	return BC_start_omap_recording_for_mount("/");
#endif
	
}


static int
BC_stop_omap_recording_for_mount(const char *mount)
{
	uint32_t enable = 0;
	int ret = fsctl(mount, APFSIOC_OMAP_TRACK_ENABLE, &enable, 0);
	if (ret != 0) {
		if (errno != ENOTTY) {
			LOG_ERRNO("Unable to stop omap recording for %s", mount);
		} else {
			DLOG("omap recording not supported for %s", mount);
			ret = 0;
		}
	} else {
		DLOG("Stopped omap recording for %s", mount);
	}

	return ret;
}

#if 0
// Needs <rdar://problem/33557142> Use OID metadata cache for non-root mounts
static void
BC_stop_omap_recording_for_mount_cache_entry(const void *key __unused, const void * __nonnull value, void * __nonnull context)
{
	// key is UUID of this mount
	// value is struct statfs* for this mount
	// context is a pointer to an error value
	const struct statfs* statfsbuf = (const struct statfs*)value;
	int* perror = (int*)context;
	
	int error = BC_stop_omap_recording_for_mount(statfsbuf->f_mntonname);
	
	if (error != 0) {
		*perror = error;
	}
}
#endif

static int
BC_stop_omap_recording_for_all_mounts(void)
{
#if 0
	// Needs <rdar://problem/33557142> Use OID metadata cache for non-root mounts
	
	// Clear cache so we get whatever is current (in case some other process started recording while we were running on a mount that appeeared since we last checked)
	clear_apfs_mount_cache();
	CFDictionaryRef cache = get_apfs_mount_cache();
	
	int error = 0;
	
	CFDictionaryApplyFunction(cache, BC_stop_omap_recording_for_mount_cache_entry, &error);
	
	return error;
#else
	return BC_stop_omap_recording_for_mount("/");
#endif
}

static int
BC_fetch_omaps_for_mount(const char *mount, struct BC_omap_history_mount *ohm)
{

	DLOG("Fetching omaps for mount %s", mount);

	int ret = 0;

	apfs_omap_track_report_t otr = {0};

	/* Stream down the records for the mount */
#define NUM_RECORDS_PER_REPORT 1024

	otr.otr_records = calloc(NUM_RECORDS_PER_REPORT, sizeof(*otr.otr_records));
	if (otr.otr_records == NULL) {
		ret = errno;
		LOG_ERRNO("Failed to allocate bufs for otr records");
		goto out;
	}

	do {
		otr.otr_record_count = NUM_RECORDS_PER_REPORT;

		ret = fsctl(mount, APFSIOC_OMAP_TRACK_REPORT, &otr, 0);
		if (ret != 0) {
			if (errno != ENOTTY) {
				LOG_ERRNO("Failed to fetch omaps for mount %s", mount);
			} else {
				DLOG("omap recording not supported for %s", mount);
				ret = 0;
			}
			goto out;
		}

		// DLOG("Received %d records from fsctl... %d remaining", otr.otr_record_count, otr.otr_records_remaining);

		/* Expand array to accomodate the new otr records */
		ohm->ohm_omaps = reallocf(ohm->ohm_omaps, (ohm->ohm_nomaps + otr.otr_record_count) * sizeof(*otr.otr_records));
		if (ohm->ohm_omaps == NULL) {
			ret = errno;
			LOG("Unable to realloc more apfs_omap_record_ts");
			goto out;
		}

		for (int i = 0; i < otr.otr_record_count; i++) {
			// DLOG("omap = (%#llx, %#llx)", otr.otr_records[i].otr_oid, otr.otr_records[i].otr_oxid);
			ohm->ohm_omaps[i + ohm->ohm_nomaps] = otr.otr_records[i];
		}

		ohm->ohm_nomaps += otr.otr_record_count;

	} while (otr.otr_records_remaining > 0);

	DLOG("Received a total of %d omaps for mount %s", ohm->ohm_nomaps, uuid_string(ohm->ohm_uuid));
	
	return ret;

out:
	if (otr.otr_records)
		free(otr.otr_records);
	ohm->ohm_nomaps = 0;
	if (ohm->ohm_omaps) {
		free(ohm->ohm_omaps);
		ohm->ohm_omaps = NULL;
	}
	return ret;
}

#if 0
// Needs <rdar://problem/33557142> Use OID metadata cache for non-root mounts
typedef struct {
	int error;
	struct BC_omap_history* oh;
} BC_fetch_omaps_for_mount_cache_entry_args;

static void
BC_fetch_omaps_for_mount_cache_entry(const void *key, const void * __nonnull value, void * __nonnull context)
{
	// key is UUID of this mount
	// value is struct statfs* for this mount
	// context is a pointer to BC_fetch_omaps_for_mount_cache_entry_args
	CFUUIDRef uuid = (CFUUIDRef)key;
	const struct statfs* statfsbuf = (const struct statfs*)value;
	BC_fetch_omaps_for_mount_cache_entry_args* args = (BC_fetch_omaps_for_mount_cache_entry_args*)context;
	int* perror = &args->error;
	struct BC_omap_history* oh = args->oh;
	
	oh->oh_nmounts++;
	oh->oh_mounts = reallocf(oh->oh_mounts, oh->oh_nmounts * sizeof(*oh->oh_mounts));
	if (oh->oh_mounts == NULL) {
		oh->oh_nmounts = 0;
		*perror = errno;
		LOG("Unable to realloc more oh_mounts");
		return;
	}
	
	struct BC_omap_history_mount* ohm = &oh->oh_mounts[oh->oh_nmounts - 1];
	
	CFUUIDBytes uuidbytes = CFUUIDGetUUIDBytes(uuid);
	uuid_copy(ohm->ohm_uuid, (/* uuid_t */ const unsigned char*)&uuidbytes);
	ohm->ohm_nomaps = 0;
	ohm->ohm_omaps = NULL;

	int error = BC_fetch_omaps_for_mount(statfsbuf->f_mntonname, ohm);
	
	if (error != 0) {
		*perror = error;
	}
}
#endif

static int
BC_fetch_omaps_for_all_mounts(struct BC_omap_history** poh)
{
	if (poh) {
		*poh = NULL;
	}

#if 0
	// Needs <rdar://problem/33557142> Use OID metadata cache for non-root mounts

	// Clear cache so we get whatever is current (in case some other process started recording while we were running on a mount that appeeared since we last checked)
	clear_apfs_mount_cache();
	CFDictionaryRef cache = get_apfs_mount_cache();

	BC_fetch_omaps_for_mount_cache_entry_args args = {
		.error = 0,
		.oh = calloc(1, sizeof(*args.oh)),
	};
	
	if (args.oh == NULL) {
		LOG_ERRNO("Failed to allocate BC_omap_history");
		return errno ?: ENOMEM;
	}
	
	CFDictionaryApplyFunction(cache, BC_fetch_omaps_for_mount_cache_entry, &args);
	
	if (poh) {
		*poh = args.oh;
	} else {
		OH_FREE_ZERO(args.oh);
	}
	
	return args.error;

#else
	struct BC_omap_history* oh = calloc(1, sizeof(*oh));
	oh->oh_nmounts = 1;
	oh->oh_mounts = calloc(1, sizeof(*oh->oh_mounts));
	if (oh->oh_mounts == NULL) {
		OH_FREE_ZERO(oh);
		LOG("Unable to alloc oh_mount");
		return errno ?: ENOMEM;
	}
	
	struct BC_omap_history_mount* ohm = &oh->oh_mounts[0];
	
	get_volume_uuid("/", ohm->ohm_uuid);
	if (uuid_is_null(ohm->ohm_uuid)) {
		LOG_ERRNO("Failed to find UUID for root mount");
		OH_FREE_ZERO(oh);
		return errno;
	}
	ohm->ohm_nomaps = 0;
	ohm->ohm_omaps = NULL;
	
	int error = BC_fetch_omaps_for_mount("/", ohm);
	
	if (error == 0 && poh) {
		*poh = oh;
	} else {
		OH_FREE_ZERO(oh);
	}
	
	return error;
#endif
}


static void get_volume_uuid(const char* volume, uuid_t uuid_out)
{
	struct attrlist list = {
		.bitmapcount = ATTR_BIT_MAP_COUNT,
		.volattr = ATTR_VOL_INFO | ATTR_VOL_UUID,
	};
	
	struct {
		uint32_t size;
		uuid_t	 uuid;
	} attrBuf = {0};
	
	if (0 != getattrlist(volume,  &list, &attrBuf, sizeof(attrBuf), 0)) {
		LOG("unable to determine uuid for volume %s", volume);
		uuid_clear(uuid_out);
	} else {
		uuid_copy(uuid_out, attrBuf.uuid);
	}
}


// returns 0 on success, non-0 on error
//
// Upon success:
//  If fs_flags_out indicates the bsddisk is an apfs volume, then container_uuid_out will indicate the UUID of its container
//  If the volume is not an APFS volume, container_uuid_out will be zero'ed out
static int apfs_flags_for_volume(const char* bsddisk, uint* fs_flags_out, uuid_t* container_uuid_out) {
	kern_return_t err;
	io_object_t volume;
	
	if (!fs_flags_out) {
		return EINVAL;
	}
	
	// Handle disk0s1 format as well as /dev/disk0s1 format
	if (0 == strncmp("/dev/disk", bsddisk, strlen("/dev/disk"))) {
		bsddisk += 5; // "/dev/"
	}
	
	*fs_flags_out = 0x0;
	if (container_uuid_out) {
		uuid_clear(*container_uuid_out);
	}
	

	CFMutableDictionaryRef dict = IOBSDNameMatching(kIOMasterPortDefault, 0, bsddisk);
	if (!dict) {
		LOG("Unable to get dict for disk %s", bsddisk);
		return ENOENT;
	}

	volume = IOServiceGetMatchingService(kIOMasterPortDefault, dict);
	if (volume == IO_OBJECT_NULL) {
		LOG("Unable to get service for disk %s", bsddisk);
		return ENOENT;
	}

	// dict is consumed by IOServiceGetMatchingService above
	dict = NULL;

	if (IOObjectConformsTo(volume, APFS_VOLUME_OBJECT)) {
		*fs_flags_out |= BC_FS_APFS;
		io_object_t container;
		
		CFBooleanRef volume_is_encrypted = IORegistryEntryCreateCFProperty(volume, CFSTR(kAPFSEncryptedKey), kCFAllocatorDefault, 0);
		if (volume_is_encrypted != NULL) {
			
			if (CFGetTypeID(volume_is_encrypted) == CFBooleanGetTypeID()) {
				if (CFBooleanGetValue(volume_is_encrypted)) {
					*fs_flags_out |= BC_FS_ENCRYPTED;
				}
			} else {
				LOG("Disk %s has bad encryption property type %lu", bsddisk, CFGetTypeID(volume_is_encrypted));
			}
			
			CFRelease(volume_is_encrypted);
		}
		
		if (container_uuid_out) {
			
			err = IORegistryEntryGetParentEntry(volume, kIOServicePlane, &container);
			if (err != 0) {
				LOG("Unable to get container for volume %s", bsddisk);
				IOObjectRelease(volume);
				return ENOENT;
			}
			
			if (! IOObjectConformsTo(container, APFS_CONTAINER_OBJECT)) {
				LOG("Parent is not an apfs container for volume %s", bsddisk);
				IOObjectRelease(container);
				IOObjectRelease(volume);
				return ENOENT;
			}
			
			CFStringRef container_uuid_str = IORegistryEntryCreateCFProperty(container, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
			IOObjectRelease(container);
			container = IO_OBJECT_NULL;
			if (container_uuid_str != NULL) {
				if (CFGetTypeID(container_uuid_str) == CFStringGetTypeID()) {
					CFUUIDRef container_uuid = CFUUIDCreateFromString(kCFAllocatorDefault, container_uuid_str);
					
					if (container_uuid) {
						CFUUIDBytes container_uuid_bytes = CFUUIDGetUUIDBytes(container_uuid);
						uuid_copy(*container_uuid_out, (/* uuid_t */ const unsigned char*)&container_uuid_bytes);
						// DLOG("Disk %s container is %s", bsddisk, uuid_string(*container_uuid_out));
						CFRelease(container_uuid);
					} else {
						LOG("Disk %s container unable to create cfuuid from uuid string %s", bsddisk, CFStringGetCStringPtr(container_uuid_str, kCFStringEncodingUTF8));
					}
				} else {
					LOG("Disk %s container has bad encryption property type %lu", bsddisk, CFGetTypeID(container_uuid_str));
				}
				
				CFRelease(container_uuid_str);
			} else {
				LOG("Disk %s container has no UUID", bsddisk);
			}
		}
	} else {
		DLOG("Disk %s is not an apfs volume", bsddisk);
	}
	
	IOObjectRelease(volume);

	return 0;
}

static struct statfs* g_mountbufs;
static CFMutableDictionaryRef g_apfs_mount_cache; // CFUUIDRef -> struct statfs* for APFS mounts

static void clear_apfs_mount_cache(void) {
	if (g_mountbufs) {
		free(g_mountbufs);
		g_mountbufs = NULL;
	}
	if (g_apfs_mount_cache) {
		CFRelease(g_apfs_mount_cache);
		g_apfs_mount_cache = NULL;
	}
}

static CFDictionaryRef get_apfs_mount_cache(void) {
	
	if (g_mountbufs == NULL || g_apfs_mount_cache == NULL) {
		clear_apfs_mount_cache(); // Make sure everything's released
		
		// Populate the cache
		int num_mountbufs = getmntinfo_r_np(&g_mountbufs, MNT_NOWAIT);
		
		g_apfs_mount_cache = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
		
		for (int i = 0; i < num_mountbufs; i++) {
			
			if (0 == strncmp(g_mountbufs[i].f_fstypename, "apfs", sizeof("apfs")+1)) {
				uuid_t mount_uuid;
				get_volume_uuid(g_mountbufs[i].f_mntonname, mount_uuid);
				if (!uuid_is_null(mount_uuid)) {
					// DLOG("%s, %s, %s: %s", g_mountbufs[i].f_mntonname, g_mountbufs[i].f_mntfromname, g_mountbufs[i].f_fstypename, uuid_string(mount_uuid));
					CFUUIDRef uuid_cf = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *(CFUUIDBytes*)mount_uuid);
					CFDictionaryAddValue(g_apfs_mount_cache, uuid_cf, g_mountbufs + i);
					CFRelease(uuid_cf);
				} else {
					DLOG("%s, %s, %s: no UUID", g_mountbufs[i].f_mntonname, g_mountbufs[i].f_mntfromname, g_mountbufs[i].f_fstypename);
				}
			}
		}
	}
	
	return g_apfs_mount_cache;
}

static const struct statfs* apfs_mount_for_uuid(uuid_t uuid) {

	CFDictionaryRef cache = get_apfs_mount_cache();
	
	CFUUIDRef uuid_cf = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *(CFUUIDBytes*)uuid);

	const struct statfs* buf = (const struct statfs*)CFDictionaryGetValue(cache, uuid_cf);
	
	CFRelease(uuid_cf);
	
	return buf;
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
 * If the file is compressed, the fd must be for the compressed resource fork.
 */
int
BC_playlist_for_file_extents(int fd, uint nextents, const struct bc_file_extent* extents, struct BC_playlist** ppc) {
	struct BC_playlist* pc = NULL;
	int error = 0;
	
	if (nextents == 0 || extents == NULL || fd < 0 || ppc == NULL) {
		return EINVAL;
	}
	
	*ppc = NULL;
	
	uuid_t uuid;
	const char* bsd_disk_path = NULL;
	bool is_apfs = false;
	
	// Get the mount's UUID where the file lives
	struct statfs statfs_buf;
	if (0 != fstatfs(fd, &statfs_buf)) {
		LOG_ERRNO("Unable to stafs %d", fd);
		goto out;
	}
	
	bsd_disk_path = statfs_buf.f_mntfromname;
	
	if (0 == strncmp("apfs", statfs_buf.f_fstypename, strlen("apfs") + 1)) {
		is_apfs = true;
	}
	
	get_volume_uuid(statfs_buf.f_mntonname, uuid);
	// Upon failure, UUID is cleared. And if we can't get a UUID, we want to assume root volume which is what a cleared UUID means
	
	// Allocate the playlist and inialize with the given mount
	pc = BC_allocate_playlist(1, 0, 0);
	if(!pc) {
		LOG("Unable to allocate playlist structure");
		error = errno;
		goto out;
	}
	uuid_copy(pc->p_mounts[0].pm_uuid, uuid);
	
	if (is_apfs) {
		error = apfs_flags_for_volume(bsd_disk_path, &pc->p_mounts[0].pm_fs_flags, NULL);
		if (error != 0) {
			// Unable to get flags/container. Error is logged in fs_flags_for_volume
			goto out;
		} else if (!(pc->p_mounts[0].pm_fs_flags & BC_FS_APFS)) {
			LOG("APFS volume without APFS flag");
			error = EINVAL;
			goto out;
		}
	} else {
		pc->p_mounts[0].pm_fs_flags = 0x0;
	}
	
	for (int extent_idx = 0; extent_idx < nextents; extent_idx++) {
		
		off_t ra_off;
		for (ra_off = extents[extent_idx].offset; (ra_off < extents[extent_idx].offset + extents[extent_idx].length); ) {
			off_t remaining = (extents[extent_idx].offset + extents[extent_idx].length - ra_off);
			struct log2phys block_range = {
				.l2p_flags		 = 0,
				.l2p_devoffset	 = ra_off,	  //As an IN parameter to F_LOG2PHYS_EXT, this is the offset into the file
				.l2p_contigbytes = remaining, //As an IN parameter to F_LOG2PHYS_EXT, this is the number of bytes to be queried
			};
			
			int ret = fcntl(fd, F_LOG2PHYS_EXT, &block_range);
			if (ret != 0) {
				//RLOG(NOTICE, "fcntl(%d, F_LOG2PHYS_EXT, &{.offset: %"PRIdoff", .bytes: %"PRIdoff"}) => %d (errno: %d %s)", fd, block_range.l2p_devoffset, block_range.l2p_contigbytes, ret, errno, strerror(errno));
				break;
			}
			
			// block_range.l2p_devoffset;	as an OUT parameter from F_LOG2PHYS_EXT, this is the offset in bytes on the disk
			// block_range.l2p_contigbytes; as an OUT parameter from F_LOG2PHYS_EXT, this is the number of bytes in the range
			
			if (remaining < block_range.l2p_contigbytes ) {
				LOG("Invalid size returned for %d from disk (%#llx bytes requested, %#llx bytes returned)", fd, remaining, block_range.l2p_contigbytes);
				break;
			}
			
			//RLOG(DEBUG, "%"PRIdoff":%"PRIdoff" is %"PRIdoff":%"PRIdoff" on disk\n", ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
			
			if (block_range.l2p_contigbytes <= 0) {
				//RLOG(INFO, "%"PRIdoff":%"PRIdoff" returned %"PRIdoff":%"PRIdoff"\n", ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
				break;
			}
			
			if (block_range.l2p_devoffset < 0) {
				LOG("Invalid block range return for %d from disk (%#llx:%#llx returned %#llx:%#llx)", fd, ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
				break;
			}
			
			ra_off += block_range.l2p_contigbytes;
			
			// Add this range to our array
			pc->p_entries = reallocf(pc->p_entries, (pc->p_nentries + 1) * sizeof(*pc->p_entries));
			if(! pc->p_entries) {
				LOG("could not allocate memory for playlist data");
				error = errno ?: ENOMEM;
				goto out;
			}
			pc->p_entries[pc->p_nentries].pe_offset		   = block_range.l2p_devoffset;
			pc->p_entries[pc->p_nentries].pe_length		   = block_range.l2p_contigbytes;
			pc->p_entries[pc->p_nentries].pe_batch		   = 0;
			pc->p_entries[pc->p_nentries].pe_flags		   = extents[extent_idx].flags;
			pc->p_entries[pc->p_nentries].pe_mount_idx	   = 0;
			if (is_apfs && pc->p_mounts[0].pm_fs_flags & BC_FS_ENCRYPTED) {
				// <rdar://problem/33489229> Ability to get file's extents plus crypto offset for BootCache playback. [ hardcode? ]
				// crypto offset == device offset, at least for most files
				pc->p_entries[pc->p_nentries].pe_crypto_offset = block_range.l2p_devoffset;
			} else {
				pc->p_entries[pc->p_nentries].pe_crypto_offset = 0;
			}
			pc->p_mounts[0].pm_nentries++;
			pc->p_nentries++;
		}
	}
	
	if (pc->p_nentries == 0) {
		LOG("No extents for file");
		error = ENOENT;
		goto out;
	}
	
#ifdef BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET
	BC_sort_playlist(pc);
#endif
	BC_coalesce_playlist(pc);
	
	if ((error = BC_verify_playlist(pc)) != 0) {
		goto out;
	}
	
	*ppc = pc;
	pc = NULL;
	error = 0;
out:
	PC_FREE_ZERO(pc);
	return (error);
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
	int error = 0;
	int compressed_fd = -1;
	off_t filesize = 0;

	*ppc = NULL;

	struct stat fd_stats;
	if (0 != fstat(fd, &fd_stats)) {
		LOG_ERRNO("Unable to stat %d", fd);
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
					LOG("Unable to open compressed resource fork %s", rsrcpath);
					error = errno;
					goto out;
				}
				fd = compressed_fd;
			} else {
				/* This is a compressed file, but it has no resource fork. Nothing to do */
				/* This happens with iCloud drive documents that haven't been downloaded yet */
				DLOG("Compressed file without a resource fork %s", fname);
				error = EINVAL;
				goto out;
			}
		} else {
			/* This is a compressed file, but we weren't provided a filename so can't grab the resource fork */
			LOG("Compressed file, but no filename");
			error = EINVAL;
			goto out;
		}
	}

	// Get the mount's UUID where the file lives
	struct statfs statfs_buf;
	if (0 != fstatfs(fd, &statfs_buf)) {
		LOG_ERRNO("Unable to stafs %d", fd);
		goto out;
	}
	
	if (maxsize != 0 && maxsize < filesize) {
		filesize = maxsize;
	}

	// Round up to the block size
	filesize = (((filesize + (statfs_buf.f_bsize - 1)) / statfs_buf.f_bsize) * statfs_buf.f_bsize);

	struct bc_file_extent whole_file = {
		.offset = 0,
		.length = filesize,
		.flags = 0x0,
	};
	
	error = BC_playlist_for_file_extents(fd, 1, &whole_file, ppc);
	if (error != 0) {
		errno = error;
		LOG_ERRNO("Unable to get playlist for %s", fname ?: "file");
	}
	
out:
	if (compressed_fd >= 0) {
		close(compressed_fd);
	}
	return (error);
}


/*
 * Check for intersection between two playlists
 */
int
BC_playlists_intersect(const struct BC_playlist* pa, const struct BC_playlist* pb)
{
	for (int a_mount_idx = 0; a_mount_idx < pa->p_nmounts; a_mount_idx++) {
		// Find matching mount in pb
		int b_mount_idx;
		for (b_mount_idx = 0; b_mount_idx < pb->p_nmounts; b_mount_idx++) {
			if (0 == uuid_compare(pa->p_mounts[a_mount_idx].pm_uuid, pb->p_mounts[b_mount_idx].pm_uuid)) {
				break;
			}
		}
		if (b_mount_idx == pb->p_nmounts) {
			// Didn't find a matching mount, skip to the next a_mount_idx
			continue;
		}
		
		// a_mount_idx is the same mount as b_mount_idx
		
		// Find first entry of each mount in the playlists
		int a_idx;
		for (a_idx = 0; a_idx< pa->p_nentries; a_idx++) {
			if (pa->p_entries[a_idx].pe_mount_idx == a_mount_idx) {
				break;
			}
		}
		if (a_idx == pa->p_nentries) {
			DLOG("No entries in pa for mount %d %s", a_mount_idx, uuid_string(pa->p_mounts[a_mount_idx].pm_uuid));
			continue;
		}
		
		int b_idx;
		for (b_idx = 0; b_idx< pb->p_nentries; b_idx++) {
			if (pb->p_entries[b_idx].pe_mount_idx == b_mount_idx) {
				break;
			}
		}
		if (b_idx == pb->p_nentries) {
			DLOG("No entries in pb for mount %d %s", b_mount_idx, uuid_string(pb->p_mounts[b_mount_idx].pm_uuid));
			continue;
		}
		
		// Zipper through the list to find matches (we only work on valid playlists that have no length 0 entries)
		while (a_idx < pa->p_nentries && pa->p_entries[a_idx].pe_mount_idx == a_mount_idx &&
			   b_idx < pb->p_nentries && pb->p_entries[b_idx].pe_mount_idx == b_mount_idx) {
			const struct BC_playlist_entry* pae = pa->p_entries + a_idx;
			const struct BC_playlist_entry* pbe = pb->p_entries + b_idx;
			
			if (pae->pe_offset == pbe->pe_offset) {
				
				DLOG("Found intersection:");
				DLOG("%s "playlist_entry_format_str, uuid_string(pa->p_mounts[a_mount_idx].pm_uuid), playlist_entry_format_args(pae));
				DLOG("%s "playlist_entry_format_str, uuid_string(pb->p_mounts[b_mount_idx].pm_uuid), playlist_entry_format_args(pbe));
				
				return true;
			} else if (pae->pe_offset < pbe->pe_offset) {
				if (pae->pe_offset + pae->pe_length > pbe->pe_offset) {
					
					DLOG("Found intersection:");
					DLOG("%s "playlist_entry_format_str, uuid_string(pa->p_mounts[a_mount_idx].pm_uuid), playlist_entry_format_args(pae));
					DLOG("%s "playlist_entry_format_str, uuid_string(pb->p_mounts[b_mount_idx].pm_uuid), playlist_entry_format_args(pbe));
					
					return true;
				}
				a_idx++;
			} else {
				//pbe->pe_offset < pae->pe_offset
				if (pbe->pe_offset + pbe->pe_length > pae->pe_offset) {
					
					DLOG("Found intersection:");
					DLOG("%s "playlist_entry_format_str, uuid_string(pa->p_mounts[a_mount_idx].pm_uuid), playlist_entry_format_args(pae));
					DLOG("%s "playlist_entry_format_str, uuid_string(pb->p_mounts[b_mount_idx].pm_uuid), playlist_entry_format_args(pbe));
					
					return true;
				}
				b_idx++;
			}
		}
	}
	
	DLOG("No intersection");
	return false;
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
			LOG_ERRNO("could not notify cache of new mount");
		return(errno);
	}
	
	
#if 0
	// Need <rdar://problem/33557142> Use OID metadata cache for non-root mounts
	return BC_start_omap_recording_for_all_mounts();
#else
	return 0;
#endif
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
			LOG_ERRNO("could not jettison cache");
		return(errno);
	}

	return(0);
}

static mach_timebase_info_data_t MachTimebase(void) {
	static mach_timebase_info_data_t info;
	static dispatch_once_t onceToken;
	dispatch_once(&onceToken, ^{
		mach_timebase_info(&info);
	});
	return info;
}

static u_int64_t MachToNano(u_int64_t mach) {
	mach_timebase_info_data_t info = MachTimebase();
	if (info.numer == info.denom) {
		return mach;
	}
	return mach * info.numer / info.denom;
}

int
BC_print_statistics(char *fname, const struct BC_statistics *ss)
{
	FILE *fp;

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
	
#define total_stat(stat)     (ss->ss_nonsharedcache.ss_##stat + ss->ss_sharedcache.ss_##stat)
#define shared_stat(stat)    (ss->ss_sharedcache.ss_##stat)
#define nonshared_stat(stat) (ss->ss_nonsharedcache.ss_##stat)
#define unknown_stat(stat) (ss->ss_##stat)

#define safe_div(num, denom) ((denom) ? ((num) / (denom)) : 0)
	
#define print_fp(fmt, args...) fprintf(fp, fmt"\n", ## args);

// Underbars are so the strings all line up below
#define print_string________________________(name, string) \
print_fp("%-26s %s", name, string)

#define print_formatted_____________________(name, fmt, args...) \
print_fp("%-26s "fmt, name, ## args)

#define stat_print_total____________________(name, stat) \
print_fp("%-26s %10u", name, total_stat(stat))
	
#define stat_print_shared___________________(name, stat) \
print_fp("%-26s %10u", name, shared_stat(stat))

#define stat_print_nonshared________________(name, stat) \
print_fp("%-26s %10u", name, nonshared_stat(stat))

#define stat_print_unknown__________________(name, stat) \
print_fp("%-26s %10u", name, unknown_stat(stat))

#define stat_print_unknown_msec_____________(name, stat) \
print_fp("%-26s %5d.%03ds", name, \
unknown_stat(stat) / 1000, \
unknown_stat(stat) % 1000)

#define stat_print_unknown_machabs__________(name, time) do {\
uint64_t _msec_total     = MachToNano(unknown_stat(time)) / 1000000; \
print_fp("%-26s  %5llu.%03llus", name, \
_msec_total / 1000, \
_msec_total % 1000); \
} while (0)

#define stat_print_machabs_manual___________(name, time) do {\
uint64_t _msec_total     = MachToNano(time) / 1000000; \
print_fp("%-26s  %5llu.%03llus", name, \
_msec_total / 1000, \
_msec_total % 1000); \
} while (0)

#define stat_print_percent_bothmanual_______(name, num, base) \
print_fp("%-26s %10u  %3.0f%%", name, num, \
safe_div((float)(num), (base)) * 100)

// Macros that print total, non-shared, and shared in a single line
#define stat_print_split____________________(name, stat) \
print_fp("%-26s %10u          %10u          %10u", name, total_stat(stat), nonshared_stat(stat), shared_stat(stat))

#define stat_print_split_readwrite_tps______(name, reads, writes, time) \
print_fp("%-26s  %4u/%4utps        %4u/%4utps        %4u/%4utps", name, \
safe_div(total_stat(reads)      * 1000, unknown_stat(time)), \
safe_div(total_stat(writes)     * 1000, unknown_stat(time)), \
safe_div(nonshared_stat(reads)  * 1000, unknown_stat(time)), \
safe_div(nonshared_stat(writes) * 1000, unknown_stat(time)), \
safe_div(shared_stat(reads)     * 1000, unknown_stat(time)), \
safe_div(shared_stat(writes)    * 1000, unknown_stat(time)))

#define stat_print_split_manual_____________(name, num) \
print_fp("%-26s %10u          %10u          %10u", name, \
(num##_nonshared + num##_shared), \
num##_nonshared, \
num##_shared)

#define stat_print_split_msec_______________(name, stat) \
print_fp("%-26s %10ums        %10ums        %10ums", name, total_stat(stat), nonshared_stat(stat), shared_stat(stat))

// total time is tracked independently from shared and nonshared batch time
#define stat_print_split_read_rate__________(name, bytes, num, time) do { \
uint64_t _msec_nonshared = MachToNano(nonshared_stat(time)) / 1000000; \
uint64_t _msec_shared    = MachToNano(   shared_stat(time)) / 1000000; \
uint64_t _msec_total     = MachToNano(  unknown_stat(time)) / 1000000; \
print_fp("%-26s    %7uKB/s %4ut/s %7uKB/s %4ut/s %7uKB/s %4ut/s", name, \
(u_int)safe_div((unsigned long long)(nonshared_stat(bytes) + shared_stat(bytes)) * 1000, (_msec_total     * 1024)),  /* KB/s */ \
(u_int)safe_div(                    (nonshared_stat(num  ) + shared_stat(num  )) * 1000, (_msec_total           )),  /* t/s  */ \
(u_int)safe_div((unsigned long long)(nonshared_stat(bytes)                     ) * 1000, (_msec_nonshared * 1024)),  /* KB/s */ \
(u_int)safe_div(                    (nonshared_stat(num  )                     ) * 1000, (_msec_nonshared       )),  /* t/s  */ \
(u_int)safe_div((unsigned long long)(   shared_stat(bytes)                     ) * 1000, (_msec_shared    * 1024)),  /* KB/s */ \
(u_int)safe_div(                    (   shared_stat(num  )                     ) * 1000, (_msec_shared          ))); /* t/s  */ \
} while (0)

#define stat_print_split_read_rate_manual___(name, bytes, num, time) do { \
uint64_t _msec_nonshared = MachToNano(time##_nonshared) / 1000000; \
uint64_t _msec_shared    = MachToNano(time##_shared)    / 1000000; \
uint64_t _msec_total     = MachToNano(time##_total)     / 1000000; \
print_fp("%-26s    %7uKB/s %4ut/s %7uKB/s %4ut/s %7uKB/s %4ut/s", name, \
(u_int)safe_div((unsigned long long)(bytes##_nonshared + bytes##_shared) * 1000, (_msec_total     * 1024)),  /* KB/s */ \
(u_int)safe_div(                    (  num##_nonshared +   num##_shared) * 1000, (_msec_total           )),  /* t/s  */ \
(u_int)safe_div((unsigned long long)(bytes##_nonshared                 ) * 1000, (_msec_nonshared * 1024)),  /* KB/s */ \
(u_int)safe_div(                    (  num##_nonshared                 ) * 1000, (_msec_nonshared       )),  /* t/s  */ \
(u_int)safe_div((unsigned long long)(bytes##_shared                    ) * 1000, (_msec_shared    * 1024)),  /* KB/s */ \
(u_int)safe_div(                    (  num##_shared                    ) * 1000, (_msec_shared          ))); /* t/s  */ \
} while (0)

#define stat_print_split_read_time__________(name, time) do { \
uint64_t _msec_nonshared = MachToNano(nonshared_stat(time)) / 1000000; \
uint64_t _msec_shared    = MachToNano(   shared_stat(time)) / 1000000; \
uint64_t _msec_total     = MachToNano(  unknown_stat(time)) / 1000000; \
print_fp("%-26s  %5llu.%03llus          %5llu.%03llus          %5llu.%03llus", name, \
_msec_total     / 1000, /* whole seconds   */ \
_msec_total     % 1000, /* decimal seconds */ \
_msec_nonshared / 1000, /* whole seconds   */ \
_msec_nonshared % 1000, /* decimal seconds */ \
_msec_shared    / 1000, /* whole seconds   */ \
_msec_shared    % 1000);/* decimal seconds */ \
} while (0)

#define stat_print_split_read_time_manual___(name, time) do { \
uint64_t _msec_nonshared = MachToNano(time##_nonshared) / 1000000; \
uint64_t _msec_shared    = MachToNano(time##_shared)    / 1000000; \
uint64_t _msec_total     = MachToNano(time##_total)     / 1000000; \
print_fp("%-26s  %5llu.%03llus          %5llu.%03llus          %5llu.%03llus", name, \
_msec_total     / 1000, /* whole seconds   */ \
_msec_total     % 1000, /* decimal seconds */ \
_msec_nonshared / 1000, /* whole seconds   */ \
_msec_nonshared % 1000, /* decimal seconds */ \
_msec_shared    / 1000, /* whole seconds   */ \
_msec_shared    % 1000);/* decimal seconds */ \
} while (0)

#define stat_print_split_msec_max___________(name, stat) \
print_fp("%-26s %10ums        %10ums        %10ums", name, MAX(nonshared_stat(stat), shared_stat(stat)), nonshared_stat(stat), shared_stat(stat))

#define stat_print_split_percent____________(name, stat, basestat) \
print_fp("%-26s %10u  %3.0f%%    %10u  %3.0f%%    %10u  %3.0f%%", name, \
    total_stat(stat), safe_div(((float)    total_stat(stat)),     total_stat(basestat)) * 100, /* num, percent */ \
nonshared_stat(stat), safe_div(((float)nonshared_stat(stat)), nonshared_stat(basestat)) * 100, /* num, percent */ \
   shared_stat(stat), safe_div(((float)   shared_stat(stat)),    shared_stat(basestat)) * 100) /* num, percent */

#define stat_print_split_percent_basemanual_(name, stat, base) \
print_fp("%-26s %10u  %3.0f%%    %10u  %3.0f%%    %10u  %3.0f%%", name, \
    total_stat(stat), safe_div(((float)    total_stat(stat)), (base##_nonshared + base##_shared)) * 100, /* num, percent */ \
nonshared_stat(stat), safe_div(((float)nonshared_stat(stat)), (base##_nonshared                )) * 100, /* num, percent */ \
   shared_stat(stat), safe_div(((float)   shared_stat(stat)), (base##_shared                   )) * 100) /* num, percent */

#define stat_print_split_percent_nummanual__(name, num, basestat) \
print_fp("%-26s %10u  %3.0f%%    %10u  %3.0f%%    %10u  %3.0f%%", name, \
(num##_nonshared + num##_shared), safe_div((float)(num##_nonshared + num##_shared),     total_stat(basestat)) * 100, /* num, percent */ \
(num##_nonshared               ), safe_div((float)(num##_nonshared               ), nonshared_stat(basestat)) * 100, /* num, percent */ \
(num##_shared                  ), safe_div((float)(num##_shared                  ),    shared_stat(basestat)) * 100) /* num, percent */
	
#define stat_print_split_percent_bothmanual_(name, stat, num, base) \
print_fp("%-26s %10u  %3.0f%%    %10u  %3.0f%%    %10u  %3.0f%%", name, \
(num##_nonshared + num##_shared), safe_div(((float)(num##_nonshared + num##_shared)), (base##_nonshared + base##_shared)) * 100, /* num, percent */ \
(num##_nonshared               ), safe_div(((float)(num##_nonshared               )), (base##_nonshared                )) * 100, /* num, percent */ \
(num##_shared                  ), safe_div(((float)(num##_shared                  )), (base##_shared                   )) * 100) /* num, percent */


print_fp("%-26s %10s %19s %19s", "", "total", "non-shared cache", "shared cache")

stat_print_split____________________("initiated reads", initiated_reads);
	if (total_stat(initiated_reads) > 0 && total_stat(read_errors) > 0) {
stat_print_split_percent____________(" read errors", read_errors, initiated_reads);
	}
stat_print_split____________________("blocks read", read_blocks);
stat_print_split____________________("bytes read", read_bytes);
	if (total_stat(read_bytes) > 0) {
		u_int bytes_remaining_nonshared = nonshared_stat(read_bytes) - nonshared_stat(read_errors_bytes) - nonshared_stat(close_discards) - nonshared_stat(hit_bytes) - nonshared_stat(stolen_discards) - nonshared_stat(error_discards) - nonshared_stat(lowpri_discards) - nonshared_stat(bypass_nocache_discards) - nonshared_stat(read_discards) - nonshared_stat(write_discards) - nonshared_stat(hit_bytes_afterhistory) - nonshared_stat(lost_bytes_afterhistory) - nonshared_stat(spurious_discards);
		u_int bytes_remaining_shared = shared_stat(read_bytes) - shared_stat(read_errors_bytes) - shared_stat(close_discards) - shared_stat(hit_bytes) - shared_stat(stolen_discards) - shared_stat(error_discards) - shared_stat(lowpri_discards) - shared_stat(bypass_nocache_discards) - shared_stat(read_discards) - shared_stat(write_discards) - shared_stat(hit_bytes_afterhistory) - shared_stat(lost_bytes_afterhistory) - shared_stat(spurious_discards);
		u_int bytes_remaining_total = bytes_remaining_shared + bytes_remaining_nonshared;
stat_print_split_percent____________(" bytes used", hit_bytes, read_bytes);
stat_print_split_percent_nummanual__(" bytes remaining", bytes_remaining, read_bytes);
stat_print_split_percent____________(" low priority bytes", read_bytes_lowpri, read_bytes);
		if (total_stat(read_bytes_lowpri) > 0) {
stat_print_split_percent____________("  lowpri bytes discarded", lowpri_discards, read_bytes_lowpri);
		}
stat_print_split_percent____________(" bytes failed to read", read_errors_bytes, read_bytes);
		if (total_stat(bypass_nocache_discards) > 0) {
stat_print_split_percent____________(" bytes discarded by noncac", bypass_nocache_discards, read_bytes);
		}
stat_print_split_percent____________(" bytes discarded by write", write_discards, read_bytes);
stat_print_split_percent____________(" bytes discarded by error", error_discards, read_bytes);
stat_print_split_percent____________(" bytes discarded by read", read_discards, read_bytes);
		if (total_stat(close_discards) > 0) {
stat_print_split_percent____________(" bytes discarded by close",close_discards, read_bytes);
		}
		if (total_stat(unable_to_discard_bytes) > 0) {
stat_print_split_percent____________(" bytes unable to discard", unable_to_discard_bytes, read_bytes);
		}
stat_print_split_percent____________(" bytes stolen from cache", stolen_discards, read_bytes);
stat_print_split_percent____________(" bytes used after boot", hit_bytes_afterhistory, read_bytes);
stat_print_split_percent____________(" bytes lost after boot", lost_bytes_afterhistory, read_bytes);
		if (bytes_remaining_total == 0 || 0 != total_stat(spurious_discards)) {
stat_print_split_percent____________(" bytes wasted", spurious_discards, read_bytes);
		}
	}
	
stat_print_split____________________("cache size", cache_bytes);
	if (total_stat(cache_bytes) > 0) {
		
		u_int bytes_unaccounted_nonshared = nonshared_stat(cache_bytes) - (nonshared_stat(read_bytes) - nonshared_stat(readerror_unread)) - nonshared_stat(close_unread) - nonshared_stat(stolen_unread) - nonshared_stat(error_unread) - nonshared_stat(lowpri_unread) - nonshared_stat(bypass_nocache_unread) - nonshared_stat(read_unread) - nonshared_stat(write_unread) - nonshared_stat(spurious_unread) - nonshared_stat(badreader_unread) - nonshared_stat(mounterror_unread) - nonshared_stat(nonroot_unread) - nonshared_stat(unsupported_unread) - nonshared_stat(extenterror_unread);
		u_int bytes_unaccounted_shared = shared_stat(cache_bytes) - (shared_stat(read_bytes) - shared_stat(readerror_unread)) - shared_stat(close_unread) - shared_stat(stolen_unread) - shared_stat(error_unread) - shared_stat(lowpri_unread) - shared_stat(bypass_nocache_unread) - shared_stat(read_unread) - shared_stat(write_unread) - shared_stat(spurious_unread) - shared_stat(badreader_unread) - shared_stat(mounterror_unread) - shared_stat(nonroot_unread) - shared_stat(unsupported_unread) - shared_stat(extenterror_unread);
		u_int bytes_unaccounted_total = bytes_unaccounted_nonshared + bytes_unaccounted_shared;

		if (bytes_unaccounted_total > 0) {
			// Math here is broken - read_bytes includes a lot of the numbers being subtracted
//stat_print_split_percent_nummanual__(" bytes unaccounted for", bytes_unaccounted, cache_bytes);
		}
stat_print_split_percent____________(" bytes unread read error", readerror_unread, cache_bytes);
stat_print_split_percent____________(" bytes unread mount error", mounterror_unread, cache_bytes);
stat_print_split_percent____________(" bytes unread extent error", extenterror_unread, cache_bytes);
stat_print_split_percent____________(" bytes unread unsupported", unsupported_unread, cache_bytes);
stat_print_split_percent____________(" bytes unread error satisf", error_unread, cache_bytes);
stat_print_split_percent____________(" bytes unread low priority", lowpri_unread, cache_bytes);
		if (total_stat(read_unread) > 0 || total_stat(write_unread) > 0) {
stat_print_split_percent____________(" bytes unread due to read", read_unread, cache_bytes);
stat_print_split_percent____________(" bytes unread due to write", write_unread, cache_bytes);
		}
		if (total_stat(stolen_unread) > 0) {
stat_print_split_percent____________(" bytes unread due to stole", stolen_unread, cache_bytes);
		}
		if (total_stat(spurious_unread) > 0) {
stat_print_split_percent____________(" bytes unread at terminate", spurious_unread, cache_bytes);
		}
		if (total_stat(close_unread) > 0) {
stat_print_split_percent____________(" bytes unread mount closed", close_unread, cache_bytes);
		}
		if (total_stat(nonroot_unread) > 0) {
stat_print_split_percent____________(" bytes unread nonroot", nonroot_unread, cache_bytes);
		}
		if (total_stat(bypass_nocache_unread) > 0) {
stat_print_split_percent____________(" bytes unread noncached", bypass_nocache_unread, cache_bytes);
		}
		if (total_stat(badreader_unread) > 0) {
stat_print_split_percent____________(" bytes unread due to threa", badreader_unread, cache_bytes);
		}
		if (unknown_stat(cache_size) > total_stat(cache_bytes)) {
stat_print_percent_bothmanual_______("cache alignment growth", unknown_stat(cache_size) - total_stat(cache_bytes), total_stat(cache_bytes));
		}
	}
	if (total_stat(cache_oversize) > 0) {
stat_print_split____________________("cache oversize", cache_oversize);
	}
	
stat_print_split____________________("bytes requested", requested_bytes);
	if (total_stat(requested_bytes) > 0) {
stat_print_split_percent____________(" bytes hit", hit_bytes, requested_bytes);
		for (int m = 0; m < STAT_MOUNTMAX; m++) {
			if (total_stat(requested_bytes_m[m]) > 0 && total_stat(requested_bytes_m[m]) != total_stat(requested_bytes)) {
				char mountstr[32];
snprintf(mountstr, sizeof(mountstr), " mount %d bytes requested", m);
				stat_print_split_percent____________(mountstr, requested_bytes_m[m], requested_bytes);
snprintf(mountstr, sizeof(mountstr), "  mount %d bytes hit", m);
				stat_print_split_percent____________(mountstr, hit_bytes_m[m], requested_bytes_m[m]);
			}
		}
		
		u_int noncached_bytes_requested_nonshared = nonshared_stat(bypass_nocache_bytes) + nonshared_stat(hit_nocache_bytes);
		u_int noncached_bytes_requested_shared = shared_stat(bypass_nocache_bytes) + shared_stat(hit_nocache_bytes);
stat_print_split_percent_nummanual__(" noncached bytes requested", noncached_bytes_requested, requested_bytes);
		if (total_stat(bypass_nocache_bytes) + total_stat(hit_nocache_bytes) > 0) {
stat_print_split_percent_basemanual_("  noncached bytes hit", hit_nocache_bytes, noncached_bytes_requested);
stat_print_split_percent_basemanual_("  noncached bytes missed", bypass_nocache_bytes, noncached_bytes_requested);
		}
	}

	if (ss->ss_cache_time > 0 || total_stat(read_bytes) == 0) {
stat_print_unknown_msec_____________("cache active time", cache_time);
	} else {
print_string________________________("cache active time", "(still active)");
	}

	


	/* inbound strategy */
print_fp("");
stat_print_split____________________("total strategy calls", strategy_calls);
	if (total_stat(strategy_calls) > 0) {
		u_int bypassed_nonshared = nonshared_stat(strategy_calls) - nonshared_stat(strategy_bypassed);
		u_int bypassed_shared = shared_stat(strategy_calls) - shared_stat(strategy_bypassed);
stat_print_split_percent_nummanual__(" filled calls", bypassed, strategy_calls);
stat_print_split_percent____________(" bypassed calls", strategy_bypassed, strategy_calls);
		if (total_stat(strategy_bypassed) > 0) {
			u_int other_nonshared = nonshared_stat(strategy_bypassed) - nonshared_stat(strategy_nonread) - nonshared_stat(strategy_noncached_mount) - nonshared_stat(strategy_unready_mount) - nonshared_stat(strategy_nonblocksize) - (nonshared_stat(extent_lookups) - nonshared_stat(extent_hits)) - nonshared_stat(strategy_bypass_duringio_unfilled) - nonshared_stat(strategy_unfilled_lowpri);
			u_int other_shared = shared_stat(strategy_bypassed) - shared_stat(strategy_nonread) - shared_stat(strategy_noncached_mount) - shared_stat(strategy_unready_mount) - shared_stat(strategy_nonblocksize) - (shared_stat(extent_lookups) - shared_stat(extent_hits)) - shared_stat(strategy_bypass_duringio_unfilled) - shared_stat(strategy_unfilled_lowpri);
stat_print_split_percent____________("  nonread calls", strategy_nonread, strategy_bypassed);
stat_print_split_percent____________("  noncached mount calls", strategy_noncached_mount, strategy_bypassed);
stat_print_split_percent____________("  unready mount calls", strategy_unready_mount, strategy_bypassed);
			if (total_stat(strategy_nonblocksize) > 0) {
stat_print_split_percent____________("  nonblocksize calls", strategy_nonblocksize, strategy_bypassed);
			}
			u_int cache_miss_nonshared = nonshared_stat(extent_lookups) - nonshared_stat(extent_hits);
			u_int cache_miss_shared = shared_stat(extent_lookups) - shared_stat(extent_hits);
			u_int cache_miss_total = cache_miss_nonshared + cache_miss_shared;
stat_print_split_percent_nummanual__("  cache misses", cache_miss, strategy_bypassed);
			if (cache_miss_total > 0) {
stat_print_split_percent_basemanual_("   crypto mismatch", extent_crypto_mismatches, cache_miss);
stat_print_split_percent_basemanual_("   partial hit", extent_partial_hits, cache_miss);
			}
stat_print_split_percent_nummanual__("  cache hit failures", other, strategy_bypassed);
			if (total_stat(strategy_bypass_duringio_unfilled) > 0) {
stat_print_split_percent____________("  unfilled extent", strategy_bypass_duringio_unfilled, strategy_bypassed);
			}
			if (total_stat(strategy_unfilled_lowpri) > 0) {
stat_print_split_percent____________("  unfilled lowpri extent", strategy_unfilled_lowpri, strategy_bypassed);
			}
		}
		u_int noncached_nonshared = nonshared_stat(strategy_bypass_nocache) + nonshared_stat(strategy_hit_nocache);
		u_int noncached_shared = shared_stat(strategy_bypass_nocache) + shared_stat(strategy_hit_nocache);
		u_int noncached_total = noncached_nonshared + noncached_shared;
stat_print_split_percent_nummanual__(" noncached calls", noncached, strategy_calls);
		if (noncached_total > 0) {
stat_print_split_percent_basemanual_("  noncached calls filled", strategy_hit_nocache, noncached);
stat_print_split_percent_basemanual_("  noncached calls bypassed", strategy_bypass_nocache, noncached);
		}
stat_print_split_percent____________(" throttled calls", strategy_throttled, strategy_calls);
stat_print_split_percent____________(" calls during readahead", strategy_duringio, strategy_calls);
		if (total_stat(strategy_duringio) > 0) {
			u_int filled_during_readahead_nonshared = nonshared_stat(strategy_duringio) - nonshared_stat(strategy_bypass_duringio);
			u_int filled_during_readahead_shared = shared_stat(strategy_duringio) - shared_stat(strategy_bypass_duringio);
stat_print_split_percent_nummanual__("  filled during readahead", filled_during_readahead, strategy_duringio);
stat_print_split_percent____________("  bypassed during readahea", strategy_bypass_duringio, strategy_duringio);
			if (total_stat(strategy_bypass_duringio) > 0) {
				u_int other_nonshared = nonshared_stat(strategy_bypass_duringio) - nonshared_stat(strategy_bypass_duringio_rootdisk_nonread) - nonshared_stat(strategy_bypass_duringio_rootdisk_read) - nonshared_stat(strategy_bypass_duringio_rootdisk_failure) - nonshared_stat(strategy_bypass_duringio_unfilled) - nonshared_stat(strategy_bypass_duringio_nocache);
				u_int other_shared = shared_stat(strategy_bypass_duringio) - shared_stat(strategy_bypass_duringio_rootdisk_nonread) - shared_stat(strategy_bypass_duringio_rootdisk_read) - shared_stat(strategy_bypass_duringio_rootdisk_failure) - shared_stat(strategy_bypass_duringio_unfilled) - shared_stat(strategy_bypass_duringio_nocache);
				if (total_stat(strategy_bypass_duringio_unfilled) > 0) {
stat_print_split_percent____________("   unfilled extent", strategy_bypass_duringio_unfilled, strategy_bypass_duringio);
					if (total_stat(unable_to_discard_count) > 0) {
stat_print_split_percent____________("    unable to discard", unable_to_discard_count, strategy_bypass_duringio_unfilled);
					}
				}
				if (total_stat(strategy_bypass_duringio_nocache) > 0) {
stat_print_split_percent____________("   noncached IO", strategy_bypass_duringio_nocache, strategy_bypass_duringio);
				}
stat_print_split_percent____________("   root disk nonread", strategy_bypass_duringio_rootdisk_nonread, strategy_bypass_duringio);
stat_print_split_percent____________("   root disk cache miss", strategy_bypass_duringio_rootdisk_read, strategy_bypass_duringio);
				if (total_stat(strategy_bypass_duringio_rootdisk_read) > 0) {
stat_print_split_percent____________("    root crypto mismatch", strategy_bypass_duringio_rootdisk_read_crypto_mismatch, strategy_bypass_duringio_rootdisk_read);
stat_print_split_percent____________("    root disk partial hit", strategy_bypass_duringio_rootdisk_read_partial_hits, strategy_bypass_duringio_rootdisk_read);
				}
stat_print_split_percent____________("   root disk hit failure", strategy_bypass_duringio_rootdisk_failure, strategy_bypass_duringio);
stat_print_split_percent_nummanual__("   non-root", other, strategy_bypass_duringio);
stat_print_split_percent____________("   forced throttled", strategy_forced_throttled, strategy_bypass_duringio);
stat_print_split_percent____________("   bypassed nonthrottled", strategy_nonthrottled, strategy_bypass_duringio);
			}
stat_print_split_percent____________("  extent hits during reada", hit_duringio, strategy_duringio);
			if (total_stat(hit_duringio) > 0) {
stat_print_split_percent____________("   blocked during readahea", strategy_blocked, hit_duringio);
stat_print_split_percent____________("   block calls timed out", strategy_timedout, hit_duringio);
stat_print_split_msec_max___________("   longest blocked call", strategy_time_longest_blocked);
stat_print_split_msec_______________("   total blocked duration", strategy_time_blocked);
			}
		}
	}
	if (total_stat(strategy_unknown) > 0) {
stat_print_split____________________("unknown strategy calls", strategy_unknown);
stat_print_split____________________("unknown strategy bytes", strategy_unknown_bytes);
	}
	
	char diskbuf[64];
	if (total_stat(read_bytes) > total_stat(read_bytes_lowpri)) {
print_fp("");
stat_print_unknown__________________("total readahead threads", readahead_threads);
		for (int d = 0; d < STAT_DISKMAX; d++) {
			u_int disk_bytes_nonshared = 0, disk_bytes_shared = 0;
			uint64_t disk_machabstime_total = 0, disk_machabstime_nonshared = 0, disk_machabstime_shared = 0;
			u_int disk_num_reads_nonshared = 0, disk_num_reads_shared = 0;
			
			for(int b = 0; b < STAT_BATCHMAX; b++) {
				disk_bytes_nonshared += nonshared_stat(batch_bytes[d][b]);
				disk_bytes_shared += shared_stat(batch_bytes[d][b]);
				disk_machabstime_nonshared += nonshared_stat(batch_time[d][b]);
				disk_machabstime_shared += shared_stat(batch_time[d][b]);
				disk_machabstime_total += unknown_stat(batch_time[d][b]);
				disk_num_reads_nonshared += nonshared_stat(batch_initiated_reads[d][b]);
				disk_num_reads_shared += shared_stat(batch_initiated_reads[d][b]);
			}
			if (0 == disk_bytes_nonshared + disk_bytes_shared) continue; /* no reads for this disk */
			
			
			if (disk_machabstime_total > 0) {
				snprintf(diskbuf, sizeof(diskbuf), "Disk %d reader rate", d);
stat_print_split_read_rate_manual___(diskbuf, disk_bytes, disk_num_reads, disk_machabstime);
			}
			
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d time", d);
stat_print_split_read_time_manual___(diskbuf, disk_machabstime);
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d bytes read", d);
stat_print_split_manual_____________(diskbuf, disk_bytes);
			
			for(int b = 0; b < STAT_BATCHMAX; b++) {
				if (total_stat(batch_bytes[d][b]) > 0) {
					snprintf(diskbuf, sizeof(diskbuf), "  batch %d time", b);
stat_print_split_read_time__________(diskbuf, batch_time[d][b]);
				}
			}
			
			for(int b = 0; b < STAT_BATCHMAX; b++) {
				if (total_stat(batch_bytes[d][b]) > 0) {
					snprintf(diskbuf, sizeof(diskbuf), " batch %d bytes read", b);
stat_print_split____________________(diskbuf, batch_bytes[d][b]);
					if (total_stat(batch_late_bytes[d][b]) > 0) {
						snprintf(diskbuf, sizeof(diskbuf), "  batch %d late bytes", b);
						stat_print_split_percent____________(diskbuf, batch_late_bytes[d][b], batch_bytes[d][b]);
					}
				}
			}
			
			for(int b = 0; b < STAT_BATCHMAX; b++) {
				if (total_stat(batch_bytes[d][b]) > 0) {
					snprintf(diskbuf, sizeof(diskbuf), " batch %d reader rate", b);
stat_print_split_read_rate__________(diskbuf, batch_bytes[d][b], batch_initiated_reads[d][b], batch_time[d][b]);
				}
			}

		}
	}

	if (total_stat(read_bytes_lowpri) > 0) {
		for (int d = 0; d < STAT_DISKMAX; d++) {
			if (0 == total_stat(batch_bytes_lowpri[d])) continue; /* no reads for this disk */
			
			if (unknown_stat(batch_time_lowpri[d]) > 0) {
				snprintf(diskbuf, sizeof(diskbuf), "Disk %d lowpri reader rate", d);
stat_print_split_read_rate__________(diskbuf, batch_bytes_lowpri[d], batch_initiated_reads_lowpri[d], batch_time_lowpri[d]);
			}
			
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d lowpri bytes read", d);
stat_print_split____________________(diskbuf, batch_bytes_lowpri[d]);
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d lowpri time", d);
stat_print_split_read_time__________(diskbuf, batch_time_lowpri[d]);
		}
	}
	
	/* extents */
	print_fp("");
stat_print_unknown__________________("mounts in cache", total_mounts);
stat_print_split____________________("extents in cache", total_extents);
	if (total_stat(extents_clipped) > 0 && total_stat(total_extents) > 0) {
stat_print_split_percent____________(" extents clipped", extents_clipped, total_extents);
	}
stat_print_split____________________("extent lookups", extent_lookups);
	if (total_stat(extent_lookups) > 0) {
stat_print_split_percent____________(" extent hits", extent_hits, extent_lookups);
		if (total_stat(extent_hits) > 0) {
stat_print_split_percent____________("  multiple extent hits", hit_multiple, extent_hits);
stat_print_split_percent____________("  hits aborted", hit_aborted, extent_hits);
stat_print_split_percent____________("  hits missing blocks", hit_blkmissing, extent_hits);
stat_print_split_percent____________("  hits stolen", hit_stolen, extent_hits);
stat_print_split_percent____________("  hits failed", hit_failure, extent_hits);
		}
	}

	/* history */
	print_fp("");
stat_print_unknown_machabs__________("kext load timestamp", load_timestamp);
	if (ss->userspace_statistics.ssup_launch_timestamp > 0) {
stat_print_machabs_manual___________("BCC launch timestamp", ss->userspace_statistics.ssup_launch_timestamp);
	}
stat_print_unknown_machabs__________("kext start timestamp", start_timestamp);
	if (ss->userspace_statistics.ssup_launch_timestamp > 0) {
stat_print_machabs_manual___________("total setup time", unknown_stat(start_timestamp) - ss->userspace_statistics.ssup_launch_timestamp);
		if (unknown_stat(start_timestamp) > ss->userspace_statistics.ssup_launch_timestamp) {
stat_print_machabs_manual___________(" BCC launch time", ss->userspace_statistics.ssup_oid_timestamp - ss->userspace_statistics.ssup_launch_timestamp);
stat_print_machabs_manual___________(" oid lookup time", unknown_stat(bc_start_timestamp) - ss->userspace_statistics.ssup_oid_timestamp);
stat_print_machabs_manual___________(" kext init time", unknown_stat(start_timestamp) - unknown_stat(bc_start_timestamp));
		}
	}
	
	if (unknown_stat(history_time) > 0 || total_stat(history_entries) == 0) {
stat_print_unknown_msec_____________("history active time", history_time);
		if (unknown_stat(history_time) > 0) {
stat_print_split_readwrite_tps______("read/write IO rate", history_reads, history_writes, history_time);
		}
	} else {
print_string________________________("history active time", "(still active)");
	}
stat_print_split____________________("history entries", history_entries);
stat_print_split____________________("history bytes", history_bytes);
stat_print_unknown__________________("history mounts", history_mounts);
stat_print_unknown__________________("unidentifiable mounts", history_mount_no_uuid);
stat_print_unknown__________________("mounts without blocksize", history_mount_no_blocksize);
	if (total_stat(history_unknown) > 0) {
stat_print_split____________________("history unknown calls", history_unknown);
stat_print_split____________________("history unknown bytes", history_unknown_bytes);
	}
	if (total_stat(history_no_blocksize) > 0) {
stat_print_split____________________("history 0 blocksize", history_no_blocksize);
stat_print_split____________________("history 0 blocksize bytes", history_no_blocksize_bytes);
	}
stat_print_unknown__________________("number history recordings", history_num_recordings);

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
BC_print_playlist(const struct BC_playlist *pc)
{
	struct BC_playlist_mount *pm;
	struct BC_playlist_entry *pe;
	int i;
	u_int64_t size = 0, size_lowpri = 0, size_batch[BC_MAXBATCHES] = {0};
	
	if (!pc) return EINVAL;
	
	LOG("Number of mounts: %d", pc->p_nmounts);
	LOG("Number of extents: %d", pc->p_nentries);
	LOG("Number of omaps: %d", pc->p_nomaps);
	
	for (i = 0; i < pc->p_nmounts; i++) {
		pm = pc->p_mounts + i;
		LOG("Mount %s flags 0x%x, %5d entries %5d omaps",
			 uuid_string(pm->pm_uuid), pm->pm_fs_flags, pm->pm_nentries, pm->pm_nomaps);
	}
	
	/*
	 * Print entries in source or "human-readable" format.
	 */
	LOG("Extents:");
	LOG("Mount                                Offset       Length Batch Flags Crypto offset");
	for (i = 0; i < pc->p_nentries; i++) {
		pe = pc->p_entries + i;
		LOG("%s "playlist_entry_format_str,
			 uuid_string(pc->p_mounts[pe->pe_mount_idx].pm_uuid), playlist_entry_format_args(pe));
		if (pe->pe_flags & BC_PE_LOWPRIORITY) {
			size_lowpri += pe->pe_length;
		} else {
			size += pe->pe_length;
			size_batch[pe->pe_batch] += pe->pe_length;
		}
	}
	
	LOG("omaps:");
	LOG("Mount                                oid          oxid");
	for (i = 0; i < pc->p_nomaps; i++) {
		struct BC_playlist_omap *po = pc->p_omaps + i;
		LOG("%s %#-12llx %#-8llx",
			 uuid_string(pc->p_mounts[po->po_mount_idx].pm_uuid), po->po_omap.otr_oid, po->po_omap.otr_oxid);
	}
	
	LOG("%12llu bytes", size);
	LOG("%12llu low-priority bytes", size_lowpri);
	for (i = 0; i < BC_MAXBATCHES; i++) {
		if (size_batch[i] != 0) {
			LOG("%12llu bytes batch %d", size_batch[i], i);
		}
	}
	
	return 0;
}


int
BC_print_history(char *fname, const struct BC_history *hc)
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
		fprintf(fp, "%s %#-12llx %#-8llx %#-12llx %5u%s%s\n",
				uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_uuid),
				hc->h_entries[i].he_offset, hc->h_entries[i].he_length,
				hc->h_entries[i].he_crypto_offset,
				hc->h_entries[i].he_pid,
				hc->h_entries[i].he_flags & BC_HE_HIT	 ? " hit"	 :
				hc->h_entries[i].he_flags & BC_HE_WRITE  ? " write"  :
				hc->h_entries[i].he_flags & BC_HE_TAG	 ? " tag"	 : " miss",
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
		LOG_ERRNO("could not insert prefetch tag");
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
