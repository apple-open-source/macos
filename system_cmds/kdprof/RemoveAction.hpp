//
//  RemoveAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_RemoveAction_hpp
#define kdprof_RemoveAction_hpp

class RemoveAction : public Action {
    public:
	RemoveAction() {}

	virtual void execute(Globals& globals);
};

#endif
