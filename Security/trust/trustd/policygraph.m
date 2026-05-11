//
//  policytree.m
//  Security
//
//

#include <Foundation/Foundation.h>
#include <libDER/oids.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>

#include <utilities/debugging.h>

#include "trust/trustd/policytree.h"
#include "trust/trustd/SecCertificateServer.h"

#define DEBUG_POLICY_GRAPH 0

/* MARK: - */
/* MARK: Policy Set */
/* NOTE: function assumes that the NSData oids were constructed with NoCopy
 * so that we have pointers to the original oid data (in the SecCertificateRef) */
static policy_set_t policy_set_create_from_array(NSArray <NSData *> *oids) {
    policy_set_t result = NULL;
    for (NSData *data_oid in oids) {
        policy_set_t p_node = (policy_set_t)malloc(sizeof(*result));
        p_node->oid.data = (void *)data_oid.bytes;
        p_node->oid.length = data_oid.length;
        p_node->oid_next = result ? result : NULL;
        result = p_node;
    }
    return result;
}

/* MARK: - */
/* MARK: Policy Node */

OS_OBJECT_DECL(policy_node);

@interface policy_node : NSObject <OS_OBJECT_CLASS(policy_node)>
@property (assign) oid_t valid_policy;
@property (assign) policy_qualifier_t qualifier_set;
@property (assign) policy_set_t expected_policy_set;
@property NSHashTable *parents; // WeakObjectsHashTable: Children don't retain their parents
@property NSMutableArray *children; // Parents retain their children

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithOid:(const oid_t *)p_oid qualifier:(policy_qualifier_t)p_q;
- (instancetype)initWithOid:(NSData *)oid
                  qualifier:(policy_qualifier_t)p_q
          expected_policies:(NSArray<NSData*>*)expected;
@end

@implementation policy_node
- (instancetype)initWithOid:(const oid_t *)p_oid
                  qualifier:(policy_qualifier_t)p_q
          expected_policy_set:(CF_CONSUMED policy_set_t)expected {
    if (self = [super init]) {
        self.valid_policy = *p_oid;
        self.qualifier_set = p_q;
        self.expected_policy_set = expected;
        self.parents = [NSHashTable weakObjectsHashTable];
        self.children = [NSMutableArray array];
    }
    return self;
}

- (instancetype)initWithOid:(const oid_t *)p_oid qualifier:(policy_qualifier_t)p_q {
    return [self initWithOid:p_oid qualifier:p_q expected_policy_set:policy_set_create(p_oid)];
}

- (instancetype)initWithOid:(NSData *)oid
                  qualifier:(policy_qualifier_t)p_q
          expected_policies:(NSArray<NSData*>*)expected {
    oid_t p_oid;
    p_oid.data = (void *)oid.bytes;
    p_oid.length = oid.length;

    policy_set_t expected_policy_set = policy_set_create_from_array(expected);
    return [self initWithOid:&p_oid qualifier:p_q expected_policy_set:expected_policy_set];
}

- (void)dealloc {
    policy_set_free(self.expected_policy_set);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"policy_node_t<%p>", self];
}

- (NSString *)debugDescription {
    NSData *oidData = [NSData dataWithBytes:self.valid_policy.data
                                     length:self.valid_policy.length];
    NSString *parents = [self.parents.allObjects componentsJoinedByString:@","];
    NSString *children = [self.children componentsJoinedByString:@","];
    return [NSString stringWithFormat:@"node<%p> (%@) p:%@ c:%@",
            self, oidData.debugDescription, parents, children];
}
@end

static policy_node_t policy_node_create(const oid_t *p_oid, policy_qualifier_t p_q) {
    return [[policy_node alloc] initWithOid:p_oid qualifier:p_q];
}

static policy_node_t policy_node_create_from_datas(NSData *oid, policy_qualifier_t p_q, NSArray<NSData*>*expected_policies) {
    return [[policy_node alloc] initWithOid:oid qualifier:p_q expected_policies:expected_policies];
}

