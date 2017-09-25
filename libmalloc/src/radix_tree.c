/*
 * Copyright (c) 2016 Apple Inc. All rights reserved.
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

#include <assert.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/reason.h>
#include <unistd.h>

#include <radix_tree.h>
#include <radix_tree_internal.h>

#if 0
int radix_tree_indent = 0;
bool radix_tree_should_print __attribute__((visibility("default")))= true;
#include <stdio.h>
#define D(s, ...)                                   \
	if (radix_tree_should_print) {                  \
		for (int i = 0; i < radix_tree_indent; i++) \
			putchar(' ');                           \
		printf(s, __VA_ARGS__);                     \
	}
#define DINDENT(x)                    \
	if (radix_tree_should_print) {    \
		for (int i = 0; i < (x); i++) \
			putchar(' ');             \
	}
#define DINC(x) radix_tree_indent += x;
#define DDEC(x) radix_tree_indent -= x;
#else
#define DINDENT(x)
#define D(s, ...)
#define DINC(x)
#define DDEC(x)
#endif

static void __attribute__((noreturn)) radix_tree_panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	char buf[256];
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);
	abort_with_reason(OS_REASON_TEST, 0, buf, 0);
}

struct interval {
	uint64_t start;
	uint64_t size;
};

struct answer {
	struct interval interval;
	uint64_t stackid;
	uint64_t limit;
};

static inline bool
in_interval(uint64_t x, struct interval interval)
{
	return x >= interval.start && ((x - interval.start) < interval.size);
}

static inline bool
intervals_intersect(struct interval a, struct interval b)
{
	if (a.size == 0 || b.size == 0)
		return false;
	return (in_interval(a.start, b) || in_interval(a.start + a.size - 1, b) || in_interval(b.start, a) ||
			in_interval(b.start + b.size - 1, a));
}

__unused static inline bool
interval_is_subset(struct interval a, struct interval b)
{
	return in_interval(a.start, b) && in_interval(a.start + a.size - 1, b);
}

static inline struct interval
truncate_interval(struct interval a, uint64_t limit)
{
	if (a.start >= limit) {
		return (struct interval){.start = a.start, .size = 0};
	} else {
		return (struct interval){.start = a.start, .size = limit - a.start};
	}
}

static inline bool
answer_found(struct answer answer)
{
	return answer.stackid != radix_tree_invalid_value;
}

/*
 * Modify the node to maintain the invariant that the lesser edge is first.
 * Return true if node needed to be modified.
 */
static bool
fixnode(struct radix_node *node)
{
	bool swap = false;
	if (node->edges[0].labelBits && node->edges[1].labelBits) {
		unsigned label0 = node->edges[0].label << (RADIX_LABEL_BITS - node->edges[0].labelBits);
		unsigned label1 = node->edges[1].label << (RADIX_LABEL_BITS - node->edges[1].labelBits);
		if (label1 < label0)
			swap = true;
	} else if (node->edges[0].labelBits == 0 && node->edges[1].labelBits != 0) {
		swap = true;
	}
	if (swap) {
		struct radix_edge edge0 = node->edges[0];
		node->edges[0] = node->edges[1];
		node->edges[1] = edge0;
	}
	return swap;
}

