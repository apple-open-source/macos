/*
 *  UtilityRoutines.h
 *  IrDAExtra
 *
 *  Created by jwilcox on Fri Jun 01 2001.
 *  Copyright (c) 2001 __CompanyName__. All rights reserved.
 *
 */

//#import <Foundation/Foundation.h>
//#import <AppKit/AppKit.h>
#include <Carbon/Carbon.h>

#include <mach/mach.h>
#include <mach/mach_interface.h>

#include <IOKit/IOTypes.h>
#if MAC_OS_X_VERSION_10_5
#include <IOKit/iokitmig.h>
#else
#include <IOKit/iokitmig_c.h>
#endif
#include <IOKit/IOKitLib.h>

#include "IrDAStats.h"
#include "IrDAUserClient.h"

io_object_t getInterfaceWithName(mach_port_t masterPort, char *className);

kern_return_t doCommand(io_connect_t con, unsigned char commandID, void *inputData, unsigned long inputDataSize, void *outputData, size_t *outputDataSize);
