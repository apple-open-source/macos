#include "removefile.h"

#include <assert.h>
#include <stdatomic.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource_private.h>
#include <unistd.h>

#if __APPLE__
#include <TargetConditionals.h>
#if !TARGET_OS_SIMULATOR
#include <apfs/apfs_fsctl.h>
#endif // !TARGET_OS_SIMULATOR
#endif // __APPLE__

// FSEvents
#if !TARGET_OS_IPHONE
#include <CoreServices/CoreServices.h>
#else
#include <MobileCoreServices/FSEvents.h>
#endif

#define FSEVENT_ARRAY_SIZE            10

/*
 * FSEvents globals, prototypes and methods
 */

// Speculative telemetry paths look like
// /.activity/1/<dev>/<inode>/<origin_id>/<age>/<use_state>/<urgency>/<size>
// /.activity/1/16777232/18/2/1728292/3/512/4096
typedef struct fsevent {
	dev_t        st_dev;
	ino_t        st_ino;
	ino_t        origin_id;
	uint64_t     age;
	use_state_t  flags;
	uint64_t     urgency;
	size_t       size;
} fsevent_t;

static atomic_uint_fast64_t event_outstanding = 0;
static FSEventStreamRef fsevents_stream = NULL;
static dispatch_queue_t fsevents_queue = NULL;
static CFArrayRef fsevents_paths = NULL;

void
fsevents_callback(ConstFSEventStreamRef stream, void *info, size_t num_events, void *paths, const FSEventStreamEventFlags *flags, const FSEventStreamEventId *ids);

void
initiate_fsevent(
	fsevent_t *event,
	char *file,
	uint64_t origin_id,
	use_state_t use_state,
	residency_reason_t residency_reason,
	uint64_t urgency,
	size_t size)
{
	struct stat st;

	assert(stat(file, &st) == 0);

	event->st_dev = st.st_dev;
	event->st_ino = st.st_ino;
	event->origin_id = origin_id;
	event->age = 0;
	event->flags = use_state & APFS_USE_STATE_EVENT_MASK;
	event->flags |= (((uint32_t) residency_reason << APFS_RESIDENCY_REASON_EVENT_SHIFT) & APFS_RESIDENCY_REASON_EVENT_MASK);
	event->urgency = urgency;
	event->size = size;
}

char **
fsevent_to_array(fsevent_t event)
{
	char **path = malloc(sizeof(char *) * FSEVENT_ARRAY_SIZE);
	path[0] = NULL; // path starts with a slash so the first item will be empty
	path[1] = strdup(".activity");
	path[2] = strdup("1");
	assert(asprintf(&path[3], "%d", event.st_dev) > -1);
	assert(asprintf(&path[4], "%llu", event.st_ino) > -1);
	assert(asprintf(&path[5], "%llu", event.origin_id) > -1);
	assert(asprintf(&path[6], "%llu", event.age) > -1);
	assert(asprintf(&path[7], "%d", event.flags) > 1);
	assert(asprintf(&path[8], "%llu", event.urgency) > -1);
	assert(asprintf(&path[9], "%zu", event.size) > -1);

	// quick validation, start at index 1 since index 0 is NULL
	for (int i=1; i<FSEVENT_ARRAY_SIZE; i++)
	{
		assert(path[i] != NULL);
	}

	return path;
}

void
listen_for_event(fsevent_t *event)
{
	printf("listening for fsevent\n");
	if (fsevents_paths == NULL)
	{
		CFTypeRef path = CFSTR("/.activity");
		fsevents_paths = CFArrayCreate(kCFAllocatorDefault, &path, 1, &kCFTypeArrayCallBacks);
	}
	FSEventStreamContext context = {0, event, NULL, NULL, NULL};
	fsevents_stream = FSEventStreamCreate(kCFAllocatorDefault, &fsevents_callback, &context, fsevents_paths, kFSEventStreamEventIdSinceNow, 0, kFSEventStreamCreateFlagFileEvents);
	assert(fsevents_stream != NULL);
	FSEventStreamSetDispatchQueue(fsevents_stream, fsevents_queue);
	FSEventStreamStart(fsevents_stream);
	atomic_store(&event_outstanding, 1);

}

