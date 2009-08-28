/*
 * Copyright (c) 2009 Apple Inc. All rights reserved.
 *
 * @APPLE_APACHE_LICENSE_HEADER_START@
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * @APPLE_APACHE_LICENSE_HEADER_END@
 */
/*
    AutoRootScanner.h
    Copyright (c) 2004-2008 Apple Inc. All rights reserved.
 */

#include "AutoMemoryScanner.h"
#include "AutoZone.h"

namespace Auto {
    typedef std::vector<Range, AuxAllocator<Range> > RangeVector;

    struct ReferenceNode : Range {
        RangeVector _incoming;
        RangeVector _outgoing;
        
        enum Kind { HEAP, ROOT, STACK, KEY };
        Kind _kind : 2;
        
        // used by shortest path algorithm, ReferenceGraph::findPath() below.
        bool _visited : 1;
        ReferenceNode* _parent;
        ReferenceNode* _next;

        // used if reference is a KEY reference.
        void *_key;
        
        ReferenceNode() : _kind(HEAP), _visited(false), _parent(NULL), _next(NULL) {}
        
        void pointsFrom(void *address, usword_t offset) {
            _incoming.push_back(Range(address, offset));
        }
        
        void pointsTo(void *address, usword_t offset) {
            _outgoing.push_back(Range(address, offset));
        }
        
        usword_t offsetOf(ReferenceNode *node) {
            if (node != NULL) {
                usword_t count = _outgoing.size();
                for (usword_t i = 0; i < count; ++i) {
                    if (node->address() == _outgoing[i].address())
                        return _outgoing[i].size();
                }
            }
            return 0;
        }
    };
    
    struct ReferenceNodeQueue {
        ReferenceNode *_head;
        ReferenceNode *_tail;
        
        ReferenceNodeQueue() : _head(NULL), _tail(NULL) {}
        
        void enqueue(ReferenceNode *node) {
            node->_next = NULL;
            if (_tail == NULL)
                _head = _tail = node;
            else {
                _tail->_next = node;
                _tail = node;
            }
        }
        
        ReferenceNode *deque() {
            ReferenceNode *node = _head;
            if (_head != NULL) {
                _head = _head->_next;
                if (_head == NULL)
                    _tail = NULL;
            }
            return node;
        }
        
        bool empty() {
            return _head == NULL;
        }
    };

    typedef __gnu_cxx::hash_map<void *, ReferenceNode, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > PtrReferenceNodeHashMap;
    typedef std::vector<ReferenceNode *, AuxAllocator<ReferenceNode *> > ReferenceNodeVector;
    
    struct ReferenceGraph {
        PtrReferenceNodeHashMap _nodes;
        
        ReferenceGraph() : _nodes() {}
        
        bool contains(void *block) {
            return _nodes.find(block) != _nodes.end();
        }
        
        ReferenceNode *addNode(const Range& range) {
            ReferenceNode &node = _nodes[range.address()];
            if (node.address() == 0) {
                node.range() = range;
            }
            return &node;
        }
        
        ReferenceNode* addNode(void *block, usword_t size) {
            return addNode(Range(block, size));
        }
        
        void removeNode(ReferenceNode *node) {
            _nodes.erase(node->address());
        }
        
        ReferenceNode *find(void *block) {
            PtrReferenceNodeHashMap::iterator i = _nodes.find(block);
            if (i != _nodes.end()) {
                return &i->second;
            }
            return NULL;
        }
        
        // performs a bread-first-search traversal starting at the from node, until the to node is reached, and returns the path.
        bool findPath(void *from, void *to, ReferenceNodeVector& path) {
            ReferenceNodeQueue queue;
            ReferenceNode *node = find(from);
            node->_visited = true;
            queue.enqueue(node);
            while (!queue.empty()) {
                node = queue.deque();
                usword_t count = node->_outgoing.size();
                for (usword_t i = 0; i < count; ++i) {
                    ReferenceNode *child = find(node->_outgoing[i].address());
                    if (!child->_visited) {
                        child->_visited = true;
                        child->_parent = node;
                        if (child->address() == to) {
                            while (child != NULL) {
                                path.push_back(child);
                                child = child->_parent;
                            }
                            return true;
                        }
                        queue.enqueue(child);
                    }
                }
            }
            return false;
        }
        
