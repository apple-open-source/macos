/*
 * Copyright © 2017-2024 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/*!
 * @header
 * Encapsulation which describes an Image4 identifier. An identifier expresses
 * the four character code in a signing request as well as the constraint that
 * the environment places on it.
 *
 * Identifiers are not created directly; they are always provided to the caller
 * by other API and exist within the context of an environment.
 */
#ifndef __IMAGE4_API_IDENTIFIER_H
#define __IMAGE4_API_IDENTIFIER_H

#include <image4/image4.h>
#include <image4/types.h>
#include <image4/coprocessor.h>

__BEGIN_DECLS
OS_ASSUME_NONNULL_BEGIN
OS_ASSUME_PTR_ABI_SINGLE_BEGIN

#pragma mark Types
/*!
 * @typedef image4_identifier_constraint_t
 * An enumeration expressing the type of constraint the identifier places on the
 * environment.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_EQ
 * The identifier must match the value in the manifest.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_LT
 * The identifier must be less than the value in the manifest.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_LE
 * The identifier must be less than or equal to the value in the manifest.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_GT
 * The identifier must be greater than the value in the manifest.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_GE
 * The identifier must be greater than or equal the value in the manifest.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_NE
 * The identifier must not be equal to the value in the manifest.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_UN
 * The identifier is not constrained at all.
 *
 * @const IMAGE4_IDENTIFIER_CONSTRAINT_NA
 * The identifier's constraints are not known, or the identifier does not
 * represent a constraint.
 */
OS_CLOSED_ENUM(image4_identifier_constraint, uint64_t,
	IMAGE4_IDENTIFIER_CONSTRAINT_EQ,
	IMAGE4_IDENTIFIER_CONSTRAINT_LT,
	IMAGE4_IDENTIFIER_CONSTRAINT_LE,
	IMAGE4_IDENTIFIER_CONSTRAINT_GT,
	IMAGE4_IDENTIFIER_CONSTRAINT_GE,
	IMAGE4_IDENTIFIER_CONSTRAINT_NE,
	IMAGE4_IDENTIFIER_CONSTRAINT_UN,
	IMAGE4_IDENTIFIER_CONSTRAINT_NA,
);

#pragma mark API
/*!
 * @function image4_identifier_get_constraint
 * Obtain the constraint for the identifier.
 *
 * @param id4
 * The identifier to query.
 *
 * @result
 * The constraint which the environment places on the identifier.
 */
IMAGE4_API_AVAILABLE_SPRING_2024
OS_EXPORT OS_WARN_RESULT OS_NONNULL1
image4_identifier_constraint_t
image4_identifier_get_constraint(const image4_identifier_t *id4);
IMAGE4_XNU_AVAILABLE_DIRECT(image4_identifier_get_constraint);

/*!
 * @function image4_identifier_get_constraint
 * Obtain a C string representation of the constraint for the identifier.
 *
 * @param id4
 * The identifier to query.
 *
 * @result
 * The C string representation of the constraint which the identifier places on
 * the environment.
 */
IMAGE4_API_AVAILABLE_SPRING_2024
OS_EXPORT OS_WARN_RESULT OS_NONNULL1
const char *
image4_identifier_get_constraint_cstr(const image4_identifier_t *id4);
IMAGE4_XNU_AVAILABLE_DIRECT(image4_identifier_get_constraint_cstr);

/*!
 * @function image4_identifier_get_fourcc
 * Obtain the four character code for the identifier.
 *
 * @param id4
 * The identifier to query.
 *
 * @result
 * The four character code which represents the identifier in a manifest.
 */
IMAGE4_API_AVAILABLE_SPRING_2024
OS_EXPORT OS_WARN_RESULT OS_NONNULL1
uint32_t
image4_identifier_get_fourcc(const image4_identifier_t *id4);
IMAGE4_XNU_AVAILABLE_DIRECT(image4_identifier_get_fourcc);

/*!
 * @function image4_identifier_get_fourcc_cstr
 * Obtain the C string representation of the four character code for the
 * identifier.
 *
 * @param id4
 * The identifier to query.
 *
 * @result
 * The C string representation of the four character code which represents the
 * identifier in a manifest.
 */
IMAGE4_API_AVAILABLE_SPRING_2024
OS_EXPORT OS_WARN_RESULT OS_NONNULL1
const char *
image4_identifier_get_fourcc_cstr(const image4_identifier_t *id4);
IMAGE4_XNU_AVAILABLE_DIRECT(image4_identifier_get_fourcc_cstr);

OS_ASSUME_PTR_ABI_SINGLE_END
OS_ASSUME_NONNULL_END
__END_DECLS

#endif // __IMAGE4_API_IDENTIFIER_H