static oid_t policy_node_get_valid_policy(policy_node_t node) {
    policy_node *pnode = (policy_node *)node;
    return pnode.valid_policy;
}

static policy_set_t policy_node_get_expected_policy_set(policy_node_t node) {
    policy_node *pnode = (policy_node *)node;
    return pnode.expected_policy_set;
}

static void policy_node_add_parent(policy_node_t child, policy_node_t parent) {
    policy_node *pnode = (policy_node *)parent;
    [pnode.children addObject:child];
    policy_node *cnode = (policy_node *)child;
    [cnode.parents addObject:parent];
}

static void policy_node_add_parents(policy_node_t child, NSArray <policy_node_t>*parents) {
    for (policy_node_t parent in parents) {
        policy_node_add_parent(child, parent);
    }
}

static NSArray <policy_node_t>*policy_node_get_children(policy_node_t node) {
    policy_node *pnode = (policy_node *)node;
    return pnode.children;
}

static NSArray <policy_node_t>*policy_node_get_parents(policy_node_t node) {
    policy_node *pnode = (policy_node *)node;
    return pnode.parents.allObjects;
}

static void policy_node_remove_child(policy_node_t parent, policy_node_t child) {
    policy_node *pnode = (policy_node *)parent;
    [pnode.children removeObject:child];
}

static void policy_node_replace_expected_policy_set(policy_node_t node, policy_set_t new_expected) {
    policy_node *pnode = (policy_node *)node;
    policy_set_free(pnode.expected_policy_set);
    pnode.expected_policy_set = new_expected;
}

#if DEBUG_POLICY_GRAPH
static NSString *policy_node_copy_debug_description(policy_node_t node) {
    policy_node *pnode = (policy_node *)node;
    return pnode.debugDescription;
}
#endif // DEBUG_POLICY_GRAPH

/* MARK: - */
/* MARK: Policy Graph */
struct policy_graph {
    NSMutableArray <NSMutableSet <policy_node_t>*>* nodes;
    size_t size;
};

/* RFC 9618 Section 5.2
 The initial value of the valid_policy_graph is a single node with valid_policy anyPolicy, an empty qualifier_set, and an expected_policy_set with the single value anyPolicy. This node is considered to be at depth zero.
 */
policy_graph_t policy_graph_create(void) {
    policy_graph_t graph = malloc(sizeof(*graph));
    memset((void*)graph, 0, sizeof(*graph));
    if (!graph) {
        return NULL;
    }
    graph->nodes = [NSMutableArray array];
    policy_node_t root = policy_node_create(&oidAnyPolicy, NULL);
    [graph->nodes addObject:[NSMutableSet setWithObject:root]];
    graph->size = 1;
    return graph;
}

void policy_graph_free(policy_graph_t graph) {
    if (graph) {
        graph->size = 0;
        [graph->nodes removeAllObjects];
        graph->nodes = nil;
        free(graph);
    }
}

static bool policy_graph_is_empty(policy_graph_t graph) {
    if (!graph || !graph->nodes || graph->size == 0) {
        return true;
    }
    return false;
}

static void policy_graph_add_level(policy_graph_t graph) {
    NSMutableSet *nodesAtDepth = [NSMutableSet set];
    [graph->nodes addObject:nodesAtDepth];
}

static bool policy_graph_add_child(policy_graph_t graph, int32_t depth, policy_node_t child) {
    if (!graph || !graph->nodes) {
        return false;
    }
    if (depth < 0 || depth >= (int)(POLICY_TREE_DEPTH_MAX) || (uint64_t)depth > graph->nodes.count) {
        return false;
    }
    if (graph->size >= POLICY_TREE_MAX_NODES) {
        secerror("policy_graph: max nodes (%d) reached at depth %d", POLICY_TREE_MAX_NODES, depth);
        return false;
    }

    NSMutableSet *nodesAtDepth = nil;
    if (graph->nodes.count > (NSUInteger)depth) {
        nodesAtDepth = graph->nodes[(uint32_t)depth];
    } else {
         /* This level should have been created when we started processing cert */
         return false;
    }
    if (![nodesAtDepth containsObject:child]) {
        [nodesAtDepth addObject:child];
        graph->size++;
    }
    return true;
}