        void resetNodes() {
            for (PtrReferenceNodeHashMap::iterator i = _nodes.begin(), end = _nodes.end(); i != end; ++i) {
                ReferenceNode& node = i->second;
                node._visited = false;
                node._parent = node._next = NULL;
            }
        }
    };

    class RootScanner : public MemoryScanner {
    protected:
        void    *_block;                                    // current block to find
        int     _first_register;                            // current first register or -1 if not registers
        RangeVector _thread_ranges;                         // thread stacks we're scanning.
        ReferenceGraph _graph;                              // graph we are building.
        PtrVector _block_stack;                             // blocks whose successors we still need to find.
    
    public:
        RootScanner(Zone *zone, void *block, void* stack_bottom)
            : MemoryScanner(zone, stack_bottom, true),
              _block(block), _first_register(-1), _thread_ranges(), _graph(), _block_stack()
        {
            _graph.addNode(block, zone->block_size(block));
        }
        
        bool on_thread_stack(void *address, Range &range) {
            for (RangeVector::iterator i = _thread_ranges.begin(), end = _thread_ranges.end(); i != end; ++i) {
                if (i->in_range(address)) {
                    range = *i;
                    return true;
                }
            }
            return false;
        }
        
        virtual void check_block(void **reference, void *block) {
            MemoryScanner::check_block(reference, block);
            
            if (block == _block && reference != NULL) {
                Range thread_range;
                if (!on_thread_stack(reference, thread_range)) {
                    // heap scan in progress.
                    void *owner = _zone->block_start((void*)reference);
                    if (owner) {
                        intptr_t offset = (intptr_t)reference - (intptr_t)owner;
                        if (!_graph.contains(owner)) {
                            ReferenceNode *ownerNode = _graph.addNode(owner, _zone->block_size(owner));
                            ownerNode->pointsTo(block, offset);
                            _block_stack.push_back(owner);
                            ReferenceNode *blockNode = _graph.find(block);
                            blockNode->pointsFrom(owner, offset);
                        }
                    } else if (_zone->is_root(reference)) {
                        if (!_graph.contains(reference)) {
                            ReferenceNode *referenceNode = _graph.addNode(reference, sizeof(void**));
                            referenceNode->_kind = ReferenceNode::ROOT;
                            referenceNode->pointsTo(block, 0);
                            // note the root reference in the graph.
                            ReferenceNode *blockNode = _graph.find(block);
                            blockNode->pointsFrom(reference, 0);
                        }
                    }
                } else if (!_graph.contains(reference)) {
                    // thread scan in progress.
                    ReferenceNode *referenceNode = _graph.addNode(Range(reference, thread_range.end()));
                    referenceNode->_kind = ReferenceNode::STACK;
                    referenceNode->pointsTo(block, 0);
                    ReferenceNode *blockNode = _graph.find(block);
                    blockNode->pointsFrom(referenceNode->address(), 0); // really offset
                }
            }
        }
        
        virtual void associate_block(void **reference, void *key, void *block) {
            if (block == _block && reference != NULL) {
                // block is ASSOCIATED with reference via key.
                if (!_graph.contains(reference)) {
                    ReferenceNode *referenceNode = _graph.addNode(Range(reference, sizeof(void**)));
                    referenceNode->_kind = ReferenceNode::KEY;
                    referenceNode->_key = key;
                    referenceNode->pointsTo(block, 0);
                    _block_stack.push_back(reference);
                    ReferenceNode *blockNode = _graph.find(block);
                    blockNode->pointsFrom(reference, 0);
                }
            }
            MemoryScanner::associate_block(reference, key, block);
        }
        
        void scan_range_from_thread(Range &range, Thread &thread) {
            _thread_ranges.push_back(range);
            MemoryScanner::scan_range_from_thread(range, thread);
        }
        
        
        void scan_range_from_registers(Range &range, Thread &thread, int first_register) {
            _first_register = first_register;
            MemoryScanner::scan_range_from_registers(range, thread, first_register);
            _first_register = -1;
        }
        
        bool has_pending_blocks() {
            if (!_block_stack.empty()) {
                _block = _block_stack.back();
                _block_stack.pop_back();
                return true;
            }
            return false;
        }
    };
}
