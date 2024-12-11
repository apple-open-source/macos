/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#import <CoreFoundation/CFRuntime.h>
#import <Foundation/NSXPCConnection.h>
#import <TargetConditionals.h>
#import "cfutil.h"
#import "symbol_scope.h"
#import "ObjectWrapper.h"
#import "IPConfigurationLog.h"
#import "IPConfigurationPrivate.h"
#import "PvDInfoRequest.h"
#import "IPHPvDInfoRequest.h"

typedef struct {
	CFStringRef pvdid;
	CFArrayRef _Nullable ipv6_prefixes;
	const char * if_name;
	uint64_t ms_delay;
} PvDInfoRequestContext;

struct __PvDInfoRequest {
	CFRuntimeBase cf_base;
	
	ObjectWrapperRef wrapper;
	dispatch_queue_t queue;
	dispatch_source_t completion_source;
	bool active_connection;
	NSXPCConnection * xpc_connection;
	id<IPHPvDInfoRequestProtocol> xpc_proxy;
	dispatch_source_t xpc_source;
	PvDInfoRequestContext context;
	PvDInfoRequestState state;
	CFDictionaryRef additional_info;
};

#pragma mark -
#pragma mark CF object defs

STATIC CFTypeID __kPvDInfoRequestTypeID = _kCFRuntimeNotATypeID;
STATIC void __PvDInfoRequestDeallocate(CFTypeRef cf);

STATIC const CFRuntimeClass __PvDInfoRequestClass = {
	0,					/* version */
	"PvDInfoRequest",			/* className */
	NULL,					/* init */
	NULL,					/* copy */
	__PvDInfoRequestDeallocate,		/* deallocate */
	NULL,					/* equal */
	NULL,					/* hash */
	NULL,					/* copyFormattingDesc */
	NULL,					/* copyDebugDesc */
};

STATIC void
PvDInfoRequestFlushContext(PvDInfoRequestRef request);

STATIC bool
PvDInfoRequestInvalidateCompletionSource(PvDInfoRequestRef request);

STATIC bool
PvDInfoRequestInvalidateXPCSource(PvDInfoRequestRef request);

STATIC void
__PvDInfoRequestDeallocate(CFTypeRef cf)
{
	PvDInfoRequestRef request = (PvDInfoRequestRef)cf;

	PvDInfoRequestInvalidateCompletionSource(request);
	PvDInfoRequestInvalidateXPCSource(request);
	if (request->wrapper != NULL) {
		if (request->queue != NULL) {
			dispatch_sync(request->queue, ^{
				ObjectWrapperClearObject(request->wrapper);
			});
		}
		ObjectWrapperRelease(request->wrapper);
		request->wrapper = NULL;
	}
	if (request->xpc_proxy != nil) {
		[request->xpc_proxy cancelRequest];
		request->xpc_proxy = nil;
	}
	if (request->xpc_connection != nil) {
		[request->xpc_connection invalidate];
		request->xpc_connection = nil;
	}
	PvDInfoRequestFlushContext(request);
	request->queue = NULL;
	request = NULL;
	
	return;
}

STATIC void
__PvDInfoRequestInitialize(void)
{
	_IPConfigurationInitLog(kIPConfigurationLogCategoryLibrary);
	__kPvDInfoRequestTypeID 
	= _CFRuntimeRegisterClass(&__PvDInfoRequestClass);
	return;
}

STATIC void
__PvDInfoRequestRegisterClass(void)
{
	STATIC dispatch_once_t onceToken;
	
	dispatch_once(&onceToken, ^{
		__PvDInfoRequestInitialize();
	});
	
	return;
}

STATIC PvDInfoRequestRef
__PvDInfoRequestAllocate(CFAllocatorRef allocator)
{
	PvDInfoRequestRef request;
	int size;
	
	__PvDInfoRequestRegisterClass();
	size = sizeof(*request) - sizeof(CFRuntimeBase);
	request 
	= (PvDInfoRequestRef)_CFRuntimeCreateInstance(allocator,
						      __kPvDInfoRequestTypeID,
						      size, NULL);
	bzero(((void *)request) + sizeof(CFRuntimeBase), size);
	
	return (request);
}