static void policy_graph_remove_node(policy_graph_t graph, int32_t depth, policy_node_t node) {
    if (!graph || !graph->nodes) {
        return;
    }
    if (depth < 0 || depth >= (int)(POLICY_TREE_DEPTH_MAX) || (uint64_t)depth >= graph->nodes.count) {
        return;
    }

    NSMutableSet *nodesAtDepth = graph->nodes[(uint32_t)depth];
    if (!nodesAtDepth || nodesAtDepth.count == 0) {
        return;
    }
    /* Remove references to this child from its parents */
    for (policy_node_t parent in policy_node_get_parents(node)) {
        policy_node_remove_child(parent, node);
    }
    [nodesAtDepth removeObject:node];
    graph->size--;
}


static bool policy_graph_for_each_at_depth(policy_graph_t graph, int32_t depth,
                                           bool (^callback)(policy_node_t)) {
    if (!graph || !graph->nodes) {
        return false;
    }
    if (depth < 0 || depth >= (int)(POLICY_TREE_DEPTH_MAX) || (uint64_t)depth >= graph->nodes.count) {
        return false;
    }

    /* Snapshot the set before iterating: callbacks (e.g. b.2 policy-mapping expansion)
     * may add new nodes to this same depth, which would otherwise trigger
     * NSFastEnumerationMutationHandler and crash. */
    NSSet <policy_node_t>*nodes = [graph->nodes[(uint32_t)depth] copy];
    bool match = false;
    for (policy_node_t node in nodes) {
        match |= callback(node);
    }
    return match;
}

static bool policy_graph_for_each(policy_graph_t graph, bool (^callback)(policy_node_t)) {
    if (!graph || !graph->nodes || graph->nodes.count == 0) {
        return false;
    }

    bool match = false;
    for (NSMutableSet <policy_node_t>*nodesAtDepth in graph->nodes) {
        for (policy_node_t node in nodesAtDepth) {
            match |= callback(node);
        }
    }
    return match;
}

#if DEBUG_POLICY_GRAPH
static NSString *policy_graph_copy_debug_description(policy_graph_t graph) {
    int depth = 0;
    NSMutableString *string = [NSMutableString string];
    for (NSMutableSet <policy_node_t>*nodesAtDepth in graph->nodes) {
        [string appendFormat:@"depth %d: ", depth];
        for (policy_node_t node in nodesAtDepth) {
            [string appendFormat:@" %@; ",policy_node_copy_debug_description(node)];
        }
        [string appendString:@"\n"];
        depth++;
    }
    return string;
}
#endif // DEBUG_POLICY_GRAPH

void policy_graph_dump(policy_graph_t graph) {
#if DEBUG_POLICY_GRAPH
    secdebug("policy", "%@", policy_graph_copy_debug_description(graph));
#endif
}

/* RFC 9618 Section 5.3 d.1.i:
 Let parent_nodes be the nodes at depth i-1 in the valid_policy_graph where P-OID is in the expected_policy_set. If parent_nodes is not empty, create a child node as follows: set the valid_policy to P-OID, set the qualifier_set to P-Q, set the expected_policy_set to {P-OID}, and set the parent nodes to parent_nodes.
 */
static bool policy_graph_add_child_if_match(policy_graph_t graph, int32_t depth, policy_node_t child) {
    return policy_graph_for_each_at_depth(graph, depth - 1, ^bool(policy_node_t parent) {
        oid_t child_policy = policy_node_get_valid_policy(child);
        policy_set_t policy_set = policy_node_get_expected_policy_set(parent);
        for (; policy_set != NULL; policy_set = policy_set->oid_next) {
            if (oid_equal(policy_set->oid, child_policy)) {
                policy_node_add_parent(child, parent);
                return policy_graph_add_child(graph, depth, child);
            }
        }
        return false;
    });
}

