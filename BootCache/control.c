/*
 * Block device "smart readahead" cache control utility.
 *
 * Provides the user-space interface to the BootCache cache;
 * loading the playlist, reading and presorting the history list
 * into a new playlist, gathering performance statistics.
 *
 */

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/attr.h>
#include <sys/syscall.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOKitLib.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <libkern/OSAtomic.h>
#include <os/assumes.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mach-o/dyld_priv.h>
#include <mach-o/dyld_cache_format.h>
#include <mach/mach_time.h>
#include <sys/kdebug_private.h>

#include "BootCache_private.h"

// --- For fusion disk checking
#include <IOKit/storage/CoreStorage/CoreStorageUserLib.h>
#include <IOKit/storage/IOMedia.h>
#include <APFS/APFSConstants.h>

static int	usage(const char *reason);
static int	do_boot_cache(void);
static int	start_cache(const char *pfname);
static int	stop_cache(const char *pfname, int debugging);
static int	jettison_cache(void);
static int	merge_playlists(const char *pfname, int argc, char *argv[], int* pbatch);
static int	print_statistics(struct BC_statistics *ss);
static int	print_debug_buffer(void);
static void	print_history(struct BC_history *he);
static int	print_playlist_on_disk(const char *pfname);
static int	unprint_playlist(const char *pfname);
static int	generalize_playlist(const char *pfname);
static int	truncate_playlist(const char *pfname, char *larg);
static int  trim_playlist_to_max_size(struct BC_playlist* pc);
static void max_playlist_size(uint64_t* max_high_priority_size_out, uint64_t* max_total_size_out);
static int	add_file(struct BC_playlist *pc, const char *fname, const int batch, const bool low_priority);
static int	add_directory(struct BC_playlist *pc, const char *fname, const int batch);
static int	add_fseventsd_files(struct BC_playlist *pc, const int batch);
static int	add_playlist_for_preheated_user(struct BC_playlist *pc, bool isFusion);
static int	add_logical_playlist(const char *playlist, struct BC_playlist *pc, int batch);
static int	add_native_shared_cache(struct BC_playlist *pc, int batch, bool low_priority);

static CFStringRef _bootcachectl_copy_root_volume_uuid(bool *is_apfs_out, bool* is_fusion_out);
static CFStringRef _bootcachectl_copy_uuid_for_bsd_device(const char* bsd_name, bool *is_apfs_out, bool *is_fusion_out);

static int verbose;
static char *myname;

static FILE *outstream;

#define LOG(fmt, args...) fprintf(outstream, fmt"\n", ##args)
#define LOG_ERRNO(fmt, args...) LOG(fmt": %d %s", ## args, errno, strerror(errno))

#ifdef DEBUG
#define DLOG(fmt, args...) LOG(fmt, ##args)
#else
#define DLOG(fmt, args...)
#endif

