/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#import <CoreFoundation/CoreFoundation.h>

#import "codedirectory.h"
#import "SecCodeSignerRemote.h"


namespace Security {
namespace CodeSigning {

CF_ASSUME_NONNULL_BEGIN

/// Performs a signing operation for the provided code directory and context, using the
/// provided certificate chain and handler to get the actual signature.
/// Output is the full CMS blob that can be placed into a code signature slot.
OSStatus
doRemoteSigning(const CodeDirectory *cd,
				CFDictionaryRef hashDict,
				CFArrayRef hashList,
				CFAbsoluteTime signingTime,
				CFArrayRef certificateChain,
				SecCodeRemoteSignHandler signHandler,
				CFDataRef * _Nonnull CF_RETURNS_RETAINED outputCMS);

CF_ASSUME_NONNULL_END

}
}
