/*
 * Copyright (c) 2011 Apple Inc. All rights reserved.
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
    auto_gdb_interface.cpp
    Routines called by gdb to implement its info gc-references and gc-roots commands.
    Copyright (c) 2007-2011 Apple Inc. All rights reserved.
 */

#include <vector>
#include <deque>
#include "auto_gdb_interface.h"
#include "auto_impl_utilities.h"
#include "Zone.h"
#include "BlockIterator.h"
#include "ReferenceIterator.h"

namespace Auto {
    template <typename ReferenceIterator> class GDBPendingStack {
        typedef std::vector<uintptr_t, AuxAllocator<uintptr_t> > uintptr_vector;
        uintptr_vector _small_stack, _large_stack;
    public:
        const PendingStackHint hints() { return PendingStackWantsEagerScanning; }

        void push(Subzone *subzone, usword_t q) {
            assert(q <= 65536);
            assert(uintptr_t(subzone) == (uintptr_t(subzone) & ~0x1FFFF));
            _small_stack.push_back(uintptr_t(subzone) | q); // 1024 * 1024 / 16 == 65536 == 0x10000
        }
        
        void push(Large *large) {
            _large_stack.push_back(uintptr_t(large));
        }

        static bool mark(Subzone *subzone, usword_t q) { return subzone->test_and_set_mark(q); }
        static bool mark(Large *large) { return large->test_and_set_mark(); }
        
        static bool is_marked(Subzone *subzone, usword_t q) { return subzone->is_marked(q); }
        static bool is_marked(Large *large) { return large->is_marked(); }
        
        void scan(ReferenceIterator &scanner) {
            for (;;) {
                // prefer scanning small blocks to large blocks, to keep the stacks shallow.
                if (_small_stack.size()) {
                    uintptr_t back = _small_stack.back();
                    _small_stack.pop_back();
                    Subzone *subzone = reinterpret_cast<Subzone *>(back & ~0x1FFFF);
                    usword_t q = back & 0x1FFFF;
                    scanner.scan(subzone, q);
                } else if (_large_stack.size()) {
                    Large *large = reinterpret_cast<Large*>(_large_stack.back());
                    _large_stack.pop_back();
                    scanner.scan(large);
                } else {
                    return;
                }
            }
        }
        
        template <typename U> struct rebind { typedef GDBPendingStack<U> other; };
    };
    
    template <typename ReferenceIterator> class GDBScanningStrategy : public FullScanningStrategy<ReferenceIterator> {
    public:
        // provide a way to rebind this template type to another just like STL allocators can do.
        template <typename U> struct rebind { typedef GDBScanningStrategy<U> other; };
        
        // Could use this to customize the scanning strategy. For now, it could just as easily be a typedef.
    };
    
    typedef std::vector<auto_memory_reference_t, AuxAllocator<auto_memory_reference_t> > RefVector;

    class GDBReferenceRecorder {
    private:
        Zone *_zone;
        void *_block;
        void *_stack_bottom;
        RefVector _refs;
        
        struct Configuration;
        typedef ReferenceIterator<Configuration> GDBReferenceIterator;
        
        struct Configuration {
            typedef GDBReferenceRecorder ReferenceVisitor;
            typedef GDBPendingStack<GDBReferenceIterator> PendingStack;
            typedef GDBScanningStrategy<GDBReferenceIterator> ScanningStrategy;
        };

    public:
        GDBReferenceRecorder(Zone *zone, void *block, void *stack_bottom) : _zone(zone), _block(block), _stack_bottom(stack_bottom) {}
        
