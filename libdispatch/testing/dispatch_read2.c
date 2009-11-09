#include <sys/stat.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <dispatch/dispatch.h>

#include "dispatch_test.h"

static size_t bytes_total;
static size_t bytes_read;

int main(void)
{
	const char *path = "/usr/share/dict/words";
	struct stat sb;

	test_start("Dispatch Source Read");
	
	int infd = open(path, O_RDONLY);
	if (infd == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	if (fstat(infd, &sb) == -1) {
		perror(path);
		exit(EXIT_FAILURE);
	}
	bytes_total = sb.st_size;

	if (fcntl(infd, F_SETFL, O_NONBLOCK) != 0) {
		perror(path);
		exit(EXIT_FAILURE);
	}

	dispatch_queue_t main_q = dispatch_get_main_queue();
	test_ptr_notnull("dispatch_get_main_queue", main_q);
		
	dispatch_source_t reader = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, infd, 0, main_q);
	test_ptr_notnull("dispatch_source_create", reader);
	assert(reader);
	
	dispatch_source_set_event_handler(reader, ^{
			size_t estimated = dispatch_source_get_data(reader);
			printf("bytes available: %zu\n", estimated);
			const ssize_t bufsiz = 1024*500; // 500 KB buffer
			static char buffer[1024*500];	// 500 KB buffer
			ssize_t actual = read(infd, buffer, sizeof(buffer));
			bytes_read += actual;
			printf("bytes read: %zd\n", actual);
			if (actual < bufsiz) {
				actual = read(infd, buffer, sizeof(buffer));
				bytes_read += actual;
				// confirm EOF condition
				test_long("EOF", actual, 0);
				dispatch_source_cancel(reader);
			}
	});
	dispatch_source_set_cancel_handler(reader, ^{
		test_long("Bytes read", bytes_read, bytes_total);
		test_stop();
	});
	dispatch_resume(reader);

	dispatch_main();
}