int
wait_for_event(void)
{
	int result = 0;
	// wait up to 10 seconds
	bool did_receive_event = false;
	for (int i=0; i<100; i++)
	{
		if (atomic_load(&event_outstanding) == 0)
		{
			did_receive_event = true;
			break;
		}
		usleep(100000);
	}
	if (!did_receive_event)
	{
		printf("Did not receive fsevent within 10 seconds\n");
		result = 1;
	}
	if (fsevents_stream) {
		FSEventStreamStop(fsevents_stream);
		FSEventStreamInvalidate(fsevents_stream);
		FSEventStreamRelease(fsevents_stream);
		fsevents_stream = NULL;
	}
	// decrement outstanding events so we don't release the stream again in the cleanup block
	atomic_fetch_sub(&event_outstanding, 1);

	return result;
}

void
fsevents_callback(ConstFSEventStreamRef stream, void *info, size_t num_events, void *paths, const FSEventStreamEventFlags *flags, const FSEventStreamEventId *ids)
{
	char **expected_path = fsevent_to_array(*(fsevent_t *)info);
	char *path = NULL, *to_free = NULL, *token = NULL;
	for (int i=0; i<num_events; i++)
	{
		path = to_free = strdup(((char **)paths)[i]);
		assert(path != NULL);
		int j = 0;
		while ((token = strsep(&path, "/")) != NULL)
		{
			// skip the first item (starts with a / so the first item is NULL)
			// skip the 6th item, as age won't be deterministic
			if (j == 0 || j == 6)
			{
				j++;
				continue;
			}

			if (strcmp(token, expected_path[j]) != 0)
			{
				printf("path did not match, path \"%s\", expected item %d to be \"%s\" found \"%s\"\n", ((char **)paths)[i], j, expected_path[j], token);
				break;
			}

			if ((j + 1) == FSEVENT_ARRAY_SIZE)
			{
				printf("found matching fsevent\n");
				atomic_fetch_sub(&event_outstanding, 1);
			}

			j++;
		}
		free(to_free);
	}
	// start at 1 because first item is NULL
	for (int i=1; i<FSEVENT_ARRAY_SIZE; i++)
	{
		free(expected_path[i]);
	}
	free(expected_path);
}

static void
verify_telemetry_xattr(char *file,
					   bool is_xattr_removed)
{
	spec_telem_state_req_t spec_req_in = {};
	uint16_t flags = 0;

	assert(file);

	if (is_xattr_removed) {
		assert(fsctl(file, APFSIOC_GET_SPEC_TELEM, &spec_req_in, 0) != 0);
	} else {
		assert(fsctl(file, APFSIOC_GET_SPEC_TELEM, &spec_req_in, 0) == 0);
	}
}

static void
speculative_download_mark_file_pristine(char *path, residency_reason_t residency_reason, bool is_skip_pristiness)
{
	spec_telem_req_t spec_telem_req = { .spec_telem_op_state = APFS_SPEC_TELEM_MARK_PRISTINE };
	spec_telem_req.residency_reason = residency_reason;
	if (is_skip_pristiness) {
		spec_telem_req.spec_telem_flags |= APFS_SPEC_TELEM_KEEP_PRISTINE;
	}

	assert(fsctl(path, APFSIOC_SPEC_TELEM_OP, (void *)&spec_telem_req, 0) == 0);
}

static void
speculative_download_hierarchy_get(char *path, bool is_set)
{
	uint16_t expected_spec_telem_flags = 0;
	spec_telem_req_t spec_telem_req = { .spec_telem_op_state = APFS_SPEC_TELEM_HIERARCHY_GET };

	assert(fsctl(path, APFSIOC_SPEC_TELEM_OP, (void *)&spec_telem_req, 0) == 0);
	assert(spec_telem_req.is_spec_telemetry_hierarchy_set == is_set);
}

static void
speculative_download_hierarchy_set(char *path)
{
	apfs_exp_dir_stats_t args = {
		.version = APFS_EXP_DIR_STATS_V1,
		.op = DIR_STATS_OP_SET,
		.flags = DIR_STATS_SET_FOR_SAF | DIR_STATS_SET_SPEC_TELEMETRY,
	};
	int error = fsctl(path, APFSIOC_DIR_STATS_OP, &args, 0);

	if (error) {
		printf("Failed to set speculative download hierarchy %d\n", errno);
		return;
	}

	speculative_download_hierarchy_get(path, true);
}


static struct timeval tv;
static void start_timer(const char* str) {
	fprintf(stderr, "%s... ", str);
	assert(gettimeofday(&tv, NULL) == 0);
}

