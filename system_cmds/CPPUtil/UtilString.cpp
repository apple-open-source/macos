//
//  UtilString.cpp
//  CPPUtil
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

bool ends_with(std::string& str, std::string postfix) {
	size_t pos = str.rfind(postfix);
	return ((pos != std::string::npos) && (pos == (str.length() - postfix.length())));
}

END_UTIL_NAMESPACE
