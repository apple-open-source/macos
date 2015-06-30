//
//  UtilTimer.h
//  CPPUtil
//
//  Created by James McIlree on 10/9/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilTimer__
#define __CPPUtil__UtilTimer__

class Timer {
    protected:
	AbsTime _start;
	AbsTime _end;
	std::string _message;

    public:
	Timer(const char* message);
	~Timer();
};

#endif /* defined(__CPPUtil__UtilTimer__) */
