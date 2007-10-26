/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <libkern/OSAtomic.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include "stack_logging.h"
#include "malloc_printf.h"
#include "_simple.h"		// as included by malloc.c, this defines ASL_LEVEL_INFO

#pragma mark -
#pragma mark Defines

#ifdef TEST_DISK_STACK_LOGGING
#define _malloc_printf fprintf
#undef ASL_LEVEL_INFO
#define ASL_LEVEL_INFO stderr
#endif

#define STACK_LOGGING_THREAD_HASH_SIZE 2048 // must be an even power of two
#define STACK_LOGGING_MAX_STACK_SIZE 512
#define STACK_LOGGING_BLOCK_WRITING_SIZE 8192
#define STACK_LOGGING_NUMBER_RECENT_BACKTRACES 50
#define STACK_LOGGING_FORCE_FULL_BACKTRACE_EVERY 100
#define STACK_LOGGING_MAX_THREAD_COLLISIONS 3
#define STACK_LOGGING_MIN_SAME_FRAMES 3
#define STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED 3
#define STACK_LOGGING_REMOTE_CACHE_DEFAULT_COLLISION_ALLOWANCE 5
#define STACK_LOGGING_REMOTE_CACHE_DEFAULT_NODE_CAPACITY 1 << 14 // <2mb for 32->32, ~3mb for (32->64 || 64->32), ~4mb for 64->64
#define STACK_LOGGING_REMOTE_CACHE_COLLISION_GROWTH_RATE 3
#define STACK_LOGGING_REMOTE_LINKS_PER_BLOCK (1 << 20) // this sets a maximum number of malloc/frees that can be read in to: 1^30;
							// this means if the .index file is >24gb, remote access will start to fail.
							// note: at this point, the .stack file will probably be ~56gb on top of that and
							// it'll also be using around 20 gb of memory in the analyzing process...
							// all of these are 64-bit stats; the 32-bit analyzing process limits are lower.
							// in short, if you want to analyze a process making > 1 billion malloc/frees 
							// (after compaction), bump this number slightly.

#pragma mark -
#pragma mark Macros

#define STACK_LOGGING_FLAGS(longlongvar) (uint8_t)((uint64_t)(longlongvar) >> 56)
#define STACK_LOGGING_OFFSET(longlongvar) ((longlongvar) & 0x00FFFFFFFFFFFFFFull)
#define STACK_LOGGING_OFFSET_AND_FLAGS(longlongvar, realshortvar) (((uint64_t)(longlongvar) & 0x00FFFFFFFFFFFFFFull) | ((uint64_t)(realshortvar) << 56))

#pragma mark -
#pragma mark Types

#pragma mark - stack_logging_backtrace_event
typedef struct {
	int16_t		offset_delta; // may want to expand this one; should always be < 0.
	uint16_t	num_identical_frames;
	uint16_t	num_new_hot_frames; // count of backtrace[]
} stack_logging_backtrace_event;

#pragma mark - stack_logging_index_event
typedef struct {
	uintptr_t	argument;
	uintptr_t	address;
	uint64_t	offset_and_flags; // top 8 bits are actually the flags!
} stack_logging_index_event;

#pragma mark - stack_logging_index_event32
typedef struct {
	uint32_t	argument;
	uint32_t	address;
	uint64_t	offset_and_flags; // top 8 bits are actually the flags!
} stack_logging_index_event32;

#pragma mark - stack_logging_index_event64
typedef struct {
	uint64_t	argument;
	uint64_t	address;
	uint64_t	offset_and_flags; // top 8 bits are actually the flags!
} stack_logging_index_event64;

#pragma mark - thread_backtrace_history
// for management of previous backtraces (by thread):
typedef struct {
	vm_address_t		thread;
	uint32_t			hash_pos;
	uint64_t		logging_index;
	int64_t			logging_offset;
	uint32_t		full_backtrace_countdown;
	uint32_t		backtrace_length;
	uintptr_t		*backtrace;
} thread_backtrace_history;

#pragma mark - stack_buffer_shared_memory
// for storing/looking up allocations that haven't yet be written to disk; consistent size across 32/64-bit processes.
// It's important that these fields don't change alignment due to the architecture because they may be accessed from an
// analyzing process with a different arch - hence the pragmas.
#pragma pack(push,4)
typedef struct {
	uint64_t		start_index_offset;
	uint64_t		start_stack_offset;
	uint32_t		next_free_index_buffer_offset;
	uint32_t		next_free_stack_buffer_offset;
	char			index_buffer[STACK_LOGGING_BLOCK_WRITING_SIZE];
	char			stack_buffer[STACK_LOGGING_BLOCK_WRITING_SIZE];
} stack_buffer_shared_memory;
#pragma pack(pop)

#pragma mark - index_ll_node
// linked-list node in table for allocations of a single address
typedef struct index_ll_node {
	struct index_ll_node	*next;
	uint64_t		index_file_offset;
} index_ll_node;

#pragma mark - remote_index_node32
// 32-bit target process address slot in table
typedef struct {
	uint32_t		address;
	index_ll_node		*linked_list;
	index_ll_node		*last_link;
} remote_index_node32;

#pragma mark - remote_index_node64
// 64-bit target process variant
typedef struct {
	uint64_t		address;
	index_ll_node		*linked_list;
	index_ll_node		*last_link;
} remote_index_node64;

#pragma mark - remote_index_cache
// for caching index information client-side:
typedef struct {
	size_t			cache_size;
	size_t			cache_node_capacity;
	uint32_t		collision_allowance;
	uint64_t		cache_node_count; // Debug only.
	uint64_t		cache_llnode_count; // Debug only.
	size_t			in_use_node_size; // sizeof(remote_index_node32) || sizeof(remote_index_node64)
	void			*table_memory; // this can be malloced; it's on the client side.
	remote_index_node32	*casted_table32; // represents table memory as 32-bit.
	remote_index_node64	*casted_table64; // ditto, 64-bit
	stack_buffer_shared_memory *shmem; // shared memory
	stack_buffer_shared_memory snapshot; // memory snapshot of the remote process' shared memory
	uint32_t		last_pre_written_index_size;
	uint64_t		last_index_file_offset;
	index_ll_node		*blocks[1024];
	uint32_t		current_block;
	uint32_t		next_block_index;
} remote_index_cache;

#pragma mark - remote_task_file_streams
// for reading stack history information from remote processes:
typedef struct {
	task_t			remote_task;
	pid_t			remote_pid;
	int32_t			task_is_64_bit;
	int32_t			in_use_count;
	FILE			*index_file_stream;
	FILE			*stack_file_stream;
	remote_index_cache	*cache;
} remote_task_file_streams;

#pragma mark -
#pragma mark Constants

static stack_buffer_shared_memory *pre_write_buffers;
static char *pre_write_backtrace_event_buffer = NULL;
static char *pre_write_index_buffer = NULL;

static OSSpinLock stack_logging_lock = OS_SPINLOCK_INIT;
static uint64_t current_logging_index = 0;
static int64_t total_offset = 0;

// single-thread access variables
static vm_address_t stack_buffer[STACK_LOGGING_NUMBER_RECENT_BACKTRACES][STACK_LOGGING_MAX_STACK_SIZE];
static thread_backtrace_history thread_buffer[STACK_LOGGING_THREAD_HASH_SIZE];
static int32_t current_stack_buffer = 0;
static uintptr_t last_logged_malloc_address = 0;
static uint32_t last_logged_backtrace_offset_diff = 0;
static thread_backtrace_history compaction_saved_differencing_history;

// Constants to define stack logging directory and path names.
// Files will get written to /tmp/stack-logs.<pid>.<progname>.XXXXXX/stack-logs.{index,stacks}
// The directory is securely created with mkdtemp() and the files inside it just have static names for simplicity.
static const char *temporary_directory = "/tmp";
static const char *stack_logging_directory_base_name = "stack-logs.";
static const char *index_file_name = "stack-logs.index";
static const char *stack_file_name = "stack-logs.stacks";

static char stack_logs_directory[PATH_MAX];
static char index_file_path[PATH_MAX];
static char stack_file_path[PATH_MAX];
static int index_file_descriptor = -1;
static int stack_file_descriptor = -1;

// for accessing remote log files
static remote_task_file_streams remote_fds[STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED];
static uint32_t next_remote_task_fd = 0;
static uint32_t remote_task_fd_count = 0;
static OSSpinLock remote_fd_list_lock = OS_SPINLOCK_INIT;

// activation variables

static int logging_use_compaction = 1; // set this to zero to always disable compaction.

// We set malloc_logger to NULL to disable logging, if we encounter errors
// during file writing
typedef void (malloc_logger_t)(uint32_t type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, uint32_t num_hot_frames_to_skip);
extern malloc_logger_t *malloc_logger;

#pragma mark -
#pragma mark Disk Stack Logging

static void delete_log_files(void);		// pre-declare

