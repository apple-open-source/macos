//
//  security_tool_commands.c
//  Security
//
//  Created by J Osborne on 1/10/13.
//
//

#include <TargetConditionals.h>
#include <stdio.h>
#include "SecurityTool/SecurityTool.h"

#include "SecurityTool/security_tool_commands.h"

#include "SecurityTool/builtin_commands.h"
#include "security2/sub_commands.h"

// Redefine for making them declaraionts.
#include "SecurityTool/security_tool_commands_table.h"

const command commands[] =
{
#include "SecurityTool/builtin_commands.h"
    
#include "security2/sub_commands.h"
    
    {}
};
