//
//  InitializeAction.cpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

void InitializeAction::execute(Globals& globals) {
	if (_buffers) {
		if (!KDBG::set_buffer_capacity(_buffers)) {
			usage("Attempt to set buffer count failed");
		}
	}

	if (!KDBG::initialize_buffers()) {
		usage("Attempt to initialize buffers failed\n");
	}
}
