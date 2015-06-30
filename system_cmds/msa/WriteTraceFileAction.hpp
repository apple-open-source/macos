//
//  WriteTraceFileAction.hpp
//  system_cmds
//
//  Created by James McIlree on 4/29/14.
//
//

#ifndef __system_cmds__WriteTraceFileAction__
#define __system_cmds__WriteTraceFileAction__

class WriteTraceFileAction : public Action {
    protected:
        std::string _path;

    public:
        WriteTraceFileAction(const char* path) :
		_path(path)
	{
		ASSERT(path, "Sanity");
	}

        virtual void execute(Globals& globals);
};

#endif /* defined(__system_cmds__WriteTraceFileAction__) */
