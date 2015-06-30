//
//  SleepAction.cpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

void SleepAction::execute(Globals& globals) {
	uint64_t nanos = _time.value();
	struct timespec ts;
	ts.tv_sec = decltype(ts.tv_sec)(nanos / NANOSECONDS_PER_SECOND);
	ts.tv_nsec = decltype(ts.tv_sec)(nanos - ts.tv_sec * NANOSECONDS_PER_SECOND);
        nanosleep(&ts, NULL);
}
