//
//  UtilTerminalColor.cpp
//  CPPUtil
//
//  Created by James McIlree on 4/26/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "CPPUtil.h"

BEGIN_UTIL_NAMESPACE

#define COLOR(FGBG, CODE, BOLD) "\033[0;" BOLD FGBG CODE "m"

#define ALLCOLORS(FGBG,BOLD) {\
	COLOR(FGBG, "0", BOLD),\
	COLOR(FGBG, "1", BOLD),\
	COLOR(FGBG, "2", BOLD),\
	COLOR(FGBG, "3", BOLD),\
	COLOR(FGBG, "4", BOLD),\
	COLOR(FGBG, "5", BOLD),\
	COLOR(FGBG, "6", BOLD),\
	COLOR(FGBG, "7", BOLD)\
}

static const char colorcodes[2][2][8][10] = {
	{ ALLCOLORS("3",""), ALLCOLORS("3","1;") },
	{ ALLCOLORS("4",""), ALLCOLORS("4","1;") }
};

const char* TerminalColorStringFor(kTerminalColor code, bool is_bold, bool is_background) {
	return colorcodes[is_background ? 1 : 0][is_bold ? 1 : 0][(uint32_t)code & 7];
}

const char* TerminalColorResetString(void) {
	return "\033[0m";
}

END_UTIL_NAMESPACE
