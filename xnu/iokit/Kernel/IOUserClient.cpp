/*
 * Copyright (c) 1998-2019 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <libkern/c++/OSKext.h>
#include <libkern/c++/OSSharedPtr.h>
#include <IOKit/IOKitServer.h>
#include <IOKit/IOKitKeysPrivate.h>
#include <IOKit/IOUserClient.h>
#include <IOKit/IOService.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/IOCatalogue.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOBSD.h>
#include <IOKit/IOStatisticsPrivate.h>
#include <IOKit/IOTimeStamp.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOUserServer.h>
#include <IOKit/system.h>
#include <libkern/OSDebug.h>
#include <DriverKit/OSAction.h>
#include <sys/proc.h>
#include <sys/kauth.h>
#include <sys/codesign.h>
#include <sys/code_signing.h>
#include <vm/vm_kern_xnu.h>

#include <mach/sdt.h>
#include <os/hash.h>

#include <libkern/amfi/amfi.h>

#if CONFIG_MACF

extern "C" {
#include <security/mac_framework.h>
};
#include <sys/kauth.h>

#define IOMACF_LOG 0

#endif /* CONFIG_MACF */

#include <IOKit/assert.h>

#include "IOServicePrivate.h"
#include "IOKitKernelInternal.h"

#define SCALAR64(x) ((io_user_scalar_t)((unsigned int)x))
#define SCALAR32(x) ((uint32_t )x)
#define ARG32(x)    ((void *)(uintptr_t)SCALAR32(x))
#define REF64(x)    ((io_user_reference_t)((UInt64)(x)))
#define REF32(x)    ((int)(x))

enum{
	kIOUCAsync0Flags          = 3ULL,
	kIOUCAsync64Flag          = 1ULL,
	kIOUCAsyncErrorLoggedFlag = 2ULL
};

#if IOKITSTATS

#define IOStatisticsRegisterCounter() \
do { \
	reserved->counter = IOStatistics::registerUserClient(this); \
} while (0)

#define IOStatisticsUnregisterCounter() \
do { \
	if (reserved) \
	        IOStatistics::unregisterUserClient(reserved->counter); \
} while (0)

#define IOStatisticsClientCall() \
do { \
	IOStatistics::countUserClientCall(client); \
} while (0)

#else

#define IOStatisticsRegisterCounter()
#define IOStatisticsUnregisterCounter()
#define IOStatisticsClientCall()

#endif /* IOKITSTATS */

#if DEVELOPMENT || DEBUG

#define FAKE_STACK_FRAME(a)                                             \
	const void ** __frameptr;                                       \
	const void  * __retaddr;                                        \
	__frameptr = (typeof(__frameptr)) __builtin_frame_address(0);   \
	__retaddr = __frameptr[1];                                      \
	__frameptr[1] = (a);

#define FAKE_STACK_FRAME_END()                                          \
	__frameptr[1] = __retaddr;

#else /* DEVELOPMENT || DEBUG */

#define FAKE_STACK_FRAME(a)
#define FAKE_STACK_FRAME_END()

#endif /* DEVELOPMENT || DEBUG */

#define ASYNC_REF_COUNT         (sizeof(io_async_ref_t) / sizeof(natural_t))
#define ASYNC_REF64_COUNT       (sizeof(io_async_ref64_t) / sizeof(io_user_reference_t))

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" {
#include <mach/mach_traps.h>
#include <vm/vm_map_xnu.h>
} /* extern "C" */

struct IOMachPortHashList;

static_assert(IKOT_MAX_TYPE <= 255);

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// IOMachPort maps OSObjects to ports, avoiding adding an ivar to OSObject.
class IOMachPort : public OSObject
{
	OSDeclareDefaultStructors(IOMachPort);
public:
	mach_port_mscount_t mscount;
	IOLock      lock;
	SLIST_ENTRY(IOMachPort) link;
	ipc_port_t  port;
	OSObject*   XNU_PTRAUTH_SIGNED_PTR("IOMachPort.object") object;

	static IOMachPort* withObjectAndType(OSObject *obj, ipc_kobject_type_t type);

	static IOMachPortHashList* bucketForObject(OSObject *obj,
	    ipc_kobject_type_t type);

	static LIBKERN_RETURNS_NOT_RETAINED IOMachPort* portForObjectInBucket(IOMachPortHashList *bucket, OSObject *obj, ipc_kobject_type_t type);

	static bool noMoreSendersForObject( OSObject * obj,
	    ipc_kobject_type_t type, mach_port_mscount_t * mscount );
	static void releasePortForObject( OSObject * obj,
	    ipc_kobject_type_t type );

	static mach_port_name_t makeSendRightForTask( task_t task,
	    io_object_t obj, ipc_kobject_type_t type );

	virtual void free() APPLE_KEXT_OVERRIDE;
};

#define super OSObject
OSDefineMetaClassAndStructorsWithZone(IOMachPort, OSObject, ZC_ZFREE_CLEARMEM)

static IOLock *         gIOObjectPortLock;
IOLock *                gIOUserServerLock;

SECURITY_READ_ONLY_LATE(const struct io_filter_callbacks *) gIOUCFilterCallbacks;

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

SLIST_HEAD(IOMachPortHashList, IOMachPort);

#if defined(XNU_TARGET_OS_OSX)
#define PORT_HASH_SIZE 4096
#else /* defined(!XNU_TARGET_OS_OSX) */
#define PORT_HASH_SIZE 256
#endif /* !defined(!XNU_TARGET_OS_OSX) */

IOMachPortHashList gIOMachPortHash[PORT_HASH_SIZE];

void
IOMachPortInitialize(void)
{
	for (size_t i = 0; i < PORT_HASH_SIZE; i++) {
		SLIST_INIT(&gIOMachPortHash[i]);
	}
}

IOMachPortHashList*
IOMachPort::bucketForObject(OSObject *obj, ipc_kobject_type_t type )
{
	return &gIOMachPortHash[os_hash_kernel_pointer(obj) % PORT_HASH_SIZE];
}

IOMachPort*
IOMachPort::portForObjectInBucket(IOMachPortHashList *bucket, OSObject *obj, ipc_kobject_type_t type)
{
	IOMachPort *machPort;

	SLIST_FOREACH(machPort, bucket, link) {
		if (machPort->object == obj && iokit_port_type(machPort->port) == type) {
			return machPort;
		}
	}
	return NULL;
}

IOMachPort*
IOMachPort::withObjectAndType(OSObject *obj, ipc_kobject_type_t type)
{
	IOMachPort *machPort = NULL;

	machPort = new IOMachPort;
	if (__improbable(machPort && !machPort->init())) {
		OSSafeReleaseNULL(machPort);
		return NULL;
	}

	machPort->object = obj;
	machPort->port = iokit_alloc_object_port(machPort, type);
	IOLockInlineInit(&machPort->lock);

	obj->taggedRetain(OSTypeID(OSCollection));
	machPort->mscount++;

	return machPort;
}

bool
IOMachPort::noMoreSendersForObject( OSObject * obj,
    ipc_kobject_type_t type, mach_port_mscount_t * mscount )
{
	IOMachPort *machPort = NULL;
	IOUserClient *uc;
	OSAction *action;
	bool destroyed = true;

	IOMachPortHashList *bucket = IOMachPort::bucketForObject(obj, type);

	obj->retain();

	lck_mtx_lock(gIOObjectPortLock);

	machPort = IOMachPort::portForObjectInBucket(bucket, obj, type);

	if (machPort) {
		destroyed = (machPort->mscount <= *mscount);
		if (!destroyed) {
			*mscount = machPort->mscount;
			lck_mtx_unlock(gIOObjectPortLock);
		} else {
			if ((IKOT_IOKIT_CONNECT == type) && (uc = OSDynamicCast(IOUserClient, obj))) {
				uc->noMoreSenders();
			}
			SLIST_REMOVE(bucket, machPort, IOMachPort, link);

			IOLockLock(&machPort->lock);
			iokit_remove_object_port(machPort->port, type);
			machPort->object = NULL;
			IOLockUnlock(&machPort->lock);

			lck_mtx_unlock(gIOObjectPortLock);

			OS_ANALYZER_SUPPRESS("77508635") OSSafeReleaseNULL(machPort);

			obj->taggedRelease(OSTypeID(OSCollection));
		}
	} else {
		lck_mtx_unlock(gIOObjectPortLock);
	}

	if ((IKOT_UEXT_OBJECT == type) && (action = OSDynamicCast(OSAction, obj))) {
		action->Aborted();
	}

	if (IKOT_UEXT_OBJECT == type && IOUserServer::shouldLeakObjects()) {
		// Leak object
		obj->retain();
	}

	obj->release();

	return destroyed;
}

void
IOMachPort::releasePortForObject( OSObject * obj,
    ipc_kobject_type_t type )
{
	IOMachPort *machPort;
	IOService  *service;
	IOMachPortHashList *bucket = IOMachPort::bucketForObject(obj, type);

	assert(IKOT_IOKIT_CONNECT != type);

	lck_mtx_lock(gIOObjectPortLock);

	machPort = IOMachPort::portForObjectInBucket(bucket, obj, type);

	if (machPort
	    && (type == IKOT_IOKIT_OBJECT)
	    && (service = OSDynamicCast(IOService, obj))
	    && !service->machPortHoldDestroy()) {
		obj->retain();
		SLIST_REMOVE(bucket, machPort, IOMachPort, link);

		IOLockLock(&machPort->lock);
		iokit_remove_object_port(machPort->port, type);
		machPort->object = NULL;
		IOLockUnlock(&machPort->lock);

		lck_mtx_unlock(gIOObjectPortLock);

		OS_ANALYZER_SUPPRESS("77508635") OSSafeReleaseNULL(machPort);

		obj->taggedRelease(OSTypeID(OSCollection));
		obj->release();
	} else {
		lck_mtx_unlock(gIOObjectPortLock);
	}
}

void
IOUserClient::destroyUserReferences( OSObject * obj )
{
	IOMachPort *machPort;
	bool        destroyPort;

	IOMachPort::releasePortForObject( obj, IKOT_IOKIT_OBJECT );

	// panther, 3160200
	// IOMachPort::releasePortForObject( obj, IKOT_IOKIT_CONNECT );

	obj->retain();
	IOMachPortHashList *bucket = IOMachPort::bucketForObject(obj, IKOT_IOKIT_CONNECT);
	IOMachPortHashList *mappingBucket = NULL;

	lck_mtx_lock(gIOObjectPortLock);

	IOUserClient * uc = OSDynamicCast(IOUserClient, obj);
	if (uc && uc->mappings) {
		mappingBucket = IOMachPort::bucketForObject(uc->mappings, IKOT_IOKIT_CONNECT);
	}

	machPort = IOMachPort::portForObjectInBucket(bucket, obj, IKOT_IOKIT_CONNECT);

	if (machPort == NULL) {
		lck_mtx_unlock(gIOObjectPortLock);
		goto end;
	}

	SLIST_REMOVE(bucket, machPort, IOMachPort, link);
	obj->taggedRelease(OSTypeID(OSCollection));

	destroyPort = true;
	if (uc) {
		uc->noMoreSenders();
		if (uc->mappings) {
			uc->mappings->taggedRetain(OSTypeID(OSCollection));
			SLIST_INSERT_HEAD(mappingBucket, machPort, link);

			IOLockLock(&machPort->lock);
			machPort->object = uc->mappings;
			IOLockUnlock(&machPort->lock);

			lck_mtx_unlock(gIOObjectPortLock);

			OSSafeReleaseNULL(uc->mappings);
			destroyPort = false;
		}
	}

	if (destroyPort) {
		IOLockLock(&machPort->lock);
		iokit_remove_object_port(machPort->port, IKOT_IOKIT_CONNECT);
		machPort->object = NULL;
		IOLockUnlock(&machPort->lock);

		lck_mtx_unlock(gIOObjectPortLock);
		OS_ANALYZER_SUPPRESS("77508635") OSSafeReleaseNULL(machPort);
	}

end:
	OSSafeReleaseNULL(obj);
}

mach_port_name_t
IOMachPort::makeSendRightForTask( task_t task,
    io_object_t obj, ipc_kobject_type_t type )
{
	return iokit_make_send_right( task, obj, type );
}