static void
append_int(char * filename, pid_t pid)
{
	unsigned int value;
	size_t len;
	unsigned int i;
	unsigned int count;
	
	len = strlen(filename);
	
	count = 0;
	value = pid;
	while (value > 0) {
		value /= 10;
		count ++;
	}
	
	filename[len + count] = 0;
	
	value = pid;
	for(i = 0 ; i < count ; i ++) {
		filename[len + count - 1 - i] = '0' + value % 10;
		value /= 10;
	}
}

// If successful, returns path to directory that was created.  Otherwise returns NULL.
static char *
create_log_files(void)
{
	pid_t pid = getpid();
	const char *progname = getprogname();
	char path_name[PATH_MAX];
	char *created_directory = NULL;

	// WARNING! use of snprintf can induce malloc() calls 
	strlcpy(stack_logs_directory, temporary_directory, PATH_MAX);
	strlcat(stack_logs_directory, "/", PATH_MAX);
	strlcat(stack_logs_directory, stack_logging_directory_base_name, PATH_MAX);
	append_int(stack_logs_directory, pid);
	if (progname && progname[0] != '\0') {
		strlcat(stack_logs_directory, ".", PATH_MAX);
		strlcat(stack_logs_directory, progname, PATH_MAX);
	}
	strlcat(stack_logs_directory, ".XXXXXX", PATH_MAX);

	// Securely make temporary directory for the log files, then create the files.
	if (mkdtemp(stack_logs_directory) == stack_logs_directory) {
		strlcpy(path_name, stack_logs_directory, PATH_MAX);
		strlcat(path_name, "/", PATH_MAX);
		strlcat(path_name, index_file_name, PATH_MAX);
		strlcpy(index_file_path, path_name, PATH_MAX);
		index_file_descriptor = open(path_name, O_WRONLY | O_TRUNC | O_CREAT, 0600);

		strlcpy(path_name, stack_logs_directory, PATH_MAX);
		strlcat(path_name, "/", PATH_MAX);
		strlcat(path_name, stack_file_name, PATH_MAX);
		strlcpy(stack_file_path, path_name, PATH_MAX);
		stack_file_descriptor = open(path_name, O_WRONLY | O_TRUNC | O_CREAT, 0600);

		if (index_file_descriptor == -1 || stack_file_descriptor == -1) {
			_malloc_printf(ASL_LEVEL_INFO, "unable to create stack log files in directory %s\n", stack_logs_directory);
			delete_log_files();
			created_directory = NULL;
		} else {
			_malloc_printf(ASL_LEVEL_INFO, "stack logs being written into %s\n", stack_logs_directory);
			created_directory = stack_logs_directory;
		}
	} else {
		_malloc_printf(ASL_LEVEL_INFO, "unable to create stack log directory %s\n", stack_logs_directory);
		created_directory = NULL;
	}
	return created_directory;
}

// This function may be called from either the target process when exiting, or from either the the target process or 
// a stack log analysis process, when reaping orphaned stack log files.
// Returns -1 if the files exist and they couldn't be removed, returns 0 otherwise.
static int
delete_log_files_in_directory(char *logdir)
{
	char path_name[PATH_MAX];
	int unlink_count = 0;
	int failure_count = 0;
	struct stat statbuf;

	if (logdir == NULL || logdir[0] == '\0') return 0;

	strlcpy(path_name, logdir, PATH_MAX);
	strlcat(path_name, "/", PATH_MAX);
	strlcat(path_name, index_file_name, PATH_MAX);
	if (unlink(path_name) == 0) {
		unlink_count++;
	} else if (stat(path_name, &statbuf) == 0) {
		failure_count++;
	}

	strlcpy(path_name, logdir, PATH_MAX);
	strlcat(path_name, "/", PATH_MAX);
	strlcat(path_name, stack_file_name, PATH_MAX);
	if (unlink(path_name) == 0) {
		unlink_count++;
	} else if (stat(path_name, &statbuf) == 0) {
		failure_count++;
	}

	if (rmdir(logdir) == -1) failure_count++;

	return (failure_count > 0) ? -1 : 0;
}

// This function will be called from atexit() in the target process.
static void
delete_log_files(void)
{
	if (stack_logs_directory == NULL || stack_logs_directory[0] == '\0') return;

	if (delete_log_files_in_directory(stack_logs_directory) == 0) {
		_malloc_printf(ASL_LEVEL_INFO, "stack logs deleted from %s\n", stack_logs_directory);
		stack_file_path[0] = '\0';
		index_file_path[0] = '\0';
	} else {
		_malloc_printf(ASL_LEVEL_INFO, "unable to delete stack logs from %s\n", stack_logs_directory);
	}
}

static bool
is_process_running(pid_t pid)
{
	struct kinfo_proc kpt[1];
	size_t size = sizeof(struct kinfo_proc);
	int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PID, pid};

	sysctl(mib, 4, kpt, &size, NULL, 0); // size is either 1 or 0 entries when we ask for a single pid
    
	return (size==sizeof(struct kinfo_proc));
}

// The log files can be quite large and aren't too useful after the process that created them no longer exists.
// Normally they should get removed when the process exits, but if the process crashed the log files might remain.
// So, reap any stack log files for processes that no longer exist.
//
// lf the remove_for_this_pid flag is set, then any log files that already exist for the current process will also be deleted.
// Those log files are probably the result of this process having been exec'ed from another one (without a fork()).
// The remove_for_this_pid flag is only set for a target process (one just starting logging);  a stack logging "client"
// process reaps log files too, but if we're using stack logging on the client process itself, then we don't want to remove
// its own log files.
static void
reap_orphaned_log_files(bool remove_for_this_pid)
{
	DIR *dp;
	struct dirent *entry;
	int prefix_length;
	char prefix_name[PATH_MAX];
	char pathname[PATH_MAX];
	pid_t current_pid = getpid();

	if ((dp = opendir(temporary_directory)) == NULL) {
		return;
	}

	strlcpy(prefix_name, stack_logging_directory_base_name, PATH_MAX);
	prefix_length = strlen(prefix_name);

	while ( (entry = readdir(dp)) != NULL ) {
		if ( entry->d_type == DT_DIR && ( strncmp( entry->d_name, prefix_name, prefix_length) == 0 ) ) {
			long pid = strtol(&entry->d_name[prefix_length], (char **)NULL, 10);
			if ( (! is_process_running(pid)) || (remove_for_this_pid && pid == current_pid) ) {
				strlcpy(pathname, temporary_directory, PATH_MAX);
				strlcat(pathname, "/", PATH_MAX);
				strlcat(pathname, entry->d_name, PATH_MAX);
				if (delete_log_files_in_directory(pathname) == 0) {
					if (remove_for_this_pid && pid == current_pid) {
						_malloc_printf(ASL_LEVEL_INFO, "stack logs deleted from %s\n", pathname);
					} else {
						_malloc_printf(ASL_LEVEL_INFO, "process %d no longer exists, stack logs deleted from %s\n", pid, pathname);
					}
				}
			}
		}
	}
	closedir(dp);
}	

/*
 * Since there a many errors that could cause stack logging to get disabled, this is a convenience method
 * for disabling any future logging in this process and for informing the user.
 */
static void
disable_stack_logging(void)
{	
	_malloc_printf(ASL_LEVEL_INFO, "stack logging disabled due to previous errors.\n");
	stack_logging_enable_logging = 0;
	malloc_logger = NULL;
}

/* A wrapper around write() that will try to reopen the index/stack file and
 * write to it if someone closed it underneath us (e.g. the process we just
 * started decide to close all file descriptors except stin/err/out). Some
 * programs like to do that and calling abort() on them is rude.
 */
static ssize_t
robust_write(int fd, const void *buf, size_t nbyte) {
	extern int errno;
	ssize_t written = write(fd, buf, nbyte);
	if (written == -1 && errno == EBADF) {
		char *file_to_reopen = NULL;
		int *fd_to_reset = NULL;

		// descriptor was closed on us. We need to reopen it
		if (fd == index_file_descriptor) {
			file_to_reopen = index_file_path;
			fd_to_reset = &index_file_descriptor;
		}
		else if (fd == stack_file_descriptor) {
			file_to_reopen = stack_file_path;
			fd_to_reset = &stack_file_descriptor;
		} else {
			// We don't know about this file. Return (and abort()).
			_malloc_printf(ASL_LEVEL_INFO, "Unknown file descriptor (it's neither the index file, nor the stacks file)\n");
			return -1;
		}

		// The file *should* already exist. If not, fail.
		fd = open(file_to_reopen, O_WRONLY | O_APPEND);
		if (fd < 3) {
			// If we somehow got stdin/out/err, we need to relinquish them and
			// get another fd.
			int fds_to_close[3] = { 0 };
			while (fd < 3) {
				if (fd == -1) {
					_malloc_printf(ASL_LEVEL_INFO, "unable to re-open stack log file %s\n", file_to_reopen);
					delete_log_files();
					return -1;
				}
				fds_to_close[fd] = 1;
				fd = dup(fd);
			}

			// We have an fd we like. Close the ones we opened.
			if (fds_to_close[0]) close(0);
			if (fds_to_close[1]) close(1);
			if (fds_to_close[2]) close(2);
		}

		*fd_to_reset = fd;
		written = write(fd, buf, nbyte);
	}
	return written;
}

