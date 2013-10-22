//
//  security_tool_commands_table.h
//  utilities
//
//  Created by J Osborne on 1/11/13.
//  Copyright (c) 2013 Apple Inc. All rights reserved.
//


// This is included to make SECURITY_COMMAND macros result in table of
// commands for use in SecurityTool

#undef SECURITY_COMMAND
#undef SECURITY_COMMAND_IOS
#undef SECURITY_COMMAND_MAC
#define SECURITY_COMMAND(name, function, parameters, description)  { name, function, parameters, description },

#if TARGET_OS_EMBEDDED
#define SECURITY_COMMAND_IOS(name, function, parameters, description)  { name, function, parameters, description },
#else
#define SECURITY_COMMAND_IOS(name, function, parameters, description)  { name, command_not_on_this_platform, "", "Not avilable on this platform" },
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#define SECURITY_COMMAND_MAC(name, function, parameters, description)  { name, function, parameters, description },
#else
#define SECURITY_COMMAND_MAC(name, function, parameters, description) { name, command_not_on_this_platform, "", "Not avilable on this platform" },
#endif