        void visit(const ReferenceInfo &info, void **slot, void *block) {
            if (block == _block) {
                auto_memory_reference_t ref = { NULL };
                switch (info.kind()) {
                case kRootReference:
                    ref.address = slot;
                    ref.offset = 0;
                    ref.kind = auto_memory_block_global;
                    break;
                case kStackReference:
                    ref.address = info.thread().stack_base();
                    ref.offset = (intptr_t)slot - (intptr_t)ref.address;
                    ref.kind = auto_memory_block_stack;
                    break;
                case kConservativeHeapReference:
                case kAllPointersHeapReference:
                    ref.address = _zone->block_start((void*)slot);
                    ref.offset = (intptr_t)slot - (intptr_t)ref.address;
                    ref.kind = auto_memory_block_bytes;
                    ref.retainCount = auto_zone_retain_count((auto_zone_t *)_zone,ref.address);
                    break;
                case kExactHeapReference:
                    ref.address = _zone->block_start((void*)slot);
                    ref.offset = (intptr_t)slot - (intptr_t)ref.address;
                    ref.kind = auto_memory_block_object;
                    ref.retainCount = auto_zone_retain_count((auto_zone_t *)_zone,ref.address);
                    break;
                case kAssociativeReference:
                    ref.address = info.object();
                    ref.offset = (intptr_t)info.key();
                    ref.kind = auto_memory_block_association;
                    ref.retainCount = auto_zone_retain_count((auto_zone_t *)_zone,ref.address);
                    break;
                default:
                    break;
                }
                if (ref.address) _refs.push_back(ref);
            }
        }
        
        void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q) {
            visit(info, slot, subzone->quantum_address(q));
        }
        
        void visit(const ReferenceInfo &info, void **slot, Large *large) {
            visit(info, slot, large->address());
        }
        
        void scan() {
            Configuration::PendingStack stack;
            GDBReferenceIterator scanner(_zone, *this, stack, _stack_bottom);
            scanner.scan();
        }

