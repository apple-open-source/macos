/*
 * Copyright (c) 2006-2007 Apple Inc. All Rights Reserved.
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
	@header SecCode
	SecCode represents separately indentified running code in the system.
	In addition to UNIX processes, this can also include (with suitable support)
	scripts, applets, widgets, etc.
*/
#ifndef _H_SECCODE
#define _H_SECCODE

#ifdef __cplusplus
extern "C" {
#endif

#include <Security/CSCommon.h>


/*!
	@function SecCodeGetTypeID
	Returns the type identifier of all SecCode instances.
*/
CFTypeID SecCodeGetTypeID(void);


/*!
	@function SecGetRootCode
	Obtains a SecCode object for the running code that hosts the entire system.
	This object usually represents the system kernel. It is always considered
	valid and is implicitly trusted by everyone.
	
	This object has no host.
	
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	
	@result An object reference to the root Code. This call should never fail.
	If this call returns NULL, Code Signing is unusable.
*/
SecCodeRef SecGetRootCode(SecCSFlags flags);


/*!
	@function SecCodeCopySelf
	Obtains a SecCode object for the code making the call.
	The calling code is determined in a way that is subject to modification over
	time, but obeys the following rules. If it is a UNIX process, its process id (pid)
	is always used. If it is an active code host that has a dedicated guest, such a guest
	is always preferred. If it is a host that has called SecHostSelectGuest, such selection
	is considered until revoked.

	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers.
 */
OSStatus SecCodeCopySelf(SecCSFlags flags, SecCodeRef *self);
	
	

/*!
	@function SecCodeCopyStaticCode
	Given a SecCode object, locate its origin in the file system and return
	a SecStaticCode object representing it.
	
	The link established by this call is generally reliable but is NOT guaranteed
	to be secure.
	
	Many API functions taking SecStaticCodeRef arguments will also directly
	accept a SecCodeRef and apply this translation implicitly, operating on
	its result or returning its error code if any. Each of these functions
	calls out that behavior in its documentation.

	@param code A valid SecCode object reference representing code running
	on the system.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param staticCode On successful return, a SecStaticCode object reference representing
	the file system origin of the given SecCode. On error, unchanged.
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeCopyStaticCode(SecCodeRef code, SecCSFlags flags, SecStaticCodeRef *staticCode);


/*!
	@function SecCodeCopyHost
	Given a SecCode object, identify the (different) SecCode object that acts
	as its host. A SecCode's host acts as a supervisor and controller,
	and is the ultimate authority on the its dynamic validity and status.
	The host relationship is securely established (absent reported errors).
	
	@param code A valid SecCode object reference representing code running
	on the system.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param host On successful return, a SecCode object reference identifying
	the code's host.
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeCopyHost(SecCodeRef guest, SecCSFlags flags, SecCodeRef *host);


/*!
	@function SecCodeCopyGuestWithAttributes
	Asks a SecCode object acting as a Code Signing host to identify one of
	its guests by the type and value of specific attribute(s). Different hosts
	support different types and combinations of attributes.
	
	The methods a host uses to identify, separate, and control its guests
	are specific to each type of host. This call provides a generic abstraction layer
	that allows uniform interrogation of all hosts. A SecCode that does not
	act as a host will always return errSecCSNoSuchCode. A SecCode that does
	support hosting may return itself to signify that the attribute refers to
	itself rather than one of its hosts.
	
	@param host A valid SecCode object reference representing code running
	on the system that acts as a Code Signing host. As a special case, passing
	NULL indicates that the Code Signing root of trust should be used as a starting
	point. Currently, that is the system kernel.
	@param attributes A CFDictionary containing one or more attribute selector
	values. Each selector has a CFString key and associated CFTypeRef value.
	The key name identifies the attribute being selected; the associated value,
	whose type depends on the the key name, selects a particular value or other
	constraint on that attribute. Each host only supports particular combinations
	of keys and values,	and errors will be returned if any unsupported set is requested.
	As a special case, NULL is taken to mean an empty attribute set.
	Also note that some hosts that support hosting chains (guests being hosts)
	may return sub-guests in this call. In other words, do not assume that
	a SecCodeRef returned by this call is an immediate guest of the queried host
	(though it will be a proximate guest, i.e. a guest's guest some way down).
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param guest On successful return, a SecCode object reference identifying
	the particular guest of the host that owns the attribute value specified.
	This argument will not be changed if the call fails (does not return noErr).
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers. In particular:
	@error errSecCSUnsupportedGuestAttributes The host does not support the attribute
	type given by attributeType.
	@error errSecCSInvalidAttributeValues The type of value given for a guest
	attribute is not supported by the host.
	@error errSecCSNoSuchCode The host has no guest with the attribute value given
	by attributeValue, even though the value is of a supported type. This may also
	be returned if the host code does not currently act as a Code Signing host.
	@error errSecCSNotAHost The putative host cannot, in fact, act as a code
	host. (It is missing the kSecCodeSignatureHost option flag in its code
	signature.)
	@error errSecCSMultipleGuests The attributes specified do not uniquely identify
	a guest (the specification is ambiguous).
*/
extern const CFStringRef kSecGuestAttributePid;
extern const CFStringRef kSecGuestAttributeCanonical;
extern const CFStringRef kSecGuestAttributeMachPort;

OSStatus SecCodeCopyGuestWithAttributes(SecCodeRef host,
	CFDictionaryRef attributes,	SecCSFlags flags, SecCodeRef *guest);


/*!
	@function SecCodeCreateWithPID
	Asks the kernel to return a SecCode object for a process identified
	by a UNIX process id (pid). This is a shorthand for asking SecGetRootCode()
	for a guest whose "pid" attribute has the given pid value.
	
	@param pid A process id for an existing UNIX process on the system.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param process On successful return, a SecCode object reference identifying
	the requesteed process.
	@result Upon success, noErr. Upon error, an OSStatus value documented in
	CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeCreateWithPID(pid_t pid, SecCSFlags flags, SecCodeRef *process);


/*!
	@function SecCodeCheckValidity
	Performs dynamic validation of the given SecCode object. The call obtains and
	verifies the signature on the code object. It checks the validity of only those
	sealed components required to establish identity. It checks the SecCode's
	dynamic validity status as reported by its host. It ensures that the SecCode's
	host is in turn valid. Finally, it validates the code against a SecRequirement
	if one is given. The call succeeds if all these conditions are satisfactory.
	It fails otherwise.
	
	This call is secure against attempts to modify the file system source of the
	SecCode.

	@param code The code object to be validated.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param requirement An optional code requirement specifying additional conditions
	the code object must satisfy to be considered valid. If NULL, no additional
	requirements are imposed.
	@param errors An optional pointer to a CFErrorRef variable. If the call fails
	(and something other than noErr is returned), and this argument is non-NULL,
	a CFErrorRef is stored there further describing the nature and circumstances
	of the failure. The caller must CFRelease() this error object when done with it.
	@result If validation passes, noErr. If validation fails, an OSStatus value
	documented in CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeCheckValidity(SecCodeRef code, SecCSFlags flags,
	SecRequirementRef requirement);

OSStatus SecCodeCheckValidityWithErrors(SecCodeRef code, SecCSFlags flags,
	SecRequirementRef requirement, CFErrorRef *errors);


/*!
	@function SecCodeCopyPath
	For a given Code or StaticCode object, returns a URL to a location on disk where the
	code object can be found. For single files, the URL points to that file.
	For bundles, it points to the directory containing the entire bundle.
	
	This returns the same URL as the kSecCodeInfoMainExecutable key returned
	by SecCodeCopySigningInformation.

	@param code The Code or StaticCode object to be located. For a Code
		argument, its StaticCode is processed as per SecCodeCopyStaticCode.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param path On successful return, contains a CFURL identifying the location
	on disk of the staticCode object.
	@result On success, noErr. On error, an OSStatus value
	documented in CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeCopyPath(SecStaticCodeRef staticCode, SecCSFlags flags,
	CFURLRef *path);


/*!
	@function SecCodeCopyDesignatedRequirement
	For a given Code or StaticCode object, determines its Designated Code Requirement.
	The Designated Requirement is the SecRequirement that the code believes
	should be used to properly identify it in the future.
	
	If the SecCode contains an explicit Designated Requirement, a copy of that
	is returned. If it does not, a SecRequirement is implicitly constructed from
	its signing authority and its embedded unique identifier. No Designated
	Requirement can be obtained from code that is unsigned. Code that is modified
	after signature, improperly signed, or has become invalid, may or may not yield
	a Designated Requirement. This call does not validate the SecStaticCode argument.
	
	@param code The Code or StaticCode object to be interrogated. For a Code
		argument, its StaticCode is processed as per SecCodeCopyStaticCode.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
	@param requirement On successful return, contains a copy of a SecRequirement
	object representing the code's Designated Requirement. On error, unchanged.
	@result On success, noErr. On error, an OSStatus value
	documented in CSCommon.h or certain other Security framework headers.
*/
OSStatus SecCodeCopyDesignatedRequirement(SecStaticCodeRef code, SecCSFlags flags,
	SecRequirementRef *requirement);


/*
	@function SecCodeCopySigningInformation
	For a given Code or StaticCode object, extract various pieces of information
	from its code signature and return them in the form of a CFDictionary. The amount
	and detail level of the data is controlled by the flags passed to the call.
	
	If the code exists but is not signed at all, this call will succeed and return
	a dictionary that does NOT contain the kSecCodeInfoIdentifier key. This is the
	recommended way to check quickly whether a code is signed.
	
	If the signing data for the code is corrupt or invalid, this call may fail or it
	may return partial data. To ensure that only valid data is returned (and errors
	are raised for invalid data), you must successfully call one of the CheckValidity
	functions on the code before calling CopySigningInformation.
	
	@param code The Code or StaticCode object to be interrogated. For a Code
		argument, its StaticCode is processed as per SecCodeCopyStaticCode.
	@param flags Optional flags. Use any or all of the kSecCS*Information flags
		to select what information to return. A generic set of entries is returned
		regardless; you may specify kSecCSDefaultFlags for just those.
	@param information A CFDictionary containing information about the code is stored
		here on successful completion. The contents of the dictionary depend on
		the flags passed. Regardless of flags, the kSecCodeInfoIdentifier key is
		always present if the code is signed, and always absent if the code is
		unsigned.
	@result On success, noErr. On error, an OSStatus value
	documented in CSCommon.h or certain other Security framework headers.
	
	@constant kSecCSSigningInformation Return cryptographic signing information,
		including the certificate chain and CMS data (if any). For ad-hoc signed
		code, there are no certificates and the CMS data is empty.
	@constant kSecCSRequirementInformation Return information about internal code
		requirements embedded in the code. This includes the Designated Requirement.
	@constant kSecCSInternalInformation Return internal code signing information.
		This information is for use by Apple, and is subject to change without notice.
		It will not be further documented here.
	@constant kSecCSDynamicInformation Return dynamic validity information about
		the Code. The subject code must be a SecCodeRef (not a SecStaticCodeRef).
	@constant kSecCSContentInformation Return more information about the file system
		contents making up the signed code on disk. It is not generally advisable to
		make use of this information, but some utilities (such as software-update
		tools) may find it useful.
 */
enum {
	kSecCSInternalInformation = 1 << 0,
	kSecCSSigningInformation = 1 << 1,
	kSecCSRequirementInformation = 1 << 2,
	kSecCSDynamicInformation = 1 << 3,
	kSecCSContentInformation = 1 << 4
};

													/* flag required to get this value */
extern const CFStringRef kSecCodeInfoCertificates;	/* Signing */
extern const CFStringRef kSecCodeInfoChangedFiles;	/* Content */
extern const CFStringRef kSecCodeInfoCMS;			/* Signing */
extern const CFStringRef kSecCodeInfoTime;			/* Signing */
extern const CFStringRef kSecCodeInfoDesignatedRequirement; /* Requirement */
extern const CFStringRef kSecCodeInfoEntitlements;	/* Requirement */
extern const CFStringRef kSecCodeInfoFormat;		/* generic */
extern const CFStringRef kSecCodeInfoIdentifier;	/* generic */
extern const CFStringRef kSecCodeInfoImplicitDesignatedRequirement; /* Requirement */
extern const CFStringRef kSecCodeInfoMainExecutable; /* generic */
extern const CFStringRef kSecCodeInfoPList;			/* generic */
extern const CFStringRef kSecCodeInfoRequirements;	/* Requirement */
extern const CFStringRef kSecCodeInfoRequirementData; /* Requirement */
extern const CFStringRef kSecCodeInfoStatus;		/* Dynamic */
extern const CFStringRef kSecCodeInfoTrust;			/* Signing */

OSStatus SecCodeCopySigningInformation(SecStaticCodeRef code, SecCSFlags flags,
	CFDictionaryRef *information);


/*
	@function SecCodeSetDetachedSignature
	For a given Code or StaticCode object, explicitly specify the detached signature
	data used to verify it.
	This call unconditionally overrides any signature embedded in the Code and any
	previously specified detached signature; only the signature data specified here
	will be used from now on for this Code object. If NULL data is specified, this
	call reverts to using any embedded signature.
	Any call to this function voids all cached validations for the Code object.
	Validations will be performed again as needed in the future. This call does not,
	by itself, perform or trigger any validations.
	Please note that it is possible to have multiple Code objects for the same static
	or dynamic code entity in the system. This function only attaches signature data
	to the particular SecStaticCodeRef involved. It is your responsibility to understand
	the object graph and pick the right one(s).
	
	@param code A Code or StaticCode object whose signature information is to be changed.
	@param signature A CFDataRef containing the signature data to be used for validating
		the given Code. This must be exactly the data previously generated as a detached
		signature by the SecCodeSignerAddSignature API or the codesign(1) command with
		the -D/--detached option.
		If signature is NULL, discards any previously set signature data and reverts
		to using the embedded signature, if any. If not NULL, the data is retained and used
		for future validation operations.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
 */
OSStatus SecCodeSetDetachedSignature(SecStaticCodeRef code, CFDataRef signature,
	SecCSFlags flags);


/*
	@function SecCodeMapMemory
	For a given Code or StaticCode object, ask the kernel to accept the signing information
	currently attached to it in the caller and use it to validate memory page-ins against it,
	updating dynamic validity state accordingly. This change affects all processes that have
	the main executable of this code mapped.
	
	@param code A Code or StaticCode object representing the signed code whose main executable
		should be subject to page-in validation.
	@param flags Optional flags. Pass kSecCSDefaultFlags for standard behavior.
 */
OSStatus SecCodeMapMemory(SecStaticCodeRef code, SecCSFlags flags);


#ifdef __cplusplus
}
#endif

#endif //_H_SECCODE
