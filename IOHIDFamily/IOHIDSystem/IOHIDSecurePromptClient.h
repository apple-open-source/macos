/*
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2010 Apple Computer, Inc.  All Rights Reserved.
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


#ifndef _IOKIT_IOHIDSECUREPROMPTCLIENT_H
#define _IOKIT_IOHIDSECUREPROMPTCLIENT_H

#include <libkern/c++/OSContainers.h>
#include <IOKit/IOUserClient.h>

struct IOHIDSecurePromptClient_ExpansionData;

#ifndef sub_iokit_hidsystem
#define sub_iokit_hidsystem                     err_sub(14)
#endif

class IOHIDSecurePromptClient : public IOUserClient
{
    OSDeclareDefaultStructors(IOHIDSecurePromptClient)
    
private:
    IOHIDSecurePromptClient_ExpansionData   *_reserved;
    
public:
    // IOService methods
    void                free();
    bool                start( IOService * provider );
    
    // IOUserClient methods
    bool                initWithTask(task_t owningTask, 
                                     void * securityToken, 
                                     UInt32 type,
                                     OSDictionary * properties);

    IOReturn			clientClose( void );
    
    IOExternalMethod*	getTargetAndMethodForIndex(IOService ** targetP, 
                                                   UInt32 index );
//  IOExternalAsyncMethod*  getAsyncTargetAndMethodForIndex(IOService ** targetP, UInt32 index );
    IOReturn            registerNotificationPort(mach_port_t port, 
                                                 UInt32 type, 
                                                 io_user_reference_t refCon);

    
    // IOHIDSecurePromptClient in kernel methods
    bool                gathering();
    bool                dead();
    void                setNotifier(IONotifier *notifier);
    
    IOReturn            postKey(UInt32 key, bool down);
    IOReturn            postKeyGated(void * p1, void * p2, void * p3,void * p4);
    
    // IOHIDSecurePromptClient external methods
    IOReturn            setGatheringMethod(void * p1, void * p2, void * p3,
                                           void * p4, void * p5, void * p6 );
    IOReturn            setGathering(UInt32 state);
    
    IOReturn            getGatheringMethod(void * p1, void * p2, void * p3,
                                           void * p4, void * p5, void * p6 );
    
    IOReturn            setLayoutMethod(void * p1, void * p2, void * p3,
                                       void * p4, void * p5, void * p6 );
    IOReturn            setLayoutGated(void * p1, void * p2, void * p3,void * p4);
    IOReturn            setLayout(UInt32 layout);
    
    IOReturn            getLayoutMethod(void * p1, void * p2, void * p3,
                                        void * p4, void * p5, void * p6 );

    IOReturn            confirmKeyMethod(void * p1, void * p2, void * p3,
                                         void * p4, void * p5, void * p6 );
    IOReturn            confirmKeyGated(void * p1, void * p2, void * p3,void * p4);
    IOReturn            confirmKey(UInt32 id, UInt32 *count);
    
    IOReturn            deleteKeysMethod(void * p1, void * p2, void * p3,
                                        void * p4, void * p5, void * p6 );
    IOReturn            deleteKeysGated(void * p1, void * p2, void * p3,void * p4);
    IOReturn            deleteKeys(SInt32 index, UInt32 count, UInt32 *length);

    IOReturn            setUUIDMethod(void * p1, void * p2, void * p3,
                                      void * p4, void * p5, void * p6 );
    IOReturn            setUUIDGated(void * p1, void * p2, void * p3,void * p4);
    IOReturn            setUUID(UInt8* bytes_in);
    
    IOReturn            getUUIDMethod(void * p1, void * p2, void * p3,
                                      void * p4, void * p5, void * p6 );
    IOReturn            getUUIDGated(void * p1, void * p2, void * p3, void * p4);
    
    IOReturn            compareClientMethod(void * p1, void * p2, void * p3,
                                            void * p4, void * p5, void * p6 );
    
    IOReturn            getIdentifierMethod(void * p1, void * p2, void * p3,
                                            void * p4, void * p5, void * p6 );
    uint64_t            identifier();

    IOReturn            getInsertionPointMethod(void * p1, void * p2, void * p3,
                                                void * p4, void * p5, void * p6 );
    uint64_t            getInsertionPoint();
    
    IOReturn            setInsertionPointMethod(void * p1, void * p2, void * p3,
                                                void * p4, void * p5, void * p6 );
    IOReturn            setInsertionPointGated(void * p1, void * p2, void * p3,void * p4);
    
    IOReturn            injectStringMethod(void * p1, void * p2, void * p3,
                                           void * p4, void * p5, void * p6 );
    IOReturn            injectStringGated(void * p1, void * p2, void * p3,void * p4);
    
    static IOHIDSecurePromptClient*
                        nextForIterator(OSIterator * iterator);

    enum {
        clientID        = 0x48535043, // HSPC
        keyMessage      = 0x48535043, // HSPC
        gatheringMessage= iokit_family_msg(sub_iokit_hidsystem, 4),
    };
    
private:
    void                queueMessage(UInt8 code);
    IOReturn            appendConfirmedKeyCode(UInt8 modifier, UInt8 code);
    void                releaseReserved();
    bool                valid();
    UInt8               modifierState();
    bool                modifierDown(UInt8 modifierFlag);
    void                sync();
    IOReturn            syncGated(void * p1, void * p2, void * p3,void * p4);
    IOReturn            ensureBufferSize(UInt32 size);

    
};

#endif /* ! _IOKIT_IOHIDSECUREPROMPTCLIENT_H */
