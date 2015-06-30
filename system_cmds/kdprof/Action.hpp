//
//  Action.hpp
//  kdprof
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_Action_hpp
#define kdprof_Action_hpp

class Action {
    public:
	virtual void execute(Globals& globals) = 0;
};

#endif
