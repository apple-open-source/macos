/*
 * Copyright (c) 2006-2007, 2010 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */             
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <pthread.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/kauth.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/time.h>

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>

#include <bsm/libbsm.h>

#include <asl.h>
#include <membership.h>
#include <launch.h>
#include <dirhelper_priv.h>

#include "dirhelper.h"
#include "dirhelper_server.h"
/*
 * Uncomment the next line to define BOOTDEBUG, which will write timing
 * info for clean_directories() to a file, /Debug.
 */
//#define BOOTDEBUG
#ifdef BOOTDEBUG
#include <mach/mach_time.h>
#endif //BOOTDEBUG

// globals for idle exit
struct idle_globals {
	mach_port_t	mp;
	long		timeout;
	struct timeval	lastmsg;
};

struct idle_globals idle_globals;

// argument structure for clean_thread
struct clean_args {
	const char** dirs;
	int machineBoot;
};

void* idle_thread(void* param __attribute__((unused)));

int file_check(const char* path, int mode, int uid, int gid, uid_t* owner, gid_t* group);
#define is_file(x) file_check((x), S_IFREG, -1, -1, NULL, NULL)
#define is_directory(x) file_check((x), S_IFDIR, -1, -1, NULL, NULL)
#define is_directory_get_owner_group(x,o,g) file_check((x), S_IFDIR, -1, -1, (o), (g))
#define is_root_wheel_directory(x) file_check((x), S_IFDIR, 0, 0, NULL, NULL)

int is_safeboot(void);

void clean_files_older_than(const char* path, time_t when);
void clean_directories(const char* names[], int);

kern_return_t
do___dirhelper_create_user_local(
	mach_port_t server_port __attribute__((unused)),
	audit_token_t au_tok)
{
	int res = 0;
	uid_t euid;
	gid_t gid = 0;
	struct passwd* pwd = NULL;

	gettimeofday(&idle_globals.lastmsg, NULL);

	audit_token_to_au32(au_tok,
			NULL, // audit uid
			&euid, // euid
			NULL, // egid
			NULL, // ruid
			NULL, // rgid
			NULL, // remote_pid
			NULL, // asid
			NULL); // aud_tid_t

	// Look-up the primary gid of the user.  We'll use this for chown(2)
	// so that the created directory is owned by a group that the user
	// belongs to, avoiding warnings if files are moved outside this dir.
	pwd = getpwuid(euid);
	if (pwd) gid = pwd->pw_gid;

	do { // begin block
		char path[PATH_MAX];
		char *next;

		if (__user_local_dirname(euid, DIRHELPER_USER_LOCAL, path, sizeof(path)) == NULL) {
			asl_log(NULL, NULL, ASL_LEVEL_ERR,
				"__user_local_dirname: %s", strerror(errno));
			break;
		}

		// All dirhelper directories are now at the same level, so
		// we need to remove the DIRHELPER_TOP_STR suffix to get the
		// parent directory.
		path[strlen(path) - (sizeof(DIRHELPER_TOP_STR) - 1)] = 0;
		
		//
		// 1. Starting with VAR_FOLDERS_PATH, make each subdirectory
		//    in path, ignoring failure if it already exists.
		// 2. Change ownership of directory to the user.
		//
		next = path + strlen(VAR_FOLDERS_PATH);
		while ((next = strchr(next, '/')) != NULL) {
			*next = 0; // temporarily truncate
			res = mkdir(path, 0755);
			if (res != 0 && errno != EEXIST) {
				asl_log(NULL, NULL, ASL_LEVEL_ERR,
					"mkdir(%s): %s", path, strerror(errno));
				break;
			}
			*next++ = '/'; // restore the slash and increment
		}
		if(next || res) // an error occurred
			break;
		res = chown(path, euid, gid);
		if (res != 0) {
			asl_log(NULL, NULL, ASL_LEVEL_ERR,
			"chown(%s): %s", path, strerror(errno));
		}
	} while(0); // end block
	return KERN_SUCCESS;
}

kern_return_t
do___dirhelper_idle_exit(
	mach_port_t server_port __attribute__((unused)),
	audit_token_t au_tok __attribute__((unused))) {

	struct timeval now;
	gettimeofday(&now, NULL);
	long delta = now.tv_sec - idle_globals.lastmsg.tv_sec;
	if (delta >= idle_globals.timeout) {
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG,
			"idle exit after %ld seconds", delta);
		exit(EXIT_SUCCESS);
	}

	return KERN_SUCCESS;
}