int
main(int argc, char *argv[])
{
	int ch, batch;
	int* pbatch;
	char *pfname;

	outstream = stdout;
	bc_log_stream = stdout;
	
	myname = argv[0];
	pfname = NULL;
	pbatch = NULL;
	while ((ch = getopt(argc, argv, "b:cvf:t:")) != -1) {
		switch(ch) {
			case 'b':
				usage("blocksize is no longer required, ignoring");
			case 'c':
				// no longer used
				break;
			case 'v':
				verbose++;
				break;
			case 'f':
				pfname = optarg;
				break;
			case 't':
				if ((sscanf(optarg, "%d", &batch)) != 1)
					usage("bad batch number");
				else
					pbatch = &batch;
				break;
			case '?':
			default:
				return(usage(NULL));
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		return(usage("missing command"));
	/* documented interface */
	if (!strcmp(argv[0], "start")) {
		if (pfname)
			return(start_cache(pfname));
		else
			return(do_boot_cache());
	}
	if (!strcmp(argv[0], "stop"))
		return(stop_cache(pfname, 0));
	if (!strcmp(argv[0], "mount"))
		return(BC_notify_mount());
	if (!strcmp(argv[0], "jettison"))
		return(jettison_cache());
	if (!strcmp(argv[0], "tag"))
		return(BC_tag_history());
	if (!strcmp(argv[0], "statistics"))
		return(print_statistics(NULL));
	if (!strcmp(argv[0], "debugbuffer"))
		return(print_debug_buffer());
	if (!strcmp(argv[0], "merge"))
		return(merge_playlists(pfname, argc - 1, argv + 1, pbatch));
	if (!strcmp(argv[0], "print"))
		return(print_playlist_on_disk(pfname));
	if (!strcmp(argv[0], "unprint"))
		return(unprint_playlist(pfname));
	if (!strcmp(argv[0], "generalize"))
		return(generalize_playlist(pfname));
	if (!strcmp(argv[0], "generate")) {
		LOG("generate is no longer supported");
		return(1);
	}
	if (!strcmp(argv[0], "truncate")) {
		if (argc < 2)
			return(usage("missing truncate length"));
		return(truncate_playlist(pfname, argv[1]));
	}

	if (!strcmp(argv[0], "unload")) {
		if (BC_unload()) {
			LOG("could not unload cache");
			return(1);
		} else {
			return(0);
		}
	}
	return(usage("invalid command"));
}

/*
 * Suggest better usage habits.
 */
static int
usage(const char *reason)
{
	if (reason != NULL)
		LOG("%s", reason);
	LOG("Usage: %s [-vvv] [-f <playlistfile>] start", myname);
	LOG("           Start recording history and play back <playlistfile>.");
	LOG("       %s [-vvv] [-f <playlistfile>] stop", myname);
	LOG("           Stop recording history and write the playlist to <playlistfile>.");
	LOG("       %s mount", myname);
	LOG("           Notify the boot cache of a new mount.");
	LOG("       %s [-vvv] jettison", myname);
	LOG("           Jettison the cache.");
	LOG("       %s statistics", myname);
	LOG("           Print statistics for the currently-active cache.");
	LOG("       %s debugbuffer", myname);
	LOG("           Print the debug buffer, if available.");
	LOG("       %s tag", myname);
	LOG("           Insert the end-prefetch tag.");
	LOG("       %s [-vvv] [-t batchnum] -f <playlistfile> merge <playlistfile1> [<playlistfile2>...]", myname);
	LOG("           Merge <playlistfile1>... into <playlistfile>.");
	LOG("           Playlist files after the first will be offset <batchnum> batches, if provided");
	LOG("       %s -f <playlistfile> print", myname);
	LOG("           Print the contents of <playlistfile>.");
	LOG("       %s -f <playlistfile> unprint", myname);
	LOG("           Read a playlist from standard input and write to <playlistfile>.");
	LOG("       %s -f <playlistfile> truncate <count>", myname);
	LOG("           Truncate <playlistfile> to <count> entries.");
	LOG("       %s -f <playlistfile> generalize", myname);
	LOG("           Modify <playlistfile> to apply to any root volume.");
	return(1);
}

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

static int add_logical_playlist(const char *playlist, struct BC_playlist *pc, int batch)
{
	char filename[MAXPATHLEN];

	FILE *lpl = fopen(playlist, "r");
	if (lpl == NULL){
		if (errno != ENOENT && errno != EINVAL) {
			LOG_ERRNO("Could not read playlist %s", playlist);
		}
		return -1;
	}

	while (fgets(filename, sizeof filename, lpl)){
		size_t len = strlen(filename);
		if (len < 3)
			continue;

		if (filename[len - 1] == '\n')
			filename[len - 1] = '\0';

		if (filename[len - 2] == '/'){
			filename[len - 2] = '\0';
			int error;
			if ((error = add_directory(pc, filename, batch))
				&& error != ENOENT && error != EINVAL)
				LOG_ERRNO("Error adding files in directory %s", filename);
		} else {
			add_file(pc, filename, batch, false);
		}
	}
	fclose(lpl);

	return 0;
}

/*
 * Kick off the boot cache.
 */
static int
do_boot_cache()
{
	struct BC_userspace_timestamps userspace_timestamps = {0};
	userspace_timestamps.ssup_launch_timestamp = mach_absolute_time();
#ifdef DEBUG
	outstream = fopen("/var/log/BootCacheControl.log", "a");
	err_set_file(outstream);
	bc_log_stream = outstream;
	
	struct tm tm;
	time_t seconds = time(NULL);
	localtime_r(&seconds, &tm);
	char date[32];
	size_t ret = strftime(date, sizeof(date), "%Y-%m-%d %T", &tm);
	if (ret != 0) {
		LOG("Launched at %s", date);
	} else {
		LOG("Launched");
	}
#endif

	struct BC_playlist *pc;
	int error;
	const char* pfname = BC_ROOT_PLAYLIST;

	if ((error = BC_test()) != 0) {
		return error;
	}

	bool isFusion = false;
	bool isAPFS = false;
	CFStringRef uuid = _bootcachectl_copy_root_volume_uuid(&isAPFS, &isFusion);
	if (!uuid) {
		isFusion = false;
		isAPFS = false;
	} else {
		CFRelease(uuid);
	}

	/* set up to start recording with no playback */
	pc = NULL;

	/*
	 * If we have a playlist, open it and prepare to load it.
	 */
	error = BC_read_playlist(pfname, &pc);

	/*
	 * If the playlist is missing or invalid, ignore it. We'll
	 * overwrite it later.
	 */
	if ((error != 0) && (error != EINVAL) && (error != ENOENT)) {
		errno = error;
		LOG_ERRNO("could not read playlist %s", pfname);
	}
	
	/*
	 * Remove the consumed playlist: it'll be replaced once BootCache
	 * completes successfully.
	 */
	
#ifdef DEBUG
	char prev_path[MAXPATHLEN];
	snprintf(prev_path, sizeof(prev_path), "%s.previous", pfname);
	error = rename(pfname, prev_path); // for debugging
	if (error != 0 && errno != ENOENT) {
		LOG_ERRNO("could not rename bootcache playlist %s", pfname);
	}
#else
	error = unlink(pfname);
	if (error != 0 && errno != ENOENT) {
		LOG_ERRNO("could not unlink bootcache playlist %s", pfname);
	}
#endif
	


	int last_shutdown_was_clean = 0;
	size_t wasCleanSz = sizeof(wasCleanSz);
	error = sysctlbyname("vfs.generic.root_unmounted_cleanly", &last_shutdown_was_clean, &wasCleanSz, NULL, 0);
	if (error != 0) {
		LOG_ERRNO("Unable to check for hard shutdown");
	}

	if (pc) {

		if (! last_shutdown_was_clean) {
			struct timeval start_time = {0,0}, end_time = {0,0};
			(void)gettimeofday(&start_time, NULL); // can't fail

			// Make sure the unlink(pfname) above hits disk in case we panic rdar://8947415
			int fd = open("/var/db/", O_RDONLY);
			if (fd != -1) {
				if (-1 != fcntl(fd, F_FULLFSYNC)) {
					LOG("Synced /var/db/");
				} else {
					LOG_ERRNO("Unable to sync /var/db/");
				}
				close(fd);
			} else {
				LOG_ERRNO("Unable to open /var/db/");
			}

			(void)gettimeofday(&end_time, NULL); // can't fail
			timersub(&end_time, &start_time, &end_time);
			// LOG("Took %dus to sync /var/db/", end_time.tv_usec);
		}

		if (!isFusion) {
			// rdar://9424845 Add any files we know are read in every boot, but change every boot
			add_fseventsd_files(pc, 0);
			add_logical_playlist(BC_ROOT_EXTRA_LOGICAL_PLAYLIST, pc, 0);
			add_logical_playlist(BC_LOGIN_EXTRA_LOGICAL_PLAYLIST, pc, 2);
		}
		
	} else {
		if (last_shutdown_was_clean) {
			pc = BC_allocate_playlist(0, 0, 0);

			// No Root playlist: we're during an install so use our premade logical playlists

			if (!isFusion) {
				add_logical_playlist(BC_ROOT_LOGICAL_PLAYLIST, pc, 0);
				add_logical_playlist(BC_LOGIN_LOGICAL_PLAYLIST, pc, 2);
			}
		}
	}
	
	// If we don't have a playlist here, we don't want to play back anything
	if (pc) {
		if (!isAPFS || !isFusion) {
			// Add the login playlist of user we expect to log in to the boot playlist
			add_playlist_for_preheated_user(pc, isFusion);
			
			// rdar://9021675 Always warm the shared cache
			DLOG("Adding shared cache");
			add_native_shared_cache(pc, -1, false);
		}
		
		error = trim_playlist_to_max_size(pc);
		if (error != 0) {
			errno = error;
			LOG_ERRNO("cache trim failed");
			PC_FREE_ZERO(pc);
		}
	}

	userspace_timestamps.ssup_oid_timestamp = mach_absolute_time();
	BC_set_userspace_timestamps(&userspace_timestamps);

	error = BC_start(pc);
	if (error != 0) {
		errno = error;
		LOG_ERRNO("could not start cache");
	}

	PC_FREE_ZERO(pc);
	
#ifdef DEBUG
	fflush(outstream);
#endif
	return(error);
}

/*
 * Fetch or create a login playlist for the given user and add it to the provided playlist in subsequent batches
 */
static int
add_playlist_for_preheated_user(struct BC_playlist *pc, bool isFusion) {

	if (!pc) return 1;

	// These defines match those in warmd
#define PLAYLIST_DIR						 "/var/db/BootCaches/"
#define MERGED_PLAYLIST						 "Merged.playlist"
#define LOGIN_PLAYLIST						 "Login.playlist"
#define DEFAULT_USER_DIR					 PLAYLIST_DIR"PreheatedUser/"
#define APP_CACHE_PLAYLIST_PREFIX			 "app."
#define PLAYLIST_SUFFIX						 ".playlist"
#define RUNNING_XATTR_NAME					 "BC_RUNNING"
#define MAX_PLAYLIST_PATH_LENGTH			 256
#define TAL_LOGIN_FOREGROUND_APP_BATCH_NUM	 0
#define TAL_LOGIN_BACKGROUND_APP_BATCH_NUM	 (TAL_LOGIN_FOREGROUND_APP_BATCH_NUM + 1)
#define TAL_LOGIN_LOWPRI_DATA_BATCH_NUM		 (TAL_LOGIN_BACKGROUND_APP_BATCH_NUM + 1)

	int error = 0, i;
	size_t playlist_path_end_idx = 0;
	char playlist_path[MAX_PLAYLIST_PATH_LENGTH] = DEFAULT_USER_DIR;
	struct BC_playlist* user_playlist = NULL;

	//rdar://8830944&9209576 Detect FDE user
	io_registry_entry_t service = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/chosen");
	if (service != MACH_PORT_NULL) {

		CFDataRef fde_login_user = IORegistryEntryCreateCFProperty(service, CFSTR(kCSFDEEFILoginUnlockIdentID),
																   kCFAllocatorDefault, kNilOptions);
		IOObjectRelease(service);
		if (fde_login_user != NULL) {
			char fde_login_user_str[128] = {0};

			CFDataGetBytes(fde_login_user, CFRangeMake(0, sizeof(fde_login_user_str)), (UInt8*)fde_login_user_str);
			CFRelease(fde_login_user);

			snprintf(playlist_path, sizeof(playlist_path), "%s%s/", PLAYLIST_DIR, fde_login_user_str);
			struct stat statbuf;
			if (0 == stat(playlist_path, &statbuf)) {
				DLOG("Using FDE user %s", fde_login_user_str);
			} else {
				LOG("No such FDE user %s, using Preheated User", fde_login_user_str);
				snprintf(playlist_path, sizeof(playlist_path), "%s", DEFAULT_USER_DIR);
			}
		}
	}

	playlist_path_end_idx = strlen(playlist_path);

	// DLOG("Reading user playlists from user dir %s", playlist_path);

	// Check for the merged playlist warmd has left
	strlcpy(playlist_path + playlist_path_end_idx, MERGED_PLAYLIST, sizeof(playlist_path) - playlist_path_end_idx);
	error = BC_read_playlist(playlist_path, &user_playlist);

	if (ENOENT == error) {
		// No merged playlist for the preheated user. Try to create one from its login and app playlists

		strlcpy(playlist_path + playlist_path_end_idx, LOGIN_PLAYLIST, sizeof(playlist_path) - playlist_path_end_idx);
		error = BC_read_playlist(playlist_path, &user_playlist);
		if (0 == error) {

			struct BC_playlist *app_playlist = NULL;
			struct dirent* direntry;
			int suffix_length = strlen(PLAYLIST_SUFFIX);
			int app_prefix_length = strlen(APP_CACHE_PLAYLIST_PREFIX);

			playlist_path[playlist_path_end_idx] = '\0';
			DIR* dir = opendir(playlist_path);


			// Iterate over the app playlists twice:
			// The first iteration to add the running apps to the playlist.
			// The second iteration to add the non-running apps to the low-priority playlist
			int iteration;
			for (iteration = 0; iteration < 2; iteration++) {
				int batch_offset = 0;
				int flags = 0x0;
				if (iteration == 0) {
					// We can't tell the order between apps, so just put them all background to give the login playlist priority
					batch_offset = TAL_LOGIN_BACKGROUND_APP_BATCH_NUM;
					if (isFusion) {
						//rdar://11294417 All user data is low priority on CoreStorage Fusion
						flags |= BC_PE_LOWPRIORITY;
					}
				} else {
					// Non-running apps are low-priority
					batch_offset = TAL_LOGIN_LOWPRI_DATA_BATCH_NUM;
					flags |= BC_PE_LOWPRIORITY;
				}

				while ((direntry = readdir(dir)) != NULL) {
					if (direntry->d_namlen > suffix_length && 0 == strncmp(direntry->d_name + direntry->d_namlen - suffix_length, PLAYLIST_SUFFIX, suffix_length)) {
						if (0 == strncmp(direntry->d_name, APP_CACHE_PLAYLIST_PREFIX, app_prefix_length)) {

							snprintf(playlist_path + playlist_path_end_idx, sizeof(playlist_path) - playlist_path_end_idx, "/%s", direntry->d_name);

							bool is_running = (-1 != getxattr(playlist_path, RUNNING_XATTR_NAME, NULL, 0, 0, 0x0));
							if ((iteration == 0) == is_running) { // First iteration we're looking for running apps; second, for non-running

								if (0 == BC_read_playlist(playlist_path, &app_playlist)) {

									// Adjust the flags and batch for this playlist as necessary
									for (i = 0; i < app_playlist->p_nentries; i++) {
										app_playlist->p_entries[i].pe_flags |= flags;
										app_playlist->p_entries[i].pe_batch += batch_offset;
									}
									for (i = 0; i < app_playlist->p_nomaps; i++) {
										app_playlist->p_omaps[i].po_omap.otr_batch += batch_offset;
									}

									if (0 != (error = BC_merge_playlists(user_playlist, app_playlist))) {
										PC_FREE_ZERO(app_playlist);
										goto out;
									}

									PC_FREE_ZERO(app_playlist);
									// DLOG("Added playlist for app %s", playlist_path);
								}

							}
						}
					} else {
						playlist_path[playlist_path_end_idx] = '\0';
						// DLOG("Unknown file in user playlist directory %s: %s", playlist_path, direntry->d_name);
					}
				}

				if (iteration == 0) {
					rewinddir(dir);
				}
			}
			closedir(dir);
			dir = NULL;




		}
	}

	if (error != 0) {
		goto out;
	}

	if (!isFusion) {
		// Add any files we know are read in every login, but change every login
		// --- none yet, use add_file ---
	}

	// Slide the user playlist by the number of batches we have in the boot playlist
	int batch_max = 0;
	for (i = 0; i < pc->p_nentries; i++) {
		if (pc->p_entries[i].pe_batch > batch_max) {
			batch_max = pc->p_entries[i].pe_batch;
		}
	}
	batch_max ++;
	for (i = 0; i < user_playlist->p_nentries; i++) {
		user_playlist->p_entries[i].pe_batch += batch_max;
	}
	for (i = 0; i < user_playlist->p_nomaps; i++) {
		user_playlist->p_omaps[i].po_omap.otr_batch += batch_max;
	}

	error = BC_merge_playlists(pc, user_playlist);
	if (error != 0) {
		BC_reset_playlist(pc);
	}

out:
	PC_FREE_ZERO(user_playlist);
	return (error);
}

/*
 * Upon return:
 * *max_high_priority_size_out is the the maximum size of the high priority batches, in bytes
 * *max_total_size_out is the maximum size of the cache when also including low priority
 *     (to limit memory impact of the low priority batch).
 *     I.E. if (high priority size) < (max_total_size), then the maximum amount of low priority
 *     bytes is (max_total_size) - (high priority size). Otherwise, no low priority bytes are allowed
 *
 * (max_total_size may be larger or smaller than max_high_priority_size)
 */
static void
max_playlist_size(uint64_t* max_high_priority_size_out, uint64_t* max_total_size_out)
{
	static dispatch_once_t onceToken;
	static uint64_t max_high_priority_size = 0;
	static uint64_t max_total_size = 0;
	dispatch_once(&onceToken, ^{
		uint64_t memsize = 0;
		size_t len = sizeof(memsize);
		int error = sysctlbyname("hw.memsize", &memsize, &len, NULL, 0);
		if(error != 0) {
			memsize = (4ull * 1024 * 1024 * 1024);
			LOG_ERRNO("sysctlbyname(\"hw.memsize\") failed. Assuming 4GB");
		} else {
			
			// Subtract any memory that is allocated by boot kernel tracing
			char bootargs[PAGE_SIZE];
			bootargs[0] = '\0';
			size_t s = sizeof(bootargs);
			if (0 != sysctlbyname("kern.bootargs", &(bootargs[0]), &s, NULL, 0)) {
				LOG_ERRNO("Unable to get kern.bootargs");
			} else {
				char* trace_str = strnstr(bootargs, "trace=", sizeof(bootargs));
				if (trace_str) {
					uint64_t trace_count;
					if (sscanf(trace_str, "trace=%lli", &trace_count) == 1) {
						uint64_t trace_size = trace_count * sizeof(kd_buf);
						if (memsize > trace_size) {
							memsize -= trace_size;
						}
					}
				}
			}

		}
		
#define BC_RESERVE_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST (1ull * 1024 * 1024 * 1024) // Make sure we leave 1GB free if the cache is completely wrong
#define BC_MIN_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST (1ull * 1024 * 1024 * 1024) // But don't force the BootCache size to less than 1GB

#define BC_RESERVE_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST (3ull * 1024 * 1024 * 1024) // Maks sure we leave 3GB of memory free since the low priority playlist isn't expected to be used (and its mainly read in after boot has completed)
#define BC_MIN_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST (0) // If we're low on memory, don't do any low priority playback

		// max high priority playlist size is memory size minus 1GB
		// (pages will be pulled from the cache as boot progresses, so its never all resident at once)
		if (memsize >= BC_RESERVE_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST + BC_MIN_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST) {
			// machine has >=2GB of memory, use default sizing
			max_high_priority_size = memsize - BC_RESERVE_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST;
		} else if (memsize >= BC_MIN_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST) {
			// 1-2GB in the machine, use 1GB
			max_high_priority_size = BC_MIN_MEM_SIZE_FOR_HIGH_PRIORITY_PLAYLIST;
		} else {
			// <1GB of memory, use memsize
			max_high_priority_size = memsize;
		}
		
		// max size that low priority entires can make a playlist to be is memory size minus 3GB
		// (low priority entries are not expected to be used, so will cause memory pressure if we pull in too much)
		// rdar://53720350 (BootCache causing memory pressure after login for 4GB machines)
		if (memsize >= BC_RESERVE_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST + BC_MIN_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST) {
			// machine has >=3GB of memory, use default sizing
			max_total_size = memsize - BC_RESERVE_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST;
		} else if (memsize >= BC_MIN_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST) {
			// <3GB in the machine, no low priority bytes allowed
			max_total_size = BC_MIN_MEM_SIZE_FOR_LOW_PRIORITY_PLAYLIST;
		} else {
			// Unused (would be used if we have a min for low priority playlist)
			max_total_size = memsize;
		}
	});
	
	*max_high_priority_size_out = max_high_priority_size;
	*max_total_size_out = max_total_size;
}

/*
 * Trim the lowest priority extents until the playlist
 * is under the provided size
 */
static int
trim_playlist_to_size(struct BC_playlist* pc, uint64_t max_high_priority_size, uint64_t max_total_size)
{

	struct BC_userspace_oversize oversize = {
		.ssup_highpri_bytes_trimmed = 0,
		.ssup_lowpri_bytes_trimmed = 0,
	};

	uint64_t playlist_low_priority_size = 0;
	uint64_t playlist_high_priority_size = 0;
	int max_batch = 0;
	for (int entry_idx = 0; entry_idx < pc->p_nentries; entry_idx++) {
		struct BC_playlist_entry* pe = pc->p_entries + entry_idx;
		if (pe->pe_flags & BC_PE_LOWPRIORITY) {
			playlist_low_priority_size += pe->pe_length;
		} else {
			playlist_high_priority_size += pe->pe_length;
			if (max_batch < pe->pe_batch) {
				max_batch = pe->pe_batch;
			}
		}
	}

	if (playlist_high_priority_size > max_high_priority_size) {
		
		// High priority part of the playlist is too large,
		// trim off entries in last-to-first batch order
		// Also, iterate in reverse block address order
		// (because lower extents tend to be grouped tighter in my experience)
		uint64_t playlist_high_priority_size_orig = playlist_high_priority_size;

		for (int batch = max_batch; batch >= 0; batch--) {
			for (int entry_idx = pc->p_nentries - 1; entry_idx >= 0; entry_idx--) {
				struct BC_playlist_entry* pe = pc->p_entries + entry_idx;
				if ((pe->pe_batch == batch) && !(pe->pe_flags & BC_PE_LOWPRIORITY)) {
					if (pe->pe_length > playlist_high_priority_size - max_high_priority_size) {
						pe->pe_length -= (playlist_high_priority_size - max_high_priority_size);
						playlist_high_priority_size = max_high_priority_size;
					} else {
						playlist_high_priority_size -= pe->pe_length;
						pe->pe_length = 0;
					}
				}
				if (playlist_high_priority_size <= max_high_priority_size) {
					break;
				}
			}
			if (playlist_high_priority_size <= max_high_priority_size) {
				break;
			}
		}
		
		LOG("Trimmed %llu high priority bytes due to oversize (%llu > %llu)", playlist_high_priority_size_orig - playlist_high_priority_size, playlist_high_priority_size_orig, max_high_priority_size);
		oversize.ssup_highpri_bytes_trimmed = playlist_high_priority_size_orig - playlist_high_priority_size;
	}
	
	if (playlist_high_priority_size + playlist_low_priority_size > max_total_size) {
		
		// Low priority part of the playlist is too large trim off entries
		uint64_t playlist_low_priority_size_orig = playlist_low_priority_size;
		
		for (int entry_idx = pc->p_nentries - 1; entry_idx >= 0; entry_idx--) {
			struct BC_playlist_entry* pe = pc->p_entries + entry_idx;
			if (pe->pe_flags & BC_PE_LOWPRIORITY) {
				if (pe->pe_length > playlist_high_priority_size + playlist_low_priority_size - max_total_size) {
					pe->pe_length -= (playlist_high_priority_size + playlist_low_priority_size - max_total_size);
					playlist_low_priority_size = max_total_size - playlist_high_priority_size;
				} else {
					playlist_low_priority_size -= pe->pe_length;
					pe->pe_length = 0;
				}
			}
			if (playlist_high_priority_size + playlist_low_priority_size <= max_total_size) {
				break;
			}
		}
		
		LOG("Trimmed %llu low priority bytes due to oversize (%llu > %llu)", playlist_low_priority_size_orig - playlist_low_priority_size, playlist_high_priority_size + playlist_low_priority_size_orig, max_total_size);
		oversize.ssup_lowpri_bytes_trimmed = playlist_low_priority_size_orig - playlist_low_priority_size;
	}
	
	int err = BC_sort_and_coalesce_playlist(pc);
	if (err == 0) {
		BC_set_userspace_oversize(&oversize);
	}
	return err;
}


/*
 * Trim the lowest priority extents until the playlist
 * is under the maximum size
 */
static int
trim_playlist_to_max_size(struct BC_playlist* pc)
{
	uint64_t max_high_priority_size = 0;
	uint64_t max_total_size = 0;
	max_playlist_size(&max_high_priority_size, &max_total_size);
	return trim_playlist_to_size(pc, max_high_priority_size, max_total_size);
}


/*
 * Add contents of the file to the bootcache
 */
static int
merge_into_batch(struct BC_playlist *dest, const struct BC_playlist *source, const int batch, const bool low_priority)
{
	if (low_priority) {
		for (int i = 0; i < source->p_nentries; i++) {
			source->p_entries[i].pe_flags |= BC_PE_LOWPRIORITY;
		}
	} else {
		
		if(batch > 0){
			int i;
			for (i = 0; i < source->p_nentries; i++) {
				source->p_entries[i].pe_batch += batch;
			}
			for (i = 0; i < source->p_nomaps; i++) {
				source->p_omaps[i].po_omap.otr_batch += batch;
			}
		} else if (batch < 0) {
			int i;
			int inverse_batch = 0 - batch;
			for (i = 0; i < dest->p_nentries; i++) {
				dest->p_entries[i].pe_batch += inverse_batch;
			}
			for (i = 0; i < dest->p_nomaps; i++) {
				dest->p_omaps[i].po_omap.otr_batch += inverse_batch;
			}
		}
	}
	
	int error = BC_merge_playlists(dest, source);
	
	if (error != 0) {
		BC_reset_playlist(dest);
	}
	
	return error;
}

/*
 * Add contents of the file to the bootcache
 */
static int
add_file(struct BC_playlist *pc, const char *fname, const int batch, const bool low_priority)
{
#define MAX_FILESIZE (10ll * 1024 * 1024) // 10MB
	int error;
	int fd = open(fname, O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT && errno != EINVAL) {
			LOG_ERRNO("Unable to open %s to add it to the boot cache", fname);
		}
		return errno;
	}

	off_t maxsize = MAX_FILESIZE;
	
	struct BC_playlist *playlist = NULL;
	error = BC_playlist_for_filename(fd, fname, maxsize, &playlist);
	close(fd);
	if (playlist == NULL) {
		if (error != ENOENT && error != EINVAL) {
			LOG_ERRNO("Unable to create playlist for %s", fname);
		}
		return error;
	}
	
	error = merge_into_batch(pc, playlist, batch, low_priority);
	PC_FREE_ZERO(playlist);
	
	return error;
}

/*
 * Add contents of a directory to the bootcache.  Does not operate recursively.
 */
static int
add_directory(struct BC_playlist *pc, const char *dname, const int batch)
{
#define MAX_FILES_PER_DIR 10

	DIR *dirp = opendir(dname);
	if (!dirp)
		return errno;

	int count = 0;

	struct dirent *dp;
	while ((dp = readdir(dirp)) != NULL){
		if (dp->d_type != DT_REG)
			continue;

		char fname[MAXPATHLEN];
		int ret = snprintf(fname, MAXPATHLEN, "%s/%s", dname, dp->d_name);
		if (ret < 0 || ret >= MAXPATHLEN)
			continue;

		add_file(pc, fname, batch, false);
		if (++count >= MAX_FILES_PER_DIR) break;
	}

	closedir(dirp);

	return 0;
}

/*
 * Add fseventsd-related files to the bootcache.
 *
 * We add the 10 newest files of the fsventsd folder to the boot cache
 */
static int
add_fseventsd_files(struct BC_playlist *pc, const int batch)
{
#define FSEVENTSD_DIR	  "/.fseventsd"
#define MAX_NAME_LENGTH   64
#define NUM_FILES_TO_WARM 10

	DIR *dirp = opendir(FSEVENTSD_DIR);
	if (!dirp)
		return errno;

	char newest_files[NUM_FILES_TO_WARM][MAX_NAME_LENGTH];
	int i;
	for (i = 0; i < NUM_FILES_TO_WARM; i++) {
		newest_files[i][0] = '\0';
	}

	// Copy the first 10 files into our array
	struct dirent *dp;
	for (i = 0; i < NUM_FILES_TO_WARM && (dp = readdir(dirp)) != NULL; i++) {
		if (dp->d_type != DT_REG)
			continue;
		strlcpy(newest_files[i], dp->d_name, MAX_NAME_LENGTH);
	}

	// Check if we ran out of directory entries
	if (dp) {

		//Find the oldest file of the first 10
		char* oldest_new_file = newest_files[0];
		for (i = 1; i < NUM_FILES_TO_WARM; i++) {
			if (strncmp(newest_files[i], oldest_new_file, MAX_NAME_LENGTH) < 0) {
				oldest_new_file = newest_files[i];
			}
		}

		// Find any files that are newer than the first 10
		while ((dp = readdir(dirp)) != NULL){
			if (dp->d_type != DT_REG)
				continue;

			// We want the last 10 files, and they're ordered alphabetically
			if (strncmp(oldest_new_file, dp->d_name, MAX_NAME_LENGTH) < 0) {
				strlcpy(oldest_new_file, dp->d_name, MAX_NAME_LENGTH);

				// We replaced the oldest file, find the oldest of the 10 newest again
				for (i = 0; i < NUM_FILES_TO_WARM; i++) {
					if (strncmp(newest_files[i], oldest_new_file, MAX_NAME_LENGTH) < 0) {
						oldest_new_file = newest_files[i];
					}
				}
			}
		}
	}
	closedir(dirp);

	for (i = 0; i < NUM_FILES_TO_WARM; i++) {
		if (newest_files[i][0] != '\0') {
			char fname[128];
			int ret = snprintf(fname, 128, "%s/%s", FSEVENTSD_DIR, newest_files[i]);
			if (ret < 0 || ret >= 128)
				continue;

			add_file(pc, fname, batch, false);
		}
	}


	return 0;
}


/*
 * Add the native shared cache to the bootcache.
 */
static int
add_shared_cache(struct BC_playlist *pc, const char* shared_cache_path, int batch, bool low_priority, bool look_for_update)
{
	int error;
	
	int fd = open(shared_cache_path, O_RDONLY);
	if (fd == -1) {
		if (errno != ENOENT && errno != EINVAL) {
			LOG_ERRNO("Unable to open %s to add it to the boot cache", shared_cache_path);
		}
		return errno;
	}

	struct BC_playlist *playlist = NULL;
	error = BC_playlist_for_filename(fd, shared_cache_path, 0, &playlist);
	close(fd);
	if (playlist == NULL) {
		if (error != ENOENT && error != EINVAL) {
			LOG_ERRNO("Unable to create playlist for %s", shared_cache_path);
		}
		return error;
	}

	for (int i = 0; i < playlist->p_nentries; i++) {
		playlist->p_entries[i].pe_flags |= BC_PE_SHARED;
	}
	
	// Handle dyld shared cache being updated since last boot
	if (look_for_update) {
		
		// Check for overlap between our playlist and the new shared cache
		if (! BC_playlists_intersect(pc, playlist)) {
			DLOG("Shared cache has moved, removing old shared cache entries, and explicitly new shared cache at high priority");
			
			// Remove all old shared cache entires (all shared caches, since when one updates, they probably all have been updated)
			for (int i = 0; i < pc->p_nentries; i++) {
				if (pc->p_entries[i].pe_flags & BC_PE_SHARED) {
					pc->p_entries[i].pe_length = 0;
				}
			}
			
			// Even if we requested low priority, make the new shared cache high priority (because our previous recording of shared cache I/Os is now wrong)
			low_priority = false;
			
		} else {
			DLOG("Shared cache hasn't moved");
			// The shared cache is in the same place on disk as last boot, don't need to do anything special
		}
	}
	

	error = merge_into_batch(pc, playlist, batch, low_priority);
	PC_FREE_ZERO(playlist);
	
	return error;
}

/*
 * Add the native shared cache to the bootcache.
 */
static int
add_native_shared_cache(struct BC_playlist *pc, int batch, bool low_priority)
{
#if ! BC_ADD_SHARED_CACHE_AT_HIGH_PRIORITY
	low_priority = true;
#endif
	
	const char* shared_cache_path = dyld_shared_cache_file_path();
	if (! shared_cache_path) {
		LOG("No shared cache path");
		return ENOENT;
	}
	
	return add_shared_cache(pc, shared_cache_path, batch, low_priority, true);
}


/*
 * Start the cache, optionally passing in the playlist file.
 */
static int
start_cache(const char *pfname)
{
	struct BC_playlist *pc;
	int error;

	/* set up to start recording with no playback */
	pc = NULL;

	if (pfname == NULL)
		errx(1, "No playlist provided");

	/*
	 * TAL App Cache needs a way to record without playback
	 */
	if (strcmp(pfname, "nofile")) {

		/*
		 * If we have a playlist, open it and prepare to load it.
		 */
		error = BC_read_playlist(pfname, &pc);

		/*
		 * If the playlist is missing or invalid, ignore it. We'll
		 * overwrite it later.
		 */
		if ((error != 0) && (error != EINVAL) && (error != ENOENT)) {
			errno = error;
			LOG_ERRNO("could not read playlist %s", pfname);
			PC_FREE_ZERO(pc);
			return(error);
		}

		/*
		 * Remove the consumed playlist: it'll be replaced once BootCache
		 * completes successfully.
		 */

		//char prev_path[MAXPATHLEN];
		//snprintf(prev_path, sizeof(prev_path), "%s.previous", pfname);
		//error = rename(pfname, prev_path); // for debugging
		error = unlink(pfname);
		if (error != 0 && errno != ENOENT) {
			LOG_ERRNO("could not unlink playlist %s", pfname);
		}
		
		error = trim_playlist_to_max_size(pc);
		if (error != 0) {
			errno = error;
			LOG_ERRNO("cache trim failed");
			PC_FREE_ZERO(pc);
			return error;
		}
	}

	error = BC_start(pc);
	if (error != 0) {
		errno = error;
		LOG_ERRNO("could not start cache");
	} else {
		LOG("Started cache successfully");
	}
	PC_FREE_ZERO(pc);
	return(error);
}

/*
 * Stop the cache and fetch the history list.  Post-process the history list
 * and save to the named playlist file.
 */
static int
stop_cache(const char *pfname, int debugging)
{
	struct BC_playlist *pc;
	struct BC_history  *hc;
	struct BC_omap_history *oh;
	struct BC_statistics *ss;
	int error;

	/*
	 * Stop the cache and fetch the history and omap list.
	 */
	if ((error = BC_stop_and_fetch(&hc, &oh)) != 0) {
		errno = error;
		LOG_ERRNO("Could not stop history and omaps");
		HC_FREE_ZERO(hc);
		OH_FREE_ZERO(oh);
		return error;
	}

	if (verbose > 0) {
		print_statistics(NULL);
		if (verbose > 1)
			print_history(hc);
	}

	/* write history and stats to debug logs if debugging */
	if (debugging) {
		BC_print_history(BC_BOOT_HISTFILE, hc);
		BC_print_omap_history(BC_BOOT_OMAPHISTFILE, oh);
		if ((error = BC_fetch_statistics(&ss)) != 0)
			errx(1, "could not fetch cache statistics: %d %s", error, strerror(error));
		if (ss != NULL) {
			BC_print_statistics(BC_BOOT_STATFILE, ss);
		}
	}

	/*
	 * If we have not been asked to update the history list, we are done
	 * here.
	 */
	if (pfname == NULL) {
		HC_FREE_ZERO(hc);
		OH_FREE_ZERO(oh);
		return(0);
	}

	/*
	 * Convert the history and omap list to playlist with omap format.
	 */
	if ((error = BC_convert_history_and_omaps(hc, oh, &pc)) != 0) {
		if (!debugging) {
			BC_print_history(BC_BOOT_HISTFILE, hc);
			BC_print_omap_history(BC_BOOT_OMAPHISTFILE, oh);
		}
		HC_FREE_ZERO(hc);
		OH_FREE_ZERO(oh);
		errx(1, "history to playlist conversion failed: %d %s", error, strerror(error));
	}

	HC_FREE_ZERO(hc);
	OH_FREE_ZERO(oh);
	
	/*
	 * Securely overwrite the previous playlist.
	 */
	if ((error = BC_write_playlist(pfname, pc) != 0)) {
		errx(1, "could not write playlist: %d %s", error, strerror(error));
	}

	return(0);
}

/*
 * Jettison the cache.
 */
static int
jettison_cache(void)
{
	int error;

	/*
	 * Stop the cache and fetch the history list.
	 */
	if ((error = BC_jettison()) != 0)
		return error;

	if (verbose > 0)
		print_statistics(NULL);

	return(0);
}


/*
 * Merge multiple playlist files.
 */
static int
merge_playlists(const char *pfname, int argc, char *argv[], int* pbatch)
{
	struct BC_playlist *pc = NULL, *npc = NULL;
	int i, j, error;

	/* Created just for the purposes of a proper initial merge */
	pc = BC_allocate_playlist(0, 0, 0);

	if (pfname == NULL)
		errx(1, "must specify a playlist file to merge");

	/*
	 * Read playlists into memory.
	 */
	for (i = 0; i < argc; i++) {

		/*
		 * Read the next playlist and merge with the current set.
		 */
		if (BC_read_playlist(argv[i], &npc) != 0) {
			error = 1;
			goto out;
		}

		/*
		 * Force the second and subsequent playlists into the specified batch
		 */
		if (pbatch && i > 0) {
			for (j = 0; j < npc->p_nentries; j++) {
				npc->p_entries[j].pe_batch += *pbatch;
			}
			for (j = 0; j < npc->p_nomaps; j++) {
				npc->p_omaps[j].po_omap.otr_batch += *pbatch;
			}
		}

		if (BC_merge_playlists(pc, npc)){
			error = 1;
			goto out;
		}
		PC_FREE_ZERO(npc);
	}

	error = BC_write_playlist(pfname, pc);

out:
	PC_FREE_ZERO(npc);
	PC_FREE_ZERO(pc);

	return error;
}

/*
 * Print statistics.
 */
static int
print_statistics(struct BC_statistics *ss)
{
	int error;

	if (ss == NULL) {
		if ((error = BC_fetch_statistics(&ss)) != 0) {
			errx(1, "could not fetch statistics: %d %s", error, strerror(error));
		}
	}

	return(BC_print_statistics(NULL, ss));
}

/*
 * Print debug buffer to stdout.
 */
static int
print_debug_buffer(void)
{
	int error;
	char* buf = NULL;
	size_t bufSize = 0;
	if ((error = BC_fetch_debug_buffer(&buf, &bufSize)) != 0) {
		errx(1, "could not fetch debug buffer: %d %s", error, strerror(error));
	}
	if (!buf || bufSize == 0) {
		errx(1, "No debug buffer");
	}
	
	ssize_t numBytesWritten = 0;
	do {
		ssize_t bytes = write(STDOUT_FILENO, buf + numBytesWritten, bufSize - numBytesWritten);
		if (bytes <= 0) {
			errx(1, "could not write debug buffer to stdout: %d %s", error, strerror(error));
		}
		numBytesWritten += bytes;
	} while (numBytesWritten < bufSize);

	free(buf);
	return(0);
}

/*
 * Print history entries.
 */
static void
print_history(struct BC_history *hc)
{
	int i;
	for (i = 0; i < hc->h_nentries; i++) {
		LOG("%s %s %-12llu %#-12llx %#-8llx %#-12llx %5u%s%s",
			uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_uuid),
			uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_group_uuid),
			hc->h_entries[i].he_inode,
			hc->h_entries[i].he_offset, hc->h_entries[i].he_length,
			hc->h_entries[i].he_crypto_offset,
			hc->h_entries[i].he_pid,
			hc->h_entries[i].he_flags & BC_HE_OPTIMIZED ? " optimized" :
			hc->h_entries[i].he_flags & BC_HE_HIT	? " hit"	:
			hc->h_entries[i].he_flags & BC_HE_WRITE	? " write"	:
			hc->h_entries[i].he_flags & BC_HE_TAG	? " tag"	: " miss",
			hc->h_entries[i].he_flags & BC_HE_SHARED ? " shared" : "");
	}
}

