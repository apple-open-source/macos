//
//  EnableAction.cpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

void EnableAction::execute(Globals& globals) {
	if (!KDBG::set_enabled(true)) {
		usage("Unable to enable tracing");
	}
}
