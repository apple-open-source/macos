/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include "kern_internal.h"
#include <kern/debug.h>

pthread_priority_t
pthread_qos_class_get_priority(int qos)
{
    /* Map the buckets we have in pthread_priority_t into a QoS tier. */
    switch (qos) {
		case THREAD_QOS_USER_INTERACTIVE: return _pthread_priority_make_newest(QOS_CLASS_USER_INTERACTIVE, 0, 0);
		case THREAD_QOS_USER_INITIATED: return _pthread_priority_make_newest(QOS_CLASS_USER_INITIATED, 0, 0);
		case THREAD_QOS_LEGACY: return _pthread_priority_make_newest(QOS_CLASS_DEFAULT, 0, 0);
		case THREAD_QOS_UTILITY: return _pthread_priority_make_newest(QOS_CLASS_UTILITY, 0, 0);
		case THREAD_QOS_BACKGROUND: return _pthread_priority_make_newest(QOS_CLASS_BACKGROUND, 0, 0);
		case THREAD_QOS_MAINTENANCE: return _pthread_priority_make_newest(QOS_CLASS_MAINTENANCE, 0, 0);
		default: return _pthread_priority_make_newest(QOS_CLASS_UNSPECIFIED, 0, 0);
    }
}

int
pthread_priority_get_qos_class(pthread_priority_t priority)
{
	/* Map the buckets we have in pthread_priority_t into a QoS tier. */
	switch (_pthread_priority_get_qos_newest(priority)) {
		case QOS_CLASS_USER_INTERACTIVE: return THREAD_QOS_USER_INTERACTIVE;
		case QOS_CLASS_USER_INITIATED: return THREAD_QOS_USER_INITIATED;
		case QOS_CLASS_DEFAULT: return THREAD_QOS_LEGACY;
		case QOS_CLASS_UTILITY: return THREAD_QOS_UTILITY;
		case QOS_CLASS_BACKGROUND: return THREAD_QOS_BACKGROUND;
		case QOS_CLASS_MAINTENANCE: return THREAD_QOS_MAINTENANCE;
		default: return THREAD_QOS_UNSPECIFIED;
	}
}

pthread_priority_t
pthread_priority_from_class_index(int index)
{
	qos_class_t qos;
	switch (index) {
		case 0: qos = QOS_CLASS_USER_INTERACTIVE; break;
		case 1: qos = QOS_CLASS_USER_INITIATED; break;
		case 2: qos = QOS_CLASS_DEFAULT; break;
		case 3: qos = QOS_CLASS_UTILITY; break;
		case 4: qos = QOS_CLASS_BACKGROUND; break;
		case 5: qos = QOS_CLASS_MAINTENANCE; break;
		default:
			/* Return the utility band if we don't understand the input. */
			qos = QOS_CLASS_UTILITY;
	}

	pthread_priority_t priority;
	priority = _pthread_priority_make_newest(qos, 0, 0);

	return priority;
}

int
pthread_priority_get_class_index(pthread_priority_t priority)
{
	switch (_pthread_priority_get_qos_newest(priority)) {
		case QOS_CLASS_USER_INTERACTIVE: return 0;
		case QOS_CLASS_USER_INITIATED: return 1;
		case QOS_CLASS_DEFAULT: return 2;
		case QOS_CLASS_UTILITY: return 3;
		case QOS_CLASS_BACKGROUND: return 4;
		case QOS_CLASS_MAINTENANCE: return 5;
		default:
			/* Return the utility band if we don't understand the input. */
			return 2;
	}
}
