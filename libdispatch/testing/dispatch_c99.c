#include <dispatch/dispatch.h>
#include <stdlib.h>

#include "dispatch_test.h"

void
work(void *context __attribute__((unused)))
{
	test_stop();
	exit(0);
}

int main(void) {
	test_start("Dispatch C99");
	dispatch_queue_t q = dispatch_get_main_queue();
	test_ptr_notnull("dispatch_get_main_queue", q);

	dispatch_async_f(dispatch_get_main_queue(), NULL, work);
	dispatch_main();
	return 0;
}
