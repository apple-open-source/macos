//
//  builtin_commands.h
//  sec
//
//  Created by Mitch Adler on 1/9/13.
//
//

#include "SecurityTool/security_tool_commands.h"

SECURITY_COMMAND("help", help,
                 "[command ...]",
                 "Show all commands. Or show usage for a command.")

SECURITY_COMMAND("digest", command_digest,
                 "algo file(s)...\n"
                 "Where algo is one of:\n"
                 "    sha1\n"
                 "    sha256\n"
                 "    sha512\n",
                 "Calculate a digest over the given file(s).")