static void stop_timer(void) {
	struct timeval tv2;
	assert(gettimeofday(&tv2, NULL) == 0);
	long sec = tv2.tv_sec - tv.tv_sec;
	long usec;
	if (sec == 0) {
		usec = tv2.tv_usec - tv.tv_usec;
	} else {
		usec = tv2.tv_usec + (1000000 - tv.tv_usec);
	}
	fprintf(stderr, "%ld.%03ld\n", sec, usec);
}


static int removefile_confirm_callback(removefile_state_t state, const char * path, void * context) {
	assert(context == (void*)1234);

	FTSENT *entry;
	assert(removefile_state_get(state, REMOVEFILE_STATE_FTSENT, &entry) == 0);
	assert(entry->fts_statp);
	fprintf(stderr, "confirm callback: %s\n", path);
	return REMOVEFILE_PROCEED;
}

static int removefile_error_callback(removefile_state_t state, const char * path, void * context) {
	assert(context == (void*)4567);

	FTSENT *entry;
	int err = 0;
	assert(removefile_state_get(state, REMOVEFILE_STATE_ERRNO, &err) == 0);
	assert(removefile_state_get(state, REMOVEFILE_STATE_FTSENT, &entry) == 0);
	assert(entry->fts_statp);
	fprintf(stderr, "error callback: %s: %s (%d)\n", path, strerror(err), err);
	return REMOVEFILE_PROCEED;
}

static int removefile_status_callback(removefile_state_t state, const char * path, void * context) {
	FTSENT *entry;
	assert(removefile_state_get(state, REMOVEFILE_STATE_FTSENT, &entry) == 0);
	assert(entry->fts_statp);

	fprintf(stderr, "status callback: %s\n", path);
	return REMOVEFILE_PROCEED;
}

static void mklargedir(void) {
	char *test_top_dir = "/tmp/removefile-test";
	char large_dir_buf[NAME_MAX];
	char *cwd = getcwd(NULL, 0);
	size_t total_len = 0;

	start_timer("Creating long directory structure");
	assert(mkdir(test_top_dir, 0755) == 0);
	total_len += sizeof(test_top_dir);
	assert(chdir(test_top_dir) == 0);
	memset_pattern8(large_dir_buf, "cutiepie", NAME_MAX);
	large_dir_buf[NAME_MAX - 1] = 0;

	// repeatedly create directories so that the total path
	// of the depest directory is > PATH_MAX.
	while (total_len <= PATH_MAX) {
		assert(mkdir(large_dir_buf, 0755) == 0);
		total_len += NAME_MAX;
		assert(chdir(large_dir_buf) == 0);
	}

	stop_timer();
	chdir(cwd);
	free(cwd);
}

static void remove_long_canonical_path(void) {
	char *test_top_dir = "/tmp/removefile-test";

	char large_dir_buf[NAME_MAX];
	char *cwd = getcwd(NULL, 0);
	size_t total_len = 0;

	start_timer("Creating long directory structure");
	assert(mkdir(test_top_dir, 0755) == 0);
	total_len += sizeof(test_top_dir);
	assert(chdir(test_top_dir) == 0);
	memset_pattern8(large_dir_buf, "cutiepie", NAME_MAX);
	large_dir_buf[NAME_MAX - 1] = 0;

	// repeatedly create directories so that the total path
	// of the deepest directory is > PATH_MAX.
	int depth = 0;
	while (total_len <= PATH_MAX) {
		assert(mkdir(large_dir_buf, 0755) == 0);
		total_len += NAME_MAX;
		assert(chdir(large_dir_buf) == 0);
		depth++;
	}

	stop_timer();

	// The canonical path for the leaf is > PATH_MAX. With the long paths i/o policy on,
	// getattrlist in removefile will succeed and return a truncated path.
	// This shouldn't be an issue and removefile should still succeed

	assert(setiopolicy_np(IOPOL_TYPE_VFS_SUPPORT_LONG_PATHS, IOPOL_SCOPE_PROCESS, IOPOL_VFS_SUPPORT_LONG_PATHS_ON) == 0);

	start_timer("Removing from the leaf up");
	while (--depth >= 0) {
		assert(chdir("..") == 0);
		assert(removefile(large_dir_buf, NULL, REMOVEFILE_RECURSIVE) == 0);
	}

	stop_timer();
	assert(setiopolicy_np(IOPOL_TYPE_VFS_SUPPORT_LONG_PATHS, IOPOL_SCOPE_PROCESS, IOPOL_VFS_SUPPORT_LONG_PATHS_DEFAULT) == 0);
	assert(removefile(test_top_dir, NULL, REMOVEFILE_RECURSIVE) == 0);

	chdir(cwd);
	free(cwd);
}