/*
 * Print a playlist from a file.
 */
static int
print_playlist_on_disk(const char *pfname)
{
	struct BC_playlist *pc;

	if (pfname == NULL)
		errx(1, "must specify a playlist file to print");

	/*
	 * Suck in the playlist.
	 */
	if (BC_read_playlist(pfname, &pc))
		errx(1, "could not read playlist");

	BC_print_playlist(pc, true);

	PC_FREE_ZERO(pc);
	return(0);
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
		errx(1, "unable to determine uuid for volume %s", volume);
	}

	uuid_copy(uuid_out, attrBuf.uuid);
}

/*
 * Read a playlist and write to a file.
 */
static int
unprint_playlist(const char *pfname)
{
	struct BC_playlist *pc;
	int nentries, alloced, error, got, i;
	uuid_string_t uuid_string;
	uuid_string_t uuid_grouping_string;
	uuid_t uuid;
	int seen_old_style;
	int m_nentries;
	int m_fs_flags;
	u_int64_t unused;
	char buf[128];

	pc = BC_allocate_playlist(0, 0, 0);
	if(!pc) errx(1, "could not allocate initial playlist");

	alloced = 0;
	nentries = 0;
	seen_old_style = 0;

	if (pfname == NULL)
		errx(1, "must specify a playlist file to create");

	while (NULL != fgets(buf, sizeof(buf), stdin)) {

		/*
		 * Parse mounts
		 */
		got = sscanf(buf, "Mount %s grouping %s flags 0x%x, %u entries",
					 uuid_string, uuid_grouping_string, &m_fs_flags, &m_nentries);

		if (got == 4) {
			/* make sure we have space for the next mount */
			if ((pc->p_mounts = realloc(pc->p_mounts, (pc->p_nmounts + 1) * sizeof(*pc->p_mounts))) == NULL)
				errx(1, "could not allocate memory for %d mounts", pc->p_nmounts + 1);

			pc->p_mounts[pc->p_nmounts].pm_nentries  = m_nentries;
			pc->p_mounts[pc->p_nmounts].pm_fs_flags  = m_fs_flags;
			pc->p_mounts[pc->p_nmounts].pm_nomaps = 0;
			uuid_parse(uuid_string, pc->p_mounts[pc->p_nmounts].pm_uuid);
			if (0 != strcmp("none", uuid_grouping_string)) {
				uuid_parse(uuid_grouping_string, pc->p_mounts[pc->p_nmounts].pm_group_uuid);
			} else {
				uuid_clear(pc->p_mounts[pc->p_nmounts].pm_group_uuid); // No grouping
			}

			pc->p_nmounts++;

			continue;
		}

		/*
		 * Parse entries
		 */

		/* make sure we have space for the next entry */
		if (nentries >= alloced) {
			if (alloced == 0) {
				alloced = 100;
			} else {
				alloced = alloced * 2;
			}
			if ((pc->p_entries = realloc(pc->p_entries, alloced * sizeof(*pc->p_entries))) == NULL)
				errx(1, "could not allocate memory for %d entries", alloced);
		}

		/* read input */
		got = sscanf(buf, "%s %llx %llx %hi %hx %llx",
					 uuid_string,
					 &(pc->p_entries + nentries)->pe_offset,
					 &(pc->p_entries + nentries)->pe_length,
					 &(pc->p_entries + nentries)->pe_batch,
					 &(pc->p_entries + nentries)->pe_flags,
					 &(pc->p_entries + nentries)->pe_crypto_offset);
		if (got == 5) {
			uuid_parse(uuid_string, uuid);
			for (i = 0; i < pc->p_nmounts; i++) {
				if (0 == uuid_compare(uuid, pc->p_mounts[i].pm_uuid)) {
					(pc->p_entries + nentries)->pe_mount_idx = i;
					break;
				}
			}
			if (i == pc->p_nmounts) {
				errx(1, "entry doesn't match any existing mount:\n%s", buf);
			}

			nentries++;
			continue;

		}

		/* Support old-style format (no UUID, root volume implied) */
		got = sscanf(buf, "%llu %llu %hu",
					 &(pc->p_entries + nentries)->pe_offset,
					 &(pc->p_entries + nentries)->pe_length,
					 &(pc->p_entries + nentries)->pe_batch);
		if (got == 3) {

			if (! seen_old_style) {
				if (pc->p_nmounts > 0) {
					errx(1, "Bad entry line (Needs mount UUID):\n%s", buf);
				}
				LOG("No mount UUID provided, assuuming root volume");
				pc->p_nmounts = 1;
				if ((pc->p_mounts = calloc(1, sizeof(*pc->p_mounts))) == NULL)
					errx(1, "could not allocate memory for %d mounts", pc->p_nmounts);
				pc->p_mounts[0].pm_nentries = 0;
				pc->p_mounts[0].pm_fs_flags = 0; // Only support non-encrypted for this old style
				get_volume_uuid("/", pc->p_mounts[0].pm_uuid);
				seen_old_style = 1;
			}

			(pc->p_entries + nentries)->pe_mount_idx = 0;
			(pc->p_entries + nentries)->pe_flags = 0;
			(pc->p_entries + nentries)->pe_crypto_offset = 0;
			nentries++;

			continue;
		}

		/* Ignore the line from print_playlist that has the total size of the cache */
		got = sscanf(buf, "%llu bytes", &unused);
		if (got == 1) {
			continue;
		}

		got = sscanf(buf, "%llu low-priority bytes", &unused);
		if (got == 1) {
			continue;
		}

		/* Ignore the line from the old-style print_playlist that has the block size of the cache */
		got = sscanf(buf, "%llu-byte blocks", &unused);
		if (got == 1) {
			continue;
		}

		/* Ignore the line from theold-style print_playlist that has the total size of the cache */
		got = sscanf(buf, "%llu blocks", &unused);
		if (got == 1) {
			continue;
		}

		errx(1, "Bad input line: '%s'", buf);
	}

	pc->p_nentries = nentries;
	if (seen_old_style)
		pc->p_mounts[0].pm_nentries = nentries;
	
	if (BC_verify_playlist(pc)) {
		errx(1, "Playlist failed verification");
	}

	if ((error = BC_write_playlist(pfname, pc)) != 0)
		errx(1, "could not create playlist: %d %s", error, strerror(error));

	PC_FREE_ZERO(pc);
	return(0);
}

