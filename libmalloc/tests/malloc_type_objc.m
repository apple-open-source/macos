#import <Foundation/Foundation.h>

#include <stdio.h>
#include <assert.h>

#include "tmo_test_defs.h"

void
validate_swift_obj_array(void **ptrs);

void
validate_obj_array(NSArray *a)
{
	void *ptrs[N_TEST_SWIFT_CLASSES];
	int i = 0;
	for (id obj in a) {
		ptrs[i] = (void *)obj;
		i++;
	}
	assert(i == N_TEST_SWIFT_CLASSES);

	validate_swift_obj_array(ptrs);
}
