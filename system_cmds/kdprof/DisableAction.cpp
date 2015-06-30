//
//  DisableAction.cpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

void DisableAction::execute(Globals& globals) {
	if (!KDBG::set_enabled(false)) {
		usage("Unable to disable tracing");
	}
}