void
IOMachPort::free( void )
{
	if (port) {
		iokit_destroy_object_port(port, iokit_port_type(port));
	}
	IOLockInlineDestroy(&lock);
	super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

static bool
IOTaskRegistryCompatibility(task_t task)
{
	return false;
}

static void
IOTaskRegistryCompatibilityMatching(task_t task, OSDictionary * matching)
{
	matching->setObject(gIOServiceNotificationUserKey, kOSBooleanTrue);
	if (!IOTaskRegistryCompatibility(task)) {
		return;
	}
	matching->setObject(gIOCompatibilityMatchKey, kOSBooleanTrue);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors( IOUserIterator, OSIterator )

IOUserIterator *
IOUserIterator::withIterator(OSIterator * iter)
{
	IOUserIterator * me;

	if (!iter) {
		return NULL;
	}

	me = new IOUserIterator;
	if (me && !me->init()) {
		me->release();
		me = NULL;
	}
	if (!me) {
		iter->release();
		return me;
	}
	me->userIteratorObject = iter;

	return me;
}

bool
IOUserIterator::init( void )
{
	if (!OSObject::init()) {
		return false;
	}

	IOLockInlineInit(&lock);
	return true;
}

void
IOUserIterator::free()
{
	if (userIteratorObject) {
		userIteratorObject->release();
	}
	IOLockInlineDestroy(&lock);
	OSObject::free();
}

void
IOUserIterator::reset()
{
	IOLockLock(&lock);
	assert(OSDynamicCast(OSIterator, userIteratorObject));
	((OSIterator *)userIteratorObject)->reset();
	IOLockUnlock(&lock);
}

bool
IOUserIterator::isValid()
{
	bool ret;

	IOLockLock(&lock);
	assert(OSDynamicCast(OSIterator, userIteratorObject));
	ret = ((OSIterator *)userIteratorObject)->isValid();
	IOLockUnlock(&lock);

	return ret;
}

OSObject *
IOUserIterator::getNextObject()
{
	assert(false);
	return NULL;
}

OSObject *
IOUserIterator::copyNextObject()
{
	OSObject * ret = NULL;

	IOLockLock(&lock);
	if (userIteratorObject) {
		ret = ((OSIterator *)userIteratorObject)->getNextObject();
		if (ret) {
			ret->retain();
		}
	}
	IOLockUnlock(&lock);

	return ret;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
extern "C" {
// functions called from osfmk/device/iokit_rpc.c

void
iokit_port_object_description(io_object_t obj, kobject_description_t desc)
{
	IORegistryEntry    * regEntry;
	IOUserNotification * __unused noti;
	_IOServiceNotifier * __unused serviceNoti;
	OSSerialize        * __unused s;
	OSDictionary       * __unused matching = NULL;

	if ((regEntry = OSDynamicCast(IORegistryEntry, obj))) {
		snprintf(desc, KOBJECT_DESCRIPTION_LENGTH, "%s(0x%qx)", obj->getMetaClass()->getClassName(), regEntry->getRegistryEntryID());
#if DEVELOPMENT || DEBUG
	} else if ((noti = OSDynamicCast(IOUserNotification, obj))) {
		// serviceNoti->matching may become NULL if the port gets a no-senders notification, so we have to lock gIOObjectPortLock
		IOLockLock(gIOObjectPortLock);
		serviceNoti = OSDynamicCast(_IOServiceNotifier, noti->userIteratorObject);
		if (serviceNoti && (matching = serviceNoti->matching)) {
			matching->retain();
		}
		IOLockUnlock(gIOObjectPortLock);

		if (matching) {
			s = OSSerialize::withCapacity((unsigned int) page_size);
			if (s && matching->serialize(s)) {
				snprintf(desc, KOBJECT_DESCRIPTION_LENGTH, "%s(%s)", obj->getMetaClass()->getClassName(), s->text());
			}
			OSSafeReleaseNULL(s);
			OSSafeReleaseNULL(matching);
		}
#endif /* DEVELOPMENT || DEBUG */
	} else {
		snprintf(desc, KOBJECT_DESCRIPTION_LENGTH, "%s", obj->getMetaClass()->getClassName());
	}
}

// FIXME: Implementation of these functions are hidden from the static analyzer.
// As for now, the analyzer doesn't consistently support wrapper functions
// for retain and release.
#ifndef __clang_analyzer__
void
iokit_add_reference( io_object_t obj, natural_t type )
{
	if (!obj) {
		return;
	}
	obj->retain();
}

void
iokit_remove_reference( io_object_t obj )
{
	if (obj) {
		obj->release();
	}
}
#endif // __clang_analyzer__

void
iokit_remove_connect_reference(LIBKERN_CONSUMED io_object_t obj )
{
	if (!obj) {
		return;
	}
	obj->release();
}

enum {
	kIPCLockNone  = 0,
	kIPCLockRead  = 1,
	kIPCLockWrite = 2
};

void
IOUserClient::ipcEnter(int locking)
{
	switch (locking) {
	case kIPCLockWrite:
		IORWLockWrite(&lock);
		break;
	case kIPCLockRead:
		IORWLockRead(&lock);
		break;
	case kIPCLockNone:
		break;
	default:
		panic("ipcEnter");
	}

	OSIncrementAtomic(&__ipc);
}

void
IOUserClient::ipcExit(int locking)
{
	bool finalize = false;

	assert(__ipc);
	if (1 == OSDecrementAtomic(&__ipc) && isInactive()) {
		IOLockLock(gIOObjectPortLock);
		if ((finalize = __ipcFinal)) {
			__ipcFinal = false;
		}
		IOLockUnlock(gIOObjectPortLock);
		if (finalize) {
			scheduleFinalize(true);
		}
	}
	switch (locking) {
	case kIPCLockWrite:
	case kIPCLockRead:
		IORWLockUnlock(&lock);
		break;
	case kIPCLockNone:
		break;
	default:
		panic("ipcExit");
	}
}

void
iokit_kobject_retain(io_kobject_t machPort)
{
	assert(OSDynamicCast(IOMachPort, machPort));
	machPort->retain();
}

io_object_t
iokit_copy_object_for_consumed_kobject(LIBKERN_CONSUMED io_kobject_t machPort, natural_t type)
{
	io_object_t  result;

	assert(OSDynamicCast(IOMachPort, machPort));

	IOLockLock(&machPort->lock);
	result = machPort->object;
	if (result) {
		iokit_add_reference(result, type);
	}
	IOLockUnlock(&machPort->lock);
	machPort->release();
	return result;
}

bool
IOUserClient::finalizeUserReferences(OSObject * obj)
{
	IOUserClient * uc;
	bool           ok = true;

	if ((uc = OSDynamicCast(IOUserClient, obj))) {
		IOLockLock(gIOObjectPortLock);
		if ((uc->__ipcFinal = (0 != uc->__ipc))) {
			ok = false;
		}
		IOLockUnlock(gIOObjectPortLock);
	}
	return ok;
}

ipc_port_t
iokit_port_for_object( io_object_t obj, ipc_kobject_type_t type, ipc_kobject_t * kobj )
{
	IOMachPort *machPort = NULL;
	ipc_port_t   port = NULL;

	IOMachPortHashList *bucket = IOMachPort::bucketForObject(obj, type);

	lck_mtx_lock(gIOObjectPortLock);

	machPort = IOMachPort::portForObjectInBucket(bucket, obj, type);

	if (__improbable(machPort == NULL)) {
		machPort = IOMachPort::withObjectAndType(obj, type);
		if (__improbable(machPort == NULL)) {
			goto end;
		}
		SLIST_INSERT_HEAD(bucket, machPort, link);
	} else {
		machPort->mscount++;
	}

	iokit_retain_port(machPort->port);
	port = machPort->port;

end:
	if (kobj) {
		*kobj = machPort;
	}
	lck_mtx_unlock(gIOObjectPortLock);

	return port;
}

kern_return_t
iokit_client_died( io_object_t obj, ipc_port_t /* port */,
    ipc_kobject_type_t type, mach_port_mscount_t * mscount )
{
	IOUserClient *      client;
	IOMemoryMap *       map;
	IOUserNotification * notify;
	IOUserServerCheckInToken * token;

	if (!IOMachPort::noMoreSendersForObject( obj, type, mscount )) {
		return kIOReturnNotReady;
	}

	switch (type) {
	case IKOT_IOKIT_CONNECT:
		if ((client = OSDynamicCast( IOUserClient, obj ))) {
			IOStatisticsClientCall();
			IORWLockWrite(&client->lock);
			client->clientDied();
			IORWLockUnlock(&client->lock);
		}
		break;
	case IKOT_IOKIT_OBJECT:
		if ((map = OSDynamicCast( IOMemoryMap, obj ))) {
			map->taskDied();
		} else if ((notify = OSDynamicCast( IOUserNotification, obj ))) {
			notify->setNotification( NULL );
		}
		break;
	case IKOT_IOKIT_IDENT:
		if ((token = OSDynamicCast( IOUserServerCheckInToken, obj ))) {
			token->cancel();
		}
		break;
	}

	return kIOReturnSuccess;
}
};      /* extern "C" */

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

class IOServiceUserNotification : public IOUserNotification
{
	OSDeclareDefaultStructors(IOServiceUserNotification);

	struct PingMsgKdata {
		mach_msg_header_t               msgHdr;
	};
	struct PingMsgUdata {
		OSNotificationHeader64          notifyHeader;
	};

	enum { kMaxOutstanding = 1024 };

	ipc_port_t          remotePort;
	void                *msgReference;
	mach_msg_size_t     msgReferenceSize;
	natural_t           msgType;
	OSArray     *       newSet;
	bool                armed;
	bool                ipcLogged;

public:

	virtual bool init( mach_port_t port, natural_t type,
	    void * reference, vm_size_t referenceSize,
	    bool clientIs64 );
	virtual void free() APPLE_KEXT_OVERRIDE;
	void invalidatePort(void);

	static bool _handler( void * target,
	    void * ref, IOService * newService, IONotifier * notifier );
	virtual bool handler( void * ref, IOService * newService );

	virtual OSObject * getNextObject() APPLE_KEXT_OVERRIDE;
	virtual OSObject * copyNextObject() APPLE_KEXT_OVERRIDE;
};

class IOServiceMessageUserNotification : public IOUserNotification
{
	OSDeclareDefaultStructors(IOServiceMessageUserNotification);

	struct PingMsgKdata {
		mach_msg_header_t               msgHdr;
		mach_msg_body_t                 msgBody;
		mach_msg_port_descriptor_t      ports[1];
	};
	struct PingMsgUdata {
		OSNotificationHeader64          notifyHeader __attribute__ ((packed));
	};

	ipc_port_t          remotePort;
	void                *msgReference;
	mach_msg_size_t     msgReferenceSize;
	mach_msg_size_t     msgExtraSize;
	natural_t           msgType;
	uint8_t             clientIs64;
	int                 owningPID;
	bool                ipcLogged;

public:

	virtual bool init( mach_port_t port, natural_t type,
	    void * reference, vm_size_t referenceSize,
	    bool clientIs64 );

	virtual void free() APPLE_KEXT_OVERRIDE;
	void invalidatePort(void);

	static IOReturn _handler( void * target, void * ref,
	    UInt32 messageType, IOService * provider,
	    void * messageArgument, vm_size_t argSize );
	virtual IOReturn handler( void * ref,
	    UInt32 messageType, IOService * provider,
	    void * messageArgument, vm_size_t argSize );

	virtual OSObject * getNextObject() APPLE_KEXT_OVERRIDE;
	virtual OSObject * copyNextObject() APPLE_KEXT_OVERRIDE;
};

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserIterator
OSDefineMetaClass( IOUserNotification, IOUserIterator );
OSDefineAbstractStructors( IOUserNotification, IOUserIterator );

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

void
IOUserNotification::free( void )
{
#if DEVELOPMENT || DEBUG
	IOLockLock( gIOObjectPortLock);

	assert(userIteratorObject == NULL);

	IOLockUnlock( gIOObjectPortLock);
#endif /* DEVELOPMENT || DEBUG */

	super::free();
}


void
IOUserNotification::setNotification( IONotifier * notify )
{
	OSObject * previousNotify;

	/*
	 * We must retain this object here before proceeding.
	 * Two threads may race in setNotification(). If one thread sets a new notifier while the
	 * other thread sets the notifier to NULL, it is possible for the second thread to call release()
	 * before the first thread calls retain(). Without the retain here, this thread interleaving
	 * would cause the object to get released and freed before it is retained by the first thread,
	 * which is a UaF.
	 */
	retain();

	IOLockLock( gIOObjectPortLock);

	previousNotify = userIteratorObject;
	userIteratorObject = notify;

	IOLockUnlock( gIOObjectPortLock);

	if (previousNotify) {
		assert(OSDynamicCast(IONotifier, previousNotify));
		((IONotifier *)previousNotify)->remove();

		if (notify == NULL) {
			release();
		}
	} else if (notify) {
		// new IONotifier, retain the object. release() will happen in setNotification(NULL)
		retain();
	}

	release(); // paired with retain() at beginning of this method
}

void
IOUserNotification::reset()
{
	// ?
}

bool
IOUserNotification::isValid()
{
	return true;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOUserNotification
OSDefineMetaClassAndStructors(IOServiceUserNotification, IOUserNotification)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
IOServiceUserNotification::init( mach_port_t port, natural_t type,
    void * reference, vm_size_t referenceSize,
    bool clientIs64 )
{
	if (!super::init()) {
		return false;
	}

	newSet = OSArray::withCapacity( 1 );
	if (!newSet) {
		return false;
	}

	if (referenceSize > sizeof(OSAsyncReference64)) {
		return false;
	}

	msgReferenceSize = mach_round_msg((mach_msg_size_t)referenceSize);
	msgReference = IOMallocZeroData(msgReferenceSize);
	if (!msgReference) {
		return false;
	}

	remotePort = port;
	msgType = type;
	bcopy( reference, msgReference, referenceSize );

	return true;
}

void
IOServiceUserNotification::invalidatePort(void)
{
	remotePort = MACH_PORT_NULL;
}

void
IOServiceUserNotification::free( void )
{
	if (remotePort) {
		iokit_release_port_send(remotePort);
	}
	IOFreeData(msgReference, msgReferenceSize);
	OSSafeReleaseNULL(newSet);

	super::free();
}

bool
IOServiceUserNotification::_handler( void * target,
    void * ref, IOService * newService, IONotifier * notifier )
{
	IOServiceUserNotification * targetObj = (IOServiceUserNotification *)target;
	bool ret;

	targetObj->retain();
	ret = targetObj->handler( ref, newService );
	targetObj->release();
	return ret;
}

bool
IOServiceUserNotification::handler( void * ref,
    IOService * newService )
{
	unsigned int        count;
	kern_return_t       kr;
	ipc_port_t          port = NULL;
	bool                sendPing = false;
	mach_msg_size_t     msgSize, payloadSize;

	IOTakeLock( &lock );

	count = newSet->getCount();
	if (count < kMaxOutstanding) {
		newSet->setObject( newService );
		if ((sendPing = (armed && (0 == count)))) {
			armed = false;
		}
	}

	IOUnlock( &lock );

	if (kIOServiceTerminatedNotificationType == msgType) {
		lck_mtx_lock(gIOObjectPortLock);
		newService->setMachPortHoldDestroy(true);
		lck_mtx_unlock(gIOObjectPortLock);
	}

	if (sendPing) {
		port = iokit_port_for_object( this, IKOT_IOKIT_OBJECT, NULL );

		payloadSize = sizeof(PingMsgUdata) - sizeof(OSAsyncReference64) + msgReferenceSize;
		msgSize = (mach_msg_size_t)(sizeof(PingMsgKdata) + payloadSize);

		kr = kernel_mach_msg_send_with_builder_internal(0, payloadSize,
		    MACH_SEND_KERNEL_IMPORTANCE, MACH_MSG_TIMEOUT_NONE, NULL,
		    ^(mach_msg_header_t *hdr, __assert_only mach_msg_descriptor_t *descs, void *payload){
			PingMsgUdata *udata = (PingMsgUdata *)payload;

			hdr->msgh_remote_port    = remotePort;
			hdr->msgh_local_port     = port;
			hdr->msgh_bits           = MACH_MSGH_BITS(
				MACH_MSG_TYPE_COPY_SEND /*remote*/,
				MACH_MSG_TYPE_MAKE_SEND /*local*/);
			hdr->msgh_size           = msgSize;
			hdr->msgh_id             = kOSNotificationMessageID;

			assert(descs == NULL);
			/* End of kernel processed data */

			udata->notifyHeader.size          = 0;
			udata->notifyHeader.type          = msgType;

			assert((char *)udata->notifyHeader.reference + msgReferenceSize <= (char *)payload + payloadSize);
			bcopy( msgReference, udata->notifyHeader.reference, msgReferenceSize );
		});

		if (port) {
			iokit_release_port( port );
		}

		if ((KERN_SUCCESS != kr) && !ipcLogged) {
			ipcLogged = true;
			IOLog("%s: kernel_mach_msg_send (0x%x)\n", __PRETTY_FUNCTION__, kr );
		}
	}

	return true;
}
OSObject *
IOServiceUserNotification::getNextObject()
{
	assert(false);
	return NULL;
}

OSObject *
IOServiceUserNotification::copyNextObject()
{
	unsigned int        count;
	OSObject *          result;

	IOLockLock(&lock);

	count = newSet->getCount();
	if (count) {
		result = newSet->getObject( count - 1 );
		result->retain();
		newSet->removeObject( count - 1);
	} else {
		result = NULL;
		armed = true;
	}

	IOLockUnlock(&lock);

	return result;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndStructors(IOServiceMessageUserNotification, IOUserNotification)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

bool
IOServiceMessageUserNotification::init( mach_port_t port, natural_t type,
    void * reference, vm_size_t referenceSize, bool client64 )
{
	if (!super::init()) {
		return false;
	}

	if (referenceSize > sizeof(OSAsyncReference64)) {
		return false;
	}

	clientIs64 = client64;

	owningPID = proc_selfpid();

	msgReferenceSize = mach_round_msg((mach_msg_size_t)referenceSize);
	msgReference = IOMallocZeroData(msgReferenceSize);
	if (!msgReference) {
		return false;
	}

	remotePort = port;
	msgType = type;
	bcopy( reference, msgReference, referenceSize );

	return true;
}

void
IOServiceMessageUserNotification::invalidatePort(void)
{
	remotePort = MACH_PORT_NULL;
}

void
IOServiceMessageUserNotification::free( void )
{
	if (remotePort) {
		iokit_release_port_send(remotePort);
	}
	IOFreeData(msgReference, msgReferenceSize);

	super::free();
}

IOReturn
IOServiceMessageUserNotification::_handler( void * target, void * ref,
    UInt32 messageType, IOService * provider,
    void * argument, vm_size_t argSize )
{
	IOServiceMessageUserNotification * targetObj = (IOServiceMessageUserNotification *)target;
	IOReturn ret;

	targetObj->retain();
	ret = targetObj->handler(
		ref, messageType, provider, argument, argSize);
	targetObj->release();
	return ret;
}

IOReturn
IOServiceMessageUserNotification::handler( void * ref,
    UInt32 messageType, IOService * provider,
    void * messageArgument, vm_size_t callerArgSize )
{
	kern_return_t                kr;
	vm_size_t                    argSize;
	mach_msg_size_t              thisMsgSize;
	ipc_port_t                   thisPort, providerPort;

	if (kIOMessageCopyClientID == messageType) {
		*((void **) messageArgument) = OSNumber::withNumber(owningPID, 32);
		return kIOReturnSuccess;
	}

	if (callerArgSize == 0) {
		if (clientIs64) {
			argSize = sizeof(io_user_reference_t);
		} else {
			argSize = sizeof(uint32_t);
		}
	} else {
		if (callerArgSize > kIOUserNotifyMaxMessageSize) {
			callerArgSize = kIOUserNotifyMaxMessageSize;
		}
		argSize = callerArgSize;
	}

	// adjust message size for ipc restrictions
	natural_t type = msgType;
	type &= ~(kIOKitNoticationMsgSizeMask << kIOKitNoticationTypeSizeAdjShift);
	type |= ((argSize & kIOKitNoticationMsgSizeMask) << kIOKitNoticationTypeSizeAdjShift);
	argSize = (argSize + kIOKitNoticationMsgSizeMask) & ~kIOKitNoticationMsgSizeMask;

	mach_msg_size_t extraSize = kIOUserNotifyMaxMessageSize + sizeof(IOServiceInterestContent64);
	mach_msg_size_t msgSize = (mach_msg_size_t) (sizeof(PingMsgKdata) +
	    sizeof(PingMsgUdata) - sizeof(OSAsyncReference64) + msgReferenceSize);

	if (os_add3_overflow(msgSize, offsetof(IOServiceInterestContent64, messageArgument), argSize, &thisMsgSize)) {
		return kIOReturnBadArgument;
	}
	mach_msg_size_t payloadSize = thisMsgSize - sizeof(PingMsgKdata);

	providerPort = iokit_port_for_object( provider, IKOT_IOKIT_OBJECT, NULL );
	thisPort = iokit_port_for_object( this, IKOT_IOKIT_OBJECT, NULL );

	kr = kernel_mach_msg_send_with_builder_internal(1, payloadSize,
	    MACH_SEND_KERNEL_IMPORTANCE, MACH_MSG_TIMEOUT_NONE, NULL,
	    ^(mach_msg_header_t *hdr, mach_msg_descriptor_t *descs, void *payload){
		mach_msg_port_descriptor_t *port_desc = (mach_msg_port_descriptor_t *)descs;
		PingMsgUdata *udata = (PingMsgUdata *)payload;
		IOServiceInterestContent64 * data;
		mach_msg_size_t dataOffset;

		hdr->msgh_remote_port    = remotePort;
		hdr->msgh_local_port     = thisPort;
		hdr->msgh_bits           = MACH_MSGH_BITS_COMPLEX
		|  MACH_MSGH_BITS(
			MACH_MSG_TYPE_COPY_SEND /*remote*/,
			MACH_MSG_TYPE_MAKE_SEND /*local*/);
		hdr->msgh_size           = thisMsgSize;
		hdr->msgh_id             = kOSNotificationMessageID;

		/* body.msgh_descriptor_count is set automatically after the closure */

		port_desc[0].name              = providerPort;
		port_desc[0].disposition       = MACH_MSG_TYPE_MAKE_SEND;
		port_desc[0].type              = MACH_MSG_PORT_DESCRIPTOR;
		/* End of kernel processed data */

		udata->notifyHeader.size          = extraSize;
		udata->notifyHeader.type          = type;
		bcopy( msgReference, udata->notifyHeader.reference, msgReferenceSize );

		/* data is after msgReference */
		dataOffset = sizeof(PingMsgUdata) - sizeof(OSAsyncReference64) + msgReferenceSize;
		data = (IOServiceInterestContent64 *) (((uint8_t *) udata) + dataOffset);
		data->messageType = messageType;

		if (callerArgSize == 0) {
		        assert((char *)data->messageArgument + argSize <= (char *)payload + payloadSize);
		        data->messageArgument[0] = (io_user_reference_t) messageArgument;
		        if (!clientIs64) {
		                data->messageArgument[0] |= (data->messageArgument[0] << 32);
			}
		} else {
		        assert((char *)data->messageArgument + callerArgSize <= (char *)payload + payloadSize);
		        bcopy(messageArgument, data->messageArgument, callerArgSize);
		}
	});

	if (thisPort) {
		iokit_release_port( thisPort );
	}
	if (providerPort) {
		iokit_release_port( providerPort );
	}

	if (kr == MACH_SEND_NO_BUFFER) {
		return kIOReturnNoMemory;
	}

	if ((KERN_SUCCESS != kr) && !ipcLogged) {
		ipcLogged = true;
		IOLog("%s: kernel_mach_msg_send (0x%x)\n", __PRETTY_FUNCTION__, kr );
	}

	return kIOReturnSuccess;
}

OSObject *
IOServiceMessageUserNotification::getNextObject()
{
	return NULL;
}

OSObject *
IOServiceMessageUserNotification::copyNextObject()
{
	return NULL;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#undef super
#define super IOService
OSDefineMetaClassAndAbstractStructors( IOUserClient, IOService )

IOLock       * gIOUserClientOwnersLock;

static_assert(offsetof(IOUserClient, __opaque_end) -
    offsetof(IOUserClient, __opaque_start) == sizeof(void *) * 9,
    "ABI check: Opaque ivars for IOUserClient must be 9 void * big");

void
IOUserClient::initialize( void )
{
	gIOObjectPortLock       = IOLockAlloc();
	gIOUserClientOwnersLock = IOLockAlloc();
	gIOUserServerLock       = IOLockAlloc();
	assert(gIOObjectPortLock && gIOUserClientOwnersLock);

#if IOTRACKING
	IOTrackingQueueCollectUser(IOUserIterator::gMetaClass.getTracking());
	IOTrackingQueueCollectUser(IOServiceMessageUserNotification::gMetaClass.getTracking());
	IOTrackingQueueCollectUser(IOServiceUserNotification::gMetaClass.getTracking());
	IOTrackingQueueCollectUser(IOUserClient::gMetaClass.getTracking());
	IOTrackingQueueCollectUser(IOMachPort::gMetaClass.getTracking());
#endif /* IOTRACKING */
}

void
#if __LP64__
__attribute__((__noreturn__))
#endif
IOUserClient::setAsyncReference(OSAsyncReference asyncRef,
    mach_port_t wakePort,
    void *callback, void *refcon)
{
#if __LP64__
	panic("setAsyncReference not valid for 64b");
#else
	asyncRef[kIOAsyncReservedIndex]      = ((uintptr_t) wakePort)
	    | (kIOUCAsync0Flags & asyncRef[kIOAsyncReservedIndex]);
	asyncRef[kIOAsyncCalloutFuncIndex]   = (uintptr_t) callback;
	asyncRef[kIOAsyncCalloutRefconIndex] = (uintptr_t) refcon;
#endif
}

void
IOUserClient::setAsyncReference64(OSAsyncReference64 asyncRef,
    mach_port_t wakePort,
    mach_vm_address_t callback, io_user_reference_t refcon)
{
	asyncRef[kIOAsyncReservedIndex]      = ((io_user_reference_t) wakePort)
	    | (kIOUCAsync0Flags & asyncRef[kIOAsyncReservedIndex]);
	asyncRef[kIOAsyncCalloutFuncIndex]   = (io_user_reference_t) callback;
	asyncRef[kIOAsyncCalloutRefconIndex] = refcon;
}

void
IOUserClient::setAsyncReference64(OSAsyncReference64 asyncRef,
    mach_port_t wakePort,
    mach_vm_address_t callback, io_user_reference_t refcon, task_t task)
{
	setAsyncReference64(asyncRef, wakePort, callback, refcon);
	if (vm_map_is_64bit(get_task_map(task))) {
		asyncRef[kIOAsyncReservedIndex] |= kIOUCAsync64Flag;
	}
}

static OSDictionary *
CopyConsoleUser(UInt32 uid)
{
	OSArray * array;
	OSDictionary * user = NULL;

	OSObject * ioProperty = IORegistryEntry::getRegistryRoot()->copyProperty(gIOConsoleUsersKey);
	if ((array = OSDynamicCast(OSArray, ioProperty))) {
		for (unsigned int idx = 0;
		    (user = OSDynamicCast(OSDictionary, array->getObject(idx)));
		    idx++) {
			OSNumber * num;

			if ((num = OSDynamicCast(OSNumber, user->getObject(gIOConsoleSessionUIDKey)))
			    && (uid == num->unsigned32BitValue())) {
				user->retain();
				break;
			}
		}
	}
	OSSafeReleaseNULL(ioProperty);
	return user;
}

static OSDictionary *
CopyUserOnConsole(void)
{
	OSArray * array;
	OSDictionary * user = NULL;

	OSObject * ioProperty = IORegistryEntry::getRegistryRoot()->copyProperty(gIOConsoleUsersKey);
	if ((array = OSDynamicCast(OSArray, ioProperty))) {
		for (unsigned int idx = 0;
		    (user = OSDynamicCast(OSDictionary, array->getObject(idx)));
		    idx++) {
			if (kOSBooleanTrue == user->getObject(gIOConsoleSessionOnConsoleKey)) {
				user->retain();
				break;
			}
		}
	}
	OSSafeReleaseNULL(ioProperty);
	return user;
}

IOReturn
IOUserClient::clientHasAuthorization( task_t task,
    IOService * service )
{
	proc_t p;

	p = (proc_t) get_bsdtask_info(task);
	if (p) {
		uint64_t authorizationID;

		authorizationID = proc_uniqueid(p);
		if (authorizationID) {
			if (service->getAuthorizationID() == authorizationID) {
				return kIOReturnSuccess;
			}
		}
	}

	return kIOReturnNotPermitted;
}

IOReturn
IOUserClient::clientHasPrivilege( void * securityToken,
    const char * privilegeName )
{
	kern_return_t           kr;
	security_token_t        token;
	mach_msg_type_number_t  count;
	task_t                  task;
	OSDictionary *          user;
	bool                    secureConsole;


	if (!strncmp(privilegeName, kIOClientPrivilegeForeground,
	    sizeof(kIOClientPrivilegeForeground))) {
		if (task_is_gpu_denied(current_task())) {
			return kIOReturnNotPrivileged;
		} else {
			return kIOReturnSuccess;
		}
	}

	if (!strncmp(privilegeName, kIOClientPrivilegeConsoleSession,
	    sizeof(kIOClientPrivilegeConsoleSession))) {
		kauth_cred_t cred;
		proc_t       p;

		task = (task_t) securityToken;
		if (!task) {
			task = current_task();
		}
		p = (proc_t) get_bsdtask_info(task);
		kr = kIOReturnNotPrivileged;

		if (p && (cred = kauth_cred_proc_ref(p))) {
			user = CopyUserOnConsole();
			if (user) {
				OSNumber * num;
				if ((num = OSDynamicCast(OSNumber, user->getObject(gIOConsoleSessionAuditIDKey)))
				    && (cred->cr_audit.as_aia_p->ai_asid == (au_asid_t) num->unsigned32BitValue())) {
					kr = kIOReturnSuccess;
				}
				user->release();
			}
			kauth_cred_unref(&cred);
		}
		return kr;
	}

	if ((secureConsole = !strncmp(privilegeName, kIOClientPrivilegeSecureConsoleProcess,
	    sizeof(kIOClientPrivilegeSecureConsoleProcess)))) {
		task = (task_t)((IOUCProcessToken *)securityToken)->token;
	} else {
		task = (task_t)securityToken;
	}

	count = TASK_SECURITY_TOKEN_COUNT;
	kr = task_info( task, TASK_SECURITY_TOKEN, (task_info_t) &token, &count );

	if (KERN_SUCCESS != kr) {
	} else if (!strncmp(privilegeName, kIOClientPrivilegeAdministrator,
	    sizeof(kIOClientPrivilegeAdministrator))) {
		if (0 != token.val[0]) {
			kr = kIOReturnNotPrivileged;
		}
	} else if (!strncmp(privilegeName, kIOClientPrivilegeLocalUser,
	    sizeof(kIOClientPrivilegeLocalUser))) {
		user = CopyConsoleUser(token.val[0]);
		if (user) {
			user->release();
		} else {
			kr = kIOReturnNotPrivileged;
		}
	} else if (secureConsole || !strncmp(privilegeName, kIOClientPrivilegeConsoleUser,
	    sizeof(kIOClientPrivilegeConsoleUser))) {
		user = CopyConsoleUser(token.val[0]);
		if (user) {
			if (user->getObject(gIOConsoleSessionOnConsoleKey) != kOSBooleanTrue) {
				kr = kIOReturnNotPrivileged;
			} else if (secureConsole) {
				OSNumber * pid = OSDynamicCast(OSNumber, user->getObject(gIOConsoleSessionSecureInputPIDKey));
				if (pid && pid->unsigned32BitValue() != ((IOUCProcessToken *)securityToken)->pid) {
					kr = kIOReturnNotPrivileged;
				}
			}
			user->release();
		} else {
			kr = kIOReturnNotPrivileged;
		}
	} else {
		kr = kIOReturnUnsupported;
	}

	return kr;
}

OSDictionary *
IOUserClient::copyClientEntitlements(task_t task)
{
	proc_t p = NULL;
	pid_t pid = 0;
	OSDictionary *entitlements = NULL;

	p = (proc_t)get_bsdtask_info(task);
	if (p == NULL) {
		return NULL;
	}
	pid = proc_pid(p);

	if (cs_entitlements_dictionary_copy(p, (void **)&entitlements) == 0) {
		if (entitlements) {
			return entitlements;
		}
	}

	// If the above fails, thats it
	return NULL;
}

OSDictionary *
IOUserClient::copyClientEntitlementsVnode(vnode_t vnode, off_t offset)
{
	OSDictionary *entitlements = NULL;

	if (cs_entitlements_dictionary_copy_vnode(vnode, offset, (void**)&entitlements) != 0) {
		return NULL;
	}
	return entitlements;
}

OSObject *
IOUserClient::copyClientEntitlement( task_t task,
    const char * entitlement )
{
	void *entitlement_object = NULL;

	if (task == NULL) {
		task = current_task();
	}

	/* Validate input arguments */
	if (task == kernel_task || entitlement == NULL) {
		return NULL;
	}
	proc_t proc = (proc_t)get_bsdtask_info(task);

	kern_return_t ret = amfi->OSEntitlements.copyEntitlementAsOSObjectWithProc(
		proc,
		entitlement,
		&entitlement_object);

	if (ret != KERN_SUCCESS) {
		return NULL;
	}
	assert(entitlement_object != NULL);

	return (OSObject*)entitlement_object;
}

OSObject *
IOUserClient::copyClientEntitlementVnode(
	struct vnode *vnode,
	off_t offset,
	const char *entitlement)
{
	OSDictionary *entitlements;
	OSObject *value;

	entitlements = copyClientEntitlementsVnode(vnode, offset);
	if (entitlements == NULL) {
		return NULL;
	}

	/* Fetch the entitlement value from the dictionary. */
	value = entitlements->getObject(entitlement);
	if (value != NULL) {
		value->retain();
	}

	entitlements->release();
	return value;
}

bool
IOUserClient::init()
{
	if (getPropertyTable() || super::init()) {
		return reserve();
	}

	return false;
}

bool
IOUserClient::init(OSDictionary * dictionary)
{
	if (getPropertyTable() || super::init(dictionary)) {
		return reserve();
	}

	return false;
}

bool
IOUserClient::initWithTask(task_t owningTask,
    void * securityID,
    UInt32 type )
{
	if (getPropertyTable() || super::init()) {
		return reserve();
	}

	return false;
}

bool
IOUserClient::initWithTask(task_t owningTask,
    void * securityID,
    UInt32 type,
    OSDictionary * properties )
{
	bool ok;

	ok = super::init( properties );
	ok &= initWithTask( owningTask, securityID, type );

	return ok;
}

bool
IOUserClient::reserve()
{
	if (!reserved) {
		reserved = IOMallocType(ExpansionData);
	}
	setTerminateDefer(NULL, true);
	IOStatisticsRegisterCounter();
	IORWLockInlineInit(&lock);
	IOLockInlineInit(&filterLock);

	return true;
}

struct IOUserClientOwner {
	task_t         task;
	queue_chain_t  taskLink;
	IOUserClient * uc;
	queue_chain_t  ucLink;
};

IOReturn
IOUserClient::registerOwner(task_t task)
{
	IOUserClientOwner * owner;
	IOReturn            ret;
	bool                newOwner;

	IOLockLock(gIOUserClientOwnersLock);

	newOwner = true;
	ret = kIOReturnSuccess;

	if (!owners.next) {
		queue_init(&owners);
	} else {
		queue_iterate(&owners, owner, IOUserClientOwner *, ucLink)
		{
			if (task != owner->task) {
				continue;
			}
			newOwner = false;
			break;
		}
	}
	if (newOwner) {
		owner = IOMallocType(IOUserClientOwner);

		owner->task = task;
		owner->uc   = this;
		queue_enter_first(&owners, owner, IOUserClientOwner *, ucLink);
		queue_enter_first(task_io_user_clients(task), owner, IOUserClientOwner *, taskLink);
		if (messageAppSuspended) {
			task_set_message_app_suspended(task, true);
		}
	}

	IOLockUnlock(gIOUserClientOwnersLock);

	return ret;
}

void
IOUserClient::noMoreSenders(void)
{
	IOUserClientOwner * owner;
	IOUserClientOwner * iter;
	queue_head_t      * taskque;
	bool                hasMessageAppSuspended;

	IOLockLock(gIOUserClientOwnersLock);

	if (owners.next) {
		while (!queue_empty(&owners)) {
			owner = (IOUserClientOwner *)(void *) queue_first(&owners);
			taskque = task_io_user_clients(owner->task);
			queue_remove(taskque, owner, IOUserClientOwner *, taskLink);
			hasMessageAppSuspended = false;
			queue_iterate(taskque, iter, IOUserClientOwner *, taskLink) {
				hasMessageAppSuspended = iter->uc->messageAppSuspended;
				if (hasMessageAppSuspended) {
					break;
				}
			}
			task_set_message_app_suspended(owner->task, hasMessageAppSuspended);
			queue_remove(&owners, owner, IOUserClientOwner *, ucLink);
			IOFreeType(owner, IOUserClientOwner);
		}
		owners.next = owners.prev = NULL;
	}

	IOLockUnlock(gIOUserClientOwnersLock);
}


extern "C" void
iokit_task_app_suspended_changed(task_t task)
{
	queue_head_t      * taskque;
	IOUserClientOwner * owner;
	OSSet             * set;

	IOLockLock(gIOUserClientOwnersLock);

	taskque = task_io_user_clients(task);
	set = NULL;
	queue_iterate(taskque, owner, IOUserClientOwner *, taskLink) {
		if (!owner->uc->messageAppSuspended) {
			continue;
		}
		if (!set) {
			set = OSSet::withCapacity(4);
			if (!set) {
				break;
			}
		}
		set->setObject(owner->uc);
	}

	IOLockUnlock(gIOUserClientOwnersLock);

	if (set) {
		set->iterateObjects(^bool (OSObject * obj) {
			IOUserClient      * uc;

			uc = (typeof(uc))obj;
#if 0
			{
			        OSString          * str;
			        str = IOCopyLogNameForPID(task_pid(task));
			        IOLog("iokit_task_app_suspended_changed(%s) %s %d\n", str ? str->getCStringNoCopy() : "",
			        uc->getName(), task_is_app_suspended(task));
			        OSSafeReleaseNULL(str);
			}
#endif
			uc->message(kIOMessageTaskAppSuspendedChange, NULL);

			return false;
		});
		set->release();
	}
}

static kern_return_t
iokit_task_terminate_phase1(task_t task)
{
	queue_head_t      * taskque;
	IOUserClientOwner * iter;
	OSSet             * userServers = NULL;

	if (!task_is_driver(task)) {
		return KERN_SUCCESS;
	}
	userServers = OSSet::withCapacity(1);

	IOLockLock(gIOUserClientOwnersLock);

	taskque = task_io_user_clients(task);
	queue_iterate(taskque, iter, IOUserClientOwner *, taskLink) {
		userServers->setObject(iter->uc);
	}
	IOLockUnlock(gIOUserClientOwnersLock);

	if (userServers) {
		IOUserServer * userServer;
		while ((userServer = OSRequiredCast(IOUserServer, userServers->getAnyObject()))) {
			userServer->clientDied();
			userServers->removeObject(userServer);
		}
		userServers->release();
	}
	return KERN_SUCCESS;
}

static kern_return_t
iokit_task_terminate_phase2(task_t task)
{
	queue_head_t      * taskque;
	IOUserClientOwner * owner;
	IOUserClient      * dead;
	IOUserClient      * uc;

	IOLockLock(gIOUserClientOwnersLock);
	taskque = task_io_user_clients(task);
	dead = NULL;
	while (!queue_empty(taskque)) {
		owner = (IOUserClientOwner *)(void *) queue_first(taskque);
		uc = owner->uc;
		queue_remove(taskque, owner, IOUserClientOwner *, taskLink);
		queue_remove(&uc->owners, owner, IOUserClientOwner *, ucLink);
		if (queue_empty(&uc->owners)) {
			uc->retain();
			IOLog("destroying out of band connect for %s\n", uc->getName());
			// now using the uc queue head as a singly linked queue,
			// leaving .next as NULL to mark it empty
			uc->owners.next = NULL;
			uc->owners.prev = (queue_entry_t) dead;
			dead = uc;
		}
		IOFreeType(owner, IOUserClientOwner);
	}
	IOLockUnlock(gIOUserClientOwnersLock);

	while (dead) {
		uc = dead;
		dead = (IOUserClient *)(void *) dead->owners.prev;
		uc->owners.prev = NULL;
		if (uc->sharedInstance || !uc->closed) {
			uc->clientDied();
		}
		uc->release();
	}

	return KERN_SUCCESS;
}

extern "C" kern_return_t
iokit_task_terminate(task_t task, int phase)
{
	switch (phase) {
	case 1:
		return iokit_task_terminate_phase1(task);
	case 2:
		return iokit_task_terminate_phase2(task);
	default:
		panic("iokit_task_terminate phase %d", phase);
	}
}

struct IOUCFilterPolicy {
	task_t             task;
	io_filter_policy_t filterPolicy;
	IOUCFilterPolicy * next;
};

io_filter_policy_t
IOUserClient::filterForTask(task_t task, io_filter_policy_t addFilterPolicy)
{
	IOUCFilterPolicy * elem;
	io_filter_policy_t filterPolicy;

	filterPolicy = 0;
	IOLockLock(&filterLock);

	for (elem = reserved->filterPolicies; elem && (elem->task != task); elem = elem->next) {
	}

	if (elem) {
		if (addFilterPolicy) {
			assert(addFilterPolicy == elem->filterPolicy);
		}
		filterPolicy = elem->filterPolicy;
	} else if (addFilterPolicy) {
		elem = IOMallocType(IOUCFilterPolicy);
		elem->task               = task;
		elem->filterPolicy       = addFilterPolicy;
		elem->next               = reserved->filterPolicies;
		reserved->filterPolicies = elem;
		filterPolicy = addFilterPolicy;
	}

	IOLockUnlock(&filterLock);
	return filterPolicy;
}

void
IOUserClient::free()
{
	if (mappings) {
		mappings->release();
	}

	IOStatisticsUnregisterCounter();

	assert(!owners.next);
	assert(!owners.prev);

	if (reserved) {
		IOUCFilterPolicy * elem;
		IOUCFilterPolicy * nextElem;
		for (elem = reserved->filterPolicies; elem; elem = nextElem) {
			nextElem = elem->next;
			if (elem->filterPolicy && gIOUCFilterCallbacks->io_filter_release) {
				gIOUCFilterCallbacks->io_filter_release(elem->filterPolicy);
			}
			IOFreeType(elem, IOUCFilterPolicy);
		}
		IOFreeType(reserved, ExpansionData);
		IORWLockInlineDestroy(&lock);
		IOLockInlineDestroy(&filterLock);
	}

	super::free();
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

OSDefineMetaClassAndAbstractStructors( IOUserClient2022, IOUserClient )


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

IOReturn
IOUserClient::clientDied( void )
{
	IOReturn ret = kIOReturnNotReady;

	if (sharedInstance || OSCompareAndSwap8(0, 1, &closed)) {
		ret = clientClose();
	}

	return ret;
}

IOReturn
IOUserClient::clientClose( void )
{
	return kIOReturnUnsupported;
}

IOService *
IOUserClient::getService( void )
{
	return NULL;
}

IOReturn
IOUserClient::registerNotificationPort(
	mach_port_t     /* port */,
	UInt32          /* type */,
	UInt32          /* refCon */)
{
	return kIOReturnUnsupported;
}

IOReturn
IOUserClient::registerNotificationPort(
	mach_port_t port,
	UInt32          type,
	io_user_reference_t refCon)
{
	return registerNotificationPort(port, type, (UInt32) refCon);
}

IOReturn
IOUserClient::getNotificationSemaphore( UInt32 notification_type,
    semaphore_t * semaphore )
{
	return kIOReturnUnsupported;
}

IOReturn
IOUserClient::connectClient( IOUserClient * /* client */ )
{
	return kIOReturnUnsupported;
}

IOReturn
IOUserClient::clientMemoryForType( UInt32 type,
    IOOptionBits * options,
    IOMemoryDescriptor ** memory )
{
	return kIOReturnUnsupported;
}

IOReturn
IOUserClient::clientMemoryForType( UInt32 type,
    IOOptionBits * options,
    OSSharedPtr<IOMemoryDescriptor>& memory )
{
	IOMemoryDescriptor* memoryRaw = nullptr;
	IOReturn result = clientMemoryForType(type, options, &memoryRaw);
	memory.reset(memoryRaw, OSNoRetain);
	return result;
}

#if !__LP64__
IOMemoryMap *
IOUserClient::mapClientMemory(
	IOOptionBits            type,
	task_t                  task,
	IOOptionBits            mapFlags,
	IOVirtualAddress        atAddress )
{
	return NULL;
}
#endif

IOMemoryMap *
IOUserClient::mapClientMemory64(
	IOOptionBits            type,
	task_t                  task,
	IOOptionBits            mapFlags,
	mach_vm_address_t       atAddress )
{
	IOReturn            err;
	IOOptionBits        options = 0;
	IOMemoryDescriptor * memory = NULL;
	IOMemoryMap *       map = NULL;

	err = clientMemoryForType((UInt32) type, &options, &memory );

	if (memory && (kIOReturnSuccess == err)) {
		FAKE_STACK_FRAME(getMetaClass());

		options = (options & ~kIOMapUserOptionsMask)
		    | (mapFlags & kIOMapUserOptionsMask);
		map = memory->createMappingInTask( task, atAddress, options );
		memory->release();

		FAKE_STACK_FRAME_END();
	}

	return map;
}

IOReturn
IOUserClient::exportObjectToClient(task_t task,
    OSObject *obj, io_object_t *clientObj)
{
	mach_port_name_t    name;

	name = IOMachPort::makeSendRightForTask( task, obj, IKOT_IOKIT_OBJECT );

	*clientObj = (io_object_t)(uintptr_t) name;

	if (obj) {
		obj->release();
	}

	return kIOReturnSuccess;
}

IOReturn
IOUserClient::copyPortNameForObjectInTask(task_t task,
    OSObject *obj, mach_port_name_t * port_name)
{
	mach_port_name_t    name;

	name = IOMachPort::makeSendRightForTask( task, obj, IKOT_IOKIT_IDENT );

	*(mach_port_name_t *) port_name = name;

	return kIOReturnSuccess;
}

IOReturn
IOUserClient::copyObjectForPortNameInTask(task_t task, mach_port_name_t port_name,
    OSObject **obj)
{
	OSObject * object;

	object = iokit_lookup_object_with_port_name(port_name, IKOT_IOKIT_IDENT, task);

	*obj = object;

	return object ? kIOReturnSuccess : kIOReturnIPCError;
}

IOReturn
IOUserClient::copyObjectForPortNameInTask(task_t task, mach_port_name_t port_name,
    OSSharedPtr<OSObject>& obj)
{
	OSObject* objRaw = NULL;
	IOReturn result = copyObjectForPortNameInTask(task, port_name, &objRaw);
	obj.reset(objRaw, OSNoRetain);
	return result;
}

IOReturn
IOUserClient::adjustPortNameReferencesInTask(task_t task, mach_port_name_t port_name, mach_port_delta_t delta)
{
	return iokit_mod_send_right(task, port_name, delta);
}

IOExternalMethod *
IOUserClient::getExternalMethodForIndex( UInt32 /* index */)
{
	return NULL;
}

IOExternalAsyncMethod *
IOUserClient::getExternalAsyncMethodForIndex( UInt32 /* index */)
{
	return NULL;
}

IOExternalTrap *
IOUserClient::
getExternalTrapForIndex(UInt32 index)
{
	return NULL;
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Suppressing the deprecated-declarations warning. Avoiding the use of deprecated
// functions can break clients of kexts implementing getExternalMethodForIndex()
IOExternalMethod *
IOUserClient::
getTargetAndMethodForIndex(IOService **targetP, UInt32 index)
{
	IOExternalMethod *method = getExternalMethodForIndex(index);

	if (method) {
		*targetP = (IOService *) method->object;
	}

	return method;
}

IOExternalMethod *
IOUserClient::
getTargetAndMethodForIndex(OSSharedPtr<IOService>& targetP, UInt32 index)
{
	IOService* targetPRaw = NULL;
	IOExternalMethod* result = getTargetAndMethodForIndex(&targetPRaw, index);
	targetP.reset(targetPRaw, OSRetain);
	return result;
}

IOExternalAsyncMethod *
IOUserClient::
getAsyncTargetAndMethodForIndex(IOService ** targetP, UInt32 index)
{
	IOExternalAsyncMethod *method = getExternalAsyncMethodForIndex(index);

	if (method) {
		*targetP = (IOService *) method->object;
	}

	return method;
}

IOExternalAsyncMethod *
IOUserClient::
getAsyncTargetAndMethodForIndex(OSSharedPtr<IOService>& targetP, UInt32 index)
{
	IOService* targetPRaw = NULL;
	IOExternalAsyncMethod* result = getAsyncTargetAndMethodForIndex(&targetPRaw, index);
	targetP.reset(targetPRaw, OSRetain);
	return result;
}

IOExternalTrap *
IOUserClient::
getTargetAndTrapForIndex(IOService ** targetP, UInt32 index)
{
	IOExternalTrap *trap = getExternalTrapForIndex(index);

	if (trap) {
		*targetP = trap->object;
	}

	return trap;
}
#pragma clang diagnostic pop

IOReturn
IOUserClient::releaseAsyncReference64(OSAsyncReference64 reference)
{
	mach_port_t port;
	port = (mach_port_t) (reference[0] & ~kIOUCAsync0Flags);

	if (MACH_PORT_NULL != port) {
		iokit_release_port_send(port);
	}

	return kIOReturnSuccess;
}

IOReturn
IOUserClient::releaseNotificationPort(mach_port_t port)
{
	if (MACH_PORT_NULL != port) {
		iokit_release_port_send(port);
	}

	return kIOReturnSuccess;
}

IOReturn
IOUserClient::sendAsyncResult(OSAsyncReference reference,
    IOReturn result, void *args[], UInt32 numArgs)
{
	OSAsyncReference64  reference64;
	OSBoundedArray<io_user_reference_t, kMaxAsyncArgs> args64;
	unsigned int        idx;

	if (numArgs > kMaxAsyncArgs) {
		return kIOReturnMessageTooLarge;
	}

	for (idx = 0; idx < kOSAsyncRef64Count; idx++) {
		reference64[idx] = REF64(reference[idx]);
	}

	for (idx = 0; idx < numArgs; idx++) {
		args64[idx] = REF64(args[idx]);
	}

	return sendAsyncResult64(reference64, result, args64.data(), numArgs);
}

IOReturn
IOUserClient::sendAsyncResult64WithOptions(OSAsyncReference64 reference,
    IOReturn result, io_user_reference_t args[], UInt32 numArgs, IOOptionBits options)
{
	return _sendAsyncResult64(reference, result, args, numArgs, options);
}

IOReturn
IOUserClient::sendAsyncResult64(OSAsyncReference64 reference,
    IOReturn result, io_user_reference_t args[], UInt32 numArgs)
{
	return _sendAsyncResult64(reference, result, args, numArgs, 0);
}

IOReturn
IOUserClient::_sendAsyncResult64(OSAsyncReference64 reference,
    IOReturn result, io_user_reference_t args[], UInt32 numArgs, IOOptionBits options)
{
	struct ReplyMsg {
		mach_msg_header_t msgHdr;
		union{
			struct{
				OSNotificationHeader     notifyHdr;
				IOAsyncCompletionContent asyncContent;
				uint32_t                 args[kMaxAsyncArgs];
			} msg32;
			struct{
				OSNotificationHeader64   notifyHdr;
				IOAsyncCompletionContent asyncContent;
				io_user_reference_t      args[kMaxAsyncArgs] __attribute__ ((packed));
			} msg64;
		} m;
	};
	ReplyMsg      replyMsg;
	mach_port_t   replyPort;
	kern_return_t kr;

	// If no reply port, do nothing.
	replyPort = (mach_port_t) (reference[0] & ~kIOUCAsync0Flags);
	if (replyPort == MACH_PORT_NULL) {
		return kIOReturnSuccess;
	}

	if (numArgs > kMaxAsyncArgs) {
		return kIOReturnMessageTooLarge;
	}

	bzero(&replyMsg, sizeof(replyMsg));
	replyMsg.msgHdr.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND /*remote*/,
	    0 /*local*/);
	replyMsg.msgHdr.msgh_remote_port = replyPort;
	replyMsg.msgHdr.msgh_local_port  = NULL;
	replyMsg.msgHdr.msgh_id          = kOSNotificationMessageID;
	if (kIOUCAsync64Flag & reference[0]) {
		replyMsg.msgHdr.msgh_size =
		    sizeof(replyMsg.msgHdr) + sizeof(replyMsg.m.msg64)
		    - (kMaxAsyncArgs - numArgs) * sizeof(io_user_reference_t);
		replyMsg.m.msg64.notifyHdr.size = sizeof(IOAsyncCompletionContent)
		    + numArgs * sizeof(io_user_reference_t);
		replyMsg.m.msg64.notifyHdr.type = kIOAsyncCompletionNotificationType;
		/* Copy reference except for reference[0], which is left as 0 from the earlier bzero */
		bcopy(&reference[1], &replyMsg.m.msg64.notifyHdr.reference[1], sizeof(OSAsyncReference64) - sizeof(reference[0]));

		replyMsg.m.msg64.asyncContent.result = result;
		if (numArgs) {
			bcopy(args, replyMsg.m.msg64.args, numArgs * sizeof(io_user_reference_t));
		}
	} else {
		unsigned int idx;

		replyMsg.msgHdr.msgh_size =
		    sizeof(replyMsg.msgHdr) + sizeof(replyMsg.m.msg32)
		    - (kMaxAsyncArgs - numArgs) * sizeof(uint32_t);

		replyMsg.m.msg32.notifyHdr.size = sizeof(IOAsyncCompletionContent)
		    + numArgs * sizeof(uint32_t);
		replyMsg.m.msg32.notifyHdr.type = kIOAsyncCompletionNotificationType;

		/* Skip reference[0] which is left as 0 from the earlier bzero */
		for (idx = 1; idx < kOSAsyncRefCount; idx++) {
			replyMsg.m.msg32.notifyHdr.reference[idx] = REF32(reference[idx]);
		}

		replyMsg.m.msg32.asyncContent.result = result;

		for (idx = 0; idx < numArgs; idx++) {
			replyMsg.m.msg32.args[idx] = REF32(args[idx]);
		}
	}

	if ((options & kIOUserNotifyOptionCanDrop) != 0) {
		kr = mach_msg_send_from_kernel_with_options( &replyMsg.msgHdr,
		    replyMsg.msgHdr.msgh_size, MACH64_SEND_TIMEOUT, MACH_MSG_TIMEOUT_NONE);
	} else {
		/* Fail on full queue. */
		kr = mach_msg_send_from_kernel(&replyMsg.msgHdr,
		    replyMsg.msgHdr.msgh_size);
	}
	if ((KERN_SUCCESS != kr) && (MACH_SEND_TIMED_OUT != kr) && !(kIOUCAsyncErrorLoggedFlag & reference[0])) {
		reference[0] |= kIOUCAsyncErrorLoggedFlag;
		IOLog("%s: mach_msg_send_from_kernel(0x%x)\n", __PRETTY_FUNCTION__, kr );
	}
	return kr;
}


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

extern "C" {
#define CHECK(cls, obj, out)                      \
	cls * out;                              \
	if( !(out = OSDynamicCast( cls, obj)))  \
	    return( kIOReturnBadArgument )

#define CHECKLOCKED(cls, obj, out)                                        \
	IOUserIterator * oIter;                                         \
	cls * out;                                                      \
	if( !(oIter = OSDynamicCast(IOUserIterator, obj)))              \
	    return (kIOReturnBadArgument);                              \
	if( !(out = OSDynamicCast(cls, oIter->userIteratorObject)))     \
	    return (kIOReturnBadArgument)

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

// Create a vm_map_copy_t or kalloc'ed data for memory
// to be copied out. ipc will free after the copyout.

static kern_return_t
copyoutkdata( const void * data, vm_size_t len,
    io_buf_ptr_t * buf )
{
	kern_return_t       err;
	vm_map_copy_t       copy;

	err = vm_map_copyin( kernel_map, CAST_USER_ADDR_T(data), len,
	    false /* src_destroy */, &copy);

	assert( err == KERN_SUCCESS );
	if (err == KERN_SUCCESS) {
		*buf = (char *) copy;
	}

	return err;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Routine io_server_version */
kern_return_t
is_io_server_version(
	mach_port_t main_port,
	uint64_t *version)
{
	*version = IOKIT_SERVER_VERSION;
	return kIOReturnSuccess;
}

/* Routine io_object_get_class */
kern_return_t
is_io_object_get_class(
	io_object_t object,
	io_name_t className )
{
	const OSMetaClass* my_obj = NULL;

	if (!object) {
		return kIOReturnBadArgument;
	}

	my_obj = object->getMetaClass();
	if (!my_obj) {
		return kIOReturnNotFound;
	}

	strlcpy( className, my_obj->getClassName(), sizeof(io_name_t));

	return kIOReturnSuccess;
}

/* Routine io_object_get_superclass */
kern_return_t
is_io_object_get_superclass(
	mach_port_t main_port,
	io_name_t obj_name,
	io_name_t class_name)
{
	IOReturn            ret;
	const OSMetaClass * meta;
	const OSMetaClass * super;
	const OSSymbol    * name;
	const char        * cstr;

	if (!obj_name || !class_name) {
		return kIOReturnBadArgument;
	}
	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	ret = kIOReturnNotFound;
	meta = NULL;
	do{
		name = OSSymbol::withCString(obj_name);
		if (!name) {
			break;
		}
		meta = OSMetaClass::copyMetaClassWithName(name);
		if (!meta) {
			break;
		}
		super = meta->getSuperClass();
		if (!super) {
			break;
		}
		cstr = super->getClassName();
		if (!cstr) {
			break;
		}
		strlcpy(class_name, cstr, sizeof(io_name_t));
		ret = kIOReturnSuccess;
	}while (false);

	OSSafeReleaseNULL(name);
	if (meta) {
		meta->releaseMetaClass();
	}

	return ret;
}

/* Routine io_object_get_bundle_identifier */
kern_return_t
is_io_object_get_bundle_identifier(
	mach_port_t main_port,
	io_name_t obj_name,
	io_name_t bundle_name)
{
	IOReturn            ret;
	const OSMetaClass * meta;
	const OSSymbol    * name;
	const OSSymbol    * identifier;
	const char        * cstr;

	if (!obj_name || !bundle_name) {
		return kIOReturnBadArgument;
	}
	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	ret = kIOReturnNotFound;
	meta = NULL;
	do{
		name = OSSymbol::withCString(obj_name);
		if (!name) {
			break;
		}
		meta = OSMetaClass::copyMetaClassWithName(name);
		if (!meta) {
			break;
		}
		identifier = meta->getKmodName();
		if (!identifier) {
			break;
		}
		cstr = identifier->getCStringNoCopy();
		if (!cstr) {
			break;
		}
		strlcpy(bundle_name, identifier->getCStringNoCopy(), sizeof(io_name_t));
		ret = kIOReturnSuccess;
	}while (false);

	OSSafeReleaseNULL(name);
	if (meta) {
		meta->releaseMetaClass();
	}

	return ret;
}

/* Routine io_object_conforms_to */
kern_return_t
is_io_object_conforms_to(
	io_object_t object,
	io_name_t className,
	boolean_t *conforms )
{
	if (!object) {
		return kIOReturnBadArgument;
	}

	*conforms = (NULL != object->metaCast( className ));

	return kIOReturnSuccess;
}

/* Routine io_object_get_retain_count */
kern_return_t
is_io_object_get_retain_count(
	io_object_t object,
	uint32_t *retainCount )
{
	if (!object) {
		return kIOReturnBadArgument;
	}

	*retainCount = object->getRetainCount();
	return kIOReturnSuccess;
}

/* Routine io_iterator_next */
kern_return_t
is_io_iterator_next(
	io_object_t iterator,
	io_object_t *object )
{
	IOReturn    ret;
	OSObject *  obj;
	OSIterator * iter;
	IOUserIterator * uiter;

	if ((uiter = OSDynamicCast(IOUserIterator, iterator))) {
		obj = uiter->copyNextObject();
	} else if ((iter = OSDynamicCast(OSIterator, iterator))) {
		obj = iter->getNextObject();
		if (obj) {
			obj->retain();
		}
	} else {
		return kIOReturnBadArgument;
	}

	if (obj) {
		*object = obj;
		ret = kIOReturnSuccess;
	} else {
		ret = kIOReturnNoDevice;
	}

	return ret;
}

/* Routine io_iterator_reset */
kern_return_t
is_io_iterator_reset(
	io_object_t iterator )
{
	CHECK( OSIterator, iterator, iter );

	iter->reset();

	return kIOReturnSuccess;
}

/* Routine io_iterator_is_valid */
kern_return_t
is_io_iterator_is_valid(
	io_object_t iterator,
	boolean_t *is_valid )
{
	CHECK( OSIterator, iterator, iter );

	*is_valid = iter->isValid();

	return kIOReturnSuccess;
}

static kern_return_t
internal_io_service_match_property_table(
	io_service_t _service,
	const char * matching,
	mach_msg_type_number_t matching_size,
	boolean_t *matches)
{
	CHECK( IOService, _service, service );

	kern_return_t       kr;
	OSObject *          obj;
	OSDictionary *      dict;

	assert(matching_size);


	obj = OSUnserializeXML(matching, matching_size);

	if ((dict = OSDynamicCast( OSDictionary, obj))) {
		IOTaskRegistryCompatibilityMatching(current_task(), dict);
		*matches = service->passiveMatch( dict );
		kr = kIOReturnSuccess;
	} else {
		kr = kIOReturnBadArgument;
	}

	if (obj) {
		obj->release();
	}

	return kr;
}

/* Routine io_service_match_property_table */
kern_return_t
is_io_service_match_property_table(
	io_service_t service,
	io_string_t matching,
	boolean_t *matches )
{
	return kIOReturnUnsupported;
}


/* Routine io_service_match_property_table_ool */
kern_return_t
is_io_service_match_property_table_ool(
	io_object_t service,
	io_buf_ptr_t matching,
	mach_msg_type_number_t matchingCnt,
	kern_return_t *result,
	boolean_t *matches )
{
	kern_return_t         kr;
	vm_offset_t           data;
	vm_map_offset_t       map_data;

	kr = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t) matching );
	data = CAST_DOWN(vm_offset_t, map_data);

	if (KERN_SUCCESS == kr) {
		// must return success after vm_map_copyout() succeeds
		*result = internal_io_service_match_property_table(service,
		    (const char *)data, matchingCnt, matches );
		vm_deallocate( kernel_map, data, matchingCnt );
	}

	return kr;
}

/* Routine io_service_match_property_table_bin */
kern_return_t
is_io_service_match_property_table_bin(
	io_object_t service,
	io_struct_inband_t matching,
	mach_msg_type_number_t matchingCnt,
	boolean_t *matches)
{
	return internal_io_service_match_property_table(service, matching, matchingCnt, matches);
}

static kern_return_t
internal_io_service_get_matching_services(
	mach_port_t main_port,
	const char * matching,
	mach_msg_type_number_t matching_size,
	io_iterator_t *existing )
{
	kern_return_t       kr;
	OSObject *          obj;
	OSDictionary *      dict;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	assert(matching_size);
	obj = OSUnserializeXML(matching, matching_size);

	if ((dict = OSDynamicCast( OSDictionary, obj))) {
		IOTaskRegistryCompatibilityMatching(current_task(), dict);
		*existing = IOUserIterator::withIterator(IOService::getMatchingServices( dict ));
		kr = kIOReturnSuccess;
	} else {
		kr = kIOReturnBadArgument;
	}

	if (obj) {
		obj->release();
	}

	return kr;
}

/* Routine io_service_get_matching_services */
kern_return_t
is_io_service_get_matching_services(
	mach_port_t main_port,
	io_string_t matching,
	io_iterator_t *existing )
{
	return kIOReturnUnsupported;
}

/* Routine io_service_get_matching_services_ool */
kern_return_t
is_io_service_get_matching_services_ool(
	mach_port_t main_port,
	io_buf_ptr_t matching,
	mach_msg_type_number_t matchingCnt,
	kern_return_t *result,
	io_object_t *existing )
{
	kern_return_t       kr;
	vm_offset_t         data;
	vm_map_offset_t     map_data;

	kr = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t) matching );
	data = CAST_DOWN(vm_offset_t, map_data);

	if (KERN_SUCCESS == kr) {
		// must return success after vm_map_copyout() succeeds
		// and mig will copy out objects on success
		*existing = NULL;
		*result = internal_io_service_get_matching_services(main_port,
		    (const char *) data, matchingCnt, existing);
		vm_deallocate( kernel_map, data, matchingCnt );
	}

	return kr;
}

/* Routine io_service_get_matching_services_bin */
kern_return_t
is_io_service_get_matching_services_bin(
	mach_port_t main_port,
	io_struct_inband_t matching,
	mach_msg_type_number_t matchingCnt,
	io_object_t *existing)
{
	return internal_io_service_get_matching_services(main_port, matching, matchingCnt, existing);
}


static kern_return_t
internal_io_service_get_matching_service(
	mach_port_t main_port,
	const char * matching,
	mach_msg_type_number_t matching_size,
	io_service_t *service )
{
	kern_return_t       kr;
	OSObject *          obj;
	OSDictionary *      dict;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	assert(matching_size);
	obj = OSUnserializeXML(matching, matching_size);

	if ((dict = OSDynamicCast( OSDictionary, obj))) {
		IOTaskRegistryCompatibilityMatching(current_task(), dict);
		*service = IOService::copyMatchingService( dict );
		kr = *service ? kIOReturnSuccess : kIOReturnNotFound;
	} else {
		kr = kIOReturnBadArgument;
	}

	if (obj) {
		obj->release();
	}

	return kr;
}

/* Routine io_service_get_matching_service */
kern_return_t
is_io_service_get_matching_service(
	mach_port_t main_port,
	io_string_t matching,
	io_service_t *service )
{
	return kIOReturnUnsupported;
}

/* Routine io_service_get_matching_services_ool */
kern_return_t
is_io_service_get_matching_service_ool(
	mach_port_t main_port,
	io_buf_ptr_t matching,
	mach_msg_type_number_t matchingCnt,
	kern_return_t *result,
	io_object_t *service )
{
	kern_return_t       kr;
	vm_offset_t         data;
	vm_map_offset_t     map_data;

	kr = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t) matching );
	data = CAST_DOWN(vm_offset_t, map_data);

	if (KERN_SUCCESS == kr) {
		// must return success after vm_map_copyout() succeeds
		// and mig will copy out objects on success
		*service = NULL;
		*result = internal_io_service_get_matching_service(main_port,
		    (const char *) data, matchingCnt, service );
		vm_deallocate( kernel_map, data, matchingCnt );
	}

	return kr;
}

/* Routine io_service_get_matching_service_bin */
kern_return_t
is_io_service_get_matching_service_bin(
	mach_port_t main_port,
	io_struct_inband_t matching,
	mach_msg_type_number_t matchingCnt,
	io_object_t *service)
{
	return internal_io_service_get_matching_service(main_port, matching, matchingCnt, service);
}

static kern_return_t
internal_io_service_add_notification(
	mach_port_t main_port,
	io_name_t notification_type,
	const char * matching,
	size_t matching_size,
	mach_port_t port,
	void * reference,
	vm_size_t referenceSize,
	bool client64,
	io_object_t * notification )
{
	IOServiceUserNotification * userNotify = NULL;
	IONotifier *                notify = NULL;
	const OSSymbol *            sym;
	OSObject *                  obj;
	OSDictionary *              dict;
	IOReturn                    err;
	natural_t                   userMsgType;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	do {
		err = kIOReturnNoResources;

		if (matching_size > (sizeof(io_struct_inband_t) * 1024)) {
			return kIOReturnMessageTooLarge;
		}

		if (!(sym = OSSymbol::withCString( notification_type ))) {
			err = kIOReturnNoResources;
		}

		assert(matching_size);
		obj = OSUnserializeXML(matching, matching_size);
		dict = OSDynamicCast(OSDictionary, obj);
		if (!dict) {
			err = kIOReturnBadArgument;
			continue;
		}
		IOTaskRegistryCompatibilityMatching(current_task(), dict);

		if ((sym == gIOPublishNotification)
		    || (sym == gIOFirstPublishNotification)) {
			userMsgType = kIOServicePublishNotificationType;
		} else if ((sym == gIOMatchedNotification)
		    || (sym == gIOFirstMatchNotification)) {
			userMsgType = kIOServiceMatchedNotificationType;
		} else if ((sym == gIOTerminatedNotification)
		    || (sym == gIOWillTerminateNotification)) {
			userMsgType = kIOServiceTerminatedNotificationType;
		} else {
			userMsgType = kLastIOKitNotificationType;
		}

		userNotify = new IOServiceUserNotification;

		if (userNotify && !userNotify->init( port, userMsgType,
		    reference, referenceSize, client64)) {
			userNotify->release();
			userNotify = NULL;
		}
		if (!userNotify) {
			continue;
		}

		notify = IOService::addMatchingNotification( sym, dict,
		    &userNotify->_handler, userNotify );
		if (notify) {
			*notification = userNotify;
			userNotify->setNotification( notify );
			err = kIOReturnSuccess;
		} else {
			err = kIOReturnUnsupported;
		}
	} while (false);

	if ((kIOReturnSuccess != err) && userNotify) {
		userNotify->setNotification(NULL);
		userNotify->invalidatePort();
		userNotify->release();
		userNotify = NULL;
	}

	if (sym) {
		sym->release();
	}
	if (obj) {
		obj->release();
	}

	return err;
}


/* Routine io_service_add_notification */
kern_return_t
is_io_service_add_notification(
	mach_port_t main_port,
	io_name_t notification_type,
	io_string_t matching,
	mach_port_t port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	io_object_t * notification )
{
	return kIOReturnUnsupported;
}

/* Routine io_service_add_notification_64 */
kern_return_t
is_io_service_add_notification_64(
	mach_port_t main_port,
	io_name_t notification_type,
	io_string_t matching,
	mach_port_t wake_port,
	io_async_ref64_t reference,
	mach_msg_type_number_t referenceCnt,
	io_object_t *notification )
{
	return kIOReturnUnsupported;
}

/* Routine io_service_add_notification_bin */
kern_return_t
is_io_service_add_notification_bin
(
	mach_port_t main_port,
	io_name_t notification_type,
	io_struct_inband_t matching,
	mach_msg_type_number_t matchingCnt,
	mach_port_t wake_port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	io_object_t *notification)
{
	io_async_ref_t zreference;

	if (referenceCnt > ASYNC_REF_COUNT) {
		return kIOReturnBadArgument;
	}
	bcopy(&reference[0], &zreference[0], referenceCnt * sizeof(zreference[0]));
	bzero(&zreference[referenceCnt], (ASYNC_REF_COUNT - referenceCnt) * sizeof(zreference[0]));

	return internal_io_service_add_notification(main_port, notification_type,
	           matching, matchingCnt, wake_port, &zreference[0], sizeof(io_async_ref_t),
	           false, notification);
}

/* Routine io_service_add_notification_bin_64 */
kern_return_t
is_io_service_add_notification_bin_64
(
	mach_port_t main_port,
	io_name_t notification_type,
	io_struct_inband_t matching,
	mach_msg_type_number_t matchingCnt,
	mach_port_t wake_port,
	io_async_ref64_t reference,
	mach_msg_type_number_t referenceCnt,
	io_object_t *notification)
{
	io_async_ref64_t zreference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	bcopy(&reference[0], &zreference[0], referenceCnt * sizeof(zreference[0]));
	bzero(&zreference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(zreference[0]));

	return internal_io_service_add_notification(main_port, notification_type,
	           matching, matchingCnt, wake_port, &zreference[0], sizeof(io_async_ref64_t),
	           true, notification);
}

static kern_return_t
internal_io_service_add_notification_ool(
	mach_port_t main_port,
	io_name_t notification_type,
	io_buf_ptr_t matching,
	mach_msg_type_number_t matchingCnt,
	mach_port_t wake_port,
	void * reference,
	vm_size_t referenceSize,
	bool client64,
	kern_return_t *result,
	io_object_t *notification )
{
	kern_return_t       kr;
	vm_offset_t         data;
	vm_map_offset_t     map_data;

	kr = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t) matching );
	data = CAST_DOWN(vm_offset_t, map_data);

	if (KERN_SUCCESS == kr) {
		// must return success after vm_map_copyout() succeeds
		// and mig will copy out objects on success
		*notification = NULL;
		*result = internal_io_service_add_notification( main_port, notification_type,
		    (char *) data, matchingCnt, wake_port, reference, referenceSize, client64, notification );
		vm_deallocate( kernel_map, data, matchingCnt );
	}

	return kr;
}

/* Routine io_service_add_notification_ool */
kern_return_t
is_io_service_add_notification_ool(
	mach_port_t main_port,
	io_name_t notification_type,
	io_buf_ptr_t matching,
	mach_msg_type_number_t matchingCnt,
	mach_port_t wake_port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	kern_return_t *result,
	io_object_t *notification )
{
	io_async_ref_t zreference;

	if (referenceCnt > ASYNC_REF_COUNT) {
		return kIOReturnBadArgument;
	}
	bcopy(&reference[0], &zreference[0], referenceCnt * sizeof(zreference[0]));
	bzero(&zreference[referenceCnt], (ASYNC_REF_COUNT - referenceCnt) * sizeof(zreference[0]));

	return internal_io_service_add_notification_ool(main_port, notification_type,
	           matching, matchingCnt, wake_port, &zreference[0], sizeof(io_async_ref_t),
	           false, result, notification);
}

/* Routine io_service_add_notification_ool_64 */
kern_return_t
is_io_service_add_notification_ool_64(
	mach_port_t main_port,
	io_name_t notification_type,
	io_buf_ptr_t matching,
	mach_msg_type_number_t matchingCnt,
	mach_port_t wake_port,
	io_async_ref64_t reference,
	mach_msg_type_number_t referenceCnt,
	kern_return_t *result,
	io_object_t *notification )
{
	io_async_ref64_t zreference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	bcopy(&reference[0], &zreference[0], referenceCnt * sizeof(zreference[0]));
	bzero(&zreference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(zreference[0]));

	return internal_io_service_add_notification_ool(main_port, notification_type,
	           matching, matchingCnt, wake_port, &zreference[0], sizeof(io_async_ref64_t),
	           true, result, notification);
}

/* Routine io_service_add_notification_old */
kern_return_t
is_io_service_add_notification_old(
	mach_port_t main_port,
	io_name_t notification_type,
	io_string_t matching,
	mach_port_t port,
	// for binary compatibility reasons, this must be natural_t for ILP32
	natural_t ref,
	io_object_t * notification )
{
	return is_io_service_add_notification( main_port, notification_type,
	           matching, port, &ref, 1, notification );
}


static kern_return_t
internal_io_service_add_interest_notification(
	io_object_t _service,
	io_name_t type_of_interest,
	mach_port_t port,
	void * reference,
	vm_size_t referenceSize,
	bool client64,
	io_object_t * notification )
{
	IOServiceMessageUserNotification *  userNotify = NULL;
	IONotifier *                        notify = NULL;
	const OSSymbol *                    sym;
	IOReturn                            err;

	CHECK( IOService, _service, service );

	err = kIOReturnNoResources;
	if ((sym = OSSymbol::withCString( type_of_interest ))) {
		do {
			userNotify = new IOServiceMessageUserNotification;

			if (userNotify && !userNotify->init( port, kIOServiceMessageNotificationType,
			    reference, referenceSize, client64 )) {
				userNotify->release();
				userNotify = NULL;
			}
			if (!userNotify) {
				continue;
			}

			notify = service->registerInterest( sym,
			    &userNotify->_handler, userNotify );
			if (notify) {
				*notification = userNotify;
				userNotify->setNotification( notify );
				err = kIOReturnSuccess;
			} else {
				err = kIOReturnUnsupported;
			}
		} while (false);

		sym->release();
	}

	if ((kIOReturnSuccess != err) && userNotify) {
		userNotify->setNotification(NULL);
		userNotify->invalidatePort();
		userNotify->release();
		userNotify = NULL;
	}

	return err;
}

/* Routine io_service_add_message_notification */
kern_return_t
is_io_service_add_interest_notification(
	io_object_t service,
	io_name_t type_of_interest,
	mach_port_t port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	io_object_t * notification )
{
	io_async_ref_t zreference;

	if (referenceCnt > ASYNC_REF_COUNT) {
		return kIOReturnBadArgument;
	}
	bcopy(&reference[0], &zreference[0], referenceCnt * sizeof(zreference[0]));
	bzero(&zreference[referenceCnt], (ASYNC_REF_COUNT - referenceCnt) * sizeof(zreference[0]));

	return internal_io_service_add_interest_notification(service, type_of_interest,
	           port, &zreference[0], sizeof(io_async_ref_t), false, notification);
}

/* Routine io_service_add_interest_notification_64 */
kern_return_t
is_io_service_add_interest_notification_64(
	io_object_t service,
	io_name_t type_of_interest,
	mach_port_t wake_port,
	io_async_ref64_t reference,
	mach_msg_type_number_t referenceCnt,
	io_object_t *notification )
{
	io_async_ref64_t zreference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	bcopy(&reference[0], &zreference[0], referenceCnt * sizeof(zreference[0]));
	bzero(&zreference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(zreference[0]));

	return internal_io_service_add_interest_notification(service, type_of_interest,
	           wake_port, &zreference[0], sizeof(io_async_ref64_t), true, notification);
}


/* Routine io_service_acknowledge_notification */
kern_return_t
is_io_service_acknowledge_notification(
	io_object_t _service,
	natural_t notify_ref,
	natural_t response )
{
	CHECK( IOService, _service, service );

	return service->acknowledgeNotification((IONotificationRef)(uintptr_t) notify_ref,
	           (IOOptionBits) response );
}

/* Routine io_connect_get_semaphore */
kern_return_t
is_io_connect_get_notification_semaphore(
	io_connect_t connection,
	natural_t notification_type,
	semaphore_t *semaphore )
{
	IOReturn ret;
	CHECK( IOUserClient, connection, client );

	IOStatisticsClientCall();
	client->ipcEnter(kIPCLockWrite);
	ret = client->getNotificationSemaphore((UInt32) notification_type,
	    semaphore );
	client->ipcExit(kIPCLockWrite);

	return ret;
}

/* Routine io_registry_get_root_entry */
kern_return_t
is_io_registry_get_root_entry(
	mach_port_t main_port,
	io_object_t *root )
{
	IORegistryEntry *   entry;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	entry = IORegistryEntry::getRegistryRoot();
	if (entry) {
		entry->retain();
	}
	*root = entry;

	return kIOReturnSuccess;
}

/* Routine io_registry_create_iterator */
kern_return_t
is_io_registry_create_iterator(
	mach_port_t main_port,
	io_name_t plane,
	uint32_t options,
	io_object_t *iterator )
{
	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	*iterator = IOUserIterator::withIterator(
		IORegistryIterator::iterateOver(
			IORegistryEntry::getPlane( plane ), options ));

	return *iterator ? kIOReturnSuccess : kIOReturnBadArgument;
}

/* Routine io_registry_entry_create_iterator */
kern_return_t
is_io_registry_entry_create_iterator(
	io_object_t registry_entry,
	io_name_t plane,
	uint32_t options,
	io_object_t *iterator )
{
	CHECK( IORegistryEntry, registry_entry, entry );

	*iterator = IOUserIterator::withIterator(
		IORegistryIterator::iterateOver( entry,
		IORegistryEntry::getPlane( plane ), options ));

	return *iterator ? kIOReturnSuccess : kIOReturnBadArgument;
}

/* Routine io_registry_iterator_enter */
kern_return_t
is_io_registry_iterator_enter_entry(
	io_object_t iterator )
{
	CHECKLOCKED( IORegistryIterator, iterator, iter );

	IOLockLock(&oIter->lock);
	iter->enterEntry();
	IOLockUnlock(&oIter->lock);

	return kIOReturnSuccess;
}

/* Routine io_registry_iterator_exit */
kern_return_t
is_io_registry_iterator_exit_entry(
	io_object_t iterator )
{
	bool        didIt;

	CHECKLOCKED( IORegistryIterator, iterator, iter );

	IOLockLock(&oIter->lock);
	didIt = iter->exitEntry();
	IOLockUnlock(&oIter->lock);

	return didIt ? kIOReturnSuccess : kIOReturnNoDevice;
}

/* Routine io_registry_entry_from_path */
kern_return_t
is_io_registry_entry_from_path(
	mach_port_t main_port,
	io_string_t path,
	io_object_t *registry_entry )
{
	IORegistryEntry *   entry;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	entry = IORegistryEntry::fromPath( path );

	if (!entry && IOTaskRegistryCompatibility(current_task())) {
		OSDictionary * matching;
		const OSObject * objects[2] = { kOSBooleanTrue, NULL };
		const OSSymbol * keys[2]    = { gIOCompatibilityMatchKey, gIOPathMatchKey };

		objects[1] = OSString::withCStringNoCopy(path);
		matching = OSDictionary::withObjects(objects, keys, 2, 2);
		if (matching) {
			entry = IOService::copyMatchingService(matching);
		}
		OSSafeReleaseNULL(matching);
		OSSafeReleaseNULL(objects[1]);
	}

	*registry_entry = entry;

	return kIOReturnSuccess;
}


/* Routine io_registry_entry_from_path */
kern_return_t
is_io_registry_entry_from_path_ool(
	mach_port_t main_port,
	io_string_inband_t path,
	io_buf_ptr_t path_ool,
	mach_msg_type_number_t path_oolCnt,
	kern_return_t *result,
	io_object_t *registry_entry)
{
	IORegistryEntry *   entry;
	vm_map_offset_t     map_data;
	const char *        cpath;
	IOReturn            res;
	kern_return_t       err;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	map_data = 0;
	entry    = NULL;
	res = err = KERN_SUCCESS;
	if (path[0]) {
		cpath = path;
	} else {
		if (!path_oolCnt) {
			return kIOReturnBadArgument;
		}
		if (path_oolCnt > (sizeof(io_struct_inband_t) * 1024)) {
			return kIOReturnMessageTooLarge;
		}

		err = vm_map_copyout(kernel_map, &map_data, (vm_map_copy_t) path_ool);
		if (KERN_SUCCESS == err) {
			// must return success to mig after vm_map_copyout() succeeds, so result is actual
			cpath = CAST_DOWN(const char *, map_data);
			if (cpath[path_oolCnt - 1]) {
				res = kIOReturnBadArgument;
			}
		}
	}

	if ((KERN_SUCCESS == err) && (KERN_SUCCESS == res)) {
		entry = IORegistryEntry::fromPath(cpath);
		res = entry ? kIOReturnSuccess : kIOReturnNotFound;
	}

	if (map_data) {
		vm_deallocate(kernel_map, map_data, path_oolCnt);
	}

	if (KERN_SUCCESS != err) {
		res = err;
	}
	*registry_entry = entry;
	*result = res;

	return err;
}


/* Routine io_registry_entry_in_plane */
kern_return_t
is_io_registry_entry_in_plane(
	io_object_t registry_entry,
	io_name_t plane,
	boolean_t *inPlane )
{
	CHECK( IORegistryEntry, registry_entry, entry );

	*inPlane = entry->inPlane( IORegistryEntry::getPlane( plane ));

	return kIOReturnSuccess;
}


/* Routine io_registry_entry_get_path */
kern_return_t
is_io_registry_entry_get_path(
	io_object_t registry_entry,
	io_name_t plane,
	io_string_t path )
{
	int         length;
	CHECK( IORegistryEntry, registry_entry, entry );

	length = sizeof(io_string_t);
	if (entry->getPath( path, &length, IORegistryEntry::getPlane( plane ))) {
		return kIOReturnSuccess;
	} else {
		return kIOReturnBadArgument;
	}
}

/* Routine io_registry_entry_get_path */
kern_return_t
is_io_registry_entry_get_path_ool(
	io_object_t registry_entry,
	io_name_t plane,
	io_string_inband_t path,
	io_buf_ptr_t *path_ool,
	mach_msg_type_number_t *path_oolCnt)
{
	enum   { kMaxPath = 16384 };
	IOReturn err;
	int      length;
	char   * buf;

	CHECK( IORegistryEntry, registry_entry, entry );

	*path_ool    = NULL;
	*path_oolCnt = 0;
	length = sizeof(io_string_inband_t);
	if (entry->getPath(path, &length, IORegistryEntry::getPlane(plane))) {
		err = kIOReturnSuccess;
	} else {
		length = kMaxPath;
		buf = IONewData(char, length);
		if (!buf) {
			err = kIOReturnNoMemory;
		} else if (!entry->getPath(buf, &length, IORegistryEntry::getPlane(plane))) {
			err = kIOReturnError;
		} else {
			*path_oolCnt = length;
			err = copyoutkdata(buf, length, path_ool);
		}
		if (buf) {
			IODeleteData(buf, char, kMaxPath);
		}
	}

	return err;
}


/* Routine io_registry_entry_get_name */
kern_return_t
is_io_registry_entry_get_name(
	io_object_t registry_entry,
	io_name_t name )
{
	CHECK( IORegistryEntry, registry_entry, entry );

	strncpy( name, entry->getName(), sizeof(io_name_t));

	return kIOReturnSuccess;
}

/* Routine io_registry_entry_get_name_in_plane */
kern_return_t
is_io_registry_entry_get_name_in_plane(
	io_object_t registry_entry,
	io_name_t planeName,
	io_name_t name )
{
	const IORegistryPlane * plane;
	CHECK( IORegistryEntry, registry_entry, entry );

	if (planeName[0]) {
		plane = IORegistryEntry::getPlane( planeName );
	} else {
		plane = NULL;
	}

	strncpy( name, entry->getName( plane), sizeof(io_name_t));

	return kIOReturnSuccess;
}

/* Routine io_registry_entry_get_location_in_plane */
kern_return_t
is_io_registry_entry_get_location_in_plane(
	io_object_t registry_entry,
	io_name_t planeName,
	io_name_t location )
{
	const IORegistryPlane * plane;
	CHECK( IORegistryEntry, registry_entry, entry );

	if (planeName[0]) {
		plane = IORegistryEntry::getPlane( planeName );
	} else {
		plane = NULL;
	}

	const char * cstr = entry->getLocation( plane );

	if (cstr) {
		strncpy( location, cstr, sizeof(io_name_t));
		return kIOReturnSuccess;
	} else {
		return kIOReturnNotFound;
	}
}

/* Routine io_registry_entry_get_registry_entry_id */
kern_return_t
is_io_registry_entry_get_registry_entry_id(
	io_object_t registry_entry,
	uint64_t *entry_id )
{
	CHECK( IORegistryEntry, registry_entry, entry );

	*entry_id = entry->getRegistryEntryID();

	return kIOReturnSuccess;
}


static OSObject *
IOCopyPropertyCompatible(IORegistryEntry * regEntry, const char * name)
{
	OSObject     * obj;
	OSObject     * compatProperties;
	OSDictionary * props;

	obj = regEntry->copyProperty(name);
	if (obj) {
		return obj;
	}

	compatProperties = regEntry->copyProperty(gIOUserServicePropertiesKey);
	if (!compatProperties
	    && IOTaskRegistryCompatibility(current_task())) {
		compatProperties = regEntry->copyProperty(gIOCompatibilityPropertiesKey);
	}
	if (compatProperties) {
		props = OSDynamicCast(OSDictionary, compatProperties);
		if (props) {
			obj = props->getObject(name);
			if (obj) {
				obj->retain();
			}
		}
		compatProperties->release();
	}

	return obj;
}

/* Routine io_registry_entry_get_property */
kern_return_t
is_io_registry_entry_get_property_bytes(
	io_object_t registry_entry,
	io_name_t property_name,
	io_struct_inband_t buf,
	mach_msg_type_number_t *dataCnt )
{
	OSObject    *       obj;
	OSData      *       data;
	OSString    *       str;
	OSBoolean   *       boo;
	OSNumber    *       off;
	UInt64              offsetBytes;
	unsigned int        len = 0;
	const void *        bytes = NULL;
	IOReturn            ret = kIOReturnSuccess;

	CHECK( IORegistryEntry, registry_entry, entry );

#if CONFIG_MACF
	if (0 != mac_iokit_check_get_property(kauth_cred_get(), entry, property_name)) {
		return kIOReturnNotPermitted;
	}
#endif

	obj = IOCopyPropertyCompatible(entry, property_name);
	if (!obj) {
		return kIOReturnNoResources;
	}

	// One day OSData will be a common container base class
	// until then...
	if ((data = OSDynamicCast( OSData, obj ))) {
		len = data->getLength();
		bytes = data->getBytesNoCopy();
		if (!data->isSerializable()) {
			len = 0;
		}
	} else if ((str = OSDynamicCast( OSString, obj ))) {
		len = str->getLength() + 1;
		bytes = str->getCStringNoCopy();
	} else if ((boo = OSDynamicCast( OSBoolean, obj ))) {
		len = boo->isTrue() ? sizeof("Yes") : sizeof("No");
		bytes = boo->isTrue() ? "Yes" : "No";
	} else if ((off = OSDynamicCast( OSNumber, obj ))) {
		offsetBytes = off->unsigned64BitValue();
		len = off->numberOfBytes();
		if (len > sizeof(offsetBytes)) {
			len = sizeof(offsetBytes);
		}
		bytes = &offsetBytes;
#ifdef __BIG_ENDIAN__
		bytes = (const void *)
		    (((UInt32) bytes) + (sizeof(UInt64) - len));
#endif
	} else {
		ret = kIOReturnBadArgument;
	}

	if (bytes) {
		if (*dataCnt < len) {
			ret = kIOReturnIPCError;
		} else {
			*dataCnt = len;
			bcopy( bytes, buf, len );
		}
	}
	obj->release();

	return ret;
}


/* Routine io_registry_entry_get_property */
kern_return_t
is_io_registry_entry_get_property(
	io_object_t registry_entry,
	io_name_t property_name,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt )
{
	kern_return_t       err;
	unsigned int        len;
	OSObject *          obj;

	CHECK( IORegistryEntry, registry_entry, entry );

#if CONFIG_MACF
	if (0 != mac_iokit_check_get_property(kauth_cred_get(), entry, property_name)) {
		return kIOReturnNotPermitted;
	}
#endif

	obj = IOCopyPropertyCompatible(entry, property_name);
	if (!obj) {
		return kIOReturnNotFound;
	}

	OSSerialize * s = OSSerialize::withCapacity(4096);
	if (!s) {
		obj->release();
		return kIOReturnNoMemory;
	}

	if (obj->serialize( s )) {
		len = s->getLength();
		*propertiesCnt = len;
		err = copyoutkdata( s->text(), len, properties );
	} else {
		err = kIOReturnUnsupported;
	}

	s->release();
	obj->release();

	return err;
}

/* Routine io_registry_entry_get_property_recursively */
kern_return_t
is_io_registry_entry_get_property_recursively(
	io_object_t registry_entry,
	io_name_t plane,
	io_name_t property_name,
	uint32_t options,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt )
{
	kern_return_t       err;
	unsigned int        len;
	OSObject *          obj;

	CHECK( IORegistryEntry, registry_entry, entry );

#if CONFIG_MACF
	if (0 != mac_iokit_check_get_property(kauth_cred_get(), entry, property_name)) {
		return kIOReturnNotPermitted;
	}
#endif

	obj = entry->copyProperty( property_name,
	    IORegistryEntry::getPlane( plane ), options );
	if (!obj) {
		return kIOReturnNotFound;
	}

	OSSerialize * s = OSSerialize::withCapacity(4096);
	if (!s) {
		obj->release();
		return kIOReturnNoMemory;
	}

	if (obj->serialize( s )) {
		len = s->getLength();
		*propertiesCnt = len;
		err = copyoutkdata( s->text(), len, properties );
	} else {
		err = kIOReturnUnsupported;
	}

	s->release();
	obj->release();

	return err;
}

/* Routine io_registry_entry_get_properties */
kern_return_t
is_io_registry_entry_get_properties(
	io_object_t registry_entry,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt )
{
	return kIOReturnUnsupported;
}

#if CONFIG_MACF

struct GetPropertiesEditorRef {
	kauth_cred_t      cred;
	IORegistryEntry * entry;
	OSCollection    * root;
};

static const LIBKERN_RETURNS_RETAINED OSMetaClassBase *
GetPropertiesEditor(void                  * reference,
    OSSerialize           * s,
    OSCollection          * container,
    const OSSymbol        * name,
    const OSMetaClassBase * value)
{
	GetPropertiesEditorRef * ref = (typeof(ref))reference;

	if (!ref->root) {
		ref->root = container;
	}
	if (ref->root == container) {
		if (0 != mac_iokit_check_get_property(ref->cred, ref->entry, name->getCStringNoCopy())) {
			value = NULL;
		}
	}
	if (value) {
		value->retain();
	}
	return value;
}

#endif /* CONFIG_MACF */

/* Routine io_registry_entry_get_properties_bin_buf */
kern_return_t
is_io_registry_entry_get_properties_bin_buf(
	io_object_t registry_entry,
	mach_vm_address_t buf,
	mach_vm_size_t *bufsize,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt)
{
	kern_return_t          err = kIOReturnSuccess;
	unsigned int           len;
	OSObject             * compatProperties;
	OSSerialize          * s;
	OSSerialize::Editor    editor = NULL;
	void                 * editRef = NULL;

	CHECK(IORegistryEntry, registry_entry, entry);

#if CONFIG_MACF
	GetPropertiesEditorRef ref;
	if (mac_iokit_check_filter_properties(kauth_cred_get(), entry)) {
		editor    = &GetPropertiesEditor;
		editRef   = &ref;
		ref.cred  = kauth_cred_get();
		ref.entry = entry;
		ref.root  = NULL;
	}
#endif

	s = OSSerialize::binaryWithCapacity(4096, editor, editRef);
	if (!s) {
		return kIOReturnNoMemory;
	}


	compatProperties = entry->copyProperty(gIOUserServicePropertiesKey);
	if (!compatProperties
	    && IOTaskRegistryCompatibility(current_task())) {
		compatProperties = entry->copyProperty(gIOCompatibilityPropertiesKey);
	}

	if (compatProperties) {
		OSDictionary * dict;

		dict = entry->dictionaryWithProperties();
		if (!dict) {
			err = kIOReturnNoMemory;
		} else {
			dict->removeObject(gIOUserServicePropertiesKey);
			dict->removeObject(gIOCompatibilityPropertiesKey);
			dict->merge(OSDynamicCast(OSDictionary, compatProperties));
			if (!dict->serialize(s)) {
				err = kIOReturnUnsupported;
			}
			dict->release();
		}
		compatProperties->release();
	} else if (!entry->serializeProperties(s)) {
		err = kIOReturnUnsupported;
	}

	if (kIOReturnSuccess == err) {
		len = s->getLength();
		if (buf && bufsize && len <= *bufsize) {
			*bufsize = len;
			*propertiesCnt = 0;
			*properties = nullptr;
			if (copyout(s->text(), buf, len)) {
				err = kIOReturnVMError;
			} else {
				err = kIOReturnSuccess;
			}
		} else {
			if (bufsize) {
				*bufsize = 0;
			}
			*propertiesCnt = len;
			err = copyoutkdata( s->text(), len, properties );
		}
	}
	s->release();

	return err;
}

/* Routine io_registry_entry_get_properties_bin */
kern_return_t
is_io_registry_entry_get_properties_bin(
	io_object_t registry_entry,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt)
{
	return is_io_registry_entry_get_properties_bin_buf(registry_entry,
	           0, NULL, properties, propertiesCnt);
}

/* Routine io_registry_entry_get_property_bin_buf */
kern_return_t
is_io_registry_entry_get_property_bin_buf(
	io_object_t registry_entry,
	io_name_t plane,
	io_name_t property_name,
	uint32_t options,
	mach_vm_address_t buf,
	mach_vm_size_t *bufsize,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt )
{
	kern_return_t       err;
	unsigned int        len;
	OSObject *          obj;
	const OSSymbol *    sym;

	CHECK( IORegistryEntry, registry_entry, entry );

#if CONFIG_MACF
	if (0 != mac_iokit_check_get_property(kauth_cred_get(), entry, property_name)) {
		return kIOReturnNotPermitted;
	}
#endif

	sym = OSSymbol::withCString(property_name);
	if (!sym) {
		return kIOReturnNoMemory;
	}

	err = kIOReturnNotFound;
	if (gIORegistryEntryPropertyKeysKey == sym) {
		obj = entry->copyPropertyKeys();
	} else {
		if ((kIORegistryIterateRecursively & options) && plane[0]) {
			obj = IOCopyPropertyCompatible(entry, property_name);
			if (obj == NULL) {
				IORegistryIterator * iter = IORegistryIterator::iterateOver(entry, IORegistryEntry::getPlane(plane), options);
				if (iter) {
					while ((NULL == obj) && (entry = iter->getNextObject())) {
						OSObject * currentObj = IOCopyPropertyCompatible(entry, property_name);
#if CONFIG_MACF
						if (currentObj != NULL && 0 != mac_iokit_check_get_property(kauth_cred_get(), entry, property_name)) {
							// Record that MAC hook blocked this entry and property, and continue to next entry
							err = kIOReturnNotPermitted;
							OSSafeReleaseNULL(currentObj);
							continue;
						}
#endif
						obj = currentObj;
					}
					iter->release();
				}
			}
		} else {
			obj = IOCopyPropertyCompatible(entry, property_name);
		}
		if (obj && gIORemoveOnReadProperties->containsObject(sym)) {
			entry->removeProperty(sym);
		}
	}

	sym->release();
	if (!obj) {
		return err;
	}

	OSSerialize * s = OSSerialize::binaryWithCapacity(4096);
	if (!s) {
		obj->release();
		return kIOReturnNoMemory;
	}

	if (obj->serialize( s )) {
		len = s->getLength();
		if (buf && bufsize && len <= *bufsize) {
			*bufsize = len;
			*propertiesCnt = 0;
			*properties = nullptr;
			if (copyout(s->text(), buf, len)) {
				err = kIOReturnVMError;
			} else {
				err = kIOReturnSuccess;
			}
		} else {
			if (bufsize) {
				*bufsize = 0;
			}
			*propertiesCnt = len;
			err = copyoutkdata( s->text(), len, properties );
		}
	} else {
		err = kIOReturnUnsupported;
	}

	s->release();
	obj->release();

	return err;
}

/* Routine io_registry_entry_get_property_bin */
kern_return_t
is_io_registry_entry_get_property_bin(
	io_object_t registry_entry,
	io_name_t plane,
	io_name_t property_name,
	uint32_t options,
	io_buf_ptr_t *properties,
	mach_msg_type_number_t *propertiesCnt )
{
	return is_io_registry_entry_get_property_bin_buf(registry_entry, plane,
	           property_name, options, 0, NULL, properties, propertiesCnt);
}


/* Routine io_registry_entry_set_properties */
kern_return_t
is_io_registry_entry_set_properties
(
	io_object_t registry_entry,
	io_buf_ptr_t properties,
	mach_msg_type_number_t propertiesCnt,
	kern_return_t * result)
{
	OSObject *          obj;
	kern_return_t       err;
	IOReturn            res;
	vm_offset_t         data;
	vm_map_offset_t     map_data;

	CHECK( IORegistryEntry, registry_entry, entry );

	if (propertiesCnt > sizeof(io_struct_inband_t) * 1024) {
		return kIOReturnMessageTooLarge;
	}

	err = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t) properties );
	data = CAST_DOWN(vm_offset_t, map_data);

	if (KERN_SUCCESS == err) {
		FAKE_STACK_FRAME(entry->getMetaClass());

		// must return success after vm_map_copyout() succeeds
		obj = OSUnserializeXML((const char *) data, propertiesCnt );
		vm_deallocate( kernel_map, data, propertiesCnt );

		if (!obj) {
			res = kIOReturnBadArgument;
		}
#if CONFIG_MACF
		else if (0 != mac_iokit_check_set_properties(kauth_cred_get(),
		    registry_entry, obj)) {
			res = kIOReturnNotPermitted;
		}
#endif
		else {
			IOService    * service = OSDynamicCast(IOService, entry);
			OSDictionary * props = OSDynamicCast(OSDictionary, obj);
			OSObject     * allowable = entry->copyProperty(gIORegistryEntryAllowableSetPropertiesKey);
			OSArray      * allowableArray;

			if (!allowable) {
				res = kIOReturnSuccess;
			} else {
				if (!props) {
					res = kIOReturnNotPermitted;
				} else if (!(allowableArray = OSDynamicCast(OSArray, allowable))) {
					res = kIOReturnNotPermitted;
				} else {
					bool allFound __block, found __block;

					allFound = true;
					props->iterateObjects(^(const OSSymbol * key, OSObject * value) {
							found = false;
							for (unsigned int idx = 0; !found; idx++) {
							        OSObject * next = allowableArray->getObject(idx);
							        if (!next) {
							                break;
								}
							        found = next->isEqualTo(key);
							}
							allFound &= found;
							if (!found) {
							        IOLog("IORegistryEntrySetProperties(%s, %s) disallowed due to " kIORegistryEntryAllowableSetPropertiesKey "\n",
							        entry->getName(), key->getCStringNoCopy());
							}
							return !allFound;
						});
					res =  allFound ? kIOReturnSuccess : kIOReturnBadArgument;
				}
			}
			if (kIOReturnSuccess == res) {
				IOUserClient *
				    client = OSDynamicCast(IOUserClient, entry);

				if (client && client->defaultLockingSetProperties) {
					IORWLockWrite(&client->lock);
				}

				if (!client && (kOSBooleanTrue == entry->getProperty(gIORegistryEntryDefaultLockingSetPropertiesKey))) {
					res = entry->runPropertyActionBlock(^IOReturn (void) {
							return entry->setProperties( obj );
						});
				} else {
					res = entry->setProperties( obj );
				}

				if (client && client->defaultLockingSetProperties) {
					IORWLockUnlock(&client->lock);
				}
				if (service && props && service->hasUserServer()) {
					res = service->UserSetProperties(props);
				}
			}
			OSSafeReleaseNULL(allowable);
		}
		if (obj) {
			obj->release();
		}

		FAKE_STACK_FRAME_END();
	} else {
		res = err;
	}

	*result = res;
	return err;
}