static struct answer
radix_tree_lookup_recursive(struct radix_tree *tree,
		struct interval keys,	 // keys we're looking for
		struct interval nodekeys, // keys it is possible that we will find
		struct radix_node *node,
		int keyshift)
{
	DINDENT(keyshift);
	D("LOOKUPREC %p keys=[%llx-%llx] nodekeys=[%llx-%llx]\n", node, keys.start, keys.start + keys.size, nodekeys.start,
			nodekeys.start + nodekeys.size);

	assert(node);
	assert(intervals_intersect(nodekeys, keys));
	assert(keyshift < RADIX_TREE_KEY_BITS);
	assert(!fixnode(node));

	if (keys.start < nodekeys.start) {
		uint64_t diff = nodekeys.start - keys.start;
		if (keys.size > diff) {
			keys.start += diff;
			keys.size -= diff;
			assert(keys.start == nodekeys.start);
		} else {
			DINDENT(keyshift);
			D("LOOKUPREC(<) quit keys.size=%llx diff=%llx\n", keys.size, diff);
			return (struct answer){.limit = nodekeys.start, .stackid = radix_tree_invalid_value};
		}
		DINDENT(keyshift);
		D("LOOKUPREC(<) %p keys=[%llx-%llx]\n", node, keys.start, keys.start + keys.size);
	}

	assert(keys.start >= nodekeys.start);
	assert(intervals_intersect(nodekeys, keys));

	for (int i = 1; i >= 0; i--) {
		struct radix_edge *edge = &node->edges[i];
		if (!edge_valid(edge)) {
			continue;
		}
		uint64_t edgekeys_start = extend_key(nodekeys.start, edge->labelBits, keyshift, edge->label);
		assert(edgekeys_start >= nodekeys.start);
		struct interval edgekeys = {.start = edgekeys_start, .size = nodekeys.size - (edgekeys_start - nodekeys.start)};

		DINDENT(keyshift);
		D("LOOKUPREC(edge%d) edgekeys=[%llx-%llx] nodekeys=[%llx-%llx]\n", i, edgekeys.start, edgekeys.start + edgekeys.size,
				nodekeys.start, nodekeys.start + nodekeys.size);

		assert(interval_is_subset(edgekeys, nodekeys));
		if (intervals_intersect(edgekeys, keys)) {
			if (edge->isLeaf) {
				struct radix_node *leaf = getnode(tree, edge->index);
				assert(leaf);
				assert(keyshift + edge->labelBits == RADIX_TREE_KEY_BITS);
				uint64_t size = leaf_size(tree, leaf);
				assert(size <= edgekeys.size);
				edgekeys.size = size; // edgekeys is now exact.
				if (intervals_intersect(edgekeys, keys)) {
					DINDENT(keyshift);
					D("LOOKUPREC(found) leaf=(%d)%p %llx\n", edge->index, leaf, leaf->stackid);
					return (struct answer){.interval = edgekeys, .stackid = leaf->stackid};
				}
				nodekeys = truncate_interval(nodekeys, edgekeys.start);
			} else {
				struct answer answer =
						radix_tree_lookup_recursive(tree, keys, edgekeys, getnode(tree, edge->index), keyshift + edge->labelBits);
				if (answer_found(answer)) {
					DINDENT(keyshift);
					D("LOOKUPREC(found) %llx\n", answer.stackid);
					return answer;
				}
				nodekeys = truncate_interval(nodekeys, answer.limit);
			}
		}
	}

	struct answer ans = {.limit = nodekeys.start + nodekeys.size, .stackid = radix_tree_invalid_value};
	DINDENT(keyshift);
	D("LOOKUPREC(notfound) limit=%llx\n", ans.limit);
	return ans;
}

static struct answer
radix_tree_lookup_interval(struct radix_tree *tree, struct interval keys)
{
	struct interval max_interval = {.start = 0, .size = (uint64_t)-1};
	struct answer ans = radix_tree_lookup_recursive(tree, keys, max_interval, getnode(tree, 0), 0);
	D("LOOKUP [%llx-%llx] -> [%llx, %llx] %llx\n", keys.start, keys.start + keys.size, ans.interval.start,
			ans.interval.start + ans.interval.size, ans.stackid);
	assert(!answer_found(ans) || intervals_intersect(keys, ans.interval));
	return ans;
}

uint64_t
radix_tree_lookup(struct radix_tree *tree, uint64_t key)
{
	return radix_tree_lookup_interval(tree, (struct interval){.start = key, .size = 1}).stackid;
}

static void radix_tree_grow(struct radix_tree **treep);

static unsigned
radix_tree_allocate_node(struct radix_tree **treep)
{
	if (!(*treep)->next_free)
		radix_tree_grow(treep);

	if (!(*treep)->next_free)
		return 0;

	struct radix_tree *tree = *treep;

	unsigned ret = tree->next_free;
	struct radix_node *node = getnode(tree, tree->next_free);
	assert(node);
	tree->next_free = node->next_free;
	if (node->next_free && !node->next_free_is_initialized) {
		struct radix_node *next = getnode(tree, node->next_free);
		assert(next);
		next->next_free = (node->next_free + 1 < tree->num_nodes) ? node->next_free + 1 : 0;
	}
	node->as_u64 = 0;
	return ret;
}

