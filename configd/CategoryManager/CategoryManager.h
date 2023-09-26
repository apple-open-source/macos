/*
 * Copyright (c) 2022 Apple Inc. All rights reserved.
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
#ifndef _CATEGORY_MANAGER_H
#define _CATEGORY_MANAGER_H

/*
 * CategoryManager.h
 * - the CategoryManager API is a thin, stateless layer to handle just the IPC
 *   details
 */

/*
 * Modification History
 *
 * November 7, 2022	Dieter Siegmund (dieter@apple.com)
 * - initial revision
 */

#include <SystemConfiguration/SCNetworkCategoryManager.h>

/*
 * Block: CategoryManagerEventHandler
 * Purpose:
 *   Asynchronous event handler.
 *  
 * kCategoryManagerEventConnectionInterrupted
 *   Give the caller an opportunity to re-sync state with the server should
 *   the server crash/exit.
 *
 *   You can only invoke CategoryManagerConnectionSynchronize() within
 *   the reconnect callback. Calling the other API will deadlock unless
 *   dispatch_async'd onto a different queue.
 *
 * kCategoryManagerEventValueAcknowledged
 *   The active value has been acknowledged.
 */
typedef CF_ENUM(uint32_t, CategoryManagerEvent) {
	kCategoryManagerEventNone = 0,
	kCategoryManagerEventConnectionInvalid = 1,
	kCategoryManagerEventConnectionInterrupted = 2,
	kCategoryManagerEventValueAcknowledged = 3,
};

typedef void (^CategoryManagerEventHandler)(xpc_connection_t connection,
					    CategoryManagerEvent event);

xpc_connection_t
CategoryManagerConnectionCreate(dispatch_queue_t queue,
				CategoryManagerEventHandler handler);

errno_t
CategoryManagerConnectionRegister(xpc_connection_t connection,
				  CFStringRef category,
				  CFStringRef ifname,
				  SCNetworkCategoryManagerFlags flags);

errno_t
CategoryManagerConnectionActivateValue(xpc_connection_t connection,
				       CFStringRef value);

CFStringRef
CategoryManagerConnectionCopyActiveValue(xpc_connection_t connection,
					 int * error);
void
CategoryManagerConnectionSynchronize(xpc_connection_t connection,
				     CFStringRef category,
				     CFStringRef ifname,
				     SCNetworkCategoryManagerFlags flags,
				     CFStringRef value);

CFStringRef
CategoryManagerConnectionCopyActiveValueNoSession(xpc_connection_t connection,
						  CFStringRef category,
						  CFStringRef ifname,
						  int * error);

#endif /* _CATEGORY_MANAGER_H */
