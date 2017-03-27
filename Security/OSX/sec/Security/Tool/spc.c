/*
 * Copyright (c) 2009-2010,2013-2014 Apple Inc. All Rights Reserved.
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
 *  spc.c - Security
 *
 *
 */

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED

#include "SecurityCommands.h"

#include <CoreFoundation/CoreFoundation.h>
#include <AssertMacros.h>
#include <TargetConditionals.h>
#include <CFNetwork/CFNetwork.h>

extern const CFStringRef kCFStreamPropertySSLPeerTrust;
#include <Security/SecTrust.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecImportExport.h>
#include <Security/SecPolicy.h>
#include <Security/SecIdentity.h>
#include <Security/SecIdentityPriv.h>
#include <Security/SecRSAKey.h>
#include <Security/SecSCEP.h>
#include <Security/SecCMS.h>
#include <Security/SecItem.h>
#include <Security/SecInternal.h>
#include <utilities/array_size.h>
#include <err.h>
#include <stdio.h>
#include <getopt.h>

//static char *scep_url = "https://localhost:2000/cgi-bin/pkiclient.exe";
static char *user = "webuser";
static char *pass = "webpassowrd";
static int debug = 0;

#define BUFSIZE 1024

unsigned char device_cert_der[] = {
  0x30, 0x82, 0x02, 0xdf, 0x30, 0x82, 0x02, 0x48, 0xa0, 0x03, 0x02, 0x01,
  0x02, 0x02, 0x0a, 0x01, 0x5c, 0x0a, 0x2f, 0xfd, 0x20, 0x02, 0x00, 0xe6,
  0x27, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x01, 0x0b, 0x05, 0x00, 0x30, 0x5a, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03,
  0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06,
  0x03, 0x55, 0x04, 0x0a, 0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20,
  0x49, 0x6e, 0x63, 0x2e, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04,
  0x0b, 0x13, 0x0c, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x69, 0x50, 0x68,
  0x6f, 0x6e, 0x65, 0x31, 0x1f, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x04, 0x03,
  0x13, 0x16, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x69, 0x50, 0x68, 0x6f,
  0x6e, 0x65, 0x20, 0x44, 0x65, 0x76, 0x69, 0x63, 0x65, 0x20, 0x43, 0x41,
  0x30, 0x1e, 0x17, 0x0d, 0x30, 0x38, 0x31, 0x32, 0x31, 0x32, 0x32, 0x33,
  0x30, 0x33, 0x35, 0x36, 0x5a, 0x17, 0x0d, 0x31, 0x31, 0x31, 0x32, 0x31,
  0x32, 0x32, 0x33, 0x30, 0x33, 0x35, 0x36, 0x5a, 0x30, 0x81, 0x87, 0x31,
  0x31, 0x30, 0x2f, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x28, 0x66, 0x31,
  0x66, 0x35, 0x30, 0x66, 0x38, 0x31, 0x32, 0x32, 0x66, 0x62, 0x31, 0x30,
  0x31, 0x34, 0x36, 0x66, 0x36, 0x35, 0x65, 0x39, 0x30, 0x33, 0x38, 0x30,
  0x31, 0x37, 0x36, 0x33, 0x34, 0x32, 0x61, 0x64, 0x64, 0x36, 0x61, 0x38,
  0x35, 0x63, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
  0x02, 0x55, 0x53, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08,
  0x13, 0x02, 0x43, 0x41, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
  0x07, 0x13, 0x09, 0x43, 0x75, 0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f,
  0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x0a, 0x41,
  0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31, 0x0f, 0x30,
  0x0d, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x06, 0x69, 0x50, 0x68, 0x6f,
  0x6e, 0x65, 0x30, 0x81, 0x9f, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48,
  0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x81, 0x8d, 0x00,
  0x30, 0x81, 0x89, 0x02, 0x81, 0x81, 0x00, 0xba, 0xc5, 0x2a, 0x7b, 0xb3,
  0x29, 0x21, 0x8d, 0xbd, 0x01, 0x12, 0xff, 0x0b, 0xf6, 0x14, 0x66, 0x15,
  0x7b, 0x99, 0xf8, 0x1e, 0x08, 0x46, 0x40, 0xd7, 0x48, 0x57, 0xf0, 0x9c,
  0x08, 0xac, 0xbc, 0x93, 0xc3, 0xd8, 0x81, 0xd8, 0x52, 0xd7, 0x9e, 0x9b,
  0x3e, 0x65, 0xef, 0x98, 0x8d, 0x93, 0x2e, 0x79, 0x58, 0x22, 0x98, 0xe2,
  0xd9, 0x2f, 0x84, 0x6d, 0xf0, 0x50, 0xc6, 0xba, 0x52, 0x28, 0x29, 0xf1,
  0x83, 0x4c, 0xad, 0xdd, 0x5a, 0x7a, 0xab, 0xd2, 0x74, 0xb2, 0x1d, 0xc1,
  0xa9, 0xe4, 0x87, 0xbd, 0xa0, 0x94, 0x0e, 0x50, 0xa7, 0xc7, 0xf1, 0x0d,
  0xda, 0x0c, 0x36, 0x94, 0xf4, 0x2c, 0xb8, 0x76, 0xc7, 0x7b, 0x32, 0x87,
  0x23, 0x70, 0x9f, 0x3f, 0x19, 0xeb, 0xc3, 0xb5, 0x3a, 0x6e, 0xec, 0xa7,
  0x9c, 0xb4, 0xdb, 0xe7, 0x6e, 0x6d, 0x5e, 0x3c, 0x14, 0xa1, 0xa6, 0xfb,
  0x50, 0xfb, 0x17, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x7e, 0x30, 0x7c,
  0x30, 0x1b, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x14, 0xb2, 0xfe, 0x21,
  0x23, 0x44, 0x86, 0x95, 0x6a, 0x79, 0xd5, 0x81, 0x26, 0x8e, 0x73, 0x10,
  0xd8, 0xa7, 0x4c, 0x8e, 0x74, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e,
  0x04, 0x16, 0x04, 0x14, 0x33, 0x2f, 0xbd, 0x38, 0x86, 0xbc, 0xb0, 0xb2,
  0x6a, 0xd3, 0x86, 0xca, 0x92, 0x7c, 0x0a, 0x6d, 0x55, 0xab, 0x34, 0x81,
  0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x01, 0x01, 0xff, 0x04, 0x02,
  0x30, 0x00, 0x30, 0x0e, 0x06, 0x03, 0x55, 0x1d, 0x0f, 0x01, 0x01, 0xff,
  0x04, 0x04, 0x03, 0x02, 0x05, 0xa0, 0x30, 0x20, 0x06, 0x03, 0x55, 0x1d,
  0x25, 0x01, 0x01, 0xff, 0x04, 0x16, 0x30, 0x14, 0x06, 0x08, 0x2b, 0x06,
  0x01, 0x05, 0x05, 0x07, 0x03, 0x01, 0x06, 0x08, 0x2b, 0x06, 0x01, 0x05,
  0x05, 0x07, 0x03, 0x02, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
  0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x81, 0x81, 0x00, 0xe2,
  0x0c, 0x37, 0x3e, 0x5f, 0x96, 0x10, 0x0d, 0xea, 0xa5, 0x15, 0x8f, 0xa2,
  0x5b, 0x01, 0xe0, 0x1f, 0x15, 0x11, 0x42, 0x4b, 0x9e, 0x4b, 0x93, 0x90,
  0x56, 0x4f, 0x4f, 0xbb, 0xc7, 0x32, 0x74, 0xfc, 0xbd, 0x1c, 0xcc, 0x06,
  0x9f, 0xbb, 0x0b, 0x11, 0xa4, 0x8b, 0xa0, 0x52, 0x59, 0x94, 0xef, 0x87,
  0x0c, 0xb1, 0xa8, 0xe0, 0x85, 0x96, 0xe1, 0x1b, 0x8b, 0x56, 0x12, 0x4d,
  0x94, 0xd6, 0xa5, 0x52, 0x7f, 0x65, 0xf3, 0x21, 0x1f, 0xaa, 0x07, 0x8c,
  0xaf, 0x69, 0xfa, 0x47, 0xbe, 0x3b, 0x74, 0x1a, 0x7c, 0xa6, 0x25, 0x63,
  0x18, 0x5f, 0x0d, 0xda, 0xc4, 0x58, 0x01, 0xbc, 0xf2, 0x6d, 0x2d, 0xc1,
  0x68, 0x3e, 0xa1, 0x2c, 0x59, 0x03, 0x3e, 0xa3, 0x13, 0x1b, 0xd9, 0x42,
  0x28, 0x1e, 0x6c, 0x7f, 0x7f, 0x9d, 0x29, 0xf6, 0x43, 0x84, 0xe7, 0x60,
  0xe3, 0x6f, 0x6a, 0x8a, 0x9f, 0x1d, 0x70
};
unsigned int device_cert_der_len = 739;

