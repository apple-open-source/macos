/*
 * Copyright (c) 2002-2009 Apple Inc. All rights reserved.
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
 * Modification History
 *
 * November 25, 2002	Dieter Siegmund (dieter@apple.com)
 * - created
 */

#ifndef _EAP802_1X_EAPCLIENTTYPES_H
#define _EAP802_1X_EAPCLIENTTYPES_H

#include <stdint.h>

enum {
    /* EAPClient specific errors */
    kEAPClientStatusOK = 0,
    kEAPClientStatusFailed = 1,
    kEAPClientStatusAllocationFailed = 2,
    kEAPClientStatusUserInputRequired = 3,
    kEAPClientStatusConfigurationInvalid = 4,
    kEAPClientStatusProtocolNotSupported = 5,
    kEAPClientStatusServerCertificateNotTrusted = 6,
    kEAPClientStatusInnerProtocolNotSupported = 7,
    kEAPClientStatusInternalError = 8,
    kEAPClientStatusUserCancelledAuthentication = 9,
    kEAPClientStatusUnknownRootCertificate = 10,
    kEAPClientStatusNoRootCertificate = 11,
    kEAPClientStatusCertificateExpired = 12,
    kEAPClientStatusCertificateNotYetValid = 13,
    kEAPClientStatusCertificateRequiresConfirmation = 14,
    kEAPClientStatusUserInputNotPossible = 15,
    kEAPClientStatusResourceUnavailable = 16,
    kEAPClientStatusProtocolError = 17,

    /* domain specific errors */
    kEAPClientStatusDomainSpecificErrorStart = 1000,
    kEAPClientStatusErrnoError = 1000,		/* errno error */
    kEAPClientStatusSecurityError = 1001,	/* Security framework error */
    kEAPClientStatusPluginSpecificError = 1002,	/* plug-in specific error */
};
typedef int32_t EAPClientStatus;

typedef int32_t EAPClientDomainSpecificError;

#endif _EAP8021X_EAPCLIENTTYPES_H
