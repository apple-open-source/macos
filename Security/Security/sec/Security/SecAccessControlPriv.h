/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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
 @header SecAccessControlPriv
 SecAccessControl defines access rights for items.
 */

#ifndef _SECURITY_SECACCESSCONTROLPRIV_H_
#define _SECURITY_SECACCESSCONTROLPRIV_H_

#include <Security/SecBase.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFData.h>
#include <CoreFoundation/CFDictionary.h>

__BEGIN_DECLS

/*! Creates new empty access control object. */
SecAccessControlRef SecAccessControlCreate(CFAllocatorRef allocator, CFErrorRef *error);

// Protection, currently only kSecAttrAccessible* constants are allowed.  In future, another probable protection type might be CTK key object ID.
CFTypeRef SecAccessControlGetProtection(SecAccessControlRef access_control);
bool SecAccessControlSetProtection(SecAccessControlRef access_control, CFTypeRef protection, CFErrorRef *error);

/*! Represents constraint of the operation. */
typedef CFTypeRef SecAccessConstraintRef;

/*! Creates constraint based on specified policy.
    @param policy Identification of policy to be used.
 */
SecAccessConstraintRef SecAccessConstraintCreatePolicy(CFTypeRef policy, CFErrorRef *error);

/*! Creates constraint which requires passcode verification.
    @param systemPasscode If true, system passcode (device-specific) will be used, otehrwise application-specific passcode will be used.
 */
SecAccessConstraintRef SecAccessConstraintCreatePasscode(bool systemPasscode);

/*! Creates constraint which requires TouchID verification.
    @param uuid Identification of finger to be verified, or NULL if any enrolled finger can be used for verification.
 */
SecAccessConstraintRef SecAccessConstraintCreateTouchID(CFDataRef uuid, CFErrorRef *error);

/*! Creates constraint composed of other constraints.
    @param numRequired Number of constraints required to be satisfied in order to consider overal constraint satisfied.
    @param constraints Array of constraints to be chosen from.
 */
SecAccessConstraintRef SecAccessConstraintCreateKofN(size_t numRequired, CFArrayRef constraints, CFErrorRef *error);

/*! Sets additional option on the constraint. */
bool SecAccessConstraintSetOption(SecAccessConstraintRef constraint, CFTypeRef option, CFTypeRef value, CFErrorRef *error);

/*! Adds new constraint for specified operation.
    @param access_control Instance of access control object to add constraint to.
    @param operation Operation type.
    @param constraint Constraint object, created by one of SecAccessControlConstraintCreate() functions or kCFBooleanTrue
                      meaning that operation will be always allowed.
 */
bool SecAccessControlAddConstraintForOperation(SecAccessControlRef access_control, CFTypeRef operation,
                                               SecAccessConstraintRef constraint, CFErrorRef *error);

/*! Removes constraint for specified operation.  Does nothing if no constraint exist for specified operation.
    @param access_control Instance of access control object to remove constraint from.
    @param operation Operation type.
 */
void SecAccessControlRemoveConstraintForOperation(SecAccessControlRef access_control, CFTypeRef operation);

/*! Sets or removes default access groups for access control object.
    @param access_control Instance of access control object to set default access group to.
    @param access_groups Set of access groups used if constraint does not contain specific access group.
 */
void SecAccessControlSetAccessGroups(SecAccessControlRef access_control, CFArrayRef access_groups);

/*! Retrieves set of access group which applies for specified operation.
    @param access_control Instance of access control object to query.
    @param operation Operation type.
    @return Set of access groups valid for specified operation, or NULL if no applicable access group was specified.
 */
CFArrayRef SecAccessControlGetAccessGroups(SecAccessControlRef access_control, CFTypeRef operation);

/*! Retrieves dictionary with constraint applicable for specified operation.
    @param access_control Instance of access control object to query.
    @param operation Operation type.
    @return Dictionary or kCFBooleanTrue representing constraint applied for requested operation.  If the operation
            is not allowed at all, NULL is returned.
 */
SecAccessConstraintRef SecAccessControlGetConstraint(SecAccessControlRef access_control, CFTypeRef operation);

/*! Retrieves dictionary with constraints keyed by operations (i.e. the ACL part of access control object).
    @return Dictionary with all constraints keyed by operation types.  Returns NULL if no operations are constrained.
 */
CFDictionaryRef SecAccessControlGetConstraints(SecAccessControlRef access_control);

/*! Sets dictionary with constraints for access control object.
 @param access_control Instance of access control object to set default access group to.
 @param constraints Constraint with all constraints.
 */
void SecAccessControlSetConstraints(SecAccessControlRef access_control, CFDictionaryRef constraints);

/*! Creates Access control instance from data serialized by SecAccessControlCopyData(). */
SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error);

/*! Serializes all access control object into binary data form. */
CFDataRef SecAccessControlCopyData(SecAccessControlRef access_control);

__END_DECLS

#endif // _SECURITY_SECACCESSCONTROLPRIV_H_