unsigned char device_key[] = {
  0x30, 0x82, 0x02, 0x5c, 0x02, 0x01, 0x00, 0x02, 0x81, 0x81, 0x00, 0xba,
  0xc5, 0x2a, 0x7b, 0xb3, 0x29, 0x21, 0x8d, 0xbd, 0x01, 0x12, 0xff, 0x0b,
  0xf6, 0x14, 0x66, 0x15, 0x7b, 0x99, 0xf8, 0x1e, 0x08, 0x46, 0x40, 0xd7,
  0x48, 0x57, 0xf0, 0x9c, 0x08, 0xac, 0xbc, 0x93, 0xc3, 0xd8, 0x81, 0xd8,
  0x52, 0xd7, 0x9e, 0x9b, 0x3e, 0x65, 0xef, 0x98, 0x8d, 0x93, 0x2e, 0x79,
  0x58, 0x22, 0x98, 0xe2, 0xd9, 0x2f, 0x84, 0x6d, 0xf0, 0x50, 0xc6, 0xba,
  0x52, 0x28, 0x29, 0xf1, 0x83, 0x4c, 0xad, 0xdd, 0x5a, 0x7a, 0xab, 0xd2,
  0x74, 0xb2, 0x1d, 0xc1, 0xa9, 0xe4, 0x87, 0xbd, 0xa0, 0x94, 0x0e, 0x50,
  0xa7, 0xc7, 0xf1, 0x0d, 0xda, 0x0c, 0x36, 0x94, 0xf4, 0x2c, 0xb8, 0x76,
  0xc7, 0x7b, 0x32, 0x87, 0x23, 0x70, 0x9f, 0x3f, 0x19, 0xeb, 0xc3, 0xb5,
  0x3a, 0x6e, 0xec, 0xa7, 0x9c, 0xb4, 0xdb, 0xe7, 0x6e, 0x6d, 0x5e, 0x3c,
  0x14, 0xa1, 0xa6, 0xfb, 0x50, 0xfb, 0x17, 0x02, 0x03, 0x01, 0x00, 0x01,
  0x02, 0x81, 0x80, 0x47, 0x9e, 0x91, 0xc2, 0xeb, 0x99, 0xeb, 0x26, 0xfa,
  0x02, 0x2e, 0x71, 0xa4, 0xf9, 0x91, 0x2a, 0xf0, 0x33, 0xfc, 0x7f, 0xdb,
  0xac, 0x5a, 0x9c, 0x44, 0xb1, 0x96, 0x1f, 0x4b, 0x06, 0x3c, 0x8e, 0xf7,
  0xae, 0xd3, 0x18, 0x3f, 0x86, 0xcc, 0xee, 0x22, 0x23, 0xd4, 0x5d, 0x03,
  0x47, 0xce, 0xd7, 0xb4, 0x6a, 0x6a, 0xa1, 0xeb, 0xe3, 0x52, 0xc8, 0x5a,
  0x8c, 0x1b, 0xbd, 0x88, 0xf7, 0x36, 0x34, 0xd2, 0xfe, 0x6b, 0x75, 0x9c,
  0xa4, 0x97, 0x2f, 0xeb, 0xa8, 0xec, 0x48, 0xb7, 0xe7, 0x53, 0x8f, 0xf1,
  0x39, 0xca, 0xf8, 0xd2, 0xee, 0x34, 0xdc, 0x10, 0x99, 0x6f, 0xfd, 0x83,
  0x21, 0xa9, 0xe0, 0xe9, 0x0c, 0x4e, 0x4e, 0x62, 0x5f, 0x9f, 0x0f, 0x05,
  0xcd, 0xf4, 0x2b, 0x08, 0x06, 0x93, 0x55, 0x76, 0xaf, 0x63, 0x77, 0x80,
  0x3e, 0xd5, 0xf2, 0xe0, 0x58, 0x7f, 0x3c, 0x41, 0x88, 0x4b, 0xc1, 0x02,
  0x41, 0x01, 0x84, 0xfc, 0xa9, 0x26, 0x74, 0xe7, 0x4e, 0xe4, 0x39, 0xc4,
  0x8f, 0x81, 0xe8, 0xc0, 0xd3, 0x6e, 0x01, 0x12, 0x7e, 0x12, 0x2d, 0x61,
  0xaf, 0xe8, 0x60, 0x65, 0x8b, 0x50, 0x46, 0xdf, 0x02, 0x42, 0xe1, 0xea,
  0xc1, 0x57, 0x9a, 0x2d, 0x90, 0xdc, 0x10, 0xf1, 0x79, 0xf0, 0xb5, 0x5c,
  0x89, 0xef, 0x3b, 0xa8, 0x7a, 0x88, 0x3d, 0x54, 0xde, 0x35, 0x9c, 0xc3,
  0x38, 0x4f, 0x2e, 0xb0, 0x74, 0xf7, 0x02, 0x40, 0x7a, 0xea, 0xc9, 0xfe,
  0x48, 0xeb, 0x94, 0x20, 0x50, 0x33, 0x9d, 0x4f, 0x6f, 0x7f, 0xb1, 0x0e,
  0xee, 0x42, 0x1c, 0xae, 0x72, 0x06, 0xb9, 0x9c, 0x80, 0x4a, 0xed, 0x07,
  0xf8, 0x76, 0x5b, 0x37, 0x81, 0x45, 0x69, 0x09, 0xa5, 0x0d, 0x92, 0xed,
  0xa8, 0xf0, 0x05, 0x6d, 0xb3, 0xa4, 0xef, 0xfb, 0x1b, 0xf0, 0x89, 0xea,
  0x80, 0x2d, 0xaf, 0x9d, 0xbe, 0xc0, 0x08, 0x44, 0x58, 0x61, 0xc2, 0xe1,
  0x02, 0x41, 0x01, 0x31, 0xaf, 0x14, 0x86, 0x82, 0x2c, 0x1c, 0x55, 0x42,
  0x08, 0x73, 0xf6, 0x55, 0x20, 0xe3, 0x86, 0x79, 0x15, 0x3d, 0x39, 0xaf,
  0xac, 0x2a, 0xfe, 0xe4, 0x72, 0x28, 0x2e, 0xe7, 0xe2, 0xec, 0xf5, 0xfe,
  0x6f, 0xeb, 0x8c, 0x9a, 0x3e, 0xe0, 0xad, 0xf0, 0x2a, 0xb3, 0xf7, 0x33,
  0xaf, 0x0b, 0x3e, 0x93, 0x95, 0x6c, 0xe5, 0x8f, 0xbd, 0x17, 0xfa, 0xed,
  0xbc, 0x84, 0x8d, 0xc5, 0x55, 0x2a, 0x35, 0x02, 0x40, 0x6b, 0x4e, 0x1f,
  0x4b, 0x03, 0x63, 0xcd, 0xab, 0xab, 0xf8, 0x73, 0x43, 0x8e, 0xa6, 0x1d,
  0xef, 0x57, 0xe6, 0x95, 0x5d, 0x61, 0x24, 0x27, 0xd3, 0xcd, 0x58, 0x1b,
  0xb7, 0x92, 0x9b, 0xd8, 0xa4, 0x0b, 0x11, 0x8a, 0x52, 0x26, 0x2a, 0x44,
  0x73, 0x7f, 0xc1, 0x12, 0x2c, 0x23, 0xe1, 0x40, 0xb3, 0xaa, 0x3f, 0x82,
  0x57, 0x1a, 0xd1, 0x47, 0x77, 0xe1, 0xb7, 0x89, 0x40, 0x09, 0x1c, 0x47,
  0x61, 0x02, 0x41, 0x00, 0xa4, 0x47, 0x7d, 0x98, 0x74, 0x25, 0x95, 0x5a,
  0xc9, 0xbe, 0x76, 0x66, 0xf8, 0x24, 0xb0, 0x83, 0x49, 0xd2, 0xce, 0x98,
  0x75, 0x7c, 0xbb, 0xd9, 0x24, 0xe9, 0xc5, 0x53, 0xea, 0xd8, 0xec, 0x94,
  0x79, 0xbb, 0x4e, 0x96, 0xc6, 0xd6, 0x17, 0x26, 0x77, 0xf3, 0x12, 0x3e,
  0x15, 0x8e, 0x0c, 0x86, 0xdf, 0xa9, 0xf1, 0x34, 0x9b, 0x49, 0x28, 0x0a,
  0x95, 0xb5, 0x29, 0x8f, 0x8a, 0x21, 0xff, 0xfc
};
unsigned int device_key_len = 608;