void*
idle_thread(void* param __attribute__((unused))) {
	for(;;) {
		struct timeval now;
		gettimeofday(&now, NULL);
		long delta = (now.tv_sec - idle_globals.lastmsg.tv_sec);
		if (delta < idle_globals.timeout) {
			// sleep for remainder of timeout	
			sleep(idle_globals.timeout - delta);
		} else {
			// timeout has elapsed, attempt to idle exit
			__dirhelper_idle_exit(idle_globals.mp);
		}
	}
	return NULL;
}

// If when == 0, all files are removed.  Otherwise, only regular files that were both created _and_ last modified before `when`.
void
clean_files_older_than(const char* path, time_t when) {
	FTS* fts;
	
	char* path_argv[] = { (char*)path, NULL };
	fts = fts_open(path_argv, FTS_PHYSICAL | FTS_XDEV, NULL);
	if (fts) {
		FTSENT* ent;
		asl_log(NULL, NULL, ASL_LEVEL_INFO, "Cleaning " VAR_FOLDERS_PATH "%s", path);
		while ((ent = fts_read(fts))) {
			switch(ent->fts_info) {
				case FTS_F:
				case FTS_DEFAULT:
					if (when == 0) {
#if DEBUG
						asl_log(NULL, NULL, ASL_LEVEL_ALERT, "unlink(" VAR_FOLDERS_PATH "%s)", ent->fts_path);
#endif
						(void)unlink(ent->fts_path);
					} else if (S_ISREG(ent->fts_statp->st_mode) && (ent->fts_statp->st_birthtime < when) && (ent->fts_statp->st_atime < when)) {
						int fd = open(ent->fts_path, O_RDONLY | O_NONBLOCK);
						if (fd != -1) {
							// Obtain an exclusive lock so
        					// that we can avoid a race with other processes
        					// attempting to open or modify the file.
							int res = flock(fd, LOCK_EX | LOCK_NB);
							if (res == 0) {
								struct stat sb;
								res = fstat(fd, &sb);
								if ((res == 0) && (sb.st_birthtime < when) && (sb.st_atime < when)) {
#if DEBUG
									asl_log(NULL, NULL, ASL_LEVEL_ALERT, "unlink(" VAR_FOLDERS_PATH "%s)", ent->fts_path);
#endif
									(void)unlink(ent->fts_path);
								}
								(void)flock(fd, LOCK_UN);
							}
							close(fd);
						}
					}
					break;
					
				case FTS_SL:
				case FTS_SLNONE:
					if (when == 0) {
#if DEBUG
						asl_log(NULL, NULL, ASL_LEVEL_ALERT, "unlink(" VAR_FOLDERS_PATH "%s)", ent->fts_path);
#endif
						(void)unlink(ent->fts_path);
					}
					break;
					
				case FTS_DP:
					if (when == 0) {
#if DEBUG
						asl_log(NULL, NULL, ASL_LEVEL_ALERT, "rmdir(" VAR_FOLDERS_PATH "%s)", ent->fts_path);
#endif
						(void)rmdir(ent->fts_path);
					}
					break;
					
				case FTS_ERR:
				case FTS_NS:
					asl_log(NULL, NULL, ASL_LEVEL_ERR, VAR_FOLDERS_PATH "%s: %s", ent->fts_path, strerror(ent->fts_errno));
					break;
					
				default:
					break;
			}
		}
		fts_close(fts);
	} else {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, VAR_FOLDERS_PATH "%s: %s", path, strerror(errno));
	}
}

int
file_check(const char* path, int mode, int uid, int gid, uid_t* owner, gid_t* group) {
	int check = 1;
	struct stat sb;
	if (lstat(path, &sb) == 0) {
		check = check && ((sb.st_mode & S_IFMT) == mode);
		check = check && ((sb.st_uid == (uid_t)uid) || uid == -1);
		check = check && ((sb.st_gid == (gid_t)gid) || gid == -1);
		if (check) {
			if (owner) *owner = sb.st_uid;
			if (group) *group = sb.st_gid;
		}
	} else {
		if (errno != ENOENT) {
			/* This will print a shorter path after chroot() */
			asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", path, strerror(errno));
		}
		check = 0;
	}
	return check;
}

int
is_safeboot(void) {
	uint32_t sb = 0;
	size_t sbsz = sizeof(sb);

	if (sysctlbyname("kern.safeboot", &sb, &sbsz, NULL, 0) != 0) {
		return 0;
	} else {
		return (int)sb;
	}
}