/* Routine io_registry_entry_get_child_iterator */
kern_return_t
is_io_registry_entry_get_child_iterator(
	io_object_t registry_entry,
	io_name_t plane,
	io_object_t *iterator )
{
	CHECK( IORegistryEntry, registry_entry, entry );

	*iterator = IOUserIterator::withIterator(entry->getChildIterator(
		    IORegistryEntry::getPlane( plane )));

	return kIOReturnSuccess;
}

/* Routine io_registry_entry_get_parent_iterator */
kern_return_t
is_io_registry_entry_get_parent_iterator(
	io_object_t registry_entry,
	io_name_t plane,
	io_object_t *iterator)
{
	CHECK( IORegistryEntry, registry_entry, entry );

	*iterator = IOUserIterator::withIterator(entry->getParentIterator(
		    IORegistryEntry::getPlane( plane )));

	return kIOReturnSuccess;
}

/* Routine io_service_get_busy_state */
kern_return_t
is_io_service_get_busy_state(
	io_object_t _service,
	uint32_t *busyState )
{
	CHECK( IOService, _service, service );

	*busyState = service->getBusyState();

	return kIOReturnSuccess;
}

/* Routine io_service_get_state */
kern_return_t
is_io_service_get_state(
	io_object_t _service,
	uint64_t *state,
	uint32_t *busy_state,
	uint64_t *accumulated_busy_time )
{
	CHECK( IOService, _service, service );

	*state                 = service->getState();
	*busy_state            = service->getBusyState();
	*accumulated_busy_time = service->getAccumulatedBusyTime();

	return kIOReturnSuccess;
}