static void
radix_tree_free_node(struct radix_tree *tree, unsigned index)
{
	struct radix_node *node = getnode(tree, index);
	assert(node);
	node->next_free = tree->next_free;
	node->next_free_is_initialized = true;
	tree->next_free = index;
}

static bool
radix_tree_insert_recursive(struct radix_tree **treep, struct interval keys, uint64_t value, unsigned node_index, int keyshift)
{
	struct radix_node *node = getnode(*treep, node_index);

	DINDENT(keyshift);
	D("INSERTREC %p keys=[%llx-%llx]\n", node, keys.start, keys.start + keys.size);

	assert(keyshift < RADIX_TREE_KEY_BITS);
	assert(node);

	for (int i = 0; i < 2; i++) {
		struct radix_edge *edge = &node->edges[i];
		int matching_bits = count_matching_bits(edge, keys.start, keyshift);
		if (matching_bits) {
			if (matching_bits == edge->labelBits) {
				if (edge->isLeaf) {
					assert(false); // it should have been deleted before we got here
					struct radix_node *leaf = getnode(*treep, edge->index);
					assert(leaf);
					assert(keyshift + edge->labelBits == RADIX_TREE_KEY_BITS);
					leaf->stackid = value;
					set_leaf_size(*treep, leaf, keys.size);
					DINDENT(keyshift);
					D("inserted %p\n", node);
					return true;
				} else {
					return radix_tree_insert_recursive(treep, keys, value, edge->index, keyshift + edge->labelBits);
				}
			} else {
				unsigned index = radix_tree_allocate_node(treep);
				if (!index) {
					DINDENT(keyshift);
					D("FAILED! %p\n", node);
					return false;
				}
				/* pointers may have changed */
				node = getnode(*treep, node_index);
				edge = &node->edges[i];

				struct radix_node *newnode = getnode(*treep, index);
				DINDENT(keyshift);
				D("splitting edge newnode=%p isleaf=%s matching_bits=%d oldLabelBits=%d\n", newnode,
						edge->isLeaf ? "true" : "false", matching_bits, edge->labelBits);
				newnode->edges[0].labelBits = (edge->labelBits - matching_bits);
				newnode->edges[0].isLeaf = edge->isLeaf;
				newnode->edges[0].index = edge->index;
				newnode->edges[0].label = edge->label & ((1 << (edge->labelBits - matching_bits)) - 1);

				edge->label = edge->label >> (edge->labelBits - matching_bits);
				edge->labelBits = matching_bits;
				edge->isLeaf = false;
				edge->index = index;

				fixnode(node);
				return radix_tree_insert_recursive(treep, keys, value, index, keyshift + matching_bits);
			}
		}
		if (edge->labelBits == 0) {
			if (RADIX_TREE_KEY_BITS - keyshift <= RADIX_LABEL_BITS) {
				unsigned index = radix_tree_allocate_node(treep);
				if (!index) {
					DINDENT(keyshift);
					D("FAILED! %p\n", node);
					return false;
				}
				/* pointers may have changed */
				node = getnode(*treep, node_index);
				edge = &node->edges[i];

				edge->labelBits = RADIX_TREE_KEY_BITS - keyshift;
				edge->isLeaf = true;
				edge->index = index;
				edge->label = keybits(keys.start, RADIX_TREE_KEY_BITS - keyshift, keyshift);
				struct radix_node *leaf = getnode(*treep, index);
				DINDENT(keyshift);
				D("new leaf node %p\n", leaf);
				leaf->stackid = value;
				set_leaf_size(*treep, leaf, keys.size);
				fixnode(node);
				return true;
			} else {
				unsigned index = radix_tree_allocate_node(treep);
				if (!index) {
					DINDENT(keyshift);
					D("FAILED! %p\n", node);
					return false;
				}
				/* pointers may have changed */
				node = getnode(*treep, node_index);
				edge = &node->edges[i];

				edge->labelBits = RADIX_LABEL_BITS;
				edge->isLeaf = false;
				edge->index = index;
				edge->label = keybits(keys.start, RADIX_LABEL_BITS, keyshift);

				struct radix_node *newnode = getnode(*treep, index);
				newnode->as_u64 = 0;
				DINDENT(keyshift);
				D("new internal node %p\n", newnode);

				fixnode(node);
				return radix_tree_insert_recursive(treep, keys, value, index, keyshift + RADIX_LABEL_BITS);
			}
		}
	}

	radix_tree_panic("MallocStackLogging INTERNAL ERROR: at least one edge must prefix-match or be unused");
}