/*
 * Modify the playlist the work with any root mount.
 */
static int
generalize_playlist(const char *pfname)
{
	struct BC_playlist *pc;

	if (pfname == NULL)
		errx(1, "must specify a playlist file to print");

	/*
	 * Suck in the playlist.
	 */
	if (BC_read_playlist(pfname, &pc))
		errx(1, "could not read playlist");

	if (pc->p_nmounts != 1)
		errx(1, "Playlist generalization only works on playlists with a single mount");

	uuid_clear(pc->p_mounts[0].pm_uuid);
	BC_write_playlist(pfname, pc);

	PC_FREE_ZERO(pc);
	return(0);
}


/*
 * Truncate a playlist to a given number of entries.
 */
static int
truncate_playlist(const char *pfname, char *larg)
{
	struct BC_playlist *pc;
	char *cp;
	int length, error, i;

	if (pfname == NULL)
		errx(1, "must specify a playlist file to truncate");

	length = (int) strtol(larg, &cp, 0);
	if ((*cp != 0) || (length < 1))
		err(1, "bad truncate length '%s'", larg);

	/*
	 * Suck in the playlist.
	 */
	if (BC_read_playlist(pfname, &pc))
		errx(1, "could not read playlist to truncate");

	/*
	 * Make sure the new length is shorter.
	 */
	if (length > pc->p_nentries) {
		LOG("playlist is shorter than specified truncate length");
	} else {

		for (i = length; i < pc->p_nentries; i++) {
			pc->p_mounts[pc->p_entries[i].pe_mount_idx].pm_nentries--;
		}
		for (i = 0; i < pc->p_nmounts; i++) {
			if (pc->p_mounts[i].pm_nentries == 0 && pc->p_mounts[i].pm_nomaps == 0) {
				memmove(pc->p_mounts + i, pc->p_mounts + i + 1, pc->p_nmounts - i - 1);
				pc->p_nmounts--;
				i--;
			}
		}
		pc->p_nentries = length;

		/*
		 * Write a shortened version of the same playlist.
		 */
		if ((error = BC_write_playlist(pfname, pc)) != 0)
			errx(1, "could not truncate playlist: %d %s", error, strerror(error));
	}
	PC_FREE_ZERO(pc);
	return(0);
}


