/*
 * Copyright (c) 2009-2010,2012,2014 Apple Inc. All Rights Reserved.
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
 * policytree.c - rfc5280 section 6.1.2 and later policy_tree implementation.
 */

#include "policytree.h"
#include <libDER/oids.h>

#include <utilities/debugging.h>

#include <stdlib.h>

#define DUMP_POLICY_TREE  0

#if !defined(DUMP_POLICY_TREE)
#include <stdio.h>
#endif

policy_set_t policy_set_create(const oid_t *p_oid) {
    policy_set_t policy_set =
        (policy_set_t)malloc(sizeof(*policy_set));
    policy_set->oid_next = NULL;
    policy_set->oid = *p_oid;
    secdebug("policy-set", "%p", policy_set);
    return policy_set;
}

void policy_set_add(policy_set_t *policy_set, const oid_t *p_oid) {
    policy_set_t node = (policy_set_t)malloc(sizeof(*node));
    node->oid_next = *policy_set;
    node->oid = *p_oid;
    *policy_set = node;
    secdebug("policy-set", "%p -> %p", node, node->oid_next);
}

void policy_set_free(policy_set_t node) {
    while (node) {
        policy_set_t next = node->oid_next;
        secdebug("policy-set", "%p -> %p", node, next);
        free(node);
        node = next;
    }
}

bool policy_set_contains(policy_set_t node, const oid_t *oid) {
    for (; node;  node = node->oid_next) {
        if (oid_equal(node->oid, *oid))
            return true;
    }
    return false;
}

void policy_set_intersect(policy_set_t *policy_set, policy_set_t other_set) {
    bool other_has_any = policy_set_contains(other_set, &oidAnyPolicy);
    if (policy_set_contains(*policy_set, &oidAnyPolicy)) {
        policy_set_t node = other_set;
        if (other_has_any) {
            /* Both sets contain anyPolicy so the intersection is anyPolicy
               plus all oids in either set.  */
            while (node) {
                if (!policy_set_contains(*policy_set, &node->oid)) {
                    policy_set_add(policy_set, &node->oid);
                }
            }
        } else {
            /* The result set contains anyPolicy and other_set doesn't. The
               result set should be a copy of other_set.  */
            policy_set_free(*policy_set);
            *policy_set = NULL;
            while (node) {
                policy_set_add(policy_set, &node->oid);
            }
        }
    } else if (!other_has_any) {
        /* Neither set contains any policy oid so remove any values from the
           result set that aren't in other_set. */
        policy_set_t *pnode = policy_set;
        while (*pnode) {
            policy_set_t node = *pnode;
            if (policy_set_contains(other_set, &node->oid)) {
                pnode = &node->oid_next;
            } else {
                *pnode = node->oid_next;
                node->oid_next = NULL;
                policy_set_free(node);
            }
        }
    }
}
