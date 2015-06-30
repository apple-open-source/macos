//
//  UtilLog.h
//  CPPUtil
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef __CPPUtil__UtilLog__
#define __CPPUtil__UtilLog__

void log_msg(int level, const char* format, ...)  __attribute__((format(printf, 2, 3)));

#endif /* defined(__CPPUtil__UtilLog__) */