#pragma mark -
// Code copied from warmd that should really be in libary.c, except libBootCache is used by other people (apfs) so I don't want to add linking requirements to it

/*!
 * @function    _bootcachectl_copy_root_volume_uuid
 *
 * @abstract
 *
 * Returns the CoreStorage Logical Volume UUID for the root disk, if it is CoreStorage, and *is_apfs_out will be false
 * Returns the APFS Volume UUID for the root disk, if it is APFS, and *is_apfs_out will be true
 * Upon non-null return, *is_fusion_out will indicate whether the volume is Fusion or not
 * Return NULL if the root volume is not CS nor APFS
 *
 */
static CFStringRef
_bootcachectl_copy_root_volume_uuid(bool *is_apfs_out, bool* is_fusion_out)
{
	//*********************************************//
	// Get the bsd device name for the root volume //
	//*********************************************//
	
	char* bsdDevPath = NULL;
	
	struct statfs buffer;
	bzero (&buffer, sizeof (buffer));
	if (0 == statfs("/", &buffer)) {
		
		if (strlen(buffer.f_mntfromname) >= 9) { /* /dev/disk */
			
			bsdDevPath = &(buffer.f_mntfromname[5]);
			
		}
	}
	
	if (NULL == bsdDevPath) {
		// No bsd device? Assume not Fusion
		return NULL;
	}
	
	return _bootcachectl_copy_uuid_for_bsd_device(bsdDevPath, is_apfs_out, is_fusion_out);
	
}
// _bootcachectl_copy_root_volume_uuid