        auto_memory_reference_list_t *copy_refs() {
            uint32_t count = _refs.size();
            auto_memory_reference_list_t *result = (auto_memory_reference_list_t *) aux_malloc(sizeof(auto_memory_reference_list_t) + count * sizeof(auto_memory_reference_t));
            result->count = count;
            std::copy(_refs.begin(), _refs.end(), result->references);
            return result;
        }
    };
    
    class GDBRootFinder {
    private:
        struct Configuration;
        typedef ReferenceIterator<Configuration> GDBRootIterator;
        struct Configuration {
            typedef GDBRootFinder ReferenceVisitor;
            typedef GDBPendingStack<GDBRootIterator> PendingStack;
            typedef GDBScanningStrategy<GDBRootIterator> ScanningStrategy;
        };
        
        struct Node;
        typedef std::vector<Node *, AuxAllocator<Node *> > NodeVector;
        typedef std::deque<Node *, AuxAllocator<Node *> > NodeQueue;
        typedef __gnu_cxx::hash_map<void *, Node*, AuxPointerHash, AuxPointerEqual, AuxAllocator<void *> > NodeSet;
        typedef __gnu_cxx::hash_map<Node *, auto_memory_reference_t, AuxPointerHash, AuxPointerEqual, AuxAllocator<Node *> > NodeRefMap; // Node * -> auto_memory_reference_t
        
        //
        // Node - Represents each object in the roots sub-graph. Contains a set that represents all of the unique pointers FROM other nodes to
        // this node. Currently, only one reference from a given node is represented; this is sufficient for nodes that represent stack
        // and global references, because there can be only from a given Node. On the other hand, since an object may contain multiple pointers
        // we might want to change the represention to be a set of pair<ref, Node>, where there is a unique entry for every distinct reference
        // to an object. This will provide a more comprehensive set of references, and may be necessary for understanding the complete picture
        // of a root set.
        //
        struct Node {
            void *_address;                         // base address of this node.
            Node *_target;                          // used by shortest path algorithm.
            NodeRefMap _references;                 // references to THIS Node, keyed by slotNode addresses.
            enum Color { White = 0, Gray, Black };  // states a Node can be in during predecessor discovery.
            Color _color : 2;
            char _is_thread_local : 1;              // true if this node is thread local.
            char _is_retained : 1;                  // true if this node has a retain count.

            Node(void *address) : _address(address), _target(NULL), _references(), _color(White), _is_thread_local(0), _is_retained(0) {}
            
            Color darken() { if (_color < Black) _color = (Color)(_color + 1); return _color; }
            
            void addRef(Zone *zone, Node *slotNode, const ReferenceInfo &info, void **slot) {
                if (_references.find(slotNode) == _references.end()) {
                    auto_memory_reference_t ref = { NULL };
                    switch (info.kind()) {
                    case kRootReference:
                        ref.address = slotNode->_address;
                        ref.offset = 0;
                        ref.kind = auto_memory_block_global;
                        break;
                    case kStackReference:
                        ref.address = info.thread().stack_base();
                        ref.offset = (intptr_t)slot - (intptr_t)ref.address;
                        ref.kind = auto_memory_block_stack;
                        break;
                    case kConservativeHeapReference:
                    case kAllPointersHeapReference:
                        ref.address = slotNode->_address;
                        ref.offset = (intptr_t)slot - (intptr_t)ref.address;
                        ref.kind = auto_memory_block_bytes;
                        ref.retainCount = auto_zone_retain_count((auto_zone_t *)zone,ref.address);
                        break;
                    case kExactHeapReference:
                        ref.address = slotNode->_address;
                        ref.offset = (intptr_t)slot - (intptr_t)ref.address;
                        ref.kind = auto_memory_block_object;
                        ref.retainCount = auto_zone_retain_count((auto_zone_t *)zone,ref.address);
                        break;
                    case kAssociativeReference:
                        ref.address = slotNode->_address;
                        ref.offset = (intptr_t)info.key();
                        ref.kind = auto_memory_block_association;
                        ref.retainCount = auto_zone_retain_count((auto_zone_t *)zone,ref.address);
                        break;
                    default:
                        return;
                    }
                    _references[slotNode] = ref;
                }
            }

            typedef void (^ref_visitor_t) (Node *targetNode, Node* slotNode, auto_memory_reference_t &ref);
            
            void visitRefs(ref_visitor_t visitor) {
                NodeQueue queue;
                queue.push_back(this);
                this->_color = Black;
                while (!queue.empty()) {
                    Node *node = queue.front();
                    queue.pop_front();
                    for (NodeRefMap::iterator i = node->_references.begin(), end = node->_references.end(); i != end; ++i) {
                        Node *child = i->first;
                        if (child->_color == White) {
                            child->_color = Black;
                            visitor(node, child, i->second);
                            queue.push_back(child);
                        }
                    }
                }
            }
        };
        
        Zone *_zone;
        Node *_blockNode;
        auto_memory_reference_t _blockRef;
        void *_stack_bottom;
        NodeSet _nodes;
        NodeSet _nodesToExplore;
        
    public:
        GDBRootFinder(Zone *zone, void *block, void* stack_bottom) : _zone(zone), _blockNode(new Node(block)), _stack_bottom(stack_bottom) {
            _nodes[_blockNode->_address] = _blockNode;
            _nodesToExplore[_blockNode->_address] = _blockNode;

            auto_block_info_sieve<AUTO_BLOCK_INFO_REFCOUNT|AUTO_BLOCK_INFO_LAYOUT> block_info(_zone, _blockNode->_address);
            _blockRef = (auto_memory_reference_t) { _blockNode->_address, 0, is_object(block_info.layout()) ? auto_memory_block_object : auto_memory_block_bytes, block_info.refcount() };
        }
        
        ~GDBRootFinder() {
            for (NodeSet::iterator i = _nodes.begin(), end = _nodes.end(); i != end; ++i) {
                delete i->second;
            }
        }
        
        Node *nodeForSlot(const ReferenceInfo &info, void **slot) {
            Node *node = NULL;
            NodeSet::iterator i;
            switch (info.kind()) {
            case kRootReference:
            case kStackReference:
                i = _nodes.find(slot);
                if (i != _nodes.end()) {
                    node = i->second;
                } else {
                    node = new Node(slot);
                    _nodes[slot] = node;
                }
                break;
            case kAssociativeReference:
                i = _nodes.find(slot);
                if (i != _nodes.end()) {
                    node = i->second;
                } else {
                    node = new Node(slot);
                    _nodes[slot] = node;
                    _nodesToExplore[slot] = node;
                }
                break;
            case kConservativeHeapReference:
            case kAllPointersHeapReference:
            case kExactHeapReference:
                {
                    void *start = _zone->block_start(slot);
                    i = _nodes.find(start);
                    if (i != _nodes.end()) {
                        node = i->second;
                    } else {
                        node = new Node(start);
                        _nodes[start] = node;
                        _nodesToExplore[start] = node;
                    }
                }
                break;
            default:
                break;
            }
            return node;
        }
        
        // New algorithm idea:  build a sub-graph starting from the specified object connected to all of its roots. The sub-graph is built effectively in parallel, by considering more
        // than one node at a time. When a new node is added to the tree, the current pass through the heap won't necessarily visit all of the children of the new node, so
        // subsequent passes will be neeeded if new nodes are added. A node is known to be have been fully explored once it has been around through a complete pass, and it can be
        // removed from the set of nodes still being explored. When this set goes empty, the subgraph is complete.
        
        void visit(const ReferenceInfo &info, void **slot, void *block) {
            NodeSet::iterator i = _nodesToExplore.find(block);
            if (i != _nodesToExplore.end()) {
                Node *node = i->second;
                switch (info.kind()) {
                case kThreadLocalReference:
                    node->_is_thread_local = 1;
                    break;
                case kRetainedReference:
                    node->_is_retained = 1;
                    break;
                default:
                    // otherwise, this is a reference that comes from a slot in memory.
                    Node *slotNode = nodeForSlot(info, slot);
                    if (slotNode) node->addRef(_zone, slotNode, info, slot);
                }
            }
        }
        
        void visit(const ReferenceInfo &info, void **slot, Subzone *subzone, usword_t q) {
            visit(info, slot, subzone->quantum_address(q));
        }
        
        void visit(const ReferenceInfo &info, void **slot, Large *large) {
            visit(info, slot, large->address());
        }
        
        bool darkenExploredNodes() {
            __block NodeVector blackNodes;
            std::for_each(_nodesToExplore.begin(), _nodesToExplore.end(), ^(NodeSet::value_type &value) {
                if (value.second->darken() == Node::Black) blackNodes.push_back(value.second);
            });
            std::for_each(blackNodes.begin(), blackNodes.end(), ^(Node *node) { _nodesToExplore.erase(node->_address); });
            return (_nodesToExplore.size() != 0);
        }

        void scan() {
            Configuration::PendingStack stack;
            GDBRootIterator scanner(_zone, *this, stack, _stack_bottom);
            while (darkenExploredNodes()) {
                scanner.scan();
                _zone->reset_all_marks();
            }
            // NodeSet::value_type is a std::pair<void *, Node *>.
            std::for_each(_nodes.begin(), _nodes.end(), ^(NodeSet::value_type &value) { value.second->_color = Node::White; });
        }
        
        typedef std::vector<RefVector, AuxAllocator<RefVector> > PathsVector;
        
        void addPath(PathsVector &paths, Node *rootNode) {
            paths.resize(paths.size() + 1);
            RefVector &path = paths.back();
            Node *slotNode = rootNode;
            while (slotNode != _blockNode) {
                Node *targetNode = slotNode->_target;
                assert(targetNode != NULL);
                auto_memory_reference_t &ref = targetNode->_references[slotNode];
                path.push_back(ref);
                slotNode = targetNode;
            }
            path.push_back(_blockRef);
        }

        auto_root_list_t *copy_roots() {
            auto_root_list_t *result = NULL;
            
            // use Djikstra's algorithm (breadth-first search) to discover the shortest path to each root.
            __block PathsVector paths;
            __block bool considerThreadLocalGarbage = false;
            
            Node::ref_visitor_t visitor = ^(Node *targetNode, Node *slotNode, auto_memory_reference_t &ref) {
                assert(slotNode->_target == NULL);
                slotNode->_target = targetNode;
                switch (ref.kind) {
                case auto_memory_block_global:
                case auto_memory_block_stack:
                    // these are both roots.
                    addPath(paths, slotNode);
                    break;
                case auto_memory_block_bytes:
                case auto_memory_block_object:
                    // retained  and thread local blocks are roots too.
                    if (slotNode->_is_retained) {
                        addPath(paths, slotNode);
                    } else if (considerThreadLocalGarbage && slotNode->_is_thread_local) {
                        addPath(paths, slotNode);
                    }
                    break;
                default:
                    break;
                }
            };
            _blockNode->visitRefs(visitor);
            
            // <rdar://problem/6426033>:  If block is retained, it roots itself.
            if (_blockRef.retainCount) addPath(paths, _blockNode);

            // <rdar://problem/8026966>:  If no roots were found, consider thread local garbage.
            if (paths.empty()) {
                // reset all nodes to run another search, collecting paths from thread-local objects.
                std::for_each(_nodes.begin(), _nodes.end(), ^(NodeSet::value_type &value) {
                    value.second->_color = Node::White;
                    value.second->_target = NULL;
                });
                considerThreadLocalGarbage = true;
                _blockNode->visitRefs(visitor);
            }
            
            size_t count = paths.size();
            size_t list_size = sizeof(auto_root_list_t) + count * sizeof(auto_memory_reference_list_t);
            for (usword_t i = 0; i < count; i++) list_size += paths[i].size() * sizeof(auto_memory_reference_t);
            result = (auto_root_list_t *)aux_malloc(list_size);
            result->count = count;
            auto_memory_reference_list_t *list = result->roots;
            for (usword_t i = 0; i < count; i++) {
                const RefVector &refs = paths[i];
                list->count = refs.size();
                std::copy(refs.begin(), refs.end(), list->references);
                list = (auto_memory_reference_list_t *)displace(list, sizeof(auto_root_list_t) + list->count * sizeof(auto_memory_reference_t));
            }
            return result;
        }
   };
};

