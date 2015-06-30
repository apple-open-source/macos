//
//  SaveTraceAction.hpp
//  kdprof
//
//  Created by James McIlree on 5/2/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __kdprof__SaveTraceAction__
#define __kdprof__SaveTraceAction__

class SaveTraceAction : public Action {
	FileDescriptor _save_fd;

    public:
	SaveTraceAction(FileDescriptor&& fd) :
		_save_fd(std::move(fd))
	{
		ASSERT(_save_fd.is_open(), "Sanity");
	}

	virtual void execute(Globals& globals);
};

#endif
