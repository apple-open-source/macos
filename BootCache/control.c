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
#include <IOKit/storage/CoreStorage/CoreStorageCryptoIDs.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <sys/mount.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <notify.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "BootCache.h"

// --- For composite disk checking
#include <IOKit/storage/CoreStorage/CoreStorageUserLib.h>
#include <IOKit/storage/IOMedia.h>

static int	usage(const char *reason);
static int	do_boot_cache();
static int	start_cache(const char *pfname);
static int	stop_cache(const char *pfname, int debugging);
static int	jettison_cache(void);
static int	merge_playlists(const char *pfname, int argc, char *argv[], int* pbatch);
static int	print_statistics(struct BC_statistics *ss);
static void	print_history(struct BC_history *he);
static int	print_playlist(const char *pfname, int source);
static int	unprint_playlist(const char *pfname);
static int	generalize_playlist(const char *pfname);
static int	generate_playlist(const char *pfname, const char *root);
static int	truncate_playlist(const char *pfname, char *larg);
static int  add_file(struct BC_playlist *pc, const char *fname, const int batch, const bool shared, const bool low_priority);
static int  add_directory(struct BC_playlist *pc, const char *fname, const int batch, const bool shared);
static int  add_fseventsd_files(struct BC_playlist *pc, const int batch, const bool shared);
static int  add_playlist_for_preheated_user(struct BC_playlist *pc, bool isCompositeDisk);
static int  add_logical_playlist(const char *playlist, struct BC_playlist *pc, int batch, int shared);

static int verbose;
static char *myname;

#ifdef DEBUG
static FILE* outstream;
#define DLOG(args...) fprintf(outstream, ##args)
#else
#define DLOG(args...)
#endif