static void
flush_data(void)
{
	ssize_t written; // signed size_t
	size_t remaining;
	char * p;
	
	if (index_file_descriptor == -1) {
		if (create_log_files() == NULL) {
			return;
		}
	}
	
	// Write the events before the index so that hopefully the events will be on disk if the index refers to them.
	p = pre_write_backtrace_event_buffer;
	remaining = (size_t)pre_write_buffers->next_free_stack_buffer_offset;
	while (remaining > 0) {
		written = robust_write(stack_file_descriptor, p, remaining);
		if (written == -1) {
			_malloc_printf(ASL_LEVEL_INFO, "Unable to write to stack logging file %s (%s)\n", stack_file_path, strerror(errno));
			disable_stack_logging();
			return;
		}
		p += written;
		remaining -= written;
	}
	p = pre_write_index_buffer;
	remaining = (size_t)pre_write_buffers->next_free_index_buffer_offset;
	while (remaining > 0) {
		written = robust_write(index_file_descriptor, p, remaining);
		if (written == -1) {
			_malloc_printf(ASL_LEVEL_INFO, "Unable to write to stack logging file %s (%s)\n", index_file_path, strerror(errno));
			disable_stack_logging();
			return;
		}
		p += written;
		remaining -= written;
	}
	
	pre_write_buffers->start_stack_offset += pre_write_buffers->next_free_stack_buffer_offset;
	pre_write_buffers->start_index_offset += pre_write_buffers->next_free_index_buffer_offset;
	pre_write_buffers->next_free_index_buffer_offset = pre_write_buffers->next_free_stack_buffer_offset = 0;
}

