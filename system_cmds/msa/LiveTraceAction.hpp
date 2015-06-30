//
//  LiveTraceAction.h
//  msa
//
//  Created by James McIlree on 2/4/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef __msa__LiveTraceAction__
#define __msa__LiveTraceAction__

class LiveTraceAction : public Action {
    public:
	LiveTraceAction() {}

	virtual void execute(Globals& globals);
};

#endif /* defined(__msa__LiveTraceAction__) */
