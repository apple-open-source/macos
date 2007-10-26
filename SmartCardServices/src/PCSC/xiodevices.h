/*
 * Copyright (c) 2006 Apple Computer, Inc. All Rights Reserved.
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

//
// xiodevices - code for finding and tracking devices via IOKit
//
#ifndef _H_XIODEVICES
#define _H_XIODEVICES

#include <security_utilities/iodevices.h>

#if defined(__cplusplus)

namespace Security {
namespace IOKit {

//
// An IOKit notification port object
//
class XNotificationPort : public MachPortNotificationPort
{
public:
	XNotificationPort() : MachPortNotificationPort() {}
	~XNotificationPort() {}
	
	class XReceiver : public Receiver
	{
	public:
		virtual void ioChange(DeviceIterator &iterator) = 0;
		virtual void ioServiceChange(void *refCon, io_service_t service,	//IOServiceInterestCallback
			natural_t messageType, void *messageArgument) = 0;
	};
	
	void add(DeviceMatch match, XReceiver &receiver, const char *type = kIOFirstMatchNotification);
	void addInterestNotification(XReceiver &receiver, io_service_t service,
		const io_name_t interestType = kIOGeneralInterest);

private:

	static void ioDeviceNotification(void *refCon, io_service_t service,
		natural_t messageType, void *messageArgument);
	static void ioNotify(void *refCon, io_iterator_t iterator);
};

} // end namespace MachPlusPlus
} // end namespace Security

#endif /* __cplusplus__ */

#endif //_H_XIODEVICES