bool
radix_tree_insert(struct radix_tree **treep, uint64_t key, uint64_t size, uint64_t value)
{
	D("INSERT %llx-%llx\n", key, key + size);
	DINC(4);
	if (key + size < key) {
		radix_tree_panic("MallocStackLogging INTERNAL ERROR: interval wraps around the end of the address space: %llx, size=%llx\n",
				key, size);
	}
	struct radix_node node = {.stackid = value, .size = size >> (*treep)->leaf_size_shift};
	if (node.stackid != value || (((uint64_t)node.size) << (*treep)->leaf_size_shift) != size) {
		radix_tree_panic("MallocStackLogging INTERNAL ERROR: cannot represent value:%llx or size:%llx (key is %llx)\n", value, size, key);
		return false;
	}
	uint64_t mask = ((uint64_t)-1) << (64 - RADIX_TREE_KEY_BITS);
	if ((key & mask) != key) {
		radix_tree_panic("MallocStackLogging INTERNAL ERROR: cannot represent key: %llx\n", key);
	}
	bool ok;
	ok = radix_tree_delete(treep, key, size);
	if (!ok) {
		goto out;
	}
	ok = radix_tree_insert_recursive(treep, (struct interval){.start = key, .size = size}, value, 0, 0);
out:
	DDEC(4);
	return ok;
}

static bool
radix_tree_delete_recursive(struct radix_tree *tree, uint64_t key, struct radix_node *node, int keyshift)
{
	assert(keyshift < RADIX_TREE_KEY_BITS);
	assert(node);

	for (int i = 0; i < 2; i++) {
		struct radix_edge *edge = &node->edges[i];
		if (edge_matches(edge, key, keyshift)) {
			if (edge->isLeaf) {
				radix_tree_free_node(tree, edge->index);
				if (i == 0) {
					node->edges[0] = node->edges[1];
					node->edges[1].labelBits = 0;
				} else {
					node->edges[1].labelBits = 0;
				}
				return true;
			} else {
				bool deleted = radix_tree_delete_recursive(tree, key, getnode(tree, edge->index), keyshift + edge->labelBits);
				if (deleted) {
					struct radix_node *child = getnode(tree, edge->index);
					assert(child);
					if (child->edges[0].labelBits == 0 && child->edges[1].labelBits == 0) {
						radix_tree_free_node(tree, edge->index);
						if (i == 0) {
							node->edges[0] = node->edges[1];
							node->edges[1].labelBits = 0;
						} else {
							node->edges[1].labelBits = 0;
						}
					}
				}
				return deleted;
			}
		}
	}
	return false;
}

bool
radix_tree_delete(struct radix_tree **treep, uint64_t key, uint64_t size)
{
	D("BALETE %llx-%llx\n", key, key + size);
	DINC(4);
	struct interval keys = {.start = key, .size = size};
	bool ok = true;
	while (1) {
		struct answer answer = radix_tree_lookup_interval(*treep, keys);
		if (!answer_found(answer)) {
			break;
		}
		ok = radix_tree_delete_recursive(*treep, answer.interval.start, getnode(*treep, 0), 0);
		assert(ok);
		D("BALETED %llx-%llx -> %llx\n", answer.interval.start, answer.interval.start + answer.interval.size, answer.stackid);
		if (answer.interval.start < keys.start) {
			D("REINSERTING %llx-%llx -> %llx\n", answer.interval.start,
					answer.interval.start + (keys.start - answer.interval.start), answer.stackid);
			ok = radix_tree_insert(treep, answer.interval.start, keys.start - answer.interval.start, answer.stackid);
			if (!ok) {
				goto out;
			}
		}
		uint64_t answer_end = answer.interval.start + answer.interval.size;
		uint64_t keys_end = keys.start + keys.size;
		if (answer_end > keys_end) {
			D("REINSERTING %llx-%llx -> %llx\n", keys_end, keys_end + (answer_end - keys_end), answer.stackid);
			ok = radix_tree_insert(treep, keys_end, answer_end - keys_end, answer.stackid);
			if (!ok) {
				goto out;
			}
		}
	}
out:
	DDEC(4);
	return ok;
}