/* RFC 9618 Section 5.3 d.1.ii:
 If there was no match in step (i) and the valid_policy_graph includes a node of depth i-1 with the valid_policy anyPolicy, generate a child node with the following values: set the valid_policy to P-OID, set the qualifier_set to P-Q, set the expected_policy_set to {P-OID}, and set the parent node to the anyPolicy node at depth i-1. */
static bool policy_graph_add_child_if_any_policy(policy_graph_t graph, int32_t depth, policy_node_t child) {
    return policy_graph_for_each_at_depth(graph, depth - 1, ^bool(policy_node_t parent) {
        oid_t parent_policy = policy_node_get_valid_policy(parent);
        if (oid_equal(parent_policy, oidAnyPolicy)) {
            policy_node_add_parent(child, parent);
            return policy_graph_add_child(graph, depth, child);
        }
        return false;
    });
}

static bool policy_graph_has_policy(policy_graph_t graph, int32_t depth, oid_t oid) {
    return policy_graph_for_each_at_depth(graph, depth, ^bool(policy_node_t node) {
        oid_t valid_policy = policy_node_get_valid_policy(node);
        if (oid_equal(oid, valid_policy)) {
            return true;
        }
        return false;
    });
}

static NSArray <policy_node_t>* policy_graph_nodes_with_expected_oid(policy_graph_t graph, int32_t depth, oid_t oid) {
    NSMutableArray *matchingNodes = [NSMutableArray array];
    policy_graph_for_each_at_depth(graph, depth, ^bool(policy_node_t node) {
        policy_set_t policy_set = policy_node_get_expected_policy_set(node);
        for (; policy_set != NULL; policy_set = policy_set->oid_next) {
            if (oid_equal(policy_set->oid, oid)) {
                [matchingNodes addObject:node];
                break; // Found match, no need to continue with this node
            }
        }
        return true;
    });
    return matchingNodes;
}

/*  RFC 9618 Section 5.3 d.2:
 For each policy OID P-OID (including anyPolicy) that appears in the expected_policy_set of some node in the valid_policy_graph for depth i-1, if P-OID does not appear as the valid_policy of some node at depth i, create a single child node with the following values: set the valid_policy to P-OID, set the qualifier_set to AP-Q, set the expected_policy_set to {P-OID}, and set the parents to the nodes at depth i-1 where P-OID appears in expected_policy_set. */
static bool policy_graph_add_expected_children(policy_graph_t graph, int32_t depth, policy_qualifier_t p_q) {
    return policy_graph_for_each_at_depth(graph, depth - 1, ^bool(policy_node_t parent) {
        policy_set_t policy_set = policy_node_get_expected_policy_set(parent);
        bool nodes_added = true;
        for (; policy_set != NULL; policy_set = policy_set->oid_next) {
            if (!policy_graph_has_policy(graph, depth, policy_set->oid)) {
                policy_node_t child = policy_node_create(&policy_set->oid, p_q);
                nodes_added &= policy_graph_add_child(graph, depth, child);
                NSArray <policy_node_t>*parents = policy_graph_nodes_with_expected_oid(graph, depth - 1, policy_set->oid);
                policy_node_add_parents(child, parents);
            }
        }
        return nodes_added;
    });
}

/* RFC 9618 Section 5.3.d.3 / 5.4 b.3.ii:
 If there is a node in the valid_policy_graph of depth i-1 or less without any child nodes, delete that node. Repeat this step until there are no nodes of depth i-1 or less without children.
 */
static void policy_graph_prune_childless_nodes(policy_graph_t graph, int32_t depth) {
    if (!graph || !graph->nodes || depth <= 0) {
        return;
    }

    for (int32_t i = depth-1; i >= 0; i--) {
        NSMutableArray <policy_node_t>*nodesToRemove = [NSMutableArray array];
        policy_graph_for_each_at_depth(graph, i, ^bool(policy_node_t node) {
            if (policy_node_get_children(node).count == 0) {
                [nodesToRemove addObject:node];
            }
            return true;
        });
        for (policy_node_t node in nodesToRemove) {
            policy_graph_remove_node(graph, i, node);
        }
    }
}

