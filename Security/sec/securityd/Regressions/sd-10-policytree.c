/*
 * Copyright (c) 2009 Apple Inc. All Rights Reserved.
 */

#include <securityd/policytree.h>

#include <stdlib.h>
#include <unistd.h>
#include <libDER/oids.h>

#include "securityd_regressions.h"

#define DUMP_POLICY_TREE  0

int verbose = DUMP_POLICY_TREE;

static bool randomly_add_children(policy_tree_t node, void *ctx) {
    int i, count;
    uint32_t rnd = arc4random();
#if 1
    count = rnd % 7;
    if (count > 4)
        count  = 0;
#else
    if (rnd < 0x40000000) {
        count = 1;
    } else if (rnd < 0x80000000) {
        count = 2;
    } else if (rnd < 0xc0000000) {
        count = 3;
    } else if (rnd < 0xf0000000) {
        count = 4;
    } else {
        count = 0;
    }
#endif

#if DUMP_POLICY_TREE
    diag("node %p add %d children", node, count);
#endif
    for (i = 1; i <= count ; ++i) {
        policy_tree_add_child(node, &oidAnyPolicy, NULL);
        //diag("node %p %d/%d children added", node, i, count);
        //policy_tree_dump(node);
    }
    return count != 0;
}

static void tests(void)
{
    policy_qualifier_t p_q = NULL;
    policy_tree_t tree;
    ok(tree = policy_tree_create(&oidAnyPolicy, p_q),
        "create tree root");
    if (verbose) policy_tree_dump(tree);

#if 0
    int i, count = 4;
    for (i = 1; i <= count ; ++i) {
        policy_tree_add_child(tree, &oidAnyPolicy, NULL);
#if DUMP_POLICY_TREE
        diag("node %p %d/%d children added", tree, i, count);
#endif
    }
    policy_tree_dump(tree);
#else
    int depth;
    for (depth = 0; tree && depth < 7; ++depth) {
        bool added = false;
        while (!added) {
            added = policy_tree_walk_depth(tree, depth,
                randomly_add_children, NULL);
#if DUMP_POLICY_TREE
            diag("depth: %d %s", depth,
                (added ? "added children" : "no children added"));
#endif
        }
        if (verbose) policy_tree_dump(tree);
#if DUMP_POLICY_TREE
        diag("prune_childless depth: %d", depth);
#endif
        policy_tree_prune_childless(&tree, depth);
        if (verbose) {
            if (tree)
                policy_tree_dump(tree);
            else {
#if DUMP_POLICY_TREE
                diag("tree empty at depth: %d", depth);
#endif
                break;
            }
        }
    }
#endif
    if (tree)
        policy_tree_prune(&tree);
}

int sd_10_policytree(int argc, char *const *argv)
{
	plan_tests(1);


	tests();

	return 0;
}
