//
//  UtilLog.cpp
//  CPPUtil
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

void log_msg(int level, const char* format, ...) {
	va_list list;
	va_start(list, format);
	asl_vlog(NULL, NULL, level, format, list);
	va_end(list);

	va_start(list, format);
	vfprintf(stderr, format, list);
	va_end(list);
}

END_UTIL_NAMESPACE