#pragma mark -
#pragma mark Internal functions

STATIC void
PvDInfoRequestSetContext(PvDInfoRequestRef request, CFStringRef pvdid, 
			 CFArrayRef prefixes, const char * ifname,
			 uint64_t ms_delay)
{
	PvDInfoRequestContext context = { 0 };
	CFRetain(pvdid);
	if (prefixes != NULL) {
		CFRetain(prefixes);
	}
	context.pvdid = pvdid;
	context.ipv6_prefixes = prefixes;
	context.if_name = ifname;
	context.ms_delay = ms_delay;
	request->context = context;
}

/*
 * Function: PvDInfoRequestInvalidateCompletionSource
 * Purpose:
 *   Make sure the completion callback can't be called again,
 *   and release all resources.
 * Returns:
 *   true if the completion source was invalidated
 */
STATIC bool
PvDInfoRequestInvalidateCompletionSource(PvDInfoRequestRef request)
{
	bool invalidated = false;

	if (request->completion_source != NULL) {
		invalidated = true;
		dispatch_source_cancel(request->completion_source);
		request->completion_source = NULL;
		/* retained within PvDInfoRequestCreateCompletionSource() */
		ObjectWrapperRelease(request->wrapper);
	}

	return (invalidated);
}

STATIC dispatch_source_t
PvDInfoRequestCreateCompletionSource(dispatch_queue_t queue,
				     dispatch_block_t completion,
				     ObjectWrapperRef wrapper)
{
	dispatch_source_t completion_source = NULL;
	dispatch_block_t handler;

	completion_source
	= dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, queue);
	if (completion_source != NULL) {
		ObjectWrapperRetain(wrapper); // released in PvDInfoRequestInvalidateCompletionSource()
		handler = ^{
			PvDInfoRequestRef request;

			request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
			if (request != NULL
			    && PvDInfoRequestInvalidateCompletionSource(request)) {
				completion();
			}
		};
		dispatch_source_set_event_handler(completion_source, handler);
		dispatch_activate(completion_source);
	}

	return (completion_source);
}

STATIC void
PvDInfoRequestFlushContext(PvDInfoRequestRef request)
{
	my_CFRelease(&request->context.pvdid);
	my_CFRelease(&request->context.ipv6_prefixes);
	my_CFRelease(&request->additional_info);
	bzero(&request->context, sizeof(request->context));
}

STATIC void
PvDInfoRequestCleanupXPC(PvDInfoRequestRef request)
{
	PvDInfoRequestInvalidateXPCSource(request);
	if (request->xpc_proxy != nil) {
		[request->xpc_proxy cancelRequest];
		request->xpc_proxy = nil;
	}
	if (request->xpc_connection != nil) {
		[request->xpc_connection invalidate];
		request->xpc_connection = nil;
	}
	request->active_connection = false;

	return;
}

STATIC void
PvDInfoRequestSendNotificationToClient(PvDInfoRequestRef request)
{
	if (request->completion_source != NULL) {
		dispatch_source_merge_data(request->completion_source, 1);
	}
	return;
}

STATIC void
PvDInfoRequestCompletedCallback(PvDInfoRequestRef request,
				CFBooleanRef valid_fetch)
{
	ObjectWrapperRef wrapper = NULL;

	if (request->queue == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: null request", __func__);
		goto done;
	}
	wrapper = request->wrapper;
	ObjectWrapperRetain(wrapper);
	dispatch_async(request->queue, ^{
		PvDInfoRequestRef request = NULL;

		request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
		if (request == NULL) {
			IPConfigLog(LOG_NOTICE, "request no longer valid");
			goto done;
		}
		if (valid_fetch == kCFBooleanFalse) {
			IPConfigLog(LOG_DEBUG, "xpc reply: failure");
			request->state = kPvDInfoRequestStateFailed;
		} else if (request->additional_info == NULL) {
			IPConfigLog(LOG_DEBUG, "xpc reply: no internet");
			request->state = kPvDInfoRequestStateIdle;
		} else if (request->additional_info != NULL) {
			/* SUCCESS */
			IPConfigLog(LOG_DEBUG,
				    "xpc reply: got addinfo dict:\n%@",
				    request->additional_info);
			request->state = kPvDInfoRequestStateObtained;
		}
		PvDInfoRequestCleanupXPC(request);
		PvDInfoRequestSendNotificationToClient(request);
	done:
		ObjectWrapperRelease(wrapper);
		return;
	});
	
done:
	return;
}

