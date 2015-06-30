//
//  DisableAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_DisableAction_hpp
#define kdprof_DisableAction_hpp

class DisableAction : public Action {
    public:
	DisableAction() {}

	virtual void execute(Globals& globals);
};

#endif