/* RFC 9618 Section 5.3.e:
   If the certificate policies extension is not present, set the valid_policy_graph to NULL.
 */
static void policy_graph_clear(policy_graph_t graph) {
    if (!graph || !graph->nodes) {
        return;
    }
    
    [graph->nodes removeAllObjects];
    graph->size = 0;
}

/* Walk the mappings to generate the dictionary idp->sdps */
static NSDictionary <NSData *, NSMutableArray <NSData *>*>*
create_policy_mappings_dictionary(const SecCEPolicyMappings *pm) {
    size_t mapping_ix, mapping_count = pm->numMappings;
    /* limit the number of mappings we'll apply */
    if (mapping_count < 0 || mapping_count >= (int)(POLICY_MAPPINGS_MAX )) {
        return nil;
    }

    NSMutableDictionary <NSData *, NSMutableArray <NSData *>*>*mappings = [NSMutableDictionary dictionary];
    for (mapping_ix = 0; mapping_ix < mapping_count; mapping_ix++) {
        oid_t issuerDomainPolicy = pm->mappings[mapping_ix].issuerDomainPolicy;
        oid_t subjectDomainPolicy = pm->mappings[mapping_ix].subjectDomainPolicy;
        if (issuerDomainPolicy.length > LONG_MAX || subjectDomainPolicy.length > LONG_MAX) {
            continue;
        }
        /* Use no-copy so that we can use the original oid_t data pointer from the certificate
         * when we use these datas. The SecCertificateRef holds this memory */
        NSData *idp = [NSData dataWithBytesNoCopy:issuerDomainPolicy.data
                                           length:issuerDomainPolicy.length
                                     freeWhenDone:NO];
        NSData *sdp = [NSData dataWithBytesNoCopy:subjectDomainPolicy.data
                                           length:subjectDomainPolicy.length
                                     freeWhenDone:NO];

        NSMutableArray <NSData *>*sdps = [mappings objectForKey:idp];
        if (sdps) {
            [sdps addObject:sdp];
        } else {
            sdps = [NSMutableArray arrayWithObject:sdp];
            [mappings setObject:sdps forKey:idp];
        }
    }
    return mappings;
}

/* RFC 9618 Section 5.4 b:
 For each issuerDomainPolicy ID-P in the policy mappings extension:
 */