/* Routine io_service_wait_quiet */
kern_return_t
is_io_service_wait_quiet(
	io_object_t _service,
	mach_timespec_t wait_time )
{
	uint64_t    timeoutNS;

	CHECK( IOService, _service, service );

	timeoutNS = wait_time.tv_sec;
	timeoutNS *= kSecondScale;
	timeoutNS += wait_time.tv_nsec;

	return service->waitQuiet(timeoutNS);
}

/* Routine io_service_wait_quiet_with_options */
kern_return_t
is_io_service_wait_quiet_with_options(
	io_object_t _service,
	mach_timespec_t wait_time,
	uint32_t options )
{
	uint64_t    timeoutNS;

	CHECK( IOService, _service, service );

	timeoutNS = wait_time.tv_sec;
	timeoutNS *= kSecondScale;
	timeoutNS += wait_time.tv_nsec;

	if ((options & kIOWaitQuietPanicOnFailure) && !IOCurrentTaskHasEntitlement(kIOWaitQuietPanicsEntitlement)) {
		OSString * taskName = IOCopyLogNameForPID(proc_selfpid());
		IOLog("IOServiceWaitQuietWithOptions(%s): Not entitled\n", taskName ? taskName->getCStringNoCopy() : "");
		OSSafeReleaseNULL(taskName);

		/* strip this option from the options before calling waitQuietWithOptions */
		options &= ~kIOWaitQuietPanicOnFailure;
	}

	return service->waitQuietWithOptions(timeoutNS, options);
}


