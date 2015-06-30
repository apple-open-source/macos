//
//  CollectAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_CollectAction_hpp
#define kdprof_CollectAction_hpp

class CollectAction : public Action {
    public:
	CollectAction() {}

	virtual void execute(Globals& globals);
};

#endif