static bool policy_graph_process_policy_mappings(policy_graph_t graph, int32_t depth, const SecCEPolicyMappings *pm, policy_qualifier_t anyPolicyQualifier) {
    if (!pm || pm->numMappings == 0) {
        return true;
    }

    NSDictionary <NSData *, NSMutableArray <NSData *>*>*mappings = create_policy_mappings_dictionary(pm);
    if (!mappings) {
        secerror("policy_graph: failed to create mappings dictionary at depth %d (%zu mappings, limit %d)",
                 depth, pm->numMappings, POLICY_MAPPINGS_MAX);
        return false;
    }

    /* b.1
     For each ID-P: if there is a node in the valid_policy_graph of depth i where ID-P is the
     valid_policy, set expected_policy_set to the set of subjectDomainPolicy values that are
     specified as equivalent to ID-P by the policy mappings extension.
     Track which IDPs were matched so b.2 can handle the rest.
     */
    NSMutableSet <NSData *>*matched_idps = [NSMutableSet set];
    policy_graph_for_each_at_depth(graph, depth, ^bool(policy_node_t node) {
        oid_t node_policy = policy_node_get_valid_policy(node);
        NSData *node_policy_data = [NSData dataWithBytes:node_policy.data length:node_policy.length];
        for (NSData *idp in mappings) {
            if ([node_policy_data isEqual:idp]) {
                policy_set_t new_expected_policies = policy_set_create_from_array([mappings objectForKey:idp]);
                policy_node_replace_expected_policy_set(node, new_expected_policies);
                [matched_idps addObject:idp];
                return true;
            }
        }
        return false;
    });

    /* b.2
     For each ID-P that was NOT matched in b.1: if there is a node of depth i with a valid_policy
     of anyPolicy, generate a child node of the node of depth i-1 that has a valid_policy of
     anyPolicy as follows:
            (i) set the valid_policy to ID-P;
            (ii) set the qualifier_set to the qualifier set of the policy anyPolicy in the
                 certificate policies extension of certificate i; and
            (iii) set the expected_policy_set to the set of subjectDomainPolicy values that are
                  specified as equivalent to ID-P by the policy mappings extension.
     */
    NSMutableSet <NSData *>*unmatched_idps = [NSMutableSet setWithArray:mappings.allKeys];
    [unmatched_idps minusSet:matched_idps];
    if (unmatched_idps.count > 0) {
        policy_graph_for_each_at_depth(graph, depth, ^bool(policy_node_t node) {
            oid_t node_policy = policy_node_get_valid_policy(node);
            if (!oid_equal(node_policy, oidAnyPolicy)) {
                return false;
            }

            NSArray <policy_node_t>*parents = policy_node_get_parents(node);
            if (!parents || parents.count != 1) {
                /* An anyPolicy child should have exactly one parent with valid_policy anyPolicy */
                return false;
            }
            policy_node_t parent = parents[0];
            bool nodes_added = true;
            for (NSData *idp in unmatched_idps) {
                NSArray <NSData *>*sdps = [mappings objectForKey:idp];
                policy_node_t new_child = policy_node_create_from_datas(idp, anyPolicyQualifier, sdps);
                policy_node_add_parent(new_child, parent);
                nodes_added &= policy_graph_add_child(graph, depth, new_child);
            }
            return nodes_added;
        });
    }
    return true;
}

/* RFC 9618 Section 5.4 b.3:
    (i) delete the node, if any, of depth i in the valid_policy_graph where ID-P is the valid_policy.
    (ii) If there is a node in the valid_policy_graph of depth i-1 or less without any child nodes, delete that node. Repeat this step until there are no nodes of depth i-1 or less without children.
 */
static bool policy_graph_delete_mapped_policies(policy_graph_t graph, int32_t depth, const SecCEPolicyMappings *pm) {
    if (!pm || pm->numMappings == 0) {
        return true;
    }
    
    size_t mapping_count = pm->numMappings;
    if (mapping_count >= POLICY_MAPPINGS_MAX) {
        return false;
    }
    
    // Collect nodes to remove (we can't modify while iterating)
    NSMutableArray *nodesToRemove = [NSMutableArray array];

    for (size_t mapping_ix = 0; mapping_ix < mapping_count; ++mapping_ix) {
        const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
        policy_graph_for_each_at_depth(graph, depth, ^bool(policy_node_t node) {
            oid_t node_policy = policy_node_get_valid_policy(node);
            if (oid_equal(node_policy, mapping->issuerDomainPolicy)) {
                [nodesToRemove addObject:node];
            }
            return true;
        });
    }

    // Remove identified nodes
    for (policy_node_t node in nodesToRemove) {
        policy_graph_remove_node(graph, depth, node);
    }

    // Prune tree (step ii)
    policy_graph_prune_childless_nodes(graph, depth);
    return true;
}

/* RFC 9618 Section 5.3 - Main policy graph verification function
   This implements the complete policy graph algorithm for certificate path validation
 */