/* Routine io_service_request_probe */
kern_return_t
is_io_service_request_probe(
	io_object_t _service,
	uint32_t options )
{
	CHECK( IOService, _service, service );

	return service->requestProbe( options );
}

/* Routine io_service_get_authorization_id */
kern_return_t
is_io_service_get_authorization_id(
	io_object_t _service,
	uint64_t *authorization_id )
{
	kern_return_t          kr;

	CHECK( IOService, _service, service );

	kr = IOUserClient::clientHasPrivilege((void *) current_task(),
	    kIOClientPrivilegeAdministrator );
	if (kIOReturnSuccess != kr) {
		return kr;
	}

#if defined(XNU_TARGET_OS_OSX)
	*authorization_id = service->getAuthorizationID();
#else /* defined(XNU_TARGET_OS_OSX) */
	*authorization_id = 0;
	kr = kIOReturnUnsupported;
#endif /* defined(XNU_TARGET_OS_OSX) */

	return kr;
}

/* Routine io_service_set_authorization_id */
kern_return_t
is_io_service_set_authorization_id(
	io_object_t _service,
	uint64_t authorization_id )
{
	CHECK( IOService, _service, service );

#if defined(XNU_TARGET_OS_OSX)
	return service->setAuthorizationID( authorization_id );
#else /* defined(XNU_TARGET_OS_OSX) */
	return kIOReturnUnsupported;
#endif /* defined(XNU_TARGET_OS_OSX) */
}

