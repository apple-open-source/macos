/*
 * Copyright (c) 2020-2022 Apple Inc. All rights reserved.
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

#include "mount_tmpfs.h"

#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)

#include "edt_fstab.h"

#include <copyfile.h>
#include <crt_externs.h>
#include <os/errno.h>
#include <paths.h>
#include <spawn.h>
#include <stdio.h>
#include <string.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#if !TARGET_OS_BRIDGE
#include <CoreFoundation/CoreFoundation.h>
#include <UserManagementLayout/UMLCoreFunctions.h>
#endif

#define TMP_MOUNT "/.b/8/"
#define BCK_MOUNT "/private/system_data/"
#define SIZE_TOK  "size="
#define TPLT_TOK  "template="

#define MOUNT_TMPFS       "/sbin/mount_tmpfs"
#define UMOUNT            "/sbin/umount"

#define environ (*_NSGetEnviron())
#define COMMAND_OUTPUT_MAX 1024

// From mount.c
extern int verbose;
int errno_or_sysexit(int err, int sysexit);
void print_mount(const char **vfslist);

/*
 * Helper function that posix_spawn a child process
 * as defined by command_argv[0].
 * If `output` is non-null, then the command's stdout will be read
 * into that buffer.
 * If `rc` is non-null, then the command's return code will be set
 * there.
 * If `signal_no` is non-null, then if the command is signaled, the
 * signal number will be set there.
 *
 *
 * This function returns
 *  -1, if there's an internal error. errno will be set
 *   0, if command exit normally with 0 as return code
 *   1, if command exit abnormally or with a non-zero return code
 */
static int
run_command(char **command_argv, char *output, int *rc, int *signal_no)
{
	int error = -1;
	int faulting_errno = 0;
	int status = -1;
	int internal_result = -1;
	pid_t pid;
	posix_spawn_file_actions_t actions = NULL;
	int output_pipe[2] = {-1, -1};
	char *command_out = NULL;
	FILE *stream = NULL;

	if (!command_argv) {
		fprintf(stderr, "command_argv is NULL\n");
		errno = EINVAL;
		goto done;
	}

	if (pipe(output_pipe)) {
		fprintf(stderr, "Failed to create pipe for command output: %d (%s)\n",
				errno, strerror(errno));
		goto done;
	}

	if ((internal_result = posix_spawn_file_actions_init(&actions)) != 0) {
		errno = internal_result;
		fprintf(stderr, "posix_spawn_file_actions_init failed: %d (%s)\n",
				errno, strerror(errno));
		goto done;
	}

	if ((internal_result = posix_spawn_file_actions_addclose(&actions, output_pipe[0])) != 0) {
		errno = internal_result;
		fprintf(stderr, "posix_spawn_file_actions_addclose output_pipe[0] failed: %d (%s)\n",
				errno, strerror(errno));
		goto done;
	}

	if ((internal_result = posix_spawn_file_actions_adddup2(&actions,
						output_pipe[1], STDOUT_FILENO)) != 0) {
		errno = internal_result;
		fprintf(stderr, "posix_spawn_file_actions_adddup2 output_pipe[1] failed: %d (%s)\n",
				errno, strerror(errno));
		goto done;
	}

	if ((internal_result = posix_spawn_file_actions_addclose(&actions, output_pipe[1])) != 0) {
		errno = internal_result;
		fprintf(stderr, "posix_spawn_file_actions_addclose output_pipe[1] failed: %d (%s)\n",
				errno, strerror(errno));
		goto done;
	}

	if (verbose) {
		fprintf(stdout, "Executing command: ");
		for (char **command_segment = command_argv; *command_segment; command_segment++) {
			fprintf(stdout, "%s ", *command_segment);
		}
		fprintf(stdout, "\n");
	}

	if ((internal_result = posix_spawn(&pid, command_argv[0], &actions, NULL,
						command_argv, environ)) != 0 ) {
		errno = internal_result;
		fprintf(stderr, "posix_spawn failed: %d (%s)\n", errno, strerror(errno));
		goto done;
	}

	// Close out our side of the pipe
	close(output_pipe[1]);
	output_pipe[1] = -1;

	// If caller specified the output buffer, we'll use that
	// Otherwise allocate a buffer and capture the output ourselves for verbose logging
	if (output != NULL) {
		command_out = output;
	} else {
		command_out = calloc(COMMAND_OUTPUT_MAX, sizeof(char));
		if (!command_out) {
			fprintf(stderr, "calloc failed: %d (%s)\n", errno, strerror(errno));
			goto done;
		}
	}

	stream = fdopen(output_pipe[0], "r");
	if ( !stream ) {
		fprintf(stderr, "fdopen failed: %d (%s)\n", errno, strerror(errno));
		goto done;
	}

	size_t length;
	size_t count = 0;
	char *line;
	while ((line = fgetln(stream, &length)) && (count < COMMAND_OUTPUT_MAX - length - 1)) {
		strncat(command_out, line, length);
		count += length;
	}

	if ( ferror(stream) ) {
		fprintf(stderr, "fgetln failed: %d (%s)\n", errno, strerror(errno));
		goto done;
	}

	if ( fclose(stream) ) {
		fprintf(stderr, "fclose failed: %d (%s)\n", errno, strerror(errno));
		stream = NULL;
		goto done;
	}
	stream = NULL;
	close(output_pipe[0]);
	output_pipe[0] = -1;

	while (waitpid(pid, &status, 0) < 0) {
		if (errno == EINTR) {
			continue;
		}
		fprintf(stderr, "waitpid failed: %d (%s)\n", errno, strerror(errno));
		goto done;
	}

	if (verbose) {
		fprintf(stdout, "Command output:\n%s\n", command_out);
	}

	if (WIFEXITED(status)) {
		int exit_status = WEXITSTATUS(status);
		if (rc) *rc = exit_status;
		if (signal_no) *signal_no = 0;
		
		if (exit_status != 0) {
			error = 1;
			fprintf(stderr, "Command failed: %d\n", exit_status);
			goto done;
		}
	}

	if (WIFSIGNALED(status)) {
		if (rc) *rc = 0;
		if (signal_no) *signal_no = WTERMSIG(status);
		
		error = 1;
		fprintf(stderr, "Command signaled: %d\n", WTERMSIG(status));
		goto done;
	}

	error = 0;
done:
	// we don't care much about the errno set by the clean up routine
	// so save the errno here and return to caller
	faulting_errno = errno;

	if (actions) {
		posix_spawn_file_actions_destroy(&actions);
	}
	if (stream) {
		fclose(stream);
	}
	if (output_pipe[0] >= 0) {
		close(output_pipe[0]);
	}
	if (output_pipe[1] >= 0) {
		close(output_pipe[1]);
	}
	if (!output && command_out) {
		free(command_out);
	}

	errno = faulting_errno;
	return error;
}