static void
prepare_to_log_stacks(void)
{
	if (!pre_write_buffers) {
		last_logged_malloc_address = 0ul;
		logging_use_compaction = (stack_logging_dontcompact ? 0 : logging_use_compaction);
		
		// Create a shared memory region to hold the pre-write index and stack buffers. This will allow remote analysis processes to access
		// these buffers to get logs for even the most recent allocations. The remote process will need to pause this process to assure that
		// the contents of these buffers don't change while being inspected.
		char shmem_name_string[PATH_MAX];
		strlcpy(shmem_name_string, stack_logging_directory_base_name, (size_t)PATH_MAX);
		append_int(shmem_name_string, getpid());
		
		int shmid = shm_open(shmem_name_string, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
		if (shmid < 0) {
			// Failed to create shared memory region; turn off stack logging.
			_malloc_printf(ASL_LEVEL_INFO, "error while allocating shared memory for disk-based stack logging output buffers\n");
			disable_stack_logging();
			return;
		}
		
		size_t full_shared_mem_size = sizeof(stack_buffer_shared_memory);
		ftruncate(shmid, (off_t)full_shared_mem_size);
		pre_write_buffers = (stack_buffer_shared_memory*)mmap(0, full_shared_mem_size, PROT_READ | PROT_WRITE, MAP_SHARED, shmid, (off_t)0);
		close(shmid);
		
		if (!pre_write_buffers) {
			_malloc_printf(ASL_LEVEL_INFO, "error mapping in shared memory for disk-based stack logging output buffers\n");
			disable_stack_logging();
			return;
		}
		
		// Store and use the buffer offsets in shared memory so that they can be accessed remotely
		pre_write_buffers->start_index_offset = pre_write_buffers->start_stack_offset = 0ull;
		pre_write_buffers->next_free_index_buffer_offset = pre_write_buffers->next_free_stack_buffer_offset = 0;
		pre_write_backtrace_event_buffer = pre_write_buffers->stack_buffer;
		pre_write_index_buffer = pre_write_buffers->index_buffer;
		
		// malloc() can be called by the following, so these need to be done outside the stack_logging_lock but after the buffers have been set up.
		atexit(delete_log_files);		// atexit() can call malloc()
		reap_orphaned_log_files(true);		// this calls opendir() which calls malloc()
		
		// this call to flush data ensures that the log files (while possibly empty) exist; analyzing processes will rely on this assumption.
		flush_data();
	}
}

void
__disk_stack_logging_log_stack(uint32_t type_flags, uintptr_t zone_ptr, uintptr_t size, uintptr_t ptr_arg, uintptr_t return_val, uint32_t num_hot_to_skip)
{
	if (!stack_logging_enable_logging) return;
	
	// check incoming data
	if (type_flags & stack_logging_type_alloc && type_flags & stack_logging_type_dealloc) {
		uintptr_t swapper = size;
		size = ptr_arg;
		ptr_arg = swapper;
		if (ptr_arg == return_val) return; // realloc had no effect, skipping
		
		if (ptr_arg == 0) { // realloc(NULL, size) same as malloc(size)
			type_flags ^= stack_logging_type_dealloc;
		} else {
			// realloc(arg1, arg2) -> result is same as free(arg1); malloc(arg2) -> result
			__disk_stack_logging_log_stack(stack_logging_type_dealloc, zone_ptr, ptr_arg, (uintptr_t)0, (uintptr_t)0, num_hot_to_skip + 1);
			__disk_stack_logging_log_stack(stack_logging_type_alloc, zone_ptr, size, (uintptr_t)0, return_val, num_hot_to_skip + 1);
			return;
		}
	}
	if (type_flags & stack_logging_type_dealloc) {
		if (size) {
			ptr_arg = size;
			size = 0;
		} else return; // free(nil)
	}
	if (type_flags & stack_logging_type_alloc && return_val == 0) return; // alloc that failed
	
	type_flags &= 0x7;
	
	// now actually begin
	prepare_to_log_stacks();

	// since there could have been a fatal (to stack logging) error such as the log files not being created, check this variable before continuing
	if (!stack_logging_enable_logging) return;
	vm_address_t self_thread = (vm_address_t)pthread_self();	// use pthread_self() rather than mach_thread_self() to avoid system call
	
	// lock and enter
	OSSpinLockLock(&stack_logging_lock);
	
	// compaction
	if (last_logged_malloc_address && (type_flags & stack_logging_type_dealloc) && STACK_LOGGING_DISGUISE(ptr_arg) == last_logged_malloc_address) {
		// *waves hand* the last allocation never occurred
		pre_write_buffers->next_free_index_buffer_offset -= (uint32_t)sizeof(stack_logging_index_event);
		pre_write_buffers->next_free_stack_buffer_offset -= last_logged_backtrace_offset_diff;
		total_offset -= (int64_t)last_logged_backtrace_offset_diff;
		last_logged_malloc_address = 0ul;
		
		// not going to subtract from the current_stack_buffer or current_logging_index indecies; 
		// there is no intention to restore the previously held stack. the differencing history
		// must be reset to its previous value, though.
		thread_buffer[compaction_saved_differencing_history.hash_pos] = compaction_saved_differencing_history;
		
		OSSpinLockUnlock(&stack_logging_lock);
		return;
	}
		
	// locate previous backtrace for this thread
	short difference = 1;
	
	uint32_t collisions = STACK_LOGGING_MAX_THREAD_COLLISIONS;
	uint32_t hashed_thread = self_thread & (STACK_LOGGING_THREAD_HASH_SIZE-1);
	while (thread_buffer[hashed_thread].thread && thread_buffer[hashed_thread].thread != self_thread) {
		if (--collisions == 0) {
			difference = 0;
			break;
		}
		hashed_thread++;
	}
	
	// gather stack
	uint32_t count;
	thread_stack_pcs(stack_buffer[current_stack_buffer], STACK_LOGGING_MAX_STACK_SIZE, &count);
	stack_buffer[current_stack_buffer][count++] = self_thread + 1;		// stuffing thread # in the coldest slot.  Add 1 to match what the old stack logging did.
	num_hot_to_skip += 2;
	if (count <= num_hot_to_skip) {
		// Oops!  Didn't get a valid backtrace from thread_stack_pcs().
		OSSpinLockUnlock(&stack_logging_lock);
		return;
        }
	
	// easy access variables
	thread_backtrace_history *historical = &thread_buffer[hashed_thread];
	vm_address_t *frames = stack_buffer[current_stack_buffer];
	
	// increment as necessary
	current_logging_index++;
	current_stack_buffer++;
	if (current_stack_buffer == STACK_LOGGING_NUMBER_RECENT_BACKTRACES) current_stack_buffer = 0;
	
	// difference (if possible)
	if (historical->logging_index + STACK_LOGGING_NUMBER_RECENT_BACKTRACES <= current_logging_index) difference = 0;
	else if (historical->full_backtrace_countdown == 0) difference = 0;
	
	uint32_t sameness = 0;
	if (difference) {
		uint32_t old_count = historical->backtrace_length;
		int32_t new_count = (int32_t)count;
		while (old_count-- && new_count-- > (int32_t)num_hot_to_skip) {
			if (historical->backtrace[old_count] == frames[new_count]) sameness++;
			else break;
		}
		
		if (sameness < STACK_LOGGING_MIN_SAME_FRAMES) { // failure; pretend nothing was the same
			difference = 0;
		}
	}
	
	// create events for byte storage
	count -= num_hot_to_skip;
	stack_logging_backtrace_event current_event;
	current_event.num_identical_frames = (difference ? sameness : 0);
	current_event.num_new_hot_frames = (difference ? count - sameness : count);
	current_event.offset_delta = (difference ? historical->logging_offset - total_offset : 0);
	int64_t this_offset_change = sizeof(stack_logging_backtrace_event) + (current_event.num_new_hot_frames * sizeof(uintptr_t));
	
	stack_logging_index_event current_index;
	if (type_flags & stack_logging_type_alloc) {
		current_index.address = STACK_LOGGING_DISGUISE(return_val);
		current_index.argument = size;
		if (logging_use_compaction) {
			last_logged_malloc_address = current_index.address; // disguised
			last_logged_backtrace_offset_diff = (uint32_t)this_offset_change;
			compaction_saved_differencing_history = *historical;
		}
	} else {
		current_index.address = STACK_LOGGING_DISGUISE(ptr_arg);
		current_index.argument = 0ul;
		last_logged_malloc_address = 0ul;
	}
	current_index.offset_and_flags = STACK_LOGGING_OFFSET_AND_FLAGS(total_offset, type_flags);
	
	// prepare for differencing next time
	historical->backtrace = (uintptr_t*)(frames + num_hot_to_skip);
	historical->backtrace_length = count;
	if (difference) historical->full_backtrace_countdown--;
	else historical->full_backtrace_countdown = STACK_LOGGING_FORCE_FULL_BACKTRACE_EVERY;
	historical->logging_index = current_logging_index;
	historical->logging_offset = total_offset;
	historical->thread = self_thread;
	historical->hash_pos = hashed_thread;
	
	// flush the data buffer to disk if necessary
	if (pre_write_buffers->next_free_stack_buffer_offset + this_offset_change >= STACK_LOGGING_BLOCK_WRITING_SIZE) {
		flush_data();
	} else if (pre_write_buffers->next_free_index_buffer_offset + sizeof(stack_logging_index_event) >= STACK_LOGGING_BLOCK_WRITING_SIZE) {
		flush_data();
	}
	
	// store bytes in buffers
	memcpy(pre_write_index_buffer+pre_write_buffers->next_free_index_buffer_offset, &current_index, sizeof(stack_logging_index_event));
	memcpy(pre_write_backtrace_event_buffer+pre_write_buffers->next_free_stack_buffer_offset, &current_event, sizeof(stack_logging_backtrace_event));
	memcpy(pre_write_backtrace_event_buffer+pre_write_buffers->next_free_stack_buffer_offset+sizeof(stack_logging_backtrace_event), frames+num_hot_to_skip, (size_t)this_offset_change - sizeof(stack_logging_backtrace_event));
	pre_write_buffers->next_free_index_buffer_offset += (uint32_t)sizeof(stack_logging_index_event);
	pre_write_buffers->next_free_stack_buffer_offset += (uint32_t)this_offset_change;
	total_offset += this_offset_change;
	
	OSSpinLockUnlock(&stack_logging_lock);
}

#pragma mark -
#pragma mark Remote Stack Log Access

#pragma mark - Design notes:

/*

this first one will look through the index, find the "stack_identifier" (i.e. the offset in the log file), and call the third function listed here.
extern kern_return_t __mach_stack_logging_get_frames(task_t task, mach_vm_address_t address, mach_vm_address_t *stack_frames_buffer, uint32_t max_stack_frames, uint32_t *num_frames);
    //  Gets the last allocation record about address
	
if !address, will load both index and stack logs and iterate through (expensive)
else will load just index, search for stack, and then use third function here to retrieve. (also expensive)
extern kern_return_t __mach_stack_logging_enumerate_records(task_t task, mach_vm_address_t address, void enumerator(mach_stack_logging_record_t, void *), void *context);
    // Applies enumerator to all records involving address sending context as enumerator's second parameter; if !address, applies enumerator to all records 

this function will load the stack file, look for the stack, and follow up to STACK_LOGGING_FORCE_FULL_BACKTRACE_EVERY references to reconstruct.
extern kern_return_t __mach_stack_logging_frames_for_uniqued_stack(task_t task, uint64_t stack_identifier, mach_vm_address_t *stack_frames_buffer, uint32_t max_stack_frames, uint32_t *count);
    // Given a uniqued_stack fills stack_frames_buffer

*/

#pragma mark - caching

static inline size_t hash_index_32(uint32_t address, size_t max_pos) __attribute__((always_inline));
static inline size_t hash_index_32(uint32_t address, size_t max_pos) {
//	return (((OSSwapInt32(address >> 2) << 3) & 0x96AAAA98) ^ (address >> 2)) % (max_pos-1);
	return (address >> 2) % (max_pos-1); // simplicity rules.
}

static inline size_t hash_index_64(uint64_t address, size_t max_pos) __attribute__((always_inline));
static inline size_t hash_index_64(uint64_t address, size_t max_pos) {
//	return (size_t)((((OSSwapInt64(address >> 3) << 2) & 0x54AA0A0AAA54ull) ^ (address >> 3)) % (max_pos - 1));
	return (size_t)((address >> 3) % (max_pos-1)); // simplicity rules.
}

static void
transfer_node_ll32(remote_index_cache *cache, remote_index_node32 *old_node)
{
	uint32_t collisions = 0;
	size_t pos = hash_index_32(old_node->address, cache->cache_node_capacity);
	do {
		if (cache->casted_table32[pos].address == old_node->address) { // hit like this shouldn't happen.
			fprintf(stderr, "impossible collision! two address==address lists! (transfer_node_ll32)\n");
			break;
		} else if (cache->casted_table32[pos].address == 0) { // empty
			cache->casted_table32[pos] = *old_node;
			break;
		} else {
			pos++;
			if (pos >= cache->cache_node_capacity) pos = 0;
		}
		collisions++;
	} while (collisions <= cache->collision_allowance);
	
	if (collisions > cache->collision_allowance) {
		fprintf(stderr, "reporting bad hash function! disk stack logging reader %lu bit. (transfer_node_ll32)\n", sizeof(void*)*8);
	}
}

static void
transfer_node_ll64(remote_index_cache *cache, remote_index_node64 *old_node)
{
	uint32_t collisions = 0;
	size_t pos = hash_index_64(old_node->address, cache->cache_node_capacity);
	do {
		if (cache->casted_table64[pos].address == old_node->address) { // hit!
			fprintf(stderr, "impossible collision! two address==address lists! (transfer_node_ll64)\n");
			break;
		} else if (cache->casted_table64[pos].address == 0) { // empty
			cache->casted_table64[pos] = *old_node;
			break;
		} else {
			pos++;
			if (pos >= cache->cache_node_capacity) pos = 0;
		}
		collisions++;
	} while (collisions <= cache->collision_allowance);
	
	if (collisions > cache->collision_allowance) {
		fprintf(stderr, "reporting bad hash function! disk stack logging reader %lu bit. (transfer_node_ll64)\n", sizeof(void*)*8);
	}
}

static void
expand_cache(remote_index_cache *cache)
{
	// keep old stats
	size_t old_node_capacity = cache->cache_node_capacity;
	uint64_t old_node_count = cache->cache_node_count;
	uint64_t old_llnode_count = cache->cache_llnode_count;
	void *old_table = cache->table_memory;
	
	// double size
	cache->cache_size <<= 1;
	cache->cache_node_capacity <<= 1;
	cache->collision_allowance += STACK_LOGGING_REMOTE_CACHE_COLLISION_GROWTH_RATE;
	cache->table_memory = (void*)calloc(cache->cache_node_capacity, cache->in_use_node_size);
	if (cache->casted_table32) cache->casted_table32 = cache->table_memory;
	else cache->casted_table64 = cache->table_memory;
	
	// repopulate (expensive!)
	size_t i;
	if (cache->casted_table32) { // if target is 32-bit
		remote_index_node32 *casted_old_table = (remote_index_node32*)old_table;
		for (i = 0; i < old_node_capacity; i++) {
			if (casted_old_table[i].address) {
				transfer_node_ll32(cache, &casted_old_table[i]);
			}
		}
	} else {
		remote_index_node64 *casted_old_table = (remote_index_node64*)old_table;
		for (i = 0; i < old_node_capacity; i++) {
			if (casted_old_table[i].address) {
				transfer_node_ll64(cache, &casted_old_table[i]);
			}
		}
	}
	
	cache->cache_node_count = old_node_count;
	cache->cache_llnode_count = old_llnode_count;
	free(old_table);
//	printf("cache expanded to %0.2f mb (eff: %3.0f%%, capacity: %lu, nodes: %llu, llnodes: %llu)\n", ((float)(cache->cache_size))/(1 << 20), ((float)(cache->cache_node_count)*100.0)/((float)(cache->cache_node_capacity)), cache->cache_node_capacity, cache->cache_node_count, cache->cache_llnode_count);
}

static void
insert_node32(remote_index_cache *cache, uint32_t address, uint64_t index_file_offset)
{
	uint32_t collisions = 0;
	size_t pos = hash_index_32(address, cache->cache_node_capacity);
	
	if (cache->next_block_index >= STACK_LOGGING_REMOTE_LINKS_PER_BLOCK) {
		cache->next_block_index = 0;
		cache->current_block++;
		cache->blocks[cache->current_block] = (index_ll_node*)malloc(STACK_LOGGING_REMOTE_LINKS_PER_BLOCK*sizeof(index_ll_node));
/*		printf("node buffer added. total nodes: %ul (%u buffers, %0.2f mb)\n", STACK_LOGGING_REMOTE_LINKS_PER_BLOCK*(cache->current_block+1),
				cache->current_block+1, ((float)(STACK_LOGGING_REMOTE_LINKS_PER_BLOCK*sizeof(index_ll_node)*(cache->current_block+1)))/(1 << 20));
*/
	}
	index_ll_node *new_node = &cache->blocks[cache->current_block][cache->next_block_index++];
	new_node->index_file_offset = index_file_offset;
	new_node->next = NULL;
	
	bool inserted = false;
	while (!inserted) {
		if (cache->casted_table32[pos].address == address) { // hit!
			cache->casted_table32[pos].last_link->next = new_node; // insert at end
			cache->casted_table32[pos].last_link = new_node;
			inserted = true;
			break;
		} else if (cache->casted_table32[pos].address == 0) { // empty
			cache->casted_table32[pos].address = address;
			cache->casted_table32[pos].linked_list = new_node;
			cache->casted_table32[pos].last_link = new_node;
			cache->cache_node_count++;
			inserted = true;
			break;
		} else {
			pos++;
			if (pos >= cache->cache_node_capacity) pos = 0;
		}
		collisions++;
		if (collisions > cache->collision_allowance) {
			expand_cache(cache);
			pos = hash_index_32(address, cache->cache_node_capacity);
			collisions = 0;
		}
	}
	
	cache->cache_llnode_count++;
	
}

static void
insert_node64(remote_index_cache *cache, uint64_t address, uint64_t index_file_offset)
{
	uint32_t collisions = 0;
	size_t pos = hash_index_64(address, cache->cache_node_capacity);
	
	if (cache->next_block_index >= STACK_LOGGING_REMOTE_LINKS_PER_BLOCK) {
		cache->next_block_index = 0;
		cache->current_block++;
		cache->blocks[cache->current_block] = (index_ll_node*)malloc(STACK_LOGGING_REMOTE_LINKS_PER_BLOCK*sizeof(index_ll_node));
	}
	index_ll_node *new_node = &cache->blocks[cache->current_block][cache->next_block_index++];
	new_node->index_file_offset = index_file_offset;
	new_node->next = NULL;
	
	bool inserted = false;
	while (!inserted) {
		if (cache->casted_table64[pos].address == address) { // hit!
			cache->casted_table64[pos].last_link->next = new_node; // insert at end
			cache->casted_table64[pos].last_link = new_node;
			inserted = true;
			break;
		} else if (cache->casted_table64[pos].address == 0) { // empty
			cache->casted_table64[pos].address = address;
			cache->casted_table64[pos].linked_list = new_node;
			cache->casted_table64[pos].last_link = new_node;
			inserted = true;
			break;
		} else {
			pos++;
			if (pos >= cache->cache_node_capacity) pos = 0;
		}
		collisions++;
		if (collisions > cache->collision_allowance) {
			expand_cache(cache);
			pos = hash_index_64(address, cache->cache_node_capacity);
			collisions = 0;
		}
	}
	
}

static void
update_cache_for_file_streams(remote_task_file_streams *descriptors)
{
	remote_index_cache *cache = descriptors->cache;
	
	// create from scratch if necessary.
	if (!cache) {
		descriptors->cache = cache = (remote_index_cache*)calloc((size_t)1, sizeof(remote_index_cache));
		cache->cache_node_capacity = STACK_LOGGING_REMOTE_CACHE_DEFAULT_NODE_CAPACITY;
		cache->collision_allowance = STACK_LOGGING_REMOTE_CACHE_DEFAULT_COLLISION_ALLOWANCE;
		cache->cache_node_count = cache->cache_llnode_count = 0;
		cache->last_index_file_offset = 0;
		cache->next_block_index = 0;
		cache->current_block = 0;
		cache->blocks[0] = (index_ll_node*)malloc(STACK_LOGGING_REMOTE_LINKS_PER_BLOCK*sizeof(index_ll_node));
		cache->in_use_node_size = (descriptors->task_is_64_bit ? sizeof(remote_index_node64) : sizeof(remote_index_node32));
		cache->cache_size = cache->cache_node_capacity*cache->in_use_node_size;
		cache->table_memory = (void*)calloc(cache->cache_node_capacity, cache->in_use_node_size);
		if (descriptors->task_is_64_bit) cache->casted_table64 = (remote_index_node64*)(cache->table_memory);
		else cache->casted_table32 = (remote_index_node32*)(cache->table_memory);
		
		// now map in the shared memory, if possible
		char shmem_name_string[PATH_MAX];
		strlcpy(shmem_name_string, stack_logging_directory_base_name, (size_t)PATH_MAX);
		append_int(shmem_name_string, descriptors->remote_pid);
		
		int shmid = shm_open(shmem_name_string, O_RDWR, S_IRUSR | S_IWUSR);
		if (shmid >= 0) {
			cache->shmem = mmap(0, sizeof(stack_buffer_shared_memory), PROT_READ | PROT_WRITE, MAP_SHARED, shmid, (off_t)0);
			close(shmid);
		}
		
		if (shmid < 0 || cache->shmem == NULL) {
			// failed to connect to the shared memory region; warn and continue.
			_malloc_printf(ASL_LEVEL_INFO, "warning: unable to connect to remote process' shared memory; allocation histories may not be up-to-date.\n");
		}
	}
	
	// suspend and see how much updating there is to do. there are three scenarios, listed below
	bool update_snapshot = false;
	if (descriptors->remote_task != mach_task_self()) {
		task_suspend(descriptors->remote_task);
	}
		
	struct stat file_statistics;
	fstat(fileno(descriptors->index_file_stream), &file_statistics);
	size_t read_size = (descriptors->task_is_64_bit ? sizeof(stack_logging_index_event64) : sizeof(stack_logging_index_event32));
	uint64_t read_this_update = 0;
	
	// the delta indecies is a complex number; there are three cases:
	// 1. there is no shared memory (or we can't connect); diff the last_index_file_offset from the filesize.
	// 2. the only updates have been in shared memory; disk file didn't change at all. delta_indecies should be zero, scan snapshot only.
	// 3. the updates have flushed to disk, meaning that most likely there is new data on disk that wasn't read from shared memory.
	//    correct delta_indecies for the pre-scanned amount and read the new data from disk and shmem.
	uint64_t delta_indecies = (file_statistics.st_size - cache->last_index_file_offset) / read_size;
	uint32_t last_snapshot_scan_index = 0;
	if (delta_indecies && cache->shmem) { 
		// case 3: add cache scanned to known from disk and recalc
		cache->last_index_file_offset += cache->snapshot.next_free_index_buffer_offset;
		delta_indecies = (file_statistics.st_size - cache->last_index_file_offset) / read_size;
		update_snapshot = true;
	} else if (cache->shmem) {
		// case 2: set the last snapshot scan count so we don't rescan something we've seen.
		last_snapshot_scan_index = cache->snapshot.next_free_index_buffer_offset / (uint32_t)read_size;
	}
	
	// no update necessary for the file; check if need a snapshot.
	if (delta_indecies == 0) {
		if (cache->shmem && !update_snapshot) {
			update_snapshot = (cache->shmem->next_free_index_buffer_offset != cache->snapshot.next_free_index_buffer_offset);
		}
	}
	
	// if a snapshot is necessary, memcpy from remote frozen process' memory 
	// note: there were two ways to do this Ð spin lock or suspend. suspend allows us to
	// analyze processes even if they were artificially suspended. with a lock, there'd be
	// worry that the target was suspended with the lock taken.
	if (update_snapshot) {
		memcpy(&cache->snapshot, cache->shmem, sizeof(stack_buffer_shared_memory));
	}
	
	// resume
	if (descriptors->remote_task != mach_task_self()) {
		task_resume(descriptors->remote_task);
	}
	
	if (!update_snapshot && delta_indecies == 0) return; // absolutely no updating needed.
	
	FILE *the_index = (descriptors->index_file_stream);
	
	// prepare for the read; target process could be 32 or 64 bit.

	stack_logging_index_event32 *target_32_index = NULL;
	stack_logging_index_event64 *target_64_index = NULL;
	
	// perform the update from the file
	uint32_t i;
	if (delta_indecies) {
		char bufferSpace[4096]; // 4 kb
		target_32_index = (stack_logging_index_event32*)bufferSpace;
		target_64_index = (stack_logging_index_event64*)bufferSpace;
		size_t number_slots = (size_t)(4096/read_size);
	
		size_t read_count = 0;
		if (fseeko(the_index, (off_t)(cache->last_index_file_offset), SEEK_SET)) {
			fprintf(stderr, "error while attempting to cache information from remote stack index file. (update_cache_for_file_streams)\n");
		}
		off_t current_index_position = cache->last_index_file_offset;
		do {
			number_slots = MIN(delta_indecies - read_this_update, number_slots);
			read_count = fread(bufferSpace, read_size, number_slots, the_index);
			if (descriptors->task_is_64_bit) {
				for (i = 0; i < read_count; i++) {
					insert_node64(cache, STACK_LOGGING_DISGUISE(target_64_index[i].address), (uint64_t)current_index_position);
					read_this_update++;
					current_index_position += read_size;
				}
			} else {
				for (i = 0; i < read_count; i++) {
					insert_node32(cache, STACK_LOGGING_DISGUISE(target_32_index[i].address), (uint64_t)current_index_position);
					read_this_update++;
					current_index_position += read_size;
				}
			}
		} while (read_count);

		if (read_this_update < delta_indecies) {
			fprintf(stderr, "insufficient data in remote stack index file; expected more records.\n");
		}
		cache->last_index_file_offset += read_this_update * read_size;
	}
	
	if (update_snapshot) {
		target_32_index = (stack_logging_index_event32*)(cache->snapshot.index_buffer);
		target_64_index = (stack_logging_index_event64*)(cache->snapshot.index_buffer);
		
		uint32_t free_snapshot_scan_index = cache->snapshot.next_free_index_buffer_offset / (uint32_t)read_size;
		off_t current_index_position = cache->snapshot.start_index_offset;
		if (descriptors->task_is_64_bit) {
			for (i = last_snapshot_scan_index; i < free_snapshot_scan_index; i++) {
				insert_node64(cache, STACK_LOGGING_DISGUISE(target_64_index[i].address), (uint64_t)(current_index_position + (i * read_size))); 
			}
		} else {
			for (i = last_snapshot_scan_index; i < free_snapshot_scan_index; i++) {
				insert_node32(cache, STACK_LOGGING_DISGUISE(target_32_index[i].address), (uint64_t)(current_index_position + (i * read_size)));
			}
		}
	}
	
}

static void
destroy_cache_for_file_streams(remote_task_file_streams *descriptors)
{
	uint32_t i;
	for (i = 0; i <= descriptors->cache->current_block; i++) {
		free(descriptors->cache->blocks[i]); // clears the linked list nodes.
	}
	if (descriptors->cache->shmem) {
		munmap(descriptors->cache->shmem, sizeof(stack_buffer_shared_memory));
	}
	free(descriptors->cache->table_memory);
	free(descriptors->cache);
	descriptors->cache = NULL;
}

#pragma mark - internal

// In the stack log analysis process, find the stack logging files for target process <pid>
// by scanning the temporary directory for directory entries with names of the form "stack-logs.<pid>."
// If we find such a directory then open the stack logging files in there.
static void
open_log_files(pid_t pid, remote_task_file_streams *this_task_streams)
{
	DIR *dp;
	struct dirent *entry;
	int prefix_length;
	char prefix_name[PATH_MAX];
	char pathname[PATH_MAX];

	reap_orphaned_log_files(false);		// reap any left-over log files (for non-existant processes, but not for this analysis process)

	if ((dp = opendir(temporary_directory)) == NULL) {
		return;
	}

	// It's OK to use snprintf in this routine since it should only be called by the clients
	// of stack logging, and thus calls to malloc are OK.
	snprintf(prefix_name, PATH_MAX, "%s%d.", stack_logging_directory_base_name, pid);	// make sure to use "%s%d." rather than just "%s%d" to match the whole pid
	prefix_length = strlen(prefix_name);

	while ( (entry = readdir(dp)) != NULL ) {
		if ( strncmp( entry->d_name, prefix_name, prefix_length) == 0 ) {
			snprintf(pathname, PATH_MAX, "%s/%s/%s", temporary_directory, entry->d_name, index_file_name);
			this_task_streams->index_file_stream = fopen(pathname, "r");

			snprintf(pathname, PATH_MAX, "%s/%s/%s", temporary_directory, entry->d_name, stack_file_name);
			this_task_streams->stack_file_stream = fopen(pathname, "r");

			break;
		}
	}
	closedir(dp);
}	

static remote_task_file_streams*
retain_file_streams_for_task(task_t task)
{
	OSSpinLockLock(&remote_fd_list_lock);
	
	// see if they're already in use
	uint32_t i = 0;
	for (i = 0; i < remote_task_fd_count; i++) {
		if (remote_fds[i].remote_task == task) {
			remote_fds[i].in_use_count++;
			OSSpinLockUnlock(&remote_fd_list_lock);
			return &remote_fds[i];
		}
	}
	
	// open them
	uint32_t failures = 0;
	if (remote_task_fd_count == STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED) {
		while (remote_fds[next_remote_task_fd].in_use_count > 0) {
			next_remote_task_fd++;
			if (next_remote_task_fd == STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED) next_remote_task_fd = 0;
			failures++;
			if (failures >= STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED) {
				OSSpinLockUnlock(&remote_fd_list_lock);
				return NULL;
			}
		}
		fclose(remote_fds[next_remote_task_fd].index_file_stream);
		fclose(remote_fds[next_remote_task_fd].stack_file_stream);
		destroy_cache_for_file_streams(&remote_fds[next_remote_task_fd]);
	}
	
	pid_t pid;
	kern_return_t err = pid_for_task(task, &pid);
	if (err != KERN_SUCCESS) {
		OSSpinLockUnlock(&remote_fd_list_lock);
		return NULL;
	}
	
	remote_task_file_streams *this_task_streams = &remote_fds[next_remote_task_fd];
	
	open_log_files(pid, this_task_streams);

	// check if opens failed
	if (this_task_streams->index_file_stream == NULL || this_task_streams->stack_file_stream == NULL) {
		if (this_task_streams->index_file_stream) fclose(this_task_streams->index_file_stream);
		if (this_task_streams->stack_file_stream) fclose(this_task_streams->stack_file_stream);
		OSSpinLockUnlock(&remote_fd_list_lock);
		return NULL;
	}
	
	// check if target pid is running 64-bit
	int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid };
	struct kinfo_proc processInfo;
	size_t bufsize = sizeof(processInfo);
	if (sysctl(mib, (unsigned)(sizeof(mib)/sizeof(int)), &processInfo, &bufsize, NULL, (size_t)0) == 0 && bufsize > 0) {
		this_task_streams->task_is_64_bit = processInfo.kp_proc.p_flag & P_LP64;
	} else {
		this_task_streams->task_is_64_bit = 0;
	}
	
	// otherwise set vars and go
	this_task_streams->in_use_count = 1;
	this_task_streams->remote_task = task;
	this_task_streams->remote_pid = pid;
	next_remote_task_fd++;
	if (next_remote_task_fd == STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED) next_remote_task_fd = 0;
	remote_task_fd_count = MIN(remote_task_fd_count + 1, STACK_LOGGING_MAX_SIMUL_REMOTE_TASKS_INSPECTED);
	
	OSSpinLockUnlock(&remote_fd_list_lock);
	return this_task_streams;
}

