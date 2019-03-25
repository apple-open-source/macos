/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#ifndef _EAP8021X_EAPCLIENTCONFIGURATION_H
#define _EAP8021X_EAPCLIENTCONFIGURATION_H

/*
 * EAPClientConfiguration.h
 * - functions to handle EAPClientConfiguration validations
 */

#include <EAP8021X/EAP.h>
#include <stdbool.h>
#include <stdio.h>
#include <CoreFoundation/CFString.h>

/*
 * Function: EAPClientConfigurationCopyShareable
 *
 * Purpose:
 *   This function takes the original EAPClientConfiguration dictionary as an input and returns
 *   a dictionary with shareable EAPClientConfiguration dictionary and shareable identity information
 *   if input EAPClientConfiguration dictionary contains "TLSIdentityHandle" key.
 *   This function is meant to be called on the source device and returned dictionary should be
 *   passed to EAPClientConfigurationCopyAndImport() on destination device(e.g. HomePod).
 *
 * Returns:
 *   NULL if the input EAPClientConfiguration dictionary cannot be shared, non-NULL CFDictionaryRef otherwise.
 */
CFDictionaryRef
EAPClientConfigurationCopyShareable(CFDictionaryRef eapConfiguration);

/*
 * Function: EAPClientConfigurationCopyAndImport
 *
 * Purpose:
 *   This function takes a dictionary returned by EAPClientConfigurationCopyShareable() as an input
 *   and returns EAPClientConfiguration dictionary that can be used for EAP authentication.
 *   If input dictionary contains valid identity information dictionary then output EAPClientConfiguration
 *   dictionary will contain "TLSIdentityHandle" key.
 *   This function is meant to be called on the destination device(e.g. HomePod) and returned dictionary should be
 *   configured for EAP authentication.
 *
 * Returns:
 *   NULL if this function fails to validate the input dictionary, non-NULL CFDictionaryRef otherwise.
 */
CFDictionaryRef
EAPClientConfigurationCopyAndImport(CFDictionaryRef shareableEapConfiguration);

#endif /* _EAP8021X_EAPCLIENTCONFIGURATION_H */
