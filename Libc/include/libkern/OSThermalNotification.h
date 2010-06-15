/*
 * Copyright (c) 2007 Apple Inc. All rights reserved.
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

#ifndef _OSTHERMALNOTIFICATION_H_
#define _OSTHERMALNOTIFICATION_H_

#include <sys/cdefs.h>

/*
**  OSThermalNotification.h
**  
**  Notification mechanism to alert registered tasks when device thermal conditions
**  reach certain thresholds. Notifications are triggered in both directions
**  so clients can manage their memory usage more and less aggressively.
**
*/

__BEGIN_DECLS

typedef enum {
	OSThermalNotificationLevelAny      = -1,
	OSThermalNotificationLevelNormal   =  0,
	OSThermalNotificationLevel70PercentTorch  =  1,
	OSThermalNotificationLevel70PercentBacklight  =  3,
	OSThermalNotificationLevel50PercentTorch  =  3,
	OSThermalNotificationLevel50PercentBacklight  =  5,
	OSThermalNotificationLevelDisableTorch  =  5,
	OSThermalNotificationLevel25PercentBacklight = 7,
	OSThermalNotificationLevelAppTerminate  =  12,
	OSThermalNotificationLevelDeviceRestart = 16
} OSThermalNotificationLevel;

/* Backwards compatibility */
#define OSThermalNotificationLevelWarning OSThermalNotificationLevel70PercentBacklight
#define OSThermalNotificationLevelUrgent OSThermalNotificationLevelAppTerminate
#define OSThermalNotificationLevelCritical OSThermalNotificationLevelDeviceRestart

/*
** Simple polling interface to detect current thermal level
*/

OSThermalNotificationLevel OSThermalNotificationCurrentLevel(void);

/*
** External notify(3) string for manual notification setup
*/

extern const char *kOSThermalNotificationName;

__END_DECLS

#endif /* _OSTHERMALNOTIFICATION_H_ */
