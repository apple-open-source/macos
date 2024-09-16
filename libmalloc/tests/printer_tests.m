//
// printer_tests.m
// libmalloc
//
// Tests for the xzone malloc json printer
//

#include <darwintest.h>
#include <darwintest_utils.h>
#include <../src/internal.h>

#if CONFIG_XZONE_MALLOC && (MALLOC_TARGET_IOS_ONLY || TARGET_OS_OSX)

#include <Foundation/Foundation.h>
#include <Foundation/NSJSONSerialization.h>

static char *print_buffer = NULL;
static size_t print_buffer_capacity = 0;
static size_t print_buffer_index = 0;

static void
reset_print_buffer(void)
{
	T_ASSERT_NULL(print_buffer, "reset_print_buffer called multiple times");
	print_buffer_index = 0;
	print_buffer_capacity = vm_page_size;
	print_buffer = malloc(print_buffer_capacity);
	T_ASSERT_NOTNULL(print_buffer, "Allocate print buffer");
}

static void
resize_print_buffer(void)
{
	T_ASSERT_NOTNULL(print_buffer, "Must call reset_print_buffer first");
	vm_address_t addr = 0;
	size_t new_capacity = print_buffer_capacity * 2;
	print_buffer = realloc(print_buffer, new_capacity);
	T_ASSERT_NOTNULL(print_buffer, "Realloc print buffer");
	print_buffer_capacity = new_capacity;
}

static void
append_to_buffer(const char *data, size_t len)
{
	while (1) {
		if (len >= print_buffer_capacity - print_buffer_index) {
			resize_print_buffer();
		} else {
			memcpy(&print_buffer[print_buffer_index], data, len);
			print_buffer_index += len;
			break;
		}
	}
}

static void
free_print_buffer(void)
{
	free(print_buffer);
	print_buffer = NULL;
	print_buffer_capacity = 0;
	print_buffer_index = 0;
}

// Calls "heap -p *procname" and returns an array of json objects, one each for
// the xzm zones in the remote process. If the process doesn't use xzm, the
// array will be empty. If the process doesn't exist, this function either calls
// T_FAIL or T_SKIP, based on skip_if_not_found. If multiple processes with name
// procname exist, this function will call T_FAIL.
static NSArray *
get_process_json(const char *procname, bool skip_if_not_found)
{
	reset_print_buffer();

	__block bool not_found = false;

	// Dump heap's stderr to our stdout, for help debugging test failures. Also
	// monitor for a magic string indicating that the requested process doesn't
	// exist, to handle skip_if_not_found
	dt_pipe_data_handler_t stderr_handler = ^bool(char *data,
			__unused size_t data_size,
			__unused dt_pipe_data_handler_context_t *context) {
		T_LOG("heap stderr: %s", data);
		const char *not_found_needle = "heap cannot find any existing process";
		if (strstr(data, not_found_needle)) {
			not_found = true;
			T_LOG("Process does not exist");
			return true;
		}
		return false;
	};
	// returning true will stop executing handler.
	dt_pipe_data_handler_t stdout_handler =
			^bool(char *data, __unused size_t data_size,
			__unused dt_pipe_data_handler_context_t *context) {
		T_LOG("heap output: %s", data);
		append_to_buffer(data, data_size);
		return false;
	};

	const char *argv[] = { "/usr/bin/heap", "-p", procname, NULL };
	pid_t pid = dt_launch_tool_pipe(argv, false, NULL, stdout_handler,
			stderr_handler, BUFFER_PATTERN_LINE, NULL);

	int exit_status;
	int signal;
	int timeout = 30; // 30 second timeout
	bool wait = dt_waitpid(pid, &exit_status, &signal, timeout);

	if (!wait && skip_if_not_found && not_found) {
		// Failed exit - should only occur when the tools couldn't find a
		// process by name
		T_SKIP("Skipping since %s doesn't exist", procname);
	}
	T_ASSERT_TRUE(wait, "heap exited successfully, status = %d, signal = %d",
			exit_status, signal);
	T_ASSERT_POSIX_ZERO(exit_status, "Exit status is success");
	T_ASSERT_POSIX_ZERO(signal, "Exit signal is success");

	// We don't have a great way to know that we've processed all of heap's
	// stderr/stdout, so use a fixed sleep to (hopefully) let those finish
	sleep(1);

	NSMutableArray *retval = [NSMutableArray arrayWithCapacity:1];
	char *heap_output = &print_buffer[0];
	size_t heap_len = print_buffer_index;
	while (heap_output) {
		const char *start_symbol = "Begin xzone malloc JSON:\n";
		const char *end_symbol = "End xzone malloc JSON\n";

		char *json_start = strnstr(heap_output, start_symbol, heap_len);
		if (!json_start) {
			// No more json to parse
			break;
		}
		json_start += strlen(start_symbol);
		char *json_end = strnstr(heap_output, end_symbol, heap_len);
		T_ASSERT_GE(json_end, json_start, "Incorrect end token");

		NSData *json_data = [NSData dataWithBytes:json_start
				length:(json_end - json_start)];

		NSError *e = nil;
		NSDictionary *json_dict =
				[NSJSONSerialization JSONObjectWithData:json_data options:0
				error:&e];
		T_ASSERT_NE(json_dict, nil, "Parsed json, error = %s",
				[[e localizedDescription] UTF8String]);

		[retval addObject:json_dict];

		char *new_heap_output = json_end + strlen(end_symbol);
		heap_len -= new_heap_output - heap_output;
		heap_output = new_heap_output;
	}

	free_print_buffer();

	return retval;
}


static void
security_critical_process_checks(NSArray *json_array)
{
	// A security critical process should have at least one xzone malloc zone,
	// and should have guard pages enabled
	T_ASSERT_GE(json_array.count, 1, "At least one zone is xzm");
	NSDictionary *guard_config = json_array[0][@"guard_config"];
	T_ASSERT_NE(guard_config, nil, "Guard config dictionary in output");
	T_ASSERT_TRUE([guard_config[@"guards_enabled"] boolValue],
			"Guards enabled in launchd");
}

T_DECL(xzone_enabled_launchd, "Check that xzone malloc is enabled in launchd",
		T_META_ENABLED(!TARGET_CPU_X86_64),
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	security_critical_process_checks(get_process_json("1", false));
}

T_DECL(xzone_enabled_logd, "Check that xzone malloc is enabled in logd",
		T_META_ENABLED(!TARGET_CPU_X86_64),
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	security_critical_process_checks(get_process_json("logd", false));
}

T_DECL(xzone_enabled_notifyd, "Check that xzone malloc is enabled in notifyd",
		T_META_ENABLED(!TARGET_CPU_X86_64),
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	security_critical_process_checks(get_process_json("notifyd", false));
}

#else // CONFIG_XZONE_MALLOC && (MALLOC_TARGET_IOS || TARGET_OS_OSX)
T_DECL(skip_json_printer_tests, "Skip printer tests")
{
	T_SKIP("Nothing to test without xzone malloc on ios/macos");
}
#endif // CONFIG_XZONE_MALLOC && (MALLOC_TARGET_IOS_ONLY || TARGET_OS_OSX)
