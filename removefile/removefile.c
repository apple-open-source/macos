/*
 * Copyright (c) 2007-2023 Apple Inc. All rights reserved.
 */

#include "removefile.h"
#include "removefile_priv.h"

#include <TargetConditionals.h>
#include <sys/vnode.h>

#define MAXLONGPATHLEN 8192

removefile_state_t
removefile_state_alloc(void) {
	removefile_state_t state = (removefile_state_t)calloc(1, sizeof(struct _removefile_state));
	if (state != NULL)
		state->urand_file = -1;
	return state;
}

int
removefile_state_free(removefile_state_t state) {
	if (state != NULL) {
		if (state->urand_file != -1) {
			close(state->urand_file);
			state->urand_file = -1;
		}
		if (state->buffer != NULL) {
			free(state->buffer);
			state->buffer = NULL;
		}
		free(state);
	}
	return 0;
}

int
removefile_state_get(removefile_state_t state, uint32_t key, void* dst) {
	switch(key) {
		case REMOVEFILE_STATE_CONFIRM_CALLBACK:
			*(removefile_callback_t*)dst = state->confirm_callback;
			break;
		case REMOVEFILE_STATE_CONFIRM_CONTEXT:
			*(void**)dst = state->confirm_context;
			break;
		case REMOVEFILE_STATE_ERROR_CALLBACK:
			*(removefile_callback_t*)dst = state->error_callback;
			break;
		case REMOVEFILE_STATE_ERROR_CONTEXT:
			*(void**)dst = state->error_context;
			break;
		case REMOVEFILE_STATE_ERRNO:
			*(int*)dst = state->error_num;
			break;
		case REMOVEFILE_STATE_STATUS_CALLBACK:
			*(removefile_callback_t*)dst = state->status_callback;
			break;
		case REMOVEFILE_STATE_STATUS_CONTEXT:
			*(void**)dst = state->status_context;
			break;
		case REMOVEFILE_STATE_FTSENT:
			if (!state->recurse_entry) {
				errno = EINVAL;
				return -1;
			}
			*(FTSENT**)dst = state->recurse_entry;
			break;
		default:
			errno = EINVAL;
			return -1;
	}
	return 0;
}

int
removefile_state_set(removefile_state_t state, uint32_t key, const void* value) {
	switch(key) {
		case REMOVEFILE_STATE_CONFIRM_CALLBACK:
			state->confirm_callback = value;
			break;
		case REMOVEFILE_STATE_CONFIRM_CONTEXT:
			state->confirm_context = (void *) value;
			break;
		case REMOVEFILE_STATE_ERROR_CALLBACK:
			state->error_callback = value;
			break;
		case REMOVEFILE_STATE_ERROR_CONTEXT:
			state->error_context = (void *) value;
			break;
		case REMOVEFILE_STATE_ERRNO:
			state->error_num = *(int*)value;
			break;
		case REMOVEFILE_STATE_STATUS_CALLBACK:
			state->status_callback = value;
			break;
		case REMOVEFILE_STATE_STATUS_CONTEXT:
			state->status_context = (void *) value;
			break;
		default:
			errno = EINVAL;
			return -1;
   }
   return 0;
}

/*
 * convert the provided path to a "canonical" path - absolute and
 * without any '/../' or '/.' parts.
 * If old_path is not a directory, don't bother with the canonicalization, as
 * the edge case of '/../' or '/.' is not relevant.
 */
