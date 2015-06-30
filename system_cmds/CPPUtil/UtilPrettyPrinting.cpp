//
//  UtilPrettyPrinting.cpp
//  CPPUtil
//
//  Created by James McIlree on 9/8/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

std::string formated_byte_size(uint64_t bytes) {
	if (bytes) {
		char tmp[128];
		const char *si_prefix[] = { "B", "KB", "MB", "GB", "TB", "PB", "EB", "ZB", "YB" };
		const int base = 1024;
		int c = std::min((int)(log((double)bytes)/log((double)base)), (int)sizeof(si_prefix) - 1);
		snprintf(tmp, sizeof(tmp), "%1.2f %s", bytes / pow((double)base, c), si_prefix[c]);
		return std::string(tmp);
	}

	return std::string("0.00 B");
}

END_UTIL_NAMESPACE
