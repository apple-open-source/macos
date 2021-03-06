/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
 */

#ifndef _TRUSTTESTS_IDENTITY_INTERFACE_H_
#define _TRUSTTESTS_IDENTITY_INTERFACE_H_

/* MARK: testImportIdentity */

static
const uint8_t test_p12[] = {
  0x30, 0x82, 0x0a, 0xb9, 0x02, 0x01, 0x03, 0x30, 0x82, 0x0a, 0x80, 0x06,
  0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01, 0xa0, 0x82,
  0x0a, 0x71, 0x04, 0x82, 0x0a, 0x6d, 0x30, 0x82, 0x0a, 0x69, 0x30, 0x82,
  0x04, 0xcf, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07,
  0x06, 0xa0, 0x82, 0x04, 0xc0, 0x30, 0x82, 0x04, 0xbc, 0x02, 0x01, 0x00,
  0x30, 0x82, 0x04, 0xb5, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
  0x01, 0x07, 0x01, 0x30, 0x1c, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7,
  0x0d, 0x01, 0x0c, 0x01, 0x06, 0x30, 0x0e, 0x04, 0x08, 0x8f, 0x4d, 0x29,
  0x1d, 0x21, 0x48, 0x05, 0x13, 0x02, 0x02, 0x08, 0x00, 0x80, 0x82, 0x04,
  0x88, 0x16, 0xac, 0xed, 0xf0, 0x0c, 0x6e, 0x05, 0x00, 0x2b, 0xa7, 0xd8,
  0x73, 0xfa, 0x08, 0xc0, 0x54, 0x06, 0x1f, 0x19, 0xf4, 0x76, 0x2a, 0x0c,
  0xa7, 0xd8, 0x0e, 0xcb, 0x8b, 0x1a, 0xac, 0xb7, 0x8b, 0xe9, 0xc8, 0xe7,
  0xe1, 0x05, 0xfe, 0xaf, 0x35, 0x55, 0xf3, 0x97, 0xf0, 0x54, 0x6a, 0xf4,
  0xac, 0xb2, 0xe8, 0xf6, 0x05, 0xa4, 0xea, 0x6d, 0xba, 0xc7, 0x4b, 0x64,
  0xbe, 0xea, 0xce, 0x36, 0x54, 0xf7, 0xbd, 0x0b, 0x19, 0x68, 0x22, 0x55,
  0xd0, 0xd9, 0x83, 0x08, 0x1d, 0x20, 0x6f, 0x42, 0x0b, 0xf1, 0x84, 0x71,
  0x57, 0x2c, 0x32, 0xb4, 0x9a, 0x26, 0x63, 0x1c, 0x6c, 0x4b, 0x97, 0xfb,
  0xb6, 0x45, 0x12, 0xe6, 0x9f, 0xaa, 0x25, 0xa6, 0xfc, 0x3c, 0xef, 0x25,
  0xc4, 0x3a, 0x9c, 0x7b, 0x84, 0xc9, 0x44, 0xc7, 0xca, 0xc2, 0xa2, 0x2d,
  0x58, 0x34, 0x38, 0xbe, 0x4f, 0xd8, 0x57, 0xa7, 0x55, 0x47, 0xb6, 0x10,
  0x92, 0x90, 0xb4, 0x0a, 0x9d, 0x4a, 0xdd, 0xd2, 0xce, 0x03, 0xe8, 0x17,
  0x01, 0xaa, 0xac, 0x69, 0xa0, 0x24, 0xd9, 0x10, 0x51, 0x65, 0x5c, 0x4f,
  0x2a, 0x85, 0x4a, 0x4d, 0x0a, 0x23, 0xf6, 0x52, 0x0c, 0xac, 0xf2, 0x49,
  0x3d, 0xd9, 0x85, 0x87, 0x2e, 0x7a, 0xe3, 0x88, 0x47, 0xef, 0xfd, 0x28,
  0x9d, 0x9c, 0xd9, 0xc2, 0xdd, 0x49, 0xb2, 0x63, 0x3f, 0x06, 0x8c, 0x6d,
  0x2e, 0x72, 0x34, 0x74, 0x3f, 0x2c, 0x31, 0xbb, 0xaf, 0xc1, 0x4a, 0x6e,
  0xd5, 0xf1, 0xfe, 0xa4, 0x77, 0x33, 0x84, 0x50, 0x24, 0xfd, 0x7a, 0xfd,
  0xa3, 0x23, 0x4b, 0xa7, 0x87, 0x92, 0xe9, 0x15, 0x43, 0xc8, 0xdf, 0x5e,
  0x2b, 0x7c, 0xed, 0x1d, 0xf0, 0xef, 0x36, 0x4f, 0x6a, 0x74, 0xbc, 0xe5,
  0xd0, 0xd7, 0xe6, 0xfc, 0x4d, 0x8d, 0x0d, 0xb2, 0xaa, 0xb0, 0xd1, 0xb9,
  0x0f, 0xb4, 0x44, 0x26, 0x0d, 0x21, 0xca, 0x4d, 0x53, 0x50, 0x20, 0xe4,
  0x3e, 0xa0, 0xdf, 0xe9, 0x17, 0x17, 0x04, 0x3f, 0xa5, 0x2e, 0xeb, 0x85,
  0x72, 0x8f, 0x59, 0xce, 0x6f, 0x7e, 0x5d, 0x32, 0xda, 0xe8, 0x43, 0x29,
  0x12, 0x0b, 0xba, 0x66, 0xf5, 0x93, 0x1f, 0x33, 0x56, 0xa3, 0xe6, 0x2d,
  0xeb, 0x6e, 0xad, 0x64, 0x3d, 0x38, 0x0d, 0x67, 0x7d, 0xe9, 0x12, 0x85,
  0x46, 0x15, 0x9e, 0x6c, 0xfb, 0xc2, 0xd6, 0x7c, 0xd0, 0x57, 0x3d, 0x5a,
  0xa8, 0x54, 0xa5, 0x0c, 0x8c, 0x46, 0x17, 0xb5, 0xf4, 0xe7, 0x41, 0x5e,
  0x6a, 0x49, 0x04, 0x1f, 0x99, 0xef, 0xe0, 0x7d, 0xcf, 0xdd, 0x2a, 0x63,
  0x77, 0x8c, 0xe3, 0x65, 0x7e, 0xab, 0x6f, 0x34, 0x15, 0x75, 0xf3, 0xdc,
  0x6e, 0xa0, 0x29, 0x39, 0xe1, 0x64, 0x89, 0xe1, 0x00, 0x8e, 0x2f, 0x3a,
  0x0e, 0xd8, 0xa3, 0x8d, 0x30, 0x3b, 0x26, 0x34, 0x99, 0xa7, 0xcc, 0xcd,
  0x09, 0x16, 0xd8, 0x65, 0x28, 0xdb, 0xc9, 0x04, 0x80, 0xa3, 0x41, 0x05,
  0x7b, 0x3d, 0xf0, 0xd5, 0x9e, 0x11, 0xca, 0x6e, 0xa1, 0x16, 0xaf, 0x0c,
  0x71, 0xc4, 0xd1, 0x00, 0x12, 0xfb, 0x13, 0xef, 0xf6, 0x55, 0x8f, 0xb4,
  0xff, 0x3c, 0x76, 0x57, 0x1e, 0x73, 0x5d, 0x46, 0x5c, 0xdf, 0xf7, 0x13,
  0xbe, 0x6f, 0xf7, 0x8b, 0x65, 0xf9, 0x6d, 0xd4, 0xcd, 0x6f, 0x56, 0x28,
  0xbd, 0x64, 0xb2, 0xd4, 0xd3, 0x31, 0xa9, 0x75, 0xda, 0x5f, 0xf1, 0xfd,
  0xc1, 0x90, 0x1c, 0xb0, 0xd9, 0x8d, 0x96, 0x8e, 0xec, 0xa7, 0x2c, 0x48,
  0x93, 0x44, 0x45, 0x67, 0x73, 0x5b, 0xe8, 0x0f, 0x97, 0x95, 0x19, 0x53,
  0x55, 0x75, 0x69, 0x39, 0x38, 0xe4, 0x18, 0xb5, 0x64, 0x14, 0x9b, 0x95,
  0x7c, 0x25, 0xcb, 0x27, 0x1d, 0x6e, 0x48, 0xff, 0xc2, 0x69, 0x7f, 0x4e,
  0x96, 0x6c, 0x0b, 0x0a, 0x3d, 0xe6, 0xe6, 0xd5, 0xa0, 0xe7, 0x2d, 0xad,
  0x55, 0xf4, 0xfb, 0x29, 0x3f, 0xf3, 0x6e, 0x06, 0x6b, 0x1d, 0xa9, 0x41,
  0xec, 0x1e, 0x8a, 0x20, 0xf1, 0x49, 0x75, 0xda, 0x8a, 0x6f, 0xb3, 0xb6,
  0x07, 0x23, 0xdb, 0xe1, 0x43, 0x36, 0x64, 0xda, 0x31, 0x39, 0x41, 0xfc,
  0xa1, 0x18, 0xc1, 0xaa, 0xa9, 0x7b, 0xf3, 0x7c, 0x10, 0x40, 0xd9, 0xfd,
  0x7b, 0x0d, 0x85, 0x4f, 0x47, 0x2c, 0x9c, 0xb8, 0xb0, 0xec, 0x00, 0x0e,
  0xf7, 0xf5, 0xb6, 0x36, 0xb6, 0x7c, 0xf9, 0x0c, 0xf3, 0xa5, 0x1a, 0x0a,
  0x39, 0x0e, 0x33, 0xde, 0x1b, 0x51, 0xac, 0xe2, 0x8f, 0x72, 0xbe, 0x3e,
  0xe0, 0x97, 0xeb, 0xd6, 0xc3, 0xcc, 0x4e, 0x5f, 0xb4, 0xa4, 0xf1, 0x95,
  0x45, 0x7a, 0x34, 0xc1, 0x92, 0x9a, 0x92, 0x42, 0xef, 0xfd, 0x49, 0xe7,
  0x2a, 0xed, 0x73, 0x43, 0x6f, 0xd4, 0xd6, 0x58, 0xeb, 0x79, 0x53, 0x9b,
  0xa4, 0x2a, 0x12, 0x17, 0x04, 0x0e, 0x2c, 0x6d, 0x16, 0x9f, 0xb9, 0x13,
  0x6a, 0x8f, 0x60, 0x81, 0x8b, 0x2f, 0xae, 0x5e, 0x32, 0x31, 0x1d, 0x58,
  0x71, 0x77, 0x4d, 0xc2, 0x13, 0x94, 0xcf, 0x1d, 0x3e, 0x19, 0x3c, 0x26,
  0x69, 0x8e, 0x0e, 0x78, 0xfd, 0x4b, 0xf2, 0xf3, 0x3b, 0xb8, 0x30, 0x96,
  0xde, 0x8d, 0x14, 0xba, 0xe1, 0x1c, 0x18, 0x6e, 0x46, 0x14, 0x8d, 0x78,
  0x55, 0x34, 0x25, 0xfa, 0x72, 0x37, 0xa8, 0x40, 0x0b, 0x9f, 0x1d, 0x4f,
  0x76, 0x19, 0x6b, 0x5d, 0xba, 0x9e, 0x75, 0x31, 0xe9, 0x59, 0x0e, 0xfd,
  0xb6, 0x6a, 0x96, 0xaa, 0xf8, 0xab, 0xa7, 0x19, 0x89, 0x49, 0xee, 0x82,
  0x8c, 0x46, 0xef, 0x2f, 0xe7, 0x0a, 0xa8, 0x87, 0x07, 0x9e, 0x7e, 0x1e,
  0x48, 0xad, 0x07, 0x3e, 0x1a, 0x84, 0xfa, 0xda, 0xf1, 0x58, 0xb1, 0x72,
  0xd2, 0x9c, 0xcc, 0x9d, 0x68, 0x08, 0x1b, 0xd5, 0x29, 0x6e, 0x8c, 0xfe,
  0x37, 0x9f, 0x97, 0x57, 0x4d, 0xe8, 0x93, 0x6b, 0x14, 0x8d, 0xf2, 0x67,
  0x47, 0xde, 0x36, 0x9a, 0xc9, 0x4b, 0x89, 0x74, 0x11, 0xef, 0x35, 0x1c,
  0x14, 0xfd, 0xfd, 0xe1, 0x95, 0xf9, 0x39, 0xe0, 0x4f, 0xad, 0xc3, 0x08,
  0x1a, 0x24, 0x18, 0xd7, 0x60, 0xec, 0x10, 0xf3, 0x1c, 0xb9, 0x9b, 0x31,
  0x5f, 0xf3, 0x81, 0xa7, 0xf8, 0x20, 0xd1, 0xe3, 0x89, 0xed, 0xe1, 0x59,
  0x8d, 0x76, 0xb2, 0x90, 0x09, 0x0d, 0x3a, 0xb2, 0xb8, 0xca, 0x82, 0x11,
  0xa9, 0xcb, 0xb2, 0xfc, 0x24, 0xb0, 0x75, 0x92, 0xc1, 0xd9, 0x21, 0x60,
  0x8c, 0xe5, 0x95, 0xbf, 0x17, 0xe8, 0x2a, 0x4a, 0x18, 0xcf, 0x1b, 0x91,
  0x5b, 0xe3, 0xfd, 0x23, 0xee, 0x53, 0x54, 0xdd, 0x8f, 0x38, 0x84, 0x8d,
  0x1c, 0x5c, 0x60, 0x47, 0x65, 0x8d, 0xd4, 0x7a, 0x13, 0xf8, 0x5d, 0xda,
  0x30, 0x37, 0xdd, 0x4c, 0xc2, 0xe9, 0xcf, 0x77, 0xde, 0xdc, 0x4c, 0xff,
  0xd8, 0x2f, 0x81, 0x7a, 0xbe, 0x44, 0x4a, 0xa1, 0xf4, 0x12, 0x01, 0x53,
  0x46, 0x99, 0x26, 0xb5, 0xd8, 0x5d, 0x77, 0x6c, 0xf1, 0x47, 0x62, 0x32,
  0x37, 0x3c, 0x8d, 0x0d, 0xbb, 0x69, 0x44, 0x44, 0x12, 0xe6, 0xb7, 0xea,
  0xa7, 0xb1, 0x0a, 0x69, 0x85, 0xbe, 0xf3, 0x47, 0xe0, 0x90, 0xfc, 0x6a,
  0xa5, 0x4d, 0x5b, 0xf6, 0x6d, 0x6f, 0xd6, 0x59, 0x52, 0xff, 0x1f, 0xea,
  0x2d, 0x00, 0x90, 0xdd, 0x64, 0xaf, 0xac, 0x8b, 0x93, 0x5c, 0xd0, 0x58,
  0x92, 0x7e, 0x44, 0x1c, 0x7d, 0xb7, 0x37, 0x8a, 0x4c, 0x23, 0xf8, 0xe5,
  0xff, 0x4d, 0x6b, 0x9b, 0xa5, 0x4a, 0x99, 0xa9, 0xdc, 0x09, 0xc7, 0x2d,
  0xee, 0x60, 0x41, 0x6f, 0xfc, 0x96, 0x96, 0x7d, 0xc9, 0xe8, 0x80, 0x36,
  0xff, 0x12, 0x29, 0x10, 0x4e, 0x02, 0x57, 0x6c, 0x0e, 0x86, 0xa8, 0x1e,
  0x2f, 0x0e, 0x05, 0x70, 0x84, 0x83, 0xcb, 0x3b, 0x22, 0x15, 0x4a, 0x07,
  0x74, 0x01, 0xb5, 0xca, 0x5c, 0x9e, 0x88, 0x8c, 0x13, 0x01, 0xf3, 0x22,
  0xb3, 0x05, 0x99, 0xa1, 0xfc, 0xf4, 0x01, 0x2b, 0xe2, 0x4f, 0x04, 0x13,
  0x12, 0xad, 0xfc, 0xf9, 0xd4, 0x24, 0x78, 0x46, 0x52, 0x94, 0x93, 0x9f,
  0xcd, 0x5c, 0x04, 0x6a, 0xa3, 0x09, 0x25, 0x95, 0xb4, 0x20, 0x51, 0x40,
  0x72, 0x32, 0xc0, 0xea, 0x69, 0x01, 0x6e, 0xb3, 0xf5, 0xb3, 0x11, 0xc5,
  0xa0, 0x8a, 0x8e, 0x62, 0x92, 0x77, 0x8f, 0x33, 0x03, 0xed, 0x77, 0xba,
  0xe7, 0x01, 0x01, 0x78, 0x19, 0x45, 0x9e, 0x84, 0xe1, 0x91, 0xb2, 0x18,
  0x53, 0xbd, 0xdf, 0x65, 0x92, 0xa7, 0xef, 0x9f, 0x2a, 0xdc, 0xc6, 0x62,
  0xff, 0xa0, 0x9f, 0xc9, 0x2c, 0x28, 0x1e, 0xa7, 0xe7, 0x66, 0x7e, 0xcc,
  0x4f, 0x88, 0x91, 0x8e, 0xad, 0x91, 0x20, 0x97, 0x59, 0x29, 0xa0, 0xd3,
  0xc4, 0x2c, 0x14, 0x81, 0xdc, 0x39, 0x09, 0x39, 0xae, 0x30, 0x82, 0x05,
  0x92, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x07, 0x01,
  0xa0, 0x82, 0x05, 0x83, 0x04, 0x82, 0x05, 0x7f, 0x30, 0x82, 0x05, 0x7b,
  0x30, 0x82, 0x05, 0x77, 0x06, 0x0b, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d,
  0x01, 0x0c, 0x0a, 0x01, 0x02, 0xa0, 0x82, 0x04, 0xee, 0x30, 0x82, 0x04,
  0xea, 0x30, 0x1c, 0x06, 0x0a, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x0c, 0x01, 0x03, 0x30, 0x0e, 0x04, 0x08, 0x42, 0x31, 0xa4, 0x16, 0x28,
  0x32, 0x3c, 0x4b, 0x02, 0x02, 0x08, 0x00, 0x04, 0x82, 0x04, 0xc8, 0xee,
  0x3e, 0xfb, 0xcb, 0xa9, 0x11, 0xc9, 0xd6, 0x3f, 0x03, 0x59, 0x2c, 0x91,
  0x8c, 0x9d, 0x6d, 0xba, 0x13, 0xbc, 0xcd, 0x40, 0x6b, 0x02, 0x6c, 0xdd,
  0x06, 0x33, 0x55, 0x35, 0x40, 0x91, 0xb5, 0x04, 0xd0, 0x4f, 0xe4, 0x40,
  0x00, 0x28, 0xa6, 0x38, 0x47, 0xdc, 0x9d, 0xda, 0xd3, 0x39, 0x68, 0x2e,
  0x26, 0xb8, 0xfe, 0xc8, 0x2f, 0xf3, 0xe4, 0xb6, 0x45, 0x02, 0xa4, 0xe5,
  0x57, 0x31, 0x57, 0x33, 0x8c, 0x18, 0x95, 0x47, 0xab, 0xac, 0x96, 0x88,
  0x94, 0x2f, 0x92, 0xb3, 0x60, 0x30, 0x12, 0x1c, 0xca, 0xa9, 0x03, 0x3a,
  0xb4, 0xe8, 0x3f, 0x75, 0x1b, 0xfc, 0xa2, 0x94, 0x8a, 0x6c, 0x14, 0xc0,
  0x55, 0x38, 0xb6, 0xc0, 0x68, 0x76, 0x45, 0x81, 0x90, 0x68, 0xf1, 0xce,
  0x11, 0x8d, 0x35, 0x49, 0xe3, 0x64, 0xa4, 0x9d, 0x9d, 0xf7, 0xb2, 0x9e,
  0x17, 0x26, 0x7e, 0x90, 0x62, 0xc1, 0xa2, 0x9e, 0x3e, 0xda, 0xce, 0x8c,
  0xfd, 0xc8, 0x24, 0xb6, 0x7b, 0x51, 0x96, 0xa2, 0xa5, 0x03, 0xca, 0x52,
  0xc6, 0x04, 0x41, 0xa2, 0xea, 0xb4, 0xbf, 0x77, 0xda, 0x81, 0xa2, 0x3c,
  0x92, 0x70, 0x41, 0xbc, 0x78, 0xb0, 0x20, 0xcf, 0x4f, 0x52, 0xad, 0xb7,
  0xa4, 0x68, 0x84, 0xa7, 0x36, 0x0b, 0x94, 0x7c, 0x34, 0x76, 0xe4, 0x08,
  0x2f, 0x50, 0x69, 0xba, 0x42, 0x1c, 0x63, 0xd4, 0x10, 0xf8, 0xaa, 0xaa,
  0xb9, 0xdf, 0x15, 0xbc, 0x56, 0x00, 0x27, 0x4f, 0x33, 0x01, 0x72, 0x06,
  0x6a, 0xca, 0xb5, 0xa4, 0xbe, 0x20, 0xe8, 0xe5, 0x50, 0xce, 0x43, 0x0b,
  0xad, 0x50, 0x45, 0x58, 0x48, 0xce, 0x49, 0xb9, 0x09, 0x7a, 0xf4, 0x4f,
  0x24, 0x71, 0x68, 0x4c, 0x7f, 0x53, 0xd2, 0x91, 0xed, 0x9b, 0xbc, 0x76,
  0x6d, 0x96, 0x66, 0x9c, 0xfd, 0xee, 0xdd, 0x95, 0xc2, 0x33, 0xdf, 0x36,
  0x37, 0x75, 0x35, 0x5e, 0x7c, 0xbe, 0x85, 0x35, 0x27, 0x65, 0x98, 0xa1,
  0x57, 0xd5, 0xf1, 0x9b, 0xc1, 0x3c, 0x74, 0x0f, 0xfd, 0x21, 0x2e, 0x6d,
  0x3f, 0xae, 0x87, 0xfb, 0x8f, 0x1f, 0x20, 0x06, 0x97, 0x43, 0xc2, 0x6c,
  0xec, 0x55, 0xc2, 0x62, 0xbb, 0xe7, 0x9b, 0x9c, 0x49, 0xd7, 0xb5, 0xd0,
  0x4b, 0x5b, 0x33, 0x29, 0xa9, 0xf0, 0x00, 0x80, 0xcf, 0xaa, 0xf1, 0x52,
  0x45, 0x86, 0x37, 0x98, 0x64, 0x98, 0x1c, 0xab, 0xdc, 0x0e, 0x87, 0xa6,
  0x52, 0x2b, 0xb3, 0xe6, 0xff, 0xa8, 0x57, 0xd2, 0xe7, 0x25, 0x66, 0x01,
  0x34, 0x80, 0x5a, 0xa5, 0x5d, 0x96, 0x3a, 0xe0, 0xe5, 0x39, 0xa3, 0x71,
  0xa3, 0x33, 0x1d, 0xbb, 0x97, 0x99, 0x62, 0x15, 0x9d, 0x12, 0x62, 0xbd,
  0x45, 0xc1, 0xcd, 0xd4, 0x37, 0xb2, 0x10, 0x7a, 0xf8, 0x66, 0xd9, 0xe4,
  0x6e, 0xa8, 0xb8, 0xe8, 0x59, 0x3a, 0xbd, 0xe8, 0x1e, 0x82, 0x62, 0xb2,
  0x7f, 0xf1, 0x5f, 0x45, 0x0c, 0x8e, 0x1c, 0x4f, 0x25, 0xa9, 0xaa, 0xae,
  0xee, 0x2c, 0x93, 0x2d, 0x0b, 0x01, 0xfb, 0xbb, 0x78, 0xed, 0x26, 0x02,
  0x81, 0x41, 0xea, 0xcd, 0xa8, 0x64, 0x9d, 0x38, 0x9d, 0xe9, 0xa3, 0x33,
  0x02, 0xd8, 0x3a, 0x3c, 0xdd, 0x2c, 0xed, 0xc2, 0x90, 0x66, 0x48, 0x97,
  0x12, 0x36, 0xfb, 0xb2, 0xf9, 0x5c, 0xe7, 0x37, 0xe3, 0xf8, 0x0d, 0xb0,
  0x09, 0x38, 0xd8, 0x4b, 0xcf, 0xa5, 0x2e, 0xfd, 0x54, 0xa8, 0xaf, 0x83,
  0xa5, 0xb7, 0x5a, 0x2b, 0xee, 0x09, 0xce, 0x5a, 0x23, 0x9c, 0xa5, 0x07,
  0x63, 0x04, 0x4a, 0xaf, 0x18, 0x9b, 0x56, 0xa5, 0xd8, 0xc5, 0xe0, 0x5d,
  0xab, 0x07, 0x60, 0xd9, 0xf8, 0x32, 0xd1, 0x73, 0x3f, 0xfa, 0x2b, 0xa5,
  0xd1, 0xe3, 0xdf, 0x6c, 0x90, 0x92, 0x7d, 0x80, 0xe8, 0x99, 0x74, 0x9c,
  0x00, 0x0c, 0x6c, 0x65, 0xe4, 0x0e, 0xc0, 0xa0, 0x8d, 0x99, 0x38, 0xe4,
  0xff, 0x7e, 0x30, 0x95, 0x9e, 0xfa, 0xbf, 0x80, 0xfc, 0x28, 0xd8, 0x73,
  0x1b, 0xd9, 0x42, 0x67, 0x17, 0x7e, 0xa9, 0x00, 0x1e, 0x88, 0x27, 0x10,
  0x98, 0x79, 0x7a, 0x21, 0x77, 0x5f, 0x65, 0x2c, 0x94, 0xfe, 0x38, 0x55,
  0x67, 0xca, 0xce, 0x01, 0x24, 0x8a, 0xb0, 0x80, 0x76, 0xca, 0x07, 0xfb,
  0x0f, 0x09, 0xb2, 0xa3, 0xb2, 0x8e, 0xbf, 0xa0, 0x8d, 0xf9, 0x09, 0xe8,
  0x25, 0x41, 0xd7, 0x93, 0x9b, 0xcc, 0xee, 0x33, 0x1e, 0x3d, 0xa9, 0x5e,
  0xb7, 0x68, 0xd8, 0x63, 0x7b, 0x33, 0xcd, 0x87, 0x79, 0x4d, 0xca, 0x2f,
  0xfd, 0xf8, 0xe8, 0xb6, 0xb1, 0x8a, 0xb9, 0xc7, 0xb7, 0x6b, 0xa6, 0x41,
  0xda, 0x7e, 0x20, 0x0e, 0xd8, 0x5c, 0x54, 0xee, 0x2b, 0x15, 0xe7, 0xb7,
  0x7c, 0x42, 0x31, 0xce, 0xbd, 0x40, 0x27, 0xeb, 0x7d, 0xbb, 0x30, 0x56,
  0x0b, 0xa9, 0x82, 0xcd, 0x54, 0x4d, 0x79, 0x64, 0x5b, 0x6e, 0x76, 0x67,
  0xdc, 0x51, 0xb4, 0x65, 0x80, 0x7a, 0x24, 0x48, 0xbc, 0x93, 0xf4, 0x32,
  0xe7, 0xcd, 0x9e, 0x54, 0x0f, 0x0b, 0x66, 0xf7, 0x7c, 0xd1, 0x41, 0xf3,
  0xf4, 0x17, 0x94, 0xf6, 0xd3, 0x5f, 0x13, 0x4a, 0x7a, 0x03, 0x85, 0x3d,
  0xc0, 0xc2, 0x42, 0x30, 0xc1, 0xdf, 0x04, 0xa1, 0x25, 0x1b, 0x40, 0x19,
  0x27, 0x1b, 0xcf, 0x32, 0xbc, 0x53, 0x9a, 0xd8, 0x5e, 0x65, 0x3c, 0x3b,
  0x6e, 0x65, 0x29, 0x3b, 0x1c, 0x48, 0xb5, 0x0d, 0xe1, 0xc6, 0xe6, 0x33,
  0x60, 0xfd, 0x23, 0x62, 0x03, 0xba, 0x35, 0x00, 0xf3, 0xec, 0x46, 0x06,
  0x91, 0xd5, 0x22, 0x16, 0x93, 0xd9, 0x8f, 0xae, 0xf7, 0x36, 0x55, 0x67,
  0xc5, 0x3b, 0x79, 0x11, 0xea, 0x99, 0x26, 0xab, 0xd2, 0xce, 0xdb, 0x07,
  0xaa, 0xc3, 0x67, 0x2a, 0x64, 0xbd, 0xd4, 0xd0, 0x92, 0xef, 0xec, 0xba,
  0x23, 0xf1, 0x03, 0x89, 0x31, 0x53, 0xed, 0x56, 0xf6, 0x42, 0xd4, 0x05,
  0x82, 0xf7, 0x9b, 0xd6, 0xb6, 0x99, 0xd0, 0x2c, 0x5f, 0xba, 0x5d, 0x82,
  0xe7, 0xbc, 0x1c, 0x95, 0xa0, 0x1e, 0xf8, 0xdd, 0x0d, 0x3a, 0xc3, 0xc5,
  0x62, 0x0f, 0xb4, 0x25, 0x5f, 0x29, 0x76, 0x18, 0x6b, 0x00, 0xe2, 0xda,
  0xe0, 0x06, 0x38, 0x30, 0x05, 0xe3, 0x2a, 0x62, 0xd3, 0x66, 0x4b, 0x0f,
  0x67, 0x08, 0x09, 0xea, 0x5c, 0xb8, 0x19, 0x96, 0x43, 0x2d, 0x97, 0x7e,
  0x41, 0x43, 0x1e, 0x17, 0x0a, 0xdf, 0xc5, 0xe7, 0x33, 0xe2, 0x3f, 0xb9,
  0xb6, 0x85, 0xf4, 0x35, 0x8a, 0x3f, 0x08, 0x3c, 0x06, 0x18, 0xb1, 0xd0,
  0x6a, 0x64, 0x6e, 0x43, 0xe1, 0x22, 0x36, 0xe5, 0xf7, 0x09, 0x7d, 0xda,
  0xf9, 0x43, 0x9d, 0x09, 0x55, 0xa4, 0xc8, 0x62, 0x78, 0x66, 0x9c, 0xcf,
  0x38, 0xd2, 0xc5, 0xb4, 0x2e, 0xdc, 0x6d, 0xde, 0xf0, 0xc7, 0xed, 0x24,
  0x98, 0x57, 0xb4, 0xd0, 0x93, 0x12, 0x26, 0xe2, 0x3f, 0x44, 0xbc, 0xd0,
  0x99, 0x69, 0x32, 0x34, 0xf8, 0xa6, 0x8d, 0xbf, 0x13, 0x02, 0x63, 0x1e,
  0x69, 0x61, 0x94, 0x9c, 0xcf, 0xf2, 0x8f, 0x3a, 0x54, 0xcd, 0x97, 0xea,
  0x84, 0x53, 0xd9, 0xf6, 0x44, 0x3c, 0xbd, 0x54, 0x48, 0x3d, 0x05, 0x55,
  0x8b, 0x85, 0xb4, 0xec, 0x41, 0xf8, 0xdf, 0x06, 0xae, 0x0b, 0x41, 0x73,
  0x79, 0xe0, 0x89, 0xa9, 0xb5, 0x68, 0x18, 0x38, 0xb0, 0xe7, 0xb9, 0x2c,
  0x09, 0xd9, 0xb9, 0x06, 0x47, 0xe8, 0xcf, 0xae, 0xb9, 0x5d, 0x88, 0x33,
  0xe1, 0x3c, 0xc5, 0x3f, 0x49, 0xda, 0x16, 0x8d, 0xa3, 0xa4, 0x27, 0x1e,
  0x51, 0xce, 0x8f, 0x52, 0x7e, 0xec, 0x90, 0x5d, 0x03, 0x70, 0xee, 0x5b,
  0xcb, 0xc1, 0x17, 0x2c, 0x45, 0x97, 0x55, 0x58, 0xb4, 0xeb, 0xa4, 0xfb,
  0x1a, 0x70, 0x5f, 0xac, 0x72, 0x59, 0x20, 0x04, 0x37, 0x57, 0x18, 0x6f,
  0x87, 0x29, 0xfc, 0x58, 0x0a, 0x8f, 0x2e, 0xc6, 0xff, 0x55, 0xc6, 0x4c,
  0x55, 0xa4, 0x06, 0x11, 0x13, 0xbc, 0xa2, 0x14, 0x77, 0xeb, 0x26, 0x21,
  0x79, 0xbb, 0x0d, 0x96, 0x7a, 0x80, 0x8a, 0xd2, 0x2d, 0x29, 0x85, 0x3d,
  0x04, 0x5a, 0x43, 0x45, 0x9d, 0x51, 0xf5, 0xaf, 0x24, 0x25, 0x85, 0xad,
  0xe2, 0xf8, 0xc4, 0x0b, 0x44, 0x84, 0x9c, 0xd0, 0x27, 0x00, 0x67, 0x48,
  0x02, 0x9f, 0x05, 0x2d, 0x3e, 0xe8, 0xf6, 0xb6, 0xeb, 0xde, 0xa9, 0xe2,
  0xd9, 0x62, 0x99, 0x02, 0x84, 0x65, 0xfb, 0x0a, 0x9b, 0x2b, 0x73, 0xb8,
  0x03, 0x8c, 0xc4, 0x98, 0xf9, 0x9d, 0xbf, 0x86, 0x5b, 0x84, 0xab, 0xe3,
  0xc6, 0x50, 0x34, 0xc6, 0x38, 0x5a, 0x41, 0x46, 0xff, 0xa6, 0xe8, 0x13,
  0x1d, 0x1a, 0x6a, 0x96, 0x6f, 0xfa, 0x87, 0xa1, 0x14, 0x15, 0x91, 0x43,
  0xad, 0xdb, 0x15, 0x48, 0x9f, 0x54, 0x41, 0x27, 0x38, 0x60, 0xbb, 0x17,
  0x01, 0xcd, 0x01, 0xab, 0x5e, 0x09, 0x74, 0x14, 0x77, 0xcd, 0xba, 0xf9,
  0x69, 0x37, 0x8f, 0xcc, 0xcc, 0x57, 0xde, 0xf9, 0xa1, 0xe7, 0x3b, 0x2f,
  0x03, 0x8b, 0xc2, 0x7b, 0x13, 0x03, 0x9c, 0x67, 0x80, 0x4e, 0xfe, 0x0d,
  0x30, 0xe2, 0x97, 0x62, 0xf6, 0xbe, 0x59, 0x89, 0xb9, 0xc8, 0x38, 0x25,
  0x3f, 0x3d, 0x66, 0xbe, 0x3a, 0x8d, 0x43, 0x84, 0x7d, 0x18, 0xa5, 0x31,
  0x76, 0x30, 0x4f, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
  0x09, 0x14, 0x31, 0x42, 0x1e, 0x40, 0x00, 0x54, 0x00, 0x65, 0x00, 0x73,
  0x00, 0x74, 0x00, 0x20, 0x00, 0x53, 0x00, 0x65, 0x00, 0x63, 0x00, 0x75,
  0x00, 0x72, 0x00, 0x69, 0x00, 0x74, 0x00, 0x79, 0x00, 0x20, 0x00, 0x46,
  0x00, 0x72, 0x00, 0x61, 0x00, 0x6d, 0x00, 0x65, 0x00, 0x77, 0x00, 0x6f,
  0x00, 0x72, 0x00, 0x6b, 0x00, 0x20, 0x00, 0x49, 0x00, 0x64, 0x00, 0x65,
  0x00, 0x6e, 0x00, 0x74, 0x00, 0x69, 0x00, 0x74, 0x00, 0x79, 0x30, 0x23,
  0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x09, 0x15, 0x31,
  0x16, 0x04, 0x14, 0x54, 0x36, 0xb0, 0x72, 0x55, 0x52, 0x47, 0x60, 0xa3,
  0x66, 0x0a, 0xef, 0x0d, 0x3b, 0x1c, 0x51, 0x2d, 0x0e, 0xa9, 0xfe, 0x30,
  0x30, 0x30, 0x21, 0x30, 0x09, 0x06, 0x05, 0x2b, 0x0e, 0x03, 0x02, 0x1a,
  0x05, 0x00, 0x04, 0x14, 0xbc, 0x8a, 0xfc, 0x3b, 0x75, 0x62, 0xaf, 0xb0,
  0x63, 0xd5, 0x2d, 0x34, 0xef, 0xac, 0x88, 0xe4, 0x7d, 0xe3, 0x2a, 0x48,
  0x04, 0x08, 0xd0, 0x1a, 0xbd, 0xe0, 0x7c, 0xb5, 0xdc, 0x5b, 0x02, 0x01,
  0x01
};

#endif /* _TRUSTTESTS_IDENTITY_INTERFACE_H_ */
