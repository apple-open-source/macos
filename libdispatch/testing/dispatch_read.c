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
	
	dispatch_source_attr_t attr = dispatch_source_attr_create();
	test_ptr_notnull("dispatch_source_attr_create", attr);
	
	dispatch_source_attr_set_finalizer(attr, ^(dispatch_source_t ds) {
		test_ptr_notnull("finalizer ran", ds);
		int res = close(infd);
		test_errno("close", res == -1 ? errno : 0, 0);
		test_stop();
	});
	
	dispatch_source_t reader = dispatch_source_read_create(infd, attr,
		main_q, ^(dispatch_event_t ev) {
			long err_val;
			long err_dom = dispatch_event_get_error(ev, &err_val);
			if (!err_dom) {
				size_t estimated = dispatch_event_get_bytes_available(ev);
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
					dispatch_release(dispatch_event_get_source(ev));
				}
			} else {
				test_long("Error domain", err_dom, DISPATCH_ERROR_DOMAIN_POSIX);
				test_long("Error value", err_val, ECANCELED);
				test_long("Bytes read", bytes_read, bytes_total);
			}
		});

	printf("reader = %p\n", reader);
	assert(reader);

	dispatch_release(attr);

	dispatch_main();
}