/*!
 * @function    _bootcachectl_copy_uuid_for_bsd_device
 *
 * @abstract
 *
 * Returns the CoreStorage Logical Volume UUID for the bsd device, if it is CoreStorage, and *is_apfs_out will be false
 * Returns the APFS Volume UUID for the bsd device, if it is APFS, and *is_apfs_out will be true
 * Upon non-null return, *is_fusion_out will indicate whether the volume is Fusion or not
 * Return NULL if the root volume is not CS nor APFS
 */
static CFStringRef
_bootcachectl_copy_uuid_for_bsd_device(const char* bsd_name, bool *is_apfs_out, bool *is_fusion_out)
{
	if (NULL == bsd_name) return NULL;
	
	if (is_apfs_out) {
		*is_apfs_out = false;
	}
	if (is_fusion_out) {
		*is_fusion_out = false;
	}
	
	//*************************************************************//
	// Get the CoreStorage Logical Volume UUID from the bsd device //
	// or the APFS volume UUID                                     //
	//*************************************************************//
	
	CFStringRef uuid = NULL;
	
	mach_port_t masterPort;
	if (KERN_SUCCESS == IOMasterPort(bootstrap_port, &masterPort)) {
		
		io_registry_entry_t serviceMatchingBSDName = IOServiceGetMatchingService(masterPort, IOBSDNameMatching(masterPort, 0, bsd_name));
		if (IO_OBJECT_NULL != serviceMatchingBSDName) {
			
			if (IOObjectConformsTo(serviceMatchingBSDName, kCoreStorageLogicalClassName) ||
				IOObjectConformsTo(serviceMatchingBSDName, APFS_VOLUME_OBJECT) ||
				IOObjectConformsTo(serviceMatchingBSDName, APFS_SNAPSHOT_OBJECT)) {
				
				CFTypeRef cfRef = IORegistryEntryCreateCFProperty(serviceMatchingBSDName, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
				if (NULL != cfRef) {
					
					if (CFGetTypeID(cfRef) == CFStringGetTypeID()) {
						
						uuid = CFStringCreateCopy(NULL, cfRef);
						
					}
					
					CFRelease(cfRef);
				}
				
				if (is_apfs_out || is_fusion_out) {
					if (IOObjectConformsTo(serviceMatchingBSDName, APFS_VOLUME_OBJECT) ||
						IOObjectConformsTo(serviceMatchingBSDName, APFS_SNAPSHOT_OBJECT)) {
						if (is_apfs_out) {
							*is_apfs_out = true;
						}
						
						if (is_fusion_out) {
							io_registry_entry_t volume = IO_OBJECT_NULL;
							if (IOObjectConformsTo(serviceMatchingBSDName, APFS_SNAPSHOT_OBJECT)) {
								kern_return_t err = IORegistryEntryGetParentEntry(serviceMatchingBSDName, kIOServicePlane, &volume);
								if (err == 0) {
									if (IOObjectConformsTo(volume, APFS_VOLUME_OBJECT)) {
										// Good case
									} else {
										LOG("Parent is not volume for snapshot %s", bsd_name);
										IOObjectRelease(volume);
										volume = IO_OBJECT_NULL;
									}
								} else {
									LOG("Unable to get volume for snapshot %s: %d", bsd_name, err);
								}
							} else {
								IOObjectRetain(serviceMatchingBSDName);
								volume = serviceMatchingBSDName;
							}


							if (volume != IO_OBJECT_NULL) {
								io_registry_entry_t container;
								kern_return_t err = IORegistryEntryGetParentEntry(volume, kIOServicePlane, &container);
								if (err == 0) {
									if (IOObjectConformsTo(container, APFS_CONTAINER_OBJECT)) {
										
										io_registry_entry_t media;
										err = IORegistryEntryGetParentEntry(container, kIOServicePlane, &media);
										if (err == 0) {
											if (IOObjectConformsTo(media, APFS_MEDIA_OBJECT)) {
												
												io_registry_entry_t scheme;
												err = IORegistryEntryGetParentEntry(media, kIOServicePlane, &scheme);
												if (err == 0) {
													if (IOObjectConformsTo(scheme, APFS_SCHEME_OBJECT)) {
														
														
														CFBooleanRef container_is_fusion = IORegistryEntryCreateCFProperty(scheme, CFSTR(kAPFSContainerIsCompositedKey), kCFAllocatorDefault, 0);
														if (container_is_fusion != NULL) {
															
															if (CFGetTypeID(container_is_fusion) == CFBooleanGetTypeID()) {
																if (CFBooleanGetValue(container_is_fusion)) {
																	*is_fusion_out = true;
																}
															} else {
																LOG("Disk %s has bad composited property type %lu", bsd_name, CFGetTypeID(container_is_fusion));
															}
															
															CFRelease(container_is_fusion);
														}
														
													} else {
														LOG("Parent is not an apfs scheme for apfs media for volume %s", bsd_name);
													}
													IOObjectRelease(scheme);
													scheme = IO_OBJECT_NULL;
												} else {
													LOG("Unable to get scheme for volume %s: %d", bsd_name, err);
												}
												
												
											} else {
												LOG("Parent is not an apfs media for apfs container for volume %s", bsd_name);
											}
											IOObjectRelease(media);
											media = IO_OBJECT_NULL;
										} else {
											LOG("Unable to get media for volume %s: %d", bsd_name, err);
										}
										
										
									} else {
										LOG("Parent is not an apfs container for volume %s", bsd_name);
									}
									IOObjectRelease(container);
									container = IO_OBJECT_NULL;
								} else {
									LOG("Unable to get container for volume %s: %d", bsd_name, err);
								}
								IOObjectRelease(volume);
								volume = IO_OBJECT_NULL;
							}
						}
						
						
					} else {
						if (is_apfs_out) {
							*is_apfs_out = false;
						}
						
						if (is_fusion_out) {
							CFBooleanRef cs_volume_is_fusion = IORegistryEntryCreateCFProperty(serviceMatchingBSDName, CFSTR(kCoreStorageIsCompositedKey), kCFAllocatorDefault, 0);
							if (cs_volume_is_fusion != NULL) {
								
								if (CFGetTypeID(cs_volume_is_fusion) == CFBooleanGetTypeID()) {
									if (CFBooleanGetValue(cs_volume_is_fusion)) {
										*is_fusion_out = true;
									} else {
										*is_fusion_out = false;
									}
								} else {
									*is_fusion_out = false;
									LOG("Disk %s has bad composited property type %lu", bsd_name, CFGetTypeID(cs_volume_is_fusion));
								}
								
								CFRelease(cs_volume_is_fusion);
							} else {
								*is_fusion_out = false;
							}
						}
					}
				}
				
				
			} else {
				// Case for non-CoreStorage, non-APFS filesystems, nothing special to do for them
			}
			
			IOObjectRelease(serviceMatchingBSDName);
		}
		
	}
	
	return uuid;
}
// _bootcachectl_copy_uuid_for_bsd_device