bool policy_graph_verify_path(policy_graph_t graph,
                              SecCertificatePathVCRef path,
                              bool anchor_trusted,
                              uint32_t *explicit_policy,
                              uint32_t *inhibit_any_policy,
                              uint32_t *policy_mapping) {
    if (!graph || !path) {
        return false;
    }
    
    int32_t n = (int32_t)SecCertificatePathVCGetCount(path);
    if (anchor_trusted && n > 0) {
        n--;
    }
    
    // Process each certificate in the path
    for (int32_t i = 1; i <= n; ++i) {
        SecCertificateRef cert = SecCertificatePathVCGetCertificateAtIndex(path, n - i);
        if (!cert) {
            continue;
        }

        policy_graph_add_level(graph);
        bool is_self_issued = SecCertificatePathVCIsCertificateAtIndexSelfIssued(path, n - i);
        const SecCECertificatePolicies *cp = SecCertificateGetCertificatePolicies(cert);
        policy_qualifier_t anyPolicyQualifier = NULL;

        // Section 5.3.d: Process certificate policies
        if (cp && cp->numPolicies > 0) {
            for (size_t policy_ix = 0; policy_ix < cp->numPolicies; ++policy_ix) {
                const SecCEPolicyInformation *policy = &cp->policies[policy_ix];
                oid_t p_oid = policy->policyIdentifier;
                policy_qualifier_t p_q = &policy->policyQualifiers;
                
                if (!oid_equal(p_oid, oidAnyPolicy)) {
                    // d.1: add child for matching policies or anyPolicy
                    policy_node_t newChild = policy_node_create(&p_oid, p_q);
                    if (!policy_graph_add_child_if_match(graph, i, newChild)) {
                        policy_graph_add_child_if_any_policy(graph, i, newChild);
                    }
                }
            }
            
            // d.2 Handle anyPolicy if conditions are met
            for (size_t policy_ix = 0; policy_ix < cp->numPolicies; ++policy_ix) {
                const SecCEPolicyInformation *policy = &cp->policies[policy_ix];
                if (oid_equal(policy->policyIdentifier, oidAnyPolicy)) {
                    if (*inhibit_any_policy > 0 || (i < n && is_self_issued)) {
                        policy_graph_add_expected_children(graph, i, &policy->policyQualifiers);
                    }
                    anyPolicyQualifier = &policy->policyQualifiers;
                }
            }
            
            // d.3: Prune childless nodes from previous depth
            policy_graph_prune_childless_nodes(graph, i);
        } else {
            // Section 5.3.e: No certificate policies extension
            policy_graph_clear(graph);
        }
        
        // Section 5.3.f: Verify that either explicit_policy is greater than 0 or the valid_policy_graph is not equal to NULL.
        // Because we remove all nodes from the graph (above) rather than setting to null above, this is saying:
        // (*explicit_policy > 0 || graph->size > 0)
        // So if !(*explicit_policy > 0 || graph->size > 0), we need to fail. So applying DeMorgan's law:
        if (*explicit_policy <= 0 && graph->size <= 0) {
            secerror("policy_graph: empty graph and explicit_policy=0 at depth %d (cert %d)", i, n - i);
            return false;
        }
        
        // If this is the last certificate, we're done with the main loop
        if (i == n) {
            break;
        }
        
        // Process policy mappings for intermediate certificates
        const SecCEPolicyMappings *pm = SecCertificateGetPolicyMappings(cert);
        if (pm && pm->present) {
            // RFC 5280 Section 6.1.4 a Validate policy mappings don't use anyPolicy
            for (size_t mapping_ix = 0; mapping_ix < pm->numMappings; ++mapping_ix) {
                const SecCEPolicyMapping *mapping = &pm->mappings[mapping_ix];
                if (oid_equal(mapping->issuerDomainPolicy, oidAnyPolicy) ||
                    oid_equal(mapping->subjectDomainPolicy, oidAnyPolicy)) {
                    secerror("policy_graph: anyPolicy in policy mapping at depth %d (cert %d), mapping %zu", i, n - i, mapping_ix);
                    return false; // anyPolicy not allowed in mappings
                }
            }

            if (*policy_mapping > 0) {
                // RFC 9618 Section 5.4 b.1 &1.2
                if (!policy_graph_process_policy_mappings(graph, i, pm, anyPolicyQualifier)) {
                    secerror("policy_graph: failed to process policy mappings at depth %d (cert %d)", i, n - i);
                    return false;
                }
            } else {
                // RFC 9618 Section 5.4 b.3 Delete mapped policies when policy_mapping is 0
                if (!policy_graph_delete_mapped_policies(graph, i, pm)) {
                    secerror("policy_graph: failed to delete mapped policies at depth %d (cert %d)", i, n - i);
                    return false;
                }
            }
        }
        
        // RFC 5280 6.1.4 h Update state variables for next iteration
        if (!is_self_issued) {
            if (*explicit_policy > 0) (*explicit_policy)--;
            if (*policy_mapping > 0) (*policy_mapping)--;
            if (*inhibit_any_policy > 0) (*inhibit_any_policy)--;
        }
        
        // RFC 5280 6.1.4 i Process policy constraints
        const SecCEPolicyConstraints *pc = SecCertificateGetPolicyConstraints(cert);
        if (pc) {
            if (pc->requireExplicitPolicyPresent && pc->requireExplicitPolicy < *explicit_policy) {
                *explicit_policy = pc->requireExplicitPolicy;
            }
            if (pc->inhibitPolicyMappingPresent && pc->inhibitPolicyMapping < *policy_mapping) {
                *policy_mapping = pc->inhibitPolicyMapping;
            }
        }
        
        // RFC 5280 6.1.4 j Process inhibit any policy extension
        const SecCEInhibitAnyPolicy *iap = SecCertificateGetInhibitAnyPolicySkipCerts(cert);
        if (iap && iap->skipCerts < *inhibit_any_policy) {
            *inhibit_any_policy = iap->skipCerts;
        }
    }
    
    return true;
}