/* Routine io_service_open_ndr */
kern_return_t
is_io_service_open_extended(
	io_object_t _service,
	task_t owningTask,
	uint32_t connect_type,
	NDR_record_t ndr,
	io_buf_ptr_t properties,
	mach_msg_type_number_t propertiesCnt,
	kern_return_t * result,
	io_object_t *connection )
{
	IOUserClient * client = NULL;
	kern_return_t  err = KERN_SUCCESS;
	IOReturn       res = kIOReturnSuccess;
	OSDictionary * propertiesDict = NULL;
	bool           disallowAccess = false;

	CHECK( IOService, _service, service );

	if (!owningTask) {
		return kIOReturnBadArgument;
	}
	assert(owningTask == current_task());
	if (owningTask != current_task()) {
		return kIOReturnBadArgument;
	}

#if CONFIG_MACF
	if (mac_iokit_check_open_service(kauth_cred_get(), service, connect_type) != 0) {
		return kIOReturnNotPermitted;
	}
#endif
	do{
		if (properties) {
			return kIOReturnUnsupported;
		}
#if 0
		{
			OSObject *      obj;
			vm_offset_t     data;
			vm_map_offset_t map_data;

			if (propertiesCnt > sizeof(io_struct_inband_t)) {
				return kIOReturnMessageTooLarge;
			}

			err = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t) properties );
			res = err;
			data = CAST_DOWN(vm_offset_t, map_data);
			if (KERN_SUCCESS == err) {
				// must return success after vm_map_copyout() succeeds
				obj = OSUnserializeXML((const char *) data, propertiesCnt );
				vm_deallocate( kernel_map, data, propertiesCnt );
				propertiesDict = OSDynamicCast(OSDictionary, obj);
				if (!propertiesDict) {
					res = kIOReturnBadArgument;
					if (obj) {
						obj->release();
					}
				}
			}
			if (kIOReturnSuccess != res) {
				break;
			}
		}
#endif
		res = service->newUserClient( owningTask, (void *) owningTask,
		    connect_type, propertiesDict, &client );

		if (propertiesDict) {
			propertiesDict->release();
		}

		if (res == kIOReturnSuccess && OSDynamicCast(IOUserClient, client) == NULL) {
			// client should always be a IOUserClient
			res = kIOReturnError;
		}

		if (res == kIOReturnSuccess) {
			if (!client->reserved) {
				if (!client->reserve()) {
					client->clientClose();
					OSSafeReleaseNULL(client);
					res = kIOReturnNoMemory;
				}
			}
		}

		if (res == kIOReturnSuccess) {
			OSString * creatorName = IOCopyLogNameForPID(proc_selfpid());
			if (creatorName) {
				client->setProperty(kIOUserClientCreatorKey, creatorName);
			}
			const char * creatorNameCStr = creatorName ? creatorName->getCStringNoCopy() : "<unknown>";
			client->sharedInstance = (NULL != client->getProperty(kIOUserClientSharedInstanceKey));
			if (client->sharedInstance) {
				IOLockLock(gIOUserClientOwnersLock);
			}
			if (!client->opened) {
				client->opened = true;

				client->messageAppSuspended = (NULL != client->getProperty(kIOUserClientMessageAppSuspendedKey));
				{
					OSObject * obj;
					extern const OSSymbol * gIOSurfaceIdentifier;
					obj = client->getProperty(kIOUserClientDefaultLockingKey);
					bool hasProps = false;

					client->uc2022 = (NULL != OSDynamicCast(IOUserClient2022, client));
					if (obj) {
						hasProps = true;
						client->defaultLocking = (kOSBooleanFalse != client->getProperty(kIOUserClientDefaultLockingKey));
					} else if (client->uc2022) {
						res = kIOReturnError;
					}
					obj = client->getProperty(kIOUserClientDefaultLockingSetPropertiesKey);
					if (obj) {
						hasProps = true;
						client->defaultLockingSetProperties = (kOSBooleanFalse != client->getProperty(kIOUserClientDefaultLockingSetPropertiesKey));
					} else if (client->uc2022) {
						res = kIOReturnError;
					}
					obj = client->getProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey);
					if (obj) {
						hasProps = true;
						client->defaultLockingSingleThreadExternalMethod = (kOSBooleanFalse != client->getProperty(kIOUserClientDefaultLockingSingleThreadExternalMethodKey));
					} else if (client->uc2022) {
						res = kIOReturnError;
					}
					if (kIOReturnSuccess != res) {
						IOLog("IOUC %s requires kIOUserClientDefaultLockingKey, kIOUserClientDefaultLockingSetPropertiesKey, kIOUserClientDefaultLockingSingleThreadExternalMethodKey\n",
						    client->getMetaClass()->getClassName());
					}
					if (!hasProps) {
						const OSMetaClass * meta;
						OSKext            * kext;
						meta = client->getMetaClass();
						kext = meta->getKext();
						if (!kext || !kext->hasDependency(gIOSurfaceIdentifier)) {
							client->defaultLocking = true;
							client->defaultLockingSetProperties = false;
							client->defaultLockingSingleThreadExternalMethod = false;
							client->setProperty(kIOUserClientDefaultLockingKey, kOSBooleanTrue);
						}
					}
				}
			}
			if (client->sharedInstance) {
				IOLockUnlock(gIOUserClientOwnersLock);
			}

			OSObject     * requiredEntitlement = client->copyProperty(gIOUserClientEntitlementsKey);
			OSString * requiredEntitlementString = OSDynamicCast(OSString, requiredEntitlement);
			//If this is an IOUserClient2022, having kIOUserClientEntitlementsKey is mandatory.
			//If it has kIOUserClientEntitlementsKey, the value must be either kOSBooleanFalse or an OSString
			//If the value is kOSBooleanFalse, we allow access.
			//If the value is an OSString, we allow access if the task has the named entitlement
			if (client->uc2022) {
				if (!requiredEntitlement) {
					IOLog("IOUC %s missing " kIOUserClientEntitlementsKey " property\n",
					    client->getMetaClass()->getClassName());
					disallowAccess = true;
				} else if (!requiredEntitlementString && requiredEntitlement != kOSBooleanFalse) {
					IOLog("IOUC %s had " kIOUserClientEntitlementsKey "with value not boolean false or string\n", client->getMetaClass()->getClassName());
					disallowAccess = true;
				}
			}

			if (requiredEntitlement && disallowAccess == false) {
				if (kOSBooleanFalse == requiredEntitlement) {
					// allow
					disallowAccess = false;
				} else {
					disallowAccess = !IOTaskHasEntitlement(owningTask, requiredEntitlementString->getCStringNoCopy());
					if (disallowAccess) {
						IOLog("IOUC %s missing entitlement in process %s\n",
						    client->getMetaClass()->getClassName(), creatorNameCStr);
					}
				}
			}

			OSSafeReleaseNULL(requiredEntitlement);

			if (disallowAccess) {
				res = kIOReturnNotPrivileged;
			}
#if CONFIG_MACF
			else if (0 != mac_iokit_check_open(kauth_cred_get(), client, connect_type)) {
				IOLog("IOUC %s failed MACF in process %s\n",
				    client->getMetaClass()->getClassName(), creatorNameCStr);
				res = kIOReturnNotPermitted;
			}
#endif

			if ((kIOReturnSuccess == res)
			    && gIOUCFilterCallbacks
			    && gIOUCFilterCallbacks->io_filter_resolver) {
				io_filter_policy_t filterPolicy;
				filterPolicy = client->filterForTask(owningTask, 0);
				if (!filterPolicy) {
					res = gIOUCFilterCallbacks->io_filter_resolver(owningTask, client, connect_type, &filterPolicy);
					if (kIOReturnUnsupported == res) {
						res = kIOReturnSuccess;
					} else if (kIOReturnSuccess == res) {
						client->filterForTask(owningTask, filterPolicy);
					} else {
						IOLog("IOUC %s failed sandbox in process %s\n",
						    client->getMetaClass()->getClassName(), creatorNameCStr);
					}
				}
			}

			if (kIOReturnSuccess == res) {
				res = client->registerOwner(owningTask);
			}
			OSSafeReleaseNULL(creatorName);

			if (kIOReturnSuccess != res) {
				IOStatisticsClientCall();
				client->clientClose();
				client->setTerminateDefer(service, false);
				client->release();
				client = NULL;
				break;
			}
			client->setTerminateDefer(service, false);
		}
	}while (false);

	*connection = client;
	*result = res;

	return err;
}

/* Routine io_service_close */
kern_return_t
is_io_service_close(
	io_connect_t connection )
{
	OSSet * mappings;
	if ((mappings = OSDynamicCast(OSSet, connection))) {
		return kIOReturnSuccess;
	}

	CHECK( IOUserClient, connection, client );

	IOStatisticsClientCall();

	if (client->sharedInstance || OSCompareAndSwap8(0, 1, &client->closed)) {
		client->ipcEnter(kIPCLockWrite);
		client->clientClose();
		client->ipcExit(kIPCLockWrite);
	} else {
		IOLog("ignored is_io_service_close(0x%qx,%s)\n",
		    client->getRegistryEntryID(), client->getName());
	}

	return kIOReturnSuccess;
}

/* Routine io_connect_get_service */
kern_return_t
is_io_connect_get_service(
	io_connect_t connection,
	io_object_t *service )
{
	IOService * theService;

	CHECK( IOUserClient, connection, client );

	client->ipcEnter(kIPCLockNone);

	theService = client->getService();
	if (theService) {
		theService->retain();
	}

	client->ipcExit(kIPCLockNone);

	*service = theService;

	return theService ? kIOReturnSuccess : kIOReturnUnsupported;
}

/* Routine io_connect_set_notification_port */
kern_return_t
is_io_connect_set_notification_port(
	io_connect_t connection,
	uint32_t notification_type,
	mach_port_t port,
	uint32_t reference)
{
	kern_return_t ret;
	CHECK( IOUserClient, connection, client );

	IOStatisticsClientCall();

	client->ipcEnter(kIPCLockWrite);
	ret = client->registerNotificationPort( port, notification_type,
	    (io_user_reference_t) reference );
	client->ipcExit(kIPCLockWrite);

	return ret;
}

/* Routine io_connect_set_notification_port */
kern_return_t
is_io_connect_set_notification_port_64(
	io_connect_t connection,
	uint32_t notification_type,
	mach_port_t port,
	io_user_reference_t reference)
{
	kern_return_t ret;
	CHECK( IOUserClient, connection, client );

	IOStatisticsClientCall();

	client->ipcEnter(kIPCLockWrite);
	ret = client->registerNotificationPort( port, notification_type,
	    reference );
	client->ipcExit(kIPCLockWrite);

	return ret;
}

/* Routine io_connect_map_memory_into_task */
kern_return_t
is_io_connect_map_memory_into_task
(
	io_connect_t connection,
	uint32_t memory_type,
	task_t into_task,
	mach_vm_address_t *address,
	mach_vm_size_t *size,
	uint32_t flags
)
{
	IOReturn            err;
	IOMemoryMap *       map;

	CHECK( IOUserClient, connection, client );

	if (!into_task) {
		return kIOReturnBadArgument;
	}

	IOStatisticsClientCall();

	client->ipcEnter(client->defaultLocking ? kIPCLockWrite : kIPCLockNone);
	map = client->mapClientMemory64( memory_type, into_task, flags, *address );

	if (map) {
		*address = map->getAddress();
		if (size) {
			*size = map->getSize();
		}

		if (client->sharedInstance
		    || (into_task != current_task())) {
			// push a name out to the task owning the map,
			// so we can clean up maps
			mach_port_name_t name __unused =
			    IOMachPort::makeSendRightForTask(
				into_task, map, IKOT_IOKIT_OBJECT );
			map->release();
		} else {
			// keep it with the user client
			IOLockLock( gIOObjectPortLock);
			if (NULL == client->mappings) {
				client->mappings = OSSet::withCapacity(2);
			}
			if (client->mappings) {
				client->mappings->setObject( map);
			}
			IOLockUnlock( gIOObjectPortLock);
			map->release();
		}
		err = kIOReturnSuccess;
	} else {
		err = kIOReturnBadArgument;
	}

	client->ipcExit(client->defaultLocking ? kIPCLockWrite : kIPCLockNone);

	return err;
}

/* Routine is_io_connect_map_memory */
kern_return_t
is_io_connect_map_memory(
	io_object_t     connect,
	uint32_t        type,
	task_t          task,
	uint32_t  *     mapAddr,
	uint32_t  *     mapSize,
	uint32_t        flags )
{
	IOReturn          err;
	mach_vm_address_t address;
	mach_vm_size_t    size;

	address = SCALAR64(*mapAddr);
	size    = SCALAR64(*mapSize);

	err = is_io_connect_map_memory_into_task(connect, type, task, &address, &size, flags);

	*mapAddr = SCALAR32(address);
	*mapSize = SCALAR32(size);

	return err;
}
} /* extern "C" */

IOMemoryMap *
IOUserClient::removeMappingForDescriptor(IOMemoryDescriptor * mem)
{
	OSIterator *  iter;
	IOMemoryMap * map = NULL;

	IOLockLock(gIOObjectPortLock);

	iter = OSCollectionIterator::withCollection(mappings);
	if (iter) {
		while ((map = OSDynamicCast(IOMemoryMap, iter->getNextObject()))) {
			if (mem == map->getMemoryDescriptor()) {
				map->retain();
				mappings->removeObject(map);
				break;
			}
		}
		iter->release();
	}

	IOLockUnlock(gIOObjectPortLock);

	return map;
}

