/*
 * Copyright (c) 2011-12 Apple Inc. All Rights Reserved.
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

/*
 * Copyright (c) 2006 - 2007 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "ossl-config.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "ossl-rand.h"
#include "ossl-randi.h"

#ifndef O_BINARY
#define O_BINARY    0
#endif

/**
 * @page page_rand RAND - random number
 *
 * See the library functions here: @ref hcrypto_rand
 */
const static RAND_METHOD *selected_meth = NULL;
static ENGINE *selected_engine = NULL;

static void
init_method(void)
{
	if (selected_meth != NULL) {
		return;
	}
#if defined(__APPLE_PRIVATE__)
#ifdef __APPLE_TARGET_EMBEDDED__
	selected_meth = &ossl_rand_unix_method;
#else

	/*
	 * XXX use arc4random(3) for now until rdar://problem/8174874
	 * is fixed.
	 */
#if 1
	selected_meth = &ossl_rand_arc4_method;
#else
	selected_meth = &ossl_rand_cc_method;
#endif
#endif
#elif defined(_WIN32)
	selected_meth = &ossl_rand_w32crypto_method;
#elif defined(__APPLE__)
	barf
	    selected_meth = &ossl_rand_unix_method;
#else
	selected_meth = &ossl_rand_fortuna_method;
#endif
}


/**
 * Seed that random number generator. Secret material can securely be
 * feed into the function, they will never be returned.
 *
 * @param indata seed data
 * @param size length seed data
 */
void
RAND_seed(const void *indata, size_t size)
{
	init_method();
	(*selected_meth->seed)(indata, size);
}


/**
 * Get a random block from the random generator, can be used for key material.
 *
 * @param outdata random data
 * @param size length random data
 *
 * @return 1 on success, 0 on failure.
 */
int
RAND_bytes(void *outdata, size_t size)
{
	if (size == 0) {
		return (1);
	}
	init_method();
	return ((*selected_meth->bytes)(outdata, size));
}


/**
 * Reset and free memory used by the random generator.
 */
void
RAND_cleanup(void)
{
	const RAND_METHOD *meth = selected_meth;
	ENGINE *engine = selected_engine;

	selected_meth = NULL;
	selected_engine = NULL;

	if (meth) {
		(*meth->cleanup)();
	}
	if (engine) {
		ENGINE_finish(engine);
	}
}


/**
 * Seed that random number generator. Secret material can securely be
 * feed into the function, they will never be returned.
 *
 * @param indata the input data.
 * @param size size of in data.
 * @param entropi entropi in data.
 */
void
RAND_add(const void *indata, size_t size, double entropi)
{
	init_method();
	(*selected_meth->add)(indata, size, entropi);
}


/**
 * Get a random block from the random generator, should NOT be used for key material.
 *
 * @param outdata random data
 * @param size length random data
 *
 * @return 1 on success, 0 on failure.
 */
int
RAND_pseudo_bytes(void *outdata, size_t size)
{
	init_method();
	return ((*selected_meth->pseudorand)(outdata, size));
}


/**
 * Return status of the random generator
 *
 * @return 1 if the random generator can deliver random data.
 */
int
RAND_status(void)
{
	init_method();
	return ((*selected_meth->status)());
}


/**
 * Set the default random method.
 *
 * @param meth set the new default method.
 *
 * @return 1 on success.
 */
int
RAND_set_rand_method(const RAND_METHOD *meth)
{
	const RAND_METHOD *old = selected_meth;

	selected_meth = meth;
	if (old) {
		(*old->cleanup)();
	}
	if (selected_engine) {
		ENGINE_finish(selected_engine);
		selected_engine = NULL;
	}
	return (1);
}


/**
 * Get the default random method.
 *
 */
const RAND_METHOD *
RAND_get_rand_method(void)
{
	init_method();
	return (selected_meth);
}


/**
 * Set the default random method from engine.
 *
 * @param engine use engine, if NULL is passed it, old method and engine is cleared.
 *
 * @return 1 on success, 0 on failure.
 *
 */