STATIC void
PvDInfoRequestXPCCompletionHandler(NSDictionary * xpc_return_dict, void * info)
{
	NSDictionary *addinfo_dict = nil;
	CFBooleanRef valid_fetch = NULL;
	PvDInfoRequestRef request = NULL;
	ObjectWrapperRef wrapper = (ObjectWrapperRef)info;

	request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
	if (request == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: null object", __func__);
		goto done;
	}
	CFRetain(request);
	valid_fetch
	= (__bridge CFBooleanRef)[xpc_return_dict
				  valueForKey:(__bridge NSString *)kPvDInfoValidFetchXPCKey];
	addinfo_dict
	= [xpc_return_dict
	   valueForKey:(__bridge NSString *)kPvDInfoAdditionalInfoDictXPCKey];
	if (valid_fetch == kCFBooleanTrue && addinfo_dict != nil) {
		request->additional_info
		= (__bridge_retained CFDictionaryRef)addinfo_dict;
	}
	PvDInfoRequestCompletedCallback(request, valid_fetch);
	
done:
	my_CFRelease(&request);
	return;
}

STATIC void
PvDInfoRequestSendXPCRequest(PvDInfoRequestRef request)
{
	ObjectWrapperRef wrapper = NULL;
	NSString *pvdid = nil;
	NSArray<NSString *> * prefixes = nil;
	NSString *if_name_str = nil;

	if (request == NULL) {
		IPConfigLog(LOG_NOTICE, "can't send xpc, null object");
		goto done;
	}
	pvdid = (__bridge NSString *)request->context.pvdid;
	prefixes = (__bridge NSArray<NSString *> *)request->context.ipv6_prefixes;
	if_name_str = [NSString stringWithCString:request->context.if_name
					 encoding:NSUTF8StringEncoding];
	if (if_name_str == nil) {
		IPConfigLog(LOG_ERR, "couldn't create ifname '%s'", 
			request->context.if_name);
		goto done;
	}
	/* actual xpc request */
	wrapper = request->wrapper;
	ObjectWrapperRetain(wrapper);
	[request->xpc_proxy fetchPvDAdditionalInformationWithPvDID:pvdid
						     prefixesArray:prefixes
						   bindToInterface:if_name_str
					      andCompletionHandler:
	 ^(NSDictionary * xpc_return_dict) {
		/* the XPC API context can later safely reference the wrapper */
		PvDInfoRequestXPCCompletionHandler(xpc_return_dict, wrapper);
		ObjectWrapperRelease(wrapper);
	}];
	request->state = kPvDInfoRequestStateScheduled;
	
done:
	if (request != NULL && request->state != kPvDInfoRequestStateScheduled) {
		IPConfigLog(LOG_ERR, "couldn't schedule fetch for pvdid '%@'",
			request->context.pvdid);
		PvDInfoRequestCleanupXPC(request);
		PvDInfoRequestFlushContext(request);
		request->state = kPvDInfoRequestStateIdle;
	}
	return;
}

STATIC bool
PvDInfoRequestInvalidateXPCSource(PvDInfoRequestRef request)
{
	bool invalidated = false;

	if (request->xpc_source != NULL) {
		invalidated = true;
		dispatch_source_cancel(request->xpc_source);
		request->xpc_source = NULL;
		/* retained within PvDInfoRequestCreateXPCSource() */
		ObjectWrapperRelease(request->wrapper);
	}

	return (invalidated);
}

