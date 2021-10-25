/*
 * Copyright (c) 1999-2020 Apple Inc. All rights reserved.
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

#include "mount_ramdisk.h"

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

#include <APFS/APFSConstants.h>
#include <MediaKit/MKMedia.h>
#include <MediaKit/MKMediaAccess.h>
#include <MediaKit/GPTTypes.h>

#define RAMDISK_BLCK_OFFSET 34
#define RAMDISK_TMP_MOUNT "/mnt2"
#define RAMDISK_BCK_MOUNT "/.mb"
#define RAMDISK_SIZE_TOK  "size="
#define RAMDISK_TPLT_TOK  "template="
#define HDIK_PATH         "/usr/sbin/hdik"

#define environ (*_NSGetEnviron())
#define COMMAND_OUTPUT_MAX 1024

// From mount.c
extern int verbose;
int errno_or_sysexit(int err, int sysexit);
void print_mount(const char **vfslist);
int mountfs(const char *vfstype, const char *fs_spec, const char *fs_file,
			int flags, const char *options, const char *mntopts);

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
int
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

int
_verify_file_flags(char *path, int flags)
{
	
	if (access(path, flags)) {
		fprintf(stderr, "Failed access check for %s with issue %s\n", path, strerror(errno));
		return errno;
	}

	return 0;
}

int
verify_executable_file_existence(char *path)
{
	return _verify_file_flags(path, F_OK | X_OK);
}

int
verify_file_existence(char *path)
{
	return _verify_file_flags(path, F_OK);
}

// Helper function that truncates whitespaces
void
truncate_whitespace(char *str)
{
	size_t idx = strcspn(str, " \n");
	if (idx != 0) {
		str[idx] = '\0';
	}
}

// Triggers newfs_apfs for the target device
int
construct_apfs_volume(char *mounted_device_name)
{
	int return_val = -1;
	int status = -1;
	char *command[5] = { "/sbin/newfs_apfs", "-v", "Var", mounted_device_name, NULL };

	status = run_command(command, NULL, &return_val, NULL);
	if (status >= 0) {
		return return_val;
	} else {
		fprintf(stderr, "Failed to execute command %s\n", command[0]);
		errno_or_sysexit(errno, -1);
	}

	// shouldn't reach here. This is to satisfy the compiler
	return -1;
}

// Unmounts device at location
int
unmount_location(char *mount_point)
{
	int return_val = -1;
	int status = -1;
	char *command[4] = { "/sbin/umount", "-f", mount_point, NULL };

	status = run_command(command, NULL, &return_val, NULL);
	if (status >= 0) {
		return return_val;
	} else {
		fprintf(stderr, "Failed to execute command %s\n", command[0]);
		return errno_or_sysexit(errno, -1);
	}
}

/*
 * The mnt_opts for fstab are standard across the different
 * mount_fs implementations. To create and mount an ephemeral
 * filesystem, it is necessary to provide additional non-standard
 * values in filesystem definition - mainly size and location of
 * the seed files.
 * The fstab definition for a ramdisk fs requires two new parameters:
 * 'size=%zu' and 'template=%s'. To keep the fstab structure
 * consistent with that of other filesystem types, these
 * parameters are appended at the end of the mnt_opts string.
 * It is necessary to split the mnt_opts into two strings, the
 * standard mountfs parameters that are used in the fs-specifnc mount
 * and the ramdisk definition parameters.
 */