static inline CFTypeRef valid_cf_obj(CFTypeRef obj, CFTypeID type)
{
    if (obj && CFGetTypeID(obj) == type)
        return obj;
    return NULL;
}

static inline CFTypeRef dict_get_value_of_type(CFDictionaryRef dict, CFTypeRef key, CFTypeID value_type)
{
    CFTypeRef value = CFDictionaryGetValue(dict, key);
    return valid_cf_obj(value, value_type);
}

#include <fcntl.h>
static inline void write_data(const char * path, CFDataRef data)
{
    int data_file = open(path, O_CREAT|O_WRONLY|O_TRUNC, 0644);
    write(data_file, CFDataGetBytePtr(data), CFDataGetLength(data));
    close(data_file);
    printf("wrote intermediate result to: %s\n", path);
}

static void _query_string_apply(const void *key, const void *value, void *context)
{
    CFStringRef escaped_key = 
        CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, 
            (CFStringRef)key, NULL, NULL, kCFStringEncodingUTF8);
    CFStringRef escaped_value = 
        CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault, 
            (CFStringRef)value, NULL, NULL, kCFStringEncodingUTF8);
    CFMutableStringRef query_string = (CFMutableStringRef)context;
    
    CFStringRef format;
    if (CFStringGetLength(query_string) > 1)
        format = CFSTR("&%@=%@");
    else
        format = CFSTR("%@=%@");

    CFStringAppendFormat(query_string, NULL, format, escaped_key, escaped_value);
    CFRelease(escaped_key);
    CFRelease(escaped_value);
}

