/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#ifndef IPHPvDInfoRequestDateUtil_h
#define IPHPvDInfoRequestDateUtil_h

#define kPvDInfoAdditionalInfoDictKeyIdentifierCStr	"identifier"
#define kPvDInfoAdditionalInfoDictKeyIdentifier		CFSTR(kPvDInfoAdditionalInfoDictKeyIdentifierCStr)
#define kPvDInfoAdditionalInfoDictKeyExpiresCStr	"expires"
#define kPvDInfoAdditionalInfoDictKeyExpires		CFSTR(kPvDInfoAdditionalInfoDictKeyExpiresCStr)
#define kPvDInfoAdditionalInfoDictKeyPrefixesCStr	"prefixes"
#define kPvDInfoAdditionalInfoDictKeyPrefixes		CFSTR(kPvDInfoAdditionalInfoDictKeyPrefixesCStr)
#define kPvDInfoExpirationDateLocaleCStr		"en_US_POSIX"
#define kPvDInfoExpirationDateLocale			CFSTR(kPvDInfoExpirationDateLocaleCStr)
#define kPvDInfoExpirationDateFormatCStr		"yyyy-MM-dd'T'HH:mm:ss'Z'"
#define kPvDInfoExpirationDateFormat			CFSTR(kPvDInfoExpirationDateFormatCStr)

#endif /* IPHPvDInfoRequestDateUtil_h */