char *
split_ramdisk_params(char *opts)
{
	char* opt             = NULL;
	char* target_str      = NULL;
	char* size_tok        = RAMDISK_SIZE_TOK;
	char* tplt_tok        = RAMDISK_TPLT_TOK;
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

char *
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

// Creates the partition table directly through MediaKit.
int
create_partition_table(size_t partition_size, char *device)
{
	MKStatus err = -1;
	MKMediaRef gpt_ref               = NULL;
	CFMutableArrayRef schemes        = NULL;
	CFMutableArrayRef partitionArray = NULL;
	CFDictionaryRef partition        = NULL;
	CFMutableDictionaryRef options   = NULL;
	CFMutableDictionaryRef layout    = NULL;
	CFMutableDictionaryRef media     = NULL;
	CFMutableDictionaryRef map       = NULL;

	layout  = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	if (!layout || !options) {
		fprintf(stderr, "Failed to create necessary CFDictionaries\n");
		err = errno;
		goto done;
	}

	CFDictionarySetValue(options, kMKMediaPropertyWritableKey, kCFBooleanTrue);
	
	gpt_ref = MKMediaCreateWithPath(kCFAllocatorDefault, device, options, &err);
	CFRelease(options);

	if (gpt_ref) {
		MKStatus mediaErr = 0;
		partition = MKCFBuildPartition(PMGPTTYPE, apple_apfs, CFSTR(EDTVolumeFSType),
				CFSTR(RAMDISK_FS_SPEC), 0, RAMDISK_BLCK_OFFSET, &mediaErr, NULL);
		if (!partition) {
			fprintf(stderr, "Failed to create partition with err %d\n", mediaErr);
			err = mediaErr;
			goto done;
		}

		partitionArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		if (!partitionArray) {
			fprintf(stderr, "Failed to create partitionArray\n");
			err = errno;
			CFRelease(partition);
			goto done;
		}
		CFArrayAppendValue(partitionArray, partition);
		CFRelease(partition);

		CFDictionaryAddValue(layout, CFSTR(MK_PARTITIONS_KEY), partitionArray);
		CFRelease(partitionArray);

		media = MKCFCreateMedia(&schemes, &mediaErr);
		if (!media) {
			fprintf(stderr, "Failed to create Schemes with error %d\n", mediaErr);
			err = mediaErr;
			goto done;
		}

		map = MKCFCreateMap(PMGPTTYPE, schemes, layout, NULL, NULL,
			NULL, NULL, NULL, gpt_ref, &mediaErr);
		if (!map) {
			fprintf(stderr, "Failed to create map with error %d\n", mediaErr);
			err = mediaErr;
			goto done;
		}

		err = MKCFWriteMedia(media, layout, NULL, NULL, gpt_ref);
		if (err) {
			fprintf(stderr, "Failed to WriteMedia with error %d\n", err);
			goto done;
		}
	} else {
		fprintf(stderr, "Failed to create gpt_ref with error %d\n", err);
		goto done;
	}

	if (verbose) {
		fprintf(stderr, "Releasing MediaKit objects\n");
	}
	err = 0;

done:
	if (media) {
		MKCFDisposeMedia(media);
	}

	if (layout) {
		CFRelease(layout);
	}

	if (gpt_ref) {
		CFRelease(gpt_ref);
	}

	return err;
}

// Creates a new unmounted ramdisk of size device_size
int
attach_device(size_t device_size , char* deviceOut)
{
	int return_val = -1;
	char ram_define [PATH_MAX];
	snprintf(ram_define, sizeof(ram_define), "ram://%zu", device_size);

	char *command[4] = { HDIK_PATH, "-nomount", ram_define, NULL };

	int status = run_command(command, deviceOut, &return_val, NULL);
	if (status == 1) {
		fprintf(stderr, "Failed to create ramdisk. HDIK returned %d.\n", return_val);
		exit(errno_or_sysexit(errno, -1));
	} else if (status != 0) {
		fprintf(stderr, "Failed to execute command %s\n", command[0]);
		exit(errno_or_sysexit(errno, -1));
	}

	truncate_whitespace(deviceOut);
	return return_val;
}

int
preflight_create_mount_ramdisk(char *mnt_opts, size_t *ramdisk_size, char *template)
{
	char *special_ramdisk_params;

	if (mnt_opts == NULL) {
		fprintf(stderr, "No mnt_opts provided to ramdisk preflight.\n");
		return EINVAL;
	}

	if (verify_executable_file_existence(HDIK_PATH) != 0) {
		fprintf(stderr, "Failed to find executable hdik at location %s \n", HDIK_PATH);
		return ENOENT;
	}

	special_ramdisk_params = split_ramdisk_params(mnt_opts);
	if (special_ramdisk_params == NULL) {
		fprintf(stderr, "Ramdisk fstab not in expected format.\n");
		return EINVAL;
	}

	if (ramdisk_size) {
		char *ramdisk_size_str = parse_parameter_for_token(special_ramdisk_params,
								RAMDISK_SIZE_TOK);

		if (ramdisk_size_str != NULL) {
			*ramdisk_size = atoi(ramdisk_size_str);
			free(ramdisk_size_str);
		}

		if (*ramdisk_size == 0) {
			fprintf(stderr, "Unexpected ramdisk size %zu\n", *ramdisk_size);
			return EINVAL;
		}
	}

	if (template) {
		char *template_str = parse_parameter_for_token(special_ramdisk_params, RAMDISK_TPLT_TOK);
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

// returns 0 upon success and a valid sysexit or errno code upon failure
int
create_mount_ramdisk(struct fstab *fs, int init_flags, char *options)
{
	int     default_flags                    =  init_flags;
	int     mount_return                     =  0;
	char    ramdisk_partition   [PATH_MAX]   =  { 0 };
	char    ramdisk_volume      [PATH_MAX]   =  { 0 };
	char    ramdisk_container   [PATH_MAX]   =  { 0 };
	char    seed_location       [PATH_MAX]   =  { 0 };
	char    *mnt_point                       =  RAMDISK_TMP_MOUNT; // intermediate
	char    *target_dir                      =  fs->fs_file; // target
	size_t  ram_size                         =  0;

	if (verify_file_existence(mnt_point) != 0) {
		if (verbose) {
			fprintf(stderr, "Default mount %s is not available. Using backup %s.\n",
					mnt_point, RAMDISK_BCK_MOUNT);
		}
		mnt_point = RAMDISK_BCK_MOUNT;
		if (verify_file_existence(mnt_point) != 0) {
			fprintf(stderr, "Mountpoints not available. Exiting.\n");
			return ENOENT;
		}
	}

	if (preflight_create_mount_ramdisk(fs->fs_mntops, &ram_size, seed_location) != 0) {
		fprintf(stderr, "Failed ramdisk preflight. Exiting.\n");
		return EINVAL;
	}

	if (verbose) {
		fprintf(stdout, "Attaching device of size %zu\n", ram_size);
	}

	if (attach_device(ram_size, ramdisk_partition) != 0){
		fprintf(stderr, "Failed to attach the ramdisk.\n");
		exit(errno_or_sysexit(ECHILD, -1));
	}

	if (verbose) {
		fprintf(stdout, "Creating partition table for device %s \n", ramdisk_partition);
	}

	if (create_partition_table(ram_size, ramdisk_partition) !=0) {
		fprintf(stderr, "Failed to create partition table.\n");
		exit(errno_or_sysexit(ECHILD, -1));
	}

	snprintf(ramdisk_container, sizeof(ramdisk_container), "%ss1", ramdisk_partition);

	if (verbose) {
		fprintf(stdout, "Creating apfs volume on partition %s\n", ramdisk_container);
	}

	if (construct_apfs_volume(ramdisk_container) != 0) {
		fprintf(stderr, "Failed to construct the apfs volume on the ramdisk.\n");
		exit(errno_or_sysexit(ECHILD, -1));
	}

	snprintf(ramdisk_volume, sizeof(ramdisk_volume), "%ss1", ramdisk_container);

	if (verify_file_existence(ramdisk_volume) != 0) {
		fprintf(stderr, "Failed to verify %s with issue %s\n", ramdisk_volume, strerror(errno));
		exit(errno_or_sysexit(errno, -1));
	}

	// Mount volume to RAMDISK_TMP_MOUNT
	if (verbose) {
		fprintf(stdout, "Mounting to tmp location %s\n", mnt_point);
	}

	mount_return = mountfs(EDTVolumeFSType, ramdisk_volume, mnt_point, default_flags, NULL, fs->fs_mntops);
	if (mount_return > 0) {
		fprintf(stderr, "Initial mount to %s failed with %d\n", mnt_point, mount_return);
		exit(errno_or_sysexit(errno, -1));
	}

	// ditto contents of RAMDISK_TMP_MOUNT to /private/var
	copyfile_state_t state = copyfile_state_alloc();
	copyfile_state_set(state, COPYFILE_STATE_STATUS_CB, _copyfile_status);
	if(copyfile(seed_location, mnt_point, state, COPYFILE_ALL | COPYFILE_RECURSIVE) < 0) {
		fprintf(stderr, "Failed to copy contents from %s to %s with error: %s\n",
				seed_location, mnt_point, strerror(errno));
		exit(errno_or_sysexit(errno, -1));
	}
	copyfile_state_free(state);

	// unount RAMDISK_TMP_MOUNT
	if(unmount_location(mnt_point) != 0){
		fprintf(stderr, "Failed to unmount device mounted at %s.\n", mnt_point);
		exit(errno_or_sysexit(ECHILD, -1));
	}

	if(verbose) {
		fprintf(stdout, "Mounting apfs volume %s to %s\n", ramdisk_volume, target_dir);
	}

	mount_return = mountfs(EDTVolumeFSType, ramdisk_volume, target_dir, default_flags, options, fs->fs_mntops);
	if (mount_return > 0) {
		fprintf(stderr, "Followup mount to %s failed with %d\n", target_dir, mount_return);
		exit(errno_or_sysexit(errno, -1));
	}

	// Verify contents in stdout
	if (verbose) {
		print_mount(NULL);
	}

	return mount_return;
}

#endif /* (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR) */
