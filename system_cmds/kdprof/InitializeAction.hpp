//
//  InitializeAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_InitializeAction_hpp
#define kdprof_InitializeAction_hpp

class InitializeAction : public Action {
    protected:
	uint32_t _buffers;

    public:
	InitializeAction(uint32_t buffers) : _buffers(buffers) { }

	virtual void execute(Globals& globals);
};

#endif
