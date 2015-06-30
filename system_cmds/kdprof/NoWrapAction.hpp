//
//  NoWrapAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_NoWrapAction_hpp
#define kdprof_NoWrapAction_hpp

class NoWrapAction : public Action {
    public:
	NoWrapAction() {}

	virtual void execute(Globals& globals);
};

#endif
