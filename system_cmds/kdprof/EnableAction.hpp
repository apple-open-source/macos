//
//  EnableAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_EnableAction_hpp
#define kdprof_EnableAction_hpp

class EnableAction : public Action {
    public:
	EnableAction() {}

	virtual void execute(Globals& globals);
};

#endif
