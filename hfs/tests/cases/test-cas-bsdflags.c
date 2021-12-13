#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <removefile.h>

#include <System/sys/fsctl.h>

#include "hfs-tests.h"
#include "test-utils.h"
#include "disk-image.h"
#include "systemx.h"

#define AFSCUTIL       "/usr/local/bin/afscutil"

// Number of files cas_bsdflags_racer_ will ffsctl(FSIOC_CAS_BSDFLAGS).
#define NUM_RACE_FILES 128

// Prefix of race filenames.
#define RACE_FILE_PREFIX "cas_bsdflags.racer"

TEST(cas_bsdflags)

static bool
cas_bsd_flags(int fd, uint32_t expected_flags, uint32_t new_flags, int expected_error,
	uint32_t *actual_flags)
{
	struct fsioc_cas_bsdflags cas;

	cas.expected_flags = expected_flags;
	cas.new_flags      = new_flags;
	cas.actual_flags   = ~0;		/* poison */

	if (expected_error != 0) {
		// no assert_call_fail() in test_hfs
		assert(ffsctl(fd, FSIOC_CAS_BSDFLAGS, &cas, 0) == -1);
		assert(errno == EPERM);
		return true; // as expected - flags were not changed
	} else {
		assert_no_err(ffsctl(fd, FSIOC_CAS_BSDFLAGS, &cas, 0));
	}

	if (actual_flags) {
		*actual_flags = cas.actual_flags;
	}

	return (cas.expected_flags == cas.actual_flags);
}

static void
write_compressible_data(int fd)
{
	// adapted from test_clonefile in apfs
	char dbuf[4096];

	// write some easily compressable data
	memset(dbuf + 0*(sizeof(dbuf)/4), 'A', sizeof(dbuf)/4);
	memset(dbuf + 1*(sizeof(dbuf)/4), 'B', sizeof(dbuf)/4);
	memset(dbuf + 2*(sizeof(dbuf)/4), 'C', sizeof(dbuf)/4);
	memset(dbuf + 3*(sizeof(dbuf)/4), 'D', sizeof(dbuf)/4);
	for (int idx = 0; idx < 32; idx++) {
		check_io(write(fd, dbuf, sizeof(dbuf)), sizeof(dbuf));
	}
}

// adapted from APFS's test_xattr and test_content_protection
typedef struct cas_bsdflags_race_args {
	loop_barrier_t barrier;
	unsigned count;
	int *fds;
} cas_bsdflags_race_args_t;

/*
 * This will race to set `flags_to_set` as the BSD flags on all the file descriptors
 * provided while accepting one loss per file descriptor (which is acceptable
 * as long as only two threads are racing).
 */
static void
cas_bsdflags_racer_(cas_bsdflags_race_args_t *args, uint32_t rivals_flags, uint32_t flags_to_set)
{
	// Race to change the bsdflags of every file.
	for (unsigned i = 0; i < args->count; i++) {
		// Wait for the other thread to catch up to this iteration.
		loop_barrier_enter(&args->barrier, i);

		uint32_t expected_flags = 0;
		bool won_race = cas_bsd_flags(args->fds[i], 0, flags_to_set, 0, &expected_flags);
		if (!won_race) {
			// Another instance of cas_bsdflags_racer() won the race.
			// Try again with the correct expected_flags.
			assert_equal_int(expected_flags, rivals_flags);
			assert(cas_bsd_flags(args->fds[i], expected_flags, expected_flags | flags_to_set, 0, &expected_flags));
		}
	}
}

static void *
uf_hidden_racer(void *args)
{
	cas_bsdflags_racer_((cas_bsdflags_race_args_t *)args, UF_TRACKED, UF_HIDDEN);
	return NULL;
}

static void *
uf_tracked_racer(void *args)
{
	cas_bsdflags_racer_((cas_bsdflags_race_args_t *)args, UF_HIDDEN, UF_TRACKED);
	return NULL;
}

/*
 * Race two threads to set different bsdflags on the same files.
 * At the end of the test, all files should have both sets of flags.
 */
