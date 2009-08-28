/*
 * Copyright (c) 2007 Apple, Inc. All rights reserved.
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
/* pthread workqueue defns */

#ifndef _POSIX_PTHREAD_WORKQUEUE_H
#define _POSIX_PTHREAD_WORKQUEUE_H

#include <sys/cdefs.h>
#include <pthread.h>


#define __PTHREAD_WORKQ_SIZE__ 128
#define __PTHREAD_WORKQ_ATTR_SIZE__ 60

#define PTHREAD_WORKQUEUE_SIG 0xBEBEBEBE
#define PTHREAD_WORKQUEUE_ATTR_SIG 0xBEBEBEBE

#ifndef __POSIX_LIB__
typedef struct { unsigned int  sig; char opaque[__PTHREAD_WORKQ_SIZE__];} *pthread_workqueue_t;
typedef struct { unsigned int  sig; char opaque[__PTHREAD_WORKQ_ATTR_SIZE__]; } pthread_workqueue_attr_t;
#endif
typedef void * pthread_workitem_handle_t;
/* Kernel expected target concurrency of the workqueue clients for the three priority queues */

#define WORKQ_HIGH_PRIOQUEUE	0
#define WORKQ_DEFAULT_PRIOQUEUE	1
#define WORKQ_LOW_PRIOQUEUE	2

#define WORKQ_NUM_PRIOQUEUE	3

extern __int32_t workq_targetconc[WORKQ_NUM_PRIOQUEUE];

__BEGIN_DECLS
int pthread_workqueue_init_np(void);
int pthread_workqueue_attr_init_np(pthread_workqueue_attr_t * attr);
int pthread_workqueue_attr_destroy_np(pthread_workqueue_attr_t * attr);
int pthread_workqueue_attr_getqueuepriority_np(const pthread_workqueue_attr_t * attr, int * qprio);
/* WORKQ_HIGH/DEFAULT/LOW_PRIOQUEUE are the only valid values */
int pthread_workqueue_attr_setqueuepriority_np(pthread_workqueue_attr_t * attr, int qprio);
int pthread_workqueue_attr_getovercommit_np(const pthread_workqueue_attr_t * attr, int * ocommp);
int pthread_workqueue_attr_setovercommit_np(pthread_workqueue_attr_t * attr, int ocomm);


int pthread_workqueue_create_np(pthread_workqueue_t * workqp, const pthread_workqueue_attr_t * attr);
int pthread_workqueue_additem_np(pthread_workqueue_t workq, void ( *workitem_func)(void *), void * workitem_arg, pthread_workitem_handle_t * itemhandlep, unsigned int *gencountp);
/* If the queue value is WORKQ_NUM_PRIOQUEUE, the request for concurrency is for all queues */
int pthread_workqueue_requestconcurrency_np(int queue, int concurrency);
int pthread_workqueue_getovercommit_np(pthread_workqueue_t workq,  unsigned int *ocommp);
/* 
 * If the arg is non zero, it enables kill on current thread.
 * If the arg of zero, it disables kill on current thread.
 */
int __pthread_workqueue_setkill(int);
__END_DECLS

#endif /* _POSIX_PTHREAD_WORKQUEUE_H */