static void mkdirs(bool mark_purgeable) {
	start_timer("Creating directory structure");
	assert(mkdir("/tmp/removefile-test", 0755) == 0);
	assert(mkdir("/tmp/removefile-test/foo", 0755) == 0);
	assert(mkdir("/tmp/removefile-test/foo/bar", 0755) == 0);
	assert(mkdir("/tmp/removefile-test/foo/baz", 0755) == 0);
	int fd;
	assert((fd = open("/tmp/removefile-test/foo/baz/woot", O_CREAT | O_TRUNC | O_WRONLY, 0644)) != -1);
	write(fd, "Hello World\n", 12);
	close(fd);
	assert((fd = open("/tmp/removefile-test/foo/baz/wootage", O_CREAT | O_TRUNC | O_WRONLY, 0644)) != -1);
	write(fd, "Hello World\n", 12);
	assert(lseek(fd, 1024*1024*30, SEEK_SET) != -1);
	write(fd, "Goodbye Moon\n", 13);
	close(fd);

#if __APPLE__ && TARGET_OS_IOS && !TARGET_OS_VISION
	if (mark_purgeable) {
		uint64_t purgeable_flags = APFS_MARK_PURGEABLE | APFS_PURGEABLE_DATA_TYPE | APFS_PURGEABLE_LOW_URGENCY;
		assert(fsctl("/tmp/removefile-test/foo/baz", APFSIOC_MARK_PURGEABLE, &purgeable_flags, 0) == 0);
	}

	// Create file under speculative download hierarchy to verify the system discarded flag
	// 1) Mark folder as dir-stats SAF with speculative download flag
	// 2) Create file under speculative download hierarchy
	// 3) Mark file as pristine
	// 4) Remove file with REMOVEFILE_SYSTEM_DISCARDED will create speculative download fsevent with system discarded flag
	assert(mkdir("/tmp/removefile-test/system-discarded", 0755) == 0);

	speculative_download_hierarchy_set("/tmp/removefile-test/system-discarded");
	assert((fd = open("/tmp/removefile-test/system-discarded/file_telem",  O_RDWR | O_CREAT, 0666)) != -1);
	write(fd, "Hello World\n", 12);
	close(fd);
	speculative_download_mark_file_pristine("/tmp/removefile-test/system-discarded/file_telem", APFS_SPEC_TELEM_RESIDENCY_REASON_TYPE_1, false);
	verify_telemetry_xattr("/tmp/removefile-test/system-discarded/file_telem", false);
#endif
	stop_timer();
}

void* threadproc(void* state) {
	sleep(1);
	fprintf(stderr, "cancelling...\n");
	assert(removefile_cancel(state) == 0);
	return NULL;
}