int
main(int argc, char *argv[])
{
	int ch, cflag, batch;
	int* pbatch;
	char *pfname;
	
	myname = argv[0];
	pfname = NULL;
	pbatch = NULL;
	cflag = 0;
	while ((ch = getopt(argc, argv, "b:cvf:t:")) != -1) {
		switch(ch) {
			case 'b':
				usage("blocksize is no longer required, ignoring");
			case 'c':
				cflag++;
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
	if (!strcmp(argv[0], "merge"))
		return(merge_playlists(pfname, argc - 1, argv + 1, pbatch));
	if (!strcmp(argv[0], "print"))
		return(print_playlist(pfname, cflag));
	if (!strcmp(argv[0], "unprint"))
		return(unprint_playlist(pfname));
	if (!strcmp(argv[0], "generalize"))
		return(generalize_playlist(pfname));
	if (!strcmp(argv[0], "generate"))
		return(generate_playlist(pfname, argc < 2 ? NULL : argv[1]));
	if (!strcmp(argv[0], "truncate")) {
		if (argc < 2) 
			return(usage("missing truncate length"));
		return(truncate_playlist(pfname, argv[1]));
	}
	
	if (!strcmp(argv[0], "unload")) {
		if (BC_unload()) {
			warnx("could not unload cache");
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
		warnx("%s", reason);
	fprintf(stderr, "Usage: %s [-vvv] [-f <playlistfile>] start\n", myname);
	fprintf(stderr, "           Start recording history and play back <playlistfile>.\n");
	fprintf(stderr, "       %s [-vvv] [-f <playlistfile>] stop\n", myname);
	fprintf(stderr, "           Stop recording history and write the playlist to <playlistfile>.\n");
	fprintf(stderr, "       %s mount\n", myname);
	fprintf(stderr, "           Notify the boot cache of a new mount.\n");
	fprintf(stderr, "       %s [-vvv] jettison\n", myname);
	fprintf(stderr, "           Jettison the cache.\n");
	fprintf(stderr, "       %s statistics\n", myname);
	fprintf(stderr, "           Print statistics for the currently-active cache.\n");
	fprintf(stderr, "       %s tag\n", myname);
	fprintf(stderr, "           Insert the end-prefetch tag.\n");
	fprintf(stderr, "       %s [-vvv] [-t batchnum] -f <playlistfile> merge <playlistfile1> [<playlistfile2>...]\n", myname);
	fprintf(stderr, "           Merge <playlistfile1>... into <playlistfile>.\n");
	fprintf(stderr, "           Playlist files after the first will be offset <batchnum> batches, if provided\n");
	fprintf(stderr, "       %s [-c] -f <playlistfile> print\n", myname);
	fprintf(stderr, "           Print the contents of <playlistfile>.\n");
	fprintf(stderr, "       %s -f <playlistfile> unprint\n", myname);
	fprintf(stderr, "           Read a playlist from standard input and write to <playlistfile>.\n");
	fprintf(stderr, "       %s -f <playlistfile> generate [<volume>]\n", myname);
	fprintf(stderr, "           Generate a playlist from standard input data for <volume> and write to <playlistfile>.\n");
	fprintf(stderr, "       %s -f <playlistfile> truncate <count>\n", myname);
	fprintf(stderr, "           Truncate <playlistfile> to <count> entries.\n");
	fprintf(stderr, "       %s -f <playlistfile> generalize\n", myname);
	fprintf(stderr, "           Modify <playlistfile> to apply to any root volume.\n");
	fflush(stderr);
	return(1);
}

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

static int add_logical_playlist(const char *playlist, struct BC_playlist *pc, int batch, int shared)
{
    char filename[MAXPATHLEN];
	
    FILE *lpl = fopen(playlist, "r");
    if (lpl == NULL){
        if (errno != ENOENT && errno != EINVAL) {
            warnx("Could not read playlist %s: %s", playlist, strerror(errno));
        }
        return -1;
    }
    
    while (fgets(filename, sizeof filename, lpl)){
        int len = strlen(filename);
        if (len < 3)
            continue;
        
        if (filename[len - 1] == '\n')
            filename[len - 1] = '\0';
        
        if (filename[len - 2] == '/'){
            filename[len - 2] = '\0';
            int error;
            if ((error = add_directory(pc, filename, batch, shared)) 
                && error != ENOENT && error != EINVAL)
                warnx("Error adding files in directory %s: %s", filename, strerror(errno));
        } else {
            add_file(pc, filename, batch, shared, false);
        }
    }
    fclose(lpl);
    
    return 0;
}

/*
 * Return whether the root disk is a composite disk.
 */
static bool
isRootCPDK()
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
		// No bsd device? Assume not composite
		return false;
	}
	
	//*************************************************************//
	// Get the CoreStorage Logical Volume UUID from the bsd device //
	//*************************************************************//
	
	CFStringRef lvUUID = NULL;
	
	mach_port_t masterPort;
	if (KERN_SUCCESS == IOMasterPort(bootstrap_port, &masterPort)) {
		
		io_registry_entry_t diskObj = IOServiceGetMatchingService(masterPort, IOBSDNameMatching(masterPort, 0, bsdDevPath));
		if (IO_OBJECT_NULL != diskObj) {
			
			if (IOObjectConformsTo(diskObj, kCoreStorageLogicalClassName)) {
				
				CFTypeRef cfRef = IORegistryEntryCreateCFProperty(diskObj, CFSTR(kIOMediaUUIDKey), kCFAllocatorDefault, 0);
				if (NULL != cfRef) {
					
					if (CFGetTypeID(cfRef) == CFStringGetTypeID()) {
						
						lvUUID = CFStringCreateCopy(NULL, cfRef);
						DLOG("CoreStorage Logical Volume UUID is %p %s\n", lvUUID, CFStringGetCStringPtr(lvUUID, 0));

					}
					
					CFRelease(cfRef);
				}
				
			} else {
				//Common case for non-CoreStorage filesystems
			}
			
			IOObjectRelease(diskObj);
		}
		
	}
	
	if (NULL == lvUUID) {
		// Not composite if it's not CoreStorage
		return false;
	}
	
	//****************************************************************************************//
	// Get the CoreStorage Logical Volume Group UUID from the CoreStorage Logical Volume UUID //
	//****************************************************************************************//	
	
	CFStringRef lvgUUID = NULL;
	
	CFMutableDictionaryRef propLV = CoreStorageCopyVolumeProperties ((CoreStorageLogicalRef)lvUUID);
	CFRelease(lvUUID);
	if (NULL != propLV) {
		
		lvgUUID = CFDictionaryGetValue(propLV, CFSTR(kCoreStorageLogicalGroupUUIDKey));
		if (NULL != lvgUUID) {

			lvgUUID = CFStringCreateCopy(NULL, lvgUUID);
			DLOG("CoreStorage Logical Volume Group UUID is %p %s\n", lvgUUID, CFStringGetCStringPtr(lvgUUID, 0));

		}
		
		CFRelease(propLV);
	}
	
	if (NULL == lvgUUID) {
		// Can't get the group? Assume not composite
		return false;
	}
	
	//**************************************************************************************************//
	// Check if the Core Storage Group Type of the CoreStorage Logical Volume Group is a Composite Disk //
	//**************************************************************************************************//	
	
	bool isCompositeDisk = false;
	
	CFMutableDictionaryRef lvgProperties = CoreStorageCopyLVGProperties ((CoreStorageGroupRef)lvgUUID);
	CFRelease(lvgUUID);
	if (NULL != lvgProperties) {
		
		CFStringRef groupType = CFDictionaryGetValue(lvgProperties, CFSTR(kCoreStorageGroupTypeKey));
		if (NULL != groupType) {
			
			isCompositeDisk = (kCFCompareEqualTo == CFStringCompare(groupType, CFSTR(kCoreStorageGroupTypeCPDK), 0x0));
			
		}
		
		CFRelease(lvgProperties);
	}
	
	if (isCompositeDisk) {
		DLOG("is cpdk\n");
	} else {
		DLOG("is not cpdk\n");
	}
	
	return isCompositeDisk;
}

/*
 * Kick off the boot cache.
 */
static int
do_boot_cache()
{
#ifdef DEBUG
	outstream = fopen("/var/log/BootCacheControl.log", "a");
#endif
	
	struct BC_playlist *pc;
	int error;
	const char* pfname = BC_ROOT_PLAYLIST;
	
	if ((error = BC_test()) != 0) {
		return error;
	}
	
	bool isCompositeDisk = isRootCPDK();
	
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
		warnx("could not read playlist %s: %s", pfname, strerror(error));
		return error;
	}
	
	int last_shutdown_was_clean = 0;
	size_t wasCleanSz = sizeof(wasCleanSz);
	error = sysctlbyname("vfs.generic.root_unmounted_cleanly", &last_shutdown_was_clean, &wasCleanSz, NULL, 0);
	if (error != 0) {
		warnx("Unable to check for hard shutdown");
	}
	
	if (pc) {
		
		/*
		 * Remove the consumed playlist: it'll be replaced once BootCache 
		 * completes successfully.
		 */
		
		//char prev_path[MAXPATHLEN];
		//snprintf(prev_path, sizeof(prev_path), "%s.previous", pfname);
		//error = rename(pfname, prev_path); // for debugging
		error = unlink(pfname);
		if (error != 0 && errno != ENOENT)
			errx(1, "could not unlink bootcache playlist %s: %d %s", pfname, errno, strerror(errno));
		
		if (! last_shutdown_was_clean) {
			struct timeval start_time = {0,0}, end_time = {0,0};
			(void)gettimeofday(&start_time, NULL); // can't fail
			
			// Make sure the unlink hits disk in case we panic rdar://8947415
			int fd = open("/var/db/", O_RDONLY);
			if (fd != -1) {
				if (-1 != fcntl(fd, F_FULLFSYNC)) {
					fprintf(stderr, "Synced /var/db/\n");
				} else {
					warnx("Unable to sync /var/db/: %d %s", errno, strerror(errno));
				}
				close(fd);
			} else {
				warnx("Unable to open /var/db/: %d %s", errno, strerror(errno));
			}
			
			(void)gettimeofday(&end_time, NULL); // can't fail
			timersub(&end_time, &start_time, &end_time);
			// warnx("Took %dus to sync /var/db/\n", end_time.tv_usec);
		}
		
		if (! isCompositeDisk) {
			// rdar://9424845 Add any files we know are read in every boot, but change every boot
			add_fseventsd_files(pc, 0, false);
			add_logical_playlist(BC_ROOT_EXTRA_LOGICAL_PLAYLIST, pc, 0, false);
			add_logical_playlist(BC_LOGIN_EXTRA_LOGICAL_PLAYLIST, pc, 2, false);
		}
	} else {
		if (last_shutdown_was_clean) {
			pc = calloc(1, sizeof(*pc));
			
			// No Root playlist: we're during an install so use our premade logical playlists
			
			if (! isCompositeDisk) {
				add_logical_playlist(BC_ROOT_LOGICAL_PLAYLIST, pc, 0, false);
				add_logical_playlist(BC_LOGIN_LOGICAL_PLAYLIST, pc, 2, false);
			}
		}
	}
	
	// If we don't have a playlist here, we don't want to play back anything
	if (pc) {
		// Add the login playlist of user we expect to log in to the boot playlist
		if (0 != add_playlist_for_preheated_user(pc, isCompositeDisk)) {
			// Unable to add user playlist, add 32-bit shared cache to low-priority playlist
			add_file(pc, BC_DYLD_SHARED_CACHE_32, 0, true, true);
			warnx("Added 32-bit shared cache to the low priority batch");
		}
		
		// rdar://9021675 Always warm the shared cache
		// Try subtypes we know about first. <rdar://problem/16093388>
		if (0 != add_file(pc, BC_DYLD_SHARED_CACHE_H, -1, true, false)) {
			add_file(pc, BC_DYLD_SHARED_CACHE, -1, true, false);
		}
	}

	error = BC_start(pc);
	if (error != 0)
		warnx("could not start cache: %d %s", error, strerror(error));
	
	PC_FREE_ZERO(pc);
	return(error);
}

/*
 * Fetch or create a login playlist for the given user and add it to the provided playlist in subsequent batches
 */
static int
add_playlist_for_preheated_user(struct BC_playlist *pc, bool isCompositeDisk) {
	
	if (!pc) return 1;
	
	// These defines match those in warmd
#define PREHEATED_USER_PLAYLIST_PATH_FMT     "/var/db/BootCaches/%s/"
#define MERGED_PLAYLIST                      "Merged.playlist"
#define LOGIN_PLAYLIST                       "Login.playlist"
#define DEFAULT_USER                         "PreheatedUser"
#define APP_CACHE_PLAYLIST_PREFIX            "app."
#define PLAYLIST_SUFFIX                      ".playlist"
#define RUNNING_XATTR_NAME                   "BC_RUNNING"
#define I386_XATTR_NAME                      "BC_I386"
#define MAX_PLAYLIST_PATH_LENGTH             256
#define TAL_LOGIN_FOREGROUND_APP_BATCH_NUM   0
#define TAL_LOGIN_BACKGROUND_APP_BATCH_NUM   (TAL_LOGIN_FOREGROUND_APP_BATCH_NUM + 1)
#define TAL_LOGIN_LOWPRI_DATA_BATCH_NUM      (TAL_LOGIN_BACKGROUND_APP_BATCH_NUM + 1)
#define I386_SHARED_CACHE_NULL_BATCH_NUM         (-2)
#define I386_SHARED_CACHE_LOW_PRIORITY_BATCH_NUM (-1)
	
	int error, i;
	int playlist_path_end_idx = 0;
	char playlist_path[MAX_PLAYLIST_PATH_LENGTH];
	char login_user[128] = DEFAULT_USER;
	struct BC_playlist* user_playlist = NULL;
	bool already_added_i386_shared_cache = false;
	
	// Limit the playlist size
	ssize_t playlist_size = 0;
	ssize_t max_playlist_size = 0;
	size_t len = sizeof(max_playlist_size);
	error = sysctlbyname("hw.memsize", &max_playlist_size, &len, NULL, 0);
	if(error != 0) {
		max_playlist_size = (1024 * 1024 * 1024);
		warnx("sysctlbyname(\"hw.memsize\") failed => %d (%d). Assuming 1GB", error, errno);
	}
	max_playlist_size = (max_playlist_size / 2) - (512 * 1024 * 1024);
	if (max_playlist_size < (512 * 1024 * 1024)) {
		max_playlist_size = (512 * 1024 * 1024);
	}
	
	for (i = 0; i < pc->p_nentries; i++) {
		playlist_size += pc->p_entries[i].pe_length;
	}
	
	if (playlist_size >= max_playlist_size) goto out;
	
	//rdar://8830944&9209576 Detect FDE user
	io_registry_entry_t service = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/chosen");
	if (service != MACH_PORT_NULL) {
		
		CFDataRef fde_login_user = IORegistryEntryCreateCFProperty(service, CFSTR(kCSFDEEFILoginUnlockIdentID),
																   kCFAllocatorDefault, kNilOptions);
		IOObjectRelease(service);
		if (fde_login_user != NULL) {
			CFDataGetBytes(fde_login_user, CFRangeMake(0, sizeof(login_user)), (UInt8*)login_user);
			CFRelease(fde_login_user);
		}
	}
	
	playlist_path_end_idx = snprintf(playlist_path, sizeof(playlist_path), PREHEATED_USER_PLAYLIST_PATH_FMT, login_user);
	
	// warnx("Reading user playlists from user dir %s", playlist_path);
	
	// Check for the merged playlist warmd has left
	strlcpy(playlist_path + playlist_path_end_idx, MERGED_PLAYLIST, sizeof(playlist_path) - playlist_path_end_idx);
	error = BC_read_playlist(playlist_path, &user_playlist);
	
	if (error == 0) {
		int i386_shared_cache_batch_num = I386_SHARED_CACHE_NULL_BATCH_NUM;
		if (-1 != getxattr(playlist_path, I386_XATTR_NAME, &i386_shared_cache_batch_num, sizeof(i386_shared_cache_batch_num), 0, 0x0)) {
			if (i386_shared_cache_batch_num != I386_SHARED_CACHE_NULL_BATCH_NUM) {
				if (i386_shared_cache_batch_num == I386_SHARED_CACHE_LOW_PRIORITY_BATCH_NUM) {
					add_file(user_playlist, BC_DYLD_SHARED_CACHE_32, 0, true, true);
				} else {
					add_file(user_playlist, BC_DYLD_SHARED_CACHE_32, i386_shared_cache_batch_num, true, false);
				}
				// warnx("Added 32-bit shared cache to batch %d", i386_shared_cache_batch_num);
				
				// already_added_i386_shared_cache = true; dead store...
			}
		}
		
	} else if (ENOENT == error) {
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
					if (isCompositeDisk) {
						//rdar://11294417 All user data is low priority on composite disks
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
										playlist_size += app_playlist->p_entries[i].pe_length;
									}
									
									// Make sure we don't oversize the playlist
									if (playlist_size > max_playlist_size) {
										// warnx("Login cache maximum size of %ldMB reached", max_playlist_size / (1024 * 0124));
										PC_FREE_ZERO(app_playlist);
										break;
									}
									
									if (0 != (error = BC_merge_playlists(user_playlist, app_playlist))) {
										PC_FREE_ZERO(app_playlist);
										goto out;
									}
									
									if (!already_added_i386_shared_cache) {
										if (-1 != getxattr(playlist_path, I386_XATTR_NAME, NULL, 0, 0, 0x0)) {
											add_file(user_playlist, BC_DYLD_SHARED_CACHE_32, batch_offset, true, flags & BC_PE_LOWPRIORITY);
											already_added_i386_shared_cache = true;
										}
									}
									
									PC_FREE_ZERO(app_playlist);
									// warnx("Added playlist for app %s", playlist_path);
								}
								
							}
						}					
					} else {
						playlist_path[playlist_path_end_idx] = '\0';
						// warnx("Unknown file in user playlist directory %s: %s", playlist_path, direntry->d_name);
					}
				}
				
				if (playlist_size > max_playlist_size) {
					break;
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
	
	if (! isCompositeDisk) {
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
	
	error = BC_merge_playlists(pc, user_playlist);
	if (error != 0) {
		if (pc->p_entries) {
			free(pc->p_entries);
		}
		if (pc->p_mounts) {
			free(pc->p_mounts);
		}
		memset(pc, 0, sizeof(*pc));
	}
	
out:
	PC_FREE_ZERO(user_playlist);
	return (error);
}

/*
 * Add contents of the file to the bootcache
 */
static int
add_file(struct BC_playlist *pc, const char *fname, const int batch, const bool shared, const bool low_priority)
{
#define MAX_FILESIZE (10ll * 1024 * 1024)
    int error;
    int fd = open(fname, O_RDONLY);
	if (fd == -1) {
        if (errno != ENOENT && errno != EINVAL) {
            warnx("Unable to open %s to add it to the boot cache: %d %s", fname, errno, strerror(errno));
        }
        return errno;
	}
    
    off_t maxsize = MAX_FILESIZE;
    if (shared) {
        maxsize = 0;
    }
    
    struct BC_playlist *playlist = NULL;
    error = BC_playlist_for_filename(fd, fname, maxsize, &playlist);
    close(fd);
    if (playlist == NULL) {
        if (error != ENOENT && error != EINVAL) {
            warnx("Unable to create playlist for %s: %d %s", fname, error, strerror(error));
        }
        return error;
    }
    if(shared || low_priority){
        int i;
		u_int16_t flags = 0x0;
		if (shared)	      flags |= BC_PE_SHARED;
		if (low_priority) flags |= BC_PE_LOWPRIORITY;
        for (i = 0; i < playlist->p_nentries; i++) {
			playlist->p_entries[i].pe_flags |= flags;
        }
    }
    
    if(batch > 0){
        int i;
        for (i = 0; i < playlist->p_nentries; i++) {
            playlist->p_entries[i].pe_batch += batch;
        }
    } else if (batch < 0) {
        int i;
		int inverse_batch = 0 - batch;
        for (i = 0; i < pc->p_nentries; i++) {
            pc->p_entries[i].pe_batch += inverse_batch;
        }
	}
    
    error = BC_merge_playlists(pc, playlist);
    PC_FREE_ZERO(playlist);
	
	if (error != 0) {
		if (pc->p_entries) {
			free(pc->p_entries);
		}
		if (pc->p_mounts) {
			free(pc->p_mounts);
		}
		memset(pc, 0, sizeof(*pc));
	}
	
    return error;
}

/*
 * Add contents of a directory to the bootcache.  Does not operate recursively.
 */
static int
add_directory(struct BC_playlist *pc, const char *dname, const int batch, const bool shared){
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
        
        add_file(pc, fname, batch, shared, false);
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
add_fseventsd_files(struct BC_playlist *pc, const int batch, const bool shared){
#define FSEVENTSD_DIR     "/.fseventsd"
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
            
            add_file(pc, fname, batch, shared, false);
        }
    }
    
    
    return 0;
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
			warnx("could not read playlist %s: %s", pfname, strerror(error));
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
			warnx("could not unlink playlist %s: %s", pfname, strerror(errno));
		}
	}
	
	error = BC_start(pc);
	if (error != 0)
		warnx("could not start cache: %d %s", error, strerror(error));
	
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
	struct BC_statistics *ss;
	int error;
	
	/*
	 * Notify whoever cares that BootCache has stopped recording.
	 */
	notify_post("com.apple.system.private.bootcache.done");
	
	/*
	 * Stop the cache and fetch the history list.
	 */
	if ((error = BC_stop(&hc)) != 0)
		return error;
	
	if (verbose > 0) {
		print_statistics(NULL);
		if (verbose > 1)
			print_history(hc);
	}
	
	/* write history and stats to debug logs if debugging */
	if (debugging) {
		BC_print_history(BC_BOOT_HISTFILE, hc);
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
		return(0);
	}
	
	/*
	 * Convert the history list to playlist format.
	 */
	if ((error = BC_convert_history(hc, &pc)) != 0) {
		if (!debugging) {
			BC_print_history(BC_BOOT_HISTFILE, hc);
		}
		HC_FREE_ZERO(hc);
		errx(1, "history to playlist conversion failed: %d %s", error, strerror(error));
	}
	
	/*
	 * Sort the playlist into block order and coalesce into the smallest set
	 * of read operations.
	 */
	//TODO: Check for SSD and don't sort (and fix coalesceing to work with unsorted lists)