using namespace Auto;

// <rdar://problem/6614079> - To avoid deadlocks with malloc stack logging, this class inhibits the logger when called
// from the debugger.

struct MallocLoggerInhibitor {
    malloc_logger_t *_old_logger;
    static void inhibited_logger(uint32_t, uintptr_t, uintptr_t, uintptr_t, uintptr_t, uint32_t) {}
    MallocLoggerInhibitor() : _old_logger(malloc_logger) { if (_old_logger) malloc_logger = &inhibited_logger; }
    ~MallocLoggerInhibitor() { if (_old_logger) malloc_logger = _old_logger; }
};

auto_memory_reference_list_t *auto_gdb_enumerate_references(auto_zone_t *zone, void *address, void *stack_base) {
    auto_memory_reference_list_t *result = NULL;
    Zone *azone = (Zone *)zone;
    if (azone && azone->block_collector()) {
        MallocLoggerInhibitor inhibitor;
        GDBReferenceRecorder recorder(azone, address, stack_base);
        recorder.scan();
        azone->reset_all_marks();
        result = recorder.copy_refs();
        azone->unblock_collector();
    }
    return result;
}

auto_root_list_t *auto_gdb_enumerate_roots(auto_zone_t *zone, void *address, void *stack_base) {
    auto_root_list_t *result = NULL;
    Zone *azone = (Zone *)zone;
    if (azone && azone->block_collector()) {
        MallocLoggerInhibitor inhibitor;
        GDBRootFinder finder(azone, address, stack_base);
        finder.scan();
        result = finder.copy_roots();
        azone->unblock_collector();
    }
    return result;
}

