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


#define DEBUG_SECURE_PROMPT 0
#if DEBUG_SECURE_PROMPT
#   define DEBUG_ASSERT_PRODUCTION_CODE         0
#   define DEBUG_ASSERT_COMPONENT_NAME_STRING   "IOHIDSecurePromptClient"
#endif

#include <AssertMacros.h>
#include <IOKit/IOLib.h>
#include <sys/proc.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/hidsystem/IOHIKeyboardMapper.h>
#include <IOKit/crypto/AppleFDEKeyStore.h>
#include <uuid/uuid.h>
#include "IOHIDSecurePromptClient.h"

/******************************************************************************/
/*  Helper functions
 */
static void         __EraseData(OSData *target);
static void         __EraseDataArray(OSArray *target);
static void         __EraseMemory(void *mem, UInt32 size);
static void         __InsertBytes(void *dest, UInt32 insertPoint, UInt32 destCount, void *source, UInt32 sourceCount, UInt32 size);
                                
/******************************************************************************/
/*  Through the course of this file, the word hash is used to mean a cryprographically
    secure one way hash
 */
extern unsigned int hid_adb_2_usb_keymap[];  //In Cosmo_USB2ADB.cpp

#undef super
#define super IOUserClient

OSDefineMetaClassAndStructors(IOHIDSecurePromptClient, IOUserClient)

/******************************************************************************/
/*  The IOHIDSecurePromptClient_RawKeystrokeData stores raw keycodes for use later 
    if we need to generate a new set of UTF32Chars for comparison.
 */
typedef struct IOHIDSecurePromptClient_RawKeystrokeData {
    UInt8               modifier[4];
    UInt8               code[4];
}   IOHIDSecurePromptClient_RawKeystrokeData;

#ifndef UTF32Char
    typedef UInt32      UTF32Char;
#endif

/*  The keyqueue message is sent to the subscribee letting them know that a new
    keystroke has been queued up.
 */
typedef struct IOHIDSecurePromptClient_KeyqueueMessage {
    mach_msg_header_t   message;
    mach_msg_body_t     body;
    UInt32              id;
}   IOHIDSecurePromptClient_KeyqueueMessage;

/*  The keyqueue entry stores the correspondence between the id sent via the 
    keyqueue message and the actual keycode delivered.
 */
typedef struct IOHIDSecurePromptClient_KeyqueueEntry {
    UInt16          id;
    UInt8           modifier;
    UInt8           code;
}   IOHIDSecurePromptClient_KeyqueueEntry;

/*  The IOHIDSecurePromptClient_ExpansionData structure stores all of the instance data for
    the IOHIDSecurePromptClient class.
 */
typedef struct IOHIDSecurePromptClient_ExpansionData {
    UInt8           gathering;          // Are we currently gathering keystrokes?
    UInt8           dead;               // Has this client been closed or abandoned?
    UInt8           uuidState;          // See kUUIDState* below
    UInt16          nextQueueID;        // Used to keyqueue ids above
    UInt16          insertionPoint;     // The insertion point for new characters
    UInt16          stringLength;       // Valid data length in the unicode and raw keystroke buffers
    UInt16          bufferLength;       // Maximum size of the unicode and raw keystroke buffers
    UInt32          layout;             // The keyboard layout used for code ->> value translation
    OSData          *layouts;           // All of the keyboard layouts
                                        // probably won't be an OSData in the end
    IONotifier      *keyboardNotifier;  // This is the notifier set up by the instantiating class
                                        // the client is responsible for releasing it.
    mach_port_t     port;               // The notification port supplied by the client user
    UTF32Char       *unicode;           // This is a buffer of UTF32Char's
    IOHIDSecurePromptClient_RawKeystrokeData
                    *rawKeystrokes;     // This is a buffer of IOHIDSecurePromptClient_RawKeystrokeData's
    OSArray         *messageQueue;      // A queue of all of the key ids that have been sent out but not yet gotten back
    uuid_t          uuid;         	  	// The uuid is used to idetify the password in the keystore. Therefore, if two
                                        // clients have the same uuid, they identify the same password. Care must be taken
                                        // so that two clients do not *modify* the same password, however. That would be bad.
                                        // This is the reason for uuidState above and kUUIDState* below.
    IOCommandGate   *gate;              // The command gate used to serialize data coming from all
                                        // of the keyboards and the client user.
    kbdBitVector    keyState;           // A bitvector to store the key state information
    IOByteCount     keyStateSize;       // The size of said bitvector
    
}   IOHIDSecurePromptClient_ExpansionData;

enum {
    kUUIDStateNeverSaved,   // client has no coresponding keystore entry
    kUUIDStateIsDirty,      // client has a keystore entry, but it is not in sync with internal data
    kUUIDStateIsClean,      // client and keystore are in sync
    kUUIDStateIsGhost       // client ghosts a keystore entry. It cannot gather new data.
};
enum {
    kControlModifier = 0x1,
    kShiftModifier   = 0x2,
    kOptionModifier  = 0x4,
    kCommandModifier = 0x8    
};




