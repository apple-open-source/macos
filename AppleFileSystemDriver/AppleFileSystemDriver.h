/* 
 * Copyright (c) 2004 Apple Computer, Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
#ifdef KERNEL
#ifdef __cplusplus

#include <IOKit/IOService.h>
#include <IOKit/storage/IOMedia.h>
#include <uuid/uuid.h>

/*
 * Class
 */

class AppleFileSystemDriver : public IOService
{
    OSDeclareDefaultStructors(AppleFileSystemDriver)

protected:

    uuid_t      _uuid;
    OSString   *_uuidString;
    OSString   *_containerUUID;
    UInt32      _state;
    
    struct ExpansionData { /* */ };
    ExpansionData * _expansionData;

public:

    virtual bool start(IOService * provider);

    virtual void free();
    
private:

    bool startWithAPFSUUIDForRecovery(OSObject *uuid);
    bool checkAPFSUUIDForRecovery(void *ref, IOService *service, IONotifier *notifier);
    bool checkAPFSRecoveryVolumeInContainer(void *ref, IOService *service, IONotifier *notifier);
    
    bool startWithAPFSUUID(OSObject *uuid);
    bool checkAPFSUUID(void *ref, IOService *service, IONotifier *notifier);

    bool startWithBootUUID(OSObject *uuid);
    bool checkBootUUID(void *ref, IOService *service, IONotifier *notifier);
    
    bool publishBootMediaAndTerminate(IOMedia *media); // calls release()
    
    static IOReturn readHFSUUID(IOMedia *media, void **uuidPtr);

    static uint16_t getAPFSRoleForVolume(IOMedia *media);
    static OSString *copyAPFSContainerUUIDForVolume(IOMedia *media);
    static bool volumeMatchesAPFSUUID(IOMedia *media, OSString *uuid, bool matchAnyRoleInGroup = false);
    static bool volumeMatchesAPFSContainerUUID(IOMedia *media, OSString *uuid);
    static bool volumeMatchesAPFSRole(IOMedia *media, uint16_t role);
    IONotifier *startMatchingForAPFSVolumes(IOServiceMatchingNotificationHandler callback);
        
};

#endif /* KERNEL */
#endif /* __cplusplus */