static CF_RETURNS_RETAINED CFURLRef build_url(CFStringRef base, CFDictionaryRef info)
{
    CFURLRef url = NULL, base_url = NULL;
    CFMutableStringRef query_string = 
        CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("?"));
    require(query_string, out);
    if (info)
        CFDictionaryApplyFunction(info, _query_string_apply, query_string);
    base_url = CFURLCreateWithString(kCFAllocatorDefault, base, NULL);
    url = CFURLCreateWithString(kCFAllocatorDefault, query_string, base_url);
out:
    CFReleaseSafe(query_string);
    CFReleaseSafe(base_url);
    return url;
}

static CF_RETURNS_RETAINED CFURLRef scep_url_operation(CFStringRef base, CFStringRef operation, CFStringRef message)
{
    CFURLRef url = NULL;
    const void *keys[] = { CFSTR("operation"), CFSTR("message") };
    const void *values[] = { operation, message };
    
    require(operation, out);
    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values, message ? 2 : 1, NULL, NULL);
    url = build_url(base, dict);
    CFRelease(dict);
out:
    return url;
}

static bool auth_failed(CFHTTPMessageRef request, CFReadStreamRef readStream)
{
    bool isAuthenticationChallenge = false;
    CFHTTPMessageRef responseHeaders = (CFHTTPMessageRef)CFReadStreamCopyProperty(readStream, kCFStreamPropertyHTTPResponseHeader);
    if (responseHeaders) {
        // Is the server response an challenge for credentials?
        isAuthenticationChallenge = (CFHTTPMessageGetResponseStatusCode(responseHeaders) == 401);
        if (isAuthenticationChallenge) {
            CFStringRef cf_user = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
            CFStringRef cf_pass = CFStringCreateWithCString(kCFAllocatorDefault, pass, kCFStringEncodingUTF8);
            CFHTTPMessageAddAuthentication(request, responseHeaders, cf_user, cf_pass, NULL, false);
            CFReleaseSafe(cf_pass);
            CFReleaseSafe(cf_user);
        }
        CFRelease(responseHeaders);
    }
    return isAuthenticationChallenge;
}

