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

#define PTHEAD_WRKQUEUE_SIG 0xBEBEBEBE
#define PTHEAD_WRKQUEUE_ATTR_SIG 0xBEBEBEBE

#ifndef __POSIX_LIB__
typedef struct { unsigned int  sig; char opaque[__PTHREAD_WORKQ_SIZE__];} *pthread_workqueue_t;
typedef struct { unsigned int  sig; char opaque[__PTHREAD_WORKQ_ATTR_SIZE__]; } pthread_workqueue_attr_t;
#endif
typedef void * pthread_workitem_handle_t;

__BEGIN_DECLS
int pthread_workqueue_init_np(void);
int pthread_workqueue_attr_init_np(pthread_workqueue_attr_t * attr);
int pthread_workqueue_attr_destroy_np(pthread_workqueue_attr_t * attr);
int pthread_workqueue_attr_getqueuepriority_np(const pthread_workqueue_attr_t * attr, int * qprio);
int pthread_workqueue_attr_setqueuepriority_np(pthread_workqueue_attr_t * attr, int qprio);

#ifdef NOTYET
/* Following attributes not supported yet */
int pthread_workqueue_attr_getstacksize_np(const pthread_workqueue_attr_t * attr, size_t * stacksizep);
int pthread_workqueue_attr_setstacksize_np(pthread_workqueue_attr_t * attr, size_t stacksize);
int pthread_workqueue_attr_getthreadtimeshare_np(const pthread_workqueue_attr_t * attr, int * istimesahrep);
int pthread_workqueue_attr_settthreadtimeshare_np(pthread_workqueue_attr_t * attr, int istimeshare);
int pthread_workqueue_attr_getthreadimportance_np(const pthread_workqueue_attr_t * attr, int * importancep);
int pthread_workqueue_attr_settthreadimportance_np(pthread_workqueue_attr_t * attr, int importance);
int pthread_workqueue_attr_getthreadaffinity_np(const pthread_workqueue_attr_t * attr, int * affinityp);
int pthread_workqueue_attr_settthreadaffinity_np(pthread_workqueue_attr_t * attr, int affinity);
#endif

int pthread_workqueue_create_np(pthread_workqueue_t * workqp, const pthread_workqueue_attr_t * attr);
int pthread_workqueue_destroy_np(pthread_workqueue_t workq, void (* callback_func)(pthread_workqueue_t, void *), void * callback_arg);
int pthread_workqueue_additem_np(pthread_workqueue_t workq, void ( *workitem_func)(void *), void * workitem_arg, pthread_workitem_handle_t * itemhandlep);
int pthread_workqueue_removeitem_np(pthread_workqueue_t workq, pthread_workitem_handle_t itemhandle);
int pthread_workqueue_addbarrier_np(pthread_workqueue_t workq, void (* callback_func)(pthread_workqueue_t, void *), void * callback_arg, int waitforcallback, pthread_workitem_handle_t *itemhandlep);
int pthread_workqueue_suspend_np(pthread_workqueue_t workq);
int pthread_workqueue_resume_np(pthread_workqueue_t workq);

__END_DECLS

#endif /* _POSIX_PTHREAD_WORKQUEUE_H */