STATIC dispatch_source_t
PvDInfoRequestCreateXPCSource(PvDInfoRequestRef request)
{
	ObjectWrapperRef wrapper = NULL;
	dispatch_source_t source = NULL;
	dispatch_block_t handler;
	dispatch_time_t when = 0;

	source
	= dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, request->queue);
	if (source == NULL) {
		IPConfigLog(LOG_ERR, "failed to create dispatch source for xpc");
		goto done;
	}
	wrapper = request->wrapper;
	ObjectWrapperRetain(wrapper); // released in PvDInfoRequestInvalidateXPCSource()
	handler = ^{
		PvDInfoRequestRef request = NULL;

		request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
		if (request != NULL
		    && PvDInfoRequestInvalidateXPCSource(request)) {
			PvDInfoRequestSendXPCRequest(request);
		}
	};
	dispatch_source_set_event_handler(source, handler);
	when = dispatch_time(DISPATCH_TIME_NOW,
			     (int64_t)(request->context.ms_delay * NSEC_PER_MSEC));
	dispatch_source_set_timer(source, when, DISPATCH_TIME_FOREVER, 0);
	dispatch_activate(source);

done:
	return (source);
}

STATIC void
PvDInfoRequestScheduleXPCRequest(void * info)
{
	ObjectWrapperRef wrapper = (ObjectWrapperRef)info;
	PvDInfoRequestRef request = NULL;

	request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
	if (request == NULL) {
		IPConfigLog(LOG_NOTICE, "can't schedule xpc, null object");
		goto done;
	}
	request->xpc_source
	= PvDInfoRequestCreateXPCSource(request);

done:
	return;
}

STATIC const char *
_state_string(PvDInfoRequestState state)
{
	switch (state) {
		case kPvDInfoRequestStateIdle:
			return "idle";
		case kPvDInfoRequestStateScheduled:
			return "scheduled";
		case kPvDInfoRequestStateObtained:
			return "obtained";
		case kPvDInfoRequestStateFailed:
			return "failed";
		default:
			return "unknown";
	}
}

STATIC void
PvDInfoRequestResumeSync(void * info)
{
	ObjectWrapperRef wrapper = (ObjectWrapperRef)info;
	PvDInfoRequestRef request = NULL;
	NSXPCConnection * connection = nil;
	id<IPHPvDInfoRequestProtocol> xpc_proxy = nil;
	bool success = false;

	request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
	if (request == NULL) {
		IPConfigLog(LOG_ERR, "can't resume a NULL request");
		goto done;
	}
	if (request->active_connection) {
		IPConfigLog(LOG_ERR, "can't resume an active request");
		goto done;
	}
	if (request->state != kPvDInfoRequestStateIdle) {
		IPConfigLog(LOG_ERR, "can only resume an idle request, "
			    "current state: %s", _state_string(request->state));
		goto done;
	}
	connection
	= [[NSXPCConnection alloc] initWithServiceName:@IPH_BUNDLE_ID];
	if (connection == nil) {
		IPConfigLog(LOG_ERR,
			    "failed creating connection to xpc service '%s'",
			    IPH_BUNDLE_ID);
		goto done;
	}
	[connection setRemoteObjectInterface:
	 [NSXPCInterface interfaceWithProtocol:
	  @protocol(IPHPvDInfoRequestProtocol)]];
	xpc_proxy = [connection remoteObjectProxyWithErrorHandler:
		     ^(NSError * _Nonnull error) {
		IPConfigLog(LOG_ERR, "couldn't get xpc remote object proxy "
			    "with error '%@'", error);
		return;
	}];
	if (xpc_proxy == nil) {
		goto done;
	}
	request->xpc_connection = connection;
	request->xpc_proxy = xpc_proxy;
	IPConfigLog(LOG_DEBUG, "connecting to xpc service '%s'", IPH_BUNDLE_ID);
	[request->xpc_connection activate];
	request->active_connection = true;
	/* this sends xpc request, delayed as necessary */
	PvDInfoRequestScheduleXPCRequest(wrapper);
	success = true;
	
done:
	if (!success) {
		if (request != NULL && request->context.pvdid != NULL) {
			IPConfigLog(LOG_NOTICE, "failed xpc for pvdid '%s'",
				CFStringGetCStringPtr(request->context.pvdid,
						      kCFStringEncodingUTF8));
		} else {
			IPConfigLog(LOG_NOTICE, "failed to schedule xpc");
		}
	}
	return;
}