static void
release_file_streams_for_task(task_t task)
{
	OSSpinLockLock(&remote_fd_list_lock);
	
	// decrement in-use count
	uint32_t i = 0;
	for (i = 0; i < remote_task_fd_count; i++) {
		if (remote_fds[i].remote_task == task) {
			remote_fds[i].in_use_count--;
			break;
		}
	}
	
	OSSpinLockUnlock(&remote_fd_list_lock);
}

#pragma mark - extern

kern_return_t
__mach_stack_logging_get_frames(task_t task, mach_vm_address_t address, mach_vm_address_t *stack_frames_buffer, uint32_t max_stack_frames, uint32_t *count)
{
	remote_task_file_streams *remote_fd = retain_file_streams_for_task(task);
	if (remote_fd == NULL) {
		return KERN_FAILURE;
	}
	
	update_cache_for_file_streams(remote_fd);
	
	uint32_t collisions = 0;
	uint64_t located_file_position = 0;
	bool found = false;
	size_t hash = 0;
	if (remote_fd->task_is_64_bit) {
		hash = hash_index_64(address, remote_fd->cache->cache_node_capacity);
		do {
			if (remote_fd->cache->casted_table64[hash].address == address) { // hit!
				located_file_position = remote_fd->cache->casted_table64[hash].last_link->index_file_offset;
				found = true;
				break;
			} else if (remote_fd->cache->casted_table64[hash].address == 0) { // failure!
				break;
			}
			hash++;
			if (hash >= remote_fd->cache->cache_node_capacity) hash = 0;
		} while (collisions <= remote_fd->cache->collision_allowance);
	} else {
		hash = hash_index_32((uint32_t)address, remote_fd->cache->cache_node_capacity);
		do {
			if (remote_fd->cache->casted_table32[hash].address == (uint32_t)address) { // hit!
				located_file_position = remote_fd->cache->casted_table32[hash].last_link->index_file_offset;
				found = true;
				break;
			} else if (remote_fd->cache->casted_table32[hash].address == 0) { // failure!
				break;
			}
			hash++;
			if (hash >= remote_fd->cache->cache_node_capacity) hash = 0;
		} while (collisions <= remote_fd->cache->collision_allowance);
	}
	
	if (found) {
		// prepare for the read; target process could be 32 or 64 bit.
		stack_logging_index_event32 *target_32_index = NULL;
		stack_logging_index_event64 *target_64_index = NULL;
		
		if (located_file_position >= remote_fd->cache->last_index_file_offset) {
			// must be in shared memory
			if (remote_fd->cache->shmem) {
				if (remote_fd->task_is_64_bit) {
					target_64_index = (stack_logging_index_event64*)(remote_fd->cache->snapshot.index_buffer + (located_file_position - remote_fd->cache->snapshot.start_index_offset));
					located_file_position = STACK_LOGGING_OFFSET(target_64_index->offset_and_flags);
				} else {
					target_32_index = (stack_logging_index_event32*)(remote_fd->cache->snapshot.index_buffer + (located_file_position - remote_fd->cache->snapshot.start_index_offset));
					located_file_position = STACK_LOGGING_OFFSET(target_32_index->offset_and_flags);				
				}
			} else {
				found = false;
			}
		
		} else {
			// it's written to disk
			char bufferSpace[128];
			
			size_t read_size = (remote_fd->task_is_64_bit ? sizeof(stack_logging_index_event64) : sizeof(stack_logging_index_event32));
			fseeko(remote_fd->index_file_stream, (off_t)located_file_position, SEEK_SET);
			size_t read_count = fread(bufferSpace, read_size, (size_t)1, remote_fd->index_file_stream);
			if (read_count) {
				if (remote_fd->task_is_64_bit) {
					target_64_index = (stack_logging_index_event64*)bufferSpace;
					located_file_position = STACK_LOGGING_OFFSET(target_64_index->offset_and_flags);
				} else {
					target_32_index = (stack_logging_index_event32*)bufferSpace;
					located_file_position = STACK_LOGGING_OFFSET(target_32_index->offset_and_flags);
				}
			} else {
				found = false;
			}
		}
	}
	
	release_file_streams_for_task(task);
	
	if (!found) {
		return KERN_FAILURE;
	}
	
	return __mach_stack_logging_frames_for_uniqued_stack(task, located_file_position, stack_frames_buffer, max_stack_frames, count);
}


