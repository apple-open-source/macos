//
//  Action.hpp
//  msa
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef msa_Action_hpp
#define msa_Action_hpp

class Action {
    public:
	virtual void execute(Globals& globals) = 0;
};

#endif