#pragma mark -
#pragma mark SPI

#define PVD_INFO_REQUEST_QUEUE_LABEL "PvDInfoRequestQueue"

PvDInfoRequestRef
PvDInfoRequestCreate(CFStringRef pvdid, CFArrayRef prefixes,
		     const char * ifname, uint64_t ms_delay)
{
	PvDInfoRequestRef request = NULL;
	bool success = false;

	request = __PvDInfoRequestAllocate(NULL);
	if (request == NULL) {
		goto done;
	}
	request->wrapper = ObjectWrapperAlloc(request);
	if (request->wrapper == NULL) {
		goto done;
	}
	request->queue = dispatch_queue_create(PVD_INFO_REQUEST_QUEUE_LABEL,
					       DISPATCH_QUEUE_SERIAL);
	if (request->queue == NULL) {
		goto done;
	}
	request->active_connection = false;
	request->xpc_proxy = nil;
	request->xpc_connection = nil;
	request->xpc_source = NULL;
	request->state = kPvDInfoRequestStateIdle;
	request->additional_info = NULL;
	PvDInfoRequestSetContext(request, pvdid, prefixes, ifname, ms_delay);
	success = true;

done:
	if (!success) {
		IPConfigLog(LOG_NOTICE,
			    "failed to create a PvDInfoRequest object");
		my_CFRelease(&request);
	}
	return (request);
}

void
PvDInfoRequestSetCompletionHandler(PvDInfoRequestRef request,
				   dispatch_block_t completion,
				   dispatch_queue_t queue)
{
	dispatch_sync(request->queue, ^{
		PvDInfoRequestInvalidateCompletionSource(request);
		request->completion_source
		= PvDInfoRequestCreateCompletionSource(queue, completion,
						       request->wrapper);
	});
	return;
}

void
PvDInfoRequestCancel(PvDInfoRequestRef request)
{
	ObjectWrapperRef wrapper = NULL;

	if (request->queue == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: null request", __func__);
		goto done;
	}
	dispatch_sync(request->queue, ^{
		PvDInfoRequestInvalidateCompletionSource(request);
	});
	wrapper = request->wrapper;
	ObjectWrapperRetain(wrapper);
	dispatch_async(request->queue, ^{
		PvDInfoRequestRef request = NULL;

		request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
		ObjectWrapperRelease(wrapper);
		if (request == NULL) {
			IPConfigLog(LOG_NOTICE, "request no longer valid");
			return;
		}
		PvDInfoRequestCleanupXPC(request);
		PvDInfoRequestFlushContext(request);
		request->state = kPvDInfoRequestStateIdle;
	});

done:
	return;
}

void
PvDInfoRequestResume(PvDInfoRequestRef request)
{
	ObjectWrapperRef wrapper = NULL;

	if (request->queue == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: null request", __func__);
		goto done;
	}
	wrapper = request->wrapper;
	ObjectWrapperRetain(wrapper);
	dispatch_async(request->queue, ^{
		PvDInfoRequestResumeSync(request->wrapper);
		ObjectWrapperRelease(wrapper);
	});
done:
	return;
}

PvDInfoRequestState
PvDInfoRequestGetCompletionStatus(PvDInfoRequestRef request)
{
	return (request->state);
}

CFDictionaryRef
PvDInfoRequestCopyAdditionalInformation(PvDInfoRequestRef request)
{
	__block CFDictionaryRef additional_info = NULL;
	dispatch_sync(request->queue, ^{
		additional_info 
		= CFDictionaryCreateCopy(NULL, request->additional_info);
	});
	return (additional_info);
}

#ifdef __TEST_PVDINFOREQUEST_SPI__

#pragma mark -
#pragma mark Test


