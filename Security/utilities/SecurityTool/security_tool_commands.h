//
//  security_tool_commands.h
//  Security
//
//  Created by Mitch Adler on 1/9/13.
//
//

// This is included to make SECURITY_COMMAND macros result in declarations of
// commands for use in SecurityTool

#ifndef SECURITY_COMMAND
#define SECURITY_COMMAND(name, function, parameters, description) int function(int argc, char * const *argv);

#if TARGET_OS_EMBEDDED
#define SECURITY_COMMAND_IOS(name, function, parameters, description) int function(int argc, char * const *argv);
#else
#define SECURITY_COMMAND_IOS(name, function, parameters, description) extern int command_not_on_this_platform(int argc, char * const *argv);
#endif

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#define SECURITY_COMMAND_MAC(name, function, parameters, description) int function(int argc, char * const *argv);
#else
#define SECURITY_COMMAND_MAC(name, function, parameters, description) extern int command_not_on_this_platform(int argc, char * const *argv);
#endif


#endif
