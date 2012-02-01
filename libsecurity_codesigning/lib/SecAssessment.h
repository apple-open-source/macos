/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
#ifndef _H_SECASSESSMENT
#define _H_SECASSESSMENT

#include <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif


/*!
 * @type SecAccessmentRef An assessment being performed.
 */
typedef struct _SecAssessment *SecAssessmentRef;


/*!
 * CF-standard type function
 */
CFTypeID SecAssessmentGetTypeID();


/*!
 * Primary operation codes. These are operations the system policy can express
 * opinions on. They are not operations *on* the system configuration itself.
 * (For those, see SecAssessmentUpdate below.)
 *
 * @constant kSecAssessmentContextKeyOperation Context key describing the type of operation
 *	being contemplated.
 * @constant kSecAssessmentOperationTypeInstall Value denoting the operation of installing
 *	software into the system.
 * @constant kSecAssessmentOperationTypeExecute Value denoting the operation of running or executing
 *	code on the system.
 */
extern const CFStringRef kSecAssessmentContextKeyOperation;	// proposed operation
extern const CFStringRef kSecAssessmentOperationTypeExecute;	// .. execute code
extern const CFStringRef kSecAssessmentOperationTypeInstall;	// .. install software
extern const CFStringRef kSecAssessmentOperationTypeOpenDocument; // .. LaunchServices-level document open


/*!
	Operational flags for SecAssessment calls
	
	@type SecAssessmentFlags A mask of flag bits passed to SecAssessment calls to influence their
		operation.
	
	@constant kSecAssessmentDefaultFlags Pass this to indicate that default behavior is desired.
	@constant kSecAssessmentFlagIgnoreCache Do not use cached information; always perform a full
		evaluation of system policy. This may be substantially slower.
	@constant kSecAssessmentFlagNoCache Do not save any evaluation outcome in the system caches.
		Any content already there is left undisturbed. Independent of kSecAssessmentFlagIgnoreCache.
	
	Flags common to multiple calls are assigned from high-bit down. Flags for particular calls
	are assigned low-bit up, and are documented with that call.
 */
typedef uint64_t SecAssessmentFlags;
enum {
	kSecAssessmentDefaultFlags = 0,					// default behavior

	kSecAssessmentFlagDirect = 1 << 30,				// in-process evaluation
	kSecAssessmentFlagAsynchronous = 1 << 29,		// request asynchronous operation
	kSecAssessmentFlagIgnoreCache = 1 << 28,		// do not search cache
	kSecAssessmentFlagNoCache = 1 << 27,			// do not populate cache
	kSecAssessmentFlagEnforce = 1 << 26,			// force on (disable bypass switches)
};


/*!
	@function SecAssessmentCreate
	Ask the system for its assessment of a proposed operation.
	
	@param path CFURL describing the file central to the operation - the program
		to be executed, archive to be installed, plugin to be loaded, etc.
	@param flags Operation flags and options. Pass kSecAssessmentDefaultFlags for default
		behavior.
	@param context Optional CFDictionaryRef containing additional information bearing
		on the requested assessment.
	@param errors Standard CFError argument for reporting errors. Note that declining to permit
		the proposed operation is not an error. Inability to form a judgment is.
	@result On success, a SecAssessment object that can be queried for its outcome.
		On error, NULL (with *errors set).
	
	Option flags:
	
	@constant kSecAssessmentFlagRequestOrigin Request additional work to produce information on
		the originator (signer) of the object being discussed.

	Context keys:

	@constant kSecAssessmentContextKeyOperation Type of operation (see overview above). This defaults
		to the kSecAssessmentOperationTypeExecute.
	@constant kSecAssessmentContextKeyEdit A CFArray of SecCertificateRefs describing the
		certificate chain of a CMS-type signature as pulled from 'path' by the caller.
		The first certificate is the signing certificate. The certificates provided must be
		sufficient to construct a valid certificate chain.
 */
extern const CFStringRef kSecAssessmentContextKeyCertificates; // certificate chain as provided by caller

