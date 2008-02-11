/*
 * Copyright (c) 2006-2007 Apple Inc. All rights reserved.
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

#include "dirhelper_server.h"

// globals for idle exit
struct idle_globals {
	mach_port_t	mp;
	long		timeout;
	struct timeval	lastmsg;
};

struct idle_globals idle_globals;

void* idle_thread(void* param __attribute__((unused)));

int file_check(const char* path, int mode, int uid, int gid);
#define is_file(x) file_check((x), S_IFREG, -1, -1)
#define is_directory(x) file_check((x), S_IFDIR, -1, -1)
#define is_root_wheel_directory(x) file_check((x), S_IFDIR, 0, 0)

int is_safeboot(void);

void clean_files_older_than(const char* path, time_t when);
void clean_directory(const char* name, int);

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

// If when == 0, all files are removed.  Otherwise, only regular files older than when.
void
clean_files_older_than(const char* path, time_t when) {
	FTS* fts;
	
	char* path_argv[] = { (char*)path, NULL };
	fts = fts_open(path_argv, FTS_PHYSICAL | FTS_XDEV, NULL);
	if (fts) {
		FTSENT* ent;
		asl_log(NULL, NULL, ASL_LEVEL_INFO, "Cleaning %s", path);
		while ((ent = fts_read(fts))) {
			switch(ent->fts_info) {
				case FTS_F:
				case FTS_DEFAULT:
					// Unlink the file if it has not been accessed since
					// the specified time.  Obtain an exclusive lock so
					// that we can avoid a race with other processes
					// attempting to open the file.
					if (when == 0) {
						(void)unlink(ent->fts_path);
					} else if (S_ISREG(ent->fts_statp->st_mode) && ent->fts_statp->st_atime < when) {
						int fd = open(ent->fts_path, O_RDONLY | O_NONBLOCK);
						if (fd != -1) {
							int res = flock(fd, LOCK_EX | LOCK_NB);
							if (res == 0) {
								struct stat sb;
								res = fstat(fd, &sb);
								if (res == 0 && sb.st_atime < when) {
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
						(void)unlink(ent->fts_path);
					}
					break;
					
				case FTS_DP:
					if (when == 0) {
						(void)rmdir(ent->fts_path);
					}
					break;
					
				case FTS_ERR:
				case FTS_NS:
					asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", ent->fts_path, strerror(ent->fts_errno));
					break;
					
				default:
					break;
			}
		}
		fts_close(fts);
	} else {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", path, strerror(errno));
	}
}

int
file_check(const char* path, int mode, int uid, int gid) {
	int check = 1;
	struct stat sb;
	if (lstat(path, &sb) == 0) {
		check = check && ((sb.st_mode & S_IFMT) == mode);
		check = check && ((sb.st_uid == (uid_t)uid) || uid == -1);
		check = check && ((sb.st_gid == (gid_t)gid) || gid == -1);
	} else {
		if (errno != ENOENT) {
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

void
clean_directory(const char* name, int machineBoot) {
	DIR* d;
	time_t when = 0;

	if (!machineBoot) {
		struct timeval now;
		long days = 3;
		const char* str = getenv("CLEAN_FILES_OLDER_THAN_DAYS");
		if (str) {
			days = strtol(str, NULL, 0);
		}
		(void)gettimeofday(&now, NULL);

		asl_log(NULL, NULL, ASL_LEVEL_INFO, "Cleaning %s older than %ld days", name, days);

		when = now.tv_sec - (days * 60 * 60 * 24);
	}

	// Look up the boot time
	struct timespec boottime;
	size_t len = sizeof(boottime);
	if (sysctlbyname("kern.boottime", &boottime, &len, NULL, 0) == -1) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", "sysctl kern.boottime", strerror(errno));
		return;
	}

	if (!is_root_wheel_directory(VAR_FOLDERS_PATH)) {
		asl_log(NULL, NULL, ASL_LEVEL_ERR, "%s: %s", VAR_FOLDERS_PATH, "invalid ownership");
		return;
	}
	
	if ((d = opendir(VAR_FOLDERS_PATH))) {
		struct dirent* e;
		char path[PATH_MAX];

		// /var/folders/*
		while ((e = readdir(d))) {
			if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) continue;
			
			snprintf(path, sizeof(path), "%s%s", VAR_FOLDERS_PATH, e->d_name);
			if (is_root_wheel_directory(path)) {
				DIR* d2 = opendir(path);
				if (d2) {
					struct dirent* e2;
					
					// /var/folders/*/*
					while ((e2 = readdir(d2))) {
						char temporary_items[PATH_MAX];
						if (strcmp(e2->d_name, ".") == 0 || strcmp(e2->d_name, "..") == 0) continue;

						snprintf(temporary_items, sizeof(temporary_items),
							"%s/%s/%s", path, e2->d_name, name);
						if (is_directory(temporary_items)) {
							// at boot time we clean all files,
							// otherwise only clean regular files.
							clean_files_older_than(temporary_items, when);
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
}

int
main(int argc, char* argv[]) {
	mach_msg_size_t mxmsgsz = MAX_TRAILER_SIZE;
	kern_return_t kr;
	long idle_timeout = 30; // default 30 second timeout

	// Clean up TemporaryItems directory when launched at boot.
	// It is safe to clean all file types at this time.
	if (argc > 1 && strcmp(argv[1], "-machineBoot") == 0) {
		clean_directory(DIRHELPER_TEMP_STR, 1);
		clean_directory("TemporaryItems", 1);
		clean_directory("Cleanup At Startup", 1);
		if (is_safeboot()) clean_directory(DIRHELPER_CACHE_STR, 1);
		exit(EXIT_SUCCESS);
	} else if (argc > 1 && strcmp(argv[1], "-cleanTemporaryItems") == 0) {
		clean_directory(DIRHELPER_TEMP_STR, 0);
		clean_directory("TemporaryItems", 0);
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
		clean_directory(DIRHELPER_TEMP_STR, 0);
		clean_directory("TemporaryItems", 0);
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