static int
canonicalize_path(const char *old_path, char *new_path, size_t max_len) {
	struct getattrDirReply {
		uint32_t length;
		uint32_t file_type;
		uint32_t link_count;
	};
	int error = 0;
	struct attrlist attr_list;
	struct getattrDirReply reply;

	memset(&attr_list, 0, sizeof(attr_list));
	memset(&reply, 0, sizeof(reply));
	attr_list.bitmapcount = ATTR_BIT_MAP_COUNT;
	attr_list.commonattr = ATTR_CMN_OBJTYPE;
	attr_list.dirattr = ATTR_DIR_LINKCOUNT;

	if ((error = getattrlist(old_path, &attr_list, &reply, sizeof(reply), FSOPT_NOFOLLOW))) {
		return errno;
	}

	/*
	 * Avoid getting the canonical path in case of a hardlink.
	 * Also avoid the canonicalization if the file is not a directory.
	 */
	if ((reply.file_type != VDIR) || (reply.link_count > 1)) {
		return -1;
	}

	struct pathReply {
		uint32_t length;
		attrreference_t attr;
		char path[];
	};
	size_t buf_len = sizeof(struct pathReply) + max_len;
	struct pathReply *buf = malloc(buf_len);
	if (buf == NULL) {
		return -1;
	}

	memset(&attr_list, 0, sizeof(attr_list));
	memset(buf, 0, buf_len);
	attr_list.bitmapcount = ATTR_BIT_MAP_COUNT;
	attr_list.commonattr = ATTR_CMN_FULLPATH;

	if ((error = getattrlist(old_path, &attr_list, buf, buf_len, FSOPT_NOFOLLOW))) {
		error = errno;
		goto out;
	}

	char *buf_end = (char *)buf + buf_len;
	char *attr_end = (char *)buf + buf->length;
	char *data = (char *)&buf->attr + buf->attr.attr_dataoffset;
	char *data_end = data + buf->attr.attr_length;

	if (data > attr_end || data_end > attr_end || data_end > buf_end || buf->attr.attr_length > max_len) {
		error = ENOBUFS;
	} else {
		memmove(new_path, data, buf->attr.attr_length);
	}

out:
	free(buf);
	return error;
}

int
removefile(const char* path, removefile_state_t state_param, removefile_flags_t flags) {
	int res = 0, error = 0;
	char* paths[] = { NULL, NULL };
	removefile_state_t state = state_param;
	size_t max_len = flags & REMOVEFILE_ALLOW_LONG_PATHS ? MAXLONGPATHLEN : PATH_MAX;

	if (path == NULL) {
		errno = EINVAL;
		return -1;
	} else if (strnlen(path, max_len) >= max_len) {
		errno = ENAMETOOLONG;
		return -1;
	}

	// allocate the state if it was not passed in
	if (state_param == NULL) {
		state = removefile_state_alloc();
		if (state == NULL) {
			errno = ENOMEM;
			return -1;
		}
	}
	state->cancelled = 0;
	state->unlink_flags = flags;

	if (flags & (REMOVEFILE_SECURE_7_PASS | REMOVEFILE_SECURE_35_PASS | REMOVEFILE_SECURE_1_PASS | REMOVEFILE_SECURE_3_PASS)) {
		__removefile_init_random(getpid(), state);
	}

	char *file_path = malloc(max_len);
	if (file_path == NULL) {
		error = errno;
		res = -1;
		goto out;
	}

	// Try to canonicalize the path, which is guaranteed to fit in `file_path`.
	// (Use the original path upon failure, which is also a legal length.)
	if (canonicalize_path(path, file_path, max_len) != 0) {
		strncpy(file_path, path, max_len);
		file_path[max_len - 1] = '\0';
	}
	paths[0] = file_path;
	if (state->unlink_flags & REMOVEFILE_RECURSIVE_SLIM) {
		res = __removefile_tree_walker_slim(file_path, state);
	} else {
		res = __removefile_tree_walker(paths, state);
	}
	error = state->error_num;

out:
	// deallocate if allocated locally
	if (state_param == NULL) {
		removefile_state_free(state);
	}

	if (res) {
		errno = error;
	}
	free(file_path);

	return res;
}

int
removefile_cancel(removefile_state_t state) {
	if (state == NULL) {
		errno = EINVAL;
		return -1;
	} else {
		state->cancelled = 1;
		return 0;
	}
}

int
removefileat(int fd, const char* path, removefile_state_t state_param, removefile_flags_t flags) {
	int error = 0;
	struct stat st;

	if (path == NULL) {
		errno = EINVAL;
		return -1;
	}
	// check if the path is absolute, or the AT_FDCWD argument was given
	char c = *(path);
	if (c == '/' || fd == AT_FDCWD)
		return removefile(path, state_param, flags);

	// make sure the provided fd is of a directory
	if ((error = fstat(fd, &st))) {
		return -1;
	}
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}

	char* base_path = malloc(PATH_MAX);
	if (base_path == NULL) {
		errno = ENOMEM;
		return -1;
	}
	error = fcntl(fd, F_GETPATH, base_path);

	// generate the base_path to call in removefile
	if (error == 0) {
		// add a '/' between base_path and the relative path.
		if (snprintf(base_path, PATH_MAX, "%s/%s", base_path, path) < PATH_MAX) {
			error = removefile(base_path, state_param, flags);
		} else {
			error = -1;
			errno = ENAMETOOLONG;
		}
	}

	free(base_path);
	return error;
}