#ifdef BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET
	BC_sort_playlist(pc);
#endif
	BC_coalesce_playlist(pc);
	
	
#if 0 	
	/* 
	 * Turning off playlist merging: this tends to bloat the cache and 
	 * gradually slow the boot until the hitrate drops far enough to force
	 * a clean slate. Previous attempts to outsmart the system, such as
	 * truncating cache growth at 5% per boot, have led to fragile behavior
	 * when the boot sequence changes. Out of a variety of strategies 
	 * (merging, intersection, voting), a memoryless cache gets 
	 * close-to-optimal performance and recovers most quickly from any 
	 * strangeness at boot time.
	 */
	int onentries;
	struct BC_playlist_entry *opc;
	
	/*
	 * In order to ensure best possible coverage, we try to merge the
	 * existing playlist with our new history (provided that the hit
	 * rate has been good).
	 *
	 * We check the number of initiated reads from our statistics against
	 * the number of entries in the "old" playlist to ensure that the
	 * filename we've been given matches the playlist that was just run.
	 *
	 * XXX we hardcode the "good" threshold here to 85%, should be tunable
	 */
	opc = NULL;
	if (ss == NULL) {
		warnx("no statistics, not merging playlists");
		goto nomerge;	/* no stats, can't make sane decision */
	}
	if (ss->ss_total_extents < 1) {
		warnx("no playlist in kernel, not merging");
		goto nomerge;
	}
	if (BC_read_playlist(pfname, &opc, &onentries) != 0) {
		warnx("no old playlist '%s', not merging", pfname);
		goto nomerge;	/* can't read old playlist, nothing to merge */
	}
	if (onentries != ss->ss_total_extents) {
		warnx("old playlist does not match in-kernel playlist, not merging");
		goto nomerge;	/* old playlist doesn't match in-kernel playlist */
	}
	if (((nentries * 100) / onentries) < 105) {
		warnx("new playlist not much bigger than old playlist, not merging");
		goto nomerge;	/* new playlist has < 5% fewer extents */
	}
	if (((ss->ss_spurious_blocks * 100) / (ss->ss_read_blocks + 1)) > 10) {
		warnx("old playlist has excess block wastage, not merging");
		goto nomerge;	/* old playlist has > 10% block wastage */
	}
	if (((ss->ss_hit_blocks * 100) / (ss->ss_requested_blocks + 1)) < 85) {
		warnx("old playlist has poor hitrate, not merging");
		goto nomerge;	/* old playlist had < 85% hitrate */
	}
	BC_merge_playlists(&pc, &nentries, opc, onentries);
	