/******************************************************************************/
bool
IOHIDSecurePromptClient::initWithTask(task_t owningTask, 
                                      void* security_id,
                                      UInt32 type,
                                      OSDictionary * properties)
{
    require(super::initWithTask(owningTask, security_id, type), init_error);
    
    _reserved = (IOHIDSecurePromptClient_ExpansionData*)IOMalloc(sizeof(IOHIDSecurePromptClient_ExpansionData));
    require(_reserved, init_error);

    bzero(_reserved, sizeof(IOHIDSecurePromptClient_ExpansionData));
    
    // initially we allow for a 32 character password but will grow it if needed
    require_noerr(ensureBufferSize(32), init_error);
    
    _reserved->messageQueue = OSArray::withCapacity(4);
    require(_reserved->messageQueue, init_error);
    
    _reserved->keyStateSize = 4*((NX_NUMKEYCODES+(EVK_BITS_PER_UNIT-1))/EVK_BITS_PER_UNIT);
    _reserved->keyState = (kbdBitVector) IOMalloc(_reserved->keyStateSize);
    bzero(_reserved->keyState, _reserved->keyStateSize);
    require(_reserved->keyState, init_error);
    uuid_generate(_reserved->uuid);

#if DEBUG_SECURE_PROMPT 
#warning remove
    uuid_string_t string;
    uuid_unparse(_reserved->uuid, string);
    IOLog("%s made UUID %s\n", __func__, string);
#endif
    
    _reserved->uuidState = kUUIDStateNeverSaved;
    
// vtn3 TODO: setup layouts
           
    return super::initWithTask(owningTask, security_id, type, properties);

init_error: 
    releaseReserved();
    return false;
}

/******************************************************************************/
bool
IOHIDSecurePromptClient::start(IOService * provider)
{
    if (!_reserved->gate) {
        IOWorkLoop *loop = provider->getWorkLoop();
        require(loop, start_error);
        
        _reserved->gate = IOCommandGate::commandGate(this);
        require(_reserved->gate, start_error);
                
        require(kIOReturnSuccess == loop->addEventSource(_reserved->gate), start_error); 
    }
    
    require(valid(), start_error);

    return super::start(provider);
    
start_error:
    return false;
}

/******************************************************************************/
void
IOHIDSecurePromptClient::releaseReserved()
{
    if (_reserved) {
        if (_reserved->gate) {
            _reserved->gate->release();
            _reserved->gate = NULL;
        }
        if (_reserved->layouts) {
            _reserved->layouts->release();
            _reserved->layouts = NULL;
        }
        
        ensureBufferSize(0); // special case erase
        
        if (_reserved->keyboardNotifier) {
            _reserved->keyboardNotifier->remove();
            _reserved->keyboardNotifier = NULL;
        }
        if (_reserved->keyState) {
            IOFree( _reserved->keyState, _reserved->keyStateSize);
            _reserved->keyState = NULL;
            _reserved->keyStateSize = 0;
        }
        IOFree(_reserved, sizeof(IOHIDSecurePromptClient_ExpansionData));
        _reserved = NULL;
    }    
}

/******************************************************************************/
void
IOHIDSecurePromptClient::free() 
{
    sync();
    releaseReserved();
    super::free();
}

/******************************************************************************/
bool
IOHIDSecurePromptClient::gathering() 
{
    require(valid(), uninitialized_data);
    return _reserved->gathering;

uninitialized_data:
    return false;
}

/******************************************************************************/
bool
IOHIDSecurePromptClient::dead() 
{
    require(_reserved, uninitialized_data);
    return _reserved->dead;

uninitialized_data:
    return true;
}

/******************************************************************************/
void
IOHIDSecurePromptClient::setNotifier(IONotifier *notifier)
{
    require(valid(), exit_early);
    if (_reserved->keyboardNotifier)
        _reserved->keyboardNotifier->remove();

    _reserved->keyboardNotifier = notifier;
    if (notifier)
        notifier->retain();
    
exit_early:
    ;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::postKey(UInt32 key, 
                                 bool down)
{
    uintptr_t p1 = key;
    uintptr_t p2 = down;
    require(valid(), uninitialized_data);
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                               this,
                                                               &IOHIDSecurePromptClient::postKeyGated),
                                          (void*)p1, (void*)p2);
uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
/*  There is some debate about using the HID keys or the old ADB keys that come
    into the function. Everything that comes in right now will be goes from HID
    to ADB and back to HID.
 */
IOReturn
IOHIDSecurePromptClient::postKeyGated(void * p1, void * p2, void *, void *)
{
    UInt32 key = (uintptr_t)p1;
    bool down = (uintptr_t)p2;
    bool keep = false;
    bool wasDown = false;
    UInt8 hidCode = key < 0x80 ? hid_adb_2_usb_keymap[key] : 0xff;
    IOReturn result = kIOReturnInternalError;

    if (dead()) {
        return kIOReturnNotOpen;
    }
    if (!gathering()) {
        return kIOReturnNotReady;
    }
    require(valid(), finished);
    
    result = kIOReturnBadArgument;
    require(key < 0xff, finished);
    
    wasDown = EVK_IS_KEYDOWN(key, _reserved->keyState);
    
    // record all presses, even if we don't suppress them
    if (down) {
        if (!wasDown) {
            EVK_KEYDOWN(key, _reserved->keyState);            
        }
    }
    else {
        if (wasDown) {
            EVK_KEYUP(key, _reserved->keyState);            
        }
        // suppress ups
        result = kIOReturnSuccess;
        goto finished;
    }
    
    if (modifierDown(kControlModifier | kCommandModifier)) {
        // control or command and we don't use it
        goto finished;
    }
    
    if ((hidCode >= 0x04) && (hidCode <= 0x27)) // a-z, 0-9
        keep = true;
    if ((hidCode >= 0x2c) && (hidCode <= 0x38)) // space, [, ], etc.
        keep = true;
    if ((hidCode >= 0x54) && (hidCode <= 0x63)) // most keypad
        keep = true;
    if ((hidCode >= 0x85) && (hidCode <= 0x86)) // keypad comma, =
        keep = true;
    
    if (keep && down && !wasDown) {
        // This will suppress all repeats and multiple keypresses
        queueMessage(key);
        // The secure client has claimed this key for its own. Suppress.
        result = kIOReturnSuccess;
    }
    else {
        // this is a key that we do not want to suppress.        
    }
    
finished:
    return result;
}

