#include <dispatch/dispatch.h>
#include <unistd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "dispatch_test.h"

int main(void) {
	long res;

	test_start("Dispatch Queue Finalizer");

#ifdef __LP64__
	void* ctxt_magic = (void*)((uintptr_t)arc4random() << 32 | arc4random());
#else
	void* ctxt_magic = (void*)arc4random();
#endif

	dispatch_queue_attr_t attr = dispatch_queue_attr_create();
	test_ptr_notnull("dispatch_queue_attr_create", attr);

	__block long finalizer_ran = 0;

	res = dispatch_queue_attr_set_finalizer(attr, ^(dispatch_queue_t dq) {
		void* ctxt = dispatch_queue_get_context(dq);
		test_ptr("dispatch_queue_get_context", ctxt, ctxt_magic);
		test_ptr_notnull("finalizer ran", dq);
		test_stop();
	});
	test_long("dispatch_queue_attr_set_finalizer", res, 0);

	dispatch_queue_t q = dispatch_queue_create(NULL, attr);
	test_ptr_notnull("dispatch_queue_new", q);

	dispatch_queue_set_context(q, ctxt_magic);

	dispatch_release(attr);

	dispatch_release(q);

	dispatch_main();
	
	return 0;
}