extern "C" bool gdb_is_local(void *address) {
    Zone *azone = (Zone *)auto_zone_from_pointer(address);
    if (azone && azone->in_subzone_memory(address)) {
        Subzone *subzone = Subzone::subzone(address);
        return subzone->is_live_thread_local(subzone->quantum_index(address));
    }
    return false;
}

#if DEBUG

extern "C" void gdb_refs(void *address) {
    auto_zone_t *zone = auto_zone_from_pointer(address);
    if (zone) {
        auto_memory_reference_list_t *refs = auto_gdb_enumerate_references(zone, address, (void *)auto_get_sp());
        if (refs) aux_free(refs);
    }
}

extern "C" void gdb_roots(void *address) {
    auto_zone_t *zone = auto_zone_from_pointer(address);
    if (zone) {
        auto_root_list_t *roots = auto_gdb_enumerate_roots(zone, address, (void *)auto_get_sp());
        if (roots) aux_free(roots);
    }
}

extern "C" bool gdb_is_root(auto_zone_t *zone, void *address) {
    Zone *azone = (Zone *)zone;
    return azone->is_root(address);
}

// Prototype of Template-Based Heap Scanner.
// This is basically sample code showing how the template-based scanner works.

struct RetainedBlocksVisitor {
    Zone *_zone;