/******************************************************************************/
void
IOHIDSecurePromptClient::queueMessage(UInt8 code)
{
    require(valid(), uninitialized_data);
    // This block is for scoping purposes only.
    {
        // set up all the data structures
        UInt8 modifier = modifierState();
        UInt16 id = _reserved->nextQueueID++;
        IOHIDSecurePromptClient_KeyqueueEntry entry = {
            id,
            modifier,
            code
        };
        IOHIDSecurePromptClient_KeyqueueMessage msg = {
            {
                MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND,0),
                sizeof(IOHIDSecurePromptClient_KeyqueueMessage),
                _reserved->port,
                MACH_PORT_NULL,
                0,
                keyMessage
            },
            {
                0
            },
            id
        };
        
        // add the message to our internal queue
        OSData          *entryData = OSData::withBytes(&entry, sizeof(entry));
        require(entryData, out_of_memory);
        _reserved->messageQueue->setObject(entryData);
        entryData->release();
        
        // and finally send the message
        kern_return_t ret = mach_msg_send_from_kernel(&msg.message, msg.message.msgh_size);
        switch ( ret ) {
            case MACH_MSG_SUCCESS: /* Message is posted */
                // good
                break;
            case MACH_SEND_TIMED_OUT: /* Already has a message posted */
                // bad
                IOLog("%s: Unexpected response to mach_msg_send_from_kernel\n", __func__);
                break;
            default: /* Log the error */
                // ugly
                IOLog("%s: Very unexpected response to mach_msg_send_from_kernel: 0x%08x\n", __func__, ret);
                break;
        }
    }
