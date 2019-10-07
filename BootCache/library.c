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
#include <os/lock.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/storage/IOMedia.h>
#include <APFS/APFS.h>

#include <notify.h>

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

#define playlist_entry_withmount_format_str "%#-12llx %#-8llx %d 0x%x %#llx mount %d"
#define playlist_entry_withmount_format_args(pe) (pe)->pe_offset, (pe)->pe_length, (pe)->pe_batch, (pe)->pe_flags, (pe)->pe_crypto_offset, (pe)->pe_mount_idx

#define ASSERT(check) \
do { \
	if (__builtin_expect(!(check), 0)) { \
		LOG("Assertion failed: '%s'", # check); \
		if (bc_log_stream) fflush(bc_log_stream); \
		os_assert(check); \
	} \
} while (0)

#define ASSERTF(check, fmt, args...) \
do { \
	if (__builtin_expect(!(check), 0)) { \
		LOG("Assertion failed: '%s' "fmt, # check, ## args); \
		if (bc_log_stream) fflush(bc_log_stream); \
		os_assert(check); \
	} \
} while (0)

/* Copied from APFS headers to keep track of optimization statistics */
#ifndef FUSION_TIER2_DEVICE_BYTE_ADDR
#define FUSION_TIER2_DEVICE_BYTE_ADDR 0x4000000000000000ULL
#endif

static void clear_apfs_mount_cache(void);
static CFDictionaryRef copy_apfs_mount_cache(void);
static int apfs_mount_for_uuid(const uuid_t uuid, char** mountname_out, char** bsddisk_out); // returns 0 on success, non-0 and sets errno on failure. mountname and bsddisk must be freed
static void get_volume_uuid(const char* volume, uuid_t uuid_out);
static int apfs_flags_for_volume(const char* bsddisk, uint* fs_flags_out, uuid_t* container_uuid_out);

static int BC_start_omap_recording_for_all_mounts(void);
static int BC_tag_omap_recording_for_all_mounts(uint8_t batchNum);
static int BC_stop_omap_recording_for_all_mounts(void);
static int BC_fetch_omaps_for_all_mounts(struct BC_omap_history **);

static int BC_sort_and_coalesce_playlist_internal(struct BC_playlist *pc);

static int _BC_start(const struct BC_playlist *pc, bool record);

static int _BC_reset(void);



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
		memmove(npc->p_entries, pc->p_entries, sizeof(*pc->p_entries) * pc->p_nentries);

	if (pc->p_nmounts > 0)
		memmove(npc->p_mounts, pc->p_mounts, sizeof(*pc->p_mounts) * pc->p_nmounts);

	if (pc->p_nomaps > 0)
		memmove(npc->p_omaps, pc->p_omaps, sizeof(*pc->p_omaps) * pc->p_nomaps);

	return npc;
}

struct BC_history *
BC_copy_history(const struct BC_history *hc)
{
	if (NULL == hc) {
		return NULL;
	}
	
	struct BC_history* copy_hc = calloc(1, sizeof(*copy_hc));

	copy_hc->h_nentries = hc->h_nentries;
	if (copy_hc->h_nentries > 0) {
		size_t entriesSize = copy_hc->h_nentries * sizeof(*copy_hc->h_entries);
		copy_hc->h_entries = malloc(entriesSize);
		memmove(copy_hc->h_entries, hc->h_entries, entriesSize);
	} else {
		copy_hc->h_entries = NULL;
	}

	copy_hc->h_nmounts = hc->h_nmounts;
	if (copy_hc->h_nmounts > 0) {
		size_t mountsSize = copy_hc->h_nmounts * sizeof(*copy_hc->h_mounts);
		copy_hc->h_mounts = malloc(mountsSize);
		memmove(copy_hc->h_mounts, hc->h_mounts, mountsSize);
	} else {
		copy_hc->h_mounts = NULL;
	}
	
	return copy_hc;
}

void
BC_merge_history(struct BC_history *ha, const struct BC_history *hb)
{
	if (NULL == ha || NULL == hb) {
		return;
	}
	
	int* b_mount_idx_to_a_mount_idx = calloc(sizeof(*b_mount_idx_to_a_mount_idx), hb->h_nmounts);
	bool* b_mount_idx_to_a_mount_idx_valid = calloc(sizeof(*b_mount_idx_to_a_mount_idx_valid), hb->h_nmounts);

	for (u_int16_t b_mount_idx = 0; b_mount_idx < hb->h_nmounts; b_mount_idx++) {
		struct BC_history_mount* hmb = hb->h_mounts + b_mount_idx;

		u_int16_t a_mount_idx;
		for (a_mount_idx = 0; a_mount_idx < ha->h_nmounts; a_mount_idx++) {
			struct BC_history_mount* hma = ha->h_mounts + a_mount_idx;
			
			if (0 == uuid_compare(hma->hm_uuid, hmb->hm_uuid)) {
				ASSERT(0 == uuid_compare(hmb->hm_group_uuid, hma->hm_group_uuid));
				break;
			}
		}
		
		
		bool can_merge_mount = true;
		if (a_mount_idx >= ha->h_nmounts) {
			ASSERT(a_mount_idx == ha->h_nmounts);
			ha->h_nmounts++;
			ha->h_mounts = realloc(ha->h_mounts, sizeof(*ha->h_mounts) * ha->h_nmounts);
			ASSERT(ha->h_mounts);
			
			struct BC_history_mount* hma = ha->h_mounts + a_mount_idx;
			hma->hm_fs_flags = hmb->hm_fs_flags;
			uuid_copy(hma->hm_uuid, hmb->hm_uuid);
			uuid_copy(hma->hm_group_uuid, hmb->hm_group_uuid);
			hma->hm_nentries = 0;
		} else {
			if (hmb->hm_fs_flags != ha->h_mounts[a_mount_idx].hm_fs_flags) {
				// Someone turned off/on encryption, or encryption rolling completed, can't merge mounts, so just give up
				LOG("Unable to merge history for %s since flags changed: %#x -> %#x", uuid_string(hmb->hm_uuid), ha->h_mounts[a_mount_idx].hm_fs_flags, hmb->hm_fs_flags);
				can_merge_mount = false;
			}
		}
		
		b_mount_idx_to_a_mount_idx[b_mount_idx] = a_mount_idx;
		b_mount_idx_to_a_mount_idx_valid[b_mount_idx] = can_merge_mount;
	}
	
	uint ha_orig_nentries = 1 + ha->h_nentries; // + 1 for tag
	ha->h_nentries += 1 + hb->h_nentries; // + 1 for tag
	ha->h_entries = realloc(ha->h_entries, sizeof(*ha->h_entries) * ha->h_nentries);
	ASSERT(ha->h_nentries);
	
	// Create a tag to separate out the histories' batches
	struct BC_history_entry* tag = ha->h_entries + ha_orig_nentries - 1;
	bzero(tag, sizeof(*tag));
	tag->he_flags |= BC_HE_TAG;
	
	memmove(ha->h_entries + ha_orig_nentries, hb->h_entries, sizeof(*hb->h_entries) * hb->h_nentries);
	for (uint i = 0; i < hb->h_nentries; i++) {
		struct BC_history_entry* he = ha->h_entries + ha_orig_nentries + i;
		
		u_int16_t b_mount_index = he->he_mount_idx;
		u_int16_t a_mount_index = b_mount_idx_to_a_mount_idx[b_mount_index];
		
		// We need to keep our nentries accurate whether we can merge or not, but if we cannot merge then we mark the entry as length 0
		
		he->he_mount_idx = a_mount_index;
		ha->h_mounts[a_mount_index].hm_nentries++;
		
		if (!b_mount_idx_to_a_mount_idx_valid[b_mount_index]) {
			he->he_length = 0;
		}
	}
	
	free(b_mount_idx_to_a_mount_idx);
	free(b_mount_idx_to_a_mount_idx_valid);
}

struct BC_omap_history *
BC_copy_omap_history(const struct BC_omap_history * oh)
{
	if (!oh) {
		return NULL;
	}
	
	struct BC_omap_history* copy_oh = calloc(1, sizeof(*copy_oh));
	
	copy_oh->oh_nmounts = oh->oh_nmounts;
	if (copy_oh->oh_nmounts > 0) {
		size_t mountsSize = copy_oh->oh_nmounts * sizeof(*copy_oh->oh_mounts);
		copy_oh->oh_mounts = malloc(mountsSize);
		memmove(copy_oh->oh_mounts, oh->oh_mounts, mountsSize);
		
		// Fixup ohm_omaps
		for (int i = 0; i < copy_oh->oh_nmounts; i++) {
			struct BC_omap_history_mount* ohm = oh->oh_mounts + i;
			struct BC_omap_history_mount* copy_ohm = copy_oh->oh_mounts + i;
			
			size_t omapsSize = copy_ohm->ohm_nomaps * sizeof(*copy_ohm->ohm_omaps);
			copy_ohm->ohm_omaps = malloc(omapsSize);
			memmove(copy_ohm->ohm_omaps, ohm->ohm_omaps, omapsSize);
		}
		
	} else {
		copy_oh->oh_mounts = NULL;
	}
	
	return copy_oh;
}

void
BC_merge_omap_history(struct BC_omap_history* oha, const struct BC_omap_history* ohb)
{
	if (NULL == oha || NULL == ohb) {
		return;
	}
	
	for (uint b_mount_idx = 0; b_mount_idx < ohb->oh_nmounts; b_mount_idx++) {
		struct BC_omap_history_mount* ohmb = ohb->oh_mounts + b_mount_idx;
		
		uint a_mount_idx;
		for (a_mount_idx = 0; a_mount_idx < oha->oh_nmounts; a_mount_idx++) {
			struct BC_omap_history_mount* ohma = oha->oh_mounts + a_mount_idx;
			
			if (0 == uuid_compare(ohma->ohm_uuid, ohmb->ohm_uuid)) {
				break;
			}
		}
		
		if (a_mount_idx >= oha->oh_nmounts) {
			ASSERT(a_mount_idx == oha->oh_nmounts);
			oha->oh_nmounts++;
			oha->oh_mounts = realloc(oha->oh_mounts, sizeof(*oha->oh_mounts) * oha->oh_nmounts);
			ASSERT(oha->oh_mounts);
			
			struct BC_omap_history_mount* ohma = oha->oh_mounts + a_mount_idx;
			uuid_copy(ohma->ohm_uuid, ohmb->ohm_uuid);
			ohma->ohm_nomaps = 0;
			ohma->ohm_omaps = NULL;
		}
		
		struct BC_omap_history_mount* ohma = oha->oh_mounts + a_mount_idx;

		
		uint ohma_orig_nentries = ohma->ohm_nomaps;
		ohma->ohm_nomaps += ohmb->ohm_nomaps;
		ohma->ohm_omaps = realloc(ohma->ohm_omaps, sizeof(*ohma->ohm_omaps) * ohma->ohm_nomaps);
		ASSERT(ohma->ohm_omaps);
		memmove(ohma->ohm_omaps + ohma_orig_nentries, ohmb->ohm_omaps, sizeof(*ohma->ohm_omaps) * ohmb->ohm_nomaps);
		
		if (ohma_orig_nentries > 0) {
			uint8_t a_max_batch  = 0;
			for (uint i = 0; i < ohma->ohm_nomaps; i++) {
				apfs_omap_track_record_v2_t* omapa = ohma->ohm_omaps + i;
				if (a_max_batch < omapa->otr_batch) {
					a_max_batch = omapa->otr_batch;
				}
			}
			for (uint i = 0; i < ohmb->ohm_nomaps; i++) {
				apfs_omap_track_record_v2_t* omapb = ohmb->ohm_omaps + i;
				omapb->otr_batch += a_max_batch + 1;
			}
		}
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
	
#ifdef DEBUG
	DLOG("Read in playlist %s:", pfname);
	BC_print_playlist(pc, false);
#endif

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

#ifdef DEBUG
	DLOG("Wrote playlist %s:", pfname);
	BC_print_playlist(pc, false);
#endif

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
	if (!pb || pb->p_nmounts == 0 || (pb->p_nentries == 0 && pb->p_nomaps == 0)) {
		return 0;
	}
	
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
			memmove(pm, new_mount, sizeof(*pm));
		} else {
			
			// We found a matching mount in pa's existing list so we update the counts
			pa->p_mounts[mount_idx_a].pm_nentries += pb->p_mounts[mount_idx_b].pm_nentries;
			pa->p_mounts[mount_idx_a].pm_nomaps += pb->p_mounts[mount_idx_b].pm_nomaps;
		}
	}

	/* merge the entries list */
	if (pb->p_nentries > 0) {
		if ((pa->p_entries = reallocf(pa->p_entries, sizeof(*pa->p_entries) * (pa->p_nentries + pb->p_nentries))) == NULL)
			return(errno ?: ENOMEM);
		memmove(pa->p_entries + pa->p_nentries, pb->p_entries, pb->p_nentries * sizeof(*pb->p_entries));
		
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
	}

	
	/* merge the omaps list */
	if (pb->p_nomaps > 0) {
		if ((pa->p_omaps = reallocf(pa->p_omaps, (pa->p_nomaps + pb->p_nomaps) * sizeof(*pa->p_omaps))) == NULL)
			return(errno ?: ENOMEM);
		memmove(pa->p_omaps + pa->p_nomaps, pb->p_omaps, pb->p_nomaps * sizeof(*pb->p_omaps));
		
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
	}


	BC_sort_and_coalesce_playlist_internal(pa);

	int err = BC_verify_playlist(pa);
	
#ifdef DEBUG
	if (err == 0) {
		DLOG("Merged into playlist:");
		BC_print_playlist(pa, false);
	}
#endif
	
	return err;
}

static int
mount_sort_order(const struct BC_playlist_mount* first_mount, const struct BC_playlist_mount* second_mount)
{
	if (first_mount == second_mount) {
		return 0;
	}
	
	if (uuid_is_null(first_mount->pm_group_uuid)) {
		if (uuid_is_null(second_mount->pm_group_uuid)) {
			if (first_mount < second_mount) {
				return -1;
			} else {
				return 1;
			}
		} else {
			return 1;
		}
	} else {
		if (uuid_is_null(second_mount->pm_group_uuid)) {
			return -1;
		} else {
			return uuid_compare(first_mount->pm_group_uuid, second_mount->pm_group_uuid);
		}
	}
	
	return 0;
}
/*
 * Sort a playlist.
 */
static int
compare_playlist_entry(const struct BC_playlist *pc, const void *vfirst, const void *vsecond)
{
	const struct BC_playlist_entry	*first, *second;

	first = (const struct BC_playlist_entry *)vfirst;
	second = (const struct BC_playlist_entry *)vsecond;
	
	// Sort by volume first, then by logical block address
	int mount_order = mount_sort_order(pc->p_mounts + first->pe_mount_idx, pc->p_mounts + second->pe_mount_idx);
	if (mount_order != 0) {
		return mount_order;
	}

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
		qsort_b((void *)pc->p_entries, pc->p_nentries, sizeof(*pc->p_entries), ^int(const void *vfirst, const void *vsecond) {
			return compare_playlist_entry(pc, vfirst, vsecond);
		});
	
	if (pc->p_nomaps > 0) {
		qsort_b(pc->p_omaps, pc->p_nomaps, sizeof(*pc->p_omaps), ^int(const void *a, const void *b) {
			const struct BC_playlist_omap* firstOmap = a;
			const struct BC_playlist_omap* secondOmap = b;
			
			if (firstOmap->po_mount_idx < secondOmap->po_mount_idx) {
				return -1;
			}
			if (firstOmap->po_mount_idx > secondOmap->po_mount_idx) {
				return 1;
			}
			
			if (firstOmap->po_omap.otr_oid < secondOmap->po_omap.otr_oid) {
				return -1;
			}
			if (firstOmap->po_omap.otr_oid > secondOmap->po_omap.otr_oid) {
				return 1;
			}

			if (firstOmap->po_omap.otr_oxid < secondOmap->po_omap.otr_oxid) {
				return -1;
			}
			if (firstOmap->po_omap.otr_oxid > secondOmap->po_omap.otr_oxid) {
				return 1;
			}
			
			if (firstOmap->po_omap.otr_batch < secondOmap->po_omap.otr_batch) {
				return -1;
			}
			if (firstOmap->po_omap.otr_batch > secondOmap->po_omap.otr_batch) {
				return 1;
			}

			return 0;
		});
	}
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
	BC_COALESCE_HIGHER_REMOVE,   // Higher to be removed and lower unmodified
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
		DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_NOT_ADJACENT", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
		return BC_COALESCE_NOT_ADJACENT;
	}
	
	if (lower_pe->pe_length == 0 || higher_pe->pe_length == 0) {
		DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_NO_CHANGE", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
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
					DLOG("Entry "playlist_entry_withmount_format_str" mismatch with "playlist_entry_withmount_format_str, playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
					DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_LOWER_REMOVE", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
					return BC_COALESCE_LOWER_REMOVE;
				} else if (higher_pe->pe_offset == higher_pe->pe_crypto_offset) {
					DLOG("Entry "playlist_entry_withmount_format_str" mismatch with "playlist_entry_withmount_format_str, playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
					DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_HIGHER_REMOVE", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
					return BC_COALESCE_HIGHER_REMOVE;
				} else {
					// Both entries have crypto offset != offset, but the deltas don't match?
					// That shouldn't happen... remove lower_pe, I guess, so in the case where it actually is wrong it doesn't knock out a bunch of correct higher extents.
					LOG("Entry "playlist_entry_withmount_format_str" mismatch with "playlist_entry_withmount_format_str, playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
					
					// Because we're unsure which entry is wrong but return BC_COALESCE_LOWER_REMOVE here, BC_COALESCE_LOWER_REMOVE doesn't necessarily mean lower_pe is actually wrong. We are sure that when we return BC_COALESCE_HIGHER_REMOVE that the higher_pe is wrong, so make sure we prefer to do BC_COALESCE_HIGHER_REMOVE before BC_COALESCE_LOWER_REMOVE
					static_assert(BC_COALESCE_HIGHER_REMOVE > BC_COALESCE_LOWER_REMOVE, "");
					
					DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_LOWER_REMOVE", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
					return BC_COALESCE_LOWER_REMOVE;
				}
			} else {
				// Adjacent. Not a problem, but they cannot be combined
				DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_NO_CHANGE due to adjacent nonmatching crypto", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
				return BC_COALESCE_NO_CHANGE;
			}
		} else {
			// Crypto match, go ahead and fall-through to coalesce like normal
		}
	}
	
#define MAX_HIGHPRI_ENTRY_GROWTH_DUE_TO_LOWPRI_COALESCING (128*1024) // 128KB
	
	// Length we would be if we merged
	u_int64_t coalesced_length = MAX((lower_pe->pe_offset + lower_pe->pe_length), (higher_pe->pe_offset + higher_pe->pe_length)) - lower_pe->pe_offset;
	
	// Check for cases where we want to merge the entries together
	if ((lower_pe->pe_flags & BC_PE_LOWPRIORITY) && (higher_pe->pe_flags & BC_PE_LOWPRIORITY)) {
		// Both low priority
		DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_MERGE both low priority", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
		return BC_COALESCE_MERGE;
	} else if (!(lower_pe->pe_flags & BC_PE_LOWPRIORITY) && !(higher_pe->pe_flags & BC_PE_LOWPRIORITY)) {
		// Both high prioirty
		DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_MERGE both high priority", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
		return BC_COALESCE_MERGE;
	} else if (lower_pe->pe_flags & BC_PE_LOWPRIORITY) {
		// lower_pe is low priority, higher_pe is high priority
		
		if (coalesced_length - higher_pe->pe_length < MAX_HIGHPRI_ENTRY_GROWTH_DUE_TO_LOWPRI_COALESCING) {
			// Adding lower_pe to higher_pe will increase higher's size by less than the max growth size
			DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_MERGE higher high priority", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
			return BC_COALESCE_MERGE;
		} else {
			// Adding lower_pe (low priority) to higher_pe (high priority) will increase higher's size by more than the max growth size, don't coalesce them
		}
	} else {
		// lower_pe is high priority, higher_pe is low priority
		
		if (coalesced_length - lower_pe->pe_length < MAX_HIGHPRI_ENTRY_GROWTH_DUE_TO_LOWPRI_COALESCING) {
			// Adding higher_pe to lower_pe will increase lower's size by less than the max growth size
			DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_MERGE lower high priority", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
			return BC_COALESCE_MERGE;
		} else {
			// Adding higher_pe (low priority) to lower_pe (high priority) will increase lower's size by more than the max growth size, don't coalesce them
		}
	}

	// If we've gotten here, then we're not merging. Check for overlap and decide how to deal with it
	// Further, because of the merging logic above, we know either lower_pe or higher_pe is low priority, but not both
	
	if (lower_pe->pe_offset + lower_pe->pe_length == higher_pe->pe_offset) {
		// No overlap
		DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_NO_CHANGE adjacent, not overlapping", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
		return BC_COALESCE_NO_CHANGE;
	}
	
	if (higher_pe->pe_flags & BC_PE_LOWPRIORITY) {
		/* lower_pe is high priority and higher_pe is low priority, subtract intersection from higher_pe (because of ordering by offset, we know higher_pe cannot encompass lower_pe thus we don't need to split higher_pe into two entries) */
		DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_HIGHER_SHRINKS", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
		return BC_COALESCE_HIGHER_SHRINKS;
	} else {
		/* lower_pe is low priority, higher_pe is high priority, subtract intersection from lower_pe */
		if ((lower_pe->pe_offset + lower_pe->pe_length) > (higher_pe->pe_offset + higher_pe->pe_length)) {
			/* lower_pe encompasses higher_pe. In order to subtract the intersection we need to split lower_pe into two entries: one below higher_pe and one below higher_pe */
			DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_LOWER_SPLIT", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
			return BC_COALESCE_LOWER_SPLIT;
		} else {
			DLOG_COALESCING(playlist_entry_withmount_format_str" vs "playlist_entry_withmount_format_str" -> BC_COALESCE_LOWER_SHRINKS", playlist_entry_withmount_format_args(lower_pe), playlist_entry_withmount_format_args(higher_pe));
			return BC_COALESCE_LOWER_SHRINKS;
		}
	}

}

int BC_sort_and_coalesce_playlist(struct BC_playlist* pc) {
	assert(pc);
	
	int error = BC_sort_and_coalesce_playlist_internal(pc);
	if (error) {
		return error;
	}
	
	return BC_verify_playlist(pc);
}

/*
 * Coalesece a sorted playlist into the smallest set of contiguous
 * entries.  Sets the new size of the playlist and realloc's the buffer.
 */