static dispatch_semaphore_t waiter;
static dispatch_queue_t mock_ipconfigagent_queue;
static dispatch_queue_t mock_iphxpc_queue;

STATIC void
_PvDInfoRequestResumeSync_tester(void * info)
{
	ObjectWrapperRef wrapper = (ObjectWrapperRef)info;
	PvDInfoRequestRef request = NULL;
	CFMutableDictionaryRef xpc_ret_dict = NULL;
	CFDictionaryRef addinfo_dict = NULL;
	bool success = false;

	IPConfigLog(LOG_DEBUG, "%s", __func__);
	request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
	if (request == NULL) {
		IPConfigLog(LOG_ERR, "can't resume a NULL request");
		goto done;
	}
	if (request->active_connection) {
		IPConfigLog(LOG_ERR, "can't resume an active request");
		goto done;
	}
	if (request->state != kPvDInfoRequestStateIdle) {
		IPConfigLog(LOG_ERR, "can only resume an idle request, "
			    "current state: %s", _state_string(request->state));
		goto done;
	}
	/* omitting actual xpc */
	request->active_connection = true;
	/* this acts as if we got a valid xpc reply */
	xpc_ret_dict = CFDictionaryCreateMutable(NULL, 2,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	addinfo_dict = CFDictionaryCreateMutable(NULL, 0,
						 &kCFTypeDictionaryKeyCallBacks,
						 &kCFTypeDictionaryValueCallBacks);
	CFDictionarySetValue(xpc_ret_dict, kPvDInfoValidFetchXPCKey, 
			     kCFBooleanTrue);
	CFDictionarySetValue(xpc_ret_dict, kPvDInfoAdditionalInfoDictXPCKey,
			     addinfo_dict);
	CFRelease(addinfo_dict);
	PvDInfoRequestXPCCompletionHandler((__bridge_transfer NSDictionary *)xpc_ret_dict,
					   wrapper);
	success = true;

done:
	if (!success) {
		IPConfigLog(LOG_NOTICE, "failed to schedule xpc");
	}
	return;
}

/*
 * This is dispatched via the SPIs dispatch source
 * onto the caller-supplied queue.
 */
STATIC void
_pvd_info_request_callback(PvDInfoRequestRef request)
{
	printf("calling back!\n");
	IPConfigLog(LOG_NOTICE, "calling back!");

	/* the following call used to emulate the crash from rdar://128450269 */
	PvDInfoRequestCancel(request);
	CFRelease(request);

	/* all good, the test can end */
	dispatch_semaphore_signal(waiter);
}

/*
 * This emulates the PvDInfoRequestRef setup flow within rtadv.c
 */
STATIC PvDInfoRequestRef
_pvd_info_request_create_with_completion(dispatch_block_t completion)
{
	PvDInfoRequestRef request = NULL;

	request = PvDInfoRequestCreate(CFSTR("test.domain.local"),
				       (__bridge CFArrayRef)@[@"fd77::"],
				       "lo0", 0);
	PvDInfoRequestSetCompletionHandler(request, completion,
					   mock_ipconfigagent_queue);

	return request;
}

STATIC void
_PvDInfoRequestCompletedCallback_tester(PvDInfoRequestRef request,
					CFBooleanRef valid_fetch)
{
	ObjectWrapperRef wrapper = NULL;

	if (request->queue == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: null request", __func__);
		goto done;
	}
	wrapper = request->wrapper;
	ObjectWrapperRetain(wrapper);
	printf("xpc waiting to put block in internal queue\n");
	dispatch_semaphore_wait(waiter, DISPATCH_TIME_FOREVER);
	dispatch_async(request->queue, ^{
		PvDInfoRequestRef request = NULL;

		request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
		if (request == NULL) {
			IPConfigLog(LOG_NOTICE, "request no longer valid");
			goto done;
		}
		if (valid_fetch == kCFBooleanFalse) {
			IPConfigLog(LOG_DEBUG, "xpc reply: failure");
			request->state = kPvDInfoRequestStateFailed;
		} else if (request->additional_info == NULL) {
			IPConfigLog(LOG_DEBUG, "xpc reply: no internet");
			request->state = kPvDInfoRequestStateIdle;
		} else if (request->additional_info != NULL) {
			/* SUCCESS */
			IPConfigLog(LOG_DEBUG,
				    "xpc reply: got addinfo dict:\n%@",
				    request->additional_info);
			request->state = kPvDInfoRequestStateObtained;
		}
		PvDInfoRequestCleanupXPC(request);
		PvDInfoRequestSendNotificationToClient(request);
	done:
		ObjectWrapperRelease(wrapper);
		return;
	});
	printf("xpc enqueued block in internal queue\n");

done:
	return;
}

STATIC void
_PvDInfoRequestXPCCompletionHandler_tester(void * info)
{
	PvDInfoRequestRef request = NULL;
	ObjectWrapperRef wrapper = (ObjectWrapperRef)info;

	request = (PvDInfoRequestRef)ObjectWrapperGetObject(wrapper);
	if (request == NULL) {
		IPConfigLog(LOG_NOTICE, "%s: null object", __func__);
		goto done;
	}
	CFRetain(request);
	_PvDInfoRequestCompletedCallback_tester(request, kCFBooleanFalse);

done:
	my_CFRelease(&request);
	return;
}

int
main(int argc, char * argv[])
{
	__block PvDInfoRequestRef request = NULL;

	waiter = dispatch_semaphore_create(0);
	mock_ipconfigagent_queue
	= dispatch_queue_create("Mock-IPConfigurationAgentQueue", NULL);

	printf("test 1 start\n");
	dispatch_sync(mock_ipconfigagent_queue, ^{
		dispatch_block_t completion;

		completion = ^{
			// emulates rtadv_pvd_additional_info_request_callback()
			_pvd_info_request_callback(request);
		};
		request = _pvd_info_request_create_with_completion(completion);
		_PvDInfoRequestResumeSync_tester(request->wrapper);
	});
	dispatch_semaphore_wait(waiter, DISPATCH_TIME_FOREVER);
	printf("test 1 done\n");

	printf("test 2 start\n");
	dispatch_sync(mock_ipconfigagent_queue, ^{
		dispatch_block_t completion;

		completion = ^{
			CFRelease(request);
		};
		request = _pvd_info_request_create_with_completion(completion);
		// what PvDInfoRequestResume() would do
		request->active_connection = true;
		// emulates rtadv_pvd_flush()
		PvDInfoRequestCancel(request);
		// crash from overrelease of ObjectWrapperRef
		// rdar://135907968
		CFRelease(request);
	});
	printf("test 2 done\n");

	/*
	 * NSXPCConnection object with IPH already enqueued a completion block 
	 * to the PvDInfoRequest's internal queue, hence the queue won't be
	 * released until said block runs. However, the PvDRequestRef was just
	 * released by IPConfigurationAgent. When the XPC completion block gets
	 * to run, it results in an overrelease of PvDInfoRequestRef.
	 * rdar://135906175
	 */
	printf("test 3 start\n");
	dispatch_sync(mock_ipconfigagent_queue, ^{
		dispatch_block_t completion;

		completion = ^{
			CFRelease(request);
		};
		request = _pvd_info_request_create_with_completion(completion);
		request->active_connection = true;
	});
	mock_iphxpc_queue = dispatch_queue_create("Mock-IPHXPCQueue", NULL);
	dispatch_async(mock_iphxpc_queue, ^{
		ObjectWrapperRef wrapper = NULL;

		wrapper = request->wrapper;
		ObjectWrapperRetain(wrapper);
		_PvDInfoRequestXPCCompletionHandler_tester(wrapper);
		ObjectWrapperRelease(wrapper);
	});
	dispatch_sync(mock_ipconfigagent_queue, ^{
		PvDInfoRequestCancel(request);
		printf("cancellation block just enqueued\n");
		dispatch_semaphore_signal(waiter);
		CFRelease(request);
	});
	sleep(1);
	printf("test 3 done\n");

	printf("exiting\n");
	return 0;
}

#endif
