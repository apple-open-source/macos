/*
 * Copyright (c) 2008-2010 Apple Inc. All Rights Reserved.
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

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <errno.h>
#include <string.h>

#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFString.h>
#include <Security/SecImportExport.h>
#include <Security/SecItem.h>
#include <Security/SecCertificate.h>
#include <Security/SecIdentity.h>
#include <Security/SecTrust.h>
#include <Security/SecInternal.h>
#include <utilities/array_size.h>

#include "SecurityCommands.h"
#include "SecurityTool/print_cert.h"

static void *
read_file(const char * filename, size_t * data_length)
{
    void *		data = NULL;
    int         len = 0;
    int			fd = -1;
    struct stat		sb;

    *data_length = 0;
    if (stat(filename, &sb) < 0)
        goto done;
    if (sb.st_size > INT32_MAX)
        goto done;
    len = (uint32_t)sb.st_size;
    if (len == 0)
        goto done;

    data = malloc(len);
    if (data == NULL)
	goto done;

    fd = open(filename, O_RDONLY);
    if (fd < 0)
	goto done;

    if (read(fd, data, len) != len) {
	goto done;
    }
 done:
    if (fd >= 0)
	close(fd);
    if (data) {
	*data_length = len;
    }
    return (data);
}

static OSStatus
add_cert_item(SecCertificateRef cert)
{
    CFDictionaryRef 	dict;
    OSStatus		status;

    dict = CFDictionaryCreate(NULL, 
			      (const void * *)&kSecValueRef,
			      (const void * *)&cert, 1,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    status = SecItemAdd(dict, NULL);
    CFReleaseNull(dict);
    return (status);
}

static OSStatus
remove_cert_item(SecCertificateRef cert)
{
    CFDictionaryRef 	dict;
    OSStatus		status;

    dict = CFDictionaryCreate(NULL, 
			      (const void * *)&kSecValueRef,
			      (const void * *)&cert, 1,
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    status = SecItemDelete(dict);
    CFReleaseNull(dict);
    if (status == errSecItemNotFound)
        status = errSecSuccess; /* already gone, no problem */
    return (status);
}

static CFArrayRef
PKCS12FileCreateArray(const char * filename, const char * password)
{
    void *		file_data = NULL;
    size_t		file_data_length;
    CFArrayRef 		items = NULL;
    CFDictionaryRef	options = NULL;
    CFDataRef 		pkcs12_data = NULL;
    CFStringRef 	password_cf = NULL;

    file_data = read_file(filename, &file_data_length);
    if (file_data == NULL) {
	int	this_error = errno;

	fprintf(stderr, "failed to read file '%s', %s\n",
		filename, strerror(this_error));
	goto done;
    }
    pkcs12_data = CFDataCreate(NULL, file_data, file_data_length);
    password_cf
	= CFStringCreateWithCString(NULL, password, kCFStringEncodingUTF8);
    
    options = CFDictionaryCreate(NULL, 
				 (const void * *)&kSecImportExportPassphrase,
				 (const void * *)&password_cf, 1, 
				 &kCFTypeDictionaryKeyCallBacks,
				 &kCFTypeDictionaryValueCallBacks);
    if (SecPKCS12Import(pkcs12_data, options, &items) != 0) {
	fprintf(stderr, "failed to import PKCS12 '%s'\n",
		filename);
    }
 done:
    if (file_data != NULL) {
	free(file_data);
    }
    CFReleaseNull(pkcs12_data);
    CFReleaseNull(password_cf);
    CFReleaseNull(options);
    return (items);
}

static void
find_identity_using_handle(CFTypeRef identity_handle)
{
    CFDictionaryRef	dict;
    CFTypeRef 		identity_ref;
    const void * 	keys[] = { kSecClass,
				   kSecReturnRef,
				   kSecValuePersistentRef };
    const void * 	values[] = { kSecClassIdentity,
				     kCFBooleanTrue,
				     identity_handle };
    OSStatus 		status;

    /* find the identity using the persistent handle */
    dict = CFDictionaryCreate(NULL, keys, values,
			      (array_size(keys)),
			      &kCFTypeDictionaryKeyCallBacks,
			      &kCFTypeDictionaryValueCallBacks);
    status = SecItemCopyMatching(dict, &identity_ref);
    CFReleaseNull(dict);
    if (status != errSecSuccess) {
	fprintf(stderr, "SecItemCopyMatching() failed %d\n", 
		(int)status);
    }
    else {
	printf("Found identity:\n");
	fflush(stdout);
	fflush(stderr);
	CFShow(identity_ref);
	CFReleaseNull(identity_ref);
    }
    return;
}