static int
_verify_file_flags(char *path, int flags)
{
	
	if (access(path, flags)) {
		fprintf(stderr, "Failed access check for %s with issue %s\n", path, strerror(errno));
		return errno;
	}

	return 0;
}

static int
verify_executable_file_existence(char *path)
{
	return _verify_file_flags(path, F_OK | X_OK);
}

static int
verify_file_existence(char *path)
{
	return _verify_file_flags(path, F_OK);
}

/*
 * The mnt_opts for fstab are standard across the different
 * mount_fs implementations. To create and mount an ephemeral
 * filesystem, it is necessary to provide additional non-standard
 * values in filesystem definition - mainly size and location of
 * the seed files.
 * The fstab definition for a ramdisk fs (which is now simply
 * a tmpfs mount and not an actual ramdisk) requires two new parameters:
 * 'size=%zu' and 'template=%s'. To keep the fstab structure
 * consistent with that of other filesystem types, these
 * parameters are appended at the end of the mnt_opts string.
 */
static char *
split_params(char *opts)
{
	char* opt             = NULL;
	char* target_str      = NULL;
	char* size_tok        = SIZE_TOK;
	char* tplt_tok        = TPLT_TOK;
	char* optbuf          = NULL;
	size_t size_tok_len   = strlen(size_tok);
	size_t tplt_tok_len   = strlen(tplt_tok);
	
	optbuf = strdup(opts);
	for (opt = optbuf; (opt = strtok(opt, ",")) != NULL; opt = NULL) {
		size_t opt_len = strlen(opt);
		if ( (opt_len > size_tok_len && !strncmp(size_tok, opt, size_tok_len) ) ||
			(opt_len > tplt_tok_len && !strncmp(tplt_tok, opt, tplt_tok_len) ) ) {
			size_t start_index = opt - optbuf;
			target_str = opts + start_index;
			opts[start_index - 1 ] = '\0'; // Break original into two strings.
			break;
		}
	}
	free(optbuf);
	return target_str;
}