    struct Configuration;
    typedef ReferenceIterator<Configuration> Iterator;
    struct Configuration {
        typedef RetainedBlocksVisitor ReferenceVisitor;
        typedef GDBPendingStack<Iterator> PendingStack;
        typedef FullScanningStrategy<Iterator> ScanningStrategy;
    };
    
    RetainedBlocksVisitor(Zone *zone) : _zone(zone) {}

    void visit(const ReferenceInfo &info, void **ref, Subzone *subzone, usword_t q) {
        if (subzone->has_refcount(q)) {
            void *block = subzone->quantum_address(q);
            SubzoneBlockRef blockRef(subzone, q);
            printf("small/medium block %p (sz = %ld, rc = %d)\n", block, subzone->size(q), (int)blockRef.refcount());
        }
    }
    
    void visit(const ReferenceInfo &info, void **ref, Large *large) {
        if (large->refcount()) {
            printf("large block %p (sz = %ld, rc = %lu)\n", large->address(), large->size(), large->refcount());
        } else if (info.kind() == kAssociativeReference) {
            printf("large block %p associatively referenced\n", large->address());
        }
    }
};

extern "C" void gdb_print_retained_blocks(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    if (azone->block_collector()) {
        RetainedBlocksVisitor visitor(azone);
        RetainedBlocksVisitor::Configuration::PendingStack pending_stack;
        RetainedBlocksVisitor::Iterator scanner(azone, visitor, pending_stack, (void *)auto_get_sp());
        scanner.scan();
        azone->reset_all_marks();
        azone->unblock_collector();
    }
}

struct NewBlocksVisitor {
    struct Configuration;
    typedef ReferenceIterator<Configuration> Iterator;
    struct Configuration {
        typedef NewBlocksVisitor ReferenceVisitor;
        typedef GDBPendingStack<Iterator> PendingStack;
        typedef GenerationalScanningStrategy<Iterator> ScanningStrategy;
    };
    size_t _small_count, _large_count;

    NewBlocksVisitor() : _small_count(0), _large_count(0) {}

    void visit(const ReferenceInfo &info, void **ref, Subzone *subzone, usword_t q) {
        if (subzone->is_new(q)) {
            ++_small_count;
            // printf("small/medium block %p (sz = %lu, age = %lu)\n", subzone->quantum_address(q), subzone->size(q), subzone->age(q));
        }
    }
    
    void visit(const ReferenceInfo &info, void **ref, Large *large) {
        if (large->is_new()) {
            ++_large_count;
            // printf("large block %p (sz = %lu, age = %lu)\n", large->address(), large->size(), large->age());
        }
    }
};

extern "C" void gdb_print_new_blocks(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    if (azone->block_collector()) {
        NewBlocksVisitor visitor;
        NewBlocksVisitor::Configuration::PendingStack pending_stack;
        NewBlocksVisitor::Iterator scanner(azone, visitor, pending_stack, (void *)auto_get_sp());
        scanner.scan();
        printf("new blocks:  %lu small/medium, %ld large\n", visitor._small_count, visitor._large_count);
        
        azone->reset_all_marks();
        azone->unblock_collector();
    }
}

extern "C" void gdb_print_large_blocks(auto_zone_t *zone) {
    Zone *azone = (Zone *)zone;
    SpinLock lock(azone->large_lock());
    if (azone->large_list()) {
        printf("global large blocks:\n");
        for (Large *large = azone->large_list(); large != NULL; large = large->next()) {
            printf("large block %p: size = %ld, rc = %lu\n", large->address(), large->size(), large->refcount());
        }
    }
}

#endif
