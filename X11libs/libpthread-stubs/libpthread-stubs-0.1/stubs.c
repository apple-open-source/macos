/* Copyright (C) 2006 Diego Pettenò
 * Inspired by libX11 code copyright (c) 1995 David E. Wexelblat.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the names of the authors or their
 * institutions shall not be used in advertising or otherwise to promote the
 * sale, use or other dealings in this Software without prior written
 * authorization from the authors.
 */

#include <pthread.h>
#include "config.h"

#ifndef HAVE_PTHREAD_SELF
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_self() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_self = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_MUTEX_INIT
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_mutex_init() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_mutex_init = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_MUTEX_DESTROY
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_mutex_destroy() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_mutex_destroy = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_MUTEX_LOCK
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_mutex_lock() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_mutex_lock = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_MUTEX_UNLOCK
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_mutex_unlock() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_mutex_unlock = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_COND_INIT
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_cond_init() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_cond_init = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_COND_DESTROY
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_cond_destroy() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_cond_destroy = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_COND_WAIT
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_cond_wait() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_cond_wait = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_COND_SIGNAL
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_cond_signal() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_cond_signal = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_COND_BROADCAST
#define NEED_ZERO_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_cond_broadcast() __attribute__ ((weak, alias ("__pthread_zero_stub")));
# else
#  pragma weak pthread_cond_broadcast = __pthread_zero_stub
# endif
#endif

#ifndef HAVE_PTHREAD_EQUAL
#define NEED_EQUAL_STUB
# ifdef SUPPORT_ATTRIBUTE_ALIAS
int pthread_equal() __attribute__ ((weak, alias ("__pthread_equal_stub")));
# else
#  pragma weak pthread_equal = __pthread_equal_stub
# endif
#endif

#ifdef NEED_ZERO_STUB
static int __pthread_zero_stub()
{
    return 0;
}
#endif

#ifdef NEED_EQUAL_STUB
static int __pthread_equal_stub(pthread_t t1, pthread_t t2)
{
    return (t1 == t2);
}
#endif
