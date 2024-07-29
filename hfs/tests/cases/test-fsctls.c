/*
 * Copyright (c) 2024 Apple, Inc. All rights reserved.
 *
 * Test medley of HFS fsctls
 */

#include <sys/ioctl.h>
#include <sys/ioccom.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/errno.h>
#include <spawn.h>
#include <sys/stat.h>
#include <System/sys/fsctl.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/dirent.h>
#include <pthread.h>

#include "../../core/hfs_fsctl.h"
#include "hfs-tests.h"
#include "test-utils.h"
#include "systemx.h"
#include "disk-image.h"

TEST(fsctls, .run_as_root = true)

static disk_image_t *di;
char *fsctl_srcdir;
char *fsctl_auxdir;
char *fsctl_targetdir;

#define KB				*1024
#define MAX_FILES		100
#define MAX_LOOPS       50000

typedef struct setclearinfo {
	char mountpath[MAXNAMLEN];
	char auxdir[MAXNAMLEN];
	char targetdir[MAXNAMLEN];
} setclearinfo_t;

static void setup_testvolume()
{
	char            *path;
	int				fd;
	void            *buf;

	// Create a test folder with MAX_FILES files
	asprintf(&fsctl_srcdir, "%s/fsinfo-test", di->mount_point);
	assert_no_err(systemx("/bin/rm", "-rf", fsctl_srcdir, NULL));
	assert_no_err(mkdir(fsctl_srcdir, 0777));

	for (unsigned i = 0; i < MAX_FILES; i++) {
		asprintf(&path, "%s/fsinfo_test.data.%u", fsctl_srcdir, getpid()+i);
		unlink(path);
		assert_with_errno((fd = open(path, O_RDWR | O_TRUNC | O_CREAT, 0666)) >= 0);
		free(path);

		unsigned buf_size = (1 KB) * (i + 1);
		buf = malloc(buf_size);
		memset(buf, 0x25, buf_size);
		check_io(write(fd, buf, buf_size), buf_size);
		free(buf);
		close(fd);
	}
	asprintf(&fsctl_auxdir, "%s/fsinfo-auxdir", "/tmp");
	systemx("/bin/rm", "-rf", fsctl_auxdir, NULL);
	assert_no_err(mkdir(fsctl_auxdir, 0777));
	asprintf(&fsctl_targetdir, "%s/fsinfo-targetdir", "/tmp");
	systemx("/bin/rm", "-rf", fsctl_targetdir, NULL);
	assert_no_err(mkdir(fsctl_targetdir, 0777));
}

static void *clear_thread_func(void *arg)
{
	setclearinfo_t *sci = (setclearinfo_t*)arg;
	uint32_t i = 0;
	for (i = 0; i < MAX_LOOPS; i++) {
		fsctl(sci->mountpath, HFSIOC_CLRBACKINGSTOREINFO, NULL, 0);
	}
	return NULL;
}

static void *set_thread_func(void *arg)
{
	uint32_t i = 0;
	setclearinfo_t *sci = (setclearinfo_t*)arg;
	struct hfs_backingstoreinfo hbsi = {0};
	hbsi.version = 1;
	hbsi.signature = 3419155;
	for (i = 0; i < MAX_LOOPS; i++) {
		// set the backing store
		// ignore errors, just loop it and verify no panics, errors
		if (i % 2 == 0) {
			hbsi.backingfd = open(sci->targetdir, O_RDONLY);
		} else {
			hbsi.backingfd = open(sci->auxdir, O_RDONLY);
		}
		if (hbsi.backingfd > 0) {
			int ret = fsctl(sci->mountpath, HFSIOC_SETBACKINGSTOREINFO, &hbsi, 0);
			if (ret < 0) {
				assert_equal_int(errno, EALREADY);
			}
			close(hbsi.backingfd);
		}
	}
	return NULL;
}

static void test_fsctl_setclrinfo()
{
	pthread_t clear_thread;
	void* thread_result;
	int fd, aux_fd, target_fd;
	struct hfs_backingstoreinfo hbsi = {0};
	setclearinfo_t setclear = {0};
	uint32_t unused_arg = 0;
	strlcpy(setclear.mountpath, di->mount_point, MAXNAMLEN);
	strlcpy(setclear.auxdir, fsctl_auxdir, MAXNAMLEN);
	strlcpy(setclear.targetdir, fsctl_targetdir, MAXNAMLEN);
	hbsi.version = 1;
	hbsi.signature = 3419155;

	thread_result = NULL;
	fd = open(di->path, O_RDONLY);
	if (fd >= 0) {
		// initial - set
		hbsi.backingfd = fd;
		fsctl (di->mount_point, HFSIOC_SETBACKINGSTOREINFO, &hbsi, 0);
		assert_equal_int(ffsctl(fd, FSIOC_FD_ONLY_OPEN_ONCE, &unused_arg, 0), -1);
		assert_equal_int(errno, EBUSY);
		close(fd);

		// simultaneous - set + clear
		int res = pthread_create(&clear_thread, NULL, clear_thread_func, (void*)&setclear);
		if (res == 0) {
			set_thread_func((void*)&setclear);
			pthread_join(clear_thread, &thread_result);
		}

		// final - clear
		fsctl(di->mount_point, HFSIOC_CLRBACKINGSTOREINFO, NULL, 0);

		// validate usecounts
		target_fd = open(fsctl_targetdir, O_RDONLY);
		assert_with_errno(target_fd > 0);
		assert_no_err(ffsctl(target_fd, FSIOC_FD_ONLY_OPEN_ONCE, &unused_arg, 0));
		close(target_fd);
		aux_fd = open(fsctl_auxdir, O_RDONLY);
		assert_with_errno(aux_fd > 0);
		assert_no_err(ffsctl(target_fd, FSIOC_FD_ONLY_OPEN_ONCE, &unused_arg, 0));
		close(aux_fd);
	} else {
		perror ("open failed");
	}
}

int run_fsctls(__unused test_ctx_t *ctx)
{
	di = disk_image_get();

	setup_testvolume();

	//run tests
	test_fsctl_setclrinfo();

	//cleanup & return
	free(fsctl_srcdir);
	free(fsctl_auxdir);
	free(fsctl_targetdir);

	return 0;
}
