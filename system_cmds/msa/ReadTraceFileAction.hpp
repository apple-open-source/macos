//
//  ReadTraceFileAction.hpp
//  msa
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef __kdprof__TraceFileAction__
#define __kdprof__TraceFileAction__

class ReadTraceFileAction : public Action {
    protected:
	std::string _path;
	
    public:
	ReadTraceFileAction(const char* path) : _path(path) {
		ASSERT(Path::is_file(_path, TRUE), "File must exist");
	}

	virtual void execute(Globals& globals);
};

#endif /* defined(__msa__TraceFileAction__) */