void *
clean_thread(void *a) {
	struct clean_args* args = (struct clean_args*)a;
	DIR* d;
	time_t when = 0;
	int i;

	if (!args->machineBoot) {
		struct timeval now;
		long days = 3;
		const char* str = getenv("CLEAN_FILES_OLDER_THAN_DAYS");
		if (str) {
			days = strtol(str, NULL, 0);
		}
		(void)gettimeofday(&now, NULL);
		for (i = 0; args->dirs[i]; i++)
			asl_log(NULL, NULL, ASL_LEVEL_INFO, "Cleaning %s older than %ld days", args->dirs[i], days);

		when = now.tv_sec - (days * 60 * 60 * 24);
	}

	// Look up the boot time
	struct timespec boottime;
	size_t len = sizeof(boottime);
	if (sysctlbyname("kern.boottime", &boottime, &len, NULL, 0) == -1) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", "sysctl kern.boottime", strerror(errno));
		return NULL;
	}

	if (!is_root_wheel_directory(VAR_FOLDERS_PATH)) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", VAR_FOLDERS_PATH, "invalid ownership");
		return NULL;
	}

	if (chroot(VAR_FOLDERS_PATH)) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "chroot(%s) failed: %s",
			VAR_FOLDERS_PATH, strerror(errno));
	}
	chdir("/");
	if ((d = opendir("/"))) {
		struct dirent* e;
		char path[PATH_MAX];

		// /var/folders/*
		while ((e = readdir(d))) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
			
			snprintf(path, sizeof(path), "%s%s", "/", e->d_name);
			if (is_root_wheel_directory(path)) {
				DIR* d2 = opendir(path);
				if (d2) {
					struct dirent* e2;
					
					// /var/folders/*/*
					while ((e2 = readdir(d2))) {
						char dirbuf[PATH_MAX];
						uid_t owner;
						gid_t group;
						if (strcmp(e2->d_name, ".") == 0 || strcmp(e2->d_name, "..") == 0) continue;
						snprintf(dirbuf, sizeof(dirbuf),
							"%s/%s", path, e2->d_name);
						if (!is_directory_get_owner_group(dirbuf, &owner, &group)) continue;
						if (pthread_setugid_np(owner, group) != 0) {
							asl_log(NULL, NULL, ASL_LEVEL_ERR,
								"skipping %s: pthread_setugid_np(%u, %u): %s",
								dirbuf, owner, group, strerror(errno));
							continue;
						}
						for (i = 0; args->dirs[i]; i++) {
							const char *name = args->dirs[i];
							snprintf(dirbuf, sizeof(dirbuf),
								 "%s/%s/%s", path, e2->d_name, name);
							if (is_directory(dirbuf)) {
								// at boot time we clean all files,
								// otherwise only clean regular files.
								clean_files_older_than(dirbuf, when);
							}
						}
						if (pthread_setugid_np(KAUTH_UID_NONE, KAUTH_GID_NONE) != 0) {
							asl_log(NULL, NULL, ASL_LEVEL_ERR,
								"%s: pthread_setugid_np(KAUTH_UID_NONE, KAUTH_GID_NONE): %s",
								dirbuf, strerror(errno));
						}
					}
					closedir(d2);
				} else {
					asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", path, strerror(errno));
				}
			}
		}
		
		closedir(d);
	} else {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", VAR_FOLDERS_PATH, strerror(errno));
	}
	return NULL;
}

void
clean_directories(const char* dirs[], int machineBoot) {
	struct clean_args args;
	pthread_t t;
	int ret;
#ifdef BOOTDEBUG
	double ratio;
	struct mach_timebase_info info;
	uint64_t begin, end;
	FILE *debug;

	mach_timebase_info(&info);
	ratio = (double)info.numer / ((double)info.denom * NSEC_PER_SEC);
	begin = mach_absolute_time();
	if((debug = fopen("/Debug", "a")) != NULL) {
	    fprintf(debug, "clean_directories: machineBoot=%d\n", machineBoot);
	}
#endif //BOOTDEBUG

	args.dirs = dirs;
	args.machineBoot = machineBoot;
	ret = pthread_create(&t, NULL, clean_thread, &args);
	if (ret == 0) {
		ret = pthread_join(t, NULL);
		if (ret) {
			asl_log(NULL, NULL, ASL_LEVEL_ERR, "clean_directories: pthread_join: %s",
				strerror(ret));
		}
	} else {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "clean_directories: pthread_create: %s",
			strerror(ret));
	}
#ifdef BOOTDEBUG
	end = mach_absolute_time();
	if(debug) {
	    fprintf(debug, "clean_directories: %f secs\n", ratio * (end - begin));
	    fclose(debug);
	}
#endif //BOOTDEBUG
}