int
RAND_set_rand_engine(ENGINE *engine)
{
	const RAND_METHOD *meth, *old = selected_meth;

	if (engine) {
		ENGINE_up_ref(engine);
		meth = ENGINE_get_RAND(engine);
		if (meth == NULL) {
			ENGINE_finish(engine);
			return (0);
		}
	} else {
		meth = NULL;
	}

	if (old) {
		(*old->cleanup)();
	}

	if (selected_engine) {
		ENGINE_finish(selected_engine);
	}

	selected_engine = engine;
	selected_meth = meth;

	return (1);
}


#define RAND_FILE_SIZE    1024

/**
 * Load a a file and feed it into RAND_seed().
 *
 * @param filename name of file to read.
 * @param size minimum size to read.
 *
 */
int
RAND_load_file(const char *filename, size_t size)
{
	unsigned char buf[128];
	size_t len;
	ssize_t slen;
	int fd, flg;

	fd = open(filename, O_RDONLY | O_BINARY, 0600);
	if (fd < 0) {
		return (0);
	}
	if ((flg = fcntl(fd, F_GETFD)) != -1) {
		fcntl(fd, F_SETFD, flg | FD_CLOEXEC);
	}
	len = 0;
	while (len < size) {
		slen = read(fd, buf, sizeof(buf));
		if (slen <= 0) {
			break;
		}
		RAND_seed(buf, slen);
		len += slen;
	}
	close(fd);

	return (len ? 1 : 0);
}


/**
 * Write of random numbers to a file to store for later initiation with RAND_load_file().
 *
 * @param filename name of file to write.
 *
 * @return 1 on success and non-one on failure.
 */
int
RAND_write_file(const char *filename)
{
	unsigned char buf[128];
	size_t len;
	int res = 0, fd, flg;

	fd = open(filename, O_WRONLY | O_CREAT | O_BINARY, 0600);
	if (fd < 0) {
		return (0);
	}
	if ((flg = fcntl(fd, F_GETFD)) != -1) {
		fcntl(fd, F_SETFD, flg | FD_CLOEXEC);
	}

	len = 0;
	while (len < RAND_FILE_SIZE) {
		res = RAND_bytes(buf, sizeof(buf));
		if (res != 1) {
			break;
		}
		if (write(fd, buf, sizeof(buf)) != sizeof(buf)) {
			res = 0;
			break;
		}
		len += sizeof(buf);
	}

	close(fd);

	return (res);
}


/**
 * Return the default random state filename for a user to use for
 * RAND_load_file(), and RAND_write_file().
 *
 * @param filename buffer to hold file name.
 * @param size size of buffer filename.
 *
 * @return the buffer filename or NULL on failure.
 *
 */
const char *
RAND_file_name(char *filename, size_t size)
{
	const char *e = NULL;
	int pathp = 0, ret;

	if (!issetugid()) {
		e = getenv("RANDFILE");
		if (e == NULL) {
			e = getenv("HOME");
		}
		if (e) {
			pathp = 1;
		}
	}

#ifndef _WIN32

	/*
	 * Here we really want to call getpwuid(getuid()) but this will
	 * cause recursive lookups if the nss library uses
	 * gssapi/krb5/hcrypto to authenticate to the ldap servers.
	 *
	 * So at least return the unix /dev/random if we have one
	 */
	if (e == NULL) {
#if defined(__APPLE_PRIVATE__)
		e = "/dev/random";
#else
		int fd;
		fd = _ossl_unix_device_fd(O_RDONLY, &e);
		if (fd >= 0) {
			close(fd);
		}
#endif
	}
#else   /* Win32 */
	if (e == NULL) {
		char profile[MAX_PATH];

		if (SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL,
		    SHGFP_TYPE_CURRENT, profile) == S_OK) {
			ret = snprintf(filename, size, "%s\\.rnd", profile);

			if ((ret > 0) && (ret < size)) {
				return (filename);
			}
		}
	}
#endif

	if (e == NULL) {
		return (NULL);
	}

	if (pathp) {
		ret = snprintf(filename, size, "%s/.rnd", e);
	} else{
		ret = snprintf(filename, size, "%s", e);
	}

	if ((ret <= 0) || (ret >= size)) {
		return (NULL);
	}

	return (filename);
}