kern_return_t
__mach_stack_logging_enumerate_records(task_t task, mach_vm_address_t address, void enumerator(mach_stack_logging_record_t, void *), void *context)
{
	remote_task_file_streams *remote_fd = retain_file_streams_for_task(task);
	if (remote_fd == NULL) {
		return KERN_FAILURE;
	}
	
	bool reading_all_addresses = (address == 0 ? true : false);
	mach_stack_logging_record_t pass_record;
	kern_return_t err = KERN_SUCCESS;
	
	if (reading_all_addresses) { // just stupidly read the index file from disk
	
		// update (read index file once and only once)
		update_cache_for_file_streams(remote_fd);
	
		FILE *the_index = (remote_fd->index_file_stream);
		
		// prepare for the read; target process could be 32 or 64 bit.
		char bufferSpace[2048]; // 2 kb
		stack_logging_index_event32 *target_32_index = (stack_logging_index_event32*)bufferSpace;
		stack_logging_index_event64 *target_64_index = (stack_logging_index_event64*)bufferSpace;
		uint32_t target_addr_32 = (uint32_t)STACK_LOGGING_DISGUISE((uint32_t)address);
		uint64_t target_addr_64 = STACK_LOGGING_DISGUISE((uint64_t)address);
		size_t read_size = (remote_fd->task_is_64_bit ? sizeof(stack_logging_index_event64) : sizeof(stack_logging_index_event32));
		size_t number_slots = (size_t)(2048/read_size);
		uint64_t total_slots = remote_fd->cache->last_index_file_offset / read_size;
		
		// perform the search
		size_t read_count = 0;
		int64_t current_file_offset = 0;
		uint32_t i;
		do {
			// at this point, we need to read index events; read them from the file until it's necessary to grab them from the shared memory snapshot
			// and crop file reading to the point where we last scanned
			number_slots = (size_t)MIN(number_slots, total_slots);
			
			// if out of file to read (as of the time we entered this function), try to use shared memory snapshot
			if (number_slots == 0) {
				if (remote_fd->cache->shmem && remote_fd->cache->snapshot.start_index_offset + remote_fd->cache->snapshot.next_free_index_buffer_offset > (uint64_t)current_file_offset) {
					// use shared memory
					target_32_index = (stack_logging_index_event32*)remote_fd->cache->snapshot.index_buffer;
					target_64_index = (stack_logging_index_event64*)remote_fd->cache->snapshot.index_buffer;
					read_count = (uint32_t)(remote_fd->cache->snapshot.start_index_offset + remote_fd->cache->snapshot.next_free_index_buffer_offset - current_file_offset) / read_size;
					current_file_offset += read_count * read_size;
				} else {
					break;
				}
			} else {
				// get and save index (enumerator could modify)
				fseeko(the_index, current_file_offset, SEEK_SET);
				read_count = fread(bufferSpace, read_size, number_slots, the_index);
				current_file_offset = ftello(the_index);
				total_slots -= read_count;
			}
			
			if (remote_fd->task_is_64_bit) {
				for (i = 0; i < read_count; i++) {
					if (reading_all_addresses || target_64_index[i].address == target_addr_64) {
						pass_record.address = STACK_LOGGING_DISGUISE(target_64_index[i].address);
						pass_record.argument = target_64_index[i].argument;
						pass_record.stack_identifier = STACK_LOGGING_OFFSET(target_64_index[i].offset_and_flags);
						pass_record.type_flags = STACK_LOGGING_FLAGS(target_64_index[i].offset_and_flags);
						enumerator(pass_record, context);
					}
				}
			} else {
				for (i = 0; i < read_count; i++) {
					if (reading_all_addresses || target_32_index[i].address == target_addr_32) {
						pass_record.address = STACK_LOGGING_DISGUISE(target_32_index[i].address);
						pass_record.argument = target_32_index[i].argument;
						pass_record.stack_identifier = STACK_LOGGING_OFFSET(target_32_index[i].offset_and_flags);
						pass_record.type_flags = STACK_LOGGING_FLAGS(target_32_index[i].offset_and_flags);
						enumerator(pass_record, context);
					}
				}
			}
		} while (read_count);
	
	} else { // searching for a single address' history
		
		// update (read index file once and only once)
		update_cache_for_file_streams(remote_fd);
		
		// get linked-list of events
		uint32_t collisions = 0;
		uint64_t located_file_position = 0;
		size_t hash = 0;
		index_ll_node *index_position_linked_list = NULL;
		if (remote_fd->task_is_64_bit) {
			hash = hash_index_64(address, remote_fd->cache->cache_node_capacity);
			do {
				if (remote_fd->cache->casted_table64[hash].address == address) { // hit!
					index_position_linked_list = remote_fd->cache->casted_table64[hash].linked_list;
					break;
				} else if (remote_fd->cache->casted_table64[hash].address == 0) { // failure!
					break;
				}
				hash++;
				if (hash >= remote_fd->cache->cache_node_capacity) hash = 0;
			} while (collisions <= remote_fd->cache->collision_allowance);
		} else {
			hash = hash_index_32((uint32_t)address, remote_fd->cache->cache_node_capacity);
			do {
				if (remote_fd->cache->casted_table32[hash].address == (uint32_t)address) { // hit!
					index_position_linked_list = remote_fd->cache->casted_table32[hash].linked_list;
					break;
				} else if (remote_fd->cache->casted_table32[hash].address == 0) { // failure!
					break;
				}
				hash++;
				if (hash >= remote_fd->cache->cache_node_capacity) hash = 0;
			} while (collisions <= remote_fd->cache->collision_allowance);
		}
		
		// if we got something, run it
		char bufferSpace[128];
		size_t read_count = 0;
		stack_logging_index_event32 *target_32_index = (stack_logging_index_event32*)bufferSpace;
		stack_logging_index_event64 *target_64_index = (stack_logging_index_event64*)bufferSpace;
		size_t read_size = (remote_fd->task_is_64_bit ? sizeof(stack_logging_index_event64) : sizeof(stack_logging_index_event32));
		while (index_position_linked_list) {
			located_file_position = index_position_linked_list->index_file_offset;
			
			if (located_file_position >= remote_fd->cache->snapshot.start_index_offset) {
				if (remote_fd->cache->shmem && located_file_position >= remote_fd->cache->snapshot.start_index_offset && remote_fd->cache->snapshot.start_index_offset + remote_fd->cache->snapshot.next_free_index_buffer_offset > (uint64_t)located_file_position) {
					// use shared memory
					target_32_index = (stack_logging_index_event32*)(remote_fd->cache->snapshot.index_buffer + located_file_position - remote_fd->cache->snapshot.start_index_offset);
					target_64_index = (stack_logging_index_event64*)target_32_index;
					read_count = 1;
				} else {
					err = KERN_FAILURE;
					break;
				}
			} else {
				fseeko(remote_fd->index_file_stream, (off_t)located_file_position, SEEK_SET);
				read_count = fread(bufferSpace, read_size, (size_t)1, remote_fd->index_file_stream);
				if (!read_count) {
					err = KERN_FAILURE;
					break;
				}
			}
			if (remote_fd->task_is_64_bit) {
				pass_record.address = STACK_LOGGING_DISGUISE(target_64_index[0].address);
				pass_record.argument = target_64_index[0].argument;
				pass_record.stack_identifier = STACK_LOGGING_OFFSET(target_64_index[0].offset_and_flags);
				pass_record.type_flags = STACK_LOGGING_FLAGS(target_64_index[0].offset_and_flags);
				enumerator(pass_record, context);
			} else {
				pass_record.address = STACK_LOGGING_DISGUISE(target_32_index[0].address);
				pass_record.argument = target_32_index[0].argument;
				pass_record.stack_identifier = STACK_LOGGING_OFFSET(target_32_index[0].offset_and_flags);
				pass_record.type_flags = STACK_LOGGING_FLAGS(target_32_index[0].offset_and_flags);
				enumerator(pass_record, context);
			}
			index_position_linked_list = index_position_linked_list->next;
		}
		
	}

	release_file_streams_for_task(task);
	return err;
}