extern const CFStringRef kSecAssessmentAssessmentVerdict;		// CFBooleanRef: master result - allow or deny
extern const CFStringRef kSecAssessmentAssessmentOriginator;	// CFStringRef: describing the signature originator
extern const CFStringRef kSecAssessmentAssessmentAuthority;	// CFDictionaryRef: authority used to arrive at result
extern const CFStringRef kSecAssessmentAssessmentSource;		// CFStringRef: primary source of authority
extern const CFStringRef kSecAssessmentAssessmentFromCache;	// present if result is from cache
extern const CFStringRef kSecAssessmentAssessmentAuthorityRow; // (internal)
extern const CFStringRef kSecAssessmentAssessmentAuthorityOverride; // (internal)

enum {
	kSecAssessmentFlagRequestOrigin = 1 << 0,		// request origin information (slower)
};

SecAssessmentRef SecAssessmentCreate(CFURLRef path,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors);


/*!
	@function SecAssessmentCopyResult

	Extract results from a completed assessment and return them as a CFDictionary.

	Assessment result keys (dictionary keys returned on success):

	@param assessment A SecAssessmentRef created with SecAssessmentCreate.
	@param flags Operation flags and options. Pass kSecAssessmentDefaultFlags for default
		behavior.
	@errors Standard CFError argument for reporting errors. Note that declining to permit
		the proposed operation is not an error. Inability to form a judgment is.
	@result On success, a CFDictionary describing the outcome and various corroborating
		data as requested by flags. The caller owns this dictionary and should release it
		when done with it. On error, NULL (with *errors set).

	@constant kSecAssessmentAssessmentVerdict A CFBoolean value indicating whether the system policy
		allows (kCFBooleanTrue) or denies (kCFBooleanFalse) the proposed operation.
	@constant kSecAssessmentAssessmentAuthority A CFDictionary describing what sources of authority
		were used to arrive at this result.
	@constant kSecAssessmentAssessmentOriginator A human-readable CFString describing the originator
		of the signature securing the subject of the verdict. Requires kSecAssessmentFlagRequireOrigin.
		May be missing anyway if no reliable source of origin can be determined.
 */
CFDictionaryRef SecAssessmentCopyResult(SecAssessmentRef assessment,
	SecAssessmentFlags flags,
	CFErrorRef *errors);


/*!
	@function SecAssessmentUpdate
	Make changes to the system policy configuration.
	
	@param path CFURL describing the file central to an operation - the program
		to be executed, archive to be installed, plugin to be loaded, etc.
		Pass NULL if the requested operation has no file subject.
	@param flags Operation flags and options. Pass kSecAssessmentDefaultFlags for default
		behavior.
	@param context Required CFDictionaryRef containing information bearing
		on the requested assessment. Must at least contain the kSecAssessmentContextKeyEdit key.
	@param errors Standard CFError argument for reporting errors. Note that declining to permit
		the proposed operation is not an error. Inability to form a judgment is.
	@result Returns True on success. Returns False on failure (and sets *error).
	
	Context keys and values:

	@constant kSecAssessmentContextKeyEdit Required context key describing the kind of change
		requested to the system policy configuration. Currently understood values:
	@constant kSecAssessmentUpdateOperationAddFile Request to add rules governing operations on the 'path'
		argument.
	@constant kSecAssessmentUpdateOperationRemoveFile Request to remove rules governing operations on the
		'path' argument.
 */
extern const CFStringRef kSecAssessmentContextKeyUpdate;		// proposed operation
extern const CFStringRef kSecAssessmentUpdateOperationAddFile;	// add to policy database
extern const CFStringRef kSecAssessmentUpdateOperationRemoveFile; // remove from policy database

extern const CFStringRef kSecAssessmentUpdateKeyPriority;		// rule priority
extern const CFStringRef kSecAssessmentUpdateKeyLabel;			// rule label

Boolean SecAssessmentUpdate(CFURLRef path,
	SecAssessmentFlags flags,
	CFDictionaryRef context,
	CFErrorRef *errors);


/*!
	@function SecAssessmentControl
	Miscellaneous system policy operations
	
 */
Boolean SecAssessmentControl(CFStringRef control, void *arguments, CFErrorRef *errors);


#ifdef __cplusplus
}
#endif

#endif //_H_SECASSESSMENT
