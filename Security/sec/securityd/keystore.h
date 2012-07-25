/*
 * Copyright (c) 2010 Apple Inc. All Rights Reserved.
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

/*!
    @header keystore
    The functions provided in keystore.h provide an interface to
    the AppleKeyStore kext.
*/

#ifndef _SECURITYD_KEYSTORE_H_
#define _SECURITYD_KEYSTORE_H_

#include <IOKit/IOKitLib.h>

#ifdef __cplusplus
/*
 * ks objects are NOT C++ objects. Nevertheless, we can at least keep C++
 * aware of type compatibility.
 */
typedef struct ks_object_s {
private:
	ks_object_s();
	~ks_object_s();
	ks_object_s(const ks_object_s &);
	void operator=(const ks_object_s &);
} *ks_object_t;
#else
typedef union {
	struct ks_object_s *_kso;
	struct ks_key_s *_ksk;
	struct ks_buffer_s *_ksb;
	struct ks_stream_s *_kss;
} ks_object_t __attribute__((transparent_union));
#endif

#ifdef __cplusplus
#define KS_DECL(name) typedef struct name##_s : public ks_object_s {} *name##_t;
#else
/*! @parseOnly */
#define KS_DECL(name) typedef struct name##_s *name##_t;
#endif

KS_DECL(ks_buffer);
KS_DECL(ks_key);
KS_DECL(ks_stream);

#if defined(__cplusplus)
extern "C" {
#endif

enum {
    KS_KEY_SIZE_128 = 16,
    KS_KEY_SIZE_192 = 24,
    KS_KEY_SIZE_256 = 32,
};

ks_key_t ks_generate_key(long size);
void ks_encrypt(ks_key_t key, ks_object_t data_in, ks_object_t data_out);
void ks_decrypt(ks_key_t key, ks_object_t data_in, ks_object_t data_out);

ks_buffer_t ks_buffer(size_t capacity);
size_t ks_get_length(ks_buffer_t buffer);
void ks_set_length(ks_buffer_t buffer, size_t length);
uint8_t *ks_bytes(ks_buffer_t buffer);
ks_buffer_t ks_append(size_t capacity);


/* TODO: Move to iokitutils or something since this is generic. */
io_connect_t ks_connect_to_service(const char *className);

io_connect_t ks_get_connect(void);


/*!
    @function ks_available
    @abstract Check if the AppleKeyStore.kext is available, you must call
    this function before using any other library function.
    @result true, unless for some reason ks isn't available then false.
 */
bool ks_available(void);

/*!
    @function ks_free
    @abstract free something allocated by a ks_ function.
    @param ks_object buffer allocated by the
 */
void ks_free(ks_object_t ks_object);

/*!
    @function ks_unwrap
    @abstract unwrap a key using the specified keyclass.
    @param keybag the keybag handle containing the class key which will be
    doing the wrapping.
    @param keyclass handle for the wrapping key.
    @param bufferSize number of bytes available in array pointed to by buffer
    @param buffer pointer to a buffer.
    @param wrappedKeySize (output) size of the wrappedKey if it had been
    written to buffer.
    @param error (optional) pointer to a CFErrorRef who's value will only be
    changed if it is NULL, in which case the caller is responsible for
    calling CFRelease on it.
    @result Returns pointer to the wrappedKey, or
    NULL if an error occured. Pass in a pointer to a CFErrorRef who's value
    is NULL to obtain an error object.
    @discussion If and only if NULL is passed for the buffer parameter, this
    function will allocate a buffer to which it writes the wrappedKey.
 */
uint8_t *ks_unwrap(uint64_t keybag, uint64_t keyclass,
    const uint8_t *wrappedKey, size_t wrappedKeySize,
    uint8_t *buffer, size_t bufferSize, size_t *keySize);

/*!
    @function ks_wrap
    @abstract wrap a 128 bit (16 byte), 192 bit (24 byte) or 256 bit (32 byte)
    key using the specified keyclass.
    @param keybag the keybag handle containing the class key which will be
    doing the wrapping.
    @param keyclass handle for the wrapping key.
    @param bufferSize number of bytes available in array pointed to by buffer
    @param buffer pointer to a buffer.
    @param wrappedKeySize (output) size of the wrappedKey if it had been
    written to buffer.
    @param error (optional) pointer to a CFErrorRef who's value will only be
    changed if it is NULL, in which case the caller is responsible for
    calling CFRelease on it.
    @result Returns pointer to the wrappedKey, or
    NULL if an error occured. Pass in a pointer to a CFErrorRef who's value
    is NULL to obtain an error object.
    @discussion If and only if NULL is passed for the buffer parameter, this
    function will allocate a buffer to which it writes the wrappedKey.
 */
uint8_t *ks_wrap(uint64_t keybag, uint64_t keyclass,
    const uint8_t *key, size_t keyByteSize,
    uint8_t *buffer, size_t bufferSize, size_t *wrappedKeySize);

#if defined(__cplusplus)
}
#endif

#endif /* _SECURITYD_KEYSTORE_H_ */
