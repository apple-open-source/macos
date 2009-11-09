#include <dispatch/dispatch.h>
#include <stdlib.h>

#include "dispatch_test.h"

int main(void) {
	test_start("Dispatch C++");
	dispatch_queue_t q = dispatch_get_main_queue();
	test_ptr_notnull("dispatch_get_main_queue", q);

	dispatch_async(dispatch_get_main_queue(), ^{
		test_stop();
		exit(0);
	});
	dispatch_main();
	return 0;
}
