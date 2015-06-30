//
//  SleepAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_SleepAction_hpp
#define kdprof_SleepAction_hpp

class SleepAction : public Action {
	NanoTime _time;
	
    public:
	SleepAction(NanoTime t) : _time(t) {}

	virtual void execute(Globals& globals);
};

#endif