static CF_RETURNS_RETAINED CFHTTPMessageRef load_request(CFHTTPMessageRef request, CFMutableDataRef data, int retry)
{
    CFHTTPMessageRef result = NULL;
    
    if (retry < 0)
        return result;

    CFReadStreamRef rs;
    rs = CFReadStreamCreateForHTTPRequest(NULL, request);

    const void *keys[] = { 
        kCFStreamSSLValidatesCertificateChain, 
//        kCFStreamSSLCertificates,
    };
    //CFArrayRef ident = mit_identity();
    const void *values[] = {
        kCFBooleanFalse, 
//        ident,
    };

    CFDictionaryRef dict = CFDictionaryCreate(NULL, keys, values,
        array_size(keys),
        &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    CFReadStreamSetProperty(rs, kCFStreamPropertySSLSettings, dict);
    CFRelease(dict);
    CFReadStreamSetProperty(rs, kCFStreamPropertyHTTPAttemptPersistentConnection, kCFBooleanTrue);
    
    if (CFReadStreamOpen(rs)) {
        do {
            if (auth_failed(request, rs)) {
                CFReadStreamClose(rs);
                CFDataSetLength(data, 0);
                if (retry) {
                    CFReleaseSafe(rs);
                    return load_request(request, data, retry - 1);
                }
                break;
            }
            UInt8 buf[BUFSIZE];
            CFIndex bytesRead = CFReadStreamRead(rs, buf, BUFSIZE);
            if (bytesRead > 0) {
                CFDataAppendBytes(data, buf, bytesRead);
            } else if (bytesRead == 0) {
                // Success
                result = (CFHTTPMessageRef)CFReadStreamCopyProperty(rs, kCFStreamPropertyHTTPResponseHeader);
                break;
            } else {
                // Error
                break;
            }
        } while (true);
    } else {
        // Error
    }

    CFReadStreamClose(rs);
    CFRelease(rs);
    if (debug)
        CFShow(result);
    return result;
}


static CF_RETURNS_RETAINED CFDictionaryRef get_encrypted_profile_packet(CFStringRef base_url)
{
#if 1
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, 
        device_cert_der, device_cert_der_len);
    SecKeyRef key = SecKeyCreateRSAPrivateKey(NULL, 
        device_key, device_key_len, kSecKeyEncodingPkcs1);
    SecIdentityRef identity = SecIdentityCreate(kCFAllocatorDefault, cert, key);
    CFRelease(cert);
    CFRelease(key);
#else
    SecIdentityRef identity = lockdown_copy_device_identity();
#endif

    CFMutableDictionaryRef machine_dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(machine_dict, CFSTR("UDID"), CFSTR("f1f50f8122fb10146f65e90380176342add6a85c"));
    CFStringRef cf_user = CFStringCreateWithCString(kCFAllocatorDefault, user, kCFStringEncodingUTF8);
    if (cf_user) {
        CFDictionarySetValue(machine_dict, CFSTR("USERNAME"), cf_user);
        CFRelease(cf_user);
    }
    CFStringRef cf_pass = CFStringCreateWithCString(kCFAllocatorDefault, pass, kCFStringEncodingUTF8);
    if (cf_pass) {
        CFDictionarySetValue(machine_dict, CFSTR("PASSWORD"), cf_pass);
        CFRelease(cf_pass);
    }
    CFDataRef machine_dict_data = CFPropertyListCreateXMLData(kCFAllocatorDefault, 
        (CFPropertyListRef)machine_dict);
    CFMutableDataRef signed_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    if (SecCMSSignDataAndAttributes(identity, machine_dict_data, false, signed_data, NULL))
        errx(1, "failed to sign data");
    CFRelease(identity);
    CFRelease(machine_dict_data);
    CFRelease(machine_dict);

    CFURLRef url = build_url(base_url, NULL);
    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(kCFAllocatorDefault,
        CFSTR("POST"), url, kCFHTTPVersion1_1);
    CFRelease(url);
    CFHTTPMessageSetBody(request, signed_data);
    CFRelease(signed_data);
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("Content-Type"), 
        CFSTR("application/pkcs7-signature"));
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFHTTPMessageRef response = load_request(request, data, 1);
    CFRelease(request);
    CFRelease(response);
    CFErrorRef error = NULL;
    CFTypeRef result = CFPropertyListCreateWithData(kCFAllocatorDefault,
        data, kCFPropertyListImmutable, NULL, &error);
    CFRelease(data);
    if (error) {
        CFShow(error);
        errx(1, "failed to decode encrypted profile response");
    }
    if (CFGetTypeID(result) != CFDictionaryGetTypeID())
    CFReleaseNull(result);
    return (CFDictionaryRef)result;
}

