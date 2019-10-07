//
//  HIDDriverKitDebug.cpp
//  HIDDriverKit
//
//  Created by dekom on 3/22/19.
//

#include <DriverKit/DriverKit.h>
#include "HIDDriverKitDebug.h"

uint32_t gHIDDKDebug()
{
    static uint32_t debug = -1;
    
    if (debug == -1) {
        debug = 0;
        IOParseBootArgNumber("hiddk", &debug, sizeof(debug));
    }
    
    return debug;
}
