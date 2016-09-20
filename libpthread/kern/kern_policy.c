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
#include <kern/assert.h>

pthread_priority_t
thread_qos_get_pthread_priority(int qos)
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
thread_qos_get_class_index(int qos)
{
    switch (qos) {
		case THREAD_QOS_USER_INTERACTIVE: return 0;
		case THREAD_QOS_USER_INITIATED: return 1;
		case THREAD_QOS_LEGACY: return 2;
		case THREAD_QOS_UTILITY: return 3;
		case THREAD_QOS_BACKGROUND: return 4;
		case THREAD_QOS_MAINTENANCE: return 5;
		default: return 2;
    }
}

int
pthread_priority_get_thread_qos(pthread_priority_t priority)
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

int
pthread_priority_get_class_index(pthread_priority_t priority)
{
	return qos_class_get_class_index(_pthread_priority_get_qos_newest(priority));
}

pthread_priority_t
class_index_get_pthread_priority(int index)
{
	qos_class_t qos;
	switch (index) {
		case 0: qos = QOS_CLASS_USER_INTERACTIVE; break;
		case 1: qos = QOS_CLASS_USER_INITIATED; break;
		case 2: qos = QOS_CLASS_DEFAULT; break;
		case 3: qos = QOS_CLASS_UTILITY; break;
		case 4: qos = QOS_CLASS_BACKGROUND; break;
		case 5: qos = QOS_CLASS_MAINTENANCE; break;
		case 6: assert(index != 6); // EVENT_MANAGER should be handled specially
		default:
			/* Return the utility band if we don't understand the input. */
			qos = QOS_CLASS_UTILITY;
	}

	pthread_priority_t priority;
	priority = _pthread_priority_make_newest(qos, 0, 0);

	return priority;
}

int
class_index_get_thread_qos(int class)
{
	int thread_qos;
	switch (class) {
		case 0: thread_qos = THREAD_QOS_USER_INTERACTIVE; break;
		case 1: thread_qos = THREAD_QOS_USER_INITIATED; break;
		case 2: thread_qos = THREAD_QOS_LEGACY; break;
		case 3: thread_qos = THREAD_QOS_UTILITY; break;
		case 4: thread_qos = THREAD_QOS_BACKGROUND; break;
		case 5: thread_qos = THREAD_QOS_MAINTENANCE; break;
		case 6: thread_qos = THREAD_QOS_LAST; break;
		default:
			thread_qos = THREAD_QOS_LAST;
	}
	return thread_qos;
}

int
qos_class_get_class_index(int qos)
{
	switch (qos){
		case QOS_CLASS_USER_INTERACTIVE: return 0;
		case QOS_CLASS_USER_INITIATED: return 1;
		case QOS_CLASS_DEFAULT: return 2;
		case QOS_CLASS_UTILITY: return 3;
		case QOS_CLASS_BACKGROUND: return 4;
		case QOS_CLASS_MAINTENANCE: return 5;
		default:
			/* Return the default band if we don't understand the input. */
			return 2;
	}
}

/**
 * Shims to help the kernel understand pthread_priority_t
 */

integer_t
_thread_qos_from_pthread_priority(unsigned long priority, unsigned long *flags)
{
    if (flags != NULL){
        *flags = (int)_pthread_priority_get_flags(priority);
    }
    int thread_qos = pthread_priority_get_thread_qos(priority);
    if (thread_qos == THREAD_QOS_UNSPECIFIED && flags != NULL){
        *flags |= _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
    }
    return thread_qos;
}

pthread_priority_t
_pthread_priority_canonicalize(pthread_priority_t priority, boolean_t for_propagation)
{
	qos_class_t qos_class;
	int relpri;
	unsigned long flags = _pthread_priority_get_flags(priority);
	_pthread_priority_split_newest(priority, qos_class, relpri);

	if (for_propagation) {
		flags = 0;
		if (relpri > 0 || relpri < -15) relpri = 0;
	} else {
		if (qos_class == QOS_CLASS_UNSPECIFIED) {
			flags = _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
		} else if (flags & (_PTHREAD_PRIORITY_EVENT_MANAGER_FLAG|_PTHREAD_PRIORITY_SCHED_PRI_FLAG)){
			flags = _PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
			qos_class = QOS_CLASS_UNSPECIFIED;
		} else {
			flags &= _PTHREAD_PRIORITY_OVERCOMMIT_FLAG|_PTHREAD_PRIORITY_EVENT_MANAGER_FLAG;
		}

		relpri = 0;
	}

	return _pthread_priority_make_newest(qos_class, relpri, flags);
}