out_of_memory:
uninitialized_data:
    ;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::clientClose() 
{
    OSIterator *itr = NULL;
    require(_reserved, uninitialized_data);
    sync();
    _reserved->gathering = false;
    _reserved->dead = true;

    // notify all providers of the state change
    itr = getProviderIterator();
    if (itr) {
        bool done = false;
        while (!done) {
            OSObject *provider;
            while (!done && (NULL != (provider = itr->getNextObject()))) {
                IOService *parent = OSDynamicCast(IOService, provider);
                detach(parent);
            }
            if (itr->isValid()) {
                done = true;
            }
            else {
                // we may end up sending the mseeage to the same provider twice.
                // that should not cause any issues.
                itr->reset();
            }
        }
        itr->release();
    }
    
uninitialized_data:
    return super::clientClose();
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::registerNotificationPort(mach_port_t port, 
                                                  UInt32 type, 
                                                  io_user_reference_t refCon)
{
    require(valid(), uninitialized_data);
    // A type of 1 was chosen for no particular reason.
    if (type == 1) {
        _reserved->port = port;
        return kIOReturnSuccess;
    }
uninitialized_data:
    return super::registerNotificationPort(port, type, refCon);
}

/******************************************************************************/
IOExternalMethod *
IOHIDSecurePromptClient::getTargetAndMethodForIndex(IOService ** targetP, 
                                                    UInt32 index)
{
    static const IOExternalMethod methodTemplate[] = {
        // 0: kIOHIDSecurePromptClient_setGatheringMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::setGatheringMethod,	kIOUCScalarIScalarO, 1, 0 },
        // 1: kIOHIDSecurePromptClient_setLayoutMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::setLayoutMethod,	kIOUCScalarIScalarO, 1, 0 },
        // 2: kIOHIDSecurePromptClient_confirmKeyMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::confirmKeyMethod, 	kIOUCScalarIScalarO, 1, 1 },
        // 3: kIOHIDSecurePromptClient_getGatheringMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::getGatheringMethod, kIOUCScalarIScalarO, 1, 0 },
        // 4: kIOHIDSecurePromptClient_deleteKeysMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::deleteKeysMethod, 	kIOUCScalarIScalarO, 2, 1 },
        // 5: kIOHIDSecurePromptClient_getLayoutMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::getLayoutMethod, 	kIOUCScalarIScalarO, 1, 0 },
        // 6: kIOHIDSecurePromptClient_getIdentifierMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::getIdentifierMethod, kIOUCScalarIScalarO, 0, 1 },
        // 7: kIOHIDSecurePromptClient_compareClientMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::compareClientMethod, kIOUCScalarIScalarO, 1, 0 },
        // 8: kIOHIDSecurePromptClient_setUUIDMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::setUUIDMethod, 		kIOUCStructIStructO, sizeof(uuid_t), 0 },
        // 9: kIOHIDSecurePromptClient_getUUIDMethod
        { NULL, (IOMethod)&IOHIDSecurePromptClient::getUUIDMethod, 		kIOUCStructIStructO, 0, sizeof(uuid_t) },
        // 10: kIOHIDSecurePromptClient_getInsertionPoint
        { NULL, (IOMethod)&IOHIDSecurePromptClient::getInsertionPointMethod,  kIOUCScalarIScalarO, 0, 1 },
        // 11: kIOHIDSecurePromptClient_setInsertionPoint
        { NULL, (IOMethod)&IOHIDSecurePromptClient::setInsertionPointMethod,  kIOUCScalarIScalarO, 1, 0 },
        // 12: kIOHIDSecurePromptClient_injectString
        { NULL, (IOMethod)&IOHIDSecurePromptClient::injectStringMethod, kIOUCStructIStructO, kIOUCVariableStructureSize, 0 },       
        // 13: redacted
        { NULL, NULL, 0, 0, 0 },
    };
    
    require(valid(), uninitialized_data);
    require(index < (sizeof(methodTemplate) / sizeof(methodTemplate[0])), bad_argument);
    
    *targetP = this;
    return( (IOExternalMethod *)(methodTemplate + index) );
    
uninitialized_data:
bad_argument:
    return NULL;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setGatheringMethod(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused, void * p5 __unused, void * p6 __unused)
{
    UInt32 state = (uintptr_t)p1;
    return setGathering(state);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setGathering(UInt32 state) 
{
    uintptr_t tempState = state ? true : false;
    require(valid(), uninitialized_data);
    require(_reserved->uuidState != kUUIDStateIsGhost, uninitialized_data);
        
    if (gathering() != tempState) {
        _reserved->gathering = tempState;
        
        // notify all providers of the state change
        OSIterator *itr = getProviderIterator();
        if (itr) {
            bool done = false;
            while (!done) {
                OSObject *provider;
                while (!done && (NULL != (provider = itr->getNextObject()))) {
                    IOService *parent = OSDynamicCast(IOService, provider);
                    parent->message(gatheringMessage, this, (void*)tempState);
                }
                if (itr->isValid()) {
                    done = true;
                }
                else {
                    // we may end up sending the mseeage to the same provider twice.
                    // that should not cause any issues.
                    itr->reset();
                }
            }
            itr->release();
        }
    }

// vtn3 TODO: release local modifier flags
    
    // clear out any queued keystrokes
    if (_reserved->messageQueue) {
    	__EraseDataArray(_reserved->messageQueue);
        _reserved->messageQueue->release();
    }
    _reserved->messageQueue = OSArray::withCapacity(4);
    
    return kIOReturnSuccess;
    
uninitialized_data:
    return kIOReturnNotOpen;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setLayoutMethod(void * p1, void * p2, void * p3, void * p4, void * p5 __unused, void * p6 __unused)
{
    require(valid(), uninitialized_data);
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                               this,
                                                               &IOHIDSecurePromptClient::setLayoutGated),
                                          p1, p2, p3, p4);
uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setLayoutGated(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused)
{
    UInt32 layout = (uintptr_t)p1;
    return setLayout(layout);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setLayout(UInt32 layout)
{
    require(valid(), uninitialized_data);
    require(_reserved->uuidState != kUUIDStateIsGhost, uninitialized_data);

    if (_reserved->uuidState == kUUIDStateIsClean)
        _reserved->uuidState = kUUIDStateIsDirty;
    
// vtn3 TODO: validate layout entry

    _reserved->layout = layout;

    // clear out any queued keystrokes
    if (_reserved->messageQueue) {
        __EraseDataArray(_reserved->messageQueue);
        _reserved->messageQueue->release();
    }
    _reserved->messageQueue = OSArray::withCapacity(4);
    
    // vtn3 TODO: release local modifier flags
    // vtn3 TODO: remap value queue
    
    return kIOReturnSuccess;

uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::confirmKeyMethod(void * p1, void * p2, void * p3, void * p4, void * p5 __unused, void * p6 __unused)
{
    require(valid(), uninitialized_data);
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                               this,
                                                               &IOHIDSecurePromptClient::confirmKeyGated),
                                          p1, p2, p3, p4);
uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::confirmKeyGated(void * p1, void * p2, void * /* p3 */, void * /* p4 */)
{
    UInt32 id = (uintptr_t)p1;
    return confirmKey(id, (UInt32*)p2);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::confirmKey(UInt32 id, 
                                    UInt32 *count)
{
    unsigned int index = 0;
    IOReturn result = kIOReturnBadArgument;
    OSCollectionIterator *iterator = NULL;

    require(valid(), uninitialized_data);
    require(_reserved->uuidState != kUUIDStateIsGhost, uninitialized_data);
    result = kIOReturnInternalError;
    require(count, invalid_argument);
    result = kIOReturnBadMessageID;
    
    // Search through the queue for the supplied ID
    iterator = OSCollectionIterator::withCollection(_reserved->messageQueue);
    require(iterator, finished);
    do {
        OSObject * obj = iterator->getNextObject();
        if (!obj) {
            if (iterator->isValid()) {
                goto finished;
            }
            else {
                iterator->reset();
            }
        }
        else {
            OSData *entryData = OSDynamicCast(OSData, obj);
            IOHIDSecurePromptClient_KeyqueueEntry *entry = NULL;
            if (entryData == NULL) {
                IOLog("%s: Found bad index in messageQueue: %d\n", __func__, index);
                continue;
            }
            entry = (IOHIDSecurePromptClient_KeyqueueEntry*)entryData->getBytesNoCopy();
            if (entry->id == id) {
                // We have a match. Append it and remove it from the queue.
                result = appendConfirmedKeyCode(entry->modifier, entry->code);
                __EraseData(entryData);
                _reserved->messageQueue->removeObject(index);
                
                // Now set the return count
                *count = _reserved->stringLength;
                goto finished;
            }
        }
    }
    while (true);

finished:
invalid_argument:
uninitialized_data:
    if (iterator)
        iterator->release();

    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::appendConfirmedKeyCode(UInt8 modifier, 
                                                UInt8 code)
{
    // Remap the zero key (a) to be an invalid key code
    IOReturn    result = kIOReturnInternalError;
    UInt8       newCode = code ? code : 0xff;
    IOHIDSecurePromptClient_RawKeystrokeData newKeystroke = {{ modifier }, { newCode }};
    
    // vtn3 TODO: get real value here.
    UTF32Char   newUnicode = (modifier << 16) | newCode;
    require(valid(), exit_early);
    
    if (_reserved->uuidState == kUUIDStateIsClean)
        _reserved->uuidState = kUUIDStateIsDirty;
    
    // vtn3 TODO: do something intelegent here.
    // if there is a previous key code
    // if it combines
    // replace the old entry
    // otherwise 
    {
        // append the new entry
        result = kIOReturnNoMemory;
        require_noerr(ensureBufferSize(_reserved->stringLength + 1), exit_early);
        __InsertBytes(_reserved->unicode, _reserved->insertionPoint, _reserved->stringLength, &newUnicode, 1, sizeof(newUnicode));
        __InsertBytes(_reserved->rawKeystrokes, _reserved->insertionPoint, _reserved->stringLength, &newKeystroke, 1, sizeof(newKeystroke));
        __EraseMemory(&newKeystroke, sizeof(newKeystroke));
        __EraseMemory(&newUnicode, sizeof(newUnicode));
        _reserved->insertionPoint++;
        _reserved->stringLength++;
        //IOLog("%s insertionPoint = %d; stringLength = %d; bufferLength = %d\n", __func__, _reserved->insertionPoint, _reserved->stringLength, _reserved->bufferLength);
    }
    result = kIOReturnSuccess;
    
exit_early:
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::deleteKeysMethod(void * p1, void * p2, void * p3, void * p4, void * p5 __unused, void * p6 __unused)
{
    require(valid(), uninitialized_data);
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                           this,
                                                           &IOHIDSecurePromptClient::deleteKeysGated),
                                      p1, p2, p3, p4);
uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::deleteKeysGated(void * p1, void * p2, void * p3, void * p4 __unused)
{
    UInt32 length;
    IOReturn result;
    require(p3, invalid_argument);
    result = deleteKeys((intptr_t)p1, (uintptr_t)p2, &length);
    *(UInt64*)p3 = length;
    return result;
    
invalid_argument:
    return kIOReturnBadArgument;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::deleteKeys(SInt32 index, 
                                    UInt32 count, 
                                    UInt32 *newLength)
{
    require(valid(), uninitialized_data);
    require(_reserved->uuidState != kUUIDStateIsGhost, uninitialized_data);
    {
        SInt32 deleteBegin = index;
        
        // in case of early return, make sure newLength returns the length
        *newLength = _reserved->stringLength;
        
        if ((count == 0) || (_reserved->stringLength == 0)) {
            // this is a valid exit early case
            return kIOReturnSuccess;            
        }
        
        if (deleteBegin < 0) {
            // delete from the end
            deleteBegin = _reserved->stringLength - count;
            if (deleteBegin < 0)
                deleteBegin = 0;
        }
        require(_reserved->stringLength > (unsigned)deleteBegin, overrun);
        
        if (((SInt64)count) > _reserved->stringLength - deleteBegin) {
            count = _reserved->stringLength - deleteBegin;
        }
        
        memmove(_reserved->unicode + deleteBegin,
                _reserved->unicode + (deleteBegin + count),
                (_reserved->stringLength - deleteBegin - count) * sizeof(UTF32Char));
        memmove(_reserved->rawKeystrokes + deleteBegin, 
                _reserved->rawKeystrokes + (deleteBegin + count), 
                (_reserved->stringLength - deleteBegin - count) * sizeof(IOHIDSecurePromptClient_RawKeystrokeData));
        
        // note it is dirty
        if (_reserved->uuidState == kUUIDStateIsClean)
            _reserved->uuidState = kUUIDStateIsDirty;

        _reserved->stringLength -= count;
        *newLength = _reserved->stringLength;
        
        // adjust insertion point
        if (_reserved->insertionPoint > index) {
            _reserved->insertionPoint -= count;
            if (_reserved->insertionPoint < index) {
                _reserved->insertionPoint = index;
            }
        }
        
        //IOLog("%s insertionPoint = %d; stringLength = %d; bufferLength = %d\n", __func__, _reserved->insertionPoint, _reserved->stringLength, _reserved->bufferLength);

        return kIOReturnSuccess;
    }
uninitialized_data:
    return kIOReturnInternalError;
overrun:
    return kIOReturnOverrun;
}

/******************************************************************************/
bool
IOHIDSecurePromptClient::valid()
{
    require(_reserved, invalid);
// vtn3 TODO: require layouts
//    require(_reserved->layouts, invalid);
    nrequire(_reserved->dead, invalid);
    require(_reserved->unicode, invalid);
    require(_reserved->rawKeystrokes, invalid);
    require(_reserved->messageQueue, invalid);
    require(_reserved->gate, invalid);
    require(_reserved->keyState, invalid);
    require(_reserved->insertionPoint <= _reserved->stringLength, invalid);
    
    return true;
invalid:
    return false;
}

/******************************************************************************/
#define CONTROL_DOWN(X)     (EVK_IS_KEYDOWN(0x3b, X) || EVK_IS_KEYDOWN(0x3e, X)) // ADB Control keys
#define SHIFT_DOWN(X)       (EVK_IS_KEYDOWN(0x38, X) || EVK_IS_KEYDOWN(0x3c, X)) // ADB Shift keys
#define OPTION_DOWN(X)      (EVK_IS_KEYDOWN(0x3a, X) || EVK_IS_KEYDOWN(0x3d, X)) // ADB Alt keys
#define COMMAND_DOWN(X)     (EVK_IS_KEYDOWN(0x37, X) || EVK_IS_KEYDOWN(0x36, X)) // ADB GUI keys

/******************************************************************************/
UInt8
IOHIDSecurePromptClient::modifierState()
{
    UInt8 result = (CONTROL_DOWN(_reserved->keyState)   ? kControlModifier  : 0) |
                   (SHIFT_DOWN(_reserved->keyState)     ? kShiftModifier    : 0) |
                   (OPTION_DOWN(_reserved->keyState)    ? kOptionModifier   : 0) |
                   (COMMAND_DOWN(_reserved->keyState)   ? kCommandModifier  : 0);
    return result;
}

/******************************************************************************/
bool
IOHIDSecurePromptClient::modifierDown(UInt8 modifierFlag)
{
    bool result = (modifierState() & modifierFlag) ? true : false;
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::getIdentifierMethod(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused, void * p5 __unused, void * p6 __unused) 
{
    require(valid() && p1, uninitialized_data);
    
    *(UInt64*)p1 = identifier();
    return kIOReturnSuccess;
    
uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
uint64_t
IOHIDSecurePromptClient::identifier()
{
    uint64_t result = 0xffffffff & (uint64_t)this;
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::compareClientMethod(void * p1, void *, void *, void *, void *, void *)
{
    // We deviate from our idiom above because we do not wish to hold both
    // gates at the same time. Probably would not cause a problem, but it is possible,
    OSData          *tempData = NULL;
    IOReturn        result = kIOReturnInternalError;
    uint64_t        targetID = (uintptr_t)p1;
    OSIterator      *siblings = NULL;
    IORegistryEntry *parent = NULL;
    IOHIDSecurePromptClient *target = NULL;
    uuid_t          targetUUID;
    uuid_t          selfUUID;
    UInt8           *targetBuffer = NULL;
    UInt8           *selfBuffer = NULL;
    uint32_t        targetBufferSize;
    uint32_t        selfBufferSize;
    
    require(valid(), uninitialized_data);
    require(AppleFDEKeyStore::instance, uninitialized_data);

    // find the target
    parent = getParentEntry(gIOServicePlane);
    require(parent, no_siblings);
    siblings = parent->getChildIterator(gIOServicePlane);
    require(siblings, no_siblings);
    
    while(NULL != (target = nextForIterator(siblings))) {
        if (target->identifier() == targetID) 
            break;
    }
    
    require(target, no_target);
    require(target->valid(), no_target);

    result = target->getUUIDMethod(targetUUID, NULL, NULL, NULL, NULL, NULL);
    require(result == kIOReturnSuccess, no_target);
    
    result = getUUIDMethod(selfUUID, NULL, NULL, NULL, NULL, NULL);
    require(result == kIOReturnSuccess, no_target);

    sync();
    target->sync();

#if DEBUG_SECURE_PROMPT
#warning remove
    uuid_string_t string;
    uuid_unparse(selfUUID, string);
    IOLog("%s of UUID %s to", __func__, string);
    uuid_unparse(targetUUID, string);
    IOLog(" UUID %s", string);
#endif
    
    targetBuffer = (UInt8*)IOMalloc(AKS_MAX_PASSPHRASE_SIZE);
    selfBuffer = (UInt8*)IOMalloc(AKS_MAX_PASSPHRASE_SIZE);
    require(targetBuffer && selfBuffer, out_of_memory);
    
    // check the buffers
    targetBufferSize = AKS_MAX_PASSPHRASE_SIZE;
    result = AppleFDEKeyStore::instance->getPassphrase(targetUUID, targetBuffer, targetBufferSize, &targetBufferSize);
    require(result == kIOReturnSuccess, keystore_error);
    selfBufferSize = AKS_MAX_PASSPHRASE_SIZE;
    result = AppleFDEKeyStore::instance->getPassphrase(selfUUID, selfBuffer, selfBufferSize, &selfBufferSize);
    require(result == kIOReturnSuccess, keystore_error);
    
    if ((targetBufferSize == selfBufferSize) && (0 == memcmp(targetBuffer, selfBuffer, targetBufferSize))) {
        goto success;
    }
    
    // for each acceptable layout
        // setLayout on self in gate to that layout
        // sync
        // check the buffers (see above)
        
    // if you get here, you have failed
    result = kIOReturnNotPrivileged;
    
success:
keystore_error:
out_of_memory:
    if (targetBuffer) {
        __EraseMemory(targetBuffer, AKS_MAX_PASSPHRASE_SIZE);
        IOFree(targetBuffer, AKS_MAX_PASSPHRASE_SIZE);
    }
    if (selfBuffer) {
        __EraseMemory(selfBuffer, AKS_MAX_PASSPHRASE_SIZE);
        IOFree(selfBuffer, AKS_MAX_PASSPHRASE_SIZE);
    }
    
no_target:
    if (siblings) {
        siblings->release();
    }
    
no_siblings:
    if (tempData) {
        __EraseData(tempData);            
        tempData->release();        
    }
    
uninitialized_data:
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::getUUIDMethod(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused, void * p5 __unused, void * p6 __unused)
{
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                           this,
                                                           &IOHIDSecurePromptClient::getUUIDGated),
                                      p1, p2, p3, p4);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::getUUIDGated(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused)
{
    UInt8 *data = (UInt8*)p1;

    require(valid(), uninitialized_data);
    require(p1, uninitialized_data);
    
    if (_reserved->uuidState == kUUIDStateNeverSaved) 
        _reserved->uuidState = kUUIDStateIsDirty;
    
    bcopy(_reserved->uuid, data, sizeof(uuid_t));
    return kIOReturnSuccess;

uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setUUIDMethod(void * p1, void * p2, void * p3, void * p4, void * p5 __unused, void * p6 __unused)
{
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                           this,
                                                           &IOHIDSecurePromptClient::setUUIDGated),
                                      p1, p2, p3, p4);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setUUIDGated(void * p1, void * p2 __unused, void * p3 __unused,void * p4 __unused)
{
    require(valid(), uninitialized_data);
    require(p1, uninitialized_data);
    return setUUID((UInt8*)p1);

uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setUUID(UInt8* bytes_in)
{
    IOReturn result = kIOReturnNoMemory;
    uint32_t bufferSize = AKS_MAX_PASSPHRASE_SIZE;

    bcopy(bytes_in, _reserved->uuid, sizeof(uuid_t));
 
#if DEBUG_SECURE_PROMPT
#warning remove
    uuid_string_t string;
    uuid_unparse(_reserved->uuid, string);
    IOLog("%s set UUID to %s\n", __func__, string);
#endif
    
    if (_reserved->uuidState != kUUIDStateIsGhost) {
        setGathering(0);
        _reserved->uuidState = kUUIDStateIsGhost;
    }
    
    UInt8 *buffer = (UInt8*)IOMalloc(AKS_MAX_PASSPHRASE_SIZE);
    require(buffer, out_of_memory);
    
    // check the UUIDs
    bufferSize = AKS_MAX_PASSPHRASE_SIZE;
    result = AppleFDEKeyStore::instance->getPassphrase(_reserved->uuid, buffer, bufferSize, &bufferSize);
    require(result == kIOReturnSuccess, bad_uuid);
        
bad_uuid:
out_of_memory:
    if (buffer) {
		__EraseMemory(buffer, AKS_MAX_PASSPHRASE_SIZE);
		IOFree(buffer, AKS_MAX_PASSPHRASE_SIZE);
    }
    
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::getInsertionPointMethod(void * p1, void *, void *, void *, void *, void *)
{
    require(valid() && p1, uninitialized_data);
    
    *(uint64_t*)p1 = getInsertionPoint();
    return kIOReturnSuccess;
    
uninitialized_data:
    return kIOReturnInternalError;
}

/******************************************************************************/
uint64_t
IOHIDSecurePromptClient::getInsertionPoint()
{
    return _reserved->insertionPoint;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setInsertionPointMethod(void * p1, void * p2, void * p3,
                                                 void * p4, void * p5 __unused, void * p6 __unused)
{
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                           this,
                                                           &IOHIDSecurePromptClient::setInsertionPointGated),
                                      p1, p2, p3, p4);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::setInsertionPointGated(void * p1, void * p2 __unused, void * p3 __unused,void * p4 __unused)
{
    IOReturn result = kIOReturnBadArgument;
    uintptr_t value = (uintptr_t)p1;

    require(valid(), bad_argument);
    require(value < 0x0fff, bad_argument); // cannot set the insertion pointer past the 4000th character
    
    if (value == 0) {
        _reserved->insertionPoint = 0;
    }
    else {
        if (value >= _reserved->stringLength) {
            _reserved->insertionPoint = _reserved->stringLength;
        }
        else {
            _reserved->insertionPoint = value;
        }
    }   
    result = kIOReturnSuccess;
    
bad_argument:
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::injectStringMethod(void * p1, void * p2, void * p3,
                                            void * p4, void * p5 __unused, void * p6 __unused)
{
    return _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                           this,
                                                           &IOHIDSecurePromptClient::injectStringGated),
                                      p1, p2, p3, p4);
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::injectStringGated(void * p1, void * p2, void * p3 __unused,void * p4 __unused)
{
    IOReturn result = kIOReturnBadArgument;
    IOHIDSecurePromptClient_RawKeystrokeData * dummyRawData = NULL;
    UTF32Char *string = (UTF32Char*)p1;
    intptr_t length = (intptr_t)p2 / sizeof(UTF32Char);
    vm_size_t dummyDataSize = length * sizeof(IOHIDSecurePromptClient_RawKeystrokeData);
    
    require(valid(), bad_argument);
    require(p1, bad_argument);
    
    require((length > 0) && (length < 0x0fff), bad_argument); // not going to insert more than 4000 characters
    
    dummyRawData = (IOHIDSecurePromptClient_RawKeystrokeData*)IOMalloc(dummyDataSize);
    memset(dummyRawData, 0xff, dummyDataSize);
    
    __InsertBytes(_reserved->rawKeystrokes, _reserved->insertionPoint, _reserved->stringLength, string, length,  sizeof(UTF32Char));
    __InsertBytes(_reserved->unicode, _reserved->insertionPoint, _reserved->stringLength, dummyRawData, length,  sizeof(UTF32Char));
    __EraseMemory(string, length * sizeof(UTF32Char));
    _reserved->insertionPoint += length;
    result = kIOReturnSuccess;
    
bad_argument:
    if (dummyRawData)
        IOFree(dummyRawData, dummyDataSize);
    return result;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::getGatheringMethod(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused, void * p5 __unused, void * p6 __unused)
{
    require(valid(), uninitialized_data);
    require(p1, uninitialized_data);
    
    *(UInt64*)p1 = gathering();
    
    return kIOReturnSuccess;
    
uninitialized_data:
    *(UInt64*)p1 = false;
    
    return kIOReturnInternalError;
}

/******************************************************************************/
void
IOHIDSecurePromptClient::sync()
{
    if (_reserved && _reserved->gate && !_reserved->dead)
        _reserved->gate->runAction(OSMemberFunctionCast(IOCommandGate::Action, 
                                                        this,
                                                        &IOHIDSecurePromptClient::syncGated),
                                   0, 0, 0, 0);  
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::syncGated(void * p1 __unused, void * p2 __unused, void * p3 __unused,void * p4 __unused)
{
    if (_reserved && (_reserved->uuidState == kUUIDStateIsDirty) && valid()) {
        IOReturn result;
        
        // this will return an error if the passphrase does not exist. don't care.
        AppleFDEKeyStore::instance->deletePassphrase(_reserved->uuid);
        
        if (_reserved->stringLength > 0) {
            result = AppleFDEKeyStore::instance->setPassphrase(_reserved->uuid,
                                                               _reserved->unicode,
                                                               _reserved->stringLength * sizeof(UTF32Char));
            if (result != kIOReturnSuccess)
                IOLog("%s failed to setPassphrase for code: %08x\n", __func__, result);
        }
        
        _reserved->uuidState = kUUIDStateIsClean;
        
#if DEBUG_SECURE_PROMPT
#warning remove
        uuid_string_t string;
        uuid_unparse(_reserved->uuid, string);
        IOLog("%s on %s\n", __func__, string);
#endif
    }
    return kIOReturnSuccess;
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::getLayoutMethod(void * p1, void * p2 __unused, void * p3 __unused, void * p4 __unused, void * p5 __unused, void * p6 __unused)
{
    require(valid(), uninitialized_data);
    require(p1, uninitialized_data);
    
    *(UInt64*)p1 = _reserved->layout;
    
    return kIOReturnSuccess;
    
uninitialized_data:
    *(UInt64*)p1 = 0;
    
    return kIOReturnInternalError;
}

/******************************************************************************/
IOHIDSecurePromptClient*
IOHIDSecurePromptClient::nextForIterator(OSIterator * iterator) 
{
    IOHIDSecurePromptClient *client = NULL;
    do {
        OSObject * obj = iterator->getNextObject();
        if (!obj) {
            if (iterator->isValid()) {
                return NULL;
            }
            else {
                iterator->reset();
            }
        }
        else {
            client = OSDynamicCast(IOHIDSecurePromptClient, obj);            
        }
    }
    while (!client);
    
    return client;
}

/******************************************************************************/
void
__EraseData(OSData *target)
{
    if (target) {
        unsigned int size = target->getLength();
        if (size) {
            void *bytes = (void*)target->getBytesNoCopy();
            __EraseMemory(bytes, size);
        }
    }
}

/******************************************************************************/
void
__EraseMemory(void *mem, UInt32 size)
{
    if (mem) {
        // overwrite with 1s, then 0s, just in case. Just in case why? Just do it, K?
        memset(mem, 0xFF, size);
        memset(mem, 0x00, size);
    }
}

/******************************************************************************/
void __EraseDataArray(OSArray *target)
{
    OSCollectionIterator *iterator = NULL;
    require(target, finished);

    iterator = OSCollectionIterator::withCollection(target);
    require(iterator, finished);
    do {
        OSObject * obj = iterator->getNextObject();
        if (!obj) {
            if (iterator->isValid()) {
                goto finished;
            }
            else {
                iterator->reset();
            }
        }
        else {
            OSData *dict = OSDynamicCast(OSData, obj);
            __EraseData(dict);
        }
    }
    while (true);

finished:
    if (iterator)
        iterator->release();
}

/******************************************************************************/
IOReturn
IOHIDSecurePromptClient::ensureBufferSize(UInt32 size)
{
    IOReturn result = kIOReturnSuccess;
    UInt8 *oldBuffer = NULL;
    UInt32 oldBufferSize = 0;
    UInt8 *newBuffer = NULL;
    UInt8 *newKeystrokeOffset;
    UInt32 newBufferSize = 0;
    UInt32 newSize = size;
    
    if (_reserved->bufferLength >= size)
        goto finished;
    if (!size) {
        oldBuffer = (UInt8*)_reserved->unicode;
        oldBufferSize = _reserved->bufferLength * (sizeof(UTF32Char) + sizeof(IOHIDSecurePromptClient_RawKeystrokeData));
        goto finished;
    }
    
    // round size up to next power of two
    newSize--;
    newSize |= newSize >> 1;
    newSize |= newSize >> 2;
    newSize |= newSize >> 4;
    newSize |= newSize >> 8;
    newSize |= newSize >> 16;
    newSize++;    
            
    result = kIOReturnNoMemory;
    require(newSize < 1024, finished);
    
    newBufferSize = newSize * (sizeof(UTF32Char) + sizeof(IOHIDSecurePromptClient_RawKeystrokeData));
    newBuffer = (UInt8*)IOMalloc(newBufferSize);
    require(newBuffer, finished);
    newKeystrokeOffset = newBuffer + newSize * sizeof(UTF32Char);
    memcpy(newBuffer, _reserved->unicode, _reserved->stringLength * sizeof(UTF32Char));
    memcpy(newKeystrokeOffset, _reserved->rawKeystrokes, _reserved->stringLength * sizeof(IOHIDSecurePromptClient_RawKeystrokeData));
    oldBuffer = (UInt8*)_reserved->unicode;
    oldBufferSize = _reserved->bufferLength * (sizeof(UTF32Char) + sizeof(IOHIDSecurePromptClient_RawKeystrokeData));
    _reserved->unicode = (UTF32Char*)newBuffer;
    _reserved->rawKeystrokes = (IOHIDSecurePromptClient_RawKeystrokeData*)newKeystrokeOffset;
    _reserved->bufferLength = newSize;
    newBuffer = NULL;
    result = kIOReturnSuccess;
    
finished:
    if (newBuffer) 
        IOFree(newBuffer, newBufferSize);
    if (oldBuffer) {
        __EraseMemory(oldBuffer, oldBufferSize);
        IOFree(oldBuffer, oldBufferSize);
    }
    
    return result;
}

/******************************************************************************/
void
__InsertBytes(void *dest, UInt32 insertPoint, UInt32 destCount, void *source, UInt32 sourceCount, UInt32 size)
{
    UInt8 *destBytes = (UInt8*)dest;
    UInt8 *sourceBytes = (UInt8*)source;
    require(destBytes && sourceBytes && sourceCount && size, bad_parameter);
    if (insertPoint > destCount) {
        insertPoint = destCount;
    }
    if (insertPoint < destCount) {
        memmove(destBytes + (insertPoint + sourceCount) * size, destBytes + insertPoint * size, sourceCount * size);
    }
    memcpy(destBytes + insertPoint * size, sourceBytes, sourceCount * size);
    
bad_parameter:
    ;
}

/******************************************************************************/

/******************************************************************************/
