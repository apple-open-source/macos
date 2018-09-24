/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
#ifndef _H_NOTARIZATION
#define _H_NOTARIZATION

#include <Security/Security.h>
#include <security_utilities/dispatch.h>
#include <security_utilities/hashing.h>
#include <security_utilities/unix++.h>
#include "requirement.h"

namespace Security {
namespace CodeSigning {

// Performs an online check for a ticket, and returns true if a revocation ticket is found.
bool checkNotarizationServiceForRevocation(CFDataRef hash, SecCSDigestAlgorithm hashType, double *date);

// Performs an offline notarization check for the hash represented in the requirement context
// and returns whether the hash has a valid, unrevoked notarization ticket.
bool isNotarized(const Requirement::Context *context);

// Representation-specific methods for extracting a stapled ticket and registering
// it with the notarization daemon.
void registerStapledTicketInPackage(const std::string& path);
void registerStapledTicketInBundle(const std::string& path);
void registerStapledTicketInDMG(CFDataRef ticketData);

} // end namespace CodeSigning
} // end namespace Security

#endif /* _H_NOTARIZATION */