/* RFC 9618 Section 5.5 g */
CFArrayRef policy_graph_copy_constrained_policy_set(policy_graph_t graph, int32_t n) {
    /*  (1) If the valid_policy_graph is NULL, set valid_policy_node_set to the empty set.*/
    if (policy_graph_is_empty(graph)) {
        return CFBridgingRetain([NSArray array]);
    }

    /*  (2) If the valid_policy_graph is not NULL, set valid_policy_node_set to the set of policy nodes whose valid_policy is not anyPolicy and whose parent list is a single node with valid_policy of anyPolicy. */
    NSMutableArray *valid_policy_node_set = [NSMutableArray array];
    policy_graph_for_each(graph, ^bool(policy_node_t node) {
        oid_t node_policy = policy_node_get_valid_policy(node);
        if (!oid_equal(node_policy, oidAnyPolicy)) {
            NSArray <policy_node_t> *parents = policy_node_get_parents(node);
            if (parents.count == 1) {
                policy_node_t parent = parents[0];
                oid_t parent_policy = policy_node_get_valid_policy(parent);
                if (oid_equal(parent_policy, oidAnyPolicy)) {
                    [valid_policy_node_set addObject:node];
                }
            }
        }
        return true;
    });

    /* (3) If the valid_policy_graph is not NULL and contains a node of depth n with the valid_policy anyPolicy, add it to valid_policy_node_set. */
    policy_graph_for_each_at_depth(graph, n, ^bool(policy_node_t node) {
        oid_t node_policy = policy_node_get_valid_policy(node);
        if (oid_equal(node_policy, oidAnyPolicy)) {
            [valid_policy_node_set addObject:node];
        }
        return true;
    });

    /* (4) Compute authority_constrained_policy_set, a set of policy OIDs and associated qualifiers as follows. For each node in valid_policy_node_set: */
    NSMutableArray * authority_constrained_policy_set = [NSMutableArray array];
    for (policy_node_t node in valid_policy_node_set) {
        /* (i) Add the node's valid_policy to authority_constrained_policy_set. */
        oid_t node_policy = policy_node_get_valid_policy(node);
        NSData *policyData = [NSData dataWithBytes:node_policy.data length:node_policy.length];
        [authority_constrained_policy_set addObject:policyData];

        /* (ii) Applications that do not use policy qualifiers MAY skip this step to simplify processing. */
    }

    /* (5) Set user_constrained_policy_set to authority_constrained_policy_set. */
    /* Skip (6) because user-initial-policy-set is anyPolicy */
    return CFBridgingRetain(authority_constrained_policy_set);
}