static bool
PKCS12ArrayAddSecItems(CFArrayRef items, bool verbose)
{
    CFIndex		count;
    CFIndex		i;
    bool		success = TRUE;

    count = CFArrayGetCount(items);
    for (i = 0; i < count; i++) {
        SecTrustRef trust_ref;
        SecIdentityRef identity;
        CFDictionaryRef item_dict = CFArrayGetValueAtIndex(items, 0);
        OSStatus	status;

        /* add identity */
        identity = (SecIdentityRef)CFDictionaryGetValue(item_dict, kSecImportItemIdentity);
        if (identity != NULL) {
            if (verbose) {
                SecCertificateRef cert = NULL;
                SecIdentityCopyCertificate(identity, &cert);
                print_cert(cert, false);
                CFReleaseSafe(cert);
            }
            CFDictionaryRef	dict;
            CFTypeRef		identity_handle = NULL;
            const void * 	keys[] = { kSecReturnPersistentRef,
                           kSecValueRef };
            const void * 	values[] = { kCFBooleanTrue,
                             identity };
            dict = CFDictionaryCreate(NULL, 
                          keys, values,
                          array_size(keys),
                          &kCFTypeDictionaryKeyCallBacks,
                          &kCFTypeDictionaryValueCallBacks);
            status = SecItemAdd(dict, &identity_handle);
            if (identity_handle != NULL) {
            find_identity_using_handle(identity_handle);
            }
            CFReleaseNull(identity_handle);
            if (status != errSecSuccess) {
                fprintf(stderr, "SecItemAdd(identity) failed %d\n",
                    (int)status);
                success = FALSE;
            }
            CFReleaseNull(dict);
        }

        /* add certs */
        trust_ref = (SecTrustRef)CFDictionaryGetValue(item_dict, kSecImportItemTrust);
        if (trust_ref != NULL) {
            CFIndex		cert_count;
            CFIndex		cert_index;

            cert_count = SecTrustGetCertificateCount(trust_ref);
            for (cert_index = 1; cert_index < cert_count; cert_index++) {
                SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust_ref, cert_index);
                if (verbose)
                    print_cert(cert, false);
                status = add_cert_item(cert);
                if (status != errSecSuccess) {
                    fprintf(stderr, "add_cert_item %d failed %d\n", (int)cert_index, (int)status);
                    success = FALSE;
                }
            }
        }
    }
    return (success);
}

static bool
PKCS12ArrayRemoveSecItems(CFArrayRef items, bool verbose)
{
    CFIndex		count;
    CFIndex		i;
    bool		success = TRUE;

    count = CFArrayGetCount(items);
    for (i = 0; i < count; i++) {
        CFTypeRef 	cert_chain;
        SecIdentityRef 	identity;
        CFDictionaryRef item_dict = CFArrayGetValueAtIndex(items, i);
        OSStatus	status;

        /* remove identity */
        identity = (SecIdentityRef)CFDictionaryGetValue(item_dict,
            kSecImportItemIdentity);
        if (identity != NULL) {
            if (verbose) {
                SecCertificateRef cert = NULL;
                SecIdentityCopyCertificate(identity, &cert);
                print_cert(cert, false);
                CFReleaseSafe(cert);
            }
            CFDictionaryRef	dict;

            dict = CFDictionaryCreate(NULL, 
                          (const void * *)&kSecValueRef,
                          (const void * *)&identity, 1,
                          &kCFTypeDictionaryKeyCallBacks,
                          &kCFTypeDictionaryValueCallBacks);
            status = SecItemDelete(dict);
            if (status != errSecSuccess) {
            fprintf(stderr, "SecItemDelete(identity) failed %d\n",
                (int)status);
            success = FALSE;
            }
            CFReleaseNull(dict);
        }
        /* remove cert chain */
        cert_chain = CFDictionaryGetValue(item_dict, kSecImportItemCertChain);
        if (cert_chain != NULL) {
            CFIndex		cert_count;
            CFIndex		cert_index;

            cert_count = CFArrayGetCount(cert_chain);
            for (cert_index = 0; cert_index < cert_count; cert_index++) {
                SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(cert_chain, cert_index);
                if (verbose)
                    print_cert(cert, false);
                status = remove_cert_item(cert);
                if (status != errSecSuccess) {
                    fprintf(stderr, "remove_cert_item %d failed %d\n", (int)cert_index, (int)status);
                    success = FALSE;
                }
            }
        }
    }
    return (success);
}


extern int pkcs12_util(int argc, char * const *argv)
{
    CFArrayRef		array;
    const char *	filename = NULL;
    const char *	passphrase = NULL;
    bool            delete = false;
    bool            verbose = false;
    char            ch;
    
    while ((ch = getopt(argc, argv, "p:dv")) != -1)
    {
        switch (ch)
        {
        case 'p':
            passphrase = optarg;
            break;
        case 'd':
            delete = true;
            break;
        case 'v':
            verbose = true;
            break;
        default:
            return 2; /* Trigger usage message. */
        }
    }
    
	argc -= optind;
	argv += optind;

    if (argc != 1 || !passphrase)
        return 2; /* Trigger usage message. */

    filename = argv[0];
    array = PKCS12FileCreateArray(filename, passphrase);
    if (array == NULL)
        return -1;
    
    bool success = false;
    if (delete)
        success = PKCS12ArrayRemoveSecItems(array, verbose);
    else
        success = PKCS12ArrayAddSecItems(array, verbose);

    CFReleaseNull(array);
    
    return success ? 0 : -1;
}

#endif // TARGET_OS_EMBEDDED
