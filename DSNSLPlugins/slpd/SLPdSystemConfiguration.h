/*
 *  SLPdSystemConfiguration.h
 *  NSLPlugins
 *
 *  Created by imlucid on Fri Sep 21 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>

#include <SystemConfiguration/SCDynamicStorePrivate.h>
#ifndef _SLPdSystemConfiguration_
#define _SLPdSystemConfiguration_

class SLPdSystemConfiguration : public SLPSystemConfiguration
{
public:
                                    SLPdSystemConfiguration								( CFRunLoopRef runLoopRef = 0 );
                                    ~SLPdSystemConfiguration							();
                        
    static 	SLPSystemConfiguration*	TheSLPSC											( CFRunLoopRef runLoopRef = 0 );			// accessor to the class
    static	void					FreeSLPSC											( void );			

    virtual void					HandleIPv4Notification								( void );
	virtual void					HandleInterfaceNotification							( void );
private:
};

#endif