kern_return_t
__mach_stack_logging_frames_for_uniqued_stack(task_t task, uint64_t stack_identifier, mach_vm_address_t *stack_frames_buffer, uint32_t max_stack_frames, uint32_t *count)
{
	remote_task_file_streams *remote_fd = retain_file_streams_for_task(task);
	if (remote_fd == NULL) {
		return KERN_FAILURE;
	}
	
	// prepare for initial read
	FILE *stack_fd;
	stack_fd = (remote_fd->stack_file_stream);
	char bytes_buffer[16];
	stack_logging_backtrace_event *target_stack_event = (stack_logging_backtrace_event*)bytes_buffer;
	size_t read_size = sizeof(stack_logging_backtrace_event);
	size_t read_count = 0;
	off_t reading_offset = (off_t)stack_identifier;
	
	// get a temporary spot for the backtrace frames to go and reference the stack space such that the reference
	// can be later pointed at the shared memory snapshot and data read from there.
	uint64_t temp_frames_buffer[STACK_LOGGING_MAX_STACK_SIZE];
	uint64_t *big_frames = (uint64_t*)temp_frames_buffer;
	uint32_t *small_frames = (uint32_t*)temp_frames_buffer;
	size_t target_frame_size = (remote_fd->task_is_64_bit ? sizeof(uint64_t) : sizeof(uint32_t));
	char *snapshot_backtrace_location = NULL;
	
	int done = 0;
	int32_t total_frames = -1;
	int32_t hot_frames_read = 0;
	size_t new_hot_frames = 0;
	int32_t number_needed_hot_frames_in_event;
	size_t number_hot_frames_to_skip;
	int32_t i;
	bool skip_file_read;
	
	while (!done) {
	
		// not in cache; read record Ð from disk if possible, shared memory snapshot if necessary.
		if (remote_fd->cache->shmem && reading_offset >= (off_t)(remote_fd->cache->snapshot.start_stack_offset)) {
			// must read from shared memory; the record isn't on disk yet
			snapshot_backtrace_location = (remote_fd->cache->snapshot.stack_buffer + (reading_offset - remote_fd->cache->snapshot.start_stack_offset));
			*target_stack_event = *(stack_logging_backtrace_event*)snapshot_backtrace_location;
			big_frames = (uint64_t*)(snapshot_backtrace_location + sizeof(stack_logging_backtrace_event));
			small_frames = (uint32_t*)big_frames;
			skip_file_read = true;
		} else {
			// the record's on disk
			i = fseeko(stack_fd, reading_offset, SEEK_SET);
			if (i != 0) break; // unable to seek to the target position
			read_count = fread(target_stack_event, read_size, (size_t)1, stack_fd);
			if (read_count == 0) break;
			
			big_frames = (uint64_t*)temp_frames_buffer;
			small_frames = (uint32_t*)temp_frames_buffer;
			skip_file_read = false;
		}
		
		if (total_frames < 0) {
			total_frames = target_stack_event->num_new_hot_frames + target_stack_event->num_identical_frames;
			if (total_frames > (int32_t)max_stack_frames) break; // don't know what to do with this; we'll just KERN_FAILURE.
		}
		
		// do the math to find how many frames to apply from previous event
		new_hot_frames = target_stack_event->num_new_hot_frames;
		number_needed_hot_frames_in_event = total_frames - hot_frames_read - target_stack_event->num_identical_frames;
		number_hot_frames_to_skip = new_hot_frames - number_needed_hot_frames_in_event;
		
		// read and apply the important frames of this one
		if (number_needed_hot_frames_in_event > 0) {
			if (!skip_file_read) {
				read_count = fread(temp_frames_buffer, target_frame_size, new_hot_frames, stack_fd);
				if (read_count < new_hot_frames) break;
			}
			
			if (remote_fd->task_is_64_bit) {
				for (i = 0; i < number_needed_hot_frames_in_event; i++) {
					stack_frames_buffer[hot_frames_read++] = big_frames[i+number_hot_frames_to_skip];
				}
			} else {
				for (i = 0; i < number_needed_hot_frames_in_event; i++) {
					stack_frames_buffer[hot_frames_read++] = small_frames[i+number_hot_frames_to_skip];
				}
			}
		}
		
		reading_offset += target_stack_event->offset_delta;
		
		if (hot_frames_read == total_frames) done = 1;
		else if (target_stack_event->offset_delta == 0) {
			fprintf(stderr, "incomplete stack record (identifier: 0x%qx)\n", reading_offset);
			break;
		}   
	}
	
	release_file_streams_for_task(task);
	
	if (done) {
		*count = hot_frames_read; 
		return KERN_SUCCESS;
	} else {
		return KERN_FAILURE;
	}
}


