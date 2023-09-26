//
//  main.c
//
//
//  Created by Justin Vreeland on 6/13/23.
//  Copyright © 2023 Apple Inc. All rights reserved.
//

#include <assert.h>
#include <stdlib.h>

#include "archive_check_entitlement.h"


int main(int argc, const char * argv[]) {
	assert(archive_allow_entitlement_format("zip"));
	assert(!archive_allow_entitlement_format("xar"));

	assert(archive_allow_entitlement_filter("compress"));
	assert(!archive_allow_entitlement_filter("bzip2"));

	exit(0);
}