extern "C" {
/* Routine io_connect_unmap_memory_from_task */
kern_return_t
is_io_connect_unmap_memory_from_task
(
	io_connect_t connection,
	uint32_t memory_type,
	task_t from_task,
	mach_vm_address_t address)
{
	IOReturn            err;
	IOOptionBits        options = 0;
	IOMemoryDescriptor * memory = NULL;
	IOMemoryMap *       map;

	CHECK( IOUserClient, connection, client );

	if (!from_task) {
		return kIOReturnBadArgument;
	}

	IOStatisticsClientCall();

	client->ipcEnter(client->defaultLocking ? kIPCLockWrite : kIPCLockNone);
	err = client->clientMemoryForType((UInt32) memory_type, &options, &memory );

	if (memory && (kIOReturnSuccess == err)) {
		options = (options & ~kIOMapUserOptionsMask)
		    | kIOMapAnywhere | kIOMapReference;

		map = memory->createMappingInTask( from_task, address, options );
		memory->release();
		if (map) {
			IOLockLock( gIOObjectPortLock);
			if (client->mappings) {
				client->mappings->removeObject( map);
			}
			IOLockUnlock( gIOObjectPortLock);

			mach_port_name_t name = 0;
			bool is_shared_instance_or_from_current_task = from_task != current_task() || client->sharedInstance;
			if (is_shared_instance_or_from_current_task) {
				name = IOMachPort::makeSendRightForTask( from_task, map, IKOT_IOKIT_OBJECT );
				map->release();
			}

			if (name) {
				map->userClientUnmap();
				err = iokit_mod_send_right( from_task, name, -2 );
				err = kIOReturnSuccess;
			} else {
				IOMachPort::releasePortForObject( map, IKOT_IOKIT_OBJECT );
			}
			if (!is_shared_instance_or_from_current_task) {
				map->release();
			}
		} else {
			err = kIOReturnBadArgument;
		}
	}

	client->ipcExit(client->defaultLocking ? kIPCLockWrite : kIPCLockNone);

	return err;
}

kern_return_t
is_io_connect_unmap_memory(
	io_object_t     connect,
	uint32_t        type,
	task_t          task,
	uint32_t        mapAddr )
{
	IOReturn            err;
	mach_vm_address_t   address;

	address = SCALAR64(mapAddr);

	err = is_io_connect_unmap_memory_from_task(connect, type, task, mapAddr);

	return err;
}


/* Routine io_connect_add_client */
kern_return_t
is_io_connect_add_client(
	io_connect_t connection,
	io_object_t connect_to)
{
	CHECK( IOUserClient, connection, client );
	CHECK( IOUserClient, connect_to, to );

	IOReturn ret;

	IOStatisticsClientCall();

	client->ipcEnter(client->defaultLocking ? kIPCLockWrite : kIPCLockNone);
	ret = client->connectClient( to );
	client->ipcExit(client->defaultLocking ? kIPCLockWrite : kIPCLockNone);

	return ret;
}


/* Routine io_connect_set_properties */
kern_return_t
is_io_connect_set_properties(
	io_connect_t connection,
	io_buf_ptr_t properties,
	mach_msg_type_number_t propertiesCnt,
	kern_return_t * result)
{
	return is_io_registry_entry_set_properties( connection, properties, propertiesCnt, result );
}

/* Routine io_user_client_method */
kern_return_t
is_io_connect_method_var_output
(
	io_connect_t connection,
	uint32_t selector,
	io_scalar_inband64_t scalar_input,
	mach_msg_type_number_t scalar_inputCnt,
	io_struct_inband_t inband_input,
	mach_msg_type_number_t inband_inputCnt,
	mach_vm_address_t ool_input,
	mach_vm_size_t ool_input_size,
	io_struct_inband_t inband_output,
	mach_msg_type_number_t *inband_outputCnt,
	io_scalar_inband64_t scalar_output,
	mach_msg_type_number_t *scalar_outputCnt,
	io_buf_ptr_t *var_output,
	mach_msg_type_number_t *var_outputCnt
)
{
	CHECK( IOUserClient, connection, client );

	IOExternalMethodArguments args;
	IOReturn ret;
	IOMemoryDescriptor * inputMD  = NULL;
	OSObject *           structureVariableOutputData = NULL;

	bzero(&args.__reserved[0], sizeof(args.__reserved));
	args.__reservedA = 0;
	args.version = kIOExternalMethodArgumentsCurrentVersion;

	args.selector = selector;

	args.asyncWakePort               = MACH_PORT_NULL;
	args.asyncReference              = NULL;
	args.asyncReferenceCount         = 0;
	args.structureVariableOutputData = &structureVariableOutputData;

	args.scalarInput = scalar_input;
	args.scalarInputCount = scalar_inputCnt;
	args.structureInput = inband_input;
	args.structureInputSize = inband_inputCnt;

	if (ool_input && (ool_input_size <= sizeof(io_struct_inband_t))) {
		return kIOReturnIPCError;
	}

	if (ool_input) {
		inputMD = IOMemoryDescriptor::withAddressRange(ool_input, ool_input_size,
		    kIODirectionOut | kIOMemoryMapCopyOnWrite,
		    current_task());
	}

	args.structureInputDescriptor = inputMD;

	args.scalarOutput = scalar_output;
	args.scalarOutputCount = *scalar_outputCnt;
	bzero(&scalar_output[0], *scalar_outputCnt * sizeof(scalar_output[0]));
	args.structureOutput = inband_output;
	args.structureOutputSize = *inband_outputCnt;
	args.structureOutputDescriptor = NULL;
	args.structureOutputDescriptorSize = 0;

	IOStatisticsClientCall();
	ret = kIOReturnSuccess;

	io_filter_policy_t filterPolicy = client->filterForTask(current_task(), 0);
	if (filterPolicy && gIOUCFilterCallbacks->io_filter_applier) {
		ret = gIOUCFilterCallbacks->io_filter_applier(client, filterPolicy, io_filter_type_external_method, selector);
	}

	if (kIOReturnSuccess == ret) {
		ret = client->callExternalMethod(selector, &args);
	}

	*scalar_outputCnt = args.scalarOutputCount;
	*inband_outputCnt = args.structureOutputSize;

	if (var_outputCnt && var_output && (kIOReturnSuccess == ret)) {
		OSSerialize * serialize;
		OSData      * data;
		unsigned int  len;

		if ((serialize = OSDynamicCast(OSSerialize, structureVariableOutputData))) {
			len = serialize->getLength();
			*var_outputCnt = len;
			ret = copyoutkdata(serialize->text(), len, var_output);
		} else if ((data = OSDynamicCast(OSData, structureVariableOutputData))) {
			data->clipForCopyout();
			len = data->getLength();
			*var_outputCnt = len;
			ret = copyoutkdata(data->getBytesNoCopy(), len, var_output);
		} else {
			ret = kIOReturnUnderrun;
		}
	}

	if (inputMD) {
		inputMD->release();
	}
	if (structureVariableOutputData) {
		structureVariableOutputData->release();
	}

	return ret;
}

/* Routine io_user_client_method */
kern_return_t
is_io_connect_method
(
	io_connect_t connection,
	uint32_t selector,
	io_scalar_inband64_t scalar_input,
	mach_msg_type_number_t scalar_inputCnt,
	io_struct_inband_t inband_input,
	mach_msg_type_number_t inband_inputCnt,
	mach_vm_address_t ool_input,
	mach_vm_size_t ool_input_size,
	io_struct_inband_t inband_output,
	mach_msg_type_number_t *inband_outputCnt,
	io_scalar_inband64_t scalar_output,
	mach_msg_type_number_t *scalar_outputCnt,
	mach_vm_address_t ool_output,
	mach_vm_size_t *ool_output_size
)
{
	CHECK( IOUserClient, connection, client );

	IOExternalMethodArguments args;
	IOReturn ret;
	IOMemoryDescriptor * inputMD  = NULL;
	IOMemoryDescriptor * outputMD = NULL;

	bzero(&args.__reserved[0], sizeof(args.__reserved));
	args.__reservedA = 0;
	args.version = kIOExternalMethodArgumentsCurrentVersion;

	args.selector = selector;

	args.asyncWakePort               = MACH_PORT_NULL;
	args.asyncReference              = NULL;
	args.asyncReferenceCount         = 0;
	args.structureVariableOutputData = NULL;

	args.scalarInput = scalar_input;
	args.scalarInputCount = scalar_inputCnt;
	args.structureInput = inband_input;
	args.structureInputSize = inband_inputCnt;

	if (ool_input && (ool_input_size <= sizeof(io_struct_inband_t))) {
		return kIOReturnIPCError;
	}
	if (ool_output) {
		if (*ool_output_size <= sizeof(io_struct_inband_t)) {
			return kIOReturnIPCError;
		}
		if (*ool_output_size > UINT_MAX) {
			return kIOReturnIPCError;
		}
	}

	if (ool_input) {
		inputMD = IOMemoryDescriptor::withAddressRange(ool_input, ool_input_size,
		    kIODirectionOut | kIOMemoryMapCopyOnWrite,
		    current_task());
	}

	args.structureInputDescriptor = inputMD;

	args.scalarOutput = scalar_output;
	args.scalarOutputCount = *scalar_outputCnt;
	bzero(&scalar_output[0], *scalar_outputCnt * sizeof(scalar_output[0]));
	args.structureOutput = inband_output;
	args.structureOutputSize = *inband_outputCnt;

	if (ool_output && ool_output_size) {
		outputMD = IOMemoryDescriptor::withAddressRange(ool_output, *ool_output_size,
		    kIODirectionIn, current_task());
	}

	args.structureOutputDescriptor = outputMD;
	args.structureOutputDescriptorSize = ool_output_size
	    ? ((typeof(args.structureOutputDescriptorSize)) * ool_output_size)
	    : 0;

	IOStatisticsClientCall();
	ret = kIOReturnSuccess;
	io_filter_policy_t filterPolicy = client->filterForTask(current_task(), 0);
	if (filterPolicy && gIOUCFilterCallbacks->io_filter_applier) {
		ret = gIOUCFilterCallbacks->io_filter_applier(client, filterPolicy, io_filter_type_external_method, selector);
	}
	if (kIOReturnSuccess == ret) {
		ret = client->callExternalMethod( selector, &args );
	}

	*scalar_outputCnt = args.scalarOutputCount;
	*inband_outputCnt = args.structureOutputSize;
	*ool_output_size  = args.structureOutputDescriptorSize;

	if (inputMD) {
		inputMD->release();
	}
	if (outputMD) {
		outputMD->release();
	}

	return ret;
}

/* Routine io_async_user_client_method */
kern_return_t
is_io_connect_async_method
(
	io_connect_t connection,
	mach_port_t wake_port,
	io_async_ref64_t reference,
	mach_msg_type_number_t referenceCnt,
	uint32_t selector,
	io_scalar_inband64_t scalar_input,
	mach_msg_type_number_t scalar_inputCnt,
	io_struct_inband_t inband_input,
	mach_msg_type_number_t inband_inputCnt,
	mach_vm_address_t ool_input,
	mach_vm_size_t ool_input_size,
	io_struct_inband_t inband_output,
	mach_msg_type_number_t *inband_outputCnt,
	io_scalar_inband64_t scalar_output,
	mach_msg_type_number_t *scalar_outputCnt,
	mach_vm_address_t ool_output,
	mach_vm_size_t * ool_output_size
)
{
	CHECK( IOUserClient, connection, client );

	IOExternalMethodArguments args;
	IOReturn ret;
	IOMemoryDescriptor * inputMD  = NULL;
	IOMemoryDescriptor * outputMD = NULL;

	if (referenceCnt < 1) {
		return kIOReturnBadArgument;
	}

	bzero(&args.__reserved[0], sizeof(args.__reserved));
	args.__reservedA = 0;
	args.version = kIOExternalMethodArgumentsCurrentVersion;

	reference[0]             = (io_user_reference_t) wake_port;
	if (vm_map_is_64bit(get_task_map(current_task()))) {
		reference[0]         |= kIOUCAsync64Flag;
	}

	args.selector = selector;

	args.asyncWakePort       = wake_port;
	args.asyncReference      = reference;
	args.asyncReferenceCount = referenceCnt;

	args.structureVariableOutputData = NULL;

	args.scalarInput = scalar_input;
	args.scalarInputCount = scalar_inputCnt;
	args.structureInput = inband_input;
	args.structureInputSize = inband_inputCnt;

	if (ool_input && (ool_input_size <= sizeof(io_struct_inband_t))) {
		return kIOReturnIPCError;
	}
	if (ool_output) {
		if (*ool_output_size <= sizeof(io_struct_inband_t)) {
			return kIOReturnIPCError;
		}
		if (*ool_output_size > UINT_MAX) {
			return kIOReturnIPCError;
		}
	}

	if (ool_input) {
		inputMD = IOMemoryDescriptor::withAddressRange(ool_input, ool_input_size,
		    kIODirectionOut | kIOMemoryMapCopyOnWrite,
		    current_task());
	}

	args.structureInputDescriptor = inputMD;

	args.scalarOutput = scalar_output;
	args.scalarOutputCount = *scalar_outputCnt;
	bzero(&scalar_output[0], *scalar_outputCnt * sizeof(scalar_output[0]));
	args.structureOutput = inband_output;
	args.structureOutputSize = *inband_outputCnt;

	if (ool_output) {
		outputMD = IOMemoryDescriptor::withAddressRange(ool_output, *ool_output_size,
		    kIODirectionIn, current_task());
	}

	args.structureOutputDescriptor = outputMD;
	args.structureOutputDescriptorSize = ((typeof(args.structureOutputDescriptorSize)) * ool_output_size);

	IOStatisticsClientCall();
	ret = kIOReturnSuccess;
	io_filter_policy_t filterPolicy = client->filterForTask(current_task(), 0);
	if (filterPolicy && gIOUCFilterCallbacks->io_filter_applier) {
		ret = gIOUCFilterCallbacks->io_filter_applier(client, filterPolicy, io_filter_type_external_async_method, selector);
	}
	if (kIOReturnSuccess == ret) {
		ret = client->callExternalMethod( selector, &args );
	}

	*scalar_outputCnt = args.scalarOutputCount;
	*inband_outputCnt = args.structureOutputSize;
	*ool_output_size  = args.structureOutputDescriptorSize;

	if (inputMD) {
		inputMD->release();
	}
	if (outputMD) {
		outputMD->release();
	}

	return ret;
}

/* Routine io_connect_method_scalarI_scalarO */
kern_return_t
is_io_connect_method_scalarI_scalarO(
	io_object_t        connect,
	uint32_t           index,
	io_scalar_inband_t       input,
	mach_msg_type_number_t   inputCount,
	io_scalar_inband_t       output,
	mach_msg_type_number_t * outputCount )
{
	IOReturn err;
	uint32_t i;
	io_scalar_inband64_t _input;
	io_scalar_inband64_t _output;

	mach_msg_type_number_t struct_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	bzero(&_output[0], sizeof(_output));
	for (i = 0; i < inputCount; i++) {
		_input[i] = SCALAR64(input[i]);
	}

	err = is_io_connect_method(connect, index,
	    _input, inputCount,
	    NULL, 0,
	    0, 0,
	    NULL, &struct_outputCnt,
	    _output, outputCount,
	    0, &ool_output_size);

	for (i = 0; i < *outputCount; i++) {
		output[i] = SCALAR32(_output[i]);
	}

	return err;
}

kern_return_t
shim_io_connect_method_scalarI_scalarO(
	IOExternalMethod *      method,
	IOService *             object,
	const io_user_scalar_t * input,
	mach_msg_type_number_t   inputCount,
	io_user_scalar_t * output,
	mach_msg_type_number_t * outputCount )
{
	IOMethod            func;
	io_scalar_inband_t  _output;
	IOReturn            err;
	err = kIOReturnBadArgument;

	bzero(&_output[0], sizeof(_output));
	do {
		if (inputCount != method->count0) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if (*outputCount != method->count1) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)*outputCount, (uint64_t)method->count1);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)*outputCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		switch (inputCount) {
		case 6:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]), ARG32(input[5]));
			break;
		case 5:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]),
			    &_output[0] );
			break;
		case 4:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]),
			    &_output[0], &_output[1] );
			break;
		case 3:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    &_output[0], &_output[1], &_output[2] );
			break;
		case 2:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]),
			    &_output[0], &_output[1], &_output[2],
			    &_output[3] );
			break;
		case 1:
			err = (object->*func)(  ARG32(input[0]),
			    &_output[0], &_output[1], &_output[2],
			    &_output[3], &_output[4] );
			break;
		case 0:
			err = (object->*func)(  &_output[0], &_output[1], &_output[2],
			    &_output[3], &_output[4], &_output[5] );
			break;

		default:
			IOLog("%s: Bad method table\n", object->getName());
		}
	}while (false);

	uint32_t i;
	for (i = 0; i < *outputCount; i++) {
		output[i] = SCALAR32(_output[i]);
	}

	return err;
}

/* Routine io_async_method_scalarI_scalarO */
kern_return_t
is_io_async_method_scalarI_scalarO(
	io_object_t        connect,
	mach_port_t wake_port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	uint32_t           index,
	io_scalar_inband_t       input,
	mach_msg_type_number_t   inputCount,
	io_scalar_inband_t       output,
	mach_msg_type_number_t * outputCount )
{
	IOReturn err;
	uint32_t i;
	io_scalar_inband64_t _input;
	io_scalar_inband64_t _output;
	io_async_ref64_t _reference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	bzero(&_output[0], sizeof(_output));
	for (i = 0; i < referenceCnt; i++) {
		_reference[i] = REF64(reference[i]);
	}
	bzero(&_reference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(_reference[0]));

	mach_msg_type_number_t struct_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	for (i = 0; i < inputCount; i++) {
		_input[i] = SCALAR64(input[i]);
	}

	err = is_io_connect_async_method(connect,
	    wake_port, _reference, referenceCnt,
	    index,
	    _input, inputCount,
	    NULL, 0,
	    0, 0,
	    NULL, &struct_outputCnt,
	    _output, outputCount,
	    0, &ool_output_size);

	for (i = 0; i < *outputCount; i++) {
		output[i] = SCALAR32(_output[i]);
	}

	return err;
}
/* Routine io_async_method_scalarI_structureO */
kern_return_t
is_io_async_method_scalarI_structureO(
	io_object_t     connect,
	mach_port_t wake_port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	uint32_t        index,
	io_scalar_inband_t input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	mach_msg_type_number_t *        outputCount )
{
	uint32_t i;
	io_scalar_inband64_t _input;
	io_async_ref64_t _reference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	for (i = 0; i < referenceCnt; i++) {
		_reference[i] = REF64(reference[i]);
	}
	bzero(&_reference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(_reference[0]));

	mach_msg_type_number_t scalar_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	for (i = 0; i < inputCount; i++) {
		_input[i] = SCALAR64(input[i]);
	}

	return is_io_connect_async_method(connect,
	           wake_port, _reference, referenceCnt,
	           index,
	           _input, inputCount,
	           NULL, 0,
	           0, 0,
	           output, outputCount,
	           NULL, &scalar_outputCnt,
	           0, &ool_output_size);
}

/* Routine io_async_method_scalarI_structureI */
kern_return_t
is_io_async_method_scalarI_structureI(
	io_connect_t            connect,
	mach_port_t wake_port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	uint32_t                index,
	io_scalar_inband_t      input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t      inputStruct,
	mach_msg_type_number_t  inputStructCount )
{
	uint32_t i;
	io_scalar_inband64_t _input;
	io_async_ref64_t _reference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	for (i = 0; i < referenceCnt; i++) {
		_reference[i] = REF64(reference[i]);
	}
	bzero(&_reference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(_reference[0]));

	mach_msg_type_number_t scalar_outputCnt = 0;
	mach_msg_type_number_t inband_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	for (i = 0; i < inputCount; i++) {
		_input[i] = SCALAR64(input[i]);
	}

	return is_io_connect_async_method(connect,
	           wake_port, _reference, referenceCnt,
	           index,
	           _input, inputCount,
	           inputStruct, inputStructCount,
	           0, 0,
	           NULL, &inband_outputCnt,
	           NULL, &scalar_outputCnt,
	           0, &ool_output_size);
}

/* Routine io_async_method_structureI_structureO */
kern_return_t
is_io_async_method_structureI_structureO(
	io_object_t     connect,
	mach_port_t wake_port,
	io_async_ref_t reference,
	mach_msg_type_number_t referenceCnt,
	uint32_t        index,
	io_struct_inband_t              input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	mach_msg_type_number_t *        outputCount )
{
	uint32_t i;
	mach_msg_type_number_t scalar_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;
	io_async_ref64_t _reference;

	if (referenceCnt > ASYNC_REF64_COUNT) {
		return kIOReturnBadArgument;
	}
	for (i = 0; i < referenceCnt; i++) {
		_reference[i] = REF64(reference[i]);
	}
	bzero(&_reference[referenceCnt], (ASYNC_REF64_COUNT - referenceCnt) * sizeof(_reference[0]));

	return is_io_connect_async_method(connect,
	           wake_port, _reference, referenceCnt,
	           index,
	           NULL, 0,
	           input, inputCount,
	           0, 0,
	           output, outputCount,
	           NULL, &scalar_outputCnt,
	           0, &ool_output_size);
}


kern_return_t
shim_io_async_method_scalarI_scalarO(
	IOExternalAsyncMethod * method,
	IOService *             object,
	mach_port_t             asyncWakePort,
	io_user_reference_t *   asyncReference,
	uint32_t                asyncReferenceCount,
	const io_user_scalar_t * input,
	mach_msg_type_number_t   inputCount,
	io_user_scalar_t * output,
	mach_msg_type_number_t * outputCount )
{
	IOAsyncMethod       func;
	uint32_t            i;
	io_scalar_inband_t  _output;
	IOReturn            err;
	io_async_ref_t      reference;

	bzero(&_output[0], sizeof(_output));
	for (i = 0; i < asyncReferenceCount; i++) {
		reference[i] = REF32(asyncReference[i]);
	}

	err = kIOReturnBadArgument;

	do {
		if (inputCount != method->count0) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if (*outputCount != method->count1) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)*outputCount, (uint64_t)method->count1);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)*outputCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		switch (inputCount) {
		case 6:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]), ARG32(input[5]));
			break;
		case 5:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]),
			    &_output[0] );
			break;
		case 4:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]),
			    &_output[0], &_output[1] );
			break;
		case 3:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    &_output[0], &_output[1], &_output[2] );
			break;
		case 2:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]),
			    &_output[0], &_output[1], &_output[2],
			    &_output[3] );
			break;
		case 1:
			err = (object->*func)(  reference,
			    ARG32(input[0]),
			    &_output[0], &_output[1], &_output[2],
			    &_output[3], &_output[4] );
			break;
		case 0:
			err = (object->*func)(  reference,
			    &_output[0], &_output[1], &_output[2],
			    &_output[3], &_output[4], &_output[5] );
			break;

		default:
			IOLog("%s: Bad method table\n", object->getName());
		}
	}while (false);

	for (i = 0; i < *outputCount; i++) {
		output[i] = SCALAR32(_output[i]);
	}

	return err;
}


/* Routine io_connect_method_scalarI_structureO */
kern_return_t
is_io_connect_method_scalarI_structureO(
	io_object_t     connect,
	uint32_t        index,
	io_scalar_inband_t input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	mach_msg_type_number_t *        outputCount )
{
	uint32_t i;
	io_scalar_inband64_t _input;

	mach_msg_type_number_t scalar_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	for (i = 0; i < inputCount; i++) {
		_input[i] = SCALAR64(input[i]);
	}

	return is_io_connect_method(connect, index,
	           _input, inputCount,
	           NULL, 0,
	           0, 0,
	           output, outputCount,
	           NULL, &scalar_outputCnt,
	           0, &ool_output_size);
}

kern_return_t
shim_io_connect_method_scalarI_structureO(

	IOExternalMethod *      method,
	IOService *             object,
	const io_user_scalar_t * input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	IOByteCount *   outputCount )
{
	IOMethod            func;
	IOReturn            err;

	err = kIOReturnBadArgument;

	do {
		if (inputCount != method->count0) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if ((kIOUCVariableStructureSize != method->count1)
		    && (*outputCount != method->count1)) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)*outputCount, (uint64_t)method->count1, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)*outputCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		switch (inputCount) {
		case 5:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]),
			    output );
			break;
		case 4:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]),
			    output, (void *)outputCount );
			break;
		case 3:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    output, (void *)outputCount, NULL );
			break;
		case 2:
			err = (object->*func)(  ARG32(input[0]), ARG32(input[1]),
			    output, (void *)outputCount, NULL, NULL );
			break;
		case 1:
			err = (object->*func)(  ARG32(input[0]),
			    output, (void *)outputCount, NULL, NULL, NULL );
			break;
		case 0:
			err = (object->*func)(  output, (void *)outputCount, NULL, NULL, NULL, NULL );
			break;

		default:
			IOLog("%s: Bad method table\n", object->getName());
		}
	}while (false);

	return err;
}


kern_return_t
shim_io_async_method_scalarI_structureO(
	IOExternalAsyncMethod * method,
	IOService *             object,
	mach_port_t             asyncWakePort,
	io_user_reference_t *   asyncReference,
	uint32_t                asyncReferenceCount,
	const io_user_scalar_t * input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	mach_msg_type_number_t *        outputCount )
{
	IOAsyncMethod       func;
	uint32_t            i;
	IOReturn            err;
	io_async_ref_t      reference;

	for (i = 0; i < asyncReferenceCount; i++) {
		reference[i] = REF32(asyncReference[i]);
	}

	err = kIOReturnBadArgument;
	do {
		if (inputCount != method->count0) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if ((kIOUCVariableStructureSize != method->count1)
		    && (*outputCount != method->count1)) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)*outputCount, (uint64_t)method->count1, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)*outputCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		switch (inputCount) {
		case 5:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]),
			    output );
			break;
		case 4:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]),
			    output, (void *)outputCount );
			break;
		case 3:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    output, (void *)outputCount, NULL );
			break;
		case 2:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]),
			    output, (void *)outputCount, NULL, NULL );
			break;
		case 1:
			err = (object->*func)(  reference,
			    ARG32(input[0]),
			    output, (void *)outputCount, NULL, NULL, NULL );
			break;
		case 0:
			err = (object->*func)(  reference,
			    output, (void *)outputCount, NULL, NULL, NULL, NULL );
			break;

		default:
			IOLog("%s: Bad method table\n", object->getName());
		}
	}while (false);

	return err;
}

/* Routine io_connect_method_scalarI_structureI */
kern_return_t
is_io_connect_method_scalarI_structureI(
	io_connect_t            connect,
	uint32_t                index,
	io_scalar_inband_t      input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t      inputStruct,
	mach_msg_type_number_t  inputStructCount )
{
	uint32_t i;
	io_scalar_inband64_t _input;

	mach_msg_type_number_t scalar_outputCnt = 0;
	mach_msg_type_number_t inband_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	for (i = 0; i < inputCount; i++) {
		_input[i] = SCALAR64(input[i]);
	}

	return is_io_connect_method(connect, index,
	           _input, inputCount,
	           inputStruct, inputStructCount,
	           0, 0,
	           NULL, &inband_outputCnt,
	           NULL, &scalar_outputCnt,
	           0, &ool_output_size);
}

kern_return_t
shim_io_connect_method_scalarI_structureI(
	IOExternalMethod *  method,
	IOService *         object,
	const io_user_scalar_t * input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              inputStruct,
	mach_msg_type_number_t  inputStructCount )
{
	IOMethod            func;
	IOReturn            err = kIOReturnBadArgument;

	do{
		if (inputCount != method->count0) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if ((kIOUCVariableStructureSize != method->count1)
		    && (inputStructCount != method->count1)) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputStructCount, (uint64_t)method->count1, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputStructCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		switch (inputCount) {
		case 5:
			err = (object->*func)( ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]),
			    inputStruct );
			break;
		case 4:
			err = (object->*func)( ARG32(input[0]), ARG32(input[1]), (void *)  input[2],
			    ARG32(input[3]),
			    inputStruct, (void *)(uintptr_t)inputStructCount );
			break;
		case 3:
			err = (object->*func)( ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL );
			break;
		case 2:
			err = (object->*func)( ARG32(input[0]), ARG32(input[1]),
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL, NULL );
			break;
		case 1:
			err = (object->*func)( ARG32(input[0]),
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL, NULL, NULL );
			break;
		case 0:
			err = (object->*func)( inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL, NULL, NULL, NULL );
			break;

		default:
			IOLog("%s: Bad method table\n", object->getName());
		}
	}while (false);

	return err;
}

kern_return_t
shim_io_async_method_scalarI_structureI(
	IOExternalAsyncMethod * method,
	IOService *             object,
	mach_port_t             asyncWakePort,
	io_user_reference_t *   asyncReference,
	uint32_t                asyncReferenceCount,
	const io_user_scalar_t * input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              inputStruct,
	mach_msg_type_number_t  inputStructCount )
{
	IOAsyncMethod       func;
	uint32_t            i;
	IOReturn            err = kIOReturnBadArgument;
	io_async_ref_t      reference;

	for (i = 0; i < asyncReferenceCount; i++) {
		reference[i] = REF32(asyncReference[i]);
	}

	do{
		if (inputCount != method->count0) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if ((kIOUCVariableStructureSize != method->count1)
		    && (inputStructCount != method->count1)) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputStructCount, (uint64_t)method->count1, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputStructCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		switch (inputCount) {
		case 5:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]), ARG32(input[4]),
			    inputStruct );
			break;
		case 4:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    ARG32(input[3]),
			    inputStruct, (void *)(uintptr_t)inputStructCount );
			break;
		case 3:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]), ARG32(input[2]),
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL );
			break;
		case 2:
			err = (object->*func)(  reference,
			    ARG32(input[0]), ARG32(input[1]),
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL, NULL );
			break;
		case 1:
			err = (object->*func)(  reference,
			    ARG32(input[0]),
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL, NULL, NULL );
			break;
		case 0:
			err = (object->*func)(  reference,
			    inputStruct, (void *)(uintptr_t)inputStructCount,
			    NULL, NULL, NULL, NULL );
			break;

		default:
			IOLog("%s: Bad method table\n", object->getName());
		}
	}while (false);

	return err;
}