static int
BC_sort_and_coalesce_playlist_internal(struct BC_playlist *pc)
{
	// First, make sure the extents are sorted
	BC_sort_playlist(pc);

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
	 * ^: Entries that are in the same ordering group, but not in the same mount, so cannot be coalesced with S
	 *
	 *            --> higher offset -->
	 * CCCCCCCCCCD--------------S**^|^||*B^*||^^*|*^+++++++++
	 *
	 * In each turn of the scan_idx loop below, we coalesce S with all overlapping/adjacent entry (the entry immediately after it), then move S to D.
	 *
	 * In each turn of the do-while loop inside the scan_idx loop, we pick an adjacent/overlapping entry, B, and coalece it with S. Afterwards, S and B must not overlap and the order of the entire list must be preserved. To keep the ordering, all adjacent/overlapping entries that won't change their offset and will change S's length when coalesced with S, are picked before any that will change their offset based on S's length (this is what the bestmatch loop does).
	 *
	 * With groupings, its possible for entries from different mounts within the same grouping to become out of proper sort order, so we do a sort pass at the end once we're done. This doesn't matter while we're coalescing because we will never consider coalescing two entries from different volumes.
	 */
	
	// There are several possibilities when coalescing two entries (see coalesce_style_t).
	// We want to make sure we don't break the invariant that all (non-0 length) entries are in sorted order.
	// No methods change scan_pe's offset, but BC_COALESCE_HIGHER_SHRINKS may raise the lookahead_pe's offset.
	// So, we want to make sure we do any BC_COALESCE_HIGHER_SHRINKS-style coalescing after anything else that may change the lower's endpoint. That way, all entries that overlap with scan_pe left after the other coalescing styles will be changed to have the same offset, thus ordering is preserved
	static_assert(BC_COALESCE_HIGHER_SHRINKS < BC_COALESCE_LOWER_SPLIT, "");
	static_assert(BC_COALESCE_HIGHER_SHRINKS < BC_COALESCE_LOWER_SHRINKS, "");
	static_assert(BC_COALESCE_HIGHER_SHRINKS < BC_COALESCE_MERGE, "");
	
#ifdef DEBUG
#if COALESCING_DEBUG_LOGGING_ENABLED
	uint nentries_original = pc->p_nentries;
#endif
#endif
	
#if COALESCING_DEBUG_LOGGING_ENABLED
//    DLOG_COALESCING("Coalescing %d entries", pc->p_nentries);
//    BC_print_playlist(pc, true);
#else
#ifdef DEBUG
	DLOG("Coalescing");
	BC_print_playlist(pc, false);
#endif
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

		bool is_encrypted = (pc->p_mounts[scan_pe->pe_mount_idx].pm_fs_flags & BC_FS_APFS_ENCRYPTED);
		
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
					
					if (0 != mount_sort_order(pc->p_mounts + scan_pe->pe_mount_idx, pc->p_mounts + lookahead_pe->pe_mount_idx)) {
						/* Further, since we sort by mount and then offset we know nothing past lookahead_pe will match by mount either, so stop looking ahead */
						
						DLOG_COALESCING("%d@"playlist_entry_withmount_format_str" mount %s group %s vs %d@"playlist_entry_withmount_format_str" mount %s group %s: not grouped together", scan_idx, playlist_entry_withmount_format_args(scan_pe), uuid_string(pc->p_mounts[scan_pe->pe_mount_idx].pm_uuid), uuid_string(pc->p_mounts[scan_pe->pe_mount_idx].pm_group_uuid), lookahead_idx, playlist_entry_withmount_format_args(lookahead_pe), uuid_string(pc->p_mounts[lookahead_pe->pe_mount_idx].pm_uuid), uuid_string(pc->p_mounts[lookahead_pe->pe_mount_idx].pm_group_uuid));
						
						break;
					} else {
						/* lookahead_pe is in a different volume so can't be coalesced with scan_pe, but their volumes are grouped together for sorting purposes, so we need to look further past lookahead_pe for entries that are in scan_pe's volume that we may want to coalesce */
						
						DLOG_COALESCING("%d@"playlist_entry_withmount_format_str" mount %s group %s vs %d@"playlist_entry_withmount_format_str" mount %s group %s: not same volume, but grouped together", scan_idx, playlist_entry_withmount_format_args(scan_pe), uuid_string(pc->p_mounts[scan_pe->pe_mount_idx].pm_uuid), uuid_string(pc->p_mounts[scan_pe->pe_mount_idx].pm_group_uuid), lookahead_idx, playlist_entry_withmount_format_args(lookahead_pe), uuid_string(pc->p_mounts[lookahead_pe->pe_mount_idx].pm_uuid), uuid_string(pc->p_mounts[lookahead_pe->pe_mount_idx].pm_group_uuid));
						
#if COALESCING_DEBUG_LOGGING_ENABLED
						did_anything = true;
#endif

						/* Cannot fast-path here and break based on non-adjacency. Extents in different mounts may become out of block address order due to HIGHER_SHRINKS merges, therefore we _cannot_ assume that subsequent extents (after lookahead) in this mount (scan_pe->pe_mount_idx) have an equal or higer block address than lookahead's
						 */
						continue;
					}
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
					DLOG_COALESCING("BC_COALESCE_HIGHER_REMOVE: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

					bestmatch_pe->pe_length = 0;
					
					DLOG_COALESCING("                        -> %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

				}
					break;
					

				case BC_COALESCE_LOWER_REMOVE:
				{
					DLOG_COALESCING("BC_COALESCE_LOWER_REMOVE: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

					scan_pe->pe_length = 0;
					
					DLOG_COALESCING("                       -> %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));
				}
					break;
					

				case BC_COALESCE_MERGE:
				{
					/* coalesce the two entries together */
					
					DLOG_COALESCING("BC_COALESCE_MERGE: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

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
					
					DLOG_COALESCING("                -> %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));
				}
					break;
					
					
				case BC_COALESCE_LOWER_SHRINKS:
				{
					/* Shorten scan_pe to the range below bestmatch_pe */

					DLOG_COALESCING("BC_COALESCE_LOWER_SHRINKS: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

					scan_pe->pe_length = bestmatch_pe->pe_offset - scan_pe->pe_offset;
					
					DLOG_COALESCING("                        -> %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

				}
					break;
					
					
				case BC_COALESCE_LOWER_SPLIT:
				{
					/* scan_pe encompasses bestmatch_pe, so we need to split it into two halves: scan_pe will be the range below bestmatch_pe and we need a new entry for the range above bestmatch_pe */

					DLOG_COALESCING("BC_COALESCE_LOWER_SPLIT: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

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
						
						int compar = compare_playlist_entry(pc, &new_entry, pc->p_entries + new_entry_idx);
						DLOG_COALESCING("compar %d: "playlist_entry_withmount_format_str" vs %d@"playlist_entry_withmount_format_str, compar, playlist_entry_withmount_format_args(&new_entry), new_entry_idx, playlist_entry_withmount_format_args(pc->p_entries + new_entry_idx));
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
					
					DLOG_COALESCING("                      -> %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str", nentries now %d", scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe), new_entry_idx, playlist_entry_withmount_format_args(pc->p_entries + new_entry_idx), pc->p_nentries);

				}
					break;
					
					
				case BC_COALESCE_HIGHER_SHRINKS:
				{
					DLOG_COALESCING("BC_COALESCE_HIGHER_SHRINKS: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));

					if ((scan_pe->pe_offset + scan_pe->pe_length) > (bestmatch_pe->pe_offset + bestmatch_pe->pe_length)) {
						// bestmatch entry is encompassed by scan_pe, so it shrinks to nothing
						bestmatch_pe->pe_length = 0;
					} else {
						
						u_int64_t end = (bestmatch_pe->pe_offset + bestmatch_pe->pe_length);
						
						bestmatch_pe->pe_offset = scan_pe->pe_offset + scan_pe->pe_length;
						bestmatch_pe->pe_length = end - bestmatch_pe->pe_offset;
						if (is_encrypted) {
							bestmatch_pe->pe_crypto_offset = scan_pe->pe_crypto_offset + scan_pe->pe_length;
						}
					}
					
					DLOG_COALESCING("                         -> %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));
				}
					break;
					
				case BC_COALESCE_NO_CHANGE:
				{
					// Nothing to do

					DLOG_COALESCING("BC_COALESCE_NO_CHANGE: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str, scan_idx, playlist_entry_withmount_format_args(scan_pe), bestmatch_idx, playlist_entry_withmount_format_args(bestmatch_pe));
				}
					break;
					
				case BC_COALESCE_NOT_ADJACENT:
				{
					// Nothing to do

#if COALESCING_DEBUG_LOGGING_ENABLED
					if (did_anything) {
						if (pc->p_nentries > lookahead_idx) {
							DLOG_COALESCING("BC_COALESCE_NOT_ADJACENT: %d@"playlist_entry_withmount_format_str" + %d@"playlist_entry_withmount_format_str" nentries is %d", scan_idx, playlist_entry_withmount_format_args(scan_pe), lookahead_idx, playlist_entry_withmount_format_args(pc->p_entries + lookahead_idx), pc->p_nentries);
						} else {
							DLOG_COALESCING("BC_COALESCE_NOT_ADJACENT: %d@"playlist_entry_withmount_format_str" at end", scan_idx, playlist_entry_withmount_format_args(scan_pe));
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
				DLOG_COALESCING("Saved %d@"playlist_entry_withmount_format_str, dest_idx, playlist_entry_withmount_format_args(pc->p_entries + dest_idx));
			}
#endif
			
			dest_idx++;
			pc->p_mounts[scan_pe->pe_mount_idx].pm_nentries++;
		}
	}
	
	// Set new size based on how many entries we ended up with
	pc->p_nentries = dest_idx;

	
	
	////// Coalesce omaps ///////
	
	/* clear the number of omaps per mount, to be recalculated as we're coalescing */
	for (int mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
		pc->p_mounts[mount_idx].pm_nomaps = 0;
	}

	// TODO: improve this N^2 algorithm
	dest_idx = 0;
	for (int scan_idx = 0; scan_idx < pc->p_nomaps; scan_idx++) {
		if (pc->p_omaps[scan_idx].po_omap.otr_oid == 0) {
			continue;
		}

		int lookahead_idx;
		for (lookahead_idx = scan_idx + 1; lookahead_idx < pc->p_nomaps; lookahead_idx++) {
			if (pc->p_omaps[scan_idx].po_mount_idx != pc->p_omaps[lookahead_idx].po_mount_idx) {
				// Don't combine omaps from different mounts
				break;
			}
			
			if (pc->p_omaps[scan_idx].po_omap.otr_oid != pc->p_omaps[lookahead_idx].po_omap.otr_oid ||
				pc->p_omaps[scan_idx].po_omap.otr_oxid != pc->p_omaps[lookahead_idx].po_omap.otr_oxid) {
				break;
			}
			
			// These omaps match
			
			// Use scan_idx as the omap entry we're keeping. update the batch to the lower of the two
			if (pc->p_omaps[scan_idx].po_omap.otr_batch > pc->p_omaps[lookahead_idx].po_omap.otr_batch) {
				pc->p_omaps[scan_idx].po_omap.otr_batch = pc->p_omaps[lookahead_idx].po_omap.otr_batch;
			}
			
			// Mark lookahead_idx as empty
			pc->p_omaps[lookahead_idx].po_omap.otr_oid = 0;
			pc->p_omaps[lookahead_idx].po_omap.otr_oxid = 0;
		}
		
		if (dest_idx != scan_idx) {
			pc->p_omaps[dest_idx] = pc->p_omaps[scan_idx];
		}
		
		pc->p_mounts[pc->p_omaps[dest_idx].po_mount_idx].pm_nomaps++;
		dest_idx++;
	}

	pc->p_nomaps = dest_idx;
	
	
	// Entries in different mounts but in the same sort group may have moved out of order
	// Sort the entire playlist to handle any of these cases
	BC_sort_playlist(pc);
	
#if COALESCING_DEBUG_LOGGING_ENABLED
//DLOG_COALESCING("Coalesced to %d entries", pc->p_nentries);
//BC_print_playlist(pc, true);
#ifdef DEBUG
	if (pc->p_nentries != nentries_original) {
		DLOG("Coalesced %d entries into %d entries", nentries_original, pc->p_nentries);
	}
#endif
#else
#ifdef DEBUG
	DLOG("Coalesced");
	BC_print_playlist(pc, false);
#endif
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

static Boolean uuid_is_equal_cf_callback(const void* a, const void* b) {
	return (0 == uuid_compare(a, b));
}

static CFHashCode uuid_hash_cf_callback(const void* a) {
	return *(CFHashCode*)a;
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
	int* mount_entry_counts = NULL;
	int* mount_omap_counts = NULL;
	CFMutableDictionaryRef group_entry_last_idx_dict = NULL;
	
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
	
	// uuid_t
	CFDictionaryKeyCallBacks keyCallbacks = {
		.equal = &uuid_is_equal_cf_callback,
		.hash = &uuid_hash_cf_callback,
	};
	
	// long
	CFDictionaryValueCallBacks valueCallbacks = {};
	
	group_entry_last_idx_dict = CFDictionaryCreateMutable(NULL, pc->p_nmounts, &keyCallbacks, &valueCallbacks);
	mount_entry_counts = calloc(pc->p_nmounts, sizeof(int));
	
	uuid_t* (^group_uuid)(int index) = ^(int index) {
		if (!uuid_is_null(pc->p_mounts[index].pm_group_uuid)) {
			return &pc->p_mounts[index].pm_group_uuid;
		}
		return &pc->p_mounts[index].pm_uuid;
	};
	
	for (i = 0; i < pc->p_nmounts; i++) {
		CFDictionarySetValue(group_entry_last_idx_dict, group_uuid(i), (void*)-1);
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
		
		long group_entry_last_idx = (long)CFDictionaryGetValue(group_entry_last_idx_dict, group_uuid(pc->p_entries[i].pe_mount_idx));
		if (group_entry_last_idx >= 0 &&
			group_entry_last_idx != i - 1) {
			
			LOG("entry %d referenced mount %d (group %s), but previous entry that referenced that group was entry %ld", i, pc->p_entries[i].pe_mount_idx, uuid_string(*group_uuid(pc->p_entries[i].pe_mount_idx)), group_entry_last_idx);
			error = EINVAL;
			goto out;
		}
		CFDictionarySetValue(group_entry_last_idx_dict, group_uuid(pc->p_entries[i].pe_mount_idx), (void*)(long)i);
		
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
	if (group_entry_last_idx_dict)
		CFRelease(group_entry_last_idx_dict);
	
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
BC_set_userspace_oversize(const struct BC_userspace_oversize* oversize)
{
	struct BC_command bc;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_SET_USER_OVERSIZE;
	bc.bc_data1 = (uintptr_t) oversize;
	bc.bc_data1_size = (unsigned int) sizeof(*oversize);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	return(0);
}

/*
 * Set cache userspace statistics.
 */
int
BC_set_userspace_timestamps(const struct BC_userspace_timestamps *userspace_timestamps)
{
	struct BC_command bc;
	int error;

	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_SET_USER_TIMESTAMPS;
	bc.bc_data1 = (uintptr_t) userspace_timestamps;
	bc.bc_data1_size = (unsigned int) sizeof(*userspace_timestamps);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	return(0);
}

int
BC_set_userspace_fusion_optimization_stats(const struct BC_userspace_fusion_optimizations *userspace_fusion_optimizations)
{
	struct BC_command bc;
	int error;
	
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_SET_FUSION_OPTIMIZATION_STATS;
	bc.bc_data1 = (uintptr_t) userspace_fusion_optimizations;
	bc.bc_data1_size = (unsigned int) sizeof(*userspace_fusion_optimizations);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	return(0);
}

int
BC_set_userspace_hdd_optimization_stats(const struct BC_userspace_hdd_optimizations *userspace_hdd_optimizations)
{
	struct BC_command bc;
	int error;
	
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_SET_HDD_OPTIMIZATION_STATS;
	bc.bc_data1 = (uintptr_t) userspace_hdd_optimizations;
	bc.bc_data1_size = (unsigned int) sizeof(*userspace_hdd_optimizations);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	return(0);
}

int
BC_set_userspace_hdd_optimization_state(const struct BC_userspace_hdd_optimization_state *userspace_hdd_optimization_state)
{
	struct BC_command bc;
	int error;
	
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_SET_HDD_OPTIMIZATION_STATE;
	bc.bc_data1 = (uintptr_t) userspace_hdd_optimization_state;
	bc.bc_data1_size = (unsigned int) sizeof(*userspace_hdd_optimization_state);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		return(errno);
	}
	return(0);
}


static struct BC_playlist*
_BC_omap_tree_playlist(const struct BC_playlist* pc)
{
	struct BC_playlist* tree_playlist = NULL;
	
	for (int mount_idx = 0; mount_idx < pc->p_nmounts; mount_idx++) {
		const struct BC_playlist_mount* mount = pc->p_mounts + mount_idx;
		if (mount->pm_nomaps == 0 || !(mount->pm_fs_flags & BC_FS_APFS)) {
			continue;
		}
		
		char *mount_name = NULL;
		apfs_mount_for_uuid(mount->pm_uuid, &mount_name, NULL);
		if (!mount_name) {
			LOG_ERRNO("Unable to get omap tree for %s: no mount found", uuid_string(mount->pm_uuid));
			continue;
		}

		// Pick up 128K omaps at a time
#define omapcount (128*1024)
		
		// fetch all the leaf node extents
		apfs_omap_tree_leaf_extents_t otlx;
		memset(&otlx, 0, sizeof(otlx));
		otlx.otlx_extents = calloc(omapcount, sizeof(*otlx.otlx_extents));
		if (!otlx.otlx_extents) {
			LOG("failed to allocate buffer for %u omaps", omapcount);
			free(mount_name);
			continue;
		}
		
		DLOG("Requesting leaf omap extents for %s", mount_name);

		int tree_playlist_mount_idx = -1;
		do {
			// fetch (more) extents
			otlx.otlx_extent_count = omapcount;
			int rv = fsctl(mount_name, APFSIOC_OMAP_TREE_LEAF_EXTENTS, &otlx, 0);
			if (rv) {
				if (tree_playlist_mount_idx >= 0) {
					LOG_ERRNO("Requesting leaf omap extents for %s failed after %d omaps", mount_name, tree_playlist->p_mounts[tree_playlist_mount_idx].pm_nentries);
				} else {
					LOG_ERRNO("Requesting leaf omap extents for %s failed", mount_name);
				}
				break;
			}
			if (otlx.otlx_extent_count == 0 && tree_playlist_mount_idx < 0) {
				LOG("Requesting leaf omap extents for %s returned 0 extents", mount_name);
				break;
			}
			
			if (tree_playlist_mount_idx < 0) {
				if (!tree_playlist) {
					tree_playlist = BC_allocate_playlist(0, 0, 0);
					if (!tree_playlist) {
						LOG("Unable to allocate playlist for leaf omap extents");
						free(otlx.otlx_extents);
						return NULL;
					}
				}
				
				// These extents always go to the container, find the container's mount index in the playlist
				for (int i = 0; i < tree_playlist->p_nmounts; i++) {
					// pm_group_uuid is the container's UUID
					if (0 == uuid_compare(tree_playlist->p_mounts[i].pm_uuid, mount->pm_group_uuid)) {
						tree_playlist_mount_idx = i;
						break;
					}
				}
				
				if (tree_playlist_mount_idx < 0) {
					// Don't have the container in the tree playlist already, create it
					
					tree_playlist_mount_idx = tree_playlist->p_nmounts;
					tree_playlist->p_nmounts++;
					tree_playlist->p_mounts = reallocf(tree_playlist->p_mounts, tree_playlist->p_nmounts * sizeof(*tree_playlist->p_mounts));
					if (!tree_playlist->p_mounts) {
						LOG("Unable to realloc for %d mounts", tree_playlist->p_nmounts);
						PC_FREE_ZERO(tree_playlist);
						free(otlx.otlx_extents);
						return NULL;
					}
					
					uuid_copy(tree_playlist->p_mounts[0].pm_uuid, mount->pm_group_uuid);
					uuid_copy(tree_playlist->p_mounts[0].pm_group_uuid, mount->pm_group_uuid);
					tree_playlist->p_mounts[0].pm_fs_flags = BC_FS_APFS | BC_FS_APFS_CONTAINER | (mount->pm_fs_flags & BC_FS_APFS_FUSION);
					tree_playlist->p_mounts[0].pm_nentries = 0;
					tree_playlist->p_mounts[0].pm_nomaps = 0;
				}
			}
			
			if (otlx.otlx_extent_count > 0) {
				int newEntriesIndex = tree_playlist->p_nentries;
				tree_playlist->p_nentries += otlx.otlx_extent_count;
				tree_playlist->p_mounts[tree_playlist_mount_idx].pm_nentries += otlx.otlx_extent_count;
				tree_playlist->p_entries = reallocf(tree_playlist->p_entries, tree_playlist->p_nentries * sizeof(*tree_playlist->p_entries));
				if (!tree_playlist->p_entries) {
					LOG("Unable to realloc for %d entries", tree_playlist->p_nentries);
					PC_FREE_ZERO(tree_playlist);
					free(otlx.otlx_extents);
					return NULL;
				}
				
				for (int i = 0; i < otlx.otlx_extent_count; i++) {
					tree_playlist->p_entries[newEntriesIndex + i].pe_mount_idx = tree_playlist_mount_idx;
					tree_playlist->p_entries[newEntriesIndex + i].pe_offset = otlx.otlx_extents[i].otn_offset;
					tree_playlist->p_entries[newEntriesIndex + i].pe_length = otlx.otlx_extents[i].otn_length;
					tree_playlist->p_entries[newEntriesIndex + i].pe_crypto_offset = 0; // Containers are never encrypted
					tree_playlist->p_entries[newEntriesIndex + i].pe_batch = 0;
					tree_playlist->p_entries[newEntriesIndex + i].pe_flags = 0x0;
				}
			}

			// repeat while it looks like there might be more extents to fetch
			// (otlx.otlx_index tracks which omaps were already returned, so the next fsctl will start from there)
		} while (otlx.otlx_extent_count == omapcount);
		
		free(otlx.otlx_extents);
		
		if (tree_playlist_mount_idx >= 0) {
			DLOG("Added %d extents for leaf omaps for %s", tree_playlist->p_mounts[tree_playlist_mount_idx].pm_nentries, mount_name);
		}
		
		free(mount_name);
	}

	if (tree_playlist) {
		DLOG("Ended up with %d extents total for leaf omaps", tree_playlist->p_nentries);
		BC_sort_and_coalesce_playlist_internal(tree_playlist);
	}

	return tree_playlist;
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
		LOG("Playlist with omaps failed to verify: %d", ret);
		return ret;
	}

	if (pc->p_nomaps == 0) return 0;
	
	// <rdar://problem/33283668> OID lookup takes 2 seconds during boot
	// The APFSIOC_OMAP_LOOKUP call below ends up doing random IO,
	// so we first ask APFS where all the leaf omap entries live on disk
	// and play that back via the BootCache kext in order to speed up the
	// APFSIOC_OMAP_LOOKUP call below.
	bool need_reset = false;
	struct BC_playlist* omap_leaves_playlist = _BC_omap_tree_playlist(pc);
	if (omap_leaves_playlist) {
		ret = _BC_start(omap_leaves_playlist, false);
		if (ret == 0) {
			DLOG("Started omap leaves playback");
			need_reset = true;
		} else {
			LOG("Unable to start omap leaves playback: %d", ret);
		}
		PC_FREE_ZERO(omap_leaves_playlist);
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
		
		
		char *mount_name = NULL;
		char *bsd_disk = NULL;
		apfs_mount_for_uuid(pc->p_mounts[mount_idx].pm_uuid, &mount_name, &bsd_disk);
		if (!mount_name || !bsd_disk) {
			LOG_ERRNO("No mount for %s", uuid_string(pc->p_mounts[mount_idx].pm_uuid));
			if (mount_name) free(mount_name);
			if (bsd_disk) free(bsd_disk);
			continue;
		}

		uint8_t* batchNumbers = NULL;
		
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
		
		/* batch numbers */
		batchNumbers = calloc(num_omaps_in_mount, sizeof(*batchNumbers));
		if (batchNumbers == NULL) {
			ret = errno ?: ENOMEM;
			LOG("Unable to allocate memory for omap batch numbers");
			goto error_loop;
		}
		
		ol_record_idx = 0;
		/* Iterate through omaps and isolate only those belonging to mount_idx
		 */
		for (int global_omap_idx = 0; global_omap_idx < pc->p_nomaps; global_omap_idx++) {
			struct BC_playlist_omap *po = &pc->p_omaps[global_omap_idx];
			if (po->po_mount_idx == mount_idx) {
				ol.ol_records[ol_record_idx].otr_oid = po->po_omap.otr_oid;
				ol.ol_records[ol_record_idx].otr_oxid = po->po_omap.otr_oxid;
				batchNumbers[ol_record_idx] = po->po_omap.otr_batch;
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
		memmove(&omap_pc->p_mounts[0], &pc->p_mounts[mount_idx], sizeof(*omap_pc->p_mounts));
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
		} else if ((fs_flags & BC_FS_APFS_ENCRYPTED) != (omap_pc->p_mounts[0].pm_fs_flags & BC_FS_APFS_ENCRYPTED)) {
			// Volume has changed encryption state, give up
			LOG("APFS volume flags mismatch: %#x != %#x", fs_flags, omap_pc->p_mounts[0].pm_fs_flags);
			ret = 0; // Can't get extents for this mount, but other mounts may be fine
			goto error_loop;
		} else if (!(fs_flags & BC_FS_APFS_ENCRYPTED)) {
			// Volume is unencrypted, move OID metadata cache to the container
			DLOG("APFS volume %s is unencrypted, moving OID metadata cache to container %s", uuid_string(omap_pc->p_mounts[0].pm_uuid), uuid_string(container_uuid));
			uuid_copy(omap_pc->p_mounts[0].pm_uuid, container_uuid);
			uuid_copy(omap_pc->p_mounts[0].pm_group_uuid, container_uuid);
			omap_pc->p_mounts[0].pm_fs_flags |= BC_FS_APFS_CONTAINER;
		} else {
			// Volume is encrypted, keep OID metadata cache in the volume
			uuid_copy(omap_pc->p_mounts[0].pm_group_uuid, container_uuid);
		}
		
		//DLOG("OMAP conversion for mount %s", mount_name);
		//DLOG("(oid, oxid) --> (offset, length, crypto)");
		
		/* Fill up the playlist entries */
		for (int i = 0; i < num_omaps_in_mount; i++) {
			struct BC_playlist_entry *pentry = omap_pc->p_entries + i;
			apfs_omap_lookup_result_t result = ol.ol_results[i];
			
#ifdef DEBUG
			apfs_omap_track_record_t record = ol.ol_records[i];
			DLOG("(0x%llx, 0x%llx) --> (0x%llx, 0x%llx, 0x%llx), batch %d",
				 record.otr_oid, record.otr_oxid,
				 result.ol_offset, result.ol_length, result.ol_cpoff,
				 batchNumbers[i]);
#endif
			
			pentry->pe_offset = result.ol_offset;
			pentry->pe_length = result.ol_length;
			if (fs_flags & BC_FS_APFS_ENCRYPTED) {
				pentry->pe_crypto_offset = result.ol_cpoff;
			} else {
				pentry->pe_crypto_offset = 0;
			}

#if BC_ADD_SHARED_CACHE_AT_HIGH_PRIORITY
			pentry->pe_batch = batchNumbers[i] + 1; // Shift by the shared cache's batch
#else
			pentry->pe_batch = batchNumbers[i];
#endif
			pentry->pe_flags = 0; /* Should ideally copy over from the po_flags, but that doesn't exist yet */
			pentry->pe_mount_idx = 0; /* The only mount in this temp playlist, pc, we're creating */
			
		}

		/* Merge this mount's metadata playlist with the rest of the playlist entries we're building up */
		if ((ret = BC_merge_playlists(pc, omap_pc)) != 0) {
			LOG("Unable to merge playlists");
			goto error_loop;
		}
		
error_loop:
		if (batchNumbers)
			free(batchNumbers);
		if (ol.ol_records)
			free(ol.ol_records);
		if (ol.ol_results)
			free(ol.ol_results);
		PC_FREE_ZERO(omap_pc);
		if (mount_name) free(mount_name);
		if (bsd_disk) free(bsd_disk);

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
	
	if (need_reset) {
		_BC_reset();
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
			uuid_copy(pm->pm_group_uuid, hm->hm_group_uuid);
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
		pe->pe_batch = (he->he_flags & BC_HE_OPTIMIZED) ? 0 : curbatch;
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
	if (oh != NULL && total_omaps > 0) {

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
			if (pc_mount_idx >= pc->p_nmounts && ohm->ohm_nomaps > 0) {
				LOG("No recorded I/O for mount %s, ignoring %d omap records", uuid_string(ohm->ohm_uuid), ohm->ohm_nomaps);
				continue;
			}
			
			
			for (int omap_idx_in_mount = 0; omap_idx_in_mount < ohm->ohm_nomaps; omap_idx_in_mount++) {
				
				po = &(pc->p_omaps[pc->p_nomaps]);
				
				po->po_omap = ohm->ohm_omaps[omap_idx_in_mount];
				po->po_mount_idx = pc_mount_idx;
				
				//DLOG("omap record = {%s, (%#llx, %#llx), btch %d}", uuid_string(pc->p_mounts[pc_mount_idx].pm_uuid), po->po_omap.otr_oid, po->po_omap.otr_oxid, po->po_omap.otr_batch);
				
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
				memmove(pm, pm + 1, sizeof(*pm) * (pc->p_nmounts - mount_idx));
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
	BC_sort_and_coalesce_playlist_internal(pc);

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



//#define DEBUG_OPTIMIZATION_FILE_TRANSLATION

#ifdef DEBUG_OPTIMIZATION_FILE_TRANSLATION
#warning DEBUG_OPTIMIZATION_FILE_TRANSLATION enabled!

#define DLOG_OFT(fmt, args...) DLOG(fmt, ## args)

#define DLOG_inode_disk_map(_inodeDiskMap) \
do { \
	DLOG("inode %llu has %d dstreams", _inodeDiskMap->inode, _inodeDiskMap->numDstreams); \
	for (int _dstreamDiskIndex = 0; _dstreamDiskIndex < _inodeDiskMap->numDstreams; _dstreamDiskIndex++) { \
		struct dstream_disk_map* _dstreamDiskMap = _inodeDiskMap->dstreamDiskMaps + _dstreamDiskIndex; \
		uint64_t _dstreamOffset = 0; \
		DLOG(" dstream %llu has %d extents", _dstreamDiskMap->dstreamID, _dstreamDiskMap->numExtents); \
		for (int _diskExtentIndex = 0; _diskExtentIndex < _dstreamDiskMap->numExtents; _diskExtentIndex++) { \
			struct extent_with_crypto* _diskExtent = _dstreamDiskMap->diskExtents + _diskExtentIndex; \
			DLOG("  disk extent %#-12llx %#-8llx %#-12llx (file range %#llx-%#llx)", _diskExtent->offset, _diskExtent->length, _diskExtent->cpoff, _dstreamOffset, _dstreamOffset + _diskExtent->length); \
			_dstreamOffset += _diskExtent->length; \
		} \
	} \
} while (0)
#else
#define DLOG_inode_disk_map(_inodeDiskMap)
#define DLOG_OFT(fmt, args...)
#endif

struct extent_with_crypto {
	int64_t		offset; // in bytes
	uint64_t	length; // in bytes
	uint64_t	cpoff;  // in bytes
};

struct dstream_disk_map {
	uint64_t dstreamID;
	struct extent_with_crypto* diskExtents;
	uint32_t numExtents;
};

struct inode_disk_map {
	uint64_t inode;
	struct dstream_disk_map* dstreamDiskMaps;
	uint32_t numDstreams;
};

struct inode_disk_map*
BC_inode_disk_map_create(const char* mount_name, uint64_t inode, int blockSize, bool isEncrypted)
{
	struct inode_disk_map* inodeDiskMap = NULL;
	
	//// Code below adopted from apfsctl.c's printext function
	
	apfs_debug_dstream_list_t* dsBuffer = NULL;
	apfs_debug_file_ext_t* extentBuffer = NULL;
	
	// Get list of dstreams
	size_t dsBufSize = sizeof (apfs_debug_dstream_list_t) * 100; // Limit to 100 dstreams per inode. We expect 1, maybe a handful
	dsBuffer = calloc(1, dsBufSize);
	ASSERT(dsBuffer);
	apfs_debug_stats_t dsParams = {
		.size = sizeof (apfs_debug_stats_t),
		.op = APFS_DEBUG_OP_GET_FILE_DSTREAMS,
		.params.file_ds_list.buffer = dsBuffer,
		.params.file_ds_list.buffer_size = dsBufSize,
		.params.file_ds_list.inode_id = inode,
	};
	int err = fsctl(mount_name, APFSIOC_DEBUG_STATS, &dsParams, 0);
	if (err || dsParams.status) {
		if (!err) {
			errno = dsParams.status;
		}
		DLOG_ERRNO("Failed to fetch dstreams for inode %llu for boot optimizations", inode);
		free(dsBuffer);
		return NULL;
	}
	
	size_t extentBufSize = sizeof (apfs_debug_file_ext_t) * 100; // Limit to 100 extents at a time. We'll loop to go through all extents, though.
	extentBuffer = calloc(1, extentBufSize);
	ASSERT(extentBuffer);
	
	inodeDiskMap = calloc(1, sizeof(*inodeDiskMap));
	inodeDiskMap->inode = inode;
	inodeDiskMap->numDstreams = (uint32_t)dsParams.params.file_ds_list.buffer_entries;
	inodeDiskMap->dstreamDiskMaps = calloc(inodeDiskMap->numDstreams, sizeof(*inodeDiskMap->dstreamDiskMaps));

	for (int dstreamDiskIndex = 0; dstreamDiskIndex < dsParams.params.file_ds_list.buffer_entries; dstreamDiskIndex++) {
		DLOG_OFT("inode %llu dstream %llu, size %llu bytes", inode, dsBuffer[dstreamDiskIndex].dstreamId, dsBuffer[dstreamDiskIndex].size);
		
		struct dstream_disk_map* dstreamDiskMap = inodeDiskMap->dstreamDiskMaps + dstreamDiskIndex;
		dstreamDiskMap->dstreamID = dsBuffer[dstreamDiskIndex].dstreamId;
		
		for (uint64_t dstreamOffset = 0; dstreamOffset < dsBuffer[dstreamDiskIndex].size; /* dstreamOffset incremented in loop based on what we get from the fsctl */) {
			
			apfs_debug_stats_t extParams = {
				.size = sizeof (apfs_debug_stats_t),
				.op = APFS_DEBUG_OP_GET_FILE_EXTS,
				.params.file_ext_list.buffer = extentBuffer,
				.params.file_ext_list.buffer_size = extentBufSize,
				.params.file_ext_list.dstream_id = dsBuffer[dstreamDiskIndex].dstreamId,
				.params.file_ext_list.start_offset = dstreamOffset
			};
			err = fsctl(mount_name, APFSIOC_DEBUG_STATS, &extParams, 0);
			if (err || extParams.status) {
				if (!err) {
					errno = extParams.status;
				}
				DLOG_ERRNO("Failed to fetch extents for inode %llu dstream %llu for boot optimizations", inode, dsBuffer[dstreamDiskIndex].dstreamId);
				break;
			}
			uint64_t numEntries = extParams.params.file_ext_list.buffer_entries;
			if (!numEntries) {
				DLOG_ERRNO("Failed to fetch extents for inode %llu dstream %llu at offset %llu out of %llu for boot optimizations", inode, dsBuffer[dstreamDiskIndex].dstreamId, dstreamOffset, dsBuffer[dstreamDiskIndex].size);
				break;
			}
			
			uint32_t oldNumExtents = dstreamDiskMap->numExtents;
			dstreamDiskMap->numExtents += (uint32_t)numEntries;
			dstreamDiskMap->diskExtents = reallocf(dstreamDiskMap->diskExtents, dstreamDiskMap->numExtents * sizeof(*dstreamDiskMap->diskExtents));
			ASSERT(dstreamDiskMap->diskExtents);
			
			for (size_t i = 0; i < numEntries; i++) {
				const apfs_debug_file_ext_t* entry = extentBuffer + i;
				struct extent_with_crypto* diskExtent = dstreamDiskMap->diskExtents + oldNumExtents + i;
				
				diskExtent->offset = entry->lbaAllocated * blockSize;
				diskExtent->length = entry->length;
				if (isEncrypted) {
					diskExtent->cpoff  = entry->cpOffset;
				}
			}
			
			dstreamOffset = extentBuffer[numEntries - 1].offset + extentBuffer[numEntries - 1].length;
		}
		
		// dstreams are in file order, so must not be sorted by disk offset
	}

	free(dsBuffer);
	free(extentBuffer);
	
	return inodeDiskMap;
}

void
BC_inode_disk_map_destroy(struct inode_disk_map* inodeDiskMap)
{
	if (inodeDiskMap) {
		if (inodeDiskMap->dstreamDiskMaps) {
			for (int dstreamDiskIndex = 0; dstreamDiskIndex < inodeDiskMap->numDstreams; dstreamDiskIndex++) {
				if (inodeDiskMap->dstreamDiskMaps[dstreamDiskIndex].diskExtents) {
					free(inodeDiskMap->dstreamDiskMaps[dstreamDiskIndex].diskExtents);
				}
			}
			free(inodeDiskMap->dstreamDiskMaps);
		}
		free(inodeDiskMap);
	}
}

struct BC_history_inode {
	uint64_t inode;
	uint32_t* entryIndexes; // indexes into BC_history->h_entries
	uint32_t numEntries;
};

/*
 * Optimize the history recording for this APFS Fusion volume to provide better performance next boot
 * This will promote blocks to the fast device.
 * For any blocks that are promoted, they are removed from the history (length set to 0)
 *
 * Partial success is possible, and any errors will be logged
 */
struct BC_userspace_fusion_optimizations
BC_optimize_history_mount_fusion(struct BC_history * hc, int hm_index, const char *mount_name, struct BC_history_inode* historyInodes, uint numInodes)
{
	struct BC_userspace_fusion_optimizations userspace_fusion_optimizations = {
		.ssup_inodes_requested = numInodes,
		.ssup_inodes_already_optimized = 0,
		.ssup_inodes_optimized = 0,
		.ssup_bytes_requested = 0,
		.ssup_bytes_optimized = 0,
		.ssup_reads_optimized = 0,
	};
	
	struct BC_history_mount *hm = hc->h_mounts + hm_index;
	ASSERT(hm->hm_fs_flags & BC_FS_APFS_FUSION);

	uint numInodesFailed = 0;
	uint numInodesAttempted = 0;
	
	uint64_t* inodeBuffer = malloc(numInodes * sizeof(*inodeBuffer));
	ASSERT(inodeBuffer);
	for (int i = 0; i < numInodes; i++) {
		inodeBuffer[i] = historyInodes[i].inode;
	}

	while (numInodesAttempted < numInodes) {
		
		apfs_bc_optimize_t optimize;
		memset(&optimize, 0, sizeof(optimize));
		optimize.out_error = 0;
		optimize.out_inode_err = 0;
		optimize.inode_cnt = MIN((numInodes - numInodesAttempted), 1024); // APFS limits the fsctl to 1024 inodes at a time
		optimize.inodes = inodeBuffer + numInodesAttempted;
		
		{
			int err = fsctl(mount_name, APFSIOC_BC_OPTIMIZE_INODES, &optimize, 0);
			if (err != 0) {
				LOG_ERRNO("Unable to optimize BootCache for %s's %d inodes (fsctl err)", uuid_string(hm->hm_uuid), numInodes);
				break;
			}
		}
		
		// fsctl succeeded, but optimization may have run into errors, tracked by optimize.out_error
		//
		// If we get no error, then all the inodes were successfully optimized
		// If we get an error and out_inode_err is -1, then we failed before even trying.
		// If we get an error and out_inode_err is >= 0, then we failed on a specific inode. Everything before that inode succeeded, and we may be able to continue trying for the rest of the inodes after out_inode_err, depending on the specific error:
		//   ECANCELED Cannot continue trying (canceled for some reason, e.g. the mount is being unmounted)
		//   ENOSPC    Can continue trying (not enough space on the fast device for out_inode_err. out_inode_err may have been some really large file and we can still optimize smaller ones)
		//   EINVAL    Can continue trying (out_inode_err is invalid, e.g. that file was deleted)
		//   ENOMEM    Cannot continue trying (unable to allocate some memory somewhere, and the sizes being allocated don't depend on the inode)
		//   EFTYPE    Can continue trying (out_inode_err isn't a file)
		//   EALREADY  Can continue trying (inode has already been optimized for other reasons)
		
		if (optimize.out_error != 0 && optimize.out_inode_err < 0) {
			// Failed before optimizing any inodes
			errno = optimize.out_error;
			LOG_ERRNO("Unable to optimize BootCache for %s's %d inodes (apfs err)", uuid_string(hm->hm_uuid), numInodes);
			break;
		}
		ASSERT(optimize.out_error == 0 || optimize.out_inode_err < optimize.inode_cnt);
		
		DLOG("Optimized %d inodes: err %d inode_err %d", optimize.inode_cnt, optimize.out_error, optimize.out_inode_err);
		
		// Cases where we succeeded to some degree
		if (optimize.out_error == 0 ||          // Success for all inodes!
			optimize.out_inode_err > 0) {       // Succeeded up until (but not including) the inode at index out_inode_err
			
			int16_t last_successful_inode_index;
			if (optimize.out_error == 0) {
				last_successful_inode_index = optimize.inode_cnt - 1;
			} else if (optimize.out_error == EALREADY) {
				last_successful_inode_index = optimize.out_inode_err;
			} else {
				numInodesFailed++;
				last_successful_inode_index = optimize.out_inode_err - 1;
			}
			
			userspace_fusion_optimizations.ssup_inodes_optimized += last_successful_inode_index;
			
			// Clear all the history entires from inodes that succeeded
			for (uint inode_index = 0; inode_index <= last_successful_inode_index; inode_index++) {
				struct BC_history_inode* historyInode = historyInodes + (numInodesAttempted + inode_index);
				
				for (int i = 0; i < historyInode->numEntries; i++) {
					struct BC_history_entry* he = hc->h_entries + historyInode->entryIndexes[i];
					
					userspace_fusion_optimizations.ssup_bytes_optimized += he->he_length;
					userspace_fusion_optimizations.ssup_reads_optimized++;

					he->he_length = 0;
				}
			}
		}
		
		if (optimize.out_error == 0) {
			numInodesAttempted += optimize.inode_cnt;
		} else {
			numInodesAttempted += optimize.out_inode_err + 1;
		}
		
		// Cases where we can't continue trying with the rest of the inodes (for unknown errors, assume we can continue)
		if (optimize.out_error != 0 && // Hit an error
			(optimize.out_inode_err < 0 || // Hit an error before attempting first inode
			 (optimize.out_error == ECANCELED ||
			  optimize.out_error == ENOMEM) // Error isn't something we can continue after
			 )
			) {
			
			errno = optimize.out_error;
			LOG_ERRNO("Unable to optimize BootCache for %s after %d out of %d inodes (%d other failures)", uuid_string(hm->hm_uuid), (numInodes - optimize.inode_cnt) + optimize.out_inode_err, numInodes, numInodesFailed);
			break;
		}
	}
	
	// Add up how many bytes were not optimized, and count number of inodes already optimized
	u_int64_t bytesNonoptimized = 0;
	for (int historyInodeIndex = 0; historyInodeIndex < numInodes; historyInodeIndex++) {
		struct BC_history_inode* historyInode = historyInodes + historyInodeIndex;
		
		bool sawOptimizedExtent = false;
		for (int i = 0; i < historyInode->numEntries; i++) {
			struct BC_history_entry* he = hc->h_entries + historyInode->entryIndexes[i];
			
			if (he->he_length > 0) {
				// We zero'ed out anything that was optimized, so anything left wasn't optimized
				bytesNonoptimized += he->he_length;
			}
			
			if (!sawOptimizedExtent && !(he->he_offset & FUSION_TIER2_DEVICE_BYTE_ADDR)) {
				sawOptimizedExtent = true;
				userspace_fusion_optimizations.ssup_inodes_already_optimized++;
			}
		}
	}
	userspace_fusion_optimizations.ssup_bytes_requested = userspace_fusion_optimizations.ssup_bytes_optimized + bytesNonoptimized;
	
	LOG("Successfully optimized %d inodes on %s for boot (%d failures)", numInodesAttempted - numInodesFailed, uuid_string(hm->hm_uuid), numInodesFailed);
	
	free(inodeBuffer);

	return userspace_fusion_optimizations;
}

struct extent {
	int64_t		offset; // in bytes
	uint64_t	length; // in bytes
						// TODO: Add batch number somehow?
};

struct dstream_file_map {
	uint64_t dstreamID;
	struct extent* fileRanges;
	uint32_t numRanges;
};

struct inode_file_map {
	uint64_t inode;
	struct dstream_file_map* dstreamFileMaps;
	uint32_t numDstreams;
};

struct bc_mount_optimization_info {
	// Filled in by BC_mount_copy_file_relative_history
	struct BC_history * hc;
	int hm_index;
	char *mount_name;
	struct BC_history_inode* historyInodes;
	struct inode_file_map* inodeFileMaps;
	uint numInodes;
	int blockSize;
	bool completed;
	
	// Filled in by BC_optimize_inodes_hdd (range only if last volume)
	int numInodesRequested;
	uint64_t optimizedRangeStart;
	uint64_t optimizedRangeLength;
	
	// Filled in by BC_mount_fixup_history_after_optimizations
	uint64_t lowestOptimizedByteAddress;
	uint64_t highestOptimizedByteAddress;
	struct BC_userspace_hdd_optimizations hdd_stats;

};

static void
BC_free_mount_optimization_info(struct bc_mount_optimization_info* mount_optimization_info)
{
	if (!mount_optimization_info) return;
	
	if (mount_optimization_info->historyInodes) {
		for (int i = 0; i < mount_optimization_info->numInodes; i++) {
			if (mount_optimization_info->historyInodes[i].entryIndexes) {
				free(mount_optimization_info->historyInodes[i].entryIndexes);
			}
		}
		free(mount_optimization_info->historyInodes);
	}
	
	if (mount_optimization_info->inodeFileMaps) {
		for (int inodeIdx = 0; inodeIdx < mount_optimization_info->numInodes; inodeIdx++) {
			struct inode_file_map* inodeFileMap = mount_optimization_info->inodeFileMaps + inodeIdx;
			if (inodeFileMap->dstreamFileMaps) {
				for (int dstreamFileIndex = 0; dstreamFileIndex < inodeFileMap->numDstreams; dstreamFileIndex++) {
					struct dstream_file_map* dstreamFileMap = inodeFileMap->dstreamFileMaps + dstreamFileIndex;
					if (dstreamFileMap->fileRanges) free(dstreamFileMap->fileRanges);
				}
				free(inodeFileMap->dstreamFileMaps);
			}
		}
		free(mount_optimization_info->inodeFileMaps);
	}
	
	if (mount_optimization_info->mount_name) {
		free(mount_optimization_info->mount_name);
	}
	free(mount_optimization_info);
}

struct bc_optimization_info {
	int num_mount_optimization_infos;
	struct bc_mount_optimization_info** mount_optimization_infos;
	struct BC_history * hc;
	struct BC_userspace_hdd_optimization_state userspace_hdd_optimization_state;
};

void BC_free_optimization_info(struct bc_optimization_info* optimization_info) {
	if (!optimization_info) return;
	
	if (optimization_info->mount_optimization_infos) {
		for (int mount_optimization_info_index = 0; mount_optimization_info_index < optimization_info->num_mount_optimization_infos; mount_optimization_info_index++) {
			if (optimization_info->mount_optimization_infos[mount_optimization_info_index]) {
				BC_free_mount_optimization_info(optimization_info->mount_optimization_infos[mount_optimization_info_index]);
			}
		}
	}
	free(optimization_info->mount_optimization_infos);
	free(optimization_info);
}

/*
 * Convert the mount's history recording, which uses disk addresses, to file-relative offsets for use in HDD optimizations
 */
static struct bc_mount_optimization_info*
BC_mount_copy_file_relative_history(struct BC_history * hc, int hm_index, const char *mount_name, struct BC_history_inode* historyInodes, uint numInodes)
{
	struct BC_history_mount *hm = hc->h_mounts + hm_index;
	ASSERT(!(hm->hm_fs_flags & BC_FS_APFS_FUSION));
	
	struct statfs statfs_buf;
	if (0 != statfs(mount_name, &statfs_buf)) {
		LOG_ERRNO("Unable to stafs %s for mount %s", mount_name, uuid_string(hm->hm_uuid));
		return NULL;
	}
	
	int blockSize = statfs_buf.f_bsize;
	
	DLOG_OFT("Converting HDD mount %s to file/offset", uuid_string(hm->hm_uuid));
	
#define INODE_NUM_LIMIT 16384
	if (numInodes > INODE_NUM_LIMIT) { // APFS limits us to 16K inodes
		LOG("Only optimizing %d out of %d inodes read during boot for mount %s", INODE_NUM_LIMIT, numInodes, uuid_string(hm->hm_uuid));
		numInodes = INODE_NUM_LIMIT;
	}
	
	struct inode_file_map* inodeFileMaps = calloc(numInodes, sizeof(*inodeFileMaps));

	for (int inodeIdx = 0; inodeIdx < numInodes; inodeIdx++) {
		struct BC_history_inode* historyInode = historyInodes + inodeIdx;

		uint64_t inode = historyInode->inode;
		struct inode_file_map* inodeFileMap = inodeFileMaps + inodeIdx;
		inodeFileMap->inode = inode;
		
		struct inode_disk_map* inodeDiskMap = BC_inode_disk_map_create(mount_name, inode, blockSize, hm->hm_fs_flags & BC_FS_APFS_ENCRYPTED);
		if (!inodeDiskMap) {
			DLOG("No disk map for inode %llu for mount %s", inode, uuid_string(hm->hm_uuid));
			// remove this inode from the list of ones to be optimized since we can't track where the file ends up
			// Typical case here is the file was deleted.
			// (the BootCache's recorded extents for this inode also get cleared later)
			historyInode->inode = 0;
			continue;
		}
		
		DLOG_inode_disk_map(inodeDiskMap);

		if (inodeDiskMap->numDstreams == 0) {
			DLOG_OFT("No dstreams for inode %llu for mount %s", inode, uuid_string(hm->hm_uuid));
			BC_inode_disk_map_destroy(inodeDiskMap);
			// remove this inode from the list of ones to be optimized since we can't track where the file ends up
			// (the BootCache's recorded extents for this inode also get cleared later)
			historyInode->inode = 0;
			continue;
		}

		// Fill in all the dstreams up-front so we can use the same dstream index for both the file and the disk maps
		inodeFileMap->numDstreams = inodeDiskMap->numDstreams;
		inodeFileMap->dstreamFileMaps = calloc(inodeFileMap->numDstreams, sizeof(*inodeFileMap->dstreamFileMaps));
		ASSERT(inodeFileMap->dstreamFileMaps);
		for (int dstreamIndex = 0; dstreamIndex < inodeFileMap->numDstreams; dstreamIndex++) {
			inodeFileMap->dstreamFileMaps[dstreamIndex].dstreamID = inodeDiskMap->dstreamDiskMaps[dstreamIndex].dstreamID;
		}
		
		DLOG_OFT("inode %llu history:", inode);

		bool exentsHaveSomeOverlap = false;
		for (int historyInodeExtentEntryIndex = 0; historyInodeExtentEntryIndex < historyInode->numEntries; historyInodeExtentEntryIndex++) {
			int he_index = historyInode->entryIndexes[historyInodeExtentEntryIndex];
			struct BC_history_entry* he = hc->h_entries + he_index;
			uint64_t overlapBytes = 0;

			for (int dstreamIndex = 0; dstreamIndex < inodeDiskMap->numDstreams && overlapBytes < he->he_length; dstreamIndex++) {
				struct dstream_disk_map* dstreamDiskMap = inodeDiskMap->dstreamDiskMaps + dstreamIndex;
				struct dstream_file_map* dstreamFileMap = inodeFileMap->dstreamFileMaps + dstreamIndex;
				uint64_t dstreamOffset = 0;

				for (int diskExtentIndex = 0; diskExtentIndex < dstreamDiskMap->numExtents && overlapBytes < he->he_length; diskExtentIndex++) {
					struct extent_with_crypto* diskExtent = dstreamDiskMap->diskExtents + diskExtentIndex;
					if (diskExtent->length == 0) {
						continue;
					}
					
					if (he->he_offset < diskExtent->offset + diskExtent->length &&
						diskExtent->offset < he->he_offset + he->he_length) {
						// they intersect
						
						dstreamFileMap->numRanges++;
						dstreamFileMap->fileRanges = reallocf(dstreamFileMap->fileRanges, dstreamFileMap->numRanges * sizeof(*dstreamFileMap->fileRanges));
						ASSERT(dstreamFileMap->fileRanges);
						
						struct extent* fileRange = dstreamFileMap->fileRanges + (dstreamFileMap->numRanges - 1);
						
						fileRange->offset = dstreamOffset;
						if (he->he_offset > diskExtent->offset) {
							fileRange->offset += (he->he_offset - diskExtent->offset);
						}
						
						fileRange->length = MIN(he->he_offset + he->he_length, diskExtent->offset + diskExtent->length) - MAX(he->he_offset, diskExtent->offset);
						
						overlapBytes += fileRange->length;
						DLOG_OFT("  entry %d: %#-12llx %#-8llx %#-12llx hit dstream %llu extent %d %#-12llx %#-8llx %#-12llx (file range %d %#8llx-%#-8llx (%#-8llx bytes)) remaining: %#llx",
							 he_index,
							 he->he_offset, he->he_length, he->he_crypto_offset,
							 dstreamDiskMap->dstreamID,
							 diskExtentIndex,
							 diskExtent->offset, diskExtent->length, diskExtent->cpoff,
							 dstreamFileMap->numRanges - 1,
							 fileRange->offset, fileRange->offset + fileRange->length, fileRange->length,
							 overlapBytes - he->he_length);
						
						exentsHaveSomeOverlap = true;
					}
					
					dstreamOffset += diskExtent->length;
				}
				
			}
			
			if (overlapBytes < he->he_length) {
				DLOG_OFT("  entry %d: %#-12llx %#-8llx %#-12llx missing %#llx bytes in dstream data",
						 he_index,
						 he->he_offset, he->he_length, he->he_crypto_offset,
						 he->he_length - overlapBytes);
			}
		}
		
		if (!exentsHaveSomeOverlap) {
			DLOG("inode %llu no longer exists where BootCache recorded for mount %s", inode, uuid_string(hm->hm_uuid));
			BC_inode_disk_map_destroy(inodeDiskMap);
			// This file has moved to a completely new location on disk since the recording we're trying to optimize
			// remove this inode from the list of ones to be optimized
			historyInode->inode = 0;
			continue;
		}
		
		// Sort/coalesce the file ranges

		for (int dstreamFileIndex = 0; dstreamFileIndex < inodeFileMap->numDstreams; dstreamFileIndex++) {
			struct dstream_file_map* dstreamFileMap = inodeFileMap->dstreamFileMaps + dstreamFileIndex;
			if (dstreamFileMap && dstreamFileMap->numRanges > 1) {
				qsort_b(dstreamFileMap->fileRanges, dstreamFileMap->numRanges, sizeof(*dstreamFileMap->fileRanges), ^int(const void * first, const void * second) {
					const struct extent* a = first;
					const struct extent* b = second;
					if (a->offset > b->offset) {
						return 1;
					}
					if (a->offset < b->offset) {
						return -1;
					}
					return 0;
				});
				
				// Coalesce the file ranges
				int previousFileRangeIndex = 0;
				for (int fileRangeIndex = 1; fileRangeIndex < dstreamFileMap->numRanges; fileRangeIndex++) {
					struct extent* previousFileRange = dstreamFileMap->fileRanges + previousFileRangeIndex;
					struct extent* fileRange = dstreamFileMap->fileRanges + fileRangeIndex;
					
					if (fileRange->length == 0) {
						continue;
					}
					
					if (previousFileRange->offset + previousFileRange->length >= fileRange->offset) {
						previousFileRange->length = MAX(previousFileRange->offset + previousFileRange->length, fileRange->offset + fileRange->length) - previousFileRange->offset;
						
						fileRange->length = 0;
					} else {
						previousFileRangeIndex = fileRangeIndex;
					}
				}
				
#ifdef DEBUG_OPTIMIZATION_FILE_TRANSLATION
#ifdef DEBUG
				DLOG_OFT("inode %llu dstream %llu coalesced/sorted to:", inode, dstreamFileMap->dstreamID);
				for (int fileRangeIndex = 0; fileRangeIndex < dstreamFileMap->numRanges; fileRangeIndex++) {
					struct extent* fileRange = dstreamFileMap->fileRanges + fileRangeIndex;
					DLOG_OFT("  file range %#llx-%#llx (%#llx bytes)", fileRange->offset, fileRange->offset + fileRange->length, fileRange->length);
				}
#endif
#endif
			}
		}
		

		BC_inode_disk_map_destroy(inodeDiskMap);
	}
	
	// Return NULL if we have no inodes to optimize
	bool haveAnyInodes = false;
	for (int inodeIdx = 0; inodeIdx < numInodes; inodeIdx++) {
		if (historyInodes[inodeIdx].inode != 0) {
			haveAnyInodes = true;
			break;
		}
	}
	if (! haveAnyInodes) {
		
		DLOG("Unable to optimize any of the %d inodes for mount %s", numInodes, uuid_string(hm->hm_uuid));
		
		if (inodeFileMaps) {
			for (int inodeIdx = 0; inodeIdx < numInodes; inodeIdx++) {
				struct inode_file_map* inodeFileMap = inodeFileMaps + inodeIdx;
				if (inodeFileMap->dstreamFileMaps) {
					for (int dstreamFileIndex = 0; dstreamFileIndex < inodeFileMap->numDstreams; dstreamFileIndex++) {
						struct dstream_file_map* dstreamFileMap = inodeFileMap->dstreamFileMaps + dstreamFileIndex;
						if (dstreamFileMap->fileRanges) free(dstreamFileMap->fileRanges);
					}
					free(inodeFileMap->dstreamFileMaps);
				}
			}
			free(inodeFileMaps);
		}
		
		return NULL;
	}
	
	struct bc_mount_optimization_info *mount_optimization_info = calloc(1, sizeof(*mount_optimization_info));
	mount_optimization_info->hc = hc;
	mount_optimization_info->hm_index = hm_index;
	mount_optimization_info->mount_name = strdup(mount_name);
	mount_optimization_info->historyInodes = historyInodes;
	mount_optimization_info->inodeFileMaps = inodeFileMaps;
	mount_optimization_info->numInodes = numInodes;
	mount_optimization_info->blockSize = blockSize;
	
	// Things to be filled in later
	mount_optimization_info->optimizedRangeStart = UINT64_MAX;
	mount_optimization_info->optimizedRangeLength = 0;
	mount_optimization_info->numInodesRequested = 0;
	mount_optimization_info->lowestOptimizedByteAddress = UINT64_MAX;
	mount_optimization_info->highestOptimizedByteAddress = 0;
	mount_optimization_info->completed = false;
	// mount_optimization_info->stats cleared due to calloc
	
	return mount_optimization_info;
}


/*
 * Optimize the inodes APFS pure HDD volume to provide better performance next boot.
 * This will reorder blocks together and to a fast portion of disk.
 *
 * If isLastVolumeInContainer is false, provides the inodes to APFS but does not actually perform optimizations (mount_optimization_info->optimizedRange* will be unset)
 * If isLastVolumeInContainer is true, provides the inodes to APFS and actually performs optimizations (mount_optimization_info->optimizedRange* will be set upon success)
 */
static int
BC_optimize_inodes_hdd(struct bc_mount_optimization_info *mount_optimization_info, bool isLastVolumeInContainer)
{
	struct BC_history_mount *hm = mount_optimization_info->hc->h_mounts + mount_optimization_info->hm_index;
	
	int numInodesRequested = 0;
	uint64_t* inodeBuffer = malloc(mount_optimization_info->numInodes * sizeof(*inodeBuffer));
	ASSERT(inodeBuffer);
	for (int i = 0; i < mount_optimization_info->numInodes; i++) {
		if (0 != mount_optimization_info->historyInodes[i].inode) {
			inodeBuffer[numInodesRequested] = mount_optimization_info->historyInodes[i].inode;
			numInodesRequested++;
		}
	}
	
	if (numInodesRequested != mount_optimization_info->numInodes) {
		DLOG("Only requesting optimization for %d out of %d inodes for mount %s", numInodesRequested, mount_optimization_info->numInodes, uuid_string(hm->hm_uuid));
	}
	
	apfs_bc_optimize_t optimize;
	memset(&optimize, 0, sizeof(optimize));
	optimize.out_paddr = 0;
	optimize.out_length = 0;
	optimize.out_error = 0;
	optimize.inode_cnt = numInodesRequested;
	optimize.inodes = inodeBuffer;
	optimize.flags = isLastVolumeInContainer ? 0x0 : APFS_BC_OPTIMIZE_INODES_UPDATE_ONLY;
	
	int err = fsctl(mount_optimization_info->mount_name, APFSIOC_BC_OPTIMIZE_INODES, &optimize, 0);
	free(inodeBuffer);
	if (err != 0) {
		LOG_ERRNO("BootCache optimization for %s's %d inodes fsctl error", uuid_string(hm->hm_uuid), numInodesRequested);
		return errno ?: EINVAL;
	}

	bool optimizationPaused = false;

	if (optimize.out_error != 0) {
		errno = optimize.out_error;
		if (optimize.out_error == ECANCELED) {
			optimizationPaused = true;
			DLOG_ERRNO("BootCache optimization for %s's %d inodes returned error", uuid_string(hm->hm_uuid), numInodesRequested);
		} else {
			LOG_ERRNO("BootCache optimization for %s's %d inodes returned error", uuid_string(hm->hm_uuid), numInodesRequested);
			// Not necessarily fatal, lets see if we get a valid range...
		}
	}

	
	// The range returned from the fsctl indicating the range on disk where the optimized data lives (only provided on last call)
	if (isLastVolumeInContainer) {
		if (optimize.out_paddr == UINT64_MAX || optimize.out_length == UINT64_MAX || optimize.out_length == 0) {
			LOG("BootCache optimization for %s's %d inodes returned invalid range", uuid_string(hm->hm_uuid), numInodesRequested);
			return EINVAL;
		}
		mount_optimization_info->optimizedRangeStart = optimize.out_paddr;
		mount_optimization_info->optimizedRangeLength = optimize.out_length;
	}
	
	mount_optimization_info->numInodesRequested = numInodesRequested;
	
	
	return optimizationPaused ? ECANCELED : 0;
}

/*
 * After optimizations have occurred, update the playlist with to the new locations for the files.
 */
static void
BC_mount_fixup_history_after_optimizations(struct bc_mount_optimization_info *mount_optimization_info, bool resumedAfterPaused)
{
	struct BC_history * hc = mount_optimization_info->hc;
	const int hm_index = mount_optimization_info->hm_index;
	const char *mount_name = mount_optimization_info->mount_name;
	struct BC_history_inode* historyInodes = mount_optimization_info->historyInodes;
	const struct inode_file_map* inodeFileMaps = mount_optimization_info->inodeFileMaps;
	const uint numInodes = mount_optimization_info->numInodes;
	const int numInodesRequested = mount_optimization_info->numInodesRequested;
	uint64_t optimizedRangeStart = mount_optimization_info->optimizedRangeStart;
	uint64_t optimizedRangeLength = mount_optimization_info->optimizedRangeLength;
	const int blockSize = mount_optimization_info->blockSize;
	
	if (resumedAfterPaused) {
		mount_optimization_info->hdd_stats.ssup_inodes_requested = 0;
		mount_optimization_info->hdd_stats.ssup_inodes_optimized = 0;
		mount_optimization_info->hdd_stats.ssup_bytes_requested = 0;
		mount_optimization_info->hdd_stats.ssup_bytes_optimized = 0;
		mount_optimization_info->hdd_stats.ssup_bytes_surplus = 0;
		mount_optimization_info->hdd_stats.ssup_bytes_nonoptimized = 0;
		mount_optimization_info->hdd_stats.ssup_reads_optimized = 0;
		mount_optimization_info->hdd_stats.ssup_optimization_range_length = 0;
		
		if (mount_optimization_info->hdd_stats.ssup_bytes_already_optimized == 0 &&
			mount_optimization_info->hdd_stats.ssup_reads_already_optimized == 0 &&
			mount_optimization_info->hdd_stats.ssup_inodes_already_optimized == 0) {
			
			// If we didn't see anything already optimized, then that was a fusion-only pass, and we can update the already* stats this time
			resumedAfterPaused = false;
		}
		
		// Don't update already_optimized stats if this a subsequent try to optimize this volume's inodes
		// (inodes we optimized on the previous tries will look like they were already optimized, giving inflated numbers)
	} else {
		bzero(&mount_optimization_info->hdd_stats, sizeof(mount_optimization_info->hdd_stats));
	}
	
	struct BC_history_mount *hm = hc->h_mounts + hm_index;
	ASSERT(!(hm->hm_fs_flags & BC_FS_APFS_FUSION));

	
	// After the data has been optimized, use APFS_DEBUG_OP_GET_FILE_DSTREAMS and APFS_DEBUG_OP_GET_FILE_EXTS to see where the files ended up
	// Some files may not have been optimized, so compare the extents returned to the original recording before adding to the playlist.
	
	DLOG("Optimized %d inodes to %#llx-%#llx (%llu length) for mount %s, checking where they ended up on disk...", numInodesRequested, optimizedRangeStart, optimizedRangeStart + optimizedRangeLength, optimizedRangeLength, uuid_string(hm->hm_uuid));
	
	// The actual range we find by iterating the optimized inodes later, because the range apfs provides isn't a tight bounds
	uint64_t lowestOptimizedByteAddress = UINT64_MAX;
	uint64_t highestOptimizedByteAddress = 0;

	uint64_t numBytesDropped = 0;
	int numInodesDropped = 0;
	
	for (int inodeIdx = 0; inodeIdx < numInodes; inodeIdx++) {
		struct BC_history_inode* historyInode = historyInodes + inodeIdx;
		uint64_t inode = historyInode->inode;
		DLOG_OFT("inode %llu post-optimization:", inode);
		
		u_int64_t readBytes = 0;
		u_int64_t optimizedBytes = 0;
		u_int64_t optimizedReadBytes = 0;

		bool sawOptimizedExtent = false;
		for (int tempHistoryInodeExtentEntryIndex = 0; tempHistoryInodeExtentEntryIndex < historyInode->numEntries; tempHistoryInodeExtentEntryIndex++) {
			int he_index = historyInode->entryIndexes[tempHistoryInodeExtentEntryIndex];
			struct BC_history_entry* he = hc->h_entries + he_index;
			
			readBytes += he->he_length;
			
			// Don't update already_optimized stats if this a subsequent try to optimize this volume's inodes
			// (inodes we optimized on the previous tries will look like they were already optimized, giving inflated numbers)
			if (!resumedAfterPaused) {
				uint64_t intersectionStart = MAX(optimizedRangeStart, he->he_offset);
				uint64_t intersectionEnd = MIN(optimizedRangeStart + optimizedRangeLength, he->he_offset + he->he_length);
				if (intersectionStart < intersectionEnd) {
					mount_optimization_info->hdd_stats.ssup_reads_already_optimized++;
					mount_optimization_info->hdd_stats.ssup_bytes_already_optimized += intersectionEnd - intersectionStart; // Bytes that were already in the optimized range, however they got there previously
					if (!sawOptimizedExtent) {
						sawOptimizedExtent = true;
						mount_optimization_info->hdd_stats.ssup_inodes_already_optimized++;
					}
				}
			}
		}

		
		int historyInodeExtentEntryIndex = 0;
		if (0 != inode) {
			
			struct inode_disk_map* inodeDiskMap = BC_inode_disk_map_create(mount_name, inode, blockSize, hm->hm_fs_flags & BC_FS_APFS_ENCRYPTED);
			if (inodeDiskMap) {
				
				DLOG_inode_disk_map(inodeDiskMap);
				
				// Check how much data was optimized (this time and previously) for statistical purposes
				for (int dstreamDiskIndex = 0; dstreamDiskIndex < inodeDiskMap->numDstreams; dstreamDiskIndex++) {
					struct dstream_disk_map* dstreamDiskMap = inodeDiskMap->dstreamDiskMaps + dstreamDiskIndex;
					for (int diskExtentIndex = 0; diskExtentIndex < dstreamDiskMap->numExtents; diskExtentIndex++) {
						struct extent_with_crypto* diskExtent = dstreamDiskMap->diskExtents + diskExtentIndex;
						
						if (diskExtent->offset < optimizedRangeStart ||
							diskExtent->offset + diskExtent->length > optimizedRangeStart + optimizedRangeLength) {
							
							DLOG_OFT("inode %llu dstream %llu extent %d not optimized:  %#-12llx %#-8llx", inode, dstreamDiskMap->dstreamID, diskExtentIndex, diskExtent->offset, diskExtent->length);
						} else {
							
							if (lowestOptimizedByteAddress > diskExtent->offset) {
								lowestOptimizedByteAddress = diskExtent->offset;
							}
							if (highestOptimizedByteAddress < diskExtent->offset + diskExtent->length) {
								highestOptimizedByteAddress = diskExtent->offset + diskExtent->length;
							}
							optimizedBytes += diskExtent->length;
						}
					}
				}
				
				// inodeFileMap contains the ranges of the file that we want to include in the playlist
				const struct inode_file_map* inodeFileMap = inodeFileMaps + inodeIdx;
				
				DLOG_OFT("inode %llu history post-optimization:", inode);
				
				
				for (int dstreamFileIndex = 0; dstreamFileIndex < inodeFileMap->numDstreams; dstreamFileIndex++) {
					struct dstream_file_map* dstreamFileMap = inodeFileMap->dstreamFileMaps + dstreamFileIndex;
					if (dstreamFileMap->numRanges == 0) {
						DLOG_OFT("inode %llu dstream %llu not in recording", inode, dstreamFileMap->dstreamID);
						continue;
					}
					
					// Find matching dstream from inodeDiskMap
					struct dstream_disk_map* dstreamDiskMap = NULL;
					for (int dstreamDiskIndex = 0; dstreamDiskIndex < inodeDiskMap->numDstreams; dstreamDiskIndex++) {
						if (inodeDiskMap->dstreamDiskMaps[dstreamDiskIndex].dstreamID == dstreamFileMap->dstreamID) {
							dstreamDiskMap = inodeDiskMap->dstreamDiskMaps + dstreamDiskIndex;
							break;
						}
					}
					if (!dstreamDiskMap) {
						DLOG_OFT("inode %llu dstream %llu no longer exists", inode, dstreamFileMap->dstreamID);
						continue;
					}
					
					DLOG_OFT(" dstream %llu", dstreamFileMap->dstreamID);
					
					// Iterate over the disk extents, keeping track of what file offset we're at (dstreamFileOffsetForDiskExtent), and replace the entries to our history for this inode with the new locations of this file
					uint64_t dstreamFileOffsetForDiskExtent = 0;
					int fileRangeIndex = 0;
					struct extent* fileRange = NULL;
					for (fileRangeIndex = 0; fileRangeIndex < dstreamFileMap->numRanges; fileRangeIndex++) {
						fileRange = dstreamFileMap->fileRanges + fileRangeIndex;
						if (fileRange->length != 0) {
							break;
						}
					}
					if (fileRangeIndex >= dstreamFileMap->numRanges) {
						DLOG_OFT("No recorded extents for dstream %llu", dstreamFileMap->dstreamID);
					}
					
					for (int diskExtentIndex = 0; diskExtentIndex < dstreamDiskMap->numExtents; diskExtentIndex++) {
						struct extent_with_crypto* diskExtent = dstreamDiskMap->diskExtents + diskExtentIndex;
						if (diskExtent->length == 0) {
							continue;
						}
						
						while (fileRange->offset < dstreamFileOffsetForDiskExtent + diskExtent->length) {
							// We want part of this disk range in our playlist, use the next entry in our history for this inode
							
							int he_index;
							if (historyInodeExtentEntryIndex < historyInode->numEntries) {
								he_index = historyInode->entryIndexes[historyInodeExtentEntryIndex];
							} else {
								hc->h_nentries ++;
								hm->hm_nentries ++;
								hc->h_entries = reallocf(hc->h_entries, hc->h_nentries * sizeof(*hc->h_entries));
								ASSERT(hc->h_entries);
								
								he_index = hc->h_nentries - 1;
								
								bzero(hc->h_entries + he_index, sizeof(*hc->h_entries));
								
								hc->h_entries[he_index].he_inode = inode;
								hc->h_entries[he_index].he_mount_idx = hm_index;
								
								// Update our entryIndexes for this inode to point to the new history entry (needed in case we got paused and go through this again later)
								ASSERT(historyInodeExtentEntryIndex == historyInode->numEntries);
								historyInode->numEntries ++;
								historyInode->entryIndexes = reallocf(historyInode->entryIndexes, historyInode->numEntries * sizeof(*historyInode->entryIndexes));
								ASSERT(historyInode->entryIndexes);
								historyInode->entryIndexes[historyInodeExtentEntryIndex] = he_index;
								
								DLOG_OFT("  Created new h_entry %d at dhistoryInodeExtentEntryIndex %d", he_index, historyInodeExtentEntryIndex);
							}
							historyInodeExtentEntryIndex++;
							
							struct BC_history_entry* he = hc->h_entries + he_index;
							
							ASSERTF(he->he_inode == inode, "entry inode %llu != inode %llu", he->he_inode, inode);
							ASSERT(he->he_mount_idx == hm_index);
							
							uint64_t fileRangeStart = MAX(dstreamFileOffsetForDiskExtent, fileRange->offset);
							uint64_t fileRangeEnd = MIN(dstreamFileOffsetForDiskExtent + diskExtent->length, fileRange->offset + fileRange->length);
							uint64_t offsetFromDiskExtentStart = (fileRangeStart - dstreamFileOffsetForDiskExtent);
							
							he->he_offset = diskExtent->offset + offsetFromDiskExtentStart;
							he->he_length = fileRangeEnd - fileRangeStart;
							if (hm->hm_fs_flags & BC_FS_APFS_ENCRYPTED) {
								he->he_crypto_offset = diskExtent->cpoff + offsetFromDiskExtentStart;
							}
							
							uint64_t intersectionStart = MAX(optimizedRangeStart, he->he_offset);
							uint64_t intersectionEnd = MIN(optimizedRangeStart + optimizedRangeLength, he->he_offset + he->he_length);
							if (intersectionStart < intersectionEnd) {
								he->he_flags |= BC_HE_OPTIMIZED;
								optimizedReadBytes += intersectionEnd - intersectionStart;
							}
							
							DLOG_OFT("  entry %d: %#-12llx %#-8llx %#-12llx %5u%s%s from disk %#-12llx file %#-12llx %#-8llx %#-12llx (file range %#llx-%#llx (%#llx bytes) overlap %#llx-%#llx (%#llx bytes))",
									 he_index,
									 he->he_offset, he->he_length,
									 he->he_crypto_offset,
									 he->he_pid,
									 he->he_flags & BC_HE_OPTIMIZED ? " optimized" :
									 he->he_flags & BC_HE_HIT	 ? " hit"	 :
									 he->he_flags & BC_HE_WRITE  ? " write"  :
									 he->he_flags & BC_HE_TAG	 ? " tag"	 : " miss",
									 he->he_flags & BC_HE_SHARED ? " shared" : "",
									 diskExtent->offset, dstreamFileOffsetForDiskExtent, diskExtent->length, diskExtent->cpoff,
									 fileRange->offset, fileRange->offset + fileRange->length, fileRange->length,
									 fileRangeStart, fileRangeEnd, fileRangeEnd - fileRangeStart);
							
							if (fileRange->offset + fileRange->length > dstreamFileOffsetForDiskExtent + diskExtent->length) {
								// This file range needs more data from this dstream, break out of the fileRange loop to iterate to the next dstream extent
								break;
							}
							
							// Completed this file range, see if the next one is also satistified by this disk extent
							for (fileRangeIndex++; fileRangeIndex < dstreamFileMap->numRanges; fileRangeIndex++) {
								fileRange = dstreamFileMap->fileRanges + fileRangeIndex;
								if (fileRange->length != 0) {
									break;
								}
							}
							if (fileRangeIndex >= dstreamFileMap->numRanges) {
								break;
							}
						}
						
						if (fileRangeIndex >= dstreamFileMap->numRanges) {
							break;
						}
						
						dstreamFileOffsetForDiskExtent += diskExtent->length;
					}
					
				}
				
				BC_inode_disk_map_destroy(inodeDiskMap);
				
			} else {
				DLOG("No disk map for inode %llu", inode);
			}
			
			
			mount_optimization_info->hdd_stats.ssup_bytes_requested += readBytes;
			if (optimizedBytes > 0) {
				mount_optimization_info->hdd_stats.ssup_inodes_optimized++;
				mount_optimization_info->hdd_stats.ssup_bytes_optimized += optimizedBytes;
			}
			if (optimizedBytes > optimizedReadBytes) {
				mount_optimization_info->hdd_stats.ssup_bytes_surplus += (optimizedBytes - optimizedReadBytes);
			}
			if (readBytes > optimizedReadBytes) {
				mount_optimization_info->hdd_stats.ssup_bytes_nonoptimized += (readBytes - optimizedReadBytes);
			}
			mount_optimization_info->hdd_stats.ssup_reads_optimized += historyInode->numEntries;
			
		}

		// Zero-out any remaining history entries we no longer need for this inode
		uint64_t bytesRemoved = 0;
		for (int i = historyInodeExtentEntryIndex; i < historyInode->numEntries; i++) {
			int he_index = historyInode->entryIndexes[i];
			bytesRemoved += hc->h_entries[he_index].he_length;
			hc->h_entries[he_index].he_length = 0;
			
			DLOG_OFT("  entry %d zero'ed out", he_index);
		}
		if (historyInodeExtentEntryIndex == 0) {
			numBytesDropped += bytesRemoved;
			numInodesDropped ++;
		}
	}
	
	if (numInodesDropped > 0) {
		DLOG("%d inodes (%llu bytes) dropped from playlist due to file deleted/moved/error (%d after optimization occurred) for mount %s", numInodesDropped, numBytesDropped, numInodesDropped - (numInodes - numInodesRequested), uuid_string(hm->hm_uuid));
	}
	
	if (lowestOptimizedByteAddress != UINT64_MAX && highestOptimizedByteAddress != 0) {
		mount_optimization_info->hdd_stats.ssup_optimization_range_length = highestOptimizedByteAddress - lowestOptimizedByteAddress;
	}
	
	mount_optimization_info->lowestOptimizedByteAddress = lowestOptimizedByteAddress;
	mount_optimization_info->highestOptimizedByteAddress = highestOptimizedByteAddress;
	mount_optimization_info->hdd_stats.ssup_inodes_requested = numInodesRequested;

	DLOG("%llu reads, %llu inodes, %llu bytes optimized on %s for boot into %#llx-%#llx (%llu bytes) (%.1f%% efficient) (%.0f%% out of %llu inodes, %.0f%% out of %llu bytes, %llu bytes (%.0f%%) surplus, %llu bytes (%.0f%%) nonoptimized), this boot had %llu reads, %llu inodes, %llu bytes previously optimized",
		 mount_optimization_info->hdd_stats.ssup_reads_optimized,
		 mount_optimization_info->hdd_stats.ssup_inodes_optimized,
		 mount_optimization_info->hdd_stats.ssup_bytes_optimized,
		 uuid_string(hm->hm_uuid),
		 lowestOptimizedByteAddress,
		 highestOptimizedByteAddress,
		 mount_optimization_info->hdd_stats.ssup_optimization_range_length,
		 (mount_optimization_info->hdd_stats.ssup_optimization_range_length != 0) ? ((double)mount_optimization_info->hdd_stats.ssup_bytes_optimized / (double)mount_optimization_info->hdd_stats.ssup_optimization_range_length * 100.0) : 0,
		 mount_optimization_info->hdd_stats.ssup_inodes_requested != 0 ? ((double)mount_optimization_info->hdd_stats.ssup_inodes_optimized / (double)mount_optimization_info->hdd_stats.ssup_inodes_requested * 100.0) : 0,
		 mount_optimization_info->hdd_stats.ssup_inodes_requested,
		 mount_optimization_info->hdd_stats.ssup_bytes_requested != 0 ? ((double)mount_optimization_info->hdd_stats.ssup_bytes_optimized / (double)mount_optimization_info->hdd_stats.ssup_bytes_requested * 100.0) : 0,
		 mount_optimization_info->hdd_stats.ssup_bytes_requested,
		 mount_optimization_info->hdd_stats.ssup_bytes_surplus,
		 mount_optimization_info->hdd_stats.ssup_bytes_requested > 0 ? ((double)mount_optimization_info->hdd_stats.ssup_bytes_surplus / (double)mount_optimization_info->hdd_stats.ssup_bytes_requested * 100.0) : 0,
		 mount_optimization_info->hdd_stats.ssup_bytes_nonoptimized,
		 mount_optimization_info->hdd_stats.ssup_bytes_requested > 0 ? ((double)mount_optimization_info->hdd_stats.ssup_bytes_nonoptimized / (double)mount_optimization_info->hdd_stats.ssup_bytes_requested * 100.0) : 0,
		 mount_optimization_info->hdd_stats.ssup_reads_already_optimized,
		 mount_optimization_info->hdd_stats.ssup_inodes_already_optimized,
		 mount_optimization_info->hdd_stats.ssup_bytes_already_optimized);
	
}

// <rdar://problem/41958867> Pause/continue BootCache APFS defragmentation based on user activity (and shutdown)
static char* current_container_being_optimized = NULL;
static os_unfair_lock current_container_lock = OS_UNFAIR_LOCK_INIT;

int
BC_pause_optimizations(void)
{
	int ret = 0;
	
	os_unfair_lock_lock(&current_container_lock);
	
	if (current_container_being_optimized) {
		ret = APFSCancelContainerResize(current_container_being_optimized);
		if (ret != 0) {
			LOG("BC_pause_optimizations for %s failed: %#x", current_container_being_optimized, ret);
		} else {
			DLOG("BC_pause_optimizations for %s succeeded", current_container_being_optimized);
		}
	} else {
		DLOG("BC_pause_optimizations no current containter being optimized");
		ret = -1;
	}
	
	os_unfair_lock_unlock(&current_container_lock);
	
	return ret;
}

static void
_BC_update_current_container_being_optimized(uuid_t volume_uuid)
{
	
	char *bsd_disk = NULL;
	if (!uuid_is_null(volume_uuid)) {
		apfs_mount_for_uuid(volume_uuid, NULL, &bsd_disk);
		if (bsd_disk) {
			char* temp = bsd_disk;
			if (0 == strncmp("/dev/", temp, strlen("/dev/"))) {
				temp += strlen("/dev/");
			}
			if (0 == strncmp("disk", temp, strlen("disk"))) {
				char* s = strrchr(temp + 4, 's');
				if (s) {
					DLOG("Chopping %s at %s to get container", bsd_disk, s);
					*s = '\0';
				}
			}
		} else {
			LOG_ERRNO("Unable to get bsd disk for container %s", uuid_string(volume_uuid));
		}
	}
	
	os_unfair_lock_lock(&current_container_lock);
	
	if (current_container_being_optimized) {
		free(current_container_being_optimized);
	}
	current_container_being_optimized = bsd_disk;
	
	DLOG("Updated containter being optimized (volume %s) to %s", uuid_string(volume_uuid), bsd_disk);

	os_unfair_lock_unlock(&current_container_lock);
}

static struct BC_userspace_hdd_optimizations
_BC_get_hdd_container_stats(const struct bc_optimization_info* optimization_info, int container_start_index, int container_end_index, uint64_t* lowestOptimizedByteAddress_out, uint64_t* highestOptimizedByteAddress_out)
{
	struct BC_userspace_hdd_optimizations userspace_hdd_optimizations_container = {0};
	uint64_t lowestOptimizedByteAddress_container = UINT64_MAX;
	uint64_t highestOptimizedByteAddress_container = 0;
	
	for (int mount_optimization_info_index = container_start_index; mount_optimization_info_index <= container_end_index; mount_optimization_info_index++) {
		
		if (lowestOptimizedByteAddress_container > optimization_info->mount_optimization_infos[mount_optimization_info_index]->lowestOptimizedByteAddress) {
			lowestOptimizedByteAddress_container = optimization_info->mount_optimization_infos[mount_optimization_info_index]->lowestOptimizedByteAddress;
		}
		if (highestOptimizedByteAddress_container < optimization_info->mount_optimization_infos[mount_optimization_info_index]->highestOptimizedByteAddress) {
			highestOptimizedByteAddress_container = optimization_info->mount_optimization_infos[mount_optimization_info_index]->highestOptimizedByteAddress;
		}
		
		userspace_hdd_optimizations_container.ssup_inodes_requested += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_inodes_requested;
		userspace_hdd_optimizations_container.ssup_inodes_optimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_inodes_optimized;
		userspace_hdd_optimizations_container.ssup_inodes_already_optimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_inodes_already_optimized;
		userspace_hdd_optimizations_container.ssup_bytes_requested += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_bytes_requested;
		userspace_hdd_optimizations_container.ssup_bytes_optimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_bytes_optimized;
		userspace_hdd_optimizations_container.ssup_bytes_surplus += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_bytes_surplus;
		userspace_hdd_optimizations_container.ssup_bytes_nonoptimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_bytes_nonoptimized;
		userspace_hdd_optimizations_container.ssup_bytes_already_optimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_bytes_already_optimized;
		userspace_hdd_optimizations_container.ssup_reads_optimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_reads_optimized;
		userspace_hdd_optimizations_container.ssup_reads_already_optimized += optimization_info->mount_optimization_infos[mount_optimization_info_index]->hdd_stats.ssup_reads_already_optimized;
	}
	
	uint64_t optimizationRangeLength_container = highestOptimizedByteAddress_container != 0 ? (highestOptimizedByteAddress_container - lowestOptimizedByteAddress_container) : 0;
	
	userspace_hdd_optimizations_container.ssup_optimization_range_length = optimizationRangeLength_container;

	if (lowestOptimizedByteAddress_out) {
		*lowestOptimizedByteAddress_out = lowestOptimizedByteAddress_container;
	}
	if (highestOptimizedByteAddress_out) {
		*highestOptimizedByteAddress_out = highestOptimizedByteAddress_container;
	}
	
	return userspace_hdd_optimizations_container;
}

/*
 * Optimize the history recording to provide better performance next boot
 * On APFS Fusion, this will promote blocks to the fast device.
 *    For any blocks that are promoted, they are removed from the history (length set to 0)
 * On APFS HDD, this may reorder the blocks on disk to provide optimized read ordering
 *    For any blocks that are moved, the history entries are updated to provide the new locations on disk
 *
 * Partial success is possible, and any errors will be logged
 *
 *
 * PAUSING / CONTINUING
 * The optimization operation can be paused by calling BC_pause_optimizations()
 * from a separate thread. The BootCache playlist will be updated with any
 * optimizations that have already completed. BC_optimize_history will return
 * ECANCELED and *optimization_info_inout will be filled in, if provided.
 *
 * In order to continue the optimization later, the caller must pass in the same
 * hc that was provided previously, and the *optimization_info_inout that was returned
 * previously.
 *
 * If the caller does not wish to continue later, the *optimization_info_inout must be freed
 * via BC_free_optimization_info().
 *
 *
 * Returns 0 on success, ECANCELED on being paused, and other values for non-continuable errors
 */
int
BC_optimize_history(struct BC_history * hc, struct bc_optimization_info** optimization_info_inout, bool fusion_only)
{
	ASSERT(hc);
	ASSERT(!optimization_info_inout || !*optimization_info_inout || (*optimization_info_inout)->hc == hc);
	
	bool optimizationPaused = false;
	
	struct BC_userspace_hdd_optimizations userspace_hdd_optimizations = {
		.ssup_inodes_requested = 0,
		.ssup_inodes_optimized = 0,
		.ssup_bytes_requested = 0,
		.ssup_bytes_optimized = 0,
		.ssup_bytes_surplus = 0,
		.ssup_bytes_nonoptimized = 0,
		.ssup_reads_optimized = 0,
		.ssup_reads_already_optimized = 0,
		.ssup_inodes_already_optimized = 0,
		.ssup_bytes_already_optimized = 0,
		.ssup_optimization_range_length = 0,
	};
	
	struct BC_userspace_fusion_optimizations userspace_fusion_optimizations = {
		.ssup_inodes_requested = 0,
		.ssup_inodes_optimized = 0,
		.ssup_bytes_requested = 0,
		.ssup_bytes_optimized = 0,
		.ssup_reads_optimized = 0,
	};
	

	struct bc_optimization_info* optimization_info = NULL;
	
	bool resumedAfterPaused = false;

	if (optimization_info_inout && *optimization_info_inout) {
		// Already have the optimization_info from last time (before we were paused by BC_pause_optimizations)
		// Don't need to go through the inode mapping work again, nor promote on fusion
		optimization_info = *optimization_info_inout;
		resumedAfterPaused = true;
	} else {
		// First time being called
		
		optimization_info = calloc(1, sizeof(*optimization_info));
		if (!optimization_info) {
			return ENOMEM;
		}
		optimization_info->hc = hc;
		
		for (uint hm_index = 0; hm_index < hc->h_nmounts; hm_index++) {
			struct BC_history_mount *hm = hc->h_mounts + hm_index;
			if (uuid_is_null(hm->hm_uuid)) {
				DLOG("Not optimizing BootCache recording for mount %d: NULL UUID", hm_index);
				continue;
			}
			
			if (hm->hm_nentries == 0) {
				DLOG("Not optimizing BootCache recording for %s: %d entries", uuid_string(hm->hm_uuid), hm->hm_nentries);
				continue;
			}
			
			if (!(hm->hm_fs_flags & BC_FS_APFS)) {
				DLOG("Not optimizing BootCache recording for %s with %d entries: non-APFS", uuid_string(hm->hm_uuid), hm->hm_nentries);
				continue;
			}
			
			
			
			
			char *mount_name = NULL;
			apfs_mount_for_uuid(hm->hm_uuid, &mount_name, NULL);
			if (!mount_name) {
				DLOG_ERRNO("Unable to optimize BootCache for %s: no mount found", uuid_string(hm->hm_uuid));
				continue;
			}
			
			uint historyInodesCount = 0;
			uint historyInodesCapacity = MIN(hm->hm_nentries, 4096);
			struct BC_history_inode* historyInodes = calloc(historyInodesCapacity, sizeof(*historyInodes));
			if (!historyInodes) {
				LOG("Unable to optimize BootCache: unable to allocate buffer for %d inodes", historyInodesCapacity);
				BC_free_optimization_info(optimization_info);
				optimization_info = NULL;
				free(mount_name);
				return ENOMEM;
			}
			
			// Keep the same ordering of inodes as the chronological order of I/O,
			// so we prefer to promote the files read earlier in boot in the case we run out of space or get interrupted
			// We need separate by mount, but the first mount is always the root one, and that's the one we want to prioritize, so... good enough.
			
			{ // for local variable scoping
				for (uint he_index = 0; he_index < hc->h_nentries; he_index++) {
					struct BC_history_entry *he = hc->h_entries + he_index;
					if (he->he_mount_idx == hm_index && he->he_length != 0 && he->he_inode != 0 && !(he->he_flags & BC_HE_WRITE) && !(he->he_flags & BC_HE_SHARED)) {
						
						// De-dup inodes since we may get multiple entries for the same file
						uint historyInodeIdx = 0;
						for (historyInodeIdx = 0; historyInodeIdx < historyInodesCount; historyInodeIdx++) {
							if (historyInodes[historyInodeIdx].inode == he->he_inode) {
								break;
							}
						}
						
						if (historyInodeIdx >= historyInodesCount) {
							ASSERT(historyInodeIdx == historyInodesCount);
							// New inode
							
							if (historyInodesCount >= historyInodesCapacity) {
								// Exceeding malloc'ed buffer size: grow buffer
								historyInodesCapacity *= 2;
								historyInodes = reallocf(historyInodes, historyInodesCapacity * sizeof(*historyInodes));
								if (!historyInodes) {
									LOG("Unable to optimize BootCache: unable to allocate buffer for %d inodes", historyInodesCapacity);
									BC_free_optimization_info(optimization_info);
									optimization_info = NULL;
									free(mount_name);
									return ENOMEM;
								}
							
							}
							historyInodesCount++;
							
							struct BC_history_inode* historyInode = historyInodes + historyInodeIdx;
							
							historyInode->inode = he->he_inode;
							historyInode->entryIndexes = NULL;
							historyInode->numEntries = 0;
						}
						
						struct BC_history_inode* historyInode = historyInodes + historyInodeIdx;
						historyInode->numEntries++;
						historyInode->entryIndexes = reallocf(historyInode->entryIndexes, historyInode->numEntries * sizeof(*historyInode->entryIndexes));
						ASSERT(historyInode->entryIndexes);
						historyInode->entryIndexes[historyInode->numEntries - 1] = he_index;
					}
				}
				
#ifdef DEBUG_OPTIMIZATION_FILE_TRANSLATION
#ifdef DEBUG
				DLOG_OFT("%s: %d inodes for %d entries", uuid_string(hm->hm_uuid), historyInodesCount, hm->hm_nentries);
				for (int historyInodeIdx = 0; historyInodeIdx < historyInodesCount; historyInodeIdx++) {
					struct BC_history_inode* historyInode = historyInodes + historyInodeIdx;
					DLOG_OFT(" inode %llu: %d entries", historyInode->inode, historyInode->numEntries);
					for (int historyInodeEntryIdx = 0; historyInodeEntryIdx < historyInode->numEntries; historyInodeEntryIdx++) {
						int he_index = historyInode->entryIndexes[historyInodeEntryIdx];
						struct BC_history_entry *he = hc->h_entries + he_index;

						DLOG_OFT("  entry %d: inode %llu %#-12llx %#-8llx %#-12llx %5u%s%s",
								 he_index,
								 he->he_inode,
								 he->he_offset,
								 he->he_length,
								 he->he_crypto_offset,
								 he->he_pid,
								 he->he_flags & BC_HE_OPTIMIZED ? " optimized" :
								 he->he_flags & BC_HE_HIT	 ? " hit"	 :
								 he->he_flags & BC_HE_WRITE  ? " write"  :
								 he->he_flags & BC_HE_TAG	 ? " tag"	 : " miss",
								 he->he_flags & BC_HE_SHARED ? " shared" : "");
					}
				}
#endif
#endif

			}
			
			if (hm->hm_fs_flags & BC_FS_APFS_FUSION) {
				
				// For Fusion volumes, we promote the inodes to the fast device, and then throw out our history (the BootCache doesn't cache anything on SSDs), so we don't need to keep track of where the files end up
				
				struct BC_userspace_fusion_optimizations userspace_fusion_optimizations_volume = BC_optimize_history_mount_fusion(hc, hm_index, mount_name, historyInodes, historyInodesCount);
				
				userspace_fusion_optimizations.ssup_inodes_requested += userspace_fusion_optimizations_volume.ssup_inodes_requested;
				userspace_fusion_optimizations.ssup_inodes_optimized += userspace_fusion_optimizations_volume.ssup_inodes_optimized;
				userspace_fusion_optimizations.ssup_bytes_requested += userspace_fusion_optimizations_volume.ssup_bytes_requested;
				userspace_fusion_optimizations.ssup_bytes_optimized += userspace_fusion_optimizations_volume.ssup_bytes_optimized;
				userspace_fusion_optimizations.ssup_reads_optimized += userspace_fusion_optimizations_volume.ssup_reads_optimized;

				for (int i = 0; i < historyInodesCount; i++) {
					if (historyInodes[i].entryIndexes) {
						free(historyInodes[i].entryIndexes);
					}
				}
				free(historyInodes);
				
			} else {
				// For HDD machines, for all APFS containers, we need to do optimizations in three parts:
				// 1. For all volumes in the container, translate our inode + disk offset/length playlist to inode + file offset/length
				// 2. For all volumes in the container, make the apfs fsctl call to pass all the inodes to apfs to optimze (the last volume's fsctl will trigger the optimizations)
				// 3. For all volumes in the container, translate back our inode + file offset/length into disk offset/length for the new locations of the files
				// We can't just go through all the steps as we iterate over the volumes, because the optimization step can only be called once per container.
				struct bc_mount_optimization_info* mount_optimization_info = BC_mount_copy_file_relative_history(hc, hm_index, mount_name, historyInodes, historyInodesCount);
				if (mount_optimization_info) {
					optimization_info->num_mount_optimization_infos++;
					optimization_info->mount_optimization_infos = realloc(optimization_info->mount_optimization_infos, sizeof(*optimization_info->mount_optimization_infos) * optimization_info->num_mount_optimization_infos);
					optimization_info->mount_optimization_infos[optimization_info->num_mount_optimization_infos - 1] = mount_optimization_info;
				} else {
					for (int i = 0; i < historyInodesCount; i++) {
						if (historyInodes[i].entryIndexes) {
							free(historyInodes[i].entryIndexes);
						}
					}
					free(historyInodes);
				}
			}
			
			free(mount_name);
		}
		
		// Done doing block -> inode/offset translation (and optimizing fusion)
		
		BC_set_userspace_fusion_optimization_stats(&userspace_fusion_optimizations);
	}
	
	
	// Now that we have the playlist in inode/offset form for all teh HDD volumes, optimize them
	if (optimization_info->mount_optimization_infos) {
		if (fusion_only) {
			optimizationPaused = true;
		} else {
			
			bool anyOptimizationAttempted = false;
			
			// Sort by container, so we do one container at a time
			qsort_b(optimization_info->mount_optimization_infos, optimization_info->num_mount_optimization_infos, sizeof(*optimization_info->mount_optimization_infos), ^int(const void * m1, const void * m2) {
				const struct bc_mount_optimization_info * const * mount_optimization_info1 = m1;
				const struct bc_mount_optimization_info * const * mount_optimization_info2 = m2;
				return uuid_compare((*mount_optimization_info1)->hc->h_mounts[(*mount_optimization_info1)->hm_index].hm_group_uuid, (*mount_optimization_info2)->hc->h_mounts[(*mount_optimization_info2)->hm_index].hm_group_uuid);
			});
			
			int container_start_index = 0;
			while (!optimizationPaused && container_start_index < optimization_info->num_mount_optimization_infos) { 		// Loop over all the containers (iterator at the end of the while loop jumps to the first volume outside this container)
				
				uuid_t* container_uuid = &(optimization_info->mount_optimization_infos[container_start_index]->hc->h_mounts[optimization_info->mount_optimization_infos[container_start_index]->hm_index].hm_group_uuid);
				
				int container_end_index = optimization_info->num_mount_optimization_infos - 1;
				for (int mount_optimization_info_index = container_start_index + 1; mount_optimization_info_index < optimization_info->num_mount_optimization_infos; mount_optimization_info_index++) {
					if (0 != uuid_compare(*container_uuid, optimization_info->mount_optimization_infos[mount_optimization_info_index]->hc->h_mounts[optimization_info->mount_optimization_infos[mount_optimization_info_index]->hm_index].hm_group_uuid)) {
						container_end_index = mount_optimization_info_index - 1;
						break;
					}
				}
				
				struct BC_userspace_hdd_optimizations userspace_hdd_optimizations_container;
				bool alreadyCompletedOptimizationsForThisContainer = optimization_info->mount_optimization_infos[container_start_index]->completed;
				
				if (alreadyCompletedOptimizationsForThisContainer) {
					// Already completed optimizations, just grab the saved statistics
#if DEBUG
					uuid_t* uuid = &(optimization_info->mount_optimization_infos[container_start_index]->hc->h_mounts[optimization_info->mount_optimization_infos[container_start_index]->hm_index].hm_uuid);
					
					DLOG("Already optimized container %s (for volume %s)", uuid_string(*container_uuid), uuid_string(*uuid));
#endif
					
					userspace_hdd_optimizations_container = _BC_get_hdd_container_stats(optimization_info, container_start_index, container_end_index, NULL, NULL);
					
				} else {
					// Haven't completed optimizations (either haven't started at all, or started and were interrupted by BC_pause_optimizations())
					uuid_t* uuid = &(optimization_info->mount_optimization_infos[container_start_index]->hc->h_mounts[optimization_info->mount_optimization_infos[container_start_index]->hm_index].hm_uuid);
					
					DLOG("Optimizing container %s (for volume %s)", uuid_string(*container_uuid), uuid_string(*uuid));
					_BC_update_current_container_being_optimized(*uuid);
					
					int numInodesRequested_container = 0;
					bool containerOptimizationAttempted = false;
					for (int mount_optimization_info_index = container_start_index; mount_optimization_info_index <= container_end_index; mount_optimization_info_index++) {
						bool isLastVolumeInContainer = (mount_optimization_info_index == container_end_index);
						
						if (isLastVolumeInContainer) {
							// The real optimization work only occurs for the last volume in the container.
							// Before that we're just building up a queue of inodes, which we don't want to consider "optimization" because we rely on knowing the optimization range after the first optimixation attempt when printing statistics
							
							containerOptimizationAttempted = true;
							if (!anyOptimizationAttempted) { // Treat optimizing multiple containers as a single optimization attempt
								anyOptimizationAttempted = true;
								optimization_info->userspace_hdd_optimization_state.ssup_num_optimizations_attempted++;
								
								optimization_info->userspace_hdd_optimization_state.ssup_optimization_state = BC_HDD_OPTIMIZATION_STATE_IN_PROGRESS;
								BC_set_userspace_hdd_optimization_state(&optimization_info->userspace_hdd_optimization_state);
							}
							
							LOG("Optimizing APFS container %s for boot performance", uuid_string(*container_uuid));
						}
						
						int err = BC_optimize_inodes_hdd(optimization_info->mount_optimization_infos[mount_optimization_info_index], isLastVolumeInContainer);
						
						if (err == ECANCELED) {
							optimizationPaused = true;
						}
						
						numInodesRequested_container += optimization_info->mount_optimization_infos[mount_optimization_info_index]->numInodesRequested;
						
						if (isLastVolumeInContainer) {
							
							// Fill in the optimization range for all the other volumes in this container
							for (int temp_index = container_start_index; temp_index < mount_optimization_info_index; temp_index++) {
								optimization_info->mount_optimization_infos[temp_index]->optimizedRangeStart = optimization_info->mount_optimization_infos[mount_optimization_info_index]->optimizedRangeStart;
								optimization_info->mount_optimization_infos[temp_index]->optimizedRangeLength = optimization_info->mount_optimization_infos[mount_optimization_info_index]->optimizedRangeLength;
							}
						}
						
						if (optimizationPaused) {
							break;
						}
					}
					
					if (!containerOptimizationAttempted) {
						// We paused before getting to the last volume in a container, so didn't do any work and can short-circuit here
						DLOG("Paused before optimization in container %s could start", uuid_string(*container_uuid));
						ASSERT(optimizationPaused);
						break;
					}
					
					if (optimizationPaused) {
						LOG("BootCache optimization paused");
					}
					
					DLOG("Optimized %d inodes in container %s to %#llx-%#llx (%llu length), checking where they ended up on disk...", numInodesRequested_container, uuid_string(*container_uuid), optimization_info->mount_optimization_infos[container_start_index]->optimizedRangeStart, optimization_info->mount_optimization_infos[container_start_index]->optimizedRangeStart + optimization_info->mount_optimization_infos[container_start_index]->optimizedRangeLength, optimization_info->mount_optimization_infos[container_start_index]->optimizedRangeLength);
					
					for (int mount_optimization_info_index = container_start_index; mount_optimization_info_index <= container_end_index; mount_optimization_info_index++) {
						ASSERT(! optimization_info->mount_optimization_infos[mount_optimization_info_index]->completed);
						BC_mount_fixup_history_after_optimizations(optimization_info->mount_optimization_infos[mount_optimization_info_index], resumedAfterPaused);
						if (!optimizationPaused) {
							optimization_info->mount_optimization_infos[mount_optimization_info_index]->completed = true;
						}
					}
					
					uint64_t lowestOptimizedByteAddress_container = UINT64_MAX;
					uint64_t highestOptimizedByteAddress_container = 0;
					userspace_hdd_optimizations_container = _BC_get_hdd_container_stats(optimization_info, container_start_index, container_end_index, &lowestOptimizedByteAddress_container, &highestOptimizedByteAddress_container);
					
					LOG("%llu reads, %llu inodes, %llu bytes optimized in container %s for boot into %#llx-%#llx (%llu bytes) (%.1f%% efficient) (%.0f%% out of %llu inodes read, %.0f%% out of %llu bytes read, %llu bytes (%.0f%%) surplus, %llu bytes (%.0f%%) nonoptimized), this boot had %llu reads, %llu inodes, %llu bytes, previously optimized",
						userspace_hdd_optimizations_container.ssup_reads_optimized,
						userspace_hdd_optimizations_container.ssup_inodes_optimized,
						userspace_hdd_optimizations_container.ssup_bytes_optimized,
						uuid_string(*container_uuid),
						lowestOptimizedByteAddress_container,
						highestOptimizedByteAddress_container,
						userspace_hdd_optimizations_container.ssup_optimization_range_length,
						(userspace_hdd_optimizations_container.ssup_optimization_range_length != 0) ? ((double)userspace_hdd_optimizations_container.ssup_bytes_optimized / (double)userspace_hdd_optimizations_container.ssup_optimization_range_length * 100.0) : 0,
						userspace_hdd_optimizations_container.ssup_inodes_requested != 0 ? ((double)userspace_hdd_optimizations_container.ssup_inodes_optimized / (double)userspace_hdd_optimizations_container.ssup_inodes_requested * 100.0) : 0,
						userspace_hdd_optimizations_container.ssup_inodes_requested,
						userspace_hdd_optimizations_container.ssup_bytes_requested != 0 ? ((double)userspace_hdd_optimizations_container.ssup_bytes_optimized / (double)userspace_hdd_optimizations_container.ssup_bytes_requested * 100.0) : 0,
						userspace_hdd_optimizations_container.ssup_bytes_requested,
						userspace_hdd_optimizations_container.ssup_bytes_surplus,
						userspace_hdd_optimizations_container.ssup_bytes_requested > 0 ? ((double)userspace_hdd_optimizations_container.ssup_bytes_surplus / (double)userspace_hdd_optimizations_container.ssup_bytes_requested * 100.0) : 0,
						userspace_hdd_optimizations_container.ssup_bytes_nonoptimized,
						userspace_hdd_optimizations_container.ssup_bytes_requested > 0 ? ((double)userspace_hdd_optimizations_container.ssup_bytes_nonoptimized / (double)userspace_hdd_optimizations_container.ssup_bytes_requested * 100.0) : 0,
						userspace_hdd_optimizations_container.ssup_reads_already_optimized,
						userspace_hdd_optimizations_container.ssup_inodes_already_optimized,
						userspace_hdd_optimizations_container.ssup_bytes_already_optimized);
					
				}
				
				// Sum up the stats across all the containers we optimized (including containers we already optimized)
				userspace_hdd_optimizations.ssup_reads_optimized += userspace_hdd_optimizations_container.ssup_reads_optimized;
				userspace_hdd_optimizations.ssup_reads_already_optimized += userspace_hdd_optimizations_container.ssup_reads_already_optimized;
				userspace_hdd_optimizations.ssup_inodes_requested += userspace_hdd_optimizations_container.ssup_inodes_requested;
				userspace_hdd_optimizations.ssup_inodes_optimized += userspace_hdd_optimizations_container.ssup_inodes_optimized;
				userspace_hdd_optimizations.ssup_inodes_already_optimized += userspace_hdd_optimizations_container.ssup_inodes_already_optimized;
				userspace_hdd_optimizations.ssup_bytes_requested += userspace_hdd_optimizations_container.ssup_bytes_requested;
				userspace_hdd_optimizations.ssup_bytes_optimized += userspace_hdd_optimizations_container.ssup_bytes_optimized;
				userspace_hdd_optimizations.ssup_bytes_surplus += userspace_hdd_optimizations_container.ssup_bytes_surplus;
				userspace_hdd_optimizations.ssup_bytes_nonoptimized += userspace_hdd_optimizations_container.ssup_bytes_nonoptimized;
				userspace_hdd_optimizations.ssup_bytes_already_optimized += userspace_hdd_optimizations_container.ssup_bytes_already_optimized;
				userspace_hdd_optimizations.ssup_optimization_range_length += userspace_hdd_optimizations_container.ssup_optimization_range_length;
				
				container_start_index = container_end_index + 1;
			}
			
			// Done optimizing
			
			uuid_t null_uuid;
			uuid_clear(null_uuid);
			_BC_update_current_container_being_optimized(null_uuid);
		}
	} else {
		// No HDD volumes to optimize
	}
	
	BC_set_userspace_hdd_optimization_stats(&userspace_hdd_optimizations);
	
	optimization_info->userspace_hdd_optimization_state.ssup_optimization_state = (optimizationPaused && optimization_info_inout) ? BC_HDD_OPTIMIZATION_STATE_PAUSED : BC_HDD_OPTIMIZATION_STATE_COMPLETE;
	BC_set_userspace_hdd_optimization_state(&optimization_info->userspace_hdd_optimization_state);

	if (optimizationPaused) {
		if (optimization_info_inout) {
			*optimization_info_inout = optimization_info;
		} else {
			LOG("BootCache optimization paused, but caller won't be able to resume");
			BC_free_optimization_info(optimization_info);
			optimization_info = NULL;
		}
		return ECANCELED;
	} else {
		if (optimization_info_inout) {
			*optimization_info_inout = NULL;
		}
		BC_free_optimization_info(optimization_info);
		optimization_info = NULL;
		return 0;
	}
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
				
#ifdef DEBUG
				BC_write_playlist("/var/tmp/BootCache.playlist", pc);
#endif
				
			}
			
		} else {
			DLOG("Empty playlist");
			pc = NULL;
		}
	}

	ret = _BC_start(pc, true);
	
	PC_FREE_ZERO(new_pc);

	return ret;
}

/*
 * Start the cache, feed it the playlist if provided.
 *
 * Optionally start recording as well
 */
static int
_BC_start(const struct BC_playlist *pc, bool record)
{
	
	struct BC_command bc;
	
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = record ? BC_OP_START : BC_OP_START_NORECORDING;
	if (pc) {
		bc.bc_data1		 = (uintptr_t)	   pc->p_mounts;
		bc.bc_data1_size = (unsigned int) (pc->p_nmounts * sizeof(*pc->p_mounts));
		bc.bc_data2		 = (uintptr_t)	   pc->p_entries;
		bc.bc_data2_size = (unsigned int) (pc->p_nentries * sizeof(*pc->p_entries));
	} else {
		if (!record) {
			return EINVAL;
		}
		bc.bc_data1		 = 0;
		bc.bc_data1_size = 0;
		bc.bc_data2		 = 0;
		bc.bc_data2_size = 0;
	}
	
	int ret = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	
	if (ret != 0 && errno != EALREADY) {
		ret = errno;
		LOG_ERRNO("Unable to enable BootCache");
		return ret;
	}
	
	if (record && ret != EALREADY) {
		// We started recording (and weren't already recording), start omap recording
		// (If we were already recording, this call resets the batches the omaps are tagged with to batch 0,
		// and we wouldn't be able to playback any new mounts that this call finds anyway)
		ret = BC_start_omap_recording_for_all_mounts();
	}
	
	return ret;
}

/*
 * Reset the cache back to a pristine state, ready to accept a playlist and start recording
 */
static int
_BC_reset(void)
{
	struct BC_command bc;
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_RESET;
	return(sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc)) ? errno : 0);
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
		
		error = errno ?: EINVAL;
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
	
	DLOG("Collected %d mounts, %d entries and %d omaps", (phc && *phc) ? (*phc)->h_nmounts : 0, (phc && *phc) ? (*phc)->h_nentries : 0, total_omaps);
#endif
	
	error = 0;
	
out:
	if (0 != error) {
		// On error, clear out OID tracking
		BC_stop_omap_recording_for_all_mounts();
		BC_fetch_omaps_for_all_mounts(NULL);
		HC_FREE_ZERO(hc);
	}
	
	/*
	 * Notify whoever cares that BootCache has stopped recording.
	 */
	notify_post("com.apple.system.private.bootcache.done");
	DLOG("Sent com.apple.system.private.bootcache.done");
	
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

static int
BC_start_omap_recording_for_all_mounts(void)
{
	// Clear cache so we get whatever is current
	clear_apfs_mount_cache();
	CFDictionaryRef cache = copy_apfs_mount_cache();
	
	int error = 0;
	
	CFDictionaryApplyFunction(cache, BC_start_omap_recording_for_mount_cache_entry, &error);
	
	CFRelease(cache);
	return error;
}



static int
BC_tag_omap_recording_for_mount(const char *mount, uint8_t batchNum)
{
	int32_t batchArg = batchNum + 1;
	int ret = fsctl(mount, APFSIOC_OMAP_TRACK_ENABLE, &batchArg, 0);
	if (ret != 0) {
		if (errno == ENOTTY) {
			DLOG("omap recording not supported for %s", mount);
			ret = 0;
		} else if (errno == EALREADY) {
			DLOG("omap recording already running batch %d for %s", batchNum, mount);
			ret = 0;
		} else {
			LOG_ERRNO("Unable to tag omap recording batch %d for %s", batchNum, mount);
		}
	} else {
		DLOG("Tagged omap recording batch %d for %s", batchNum, mount);
	}
	
	return ret;
}

struct BC_tag_omap_recording_for_mount_cache_entry_args {
	uint8_t batchNum;
	int error;
};

static void
BC_tag_omap_recording_for_mount_cache_entry(const void *key __unused, const void * __nonnull value, void * __nonnull context)
{
	// key is UUID of this mount
	// value is struct statfs* for this mount
	// context is a pointer to struct BC_tag_omap_recording_for_mount_cache_entry_args
	const struct statfs* statfsbuf = (const struct statfs*)value;
	int* perror = &((struct BC_tag_omap_recording_for_mount_cache_entry_args*)context)->error;
	
	int error = BC_tag_omap_recording_for_mount(statfsbuf->f_mntonname, ((struct BC_tag_omap_recording_for_mount_cache_entry_args*)context)->batchNum);
	
	if (error != 0) {
		*perror = error;
	}
}


static int
BC_tag_omap_recording_for_all_mounts(uint8_t batchNum)
{
	
	// Clear cache so we get whatever is current
	clear_apfs_mount_cache();
	CFDictionaryRef cache = copy_apfs_mount_cache();
	
	struct BC_tag_omap_recording_for_mount_cache_entry_args args = {
		.batchNum = batchNum,
		.error = 0,
	};
	
	CFDictionaryApplyFunction(cache, BC_tag_omap_recording_for_mount_cache_entry, &args);
	
	CFRelease(cache);
	return args.error;
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

static int
BC_stop_omap_recording_for_all_mounts(void)
{
	// Clear cache so we get whatever is current (in case some other process started recording while we were running on a mount that appeeared since we last checked)
	clear_apfs_mount_cache();
	CFDictionaryRef cache = copy_apfs_mount_cache();
	
	int error = 0;
	
	CFDictionaryApplyFunction(cache, BC_stop_omap_recording_for_mount_cache_entry, &error);
	
	CFRelease(cache);
	return error;
}

static int
BC_fetch_omaps_for_mount(const char *mount, struct BC_omap_history_mount *ohm)
{

	DLOG("Fetching omaps for mount %s", mount);

	int ret = 0;

	apfs_omap_track_report_t otr = {0};

	/* Stream down the records for the mount */
#define NUM_RECORDS_PER_REPORT 1024

	otr.otr_records = calloc(NUM_RECORDS_PER_REPORT, sizeof(apfs_omap_track_record_v2_t));
	otr.otr_flags = APFS_OMAP_TRACK_REPORT_V2_RECORDS;
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

		if (otr.otr_record_count > 0) {
			/* Expand array to accomodate the new otr records */
			ohm->ohm_omaps = reallocf(ohm->ohm_omaps, (ohm->ohm_nomaps + otr.otr_record_count) * sizeof(*ohm->ohm_omaps));
			if (ohm->ohm_omaps == NULL) {
				ret = errno;
				LOG("Unable to realloc more apfs_omap_record_ts");
				goto out;
			}
			
			// TODO: why not memmove the entire thing?
			for (int i = 0; i < otr.otr_record_count; i++) {
				// DLOG("omap = (%#llx, %#llx) batch %d", otr.otr_records[i].otr_oid, otr.otr_records[i].otr_oxid), otr.otr_records[i].otr_batch;
				ohm->ohm_omaps[i + ohm->ohm_nomaps] = ((apfs_omap_track_record_v2_t*)otr.otr_records)[i];
			}
			
			ohm->ohm_nomaps += otr.otr_record_count;
		}

	} while (otr.otr_records_remaining > 0);

	DLOG("Received a total of %d omaps for mount %s", ohm->ohm_nomaps, uuid_string(ohm->ohm_uuid));
	
	if (otr.otr_records) {
		free(otr.otr_records);
	}

	return 0;

out:
	if (otr.otr_records) {
		free(otr.otr_records);
	}
	ohm->ohm_nomaps = 0;
	if (ohm->ohm_omaps) {
		free(ohm->ohm_omaps);
		ohm->ohm_omaps = NULL;
	}
	return ret;
}

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

static int
BC_fetch_omaps_for_all_mounts(struct BC_omap_history** poh)
{
	if (poh) {
		*poh = NULL;
	}

	// Clear cache so we get whatever is current (in case some other process started recording while we were running on a mount that appeeared since we last checked)
	clear_apfs_mount_cache();
	CFDictionaryRef cache = copy_apfs_mount_cache();

	BC_fetch_omaps_for_mount_cache_entry_args args = {
		.error = 0,
		.oh = calloc(1, sizeof(*args.oh)),
	};
	
	if (args.oh == NULL) {
		LOG_ERRNO("Failed to allocate BC_omap_history");
		CFRelease(cache);
		return errno ?: ENOMEM;
	}
	
	CFDictionaryApplyFunction(cache, BC_fetch_omaps_for_mount_cache_entry, &args);
	CFRelease(cache);
	
	if (poh) {
		*poh = args.oh;
	} else {
		OH_FREE_ZERO(args.oh);
	}
	
	return args.error;

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
					*fs_flags_out |= BC_FS_APFS_ENCRYPTED;
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
static os_unfair_lock g_apfs_mount_cache_lock = OS_UNFAIR_LOCK_INIT;

static void _clear_apfs_mount_cache_locked(void) {

	if (g_mountbufs) {
		free(g_mountbufs);
		g_mountbufs = NULL;
	}
	if (g_apfs_mount_cache) {
		CFRelease(g_apfs_mount_cache);
		g_apfs_mount_cache = NULL;
	}
}

static void clear_apfs_mount_cache(void) {
	os_unfair_lock_lock(&g_apfs_mount_cache_lock);
	
	_clear_apfs_mount_cache_locked();
	
	os_unfair_lock_unlock(&g_apfs_mount_cache_lock);
}

static CFDictionaryRef copy_apfs_mount_cache(void) {
	os_unfair_lock_lock(&g_apfs_mount_cache_lock);

	if (g_mountbufs == NULL || g_apfs_mount_cache == NULL) {
		_clear_apfs_mount_cache_locked(); // Make sure everything's released
		
		// Populate the cache
		int num_mountbufs = getmntinfo_r_np(&g_mountbufs, MNT_NOWAIT);
		
		g_apfs_mount_cache = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, NULL);
		
		DLOG("%d mounts:", num_mountbufs);
		for (int i = 0; i < num_mountbufs; i++) {
			
			if (0 == strncmp(g_mountbufs[i].f_fstypename, "apfs", sizeof("apfs")+1)) {
				uuid_t mount_uuid;
				get_volume_uuid(g_mountbufs[i].f_mntonname, mount_uuid);
				if (!uuid_is_null(mount_uuid)) {
					DLOG("%s, %s, %s: %s", g_mountbufs[i].f_mntonname, g_mountbufs[i].f_mntfromname, g_mountbufs[i].f_fstypename, uuid_string(mount_uuid));
					CFUUIDRef uuid_cf = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *(CFUUIDBytes*)mount_uuid);
					CFDictionaryAddValue(g_apfs_mount_cache, uuid_cf, g_mountbufs + i);
					CFRelease(uuid_cf);
				} else {
					DLOG("%s, %s, %s: no UUID", g_mountbufs[i].f_mntonname, g_mountbufs[i].f_mntfromname, g_mountbufs[i].f_fstypename);
				}
			} else {
				DLOG("%s, %s, %s: not apfs", g_mountbufs[i].f_mntonname, g_mountbufs[i].f_mntfromname, g_mountbufs[i].f_fstypename);
			}
		}
	}
	
	if (g_apfs_mount_cache) {
		CFRetain(g_apfs_mount_cache);
	}

	CFDictionaryRef ret = g_apfs_mount_cache;

	os_unfair_lock_unlock(&g_apfs_mount_cache_lock);

	return ret;
}

static int apfs_mount_for_uuid(const uuid_t uuid, char** mountname_out, char** bsddisk_out) {
	int err = 0;
	
	if (mountname_out) *mountname_out = NULL;
	if (bsddisk_out)   *bsddisk_out = NULL;

	CFUUIDRef uuid_cf = CFUUIDCreateFromUUIDBytes(kCFAllocatorDefault, *(CFUUIDBytes*)uuid);

	CFDictionaryRef cache = copy_apfs_mount_cache();
	const struct statfs* buf = (const struct statfs*)CFDictionaryGetValue(cache, uuid_cf);
	if (!buf) {
		// Didn't have the mount in our cache, clear the caache and try again
		CFRelease(cache);
		clear_apfs_mount_cache();
		cache = copy_apfs_mount_cache();
		buf = (const struct statfs*)CFDictionaryGetValue(cache, uuid_cf);
	}
	
	if (buf) {
		if (mountname_out) {
			*mountname_out = strdup(buf->f_mntonname);
		}
		if (bsddisk_out) {
			*bsddisk_out = strdup(buf->f_mntfromname);
		}
		err = 0;
	} else {
		err = ENOENT;
	}
	
	CFRelease(cache);
	CFRelease(uuid_cf);

	errno = err;
	return err ? 1 : 0;
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
		error = apfs_flags_for_volume(bsd_disk_path, &pc->p_mounts[0].pm_fs_flags, &pc->p_mounts[0].pm_group_uuid);
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
		uuid_clear(pc->p_mounts[0].pm_group_uuid);
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
			
			//RLOG(DEBUG, "%"PRIdoff":%"PRIdoff" is %"PRIdoff":%"PRIdoff" on disk", ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
			
			if (block_range.l2p_contigbytes <= 0) {
				//RLOG(INFO, "%"PRIdoff":%"PRIdoff" returned %"PRIdoff":%"PRIdoff"", ra_off, remaining, block_range.l2p_devoffset, block_range.l2p_contigbytes);
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
			if (is_apfs && pc->p_mounts[0].pm_fs_flags & BC_FS_APFS_ENCRYPTED) {
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
	
	BC_sort_and_coalesce_playlist_internal(pc);
	
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
		struct BC_playlist_mount* pma = pa->p_mounts + a_mount_idx;

		// Find matching mount in pb
		int b_mount_idx;
		for (b_mount_idx = 0; b_mount_idx < pb->p_nmounts; b_mount_idx++) {
			if (0 == uuid_compare(pma->pm_uuid, pb->p_mounts[b_mount_idx].pm_uuid)) {
				break;
			}
		}
		if (b_mount_idx == pb->p_nmounts) {
			// Didn't find a matching mount, skip to the next a_mount_idx
			continue;
		}
		
		struct BC_playlist_mount* pmb = pb->p_mounts + b_mount_idx;

		ASSERT(0 == uuid_compare(pma->pm_group_uuid, pmb->pm_group_uuid));
		
		// a_mount_idx is the same mount as b_mount_idx
		
		// Find first entry of each mount in the playlists
		int a_idx;
		for (a_idx = 0; a_idx< pa->p_nentries; a_idx++) {
			if (pa->p_entries[a_idx].pe_mount_idx == a_mount_idx) {
				break;
			}
		}
		if (a_idx >= pa->p_nentries) {
			DLOG("No entries in pa for mount %d %s", a_mount_idx, uuid_string(pma->pm_uuid));
			continue;
		}
		
		int b_idx;
		for (b_idx = 0; b_idx< pb->p_nentries; b_idx++) {
			if (pb->p_entries[b_idx].pe_mount_idx == b_mount_idx) {
				break;
			}
		}
		if (b_idx >= pb->p_nentries) {
			DLOG("No entries in pb for mount %d %s", b_mount_idx, uuid_string(pmb->pm_uuid));
			continue;
		}
		
		// Zipper through the list to find matches (we only work on valid playlists that have no length 0 entries)
		while (a_idx < pa->p_nentries && b_idx < pb->p_nentries) {
			const struct BC_playlist_entry* pae = pa->p_entries + a_idx;
			const struct BC_playlist_entry* pbe = pb->p_entries + b_idx;
			
			if (pae->pe_offset == pbe->pe_offset) {
				
				DLOG("Found intersection:");
				DLOG("%s "playlist_entry_format_str, uuid_string(pma->pm_uuid), playlist_entry_format_args(pae));
				DLOG("%s "playlist_entry_format_str, uuid_string(pmb->pm_uuid), playlist_entry_format_args(pbe));
				
				return true;
			} else if (pae->pe_offset < pbe->pe_offset) {
				if (pae->pe_offset + pae->pe_length > pbe->pe_offset) {
					
					DLOG("Found intersection:");
					DLOG("%s "playlist_entry_format_str, uuid_string(pma->pm_uuid), playlist_entry_format_args(pae));
					DLOG("%s "playlist_entry_format_str, uuid_string(pmb->pm_uuid), playlist_entry_format_args(pbe));
					
					return true;
				}
				
				// Find the next entry for this mount
				do {
					a_idx++;
				} while (a_idx < pa->p_nentries && pa->p_entries[a_idx].pe_mount_idx != a_mount_idx);
			} else {
				//pbe->pe_offset < pae->pe_offset
				if (pbe->pe_offset + pbe->pe_length > pae->pe_offset) {
					
					DLOG("Found intersection:");
					DLOG("%s "playlist_entry_format_str, uuid_string(pma->pm_uuid), playlist_entry_format_args(pae));
					DLOG("%s "playlist_entry_format_str, uuid_string(pmb->pm_uuid), playlist_entry_format_args(pbe));
					
					return true;
				}

				// Find the next entry for this mount
				do {
					b_idx++;
				} while (b_idx < pb->p_nentries && pb->p_entries[b_idx].pe_mount_idx == b_mount_idx);
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
	

	// Don't bother recording new mounts' omaps, since they won't be able to play back
	// because they won't be around when BootCacheControl kicks off the BootCache early during boot
	// See <rdar://problem/33557142> Use OID metadata cache for non-root mounts
	return 0;
}

/*
 * Check if the boot cache is supported on this device.
 * Returns 0 if it is supported, non-0 if it isn't
 */
int
BC_test(void)
{
	struct stat statbuf;
	if (0 == stat(BC_BOOT_DISABLEFILE, &statbuf)) {
		LOG("BootCache disabled due to "BC_BOOT_DISABLEFILE);
		return ECANCELED;
	}
	
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
print_fp("%-26s %10llu", name, total_stat(stat))
	
#define stat_print_shared___________________(name, stat) \
print_fp("%-26s %10llu", name, shared_stat(stat))

#define stat_print_nonshared________________(name, stat) \
print_fp("%-26s %10llu", name, nonshared_stat(stat))

#define stat_print_unknown__________________(name, stat) \
print_fp("%-26s %10llu", name, unknown_stat(stat))

#define stat_print_manual___________________(name, stat) \
print_fp("%-26s %10llu", name, stat)
	
#define stat_print_manual_nonshared_________(name, stat) \
print_fp("%-26s                     %10llu", name, stat)

#define stat_print_unknown_msec_____________(name, stat) \
print_fp("%-26s %5llu.%03llus", name, \
unknown_stat(stat) / 1000, \
unknown_stat(stat) % 1000)

#define stat_print_unknown_machabs__________(name, time) do {\
u_int64_t _msec_total     = MachToNano(unknown_stat(time)) / 1000000; \
print_fp("%-26s  %5llu.%03llus", name, \
_msec_total / 1000, \
_msec_total % 1000); \
} while (0)

#define stat_print_machabs_manual___________(name, time) do {\
u_int64_t _msec_total     = MachToNano(time) / 1000000; \
print_fp("%-26s  %5llu.%03llus", name, \
_msec_total / 1000, \
_msec_total % 1000); \
} while (0)

#define stat_print_percent_bothmanual_______(name, num, base) \
print_fp("%-26s %10llu  %3.0f%%", name, num, \
safe_div((float)(num), (base)) * 100)

#define stat_print_percent_bothmanual_nonsh_(name, num, base) \
print_fp("%-26s                     %10llu  %3.0f%%", name, num, \
safe_div((float)(num), (base)) * 100)
	
// Macros that print total, non-shared, and shared in a single line
#define stat_print_split____________________(name, stat) \
print_fp("%-26s %10llu          %10llu          %10llu", name, total_stat(stat), nonshared_stat(stat), shared_stat(stat))

#define stat_print_split_readwrite_tps______(name, reads, writes, time) \
print_fp("%-26s  %4llu/%4llutps        %4llu/%4llutps        %4llu/%4llutps", name, \
safe_div(total_stat(reads)      * 1000, unknown_stat(time)), \
safe_div(total_stat(writes)     * 1000, unknown_stat(time)), \
safe_div(nonshared_stat(reads)  * 1000, unknown_stat(time)), \
safe_div(nonshared_stat(writes) * 1000, unknown_stat(time)), \
safe_div(shared_stat(reads)     * 1000, unknown_stat(time)), \
safe_div(shared_stat(writes)    * 1000, unknown_stat(time)))

#define stat_print_split_manual_____________(name, num) \
print_fp("%-26s %10llu          %10llu          %10llu", name, \
(num##_nonshared + num##_shared), \
num##_nonshared, \
num##_shared)

#define stat_print_split_msec_______________(name, stat) \
print_fp("%-26s %10llums        %10llums        %10llums", name, total_stat(stat), nonshared_stat(stat), shared_stat(stat))

// total time is tracked independently from shared and nonshared batch time
#define stat_print_split_read_rate__________(name, bytes, num, time) do { \
u_int64_t _msec_nonshared = MachToNano(nonshared_stat(time)) / 1000000; \
u_int64_t _msec_shared    = MachToNano(   shared_stat(time)) / 1000000; \
u_int64_t _msec_total     = MachToNano(  unknown_stat(time)) / 1000000; \
print_fp("%-26s    %7lluKB/s %4llut/s %7lluKB/s %4llut/s %7lluKB/s %4llut/s", name, \
safe_div((unsigned long long)(nonshared_stat(bytes) + shared_stat(bytes)) * 1000, (_msec_total     * 1024)),  /* KB/s */ \
safe_div(                    (nonshared_stat(num  ) + shared_stat(num  )) * 1000, (_msec_total           )),  /* t/s  */ \
safe_div((unsigned long long)(nonshared_stat(bytes)                     ) * 1000, (_msec_nonshared * 1024)),  /* KB/s */ \
safe_div(                    (nonshared_stat(num  )                     ) * 1000, (_msec_nonshared       )),  /* t/s  */ \
safe_div((unsigned long long)(   shared_stat(bytes)                     ) * 1000, (_msec_shared    * 1024)),  /* KB/s */ \
safe_div(                    (   shared_stat(num  )                     ) * 1000, (_msec_shared          ))); /* t/s  */ \
} while (0)

#define stat_print_split_read_rate_manual___(name, bytes, num, time) do { \
u_int64_t _msec_nonshared = MachToNano(time##_nonshared) / 1000000; \
u_int64_t _msec_shared    = MachToNano(time##_shared)    / 1000000; \
u_int64_t _msec_total     = MachToNano(time##_total)     / 1000000; \
print_fp("%-26s    %7lluKB/s %4llut/s %7lluKB/s %4llut/s %7lluKB/s %4llut/s", name, \
safe_div((unsigned long long)(bytes##_nonshared + bytes##_shared) * 1000, (_msec_total     * 1024)),  /* KB/s */ \
safe_div(                    (  num##_nonshared +   num##_shared) * 1000, (_msec_total           )),  /* t/s  */ \
safe_div((unsigned long long)(bytes##_nonshared                 ) * 1000, (_msec_nonshared * 1024)),  /* KB/s */ \
safe_div(                    (  num##_nonshared                 ) * 1000, (_msec_nonshared       )),  /* t/s  */ \
safe_div((unsigned long long)(bytes##_shared                    ) * 1000, (_msec_shared    * 1024)),  /* KB/s */ \
safe_div(                    (  num##_shared                    ) * 1000, (_msec_shared          ))); /* t/s  */ \
} while (0)

#define stat_print_split_read_time__________(name, time) do { \
u_int64_t _msec_nonshared = MachToNano(nonshared_stat(time)) / 1000000; \
u_int64_t _msec_shared    = MachToNano(   shared_stat(time)) / 1000000; \
u_int64_t _msec_total     = MachToNano(  unknown_stat(time)) / 1000000; \
print_fp("%-26s  %5llu.%03llus          %5llu.%03llus          %5llu.%03llus", name, \
_msec_total     / 1000, /* whole seconds   */ \
_msec_total     % 1000, /* decimal seconds */ \
_msec_nonshared / 1000, /* whole seconds   */ \
_msec_nonshared % 1000, /* decimal seconds */ \
_msec_shared    / 1000, /* whole seconds   */ \
_msec_shared    % 1000);/* decimal seconds */ \
} while (0)

#define stat_print_split_read_time_manual___(name, time) do { \
u_int64_t _msec_nonshared = MachToNano(time##_nonshared) / 1000000; \
u_int64_t _msec_shared    = MachToNano(time##_shared)    / 1000000; \
u_int64_t _msec_total     = MachToNano(time##_total)     / 1000000; \
print_fp("%-26s  %5llu.%03llus          %5llu.%03llus          %5llu.%03llus", name, \
_msec_total     / 1000, /* whole seconds   */ \
_msec_total     % 1000, /* decimal seconds */ \
_msec_nonshared / 1000, /* whole seconds   */ \
_msec_nonshared % 1000, /* decimal seconds */ \
_msec_shared    / 1000, /* whole seconds   */ \
_msec_shared    % 1000);/* decimal seconds */ \
} while (0)

#define stat_print_split_msec_max___________(name, stat) \
print_fp("%-26s %10llums        %10llums        %10llums", name, MAX(nonshared_stat(stat), shared_stat(stat)), nonshared_stat(stat), shared_stat(stat))

#define stat_print_split_percent____________(name, stat, basestat) \
print_fp("%-26s %10llu  %3.0f%%    %10llu  %3.0f%%    %10llu  %3.0f%%", name, \
    total_stat(stat), safe_div(((float)    total_stat(stat)),     total_stat(basestat)) * 100, /* num, percent */ \
nonshared_stat(stat), safe_div(((float)nonshared_stat(stat)), nonshared_stat(basestat)) * 100, /* num, percent */ \
   shared_stat(stat), safe_div(((float)   shared_stat(stat)),    shared_stat(basestat)) * 100) /* num, percent */

#define stat_print_split_percent_basemanual_(name, stat, base) \
print_fp("%-26s %10llu  %3.0f%%    %10llu  %3.0f%%    %10llu  %3.0f%%", name, \
    total_stat(stat), safe_div(((float)    total_stat(stat)), (base##_nonshared + base##_shared)) * 100, /* num, percent */ \
nonshared_stat(stat), safe_div(((float)nonshared_stat(stat)), (base##_nonshared                )) * 100, /* num, percent */ \
   shared_stat(stat), safe_div(((float)   shared_stat(stat)), (base##_shared                   )) * 100) /* num, percent */

#define stat_print_split_percent_nummanual__(name, num, basestat) \
print_fp("%-26s %10llu  %3.0f%%    %10llu  %3.0f%%    %10llu  %3.0f%%", name, \
(num##_nonshared + num##_shared), safe_div((float)(num##_nonshared + num##_shared),     total_stat(basestat)) * 100, /* num, percent */ \
(num##_nonshared               ), safe_div((float)(num##_nonshared               ), nonshared_stat(basestat)) * 100, /* num, percent */ \
(num##_shared                  ), safe_div((float)(num##_shared                  ),    shared_stat(basestat)) * 100) /* num, percent */
	
#define stat_print_split_percent_bothmanual_(name, stat, num, base) \
print_fp("%-26s %10llu  %3.0f%%    %10llu  %3.0f%%    %10llu  %3.0f%%", name, \
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
		u_int64_t bytes_remaining_nonshared = nonshared_stat(read_bytes) - nonshared_stat(read_errors_bytes) - nonshared_stat(close_discards) - nonshared_stat(hit_bytes) - nonshared_stat(stolen_discards) - nonshared_stat(error_discards) - nonshared_stat(lowpri_discards) - nonshared_stat(bypass_nocache_discards) - nonshared_stat(read_discards) - nonshared_stat(write_discards) - nonshared_stat(hit_bytes_afterhistory) - nonshared_stat(lost_bytes_afterhistory) - nonshared_stat(spurious_discards);
		u_int64_t bytes_remaining_shared = shared_stat(read_bytes) - shared_stat(read_errors_bytes) - shared_stat(close_discards) - shared_stat(hit_bytes) - shared_stat(stolen_discards) - shared_stat(error_discards) - shared_stat(lowpri_discards) - shared_stat(bypass_nocache_discards) - shared_stat(read_discards) - shared_stat(write_discards) - shared_stat(hit_bytes_afterhistory) - shared_stat(lost_bytes_afterhistory) - shared_stat(spurious_discards);
		u_int64_t bytes_remaining_total = bytes_remaining_shared + bytes_remaining_nonshared;
stat_print_split_percent____________(" bytes used", hit_bytes, read_bytes);
stat_print_split_percent_nummanual__(" bytes remaining", bytes_remaining, read_bytes);
		if (total_stat(read_bytes_lowpri) > 0) {
			u_int64_t read_bytes_highpri_nonshared = nonshared_stat(read_bytes) - nonshared_stat(read_bytes_lowpri);
			u_int64_t read_bytes_highpri_shared = shared_stat(read_bytes) - shared_stat(read_bytes_lowpri);
stat_print_split_percent_nummanual__(" high priority bytes", read_bytes_highpri, read_bytes);
stat_print_split_percent____________(" low priority bytes", read_bytes_lowpri, read_bytes);
		}
stat_print_split_percent____________(" bytes failed to read", read_errors_bytes, read_bytes);
		if (total_stat(lowpri_discards) > 0) {
stat_print_split_percent____________(" bytes discarded by lowpri", lowpri_discards, read_bytes);
		}
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
		
		u_int64_t bytes_unaccounted_nonshared = nonshared_stat(cache_bytes) - (nonshared_stat(read_bytes) - nonshared_stat(readerror_unread)) - nonshared_stat(close_unread) - nonshared_stat(stolen_unread) - nonshared_stat(error_unread) - nonshared_stat(lowpri_unread) - nonshared_stat(bypass_nocache_unread) - nonshared_stat(read_unread) - nonshared_stat(write_unread) - nonshared_stat(spurious_unread) - nonshared_stat(badreader_unread) - nonshared_stat(mounterror_unread) - nonshared_stat(nonroot_unread) - nonshared_stat(unsupported_unread) - nonshared_stat(extenterror_unread);
		u_int64_t bytes_unaccounted_shared = shared_stat(cache_bytes) - (shared_stat(read_bytes) - shared_stat(readerror_unread)) - shared_stat(close_unread) - shared_stat(stolen_unread) - shared_stat(error_unread) - shared_stat(lowpri_unread) - shared_stat(bypass_nocache_unread) - shared_stat(read_unread) - shared_stat(write_unread) - shared_stat(spurious_unread) - shared_stat(badreader_unread) - shared_stat(mounterror_unread) - shared_stat(nonroot_unread) - shared_stat(unsupported_unread) - shared_stat(extenterror_unread);
		u_int64_t bytes_unaccounted_total = bytes_unaccounted_nonshared + bytes_unaccounted_shared;

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
		if (total_stat(cache_oversize) > 0) {
stat_print_split_percent____________("cache oversize", cache_oversize, cache_bytes);
		}
	}
	if (ss->userspace_oversize.ssup_highpri_bytes_trimmed > 0) {
print_formatted_____________________("high priority bytes trimme", "%llu", ss->userspace_oversize.ssup_highpri_bytes_trimmed);
	}
	if (ss->userspace_oversize.ssup_lowpri_bytes_trimmed > 0) {
print_formatted_____________________("low priority bytes trimmed", "%llu", ss->userspace_oversize.ssup_lowpri_bytes_trimmed);
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
		
		u_int64_t noncached_bytes_requested_nonshared = nonshared_stat(bypass_nocache_bytes) + nonshared_stat(hit_nocache_bytes);
		u_int64_t noncached_bytes_requested_shared = shared_stat(bypass_nocache_bytes) + shared_stat(hit_nocache_bytes);
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
	
	
	

	/* optimizations */
	if (total_stat(history_entries) > 0) {
print_fp("");
		const char* optimization_state = "unknown";
		switch (ss->userspace_hdd_optimization_state.ssup_optimization_state) {
			case BC_HDD_OPTIMIZATION_STATE_PENDING:
				optimization_state = "pending";
				break;
				
			case BC_HDD_OPTIMIZATION_STATE_IN_PROGRESS:
				optimization_state = "in progress";
				break;
				
			case BC_HDD_OPTIMIZATION_STATE_PAUSED:
				optimization_state = "paused";
				break;
				
			case BC_HDD_OPTIMIZATION_STATE_COMPLETE:
				optimization_state = "completed";
				break;
		}
		
print_formatted_____________________("apfs optimizations", "%d attempt%s (%s)", ss->userspace_hdd_optimization_state.ssup_num_optimizations_attempted, ss->userspace_hdd_optimization_state.ssup_num_optimizations_attempted != 1 ? "s" : "", optimization_state);
		
		
stat_print_split____________________("reads recorded", history_entries);
		
		u_int64_t fusion_history_reads_nonshared = nonshared_stat(fusion_history_already_optimized_reads) + nonshared_stat(fusion_history_not_already_optimized_reads);
		u_int64_t fusion_history_reads_shared = shared_stat(fusion_history_already_optimized_reads) + shared_stat(fusion_history_not_already_optimized_reads);
		u_int64_t fusion_history_reads_total = fusion_history_reads_nonshared + fusion_history_reads_shared;
stat_print_split_percent_nummanual__(" fusion", fusion_history_reads, history_entries);
		if (fusion_history_reads_total > 0) {
stat_print_split_percent_basemanual_("  preoptimized", fusion_history_already_optimized_reads, fusion_history_reads);
stat_print_split_percent_basemanual_("  nonpreoptimized", fusion_history_not_already_optimized_reads, fusion_history_reads);
stat_print_percent_bothmanual_nonsh_("  optimized", ss->userspace_fusion_optimizations.ssup_reads_optimized, fusion_history_reads_nonshared);
stat_print_percent_bothmanual_nonsh_("  nonoptimized", fusion_history_reads_nonshared - ss->userspace_fusion_optimizations.ssup_reads_optimized, fusion_history_reads_nonshared);
		}
		
stat_print_split_percent____________(" hdd", hdd_history_reads, history_entries);
		// Only print stat if weve attempted optimization at least once and either paused or completed. ssup_reads_already_optimized won't be filled in otherwise
		if (nonshared_stat(hdd_history_reads) > 0 && ss->userspace_hdd_optimization_state.ssup_num_optimizations_attempted > 0 && (ss->userspace_hdd_optimization_state.ssup_num_optimizations_attempted > 1 || ss->userspace_hdd_optimization_state.ssup_optimization_state > BC_HDD_OPTIMIZATION_STATE_IN_PROGRESS)) {
stat_print_percent_bothmanual_nonsh_("  preoptimized", ss->userspace_hdd_optimizations.ssup_reads_already_optimized, nonshared_stat(hdd_history_reads));
stat_print_percent_bothmanual_nonsh_("  nonpreoptimized", nonshared_stat(hdd_history_reads) - ss->userspace_hdd_optimizations.ssup_reads_already_optimized, nonshared_stat(hdd_history_reads));
stat_print_percent_bothmanual_nonsh_("  optimized", ss->userspace_hdd_optimizations.ssup_reads_optimized, nonshared_stat(hdd_history_reads));
stat_print_percent_bothmanual_nonsh_("  nonoptimized", nonshared_stat(hdd_history_reads) - ss->userspace_hdd_optimizations.ssup_reads_optimized, nonshared_stat(hdd_history_reads));
		}
		
		
		u_int64_t numInodes = ss->userspace_hdd_optimizations.ssup_inodes_requested + ss->userspace_fusion_optimizations.ssup_inodes_requested;
stat_print_manual_nonshared_________("inodes recorded", numInodes);
		
		if (ss->userspace_fusion_optimizations.ssup_inodes_requested > 0) {
stat_print_percent_bothmanual_nonsh_(" fusion", ss->userspace_fusion_optimizations.ssup_inodes_requested, numInodes);
stat_print_percent_bothmanual_nonsh_("  preoptimized", ss->userspace_fusion_optimizations.ssup_inodes_already_optimized, ss->userspace_fusion_optimizations.ssup_inodes_requested);
stat_print_percent_bothmanual_nonsh_("  nonpreoptimized", ss->userspace_fusion_optimizations.ssup_inodes_requested - ss->userspace_fusion_optimizations.ssup_inodes_already_optimized, ss->userspace_fusion_optimizations.ssup_inodes_requested);
stat_print_percent_bothmanual_nonsh_("  optimized", ss->userspace_fusion_optimizations.ssup_inodes_optimized, ss->userspace_fusion_optimizations.ssup_inodes_requested);
stat_print_percent_bothmanual_nonsh_("  nonoptimized", ss->userspace_fusion_optimizations.ssup_inodes_requested - ss->userspace_fusion_optimizations.ssup_inodes_optimized, ss->userspace_fusion_optimizations.ssup_inodes_requested);
		}

		if (ss->userspace_hdd_optimizations.ssup_inodes_requested > 0) {
stat_print_percent_bothmanual_nonsh_(" hdd", ss->userspace_hdd_optimizations.ssup_inodes_requested, numInodes);
			u_int64_t history_inodes_nonpreoptimized = ss->userspace_hdd_optimizations.ssup_inodes_requested - ss->userspace_hdd_optimizations.ssup_inodes_already_optimized;
stat_print_percent_bothmanual_nonsh_("  preoptimized", ss->userspace_hdd_optimizations.ssup_inodes_already_optimized, ss->userspace_hdd_optimizations.ssup_inodes_requested);
stat_print_percent_bothmanual_nonsh_("  nonpreoptimized", history_inodes_nonpreoptimized, ss->userspace_hdd_optimizations.ssup_inodes_requested);
stat_print_percent_bothmanual_nonsh_("  optimized", ss->userspace_hdd_optimizations.ssup_inodes_optimized, ss->userspace_hdd_optimizations.ssup_inodes_requested);
stat_print_percent_bothmanual_nonsh_("  nonoptimized", ss->userspace_hdd_optimizations.ssup_inodes_requested - ss->userspace_hdd_optimizations.ssup_inodes_optimized, ss->userspace_hdd_optimizations.ssup_inodes_requested);
		}
		

stat_print_split____________________("bytes recorded", history_entries_bytes);
		
		u_int64_t fusion_history_bytes_nonshared = nonshared_stat(fusion_history_already_optimized_bytes) + nonshared_stat(fusion_history_not_already_optimized_bytes);
		u_int64_t fusion_history_bytes_shared = shared_stat(fusion_history_already_optimized_bytes) + shared_stat(fusion_history_not_already_optimized_bytes);
stat_print_split_percent_nummanual__(" fusion", fusion_history_bytes, history_entries_bytes);
		if (total_stat(fusion_history_already_optimized_reads) + total_stat(fusion_history_not_already_optimized_reads) > 0) {
stat_print_split_percent_basemanual_("  preoptimized", fusion_history_already_optimized_bytes, fusion_history_bytes);
stat_print_split_percent_basemanual_("  nonpreoptimized", fusion_history_not_already_optimized_bytes, fusion_history_bytes);
			if (fusion_history_bytes_nonshared != ss->userspace_fusion_optimizations.ssup_bytes_requested) {
stat_print_percent_bothmanual_nonsh_("  optimize attempted", ss->userspace_fusion_optimizations.ssup_bytes_requested, fusion_history_bytes_nonshared);
			}
stat_print_percent_bothmanual_nonsh_("  optimized", ss->userspace_fusion_optimizations.ssup_bytes_optimized, fusion_history_bytes_nonshared)
stat_print_percent_bothmanual_nonsh_("  nonoptimized", ss->userspace_fusion_optimizations.ssup_bytes_requested - ss->userspace_fusion_optimizations.ssup_bytes_optimized, fusion_history_bytes_nonshared);
		}
		
stat_print_split_percent____________(" hdd", hdd_history_bytes, history_entries_bytes);
		// Only print stat if weve attempted optimization at least once and either paused or completed. ssup_bytes_already_optimized won't be filled in otherwise
		if (nonshared_stat(hdd_history_bytes) > 0 && ss->userspace_hdd_optimization_state.ssup_num_optimizations_attempted > 0 && (ss->userspace_hdd_optimization_state.ssup_num_optimizations_attempted > 1 || ss->userspace_hdd_optimization_state.ssup_optimization_state > BC_HDD_OPTIMIZATION_STATE_IN_PROGRESS)) {
			u_int64_t history_bytes_nonpreoptimized = nonshared_stat(hdd_history_bytes) - ss->userspace_hdd_optimizations.ssup_bytes_already_optimized;
stat_print_percent_bothmanual_nonsh_("  preoptimized", ss->userspace_hdd_optimizations.ssup_bytes_already_optimized, nonshared_stat(hdd_history_bytes));
stat_print_percent_bothmanual_nonsh_("  nonpreoptimized", history_bytes_nonpreoptimized, nonshared_stat(hdd_history_bytes));
			if (nonshared_stat(hdd_history_bytes) != ss->userspace_hdd_optimizations.ssup_bytes_requested) {
stat_print_percent_bothmanual_nonsh_("  optimize attempted", ss->userspace_hdd_optimizations.ssup_bytes_requested, nonshared_stat(hdd_history_bytes));
stat_print_percent_bothmanual_nonsh_("   optimized", ss->userspace_hdd_optimizations.ssup_bytes_optimized, ss->userspace_hdd_optimizations.ssup_bytes_requested);
stat_print_percent_bothmanual_nonsh_("    range size", ss->userspace_hdd_optimizations.ssup_optimization_range_length, ss->userspace_hdd_optimizations.ssup_bytes_optimized);
stat_print_percent_bothmanual_nonsh_("   surplus", ss->userspace_hdd_optimizations.ssup_bytes_surplus, ss->userspace_hdd_optimizations.ssup_bytes_requested);
stat_print_percent_bothmanual_nonsh_("   nonoptimized", ss->userspace_hdd_optimizations.ssup_bytes_nonoptimized, ss->userspace_hdd_optimizations.ssup_bytes_requested);
			} else {
stat_print_percent_bothmanual_nonsh_("  optimized", ss->userspace_hdd_optimizations.ssup_bytes_optimized, nonshared_stat(hdd_history_bytes));
stat_print_percent_bothmanual_nonsh_("   range size", ss->userspace_hdd_optimizations.ssup_optimization_range_length, ss->userspace_hdd_optimizations.ssup_bytes_optimized);
stat_print_percent_bothmanual_nonsh_("  surplus", ss->userspace_hdd_optimizations.ssup_bytes_surplus, nonshared_stat(hdd_history_bytes));
stat_print_percent_bothmanual_nonsh_("  nonoptimized", ss->userspace_hdd_optimizations.ssup_bytes_nonoptimized, nonshared_stat(hdd_history_bytes));
			}
		}
		
	}

	
	
	/* inbound strategy */
print_fp("");
stat_print_split____________________("total strategy calls", strategy_calls);
	if (total_stat(strategy_calls) > 0) {
		u_int64_t bypassed_nonshared = nonshared_stat(strategy_calls) - nonshared_stat(strategy_bypassed);
		u_int64_t bypassed_shared = shared_stat(strategy_calls) - shared_stat(strategy_bypassed);
stat_print_split_percent_nummanual__(" filled calls", bypassed, strategy_calls);
stat_print_split_percent____________(" bypassed calls", strategy_bypassed, strategy_calls);
		if (total_stat(strategy_bypassed) > 0) {
			u_int64_t other_nonshared = nonshared_stat(strategy_bypassed) - nonshared_stat(strategy_nonread) - nonshared_stat(strategy_noncached_mount) - nonshared_stat(strategy_unready_mount) - nonshared_stat(strategy_nonblocksize) - (nonshared_stat(extent_lookups) - nonshared_stat(extent_hits)) - nonshared_stat(strategy_bypass_duringio_unfilled) - nonshared_stat(strategy_unfilled_lowpri);
			u_int64_t other_shared = shared_stat(strategy_bypassed) - shared_stat(strategy_nonread) - shared_stat(strategy_noncached_mount) - shared_stat(strategy_unready_mount) - shared_stat(strategy_nonblocksize) - (shared_stat(extent_lookups) - shared_stat(extent_hits)) - shared_stat(strategy_bypass_duringio_unfilled) - shared_stat(strategy_unfilled_lowpri);
stat_print_split_percent____________("  nonread calls", strategy_nonread, strategy_bypassed);
stat_print_split_percent____________("  noncached mount calls", strategy_noncached_mount, strategy_bypassed);
stat_print_split_percent____________("  unready mount calls", strategy_unready_mount, strategy_bypassed);
			if (total_stat(strategy_nonblocksize) > 0) {
stat_print_split_percent____________("  nonblocksize calls", strategy_nonblocksize, strategy_bypassed);
			}
			u_int64_t cache_miss_nonshared = nonshared_stat(extent_lookups) - nonshared_stat(extent_hits);
			u_int64_t cache_miss_shared = shared_stat(extent_lookups) - shared_stat(extent_hits);
			u_int64_t cache_miss_total = cache_miss_nonshared + cache_miss_shared;
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
		u_int64_t noncached_nonshared = nonshared_stat(strategy_bypass_nocache) + nonshared_stat(strategy_hit_nocache);
		u_int64_t noncached_shared = shared_stat(strategy_bypass_nocache) + shared_stat(strategy_hit_nocache);
		u_int64_t noncached_total = noncached_nonshared + noncached_shared;
stat_print_split_percent_nummanual__(" noncached calls", noncached, strategy_calls);
		if (noncached_total > 0) {
stat_print_split_percent_basemanual_("  noncached calls filled", strategy_hit_nocache, noncached);
stat_print_split_percent_basemanual_("  noncached calls bypassed", strategy_bypass_nocache, noncached);
		}
stat_print_split_percent____________(" throttled calls", strategy_throttled, strategy_calls);
stat_print_split_percent____________(" calls during readahead", strategy_duringio, strategy_calls);
		if (total_stat(strategy_duringio) > 0) {
			u_int64_t filled_during_readahead_nonshared = nonshared_stat(strategy_duringio) - nonshared_stat(strategy_bypass_duringio);
			u_int64_t filled_during_readahead_shared = shared_stat(strategy_duringio) - shared_stat(strategy_bypass_duringio);
stat_print_split_percent_nummanual__("  filled during readahead", filled_during_readahead, strategy_duringio);
stat_print_split_percent____________("  bypassed during readahea", strategy_bypass_duringio, strategy_duringio);
			if (total_stat(strategy_bypass_duringio) > 0) {
				u_int64_t other_nonshared = nonshared_stat(strategy_bypass_duringio) - nonshared_stat(strategy_bypass_duringio_rootdisk_nonread) - nonshared_stat(strategy_bypass_duringio_rootdisk_read) - nonshared_stat(strategy_bypass_duringio_rootdisk_failure) - nonshared_stat(strategy_bypass_duringio_unfilled) - nonshared_stat(strategy_bypass_duringio_nocache);
				u_int64_t other_shared = shared_stat(strategy_bypass_duringio) - shared_stat(strategy_bypass_duringio_rootdisk_nonread) - shared_stat(strategy_bypass_duringio_rootdisk_read) - shared_stat(strategy_bypass_duringio_rootdisk_failure) - shared_stat(strategy_bypass_duringio_unfilled) - shared_stat(strategy_bypass_duringio_nocache);
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
			u_int64_t disk_bytes_nonshared = 0, disk_bytes_shared = 0;
			u_int64_t disk_machabstime_total = 0, disk_machabstime_nonshared = 0, disk_machabstime_shared = 0;
			u_int64_t disk_num_reads_nonshared = 0, disk_num_reads_shared = 0;
			
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
			
			
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d time", d);
stat_print_split_read_time_manual___(diskbuf, disk_machabstime);
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d bytes read", d);
stat_print_split_manual_____________(diskbuf, disk_bytes);
			if (disk_machabstime_total > 0) {
				snprintf(diskbuf, sizeof(diskbuf), "Disk %d reader rate", d);
stat_print_split_read_rate_manual___(diskbuf, disk_bytes, disk_num_reads, disk_machabstime);
			}
			

			for(int b = 0; b < STAT_BATCHMAX; b++) {
				if (total_stat(batch_bytes[d][b]) > 0) {
					snprintf(diskbuf, sizeof(diskbuf), " batch %d time", b);
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
			
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d lowpri time", d);
stat_print_split_read_time__________(diskbuf, batch_time_lowpri[d]);
			snprintf(diskbuf, sizeof(diskbuf), "Disk %d lowpri bytes read", d);
stat_print_split____________________(diskbuf, batch_bytes_lowpri[d]);
			if (unknown_stat(batch_time_lowpri[d]) > 0) {
				snprintf(diskbuf, sizeof(diskbuf), "Disk %d lowpri reader rate", d);
stat_print_split_read_rate__________(diskbuf, batch_bytes_lowpri[d], batch_initiated_reads_lowpri[d], batch_time_lowpri[d]);
			}
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
	if (ss->userspace_timestamps.ssup_launch_timestamp > 0) {
stat_print_machabs_manual___________("BCC launch timestamp", ss->userspace_timestamps.ssup_launch_timestamp);
	}
stat_print_unknown_machabs__________("kext start timestamp", start_timestamp);
	if (ss->userspace_timestamps.ssup_launch_timestamp > 0) {
stat_print_machabs_manual___________("total setup time", unknown_stat(start_timestamp) - ss->userspace_timestamps.ssup_launch_timestamp);
		if (unknown_stat(start_timestamp) > ss->userspace_timestamps.ssup_launch_timestamp) {
stat_print_machabs_manual___________(" BCC launch time", ss->userspace_timestamps.ssup_oid_timestamp - ss->userspace_timestamps.ssup_launch_timestamp);
stat_print_machabs_manual___________(" oid lookup time", unknown_stat(bc_start_timestamp) - ss->userspace_timestamps.ssup_oid_timestamp);
stat_print_machabs_manual___________(" kext init time", unknown_stat(start_timestamp) - unknown_stat(bc_start_timestamp));
		}
	}
	
	if (unknown_stat(history_time) > 0 || total_stat(history_entries) == 0) {
stat_print_unknown_msec_____________("recording active time", history_time);
		if (unknown_stat(history_time) > 0) {
stat_print_split_readwrite_tps______("read/write IO rate", history_reads, history_writes, history_time);
		}
	} else {
print_string________________________("recording active time", "(still active)");
	}
	
	
stat_print_split____________________("num writes during boot", history_writes);
stat_print_split____________________("num reads during boot", history_reads);
stat_print_split_percent____________(" bootcache entries", history_entries, history_reads);
	if (total_stat(history_reads_truncated) > 0) {
stat_print_split_percent____________(" truncated", history_reads_truncated, history_reads);
	}
	if (total_stat(history_reads_nomount) > 0) {
stat_print_split_percent____________(" no mount", history_reads_nomount, history_reads);
	}
	if (total_stat(history_reads_unknown) > 0) {
stat_print_split_percent____________(" unknown mount", history_reads_unknown, history_reads);
	}
	if (total_stat(history_reads_no_blocksize) > 0) {
stat_print_split_percent____________(" no blocksize", history_reads_no_blocksize, history_reads);
	}
	if (total_stat(history_reads_nonroot) > 0) {
stat_print_split_percent____________(" nonroot disk", history_reads_nonroot, history_reads);
	}
	if (total_stat(history_reads_ssd) > 0) {
stat_print_split_percent____________(" ssd (noncached)", history_reads_ssd, history_reads);
	}

stat_print_split____________________("bytes written during boot", history_writes_bytes);
stat_print_split____________________("bytes read during boot", history_reads_bytes);
stat_print_split_percent____________(" bootcache bytes", history_entries_bytes, history_reads_bytes);
	if (total_stat(history_reads_truncated) > 0) {
stat_print_split_percent____________(" truncated bytes", history_reads_truncated_bytes, history_reads_bytes);
	}
	if (total_stat(history_reads_nomount) > 0) {
stat_print_split_percent____________(" no mount bytes", history_reads_nomount_bytes, history_reads_bytes);
	}
	if (total_stat(history_reads_unknown) > 0) {
stat_print_split_percent____________(" unknown mount bytes", history_reads_unknown_bytes, history_reads_bytes);
	}
	if (total_stat(history_reads_no_blocksize) > 0) {
stat_print_split_percent____________(" no blocksize bytes", history_reads_no_blocksize_bytes, history_reads_bytes);
	}
	if (total_stat(history_reads_nonroot) > 0) {
stat_print_split_percent____________(" nonroot bytes", history_reads_nonroot_bytes, history_reads_bytes);
	}
	if (total_stat(history_reads_ssd) > 0) {
stat_print_split_percent____________(" ssd (noncached) bytes", history_reads_ssd_bytes, history_reads_bytes);
	}

stat_print_unknown__________________("recording num mounts", history_mounts);
stat_print_unknown__________________("unidentifiable mounts", history_mount_no_uuid);
stat_print_unknown__________________("mounts without blocksize", history_mount_no_blocksize);
stat_print_unknown__________________("num recordings", history_num_recordings);

	/* flags */
fprintf(fp, "\n");
fprintf(fp, "current flags              0x%llx", ss->ss_cache_flags);
	if (ss->ss_cache_flags & BC_FLAG_SETUP) {
		fprintf(fp, ", setup");
	}
	if (ss->ss_cache_flags & BC_FLAG_CACHEACTIVE) {
		fprintf(fp, ", cache active");
	}
	if (ss->ss_cache_flags & BC_FLAG_HISTORYACTIVE) {
		fprintf(fp, ", history active");
	}
	if (ss->ss_cache_flags & BC_FLAG_HTRUNCATED) {
		fprintf(fp, ", truncated");
	}
	if (ss->ss_cache_flags & BC_FLAG_SHUTDOWN) {
		fprintf(fp, ", shut down");
	}
	fprintf(fp, "\n");
	
	if (ss->ss_playback_end_reason[0] != '\0') {
print_string________________________("Playback end reason", ss->ss_playback_end_reason);
	}
	if (ss->ss_history_end_reason[0] != '\0') {
print_string________________________("History end reason", ss->ss_history_end_reason);
	}
	if (ss->ss_cache_end_reason[0] != '\0') {
print_string________________________("Cache end reason", ss->ss_cache_end_reason);
	}

	if (fp == stdout) {
		fflush(fp);
	} else {
		fclose(fp);
	}
	return(0);
}

int
BC_print_playlist(const struct BC_playlist *pc, bool verbose)
{
	struct BC_playlist_mount *pm;
	struct BC_playlist_entry *pe;
	int i;
	int numentries = 0, numentries_lowpri = 0, numentries_batch[BC_MAXBATCHES] = {0};
	int numentries_shared = 0, numentries_shared_lowpri = 0, numentries_shared_batch[BC_MAXBATCHES] = {0};
	u_int64_t size = 0, size_lowpri = 0, size_batch[BC_MAXBATCHES] = {0};
	u_int64_t size_shared = 0, size_shared_lowpri = 0, size_shared_batch[BC_MAXBATCHES] = {0};

	if (!pc) return EINVAL;
	
	LOG("Number of mounts: %d", pc->p_nmounts);
	LOG("Number of extents: %d", pc->p_nentries);
	LOG("Number of omaps: %d", pc->p_nomaps);
	
	for (i = 0; i < pc->p_nmounts; i++) {
		pm = pc->p_mounts + i;
		LOG("Mount %s grouping %s flags 0x%x, %5d entries %5d omaps",
			uuid_string(pm->pm_uuid), uuid_is_null(pm->pm_group_uuid) ? "none" : uuid_string(pm->pm_group_uuid), pm->pm_fs_flags, pm->pm_nentries, pm->pm_nomaps);
	}
	
	/*
	 * Print entries in source or "human-readable" format.
	 */
	if (verbose) {
		LOG("Extents:");
		LOG("Mount                                Offset       Length Batch Flags Crypto offset");
	}
	for (i = 0; i < pc->p_nentries; i++) {
		pe = pc->p_entries + i;
		if (verbose) {
			LOG("%s "playlist_entry_format_str,
				uuid_string(pc->p_mounts[pe->pe_mount_idx].pm_uuid), playlist_entry_format_args(pe));
		}
		if (pe->pe_flags & BC_PE_LOWPRIORITY) {
			if (pe->pe_flags & BC_PE_SHARED) {
				size_shared_lowpri += pe->pe_length;
				numentries_shared_lowpri++;
			} else {
				size_lowpri += pe->pe_length;
				numentries_lowpri++;
			}
		} else {
			if (pe->pe_flags & BC_PE_SHARED) {
				size_shared += pe->pe_length;
				numentries_shared++;
				size_shared_batch[pe->pe_batch] += pe->pe_length;
				numentries_shared_batch[pe->pe_batch]++;
			} else {
				size += pe->pe_length;
				numentries ++;
				size_batch[pe->pe_batch] += pe->pe_length;
				numentries_batch[pe->pe_batch] ++;
			}
		}
	}
	
	if (verbose) {
		LOG("omaps:");
		LOG("Mount                                oid          oxid     Batch");
		for (i = 0; i < pc->p_nomaps; i++) {
			struct BC_playlist_omap *po = pc->p_omaps + i;
			LOG("%s %#-12llx %#-8llx %d",
				uuid_string(pc->p_mounts[po->po_mount_idx].pm_uuid), po->po_omap.otr_oid, po->po_omap.otr_oxid, po->po_omap.otr_batch);
		}
	}
	
	LOG("%12llu bytes, %d entries", size, numentries);
	LOG("%12llu low-priority bytes, %d entries", size_lowpri, numentries_lowpri);
	LOG("%12llu sharedcache bytes, %d entries", size_shared, numentries_shared);
	LOG("%12llu sharedcache low-priority bytes, %d entries", size_shared_lowpri, numentries_shared_lowpri);
	for (i = 0; i < BC_MAXBATCHES; i++) {
		if (size_batch[i] != 0) {
			LOG("%12llu bytes, %d entries batch %d", size_batch[i], numentries_batch[i], i);
			LOG("%12llu sharedcache bytes, %d entries batch %d", size_shared_batch[i], numentries_shared_batch[i], i);
		}
	}
	
	return 0;
}


int
BC_print_history(char *fname, const struct BC_history *hc)
{
	if (!hc) {
		return EINVAL;
	}
	
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

	fprintf(fp, "%36s %36s %-12s %-12s %-8s %-12s %5s type shared?\n",
			"uuid",
			"group uuid",
			"inode",
			"offset",
			"length",
			"crypto off",
			"pid");
	for (i = 0; i < hc->h_nentries; i++) {
		fprintf(fp, "%s %s %-12llu %#-12llx %#-8llx %#-12llx %5u%s%s\n",
				uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_uuid),
				uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_group_uuid),
				hc->h_entries[i].he_inode,
				hc->h_entries[i].he_offset, hc->h_entries[i].he_length,
				hc->h_entries[i].he_crypto_offset,
				hc->h_entries[i].he_pid,
				hc->h_entries[i].he_flags & BC_HE_OPTIMIZED ? " optimized" :
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
BC_print_omap_history(char *fname, const struct BC_omap_history *ho)
{
	if (!ho) {
		return EINVAL;
	}
	
	FILE *fp;
	int i, j;
	
	errno = 0;
	if (fname != NULL) {
		fp = fopen(fname, "w");
	} else {
		fp = stdout;
	}
	if (fp == NULL)
		return(errno);
	
	fprintf(fp, "Mount                                oid          oxid     Batch\n");
	
	for (i = 0; i < ho->oh_nmounts; i++) {
		struct BC_omap_history_mount* mount = ho->oh_mounts + i;
		for (j = 0; j < mount->ohm_nomaps; j++) {
			fprintf(fp, "%s %#-12llx %#-8llx %d\n", uuid_string(mount->ohm_uuid), mount->ohm_omaps[j].otr_oid, mount->ohm_omaps[j].otr_oxid, mount->ohm_omaps[j].otr_batch);
		}
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

	int batch_num = -1;
	
	bc.bc_magic = BC_MAGIC;
	bc.bc_opcode = BC_OP_TAG;
	bc.bc_data1 = (uintptr_t) &batch_num;
	bc.bc_data1_size = (unsigned int) sizeof(batch_num);
	error = sysctlbyname(BC_SYSCTL, NULL, NULL, &bc, sizeof(bc));
	if (error != 0) {
		LOG_ERRNO("could not insert prefetch tag");
		return(ENOENT);
	}
	
	uint8_t omapBatch = MAX(0, MIN(batch_num, UINT8_MAX));
	
	return BC_tag_omap_recording_for_all_mounts(omapBatch);
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