static void
test_cas_bsdflags_race(char *test_dir)
{
	char namebuf[128];
	memset(namebuf, 0, sizeof(namebuf));
	pthread_t uf_hidden_thread;
	pthread_t uf_tracked_thread;

	cas_bsdflags_race_args_t targs = {0};
	targs.count = NUM_RACE_FILES;
	targs.fds = malloc(targs.count * sizeof(int));
	assert(targs.fds);

	loop_barrier_init(&targs.barrier, 2);

	struct stat sb;

	// create the files
	for (unsigned i = 0; i < NUM_RACE_FILES; i++) {
		snprintf(namebuf, sizeof(namebuf), "%s/%s.%u", test_dir, RACE_FILE_PREFIX, i);
		int fd = open(namebuf, O_CREAT | O_RDWR | O_TRUNC, 0666);
		assert_with_errno(fd != -1);
		targs.fds[i] = fd;
	}

	// Start up the threads.
	assert_pthread_ok(pthread_create(&uf_hidden_thread, NULL, &uf_hidden_racer, &targs));
	assert_pthread_ok(pthread_create(&uf_tracked_thread, NULL, &uf_tracked_racer, &targs));

	// Wait for them to finish.
	assert_pthread_ok(pthread_join(uf_hidden_thread, NULL));
	assert_pthread_ok(pthread_join(uf_tracked_thread, NULL));

	// Check that both flags are set.
	for (unsigned i = 0; i < NUM_RACE_FILES; i++) {
		assert_no_err(fstat(targs.fds[i], &sb));
		assert_equal_int(sb.st_flags, UF_HIDDEN | UF_TRACKED);

		assert_no_err(close(targs.fds[i]));
	}

	// Clean up.
	loop_barrier_free(&targs.barrier);
	free(targs.fds);
}

int run_cas_bsdflags(__unused test_ctx_t *ctx)
{
	disk_image_t *di = disk_image_get();
	struct stat sb;
	int fd;

	char *test_dir;
	asprintf(&test_dir, "%s/cas_bsdflags", di->mount_point);

	// Make a new directory to house all the test files.
	assert_no_err(mkdir(test_dir, 0777));

	char *file;
	asprintf(&file, "%s/cas_bsdflags.data", test_dir);

	assert_with_errno((fd = open(file,
								 O_CREAT | O_RDWR | O_TRUNC, 0666)) >= 0);

	assert_no_err(fchflags(fd, UF_HIDDEN));
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_HIDDEN);

	assert(cas_bsd_flags(fd, 0, UF_NODUMP, 0, NULL) == false);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_HIDDEN);

	assert(cas_bsd_flags(fd, UF_HIDDEN, UF_NODUMP, 0, NULL) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_NODUMP);

	assert(cas_bsd_flags(fd, UF_NODUMP, 0, 0, NULL) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, 0);

	// Add some data to our (non-compressed) file,
	// mark it with UF_COMPRESSED,
	// and check that UF_COMPRESSED is *not* set -
	// as there is no decmpfs xattr present.
	check_io(write(fd, "J", 1), 1);
	assert_no_err(fstat(fd, &sb));
	assert(sb.st_size > 0);

	assert(cas_bsd_flags(fd, 0, UF_COMPRESSED, EPERM, NULL) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, 0);

	// Now, add some compressible data to the file and compress it using afscutil.
	write_compressible_data(fd);
	assert(!systemx(AFSCUTIL, "-c", file, NULL));
	assert_no_err(fstat(fd, &sb));
	assert_equal_int(sb.st_flags, UF_COMPRESSED);

	// Now, remove UF_COMPRESSED from our file and
	// check that the file is 0-length.
	assert(cas_bsd_flags(fd, UF_COMPRESSED, 0, 0, NULL) == true);
	assert_no_err(fstat(fd, &sb));
	assert_equal_ll(sb.st_size, 0);

	close(fd);
	assert_no_err(unlink(file));
	free(file);

	// Lastly, run our racing test.
	test_cas_bsdflags_race(test_dir);

	(void)removefile(test_dir, NULL, REMOVEFILE_RECURSIVE);
	free(test_dir);

	return 0;
}
