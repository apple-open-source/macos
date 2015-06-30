//
//  PrintStateAction.hpp
//  kdprof
//
//  Created by James McIlree on 4/16/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_PrintStateAction_hpp
#define kdprof_PrintStateAction_hpp

class PrintStateAction : public Action {
    public:
	PrintStateAction() {}

	virtual void execute(Globals& globals);
};

#endif
