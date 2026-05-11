/*
 * Copyright (c) 2009-2010,2012-2014 Apple Inc. All Rights Reserved.
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
    @header policytree
    The functions provided in policytree.h provide an interface to
    a policy_tree implementation as specified in section 6 of rfc5280.
*/

#ifndef _SECURITY_POLICYTREE_H_
#define _SECURITY_POLICYTREE_H_

#include <libDER/libDER.h>
#include <stdbool.h>

#include "trust/trustd/SecCertificateServer.h"

__BEGIN_DECLS

#define POLICY_MAPPINGS_MAX 20
#define POLICY_TREE_DEPTH_MAX 15
#define POLICY_TREE_MAX_NODES 512

#define oid_equal(oid1, oid2) DEROidCompare(&oid1, &oid2)
typedef DERItem oid_t;
typedef DERItem der_t;

typedef struct policy_set *policy_set_t;
struct policy_set {
    oid_t oid;
    policy_set_t oid_next;
};

policy_set_t policy_set_create(const oid_t *p_oid);
void policy_set_add(policy_set_t *policy_set, const oid_t *p_oid);
void policy_set_intersect(policy_set_t *policy_set, policy_set_t other_set);
bool policy_set_contains(policy_set_t policy_set, const oid_t *oid);
void policy_set_free(policy_set_t policy_set);

typedef const DERItem *policy_qualifier_t;

typedef struct policy_graph *policy_graph_t;
policy_graph_t policy_graph_create(void);
void policy_graph_free(policy_graph_t graph);
void policy_graph_dump(policy_graph_t graph);

// RFC 9618 policy graph verification function
bool policy_graph_verify_path(policy_graph_t graph,
                              SecCertificatePathVCRef path,
                              bool anchor_trusted,
                              uint32_t *explicit_policy,
                              uint32_t *inhibit_any_policy,
                              uint32_t *policy_mapping);

CFArrayRef policy_graph_copy_constrained_policy_set(policy_graph_t graph, int32_t n);

__END_DECLS

#endif /* !_SECURITY_POLICYTREE_H_ */
