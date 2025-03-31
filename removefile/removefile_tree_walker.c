/* srm */
/* Copyright (c) 2000 Matthew D. Gauthier
 * Portions copyright (c) 2015-21 Apple Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE CONTRIBUTORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the contributors shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization.
 */

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fts.h>
#include <dirent.h>

#include <TargetConditionals.h>

#include "removefile.h"
#include "removefile_priv.h"

#if __APPLE__ && !TARGET_OS_SIMULATOR
#include <apfs/apfs_fsctl.h>
#endif

static bool iopolicy_materialization_on(void)
{
	// NOTE: some sandboxed processes may crash when calling to getiopolicy_np. See 76141982.
	// also, per-thread policies override process policies.

	int policy = (getiopolicy_np(IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES, IOPOL_SCOPE_THREAD));
	if (policy == IOPOL_MATERIALIZE_DATALESS_FILES_ON) {
		return true;
	} else if (policy == IOPOL_MATERIALIZE_DATALESS_FILES_OFF) {
		return false;
	}

	policy = (getiopolicy_np(IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES, IOPOL_SCOPE_PROCESS));
	if (policy == IOPOL_MATERIALIZE_DATALESS_FILES_ON) {
		return true;
	}

	return false;
}

static int
__removefile_process_file(FTS* stream, FTSENT* current_file, removefile_state_t state) {
	int res = 0;
	char* path = current_file->fts_accpath;

	int recursive = state->unlink_flags & REMOVEFILE_RECURSIVE;
	int keep_parent = state->unlink_flags & REMOVEFILE_KEEP_PARENT;
	int secure = state->unlink_flags & (REMOVEFILE_SECURE_7_PASS | REMOVEFILE_SECURE_35_PASS | REMOVEFILE_SECURE_1_PASS | REMOVEFILE_SECURE_3_PASS | REMOVEFILE_SECURE_1_PASS_ZERO);

	switch (current_file->fts_info) {
		// attempt to unlink the directory on pre-order in case it is
		// a directory hard-link.  If we succeed, it was a hard-link
		// and we should not descend any further.
		case FTS_D:
			if (unlink(path) == 0) {
				fts_set(stream, current_file, FTS_SKIP);
				break;
			}
#if __APPLE__ && !TARGET_OS_SIMULATOR
			else if (state->unlink_flags & REMOVEFILE_CLEAR_PURGEABLE) {
				uint64_t cp_flags = APFS_CLEAR_PURGEABLE;
				(void)fsctl(path, APFSIOC_MARK_PURGEABLE, &cp_flags, 0);
			}
#endif
#if __APPLE__
			/*
			 * When traversing a directory with fts_read,
			 * it will be accessed first in pre-order (FTS_D),
			 * then the children will be read in and processed, and later it will be
			 * accessed again in post-order (FTS_DP).
			 * If the directory is dataless we should skip the children read when possible.
			 * To know the list of children the directory needs to be materialised,
			 * and this operation can be very expensive.
			 */
			int is_dataless = (current_file->fts_statp->st_flags & SF_DATALESS) != 0;
			if (!is_dataless) {
				break;
			}
			// If recursive is not set the calling function will skip the children read anyway.
			// Do not try to unlink the rootlevel if REMOVEFILE_KEEP_PARENT is set.
			if (!recursive || secure || (keep_parent && current_file->fts_level == FTS_ROOTLEVEL)) {
				break;
			}
			if ((geteuid() == 0) &&
				(current_file->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
				!(current_file->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
				chflags(path, current_file->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE)) < 0) {
				break;
			}
			if (state->confirm_callback != NULL && iopolicy_materialization_on()) {
				break;
			}
			/*
			 * The directory could be marked as dataless but still have children on disk.
			 * If children are present we need to process them before unlinking the directory.
			 * The only way to discern this case without materialising
			 * the directory is to try to unlink it.
			 * In that case the unlink will fail.
			 */
			if (unlinkat(AT_FDCWD, path, AT_REMOVEDIR_DATALESS) == 0) {
				/*
				 * We succeeded at removing the directory.
				 * We need to both skip the children read and the next
				 * access of the directory in post-order.
				 */
				fts_set(stream, current_file, FTS_SKIP);
				state->runtime_flags = REMOVEFILE_SKIP;
			}
#endif
			break;
		case FTS_DC:
			state->error_num = ELOOP;
			res = -1;
			break;
		case FTS_DNR:
		case FTS_ERR:
		case FTS_NS:
			state->error_num = current_file->fts_errno;
			res = -1;
			break;
		case FTS_DP:
			if (state->runtime_flags == REMOVEFILE_SKIP) {
				state->runtime_flags = 0;
				break;
			}
			if (recursive &&
			    (!keep_parent ||
			     current_file->fts_level != FTS_ROOTLEVEL)) {
				if (secure) {
					res = __removefile_rename_unlink(path,
						state);
				} else {
					if (geteuid() == 0 &&
						(current_file->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
						!(current_file->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
						chflags(path, current_file->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE)) < 0) {
						errno = EACCES;
						res = -1;
					} else {
#if __APPLE__
						int is_dataless = (current_file->fts_statp->st_flags & SF_DATALESS) != 0;
						if (is_dataless) {
							if (!iopolicy_materialization_on() || state->confirm_callback == NULL) {
								res = unlinkat(AT_FDCWD, path, AT_REMOVEDIR_DATALESS);
							} else {
								res = rmdir(path);
							}
						} else {
							res = rmdir(path);
						}
#else
						res = rmdir(path);
#endif
					}
				}
				if (res == -1) state->error_num = errno;
			}
			break;
		case FTS_F:
		case FTS_SL:
		case FTS_SLNONE:
		case FTS_DEFAULT:
			if (secure) {
				res = __removefile_sunlink(path, state);
			} else if (geteuid() == 0 &&
				(current_file->fts_statp->st_flags & (UF_APPEND|UF_IMMUTABLE)) &&
				!(current_file->fts_statp->st_flags & (SF_APPEND|SF_IMMUTABLE)) &&
				chflags(path, current_file->fts_statp->st_flags &= ~(UF_APPEND|UF_IMMUTABLE)) < 0) {
				errno = EACCES;
				res = -1;

#if __APPLE__
			} else if (state->unlink_flags & REMOVEFILE_SYSTEM_DISCARDED) {
				res = unlinkat(AT_FDCWD, path, AT_SYSTEM_DISCARDED);
#endif
			} else {
				res = unlink(path);
			}
			if (res == -1) state->error_num = errno;
			break;
		case FTS_DOT:
		default:
			break;
	}
	return res;
}

int
__removefile_tree_walker(char **trees, removefile_state_t state) {
	FTSENT *current_file;
	FTS *stream;
	int rval = 0;
	int open_flags = 0;

	removefile_callback_t cb_confirm = NULL;
	removefile_callback_t cb_status = NULL;
	removefile_callback_t cb_error = NULL;

	cb_confirm = state->confirm_callback;
	cb_status = state->status_callback;
	cb_error = state->error_callback;

	open_flags = FTS_PHYSICAL | FTS_NOCHDIR | FTS_XDEV;

	/*
	 * If support of long paths is desired,
	 * we have to let FTS(3) change our working directory.
	 */
	if ((REMOVEFILE_ALLOW_LONG_PATHS & state->unlink_flags) != 0) {
		open_flags &= ~FTS_NOCHDIR;
	}

	/*
	 * Don't cross a mount point when deleting recursively by default.
	 * This default was changed in 10.11, previous to which there was
	 * no way to prevent removefile from crossing mount points.
	 * see: rdar://problem/6799948
	 */
	if ((REMOVEFILE_CROSS_MOUNT & state->unlink_flags) != 0)
		open_flags &= ~FTS_XDEV;

	stream = fts_open(trees, open_flags, NULL);
	if (stream == NULL) {
		state->error_num = errno;
		return -1;
	}

	while ((current_file = fts_read(stream)) != NULL) {
		int res = REMOVEFILE_PROCEED;

		/*
		 * fts_number is set to REMOVEFILE_SKIP for directories where
		 * the confirmation callback has asked to skip it.  We check
		 * this on post-order, so we don't remove an empty directory that
		 * the caller wanted preserved.
		 */
		if (current_file->fts_info == FTS_DP &&
		    current_file->fts_number == REMOVEFILE_SKIP) {
			current_file->fts_number = 0;
			continue;
		}

		/* Setting FTS_XDEV skips the mountpoint on pre-order traversal,
		 * but you have to manually hop over it on post-order or
		 * removefile will return an error.
		 */
		if (current_file->fts_info == FTS_DP &&
			stream->fts_options & FTS_XDEV &&
			stream->fts_dev != current_file->fts_dev) {
			continue;
		}

		// don't process the file if a cancel has been requested
		if (__removefile_state_test_cancel(state)) break;

		state->recurse_entry = current_file;

		// confirm regular files and directories in pre-order 
		if (cb_confirm && current_file->fts_info != FTS_DP) {
			res = cb_confirm(state,
				current_file->fts_path, state->confirm_context);
		}

		// don't process the file if a cancel has been requested
		// by the callback
		if (__removefile_state_test_cancel(state)) break;

		if (res == REMOVEFILE_PROCEED) {
			state->error_num = 0;
			rval = __removefile_process_file(stream, current_file,
					state);

			if (state->error_num != 0) {
				// Since removefile(3) is abstracting the
				// traversal of the directory tree, suppress
				// all ENOENT and ENOTDIR errors from the
				// calling process.
				// However, these errors should still be
				// reported for the ROOTLEVEL since those paths
				// were explicitly provided by the application.
				if ((state->error_num != ENOENT &&
				     state->error_num != ENOTDIR) ||
				    current_file->fts_level == FTS_ROOTLEVEL) {
					if (cb_error) {
						res = cb_error(state,
							current_file->fts_path,
							state->error_context);
						if (res == REMOVEFILE_PROCEED ||
							res == REMOVEFILE_SKIP) {
							rval = 0;
						} else if (res == REMOVEFILE_STOP) {
							rval = -1;
						}
					} else {
						res = REMOVEFILE_STOP;
					}
				}
			// show status for regular files and directories
			// in post-order
			} else if (cb_status &&
				   current_file->fts_info != FTS_D) {
				res = cb_status(state, current_file->fts_path,
					state->status_context);
			}
		}

		if (current_file->fts_info == FTS_D && res == REMOVEFILE_SKIP) {
			current_file->fts_number = REMOVEFILE_SKIP;
		}

		if (res == REMOVEFILE_SKIP ||
    		    !(state->unlink_flags & REMOVEFILE_RECURSIVE))
			fts_set(stream, current_file, FTS_SKIP);
		if (res == REMOVEFILE_STOP ||
		    __removefile_state_test_cancel(state))
			break;
	}

	if (__removefile_state_test_cancel(state)) {
		state->error_num = ECANCELED;
		rval = -1;
	}

	state->recurse_entry = NULL;

	fts_close(stream);
	return rval;
}

static int check_error_cb(char *path, removefile_state_t state, int level)
{
	state->error_num = errno;

	// abstract away ENOENT and ENOTDIR errors to the callers, like in the regular removefile implementation
	if ((state->error_num == ENOENT || state->error_num == ENOTDIR) && (level != 0)) {
		return 0;
	}

	if (state->error_callback &&
		state->error_callback(state, path, state->error_context) != REMOVEFILE_STOP) {
		return 0;
	}

	return -1;
}

typedef struct dir_entry {
	DIR *dir;
	struct stat sb;
} dir_entry_t;

static void
change_path_to_parent(char *path) {
	char *last = strrchr(path, '/');
	if (last) {
		last[0] = '\0';
	}
}

static int
move_to_parent_dir(char *cur_path, int *level, dir_entry_t *directories, DIR **cur_dir)
{
	if (*cur_dir && (closedir(*cur_dir) != 0)) {
		return -1;
	}

	directories[*level].dir = NULL;
	if (*level == 0) {
		// can't move to the parent of the root directory we are removing
		*cur_dir = NULL;
		return 0;
	}

	// move to the parent directory
	(*level)--;
	*cur_dir = directories[*level].dir;
	change_path_to_parent(cur_path);

	return 0;
}

#define INITIAL_DIRECTORIS_LEN 8

int
__removefile_tree_walker_slim(const char *path, removefile_state_t state) {
	int directories_len = INITIAL_DIRECTORIS_LEN;
	dir_entry_t *directories;
	struct dirent *entry;
	int rval = 0;
	int level = 0;
	DIR *cur_dir;
	char cur_path[MAXPATHLEN];
	struct stat tmp_sb;
	int unsupported = state->unlink_flags & (REMOVEFILE_SECURE_7_PASS | REMOVEFILE_SECURE_35_PASS | REMOVEFILE_SECURE_1_PASS | REMOVEFILE_SECURE_3_PASS | REMOVEFILE_SECURE_1_PASS_ZERO | REMOVEFILE_ALLOW_LONG_PATHS);

	if (state->confirm_callback || state->status_callback || unsupported) {
		state->error_num = EINVAL;
		return -1;
	}

	directories = calloc(directories_len, sizeof(dir_entry_t));
	if (!directories) {
		state->error_num = ENOMEM;
		return -1;
	}

	snprintf(cur_path, sizeof(cur_path), "%s", path);
	cur_dir = directories[0].dir = opendir(cur_path);
	if (!cur_dir) {
		state->error_num = errno;
		free(directories);
		return -1;
	}

	if (fstat(dirfd(cur_dir), &directories[0].sb)) {
		state->error_num = errno;
		closedir(cur_dir);
		free(directories);
		return -1;
	}

	while (directories[0].dir != NULL) {
		if (__removefile_state_test_cancel(state)) {
			rval = -1;
			state->error_num = ECANCELED;
			break;
		}

		errno = 0;
		entry = readdir(cur_dir);
		if (entry == NULL) {
			// check for readdir errors
			if (errno) {
				rval = -1;
				state->error_num = errno;
				break;
			}

			if (!((state->unlink_flags & REMOVEFILE_KEEP_PARENT) && level == 0)) {
				rval = rmdir(cur_path);
				if (rval && (rval = check_error_cb(cur_path, state, level))) {
					break;
				}
			}

			rval = move_to_parent_dir(cur_path, &level, directories, &cur_dir);
			if (rval) {
				state->error_num = errno;
				break;
			}

			continue;
		}

		if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) {
			continue;
		}

		if (snprintf(cur_path, sizeof(cur_path), "%s/%s", cur_path, entry->d_name) >= MAXPATHLEN) {
			rval = -1;
			state->error_num = ENAMETOOLONG;
			break;
		}

check_entry:
		if (entry->d_type == DT_UNKNOWN) {
			// some implementations don't populate d_type, get the details from stat(2)
			rval = (lstat(cur_path, &tmp_sb));
			if (rval) {
				// can't stat this entry - just continue to the next one
				continue;
			}
			if (S_ISREG(tmp_sb.st_mode)) {
				entry->d_type = DT_DIR;
			} else if (S_ISLNK(tmp_sb.st_mode)) {
				entry->d_type = DT_LNK;
			} else {
				// fake everything else like it's a regular file
				entry->d_type = DT_REG;
			}
			goto check_entry;
		}

		if (entry->d_type == DT_DIR) {
			if (!(state->unlink_flags & REMOVEFILE_CROSS_MOUNT)) {
				rval = stat(cur_path, &tmp_sb);
				if (rval) {
					state->error_num = errno;
					break;
				}
				if (tmp_sb.st_dev != directories[level].sb.st_dev) {
					change_path_to_parent(cur_path);
					continue;
				}
			}
#if __APPLE__ && !TARGET_OS_SIMULATOR
			if (state->unlink_flags & REMOVEFILE_CLEAR_PURGEABLE) {
				uint64_t cp_flags = APFS_CLEAR_PURGEABLE;
				(void)fsctl(cur_path, APFSIOC_MARK_PURGEABLE, &cp_flags, 0);
			}
#endif

			level++;
			if (level >= directories_len) {
				directories_len *= 2;
				directories = realloc(directories, sizeof(dir_entry_t) * directories_len);
				if (!directories) {
					rval = -1;
					state->error_num = ENOMEM;
					break;
				}
				memset(&directories[level], 0, sizeof(dir_entry_t) * directories_len / 2);
			}

			// try to remove preorder empty or dataless directories
			if (unlinkat(AT_FDCWD, cur_path, AT_REMOVEDIR_DATALESS) == 0) {
				change_path_to_parent(cur_path);
				level--;
				continue;
			}

			// descend into this directory
			directories[level].dir = opendir(cur_path);
			cur_dir = directories[level].dir;
			if (!cur_dir || fstat(dirfd(cur_dir), &directories[level].sb)) {
				// can't stat this directory, don't descend into it
				rval = move_to_parent_dir(cur_path, &level, directories, &cur_dir);
			}
		} else {
			// regular files, symbolic links and everything else
			rval = unlink(cur_path);
			if (rval && (rval = check_error_cb(cur_path, state, level))) {
				break;
			}
			// restore cur_path to point at the parent directory
			change_path_to_parent(cur_path);
		}

		if (rval) {
			if (!state->error_num) {
				state->error_num = errno;
			}
			break;
		}
	}

	for (int i = 0; i < directories_len; i++) {
		if (directories[i].dir) {
			closedir(directories[i].dir);
		}
	}

	free(directories);
	return rval;
}