nomerge:
	PC_FREE_ZERO(opc);
#endif /* 0 */
	
	/*
	 * Securely overwrite the previous playlist.
	 */
	if ((error = BC_write_playlist(pfname, pc)) != 0) {
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
	
	if (pfname == NULL)
		errx(1, "must specify a playlist file to merge");
	
	pc = calloc(1, sizeof(*pc));
	if(!pc) errx(1, "could not allocate initial playlist");
	
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
		if (pbatch && i > 0)
			for (j = 0; j < npc->p_nentries; j++)
				npc->p_entries[j].pe_batch += *pbatch;
		
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
 * Print history entries.
 */
static void
print_history(struct BC_history *hc)
{
	int i;
	for (i = 0; i < hc->h_nentries; i++) {
		printf("%s %-12llu %-8llu %5u%s%s\n",
			   uuid_string(hc->h_mounts[hc->h_entries[i].he_mount_idx].hm_uuid),
			   hc->h_entries[i].he_offset, hc->h_entries[i].he_length,
			   hc->h_entries[i].he_pid,
			   hc->h_entries[i].he_flags & BC_HE_HIT    ? " hit"    :
			   hc->h_entries[i].he_flags & BC_HE_WRITE  ? " write"  :
			   hc->h_entries[i].he_flags & BC_HE_TAG    ? " tag"    : " miss",
			   hc->h_entries[i].he_flags & BC_HE_SHARED ? " shared" : "");
	}
}

/*
 * Print a playlist from a file.
 */
static int
print_playlist(const char *pfname, int source)
{
	struct BC_playlist *pc;
	struct BC_playlist_mount *pm;
	struct BC_playlist_entry *pe;
	int i;
	u_int64_t size = 0, size_lowpri = 0, size_batch[BC_MAXBATCHES] = {0};
	
	if (pfname == NULL)
		errx(1, "must specify a playlist file to print");
	
	/*
	 * Suck in the playlist.
	 */
	if (BC_read_playlist(pfname, &pc))
		errx(1, "could not read playlist");
	
	if (source) {
		printf("static struct BC_playlist_mount BC_mounts[] = {\n");
		for (i = 0; i < pc->p_nmounts; i++) {
			pm = pc->p_mounts + i;
			printf("    {\"%s\", 0x%x}%s\n",
				   uuid_string(pm->pm_uuid), pm->pm_nentries,
				   (i < (pc->p_nmounts - 1)) ? "," : "");
		}
		printf("};\n");
		printf("static struct BC_playlist_entry BC_entries[] = {\n");
	} else {
		for (i = 0; i < pc->p_nmounts; i++) {
			pm = pc->p_mounts + i;
			printf("Mount %s %5d entries\n",
				   uuid_string(pm->pm_uuid), pm->pm_nentries);
		}
	}
	
	/*
	 * Print entries in source or "human-readable" format.
	 */
	for (i = 0; i < pc->p_nentries; i++) {
		pe = pc->p_entries + i;
		if (source) {
			printf("    {0x%llx, 0x%llx, 0x%x, 0x%x, 0x%x}%s\n",
				   pe->pe_offset, pe->pe_length, pe->pe_batch, pe->pe_flags, pe->pe_mount_idx,
				   (i < (pc->p_nentries - 1)) ? "," : "");
		} else {
			printf("%s %-12llu %-8llu %d 0x%x\n",
				   uuid_string(pc->p_mounts[pe->pe_mount_idx].pm_uuid), pe->pe_offset, pe->pe_length, pe->pe_batch, pe->pe_flags);
		}
		if (pe->pe_flags & BC_PE_LOWPRIORITY) {
			size_lowpri += pe->pe_length;
		} else {
			size += pe->pe_length;
			size_batch[pe->pe_batch] += pe->pe_length;
		}
	}
	if (source) {
		printf("};\n");
	} else {
		printf("%12llu bytes\n", size);
		printf("%12llu low-priority bytes\n", size_lowpri);
		for (i = 0; i < BC_MAXBATCHES; i++) {
			if (size_batch[i] != 0) {
				printf("%12llu bytes batch %d\n", size_batch[i], i);
			}
		}
	}
	
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
		uuid_t   uuid;
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
	uuid_t uuid;
	int seen_old_style;
	int m_nentries;
	uint64_t unused;
	char buf[128];
	
	pc = calloc(1, sizeof(*pc));
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
		got = sscanf(buf, "Mount %s %u entries ",
					 uuid_string, &m_nentries);
		
		if (got == 2) {
			/* make sure we have space for the next mount */
			if ((pc->p_mounts = realloc(pc->p_mounts, (pc->p_nmounts + 1) * sizeof(*pc->p_mounts))) == NULL)
				errx(1, "could not allocate memory for %d mounts", pc->p_nmounts + 1);
			
			pc->p_mounts[pc->p_nmounts].pm_nentries  = m_nentries;
			uuid_parse(uuid_string, pc->p_mounts[pc->p_nmounts].pm_uuid);
			
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
		got = sscanf(buf, "%s %lli %lli %hi %hi",
					 uuid_string,
					 &(pc->p_entries + nentries)->pe_offset,
					 &(pc->p_entries + nentries)->pe_length,
					 &(pc->p_entries + nentries)->pe_batch,
					 &(pc->p_entries + nentries)->pe_flags);
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
				warnx("No mount UUID provided, assuuming root volume");
				pc->p_nmounts = 1;
				if ((pc->p_mounts = malloc(sizeof(*pc->p_mounts))) == NULL)
					errx(1, "could not allocate memory for %d mounts", pc->p_nmounts);
				pc->p_mounts[0].pm_nentries = 0;
				get_volume_uuid("/", pc->p_mounts[0].pm_uuid);
				seen_old_style = 1;
			}
			
			(pc->p_entries + nentries)->pe_mount_idx = 0;
			(pc->p_entries + nentries)->pe_flags = 0;
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
 * Read a canned playlist specification from stdin and generate
 * a sorted playlist from it, as applied to the given root volume
 * (which defaults to "/"). This is used during the OS Install to
 * seed a bootcache for first boot from a list of files + offsets.
 */
static int
generate_playlist(const char *pfname, const char *root)
{
	struct BC_playlist *pc;
	int nentries, alloced;
    long block_size;
	
	if (pfname == NULL)
		errx(1, "must specify a playlist file to create");
	
	if (root == NULL)
		root = "/";
	if (strlen(root) >= 512)
		errx(1, "root path must be less than 512 characters");
	
	pc = malloc(sizeof(*pc));
	
	/* Setup the root volume as the only mount */
	pc->p_nmounts = 1;
	if ((pc->p_mounts = malloc(sizeof(*pc->p_mounts))) == NULL)
		errx(1, "could not allocate memory for %d mounts", pc->p_nmounts);
	pc->p_mounts[0].pm_nentries = 0;
    
	struct statfs statfs_buf;
    if (0 != statfs(root, &statfs_buf)) {
		warnx("Unable to stafs %s: %d %s", root, errno, strerror(errno));
        // Assume 512-byte block size
        block_size = 512;
	} else {
        block_size = statfs_buf.f_bsize;
	}
	
	get_volume_uuid(root, pc->p_mounts[0].pm_uuid);
	
	/* Fill in the entries */
	nentries = 0;
	alloced = 4096; /* we know the rough size of the list */
	pc->p_entries = malloc(alloced * sizeof(*pc->p_entries));
	if(!pc->p_entries) errx(1, "could not allocate initial playlist");	
	for (;;) { /* go over each line */
		
		/* read input */
		int64_t offset, count;
		int batch;
		if(fscanf(stdin, "%lli %lli %i ", &offset, &count, &batch) < 3) break;
		
		/* build the path */
		char path[2048];
		unsigned int path_offset = (unsigned int) strlen(root);
		strlcpy(path, root, sizeof(path));
		while(path_offset < 2048) {
			int read = fgetc(stdin);
			if(read == EOF || read == '\n') {
				path[path_offset] = '\0';
				break;
			} else {
				path[path_offset++] = (char) read;
			}
		}
		if(path_offset == 2048) continue;
		
		/* open and stat the file */
		struct stat sb;
		int fd = open(path, O_RDONLY);
		if(fd == -1 || fstat(fd, &sb) < 0) {
			/* give up on this line */
			if(fd != -1) close(fd);
			continue;
		}
        
        // Round up to the block size
        sb.st_size = (((sb.st_size + (block_size - 1)) / block_size) * block_size);
		
		/* add metadata blocks for file */
		/*	TODO:
		 as a further enhancement, since we know we're going to access this file
		 by name, it would make sense to add to the bootcache any blocks that
		 will be used in doing so, including the directory entries of the parent
		 directories on up the chain.
		 */
		
		/* find blocks in the file */
		off_t position;
		for(position = offset; position < offset + count && position < sb.st_size; ) {
			
			off_t remaining = (count - (position - offset));
			struct log2phys l2p = {
				.l2p_flags       = 0,
				.l2p_devoffset   = position,   //As an IN parameter to F_LOG2PHYS_EXT, this is the offset into the file
				.l2p_contigbytes = remaining, //As an IN parameter to F_LOG2PHYS_EXT, this is the number of bytes to be queried
			};
			
			int ret = fcntl(fd, F_LOG2PHYS_EXT, &l2p);
			if (ret != 0) {
				errx(1, "fcntl(%d (%s), F_LOG2PHYS_EXT, &{.offset: %lld, .bytes: %lld}) => %d (errno: %d %s)",
					 fd, path, l2p.l2p_devoffset, l2p.l2p_contigbytes, ret, errno, strerror(errno));
				break;
			}
			
			// l2p.l2p_devoffset;   as an OUT parameter from F_LOG2PHYS_EXT, this is the offset in bytes on the disk
			// l2p.l2p_contigbytes; as an OUT parameter from F_LOG2PHYS_EXT, this is the number of bytes in the range
			
			position += l2p.l2p_contigbytes;
			
			if (remaining < l2p.l2p_contigbytes ) {
				warnx("Invalid size returned for %d from disk (%lld bytes requested, %lld bytes returned)", fd, remaining, l2p.l2p_contigbytes);
				break;
			}
			
			if (l2p.l2p_contigbytes <= 0) {
				//RLOG(INFO, "%"PRIdoff":%"PRIdoff" returned %"PRIdoff":%"PRIdoff"\n", position, remaining, l2p.l2p_devoffset, l2p.l2p_contigbytes);
				break;
			}
			
			if (l2p.l2p_devoffset + l2p.l2p_contigbytes <= l2p.l2p_devoffset) {
				warnx("Invalid block range return for %d from disk (%lld:%lld returned %lld:%lld)\n", fd, position, remaining, l2p.l2p_devoffset, l2p.l2p_contigbytes);
				break;
			}
			
			/* create the new entry */
			pc->p_entries[nentries].pe_offset = l2p.l2p_devoffset;
			pc->p_entries[nentries].pe_length = l2p.l2p_contigbytes;
			pc->p_entries[nentries].pe_batch = batch;
			pc->p_entries[nentries].pe_flags = 0;
			pc->p_entries[nentries].pe_mount_idx = 0;
			
			/* create space for a new entry */
			if(++nentries >= alloced) {
				alloced *= 2;
				if((pc->p_entries = realloc(pc->p_entries, alloced * sizeof(*pc->p_entries))) == NULL) {
					errx(1, "could not allocate memory for %d entries", alloced);
				}
			}
			
		}
		close(fd);
	}
	if(nentries == 0)
		errx(1, "no blocks found for playlist");
	
	pc->p_nentries = nentries;
	pc->p_mounts[0].pm_nentries = nentries;
	
	if (BC_verify_playlist(pc)) {
		errx(1, "Playlist failed verification");
	}	
	
	/* sort the playlist */
#ifdef BOOTCACHE_ENTRIES_SORTED_BY_DISK_OFFSET
	BC_sort_playlist(pc);
#endif
	BC_coalesce_playlist(pc);
	
	/* write the playlist */
	if (BC_write_playlist(pfname, pc) != 0)
		errx(1, "could not create playlist");
	
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
		warnx("playlist is shorter than specified truncate length");
	} else {
		
		for (i = length; i < pc->p_nentries; i++) {
			pc->p_mounts[pc->p_entries[i].pe_mount_idx].pm_nentries--;
		}
		for (i = 0; i < pc->p_nmounts; i++) {
			if (pc->p_mounts[i].pm_nentries == 0) {
				memcpy(pc->p_mounts + i, pc->p_mounts + i + 1, pc->p_nmounts - i - 1);
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