static SecCertificateRef get_ca_cert(CFStringRef scep_base_url, CFStringRef scep_ca_name)
{
    SecCertificateRef cert = NULL;
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);

    CFURLRef url = scep_url_operation(scep_base_url, CFSTR("GetCACert"), scep_ca_name);
    CFHTTPMessageRef request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, 
        CFSTR("GET"), url, kCFHTTPVersion1_1);
    CFReleaseSafe(url);
    CFHTTPMessageRef result = load_request(request, data, 1);
    CFReleaseSafe(request);

    if (!result) {
        CFReleaseSafe(data);
        return NULL;
    }
    CFStringRef contentTypeValue = CFHTTPMessageCopyHeaderFieldValue(result, CFSTR("Content-Type"));
    CFReleaseSafe(result);
//    CFHTTPMessageRef response = CFHTTPMessageCreateEmpty(kCFAllocatorDefault, false);
//    CFHTTPMessageAppendBytes(response, CFDataGetBytePtr(data), CFDataGetLength(data));
//    CFDataRef bodyValue = CFHTTPMessageCopyBody(response);
    if (kCFCompareEqualTo == CFStringCompare(CFSTR("application/x-x509-ca-cert"), 
        contentTypeValue, kCFCompareCaseInsensitive)) {
        // CA only response: application/x-x509-ca-cert
        cert = SecCertificateCreateWithData(kCFAllocatorDefault, data);
        if (debug)
            write_data("/tmp/ca_cert.der", data);
    } else if (kCFCompareEqualTo == CFStringCompare(CFSTR("application/x-x509-ca-ra-cert"), 
        contentTypeValue, kCFCompareCaseInsensitive)) {
        // RA/CA response: application/x-x509-ca-ra-cert
        CFArrayRef cert_array = SecCMSCertificatesOnlyMessageCopyCertificates(data);
        if (debug)
            write_data("/tmp/ca_cert_array.der", data);
        cert = (SecCertificateRef)CFArrayGetValueAtIndex(cert_array, 0);
        CFRetain(cert);
        CFRelease(cert_array);
    }
    CFRelease(contentTypeValue);
    CFRelease(data);
    return cert;
}


static SecIdentityRef perform_scep(CFDictionaryRef scep_dict)
{
    SecIdentityRef identity = NULL;
    CFDictionaryRef parameters = NULL, csr_parameters = NULL;
    CFStringRef scep_base_url = NULL, scep_instance_name = NULL,
        scep_challenge = NULL, scep_subject = NULL, scep_subject_requested = NULL;
    CFTypeRef scep_key_bitsize = NULL;
    CFNumberRef key_usage_num = NULL;
    SecCertificateRef ca_cert = NULL;
    const void *keygen_keys[] = { kSecAttrKeyType, kSecAttrKeySizeInBits };
    const void *keygen_vals[] = { kSecAttrKeyTypeRSA, CFSTR("512") };

    require(valid_cf_obj(scep_dict,CFDictionaryGetTypeID()), out);
    require(scep_base_url = 
        dict_get_value_of_type(scep_dict, CFSTR("URL"), CFStringGetTypeID()), out);
    require(scep_instance_name = 
        dict_get_value_of_type(scep_dict, CFSTR("NAME"), CFStringGetTypeID()), out);
    require(scep_challenge = 
        dict_get_value_of_type(scep_dict, CFSTR("CHALLENGE"), CFStringGetTypeID()), out);
        
    scep_subject_requested = 
        dict_get_value_of_type(scep_dict, CFSTR("SUBJECT"), CFStringGetTypeID());
    if (scep_subject_requested) {
        CFArrayRef scep_subject_components = 
            CFStringCreateArrayBySeparatingStrings(kCFAllocatorDefault, 
                scep_subject_requested, CFSTR("="));
        if (CFArrayGetCount(scep_subject_components) == 2)
            scep_subject = CFArrayGetValueAtIndex(scep_subject_components, 1);
    }

    scep_key_bitsize =
        dict_get_value_of_type(scep_dict, CFSTR("KEYSIZE"), CFNumberGetTypeID());
    if (!scep_key_bitsize)
        scep_key_bitsize =
            dict_get_value_of_type(scep_dict, CFSTR("KEYSIZE"), CFStringGetTypeID());
    if (scep_key_bitsize)
        keygen_vals[1] = scep_key_bitsize;
        
    parameters = CFDictionaryCreate(kCFAllocatorDefault, 
        keygen_keys, keygen_vals, array_size(keygen_vals),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks );

    int key_usage = kSecKeyUsageDigitalSignature | kSecKeyUsageKeyEncipherment;
    key_usage_num = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &key_usage);
    const void *key[] = { kSecCSRChallengePassword, kSecCertificateKeyUsage };
    const void *val[] = { scep_challenge ? scep_challenge : CFSTR("magic"), key_usage_num };
    csr_parameters = CFDictionaryCreate(kCFAllocatorDefault, key, val, array_size(key), NULL, NULL);

    ca_cert = get_ca_cert(scep_base_url, scep_instance_name ? scep_instance_name : CFSTR("test"));
    if (!ca_cert)
        errx(1, "no ca cert returned from scep server.");

    identity = NULL; // enroll_scep(scep_base_url, scep_subject, csr_parameters, parameters, ca_cert);
    if (!identity)
        errx(1, "failed to get identity from scep server.");
    