static char *
parse_parameter_for_token(char *opts, char *search_string)
{
	char *return_str = NULL;
	char *tmp_str    = NULL;
	char *target_str = strstr(opts, search_string);
	size_t len = strlen(search_string);
	
	if (target_str && strlen(target_str) > len) {
		tmp_str = target_str + len;
		size_t idx = strcspn(tmp_str, ",\0");
		if ( idx != 0 && (idx < MAXPATHLEN) ) {
			return_str = calloc(1, idx+1); //for null terminator
			strncpy(return_str, tmp_str, idx);
		}
	}

	return return_str;
}


static int
_copyfile_status(int what, int stage, copyfile_state_t state, const char *src, const char *dst, void *ctx)
{
	if (verbose && stage == COPYFILE_START) {
		if (what == COPYFILE_RECURSE_FILE) {
			fprintf(stderr, "Copying %s -> %s\n", src, dst);
		} else if (what == COPYFILE_RECURSE_DIR) {
			fprintf(stderr, "Creating %s/\n", dst);
		}
	}

	return COPYFILE_CONTINUE;
}

static int
preflight_create_tmpfs_mount(char *mnt_opts, size_t *size, char *template)
{
	char *mount_params;

	if (mnt_opts == NULL) {
		fprintf(stderr, "No mnt_opts provided to mount preflight.\n");
		return EINVAL;
	}

	if (verify_executable_file_existence(MOUNT_TMPFS) != 0) {
		fprintf(stderr, "Failed to find executable: %s \n", MOUNT_TMPFS);
		return ENOENT;
	}

	mount_params = split_params(mnt_opts);
	if (mount_params == NULL) {
		fprintf(stderr, "Ramdisk fstab not in expected format.\n");
		return EINVAL;
	}

	if (size) {
		char *size_str = parse_parameter_for_token(mount_params, SIZE_TOK);

		if (size_str != NULL) {
			long parsed_size;
			char *end;
			parsed_size = strtol(size_str, &end, 0);
			if (end == size_str || *end != '\0' || parsed_size <= 0) {
				fprintf(stderr, "Unexpected size string: %s\n", size_str);
				free(size_str);
				return EINVAL;
			}
			free(size_str);
			*size = parsed_size;
		}

		if (*size == 0) {
			fprintf(stderr, "Unexpected mount size %zu\n", *size);
			return EINVAL;
		}
	}

	if (template) {
		char *template_str = parse_parameter_for_token(mount_params, TPLT_TOK);
		if (template_str != NULL) {
			strlcpy(template, template_str, PATH_MAX);
			free(template_str);
		}

		if ( template == NULL ) {
			fprintf(stderr, "Ramdisk template path not found\n");
			return EINVAL;
		}
	}

	return 0;
}

static int
mount_tmpfs(char *mount_point, size_t size)
{
	int return_val = -1;
	int status = -1;
	char size_str[64];

	snprintf(size_str, sizeof(size_str), "%zu", size);
	char *command[5] = {
		MOUNT_TMPFS,
		"-s",
		size_str,
		mount_point,
		NULL,
	};

	status = run_command(command, NULL, &return_val, NULL);
	if (status >= 0) {
		return return_val;
	} else {
		fprintf(stderr, "Failed to execute command %s\n", command[0]);
		return errno_or_sysexit(errno, -1);
	}
}

// Unmounts device at location
static int
unmount_location(char *mount_point)
{
	int return_val = -1;
	int status = -1;
	char *command[4] = {
		UMOUNT,
		"-f",
		mount_point,
		NULL,
	};

	status = run_command(command, NULL, &return_val, NULL);
	if (status >= 0) {
		return return_val;
	} else {
		fprintf(stderr, "Failed to execute command %s\n", command[0]);
		return errno_or_sysexit(errno, -1);
	}
}