int
main(int argc, char* argv[]) {
	mach_msg_size_t mxmsgsz = MAX_TRAILER_SIZE;
	kern_return_t kr;
	long idle_timeout = 30; // default 30 second timeout

#ifdef BOOTDEBUG
	{
		FILE *debug;
		int i;
		if((debug = fopen("/Debug", "a")) != NULL) {
		    for(i = 0; i < argc; i++) {
			    fprintf(debug, " %s", argv[i]);
		    }
		    fputc('\n', debug);
		    fclose(debug);
		}
	}
#endif //BOOTDEBUG
	// Clean up TemporaryItems directory when launched at boot.
	// It is safe to clean all file types at this time.
	if (argc > 1 && strcmp(argv[1], "-machineBoot") == 0) {
		const char *dirs[5];
		int i = 0;
		dirs[i++] = DIRHELPER_TEMP_STR;
		dirs[i++] = "TemporaryItems";
		dirs[i++] = "Cleanup At Startup";
		if (is_safeboot()) {
			dirs[i++] = DIRHELPER_CACHE_STR;
		}
		dirs[i] = NULL;
		clean_directories(dirs, 1);
		exit(EXIT_SUCCESS);
	} else if (argc > 1 && strcmp(argv[1], "-cleanTemporaryItems") == 0) {
		const char *dirs[] = {
			DIRHELPER_TEMP_STR,
			"TemporaryItems",
			NULL
		};
		clean_directories(dirs, 0);
		exit(EXIT_SUCCESS);
	} else if (argc > 1) {
		exit(EXIT_FAILURE);
	}

	launch_data_t config = NULL, checkin = NULL;
	checkin = launch_data_new_string(LAUNCH_KEY_CHECKIN);
	config = launch_msg(checkin);
	if (!config || launch_data_get_type(config) == LAUNCH_DATA_ERRNO) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "launchd checkin failed");
		exit(EXIT_FAILURE);
	}

	launch_data_t tmv;
	tmv = launch_data_dict_lookup(config, LAUNCH_JOBKEY_TIMEOUT);
	if (tmv) {
		idle_timeout = launch_data_get_integer(tmv);
		asl_log(NULL, NULL, ASL_LEVEL_DEBUG,
			"idle timeout set: %ld seconds", idle_timeout);
	}

	launch_data_t svc;
	svc = launch_data_dict_lookup(config, LAUNCH_JOBKEY_MACHSERVICES);
	if (!svc) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "no mach services");
		exit(EXIT_FAILURE);
	}

	svc = launch_data_dict_lookup(svc, DIRHELPER_BOOTSTRAP_NAME);
	if (!svc) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "no mach service: %s",
			DIRHELPER_BOOTSTRAP_NAME);
		exit(EXIT_FAILURE);
	}

	mach_port_t mp = launch_data_get_machport(svc);
	if (mp == MACH_PORT_NULL) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "NULL mach service: %s",
			DIRHELPER_BOOTSTRAP_NAME);
		exit(EXIT_FAILURE);
	}

	// insert a send right so we can send our idle exit message
	kr = mach_port_insert_right(mach_task_self(), mp, mp,
		MACH_MSG_TYPE_MAKE_SEND);
	if (kr != KERN_SUCCESS) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "send right failed: %s",
			mach_error_string(kr));
		exit(EXIT_FAILURE);
	}

	// spawn a thread for our idle timeout
	pthread_t thread;
	idle_globals.mp = mp;
	idle_globals.timeout = idle_timeout;
	gettimeofday(&idle_globals.lastmsg, NULL);
	pthread_create(&thread, NULL, &idle_thread, NULL);

	// look to see if we have any messages queued.  if not, assume
	// we were launched because of the calendar interval, and attempt
	// to clean the temporary items.
	mach_msg_type_number_t status_count = MACH_PORT_RECEIVE_STATUS_COUNT;
	mach_port_status_t status;
	kr = mach_port_get_attributes(mach_task_self(), mp,
		MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&status, &status_count);
	if (kr == KERN_SUCCESS && status.mps_msgcount == 0) {
		const char *dirs[] = {
			DIRHELPER_TEMP_STR,
			"TemporaryItems",
			NULL
		};
		clean_directories(dirs, 0);
		exit(EXIT_SUCCESS);
	}

	// main event loop

	kr = mach_msg_server(dirhelper_server, mxmsgsz, mp,
			MACH_RCV_TRAILER_ELEMENTS(MACH_RCV_TRAILER_AUDIT) |
			MACH_RCV_TRAILER_TYPE(MACH_MSG_TRAILER_FORMAT_0));
	if (kr != KERN_SUCCESS) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR,
			"mach_msg_server(mp): %s", mach_error_string(kr));
		exit(EXIT_FAILURE);
	}

	exit(EXIT_SUCCESS);
}