out:
    CFReleaseSafe(ca_cert);
    CFReleaseSafe(scep_subject);
    CFReleaseSafe(key_usage_num);
    CFReleaseSafe(csr_parameters);
    CFReleaseSafe(parameters);
    return identity;
}

static CFDataRef CF_RETURNS_RETAINED get_profile(CFStringRef url_cfstring, SecIdentityRef identity)
{
    CFURLRef url = NULL;
    CFHTTPMessageRef request = NULL;
    SecCertificateRef cert = NULL;
    CFDataRef cert_data = NULL;
    CFMutableDataRef data = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFHTTPMessageRef response = NULL;
    
    require(url = CFURLCreateWithString(kCFAllocatorDefault, url_cfstring, NULL), out);
    require(request = CFHTTPMessageCreateRequest(kCFAllocatorDefault, 
        CFSTR("POST"), url, kCFHTTPVersion1_1), out);
    require_noerr(SecIdentityCopyCertificate(identity, &cert), out);
    require(cert_data = SecCertificateCopyData(cert), out);
    CFHTTPMessageSetBody(request, cert_data);
    // this is technically the wrong mimetype; we'll probably switch to signed data
    CFHTTPMessageSetHeaderFieldValue(request, CFSTR("Content-Type"), CFSTR("application/x-x509-ca-cert"));
    response = load_request(request, data, 1);
    if (debug && data)
        CFShow(data);
out:
    //CFReleaseSafe(response);
    CFReleaseSafe(request);
    CFReleaseSafe(url);
    CFReleaseSafe(cert_data);
    CFReleaseSafe(cert);
    CFReleaseSafe(response);
    return data;

}

static bool validate_profile(CFDataRef profile_plist_data, SecIdentityRef identity)
{
    CFStringRef type = NULL, uuid = NULL;
    CFMutableDataRef dec_data = NULL;
    CFDataRef enc_payload = NULL;
    bool ok = false;
    CFTypeRef result = CFPropertyListCreateWithData(kCFAllocatorDefault,
        profile_plist_data, kCFPropertyListImmutable, NULL, NULL);
    require(valid_cf_obj(result, CFDictionaryGetTypeID()), out);
    require(type = dict_get_value_of_type(result, CFSTR("PayloadType"), 
        CFStringGetTypeID()), out);
    require(uuid = dict_get_value_of_type(result, CFSTR("PayloadUUID"), 
        CFStringGetTypeID()), out);
    enc_payload = dict_get_value_of_type(result, CFSTR("EncryptedPayloadContent"), 
        CFDataGetTypeID());
    if (enc_payload) {
        dec_data = CFDataCreateMutable(kCFAllocatorDefault, 0);
        require_noerr(SecCMSDecryptEnvelopedData(enc_payload, dec_data, NULL), out);
        ok = validate_profile(dec_data, identity);
    } else {
        ok = true;
        if (debug)
            write_data("/tmp/unencrypted_profile.mobileconfig", profile_plist_data);
        //CFDictionaryRef sub_conf = dict_get_value_of_type(result, CFSTR("PayloadContent"), CFDictionaryGetTypeID());
    }
out:
    CFReleaseSafe(dec_data);
    CFReleaseSafe(result);
    return ok;
}

#if 0
static const char *auth = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n\
<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/\n\
PropertyList-1.0.dtd\">\n\
<plist version=\"1.0\">\n\
<dict>\n\
        <key>PayloadContent</key>\n\
        <dict>\n\
                <key>URL</key>\n\
                <string>https://phone.vpn.apple.com/deviceid/</string>\n\
                <key>DeviceAttributes</key>\n\
                <array>\n\
                        <string>UDID</string>\n\
                        <string>VERSION</string>\n\
                        <string>MAC_ADDRESS_EN0</string>\n\
                        <string>MAC_ADDRESS_IP0</string>\n\
                </array>\n\
	       <key>CHALLENGE</key>\n\
	       <string>${USER_COOKIE}</string>\n\
        </dict>\n\
        <key>PayloadType</key>\n\
        <string>Device Identification</string>\n\
</dict>\n\
</plist>\n";