// returns 0 upon success and a valid sysexit or errno code upon failure
int
create_tmpfs_mount(struct fstab *fs, char *options)
{
	int     mount_return                     =  0;
	char    seed_location       [PATH_MAX]   =  { 0 };
	char    *mnt_point                       =  TMP_MOUNT; // intermediate
	char    *target_dir                      =  fs->fs_file; // target
	size_t  size                             =  0;

	if (verify_file_existence(mnt_point) != 0) {
		if (verbose) {
			fprintf(stderr, "Default mount %s is not available. Using backup %s.\n",
					mnt_point, BCK_MOUNT);
		}
		mnt_point = BCK_MOUNT;
		if (verify_file_existence(mnt_point) != 0) {
			fprintf(stderr, "Mountpoints not available. Exiting.\n");
			return ENOENT;
		}
	}

	if (preflight_create_tmpfs_mount(fs->fs_mntops, &size, seed_location) != 0) {
		fprintf(stderr, "Failed mount preflight. Exiting.\n");
		return EINVAL;
	}
	// EDT size is in units of 512B blocks.
	size *= 512;

	// Mount volume to TMP_MOUNT.
	if (verbose) {
		fprintf(stdout, "Mounting tmpfs volume at tmp location %s\n", mnt_point);
	}
	mount_return = mount_tmpfs(mnt_point, size);
	if (mount_return > 0) {
		fprintf(stderr, "Initial mount to %s failed with %d\n", mnt_point, mount_return);
		exit(errno_or_sysexit(errno, -1));
	}

	// Ditto contents of seed_location (/private/var) to mnt_point (TMP_MOUNT).
	// This is a temporary copy, as we need to mount over /private/var.
	copyfile_state_t state = copyfile_state_alloc();
	copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, _copyfile_status);
	if (copyfile(seed_location, mnt_point, state, COPYFILE_ALL | COPYFILE_RECURSIVE) < 0) {
		fprintf(stderr, "Failed to copy contents from %s to %s with error: %s\n",
				seed_location, mnt_point, strerror(errno));
		exit(errno_or_sysexit(errno, -1));
	}
	copyfile_state_free(state);

	// Mount volume to target_dir (/private/var).
	if (verbose) {
		fprintf(stdout, "Mounting tmpfs volume at %s\n", target_dir);
	}
	mount_return = mount_tmpfs(target_dir, size);
	if (mount_return > 0) {
		fprintf(stderr, "Final mount to %s failed with %d\n", target_dir, mount_return);
		exit(errno_or_sysexit(errno, -1));
	}

	// Ditto contents of TMP_MOUNT to target_dir.
	state = copyfile_state_alloc();
	copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, _copyfile_status);
	if (copyfile(mnt_point, target_dir, state, COPYFILE_ALL | COPYFILE_RECURSIVE) < 0) {
		fprintf(stderr, "Failed to copy contents from %s to %s with error: %s\n",
				mnt_point, target_dir, strerror(errno));
		exit(errno_or_sysexit(errno, -1));
	}
	copyfile_state_free(state);

	// unount mnt_point (TMP_MOUNT).
	if (unmount_location(mnt_point) != 0){
		fprintf(stderr, "Failed to unmount %s (errno %d).\n", mnt_point, errno);
		// Ignored.
	}

#if !TARGET_OS_BRIDGE
	if (enhanced_apfs_supported() && UMLCreatePrimaryUserLayout) {
		// Copy over user template
		CFErrorRef error = NULL;
		const CFStringRef system_path = CFSTR("/");
		const CFStringRef user_path = CFSTR("/private/var/mobile");
		bool ret = UMLCreatePrimaryUserLayout(system_path, user_path, false, &error);
		if (!ret) {
			char error_string[256];
			CFStringRef error_description = CFErrorCopyDescription(error);
			CFIndex error_code = CFErrorGetCode(error);

			CFStringGetCString(error_description, error_string, sizeof(error_string),
							   kCFStringEncodingUTF8);

			fprintf(stderr, "Failed to copy user template: %s (%ld).\n",
					error_string, error_code);
			CFRelease(error);
			CFRelease(error_description);
			// Ignored.
		} else {
			printf("UMLCreatePrimaryUserLayout passed without error\n");
		}
	}
#endif /* !TARGET_OS_BRIDGE */

	// Verify contents in stdout
	if (verbose) {
		print_mount(NULL);
	}

	return mount_return;
}

#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */
