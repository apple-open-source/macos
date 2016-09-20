//
//  IOHIDNXEventDescription.h
//  IOHIDFamily
//
//  Created by YG on 9/21/15.
//
//

#ifndef IOHIDNXEVENTDESCRIPTION_H
#define IOHIDNXEVENTDESCRIPTION_H

#include <IOKit/hidsystem/IOHIDShared.h>

CFStringRef NxEventCreateDescription (NXEvent *event);
CFStringRef NxEventExtCreateDescription (NXEventExt *event);

#endif /* IOHIDNXEVENTDESCRIPTION_H */