struct radix_tree *
radix_tree_init(void *buf, size_t size)
{
	struct radix_tree *tree = buf;
	memcpy(tree->header, "radixv2", 8);
	void *nodestart = &tree->nodes[0];
	void *nodesend = buf + size;
	assert(nodestart < nodesend);
	tree->num_nodes = (uint32_t)(nodesend - nodestart) / sizeof(struct radix_node);
	assert(tree->num_nodes >= 3);
	tree->next_free = 1;
	tree->nodes[0].as_u64 = tree->nodes[1].as_u64 = 0;
	tree->nodes[1].next_free = 2;
	tree->leaf_size_shift = 12; // smallest size of a VM region is 4096
	return tree;
}

struct radix_tree *
radix_tree_create()
{
	mach_vm_size_t size = PAGE_SIZE;
	mach_vm_address_t allocated;
	kern_return_t kr =
			mach_vm_allocate(mach_task_self(), &allocated, size, VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_ANALYSIS_TOOL));
	if (kr != KERN_SUCCESS) {
		return NULL;
	}
	return radix_tree_init((void *)allocated, PAGE_SIZE);
	return NULL;
}

static void
radix_tree_grow(struct radix_tree **treep)
{
	mach_vm_size_t max_size = (1 << 16) * sizeof(struct radix_node);

	assert((*treep)->next_free == 0);
	mach_vm_size_t size = sizeof(struct radix_tree) + sizeof(struct radix_node) * (*treep)->num_nodes;
	assert(size % PAGE_SIZE == 0);
	mach_vm_size_t newsize = size * 2;
	if (newsize > max_size) {
		newsize = max_size;
	}
	if (newsize <= size) {
		return;
	}
	mach_vm_address_t allocated;
	kern_return_t kr =
			mach_vm_allocate(mach_task_self(), &allocated, newsize, VM_FLAGS_ANYWHERE | VM_MAKE_TAG(VM_MEMORY_ANALYSIS_TOOL));
	if (kr != KERN_SUCCESS) {
		return;
	}
	D("GROW %p -> %p\n", *treep, (void *)allocated);
	kr = mach_vm_copy(mach_task_self(), (mach_vm_address_t)*treep, size, allocated);
	if (kr != KERN_SUCCESS) {
		mach_vm_deallocate(mach_task_self(), allocated, newsize);
		return;
	}
	uint32_t old_num_nodes = (*treep)->num_nodes;
	mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)*treep, size);
	*treep = (void *)allocated;

	void *nodestart = &(*treep)->nodes[0];
	void *nodesend = ((void *)(*treep)) + newsize;
	(*treep)->num_nodes = (uint32_t)(nodesend - nodestart) / sizeof(struct radix_node);
	(*treep)->next_free = old_num_nodes;

	(*treep)->nodes[old_num_nodes].next_free_is_initialized = 0;
	(*treep)->nodes[old_num_nodes].next_free = old_num_nodes + 1;
}

void
radix_tree_destory(struct radix_tree *tree)
{
	mach_vm_size_t size = sizeof(struct radix_tree) + sizeof(struct radix_node) * tree->num_nodes;
	assert(size % PAGE_SIZE == 0);
	mach_vm_deallocate(mach_task_self(), (mach_vm_address_t)tree, size);
}

static uint64_t
radix_tree_count_recursive(struct radix_tree *tree, struct radix_node *node)
{
	uint64_t count = 0;
	for (int i = 0; i < 2; i++) {
		struct radix_edge *edge = &node->edges[i];
		if (edge->labelBits == 0)
			continue;
		if (edge->isLeaf) {
			count += leaf_size(tree, getnode(tree, edge->index));
		} else {
			count += radix_tree_count_recursive(tree, getnode(tree, edge->index));
		}
	}
	return count;
}

uint64_t
radix_tree_count(struct radix_tree *tree)
{
	return radix_tree_count_recursive(tree, getnode(tree, 0));
}

uint64_t
radix_tree_size(struct radix_tree *tree)
{
	mach_vm_size_t size = sizeof(struct radix_tree) + sizeof(struct radix_node) * tree->num_nodes;
	return size;
}
