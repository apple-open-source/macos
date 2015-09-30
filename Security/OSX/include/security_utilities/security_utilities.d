/*
 * Copyright (c) 2008 Apple Inc. All Rights Reserved.
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
 *
 *
 * DTrace static providers at the security utility layer
 */
#define int32_t int
#define uint32_t unsigned


/*
 * The security-debug provider replaces the old text-based debug log facility
 */
provider security_debug {
	probe log(const char *scope, const char *message);
	probe logp(const char *scope, void *object, const char *message);
	probe delay(const char *path);
	
	probe sec__create(void *object, const char *className, uint32_t classID);
	probe sec__destroy(void *object);
	
	probe refcount__create(const void *p);
	probe refcount__up(const void *p, unsigned count);
	probe refcount__down(const void *p, unsigned count);
};


/*
 * Track and monitor C++ exception activity
 */
typedef const void *DTException;

provider security_exception {
	probe throw__osstatus(DTException error, int32_t status);
	probe throw__cssm(DTException error, uint32_t status);
	probe throw__mach(DTException error, int32_t status);
	probe throw__unix(DTException error, int errno);
	probe throw__pcsc(DTException error, uint32_t status);
	probe throw__sqlite(DTException error, int status, const char *message);
	probe throw__cf(DTException error);
	probe throw__other(DTException error, uint32_t status, const char *type);
	probe copy(DTException newError, DTException oldError);
	probe handled(DTException error);
};


/*
 * The MachServer run-loop controller
 */
provider security_machserver {
	probe start_thread(int subthread);
	probe end_thread(int thrown);
	probe begin(uint32_t servicePort, int id);
	probe end();
	probe timer__start(void *timer, int longterm, double when);
	probe timer__end(int thrown);
	probe port__add(uint32_t port);
	probe port__remove(uint32_t port);
	probe receive(double timeout);	// (timeout == 0 means infinite)
	probe receive_error(uint32_t machError);
	probe send_error(uint32_t machError, uint32_t replyPort);
	probe reap(uint32_t count, uint32_t idle);
	probe alloc__register(const void *memory, const void *allocator);
	probe alloc__release(const void *memory, const void *allocator);
};
