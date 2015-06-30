//
//  UtilTerminalColor.hpp
//  CPPUtil
//
//  Created by James McIlree on 4/26/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef CPPUtil_UtilTerminalColor_hpp
#define CPPUtil_UtilTerminalColor_hpp

// This is borrowed from clang

enum class kTerminalColor : std::uint32_t {
	BLACK=0,
	RED,
	GREEN,
	YELLOW,
	BLUE,
	MAGENTA,
	CYAN,
	WHITE
};

const char* TerminalColorStringFor(kTerminalColor code, bool is_bold, bool is_background);
const char* TerminalColorResetString(void);

#endif