static void annotate_machine_info(const void *value, void *context)
{
    CFDictionaryRef machine_dict = NULL;
    CFStringRef machine_key = NULL;
    require(machine_dict = (CFDictionaryRef)valid_cf_obj(context, 
        CFDictionaryGetTypeID()), out);
    require(machine_key = (CFStringRef)valid_cf_obj((CFTypeRef)value, 
        CFStringGetTypeID()), out);
    if (CFEqual(machine_key, CFSTR("UDID"))) {
    
    }
out:
    return;
}

static void machine_authentication(CFDataRef req, CFDataRef reply)
{
    CFDataRef machine_auth_request = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, 
        (uint8_t*)auth, strlen(auth), kCFAllocatorNull);
    CFDictionaryRef auth_dict = NULL;
    CFMutableDictionaryRef auth_reply_dict = 
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0, 
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        
    if (req) { CFRelease(machine_auth_request); machine_auth_request = req; CFRetain(req); }
    CFDictionaryRef auth_request_dict = CFPropertyListCreateWithData(kCFAllocatorDefault,
        machine_auth_request, kCFPropertyListImmutable, NULL, NULL);
    
    require(auth_reply_dict, out);
    require(valid_cf_obj(auth_request_dict, CFDictionaryGetTypeID()), out);
    require(auth_dict = dict_get_value_of_type(auth_request_dict, CFSTR("PayloadContent"), CFDictionaryGetTypeID()), out);
    CFStringRef challenge = dict_get_value_of_type(auth_dict, CFSTR("CHALLENGE"), CFStringGetTypeID());
    if (challenge)
        CFDictionarySetValue(auth_reply_dict, CFSTR("CHALLENGE"), challenge);
    CFArrayRef device_info = dict_get_value_of_type(auth_dict, CFSTR("DeviceAttributes"), CFArrayGetTypeID());
    if (device_info)
        CFArrayApplyFunction(device_info, CFRangeMake(0, CFArrayGetCount(device_info)), 
            annotate_machine_info, auth_reply_dict);
    
    // make copy of reply dict
out:
    CFReleaseSafe(machine_auth_request);
    CFReleaseSafe(auth_request_dict);
}
#endif

extern int command_spc(int argc, char * const *argv)
{
    SecIdentityRef identity = NULL;
	extern char *optarg;
    extern int optind;
	int arg;
	while ((arg = getopt(argc, argv, "du:p:")) != -1) {
		switch (arg) {
            case 'd':
                debug = 1;
                break;
            case 'u':
				user = optarg;
				break;
			case 'p':
                pass = optarg;
				break;
			case 'h':
			default:
				return 2;
		}
	}

	argc -= optind;
	argv += optind;

#if 0
    if (argc == 1) {
        // get plist from argv[0] url
    } else if (argc == 0) {
        machine_authentication(NULL, NULL);
    } else return 2;
#endif

    if (argc != 1)
        return 2;

    int result = -1;
    CFDictionaryRef dict = NULL;
    CFDictionaryRef scep_config = NULL;
    CFDictionaryRef payload_content = NULL;
    CFStringRef profile_url = NULL;
    CFDataRef profile_data = NULL;
    CFDataRef profile_plist = NULL;
    CFStringRef machine_id_url_cfstring = CFStringCreateWithCString(kCFAllocatorDefault,
        argv[0], kCFStringEncodingUTF8);
    CFDictionaryRef enroll_packet =
        get_encrypted_profile_packet(machine_id_url_cfstring);
    require(enroll_packet && CFGetTypeID(enroll_packet) == CFDictionaryGetTypeID(), out);
    //require(payload_type(enroll_packet, "Encrypted Profile Service"), out);
    require(payload_content = dict_get_value_of_type(enroll_packet,
        CFSTR("PayloadContent"), CFDictionaryGetTypeID()), out);
    require(scep_config = dict_get_value_of_type(payload_content, 
        CFSTR("SCEP"), CFDictionaryGetTypeID()), out);
    require(identity = perform_scep(scep_config), out);
    dict = CFDictionaryCreate(NULL, (const void **)&kSecValueRef, (const void **)&identity, 1, NULL, NULL);
    require_noerr(SecItemAdd(dict, NULL), out);
    CFReleaseNull(dict);
    require(profile_url = dict_get_value_of_type(payload_content,
        CFSTR("URL"), CFStringGetTypeID()), out);
        
    require(profile_data = get_profile(profile_url, identity), out);
    if (debug)
        write_data("/tmp/profile.mobileconfig", profile_data);
    
    require_noerr(SecCMSVerify(profile_data, NULL, NULL, NULL, &profile_plist), out);
    CFReleaseNull(profile_data);
    require(profile_plist, out);
    require(validate_profile(profile_plist, identity), out);

    result = 0;
out:
    CFReleaseSafe(dict);
    CFReleaseSafe(identity);
    CFReleaseSafe(enroll_packet);
    CFReleaseSafe(profile_data);
    CFReleaseSafe(profile_plist);
    CFReleaseSafe(machine_id_url_cfstring);

    if (result != 0)
        errx(1, "fail.");
    return result;
}


#endif // TARGET_OS_EMBEDDED