/* Routine io_connect_method_structureI_structureO */
kern_return_t
is_io_connect_method_structureI_structureO(
	io_object_t     connect,
	uint32_t        index,
	io_struct_inband_t              input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	mach_msg_type_number_t *        outputCount )
{
	mach_msg_type_number_t scalar_outputCnt = 0;
	mach_vm_size_t ool_output_size = 0;

	return is_io_connect_method(connect, index,
	           NULL, 0,
	           input, inputCount,
	           0, 0,
	           output, outputCount,
	           NULL, &scalar_outputCnt,
	           0, &ool_output_size);
}

kern_return_t
shim_io_connect_method_structureI_structureO(
	IOExternalMethod *  method,
	IOService *         object,
	io_struct_inband_t              input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	IOByteCount *   outputCount )
{
	IOMethod            func;
	IOReturn            err = kIOReturnBadArgument;

	do{
		if ((kIOUCVariableStructureSize != method->count0)
		    && (inputCount != method->count0)) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if ((kIOUCVariableStructureSize != method->count1)
		    && (*outputCount != method->count1)) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)*outputCount, (uint64_t)method->count1, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)*outputCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		if (method->count1) {
			if (method->count0) {
				err = (object->*func)( input, output,
				    (void *)(uintptr_t)inputCount, outputCount, NULL, NULL );
			} else {
				err = (object->*func)( output, outputCount, NULL, NULL, NULL, NULL );
			}
		} else {
			err = (object->*func)( input, (void *)(uintptr_t)inputCount, NULL, NULL, NULL, NULL );
		}
	}while (false);


	return err;
}

kern_return_t
shim_io_async_method_structureI_structureO(
	IOExternalAsyncMethod * method,
	IOService *             object,
	mach_port_t           asyncWakePort,
	io_user_reference_t * asyncReference,
	uint32_t              asyncReferenceCount,
	io_struct_inband_t              input,
	mach_msg_type_number_t  inputCount,
	io_struct_inband_t              output,
	mach_msg_type_number_t *        outputCount )
{
	IOAsyncMethod       func;
	uint32_t            i;
	IOReturn            err;
	io_async_ref_t      reference;

	for (i = 0; i < asyncReferenceCount; i++) {
		reference[i] = REF32(asyncReference[i]);
	}

	err = kIOReturnBadArgument;
	do{
		if ((kIOUCVariableStructureSize != method->count0)
		    && (inputCount != method->count0)) {
			IOLog("%s:%d %s: IOUserClient inputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)inputCount, (uint64_t)method->count0, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)inputCount, uint64_t, (uint64_t)method->count0);
			continue;
		}
		if ((kIOUCVariableStructureSize != method->count1)
		    && (*outputCount != method->count1)) {
			IOLog("%s:%d %s: IOUserClient outputCount count mismatch 0x%llx 0x%llx 0x%llx\n", __FUNCTION__, __LINE__, object->getName(), (uint64_t)*outputCount, (uint64_t)method->count1, (uint64_t)kIOUCVariableStructureSize);
			DTRACE_IO2(iokit_count_mismatch, uint64_t, (uint64_t)*outputCount, uint64_t, (uint64_t)method->count1);
			continue;
		}

		func = method->func;

		if (method->count1) {
			if (method->count0) {
				err = (object->*func)( reference,
				    input, output,
				    (void *)(uintptr_t)inputCount, outputCount, NULL, NULL );
			} else {
				err = (object->*func)( reference,
				    output, outputCount, NULL, NULL, NULL, NULL );
			}
		} else {
			err = (object->*func)( reference,
			    input, (void *)(uintptr_t)inputCount, NULL, NULL, NULL, NULL );
		}
	}while (false);

	return err;
}

/* Routine io_catalog_send_data */
kern_return_t
is_io_catalog_send_data(
	mach_port_t             main_port,
	uint32_t                flag,
	io_buf_ptr_t            inData,
	mach_msg_type_number_t  inDataCount,
	kern_return_t *         result)
{
	// Allow sending catalog data if there is no kextd and the kernel is DEVELOPMENT || DEBUG
#if NO_KEXTD && !(DEVELOPMENT || DEBUG)
	return kIOReturnNotPrivileged;
#else /* NO_KEXTD && !(DEVELOPMENT || DEBUG) */
	OSObject * obj = NULL;
	vm_offset_t data;
	kern_return_t kr = kIOReturnError;

	//printf("io_catalog_send_data called. flag: %d\n", flag);

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	if ((flag != kIOCatalogRemoveKernelLinker__Removed &&
	    flag != kIOCatalogKextdActive &&
	    flag != kIOCatalogKextdFinishedLaunching) &&
	    (!inData || !inDataCount)) {
		return kIOReturnBadArgument;
	}

	if (!IOCurrentTaskHasEntitlement(kIOCatalogManagementEntitlement)) {
		OSString * taskName = IOCopyLogNameForPID(proc_selfpid());
		IOLog("IOCatalogueSendData(%s): Not entitled\n", taskName ? taskName->getCStringNoCopy() : "");
		OSSafeReleaseNULL(taskName);
		// For now, fake success to not break applications relying on this function succeeding.
		// See <rdar://problem/32554970> for more details.
		return kIOReturnSuccess;
	}

	if (inData) {
		vm_map_offset_t map_data;

		if (inDataCount > sizeof(io_struct_inband_t) * 1024) {
			return kIOReturnMessageTooLarge;
		}

		kr = vm_map_copyout( kernel_map, &map_data, (vm_map_copy_t)inData);
		data = CAST_DOWN(vm_offset_t, map_data);

		if (kr != KERN_SUCCESS) {
			return kr;
		}

		// must return success after vm_map_copyout() succeeds

		if (inDataCount) {
			obj = (OSObject *)OSUnserializeXML((const char *)data, inDataCount);
			vm_deallocate( kernel_map, data, inDataCount );
			if (!obj) {
				*result = kIOReturnNoMemory;
				return KERN_SUCCESS;
			}
		}
	}

	switch (flag) {
	case kIOCatalogResetDrivers:
	case kIOCatalogResetDriversNoMatch: {
		OSArray * array;

		array = OSDynamicCast(OSArray, obj);
		if (array) {
			if (!gIOCatalogue->resetAndAddDrivers(array,
			    flag == kIOCatalogResetDrivers)) {
				kr = kIOReturnError;
			}
		} else {
			kr = kIOReturnBadArgument;
		}
	}
	break;

	case kIOCatalogAddDrivers:
	case kIOCatalogAddDriversNoMatch: {
		OSArray * array;

		array = OSDynamicCast(OSArray, obj);
		if (array) {
			if (!gIOCatalogue->addDrivers( array,
			    flag == kIOCatalogAddDrivers)) {
				kr = kIOReturnError;
			}
		} else {
			kr = kIOReturnBadArgument;
		}
	}
	break;

	case kIOCatalogRemoveDrivers:
	case kIOCatalogRemoveDriversNoMatch: {
		OSDictionary * dict;

		dict = OSDynamicCast(OSDictionary, obj);
		if (dict) {
			if (!gIOCatalogue->removeDrivers( dict,
			    flag == kIOCatalogRemoveDrivers )) {
				kr = kIOReturnError;
			}
		} else {
			kr = kIOReturnBadArgument;
		}
	}
	break;

	case kIOCatalogStartMatching__Removed:
	case kIOCatalogRemoveKernelLinker__Removed:
	case kIOCatalogKextdActive:
	case kIOCatalogKextdFinishedLaunching:
		kr = KERN_NOT_SUPPORTED;
		break;

	default:
		kr = kIOReturnBadArgument;
		break;
	}

	if (obj) {
		obj->release();
	}

	*result = kr;
	return KERN_SUCCESS;
#endif /* NO_KEXTD && !(DEVELOPMENT || DEBUG) */
}

/* Routine io_catalog_terminate */
kern_return_t
is_io_catalog_terminate(
	mach_port_t main_port,
	uint32_t flag,
	io_name_t name )
{
	kern_return_t          kr;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	kr = IOUserClient::clientHasPrivilege((void *) current_task(),
	    kIOClientPrivilegeAdministrator );
	if (kIOReturnSuccess != kr) {
		return kr;
	}

	switch (flag) {
#if !defined(SECURE_KERNEL)
	case kIOCatalogServiceTerminate:
		kr = gIOCatalogue->terminateDrivers(NULL, name, false);
		break;

	case kIOCatalogModuleUnload:
	case kIOCatalogModuleTerminate:
		kr = gIOCatalogue->terminateDriversForModule(name,
		    flag == kIOCatalogModuleUnload);
		break;
#endif

	default:
		kr = kIOReturnBadArgument;
		break;
	}

	return kr;
}

/* Routine io_catalog_get_data */
kern_return_t
is_io_catalog_get_data(
	mach_port_t             main_port,
	uint32_t                flag,
	io_buf_ptr_t            *outData,
	mach_msg_type_number_t  *outDataCount)
{
	kern_return_t kr = kIOReturnSuccess;
	OSSerialize * s;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	//printf("io_catalog_get_data called. flag: %d\n", flag);

	s = OSSerialize::withCapacity(4096);
	if (!s) {
		return kIOReturnNoMemory;
	}

	kr = gIOCatalogue->serializeData(flag, s);

	if (kr == kIOReturnSuccess) {
		mach_vm_address_t data;
		vm_map_copy_t copy;
		unsigned int size;

		size = s->getLength();
		kr = mach_vm_allocate_kernel(kernel_map, &data, size,
		    VM_MAP_KERNEL_FLAGS_ANYWHERE(.vm_tag = VM_KERN_MEMORY_IOKIT));
		if (kr == kIOReturnSuccess) {
			bcopy(s->text(), (void *)data, size);
			kr = vm_map_copyin(kernel_map, data, size, true, &copy);
			*outData = (char *)copy;
			*outDataCount = size;
		}
	}

	s->release();

	return kr;
}

/* Routine io_catalog_get_gen_count */
kern_return_t
is_io_catalog_get_gen_count(
	mach_port_t             main_port,
	uint32_t                *genCount)
{
	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	//printf("io_catalog_get_gen_count called.\n");

	if (!genCount) {
		return kIOReturnBadArgument;
	}

	*genCount = gIOCatalogue->getGenerationCount();

	return kIOReturnSuccess;
}

/* Routine io_catalog_module_loaded.
 * Is invoked from IOKitLib's IOCatalogueModuleLoaded(). Doesn't seem to be used.
 */
kern_return_t
is_io_catalog_module_loaded(
	mach_port_t             main_port,
	io_name_t               name)
{
	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	//printf("io_catalog_module_loaded called. name %s\n", name);

	if (!name) {
		return kIOReturnBadArgument;
	}

	gIOCatalogue->moduleHasLoaded(name);

	return kIOReturnSuccess;
}

kern_return_t
is_io_catalog_reset(
	mach_port_t             main_port,
	uint32_t                flag)
{
	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	switch (flag) {
	case kIOCatalogResetDefault:
		gIOCatalogue->reset();
		break;

	default:
		return kIOReturnBadArgument;
	}

	return kIOReturnSuccess;
}

kern_return_t
iokit_user_client_trap(struct iokit_user_client_trap_args *args)
{
	kern_return_t    result = kIOReturnBadArgument;
	IOUserClient   * userClient;
	OSObject       * object;
	uintptr_t        ref;
	mach_port_name_t portName;

	ref     = (uintptr_t) args->userClientRef;

	if ((ref == MACH_PORT_DEAD) || (ref == (uintptr_t) MACH_PORT_NULL)) {
		return kIOReturnBadArgument;
	}
	// kobject port names always have b0-1 set, so we use these bits as flags to
	// iokit_user_client_trap()
	// keep this up to date with ipc_entry_name_mask();
	portName = (mach_port_name_t) (ref | 3);
	if (((1ULL << 32) & ref) || !(1 & ref)) {
		object = iokit_lookup_uext_ref_current_task(portName);
		if (object) {
			result = IOUserServerUEXTTrap(object, args->p1, args->p2, args->p3, args->p4, args->p5, args->p6);
		}
		OSSafeReleaseNULL(object);
	} else {
		io_object_t ref_current_task = iokit_lookup_connect_ref_current_task((mach_port_name_t) ref);
		if ((userClient = OSDynamicCast(IOUserClient, ref_current_task))) {
			IOExternalTrap *trap = NULL;
			IOService *target = NULL;

			result = kIOReturnSuccess;
			io_filter_policy_t filterPolicy = userClient->filterForTask(current_task(), 0);
			if (filterPolicy && gIOUCFilterCallbacks->io_filter_applier) {
				result = gIOUCFilterCallbacks->io_filter_applier(userClient, filterPolicy, io_filter_type_trap, args->index);
			}
			if (kIOReturnSuccess == result) {
				trap = userClient->getTargetAndTrapForIndex(&target, args->index);
			}
			if (trap && target) {
				IOTrap func;

				func = trap->func;

				if (func) {
					result = (target->*func)(args->p1, args->p2, args->p3, args->p4, args->p5, args->p6);
				}
			}

			iokit_remove_connect_reference(userClient);
		} else {
			OSSafeReleaseNULL(ref_current_task);
		}
	}

	return result;
}

/* Routine io_device_tree_entry_exists_with_name */
kern_return_t
is_io_device_tree_entry_exists_with_name(
	mach_port_t main_port,
	io_name_t name,
	boolean_t *exists )
{
	OSCollectionIterator *iter;

	if (main_port != main_device_port) {
		return kIOReturnNotPrivileged;
	}

	iter = IODTFindMatchingEntries(IORegistryEntry::getRegistryRoot(), kIODTRecursive, name);
	*exists = iter && iter->getNextObject();
	OSSafeReleaseNULL(iter);

	return kIOReturnSuccess;
}
} /* extern "C" */

IOReturn
IOUserClient::callExternalMethod(uint32_t selector, IOExternalMethodArguments * args)
{
	IOReturn ret;

	ipcEnter(defaultLocking ? (defaultLockingSingleThreadExternalMethod ? kIPCLockWrite : kIPCLockRead) : kIPCLockNone);
	if (uc2022) {
		ret = ((IOUserClient2022 *) this)->externalMethod(selector, (IOExternalMethodArgumentsOpaque *) args);
	} else {
		ret = externalMethod(selector, args);
	}
	ipcExit(defaultLocking ? (defaultLockingSingleThreadExternalMethod ? kIPCLockWrite : kIPCLockRead) : kIPCLockNone);

	return ret;
}

MIG_SERVER_ROUTINE IOReturn
IOUserClient2022::externalMethod(uint32_t selector, IOExternalMethodArguments * arguments,
    IOExternalMethodDispatch *dispatch,
    OSObject *target, void *reference)
{
	panic("wrong externalMethod for IOUserClient2022");
}

IOReturn
IOUserClient2022::dispatchExternalMethod(uint32_t selector, IOExternalMethodArgumentsOpaque *arguments,
    const IOExternalMethodDispatch2022 dispatchArray[], size_t dispatchArrayCount,
    OSObject * target, void * reference)
{
	IOReturn    err;
	IOExternalMethodArguments * args = (typeof(args))arguments;
	const IOExternalMethodDispatch2022 * dispatch;

	if (!dispatchArray) {
		return kIOReturnError;
	}
	if (selector >= dispatchArrayCount) {
		return kIOReturnBadArgument;
	}
	dispatch = &dispatchArray[selector];

	uint32_t count;
	count = dispatch->checkScalarInputCount;
	if ((kIOUCVariableStructureSize != count) && (count != args->scalarInputCount)) {
		return kIOReturnBadArgument;
	}

	count = dispatch->checkStructureInputSize;
	if ((kIOUCVariableStructureSize != count)
	    && (count != ((args->structureInputDescriptor)
	    ? args->structureInputDescriptor->getLength() : args->structureInputSize))) {
		return kIOReturnBadArgument;
	}

	count = dispatch->checkScalarOutputCount;
	if ((kIOUCVariableStructureSize != count) && (count != args->scalarOutputCount)) {
		return kIOReturnBadArgument;
	}

	count = dispatch->checkStructureOutputSize;
	if ((kIOUCVariableStructureSize != count)
	    && (count != ((args->structureOutputDescriptor)
	    ? args->structureOutputDescriptor->getLength() : args->structureOutputSize))) {
		return kIOReturnBadArgument;
	}

	if (args->asyncWakePort && !dispatch->allowAsync) {
		return kIOReturnBadArgument;
	}

	if (dispatch->checkEntitlement) {
		if (!IOCurrentTaskHasEntitlement(dispatch->checkEntitlement)) {
			return kIOReturnNotPrivileged;
		}
	}

	if (dispatch->function) {
		err = (*dispatch->function)(target, reference, args);
	} else {
		err = kIOReturnNoCompletion; /* implementer can dispatch */
	}
	return err;
}

IOReturn
IOUserClient::externalMethod( uint32_t selector, IOExternalMethodArguments * args,
    IOExternalMethodDispatch * dispatch, OSObject * target, void * reference )
{
	IOReturn    err;
	IOService * object;
	IOByteCount structureOutputSize;

	if (dispatch) {
		uint32_t count;
		count = dispatch->checkScalarInputCount;
		if ((kIOUCVariableStructureSize != count) && (count != args->scalarInputCount)) {
			return kIOReturnBadArgument;
		}

		count = dispatch->checkStructureInputSize;
		if ((kIOUCVariableStructureSize != count)
		    && (count != ((args->structureInputDescriptor)
		    ? args->structureInputDescriptor->getLength() : args->structureInputSize))) {
			return kIOReturnBadArgument;
		}

		count = dispatch->checkScalarOutputCount;
		if ((kIOUCVariableStructureSize != count) && (count != args->scalarOutputCount)) {
			return kIOReturnBadArgument;
		}

		count = dispatch->checkStructureOutputSize;
		if ((kIOUCVariableStructureSize != count)
		    && (count != ((args->structureOutputDescriptor)
		    ? args->structureOutputDescriptor->getLength() : args->structureOutputSize))) {
			return kIOReturnBadArgument;
		}

		if (dispatch->function) {
			err = (*dispatch->function)(target, reference, args);
		} else {
			err = kIOReturnNoCompletion; /* implementer can dispatch */
		}
		return err;
	}


	// pre-Leopard API's don't do ool structs
	if (args->structureInputDescriptor || args->structureOutputDescriptor) {
		err = kIOReturnIPCError;
		return err;
	}

	structureOutputSize = args->structureOutputSize;

	if (args->asyncWakePort) {
		IOExternalAsyncMethod * method;
		object = NULL;
		if (!(method = getAsyncTargetAndMethodForIndex(&object, selector)) || !object) {
			return kIOReturnUnsupported;
		}

		if (kIOUCForegroundOnly & method->flags) {
			if (task_is_gpu_denied(current_task())) {
				return kIOReturnNotPermitted;
			}
		}

		switch (method->flags & kIOUCTypeMask) {
		case kIOUCScalarIStructI:
			err = shim_io_async_method_scalarI_structureI( method, object,
			    args->asyncWakePort, args->asyncReference, args->asyncReferenceCount,
			    args->scalarInput, args->scalarInputCount,
			    (char *)args->structureInput, args->structureInputSize );
			break;

		case kIOUCScalarIScalarO:
			err = shim_io_async_method_scalarI_scalarO( method, object,
			    args->asyncWakePort, args->asyncReference, args->asyncReferenceCount,
			    args->scalarInput, args->scalarInputCount,
			    args->scalarOutput, &args->scalarOutputCount );
			break;

		case kIOUCScalarIStructO:
			err = shim_io_async_method_scalarI_structureO( method, object,
			    args->asyncWakePort, args->asyncReference, args->asyncReferenceCount,
			    args->scalarInput, args->scalarInputCount,
			    (char *) args->structureOutput, &args->structureOutputSize );
			break;


		case kIOUCStructIStructO:
			err = shim_io_async_method_structureI_structureO( method, object,
			    args->asyncWakePort, args->asyncReference, args->asyncReferenceCount,
			    (char *)args->structureInput, args->structureInputSize,
			    (char *) args->structureOutput, &args->structureOutputSize );
			break;

		default:
			err = kIOReturnBadArgument;
			break;
		}
	} else {
		IOExternalMethod *      method;
		object = NULL;
		if (!(method = getTargetAndMethodForIndex(&object, selector)) || !object) {
			return kIOReturnUnsupported;
		}

		if (kIOUCForegroundOnly & method->flags) {
			if (task_is_gpu_denied(current_task())) {
				return kIOReturnNotPermitted;
			}
		}

		switch (method->flags & kIOUCTypeMask) {
		case kIOUCScalarIStructI:
			err = shim_io_connect_method_scalarI_structureI( method, object,
			    args->scalarInput, args->scalarInputCount,
			    (char *) args->structureInput, args->structureInputSize );
			break;

		case kIOUCScalarIScalarO:
			err = shim_io_connect_method_scalarI_scalarO( method, object,
			    args->scalarInput, args->scalarInputCount,
			    args->scalarOutput, &args->scalarOutputCount );
			break;

		case kIOUCScalarIStructO:
			err = shim_io_connect_method_scalarI_structureO( method, object,
			    args->scalarInput, args->scalarInputCount,
			    (char *) args->structureOutput, &structureOutputSize );
			break;


		case kIOUCStructIStructO:
			err = shim_io_connect_method_structureI_structureO( method, object,
			    (char *) args->structureInput, args->structureInputSize,
			    (char *) args->structureOutput, &structureOutputSize );
			break;

		default:
			err = kIOReturnBadArgument;
			break;
		}
	}

	if (structureOutputSize > UINT_MAX) {
		structureOutputSize = 0;
		err = kIOReturnBadArgument;
	}

	args->structureOutputSize = ((typeof(args->structureOutputSize))structureOutputSize);

	return err;
}

IOReturn
IOUserClient::registerFilterCallbacks(const struct io_filter_callbacks *callbacks, size_t size)
{
	if (size < sizeof(*callbacks)) {
		return kIOReturnBadArgument;
	}
	if (!OSCompareAndSwapPtr(NULL, __DECONST(void *, callbacks), &gIOUCFilterCallbacks)) {
		return kIOReturnBusy;
	}
	return kIOReturnSuccess;
}


OSMetaClassDefineReservedUnused(IOUserClient, 0);
OSMetaClassDefineReservedUnused(IOUserClient, 1);
OSMetaClassDefineReservedUnused(IOUserClient, 2);
OSMetaClassDefineReservedUnused(IOUserClient, 3);
OSMetaClassDefineReservedUnused(IOUserClient, 4);
OSMetaClassDefineReservedUnused(IOUserClient, 5);
OSMetaClassDefineReservedUnused(IOUserClient, 6);
OSMetaClassDefineReservedUnused(IOUserClient, 7);
OSMetaClassDefineReservedUnused(IOUserClient, 8);
OSMetaClassDefineReservedUnused(IOUserClient, 9);
OSMetaClassDefineReservedUnused(IOUserClient, 10);
OSMetaClassDefineReservedUnused(IOUserClient, 11);
OSMetaClassDefineReservedUnused(IOUserClient, 12);
OSMetaClassDefineReservedUnused(IOUserClient, 13);
OSMetaClassDefineReservedUnused(IOUserClient, 14);
OSMetaClassDefineReservedUnused(IOUserClient, 15);

OSMetaClassDefineReservedUnused(IOUserClient2022, 0);
OSMetaClassDefineReservedUnused(IOUserClient2022, 1);
OSMetaClassDefineReservedUnused(IOUserClient2022, 2);
OSMetaClassDefineReservedUnused(IOUserClient2022, 3);
