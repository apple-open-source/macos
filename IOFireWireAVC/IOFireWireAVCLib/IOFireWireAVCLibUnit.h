/*
 *  IOFireWireAVCLibUnit.h
 *  IOFireWireAVC
 *
 *  Created by cpieper on Wed Feb 06 2002.
 *  Copyright (c) 2001 __MyCompanyName__. All rights reserved.
 *
 */

#ifndef _IOKIT_IOFIREWIREAVCLIBUNIT_H_
#define _IOKIT_IOFIREWIREAVCLIBUNIT_H_

#include <IOKit/IOKitLib.h>

class IOFireWireAVCLibConsumer;

__BEGIN_DECLS
void consumerPlugDestroyed( void * self, IOFireWireAVCLibConsumer * consumer );
Boolean isDeviceSuspended( void * self );
__END_DECLS

#endif