//
// With no arguments, this is our main automated test suite for removefile.
//
int main(int argc, char *argv[]) {
	removefile_state_t state = NULL;
	removefile_callback_t callback = NULL;
	pthread_t thread = NULL;
	int err = 0;
	struct stat st;

	fsevents_queue = dispatch_queue_create("com.apple.test_apfs.fsevents", NULL);

    if (argc == 2) {
        /* pass in a directory with a mountpoint under it to test REMOVEFILE_CROSS_MOUNT */
		state = removefile_state_alloc();
		removefile_state_set(state, REMOVEFILE_STATE_ERROR_CALLBACK, removefile_error_callback);
		removefile_state_set(state, REMOVEFILE_STATE_ERROR_CONTEXT, (void*)4567);
		err = removefile(argv[1], state,  REMOVEFILE_CROSS_MOUNT | REMOVEFILE_RECURSIVE);
		return err;
    }

	mkdirs(false);
	start_timer("removefile(NULL)");
	assert(removefile("/tmp/removefile-test", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	stop_timer();

	// This makes a path that is (including NUL) PATH_MAX length,
	// while each component is no longer than NAME_MAX.
	// (This should be a valid path name, even if it's not present on disk.)
	char longpathname[PATH_MAX + 2] = {0};
	char *path_namemax_buf = longpathname;
	for (int i = 0; i < (PATH_MAX / NAME_MAX); i++) {
		memset(path_namemax_buf, '9', NAME_MAX);
		path_namemax_buf += NAME_MAX;
		*path_namemax_buf = '/';
		path_namemax_buf++;
	}
	longpathname[PATH_MAX - 1] = '\0';

	start_timer("removefile(PATH_MAX)");
	assert(removefile(longpathname, NULL, REMOVEFILE_RECURSIVE) == -1 && errno == ENOENT);
	stop_timer();

	// Now, add one character and see that the pathname is invalid.
	longpathname[PATH_MAX - 1] = '/';
	longpathname[PATH_MAX] = 'a';
	longpathname[PATH_MAX + 1] = '\0';
	start_timer("removefile(PATH_MAX+1)");
	assert(removefile(longpathname, NULL, REMOVEFILE_RECURSIVE) == -1 && errno == ENAMETOOLONG);
	stop_timer();

	// See that pathname is invalid even if we enable the long paths i/o policy
	assert(setiopolicy_np(IOPOL_TYPE_VFS_SUPPORT_LONG_PATHS, IOPOL_SCOPE_PROCESS, IOPOL_VFS_SUPPORT_LONG_PATHS_ON) == 0);
	start_timer("removefile(PATH_MAX+1), long paths i/o policy");
	assert(removefile(longpathname, NULL, REMOVEFILE_RECURSIVE) == -1 && errno == ENAMETOOLONG);
	stop_timer();
	assert(setiopolicy_np(IOPOL_TYPE_VFS_SUPPORT_LONG_PATHS, IOPOL_SCOPE_PROCESS, IOPOL_VFS_SUPPORT_LONG_PATHS_DEFAULT) == 0);

	// See that pathname is invalid if we are accepting long paths
	// but didn't enable the long paths i/o policy
	start_timer("removefile(PATH_MAX+1), allow long paths, no long paths i/o policy");
	assert(removefile(longpathname, NULL, REMOVEFILE_RECURSIVE | REMOVEFILE_ALLOW_LONG_PATHS) == -1 && errno == ENAMETOOLONG);
	stop_timer();

	// See that pathname is valid iff we allow long paths and enable the long paths i/o policy
	assert(setiopolicy_np(IOPOL_TYPE_VFS_SUPPORT_LONG_PATHS, IOPOL_SCOPE_PROCESS, IOPOL_VFS_SUPPORT_LONG_PATHS_ON) == 0);
	start_timer("removefile(PATH_MAX+1), allow long paths, long paths i/o policy");
	assert(removefile(longpathname, NULL, REMOVEFILE_RECURSIVE | REMOVEFILE_ALLOW_LONG_PATHS) == -1 && errno == ENOENT);
	stop_timer();
	assert(setiopolicy_np(IOPOL_TYPE_VFS_SUPPORT_LONG_PATHS, IOPOL_SCOPE_PROCESS, IOPOL_VFS_SUPPORT_LONG_PATHS_DEFAULT) == 0);

	remove_long_canonical_path();

	mkdirs(false);
	assert((state = removefile_state_alloc()) != NULL);
	assert(pthread_create(&thread, NULL, threadproc, state) == 0);
	start_timer("removefile(state) with cancel");
	assert(removefile_state_set(state, REMOVEFILE_STATE_ERROR_CALLBACK, removefile_error_callback) == 0);
	assert(removefile_state_set(state, REMOVEFILE_STATE_ERROR_CONTEXT, (void*)4567) == 0);
	assert(removefile("/tmp/removefile-test", state, REMOVEFILE_SECURE_35_PASS | REMOVEFILE_RECURSIVE) == -1 && errno == ECANCELED);
	stop_timer();

	start_timer("removefile(NULL)");
	assert(removefile("/tmp/removefile-test", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	stop_timer();

	mkdirs(false);
	assert(removefile_state_set(state, 1234567, (void*)1234567) == -1 && errno == EINVAL);

	assert(removefile_state_set(state, REMOVEFILE_STATE_CONFIRM_CALLBACK, removefile_confirm_callback) == 0);
	assert(removefile_state_get(state, REMOVEFILE_STATE_CONFIRM_CALLBACK, &callback) == 0);
	assert(callback == removefile_confirm_callback);
	assert(removefile_state_set(state, REMOVEFILE_STATE_CONFIRM_CONTEXT, (void*)1234) == 0);

	assert(removefile_state_set(state, REMOVEFILE_STATE_ERROR_CALLBACK, removefile_error_callback) == 0);
	assert(removefile_state_get(state, REMOVEFILE_STATE_ERROR_CALLBACK, &callback) == 0);
	assert(callback == removefile_error_callback);
	assert(removefile_state_set(state, REMOVEFILE_STATE_ERROR_CONTEXT, (void*)4567) == 0);

	assert(removefile_state_set(state, REMOVEFILE_STATE_STATUS_CALLBACK, removefile_status_callback) == 0);
	assert(removefile_state_get(state, REMOVEFILE_STATE_STATUS_CALLBACK, &callback) == 0);
	assert(callback == removefile_status_callback);
	assert(removefile_state_set(state, REMOVEFILE_STATE_STATUS_CONTEXT, (void*)5678) == 0);

#if __APPLE__ && TARGET_OS_IOS && !TARGET_OS_VISION
	fsevent_t event = {};

	assert(stat("/tmp/removefile-test/system-discarded/", &st) == 0);
	// fill in the expected event for the file and reset the fields except origin_id and urgency
	initiate_fsevent(&event, "/tmp/removefile-test/system-discarded/file_telem", st.st_ino, USE_STATE_SYSTEM_DISCARDED,
					 APFS_SPEC_TELEM_RESIDENCY_REASON_TYPE_1, 0, 4096);
	listen_for_event(&event);
	assert(removefile("/tmp/removefile-test/system-discarded/file_telem", NULL, REMOVEFILE_SYSTEM_DISCARDED) == 0);
	assert(wait_for_event() == 0);

	//clean up
	assert(rmdir("/tmp/removefile-test/system-discarded") == 0);
#endif
	
	start_timer("removefile(state)");
	assert(removefile("/tmp/removefile-test", state, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	stop_timer();

	for (int i = 0; i < 2; i++) {
		start_timer("removefile(NULL, REMOVEFILE_FORCE)");
		mklargedir();
		assert(removefile("/tmp/removefile-test", NULL,
			(i == 1) ? REMOVEFILE_SECURE_1_PASS | REMOVEFILE_ALLOW_LONG_PATHS | REMOVEFILE_RECURSIVE
			: REMOVEFILE_ALLOW_LONG_PATHS | REMOVEFILE_RECURSIVE) == 0);
		stop_timer();
	}

	int fd;
	mkdirs(true);
	assert((fd = open("/tmp/removefile-test", O_RDONLY)) != -1);

	start_timer("removefileat(NULL)");
	assert(removefileat(fd, "/tmp/removefile-test/foo/baz/woot", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	assert(removefileat(fd, "../removefile-test/foo/baz", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE | REMOVEFILE_CLEAR_PURGEABLE) == 0);
	assert(removefileat(fd, "foo/bar", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	assert(removefileat(fd, "./foo", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	char path[1024];
	memset_pattern4(path, "././", 1000);
	path[1000] = NULL;
	assert(removefileat(fd, path, NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == -1 && errno == ENAMETOOLONG);
	assert(removefileat(AT_FDCWD, "/tmp/removefile-test", NULL, REMOVEFILE_SECURE_1_PASS | REMOVEFILE_RECURSIVE) == 0);
	stop_timer();
	close(fd);

	mkdirs(true);
	start_timer("removefile(slim)");
	assert(removefile("/tmp/removefile-test", NULL, REMOVEFILE_RECURSIVE_SLIM | REMOVEFILE_CLEAR_PURGEABLE | REMOVEFILE_KEEP_PARENT) == 0);
	stop_timer();
	// check that the parent wasn't removed
	assert((fd = open("/tmp/removefile-test", O_RDONLY | O_DIRECTORY)) != -1);
	close(fd);
	assert(removefile("/tmp/removefile-test", NULL, REMOVEFILE_RECURSIVE_SLIM | REMOVEFILE_CLEAR_PURGEABLE) == 0);

	printf("Success!\n");

	if (fsevents_paths) {
		CFRelease(fsevents_paths);
		fsevents_paths = NULL;
	}
	if (fsevents_queue) {
		dispatch_release(fsevents_queue);
		fsevents_queue = NULL;
	}

	return 0;
}