#ifdef TEST_DISK_STACK_LOGGING

// cc -o stack_logging_disk stack_logging_disk.c -DTEST_DISK_STACK_LOGGING

#include <sys/wait.h>

main()
{
  int status;
  int i;

  fprintf(stderr, "master test process is %d\n", getpid());
  fprintf(stderr, "sizeof stack_buffer: %d\n", sizeof(stack_buffer));
  fprintf(stderr, "sizeof thread_buffer: %d\n", sizeof(thread_buffer));
  fprintf(stderr, "sizeof stack_logs_directory: %d\n", sizeof(stack_logs_directory));
  fprintf(stderr, "sizeof remote_fds: %d\n", sizeof(remote_fds));
  fprintf(stderr, "address of pre_write_backtrace_event_buffer: %p\n", &pre_write_backtrace_event_buffer);
  fprintf(stderr, "address of logging_use_compaction: %p\n", &logging_use_compaction);
  //  fprintf(stderr, "size of all global data:  %d\n", (logging_use_compaction) - (pre_write_backtrace_event_buffer) + sizeof(logging_use_compaction));

  create_log_files();

  // create a few child processes and exit them cleanly so their logs should get cleaned up
  fprintf(stderr, "\ncreating child processes and exiting cleanly\n");
  for (i = 0; i < 3; i++) {
    if (fork() == 0) {
      fprintf(stderr, "\nin child processes %d\n", getpid());
      create_log_files();
      fprintf(stderr, "exiting child processes %d\n", getpid());
      exit(1);
    }
    wait(&status);
  }

  // create a few child processes and abruptly _exit them, leaving their logs around
  fprintf(stderr, "\ncreating child processes and exiting abruptly, leaving logs around\n");
  for (i = 0; i < 3; i++) {
    if (fork() == 0) {
      fprintf(stderr, "\nin child processes %d\n", getpid());
      create_log_files();
      fprintf(stderr, "exiting child processes %d\n", getpid());
      _exit(1);
    }
    wait(&status);
  }

  // this should reap any remaining logs
  fprintf(stderr, "\nexiting master test process %d\n", getpid());
  delete_log_files();
}

#endif
