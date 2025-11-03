# xzone malloc

xzone malloc is a memory allocator for Apple OS platforms designed to mitigate
heap memory safety vulnerabilities to the maximum extent possible while also
achieving excellent performance.  It is a part of Apple's [Memory Integrity
Enforcement][mie] technology.

## Security features

Key security features of xzone malloc include:

- Bucketed type isolation
- Zero-on-free
- Externalized metadata
- Probabilistic guard pages
- Allocation fronts
- MTE support

### Type isolation

One of the most important security features of xzone malloc is _bucketed type
isolation_.  On Apple platforms, clients of the system memory allocator pass
information about the associated type for each allocation in addition to size.
xzone malloc uses this information to partition the type space into buckets and
serve allocations for each bucket from mutually isolated areas of the virtual
address space.  This makes it impossible to _reliably_ cause allocations that
may fall into different buckets to ever share the same virtual address, which
disrupts many use-after-free type confusion exploitation techniques that depend
on this.

The benefit of using the bucketed approach for this mitigation, rather than
trying to isolate at the level of individual types, is that the fragmentation
impact is significantly lower.  There's a security-performance trade-off in the
selection of the bucket count, with more buckets being better for security but
coming at the cost of increased fragmentation, so deployments of xzone malloc in
different contexts are able to choose the best practical option for that
context's security significance and memory constraints.

Type information used to perform the bucketing of allocations is passed to
libmalloc in the form of "type descriptors", which are described in
`<malloc/malloc.h>` and come from a number of possible sources:

- For manual C/C++ calls to `malloc()`/etc, the [Typed Memory Operations][tmo]
  [compiler feature][tmo-rfc], which is enabled by default for software in Apple
  operating systems, infers the allocation type from the passed size expression
  and rewrites the call to the corresponding `malloc_type_` interface
- For C++ `operator new`, Typed Memory Operations uses the type information
  available via the language and rewrites the call to the typed `operator new`
  entrypoint
- Allocations for objects in the Objective-C and Swift languages have suitable
  type descriptors synthesized by the runtime
- When no type information is supplied directly to the allocator, the caller
  program counter value is used as a fallback proxy for type

xzone malloc's partitioning policy varies according to OS platform, process type
and other factors, but normally involves:

- A special bucket for "pure data" allocations, i.e. allocations that should not
  contain pointers
- A special bucket for Objective-C object allocations
- N general or "pointer" buckets, which contain allocations not falling into any
  of the special categories

The number of general buckets ranges from 1 to 4 depending on configuration.
Allocations are assigned randomly to buckets by type using a source of entropy
that is stable across executions of a given binary within the same boot, to
prevent an attacker to achieve a desired bucketing by repeatedly crashing their
process of interest.

Allocations for a given (size, type space partition) combination are served from
virtual addresses that are isolated from any others for the life of the program,
so that it's not possible to reliably cause allocations that may fall into
different buckets to ever share the same virtual address.

#### Early allocator

To mitigate fragmentation caused by type isolation in cases where a particular
size-and-type bucket is only lightly utilized, xzone malloc has special
functionality for serving "early" allocations.  An allocation from a particular
bucket is considered early if it's one of the first N allocations of that type
in the lifetime of the address space.  These allocations generally aren't easily
controlled by attackers, so by policy they are allowed to be allocated from a
separate, simpler allocator that doesn't enforce the type isolation properties
and that is optimized to minimize fragmentation.

### Zero-on-free

To reduce the risk of an information leak due to missing initialization of
memory, allocations below a size threshold (currently 1KB) are zeroed on free.

### Externalized metadata

Almost all of xzone malloc's metadata is stored in portions of the address space
that are located separately and at unpredictable offsets from the allocations
themselves.  This prevents an attacker from turning a general heap memory-safety
bug into corruption of the allocator metadata.

The only case where xzone malloc uses inline metadata is for free-list linkages,
and it takes special steps to defend these from manipulation: when [ARM pointer
authentication][pac] is available, each free-list linkage is PAC-signed, with
the signature incorporating a sequence number to prevent straightforward replay
attacks.

### Probabilistic guard pages

When resources permit, xzone malloc probabilistically places inaccessible guard
pages between ranges of the heap being used to serve allocations.  This is
intended to frustrate grooming of the heap and exploitation of out-of-bounds
bugs by making it difficult to predict whether any given page will be mapped or
used for any particular bucket, even given knowledge about allocations in other
pages.

### Allocation fronts

To prevent allocations falling into different buckets from being reliably
interleaved, xzone malloc partitions the set of buckets into two groups that are
each assigned a direction in which to grow within the virtual address space
(i.e. a "front").  This ensures that it's not possible to induce a reliable
spatial A-before-B placement relationship between any two types A and B that may
fall into different buckets.

### MTE support

xzone malloc supports [ARM MTE][mte].  When configured to do so on supported
hardware, xzone malloc assigns MTE tags to most blocks that are <= 32KB in size.
This strongly mitigates exploitation of many use-after-free and overflow bugs.

For blocks up to 4KB in size, tags for each block are reassigned on free.  The
tag-on-free policy is excellent for security and bug-finding, catching
use-after-free immediately after deallocation with high probability.

For larger blocks up to 32KB in size, tags are reassigned on allocation.  The
tag-on-alloc policy is better for performance at larger sizes, although it
comes at the cost of allowing use-after-free accesses to free blocks before
they're next reallocated.  That allows an attacker to use use-after-free
accesses to a block for scratch space, but they are still prevented from
exploiting use-after-free type confusion since that requires the block to be
reused.

When assigning tags, the tags of the previous incarnation of the block as well
as its neighbors are excluded (except, as a current implementation detail, at
page boundaries).

Under the default policy, allocations that are "pure data", i.e. allocations
whose type information indicates that they contain no pointers, are not tagged.
xzone malloc can be configured to also tag them via entitlement.

## Configuration

xzone malloc's configuration can be modified via mechanisms including
entitlements and environment variables in cases where a different security or
performance profile than the platform default is needed.

### Hardened heap

The "hardened heap" configuration of xzone malloc enables some security features
and behavior that are too costly to be part of the platform defaults, but
provide valuable additional protection in especially security-sensitive
processes.  It is engaged:

- By the `com.apple.security.hardened-process.hardened-heap`
  [entitlement][hardened-heap], which is part of the set included by the
  [Enhanced Security][enhanced-security] capability in Xcode
- By the entitlements in the `com.apple.developer.web-browser-engine` family
- And by default for certain Apple operating system processes

On all platforms, the probabilistic guard pages feature is enabled by the
hardened heap configuration.

On iOS and watchOS, the hardened heap configuration increases the number of
general type isolation buckets.

### MTE

xzone malloc's support for MTE is engaged in processes that have MTE enabled.
MTE is enabled for a process via the
`com.apple.security.hardened-process.checked-allocations`
[entitlement][checked-allocations].

Tagging of pure-data allocations up to the tagging size threshold is enabled via
the `com.apple.security.hardened-process.checked-allocations.enable-pure-data`
[entitlement][enable-pure-data].

## Design overview

An individual allocation served by xzone malloc is a **block**.

The finest granularity of virtual memory at which xzone malloc normally manages
metadata is a **slice**.  This is typically one operating system virtual page,
which is 16KB on most Apple platforms.

A **span** is a contiguous range of one or more slices.  A span that is
currently being used to serve one or more blocks is a **chunk**.  A span that
isn't currently in use is a **free span**.

A reservation of virtual address space from which chunks are allocated is a
**segment**.  The standard segment size in xzone malloc is 4MB.  Each segment
has its own **segment metadata array**, which is an array with an entry for each
slice in the segment containing the metadata (e.g. free-list head or bitmap) for
that slice.

xzone malloc uses different strategies to serve allocations depending on their
size and its configuration.

For allocations that are around the standard segment size, xzone malloc
allocates a private segment for each allocation, so there is a 1:1:1
relationship between block : chunk : segment.  This is the `HUGE` allocation
strategy.

Allocations that are greater than the slice size are served from standard-sized
segments that are subdivided into smaller chunks.  In this case, the block :
chunk relationship is 1:1, and the chunk : segment relationship is many-to-1.
This is the `LARGE` allocation strategy.

The _segment layer_ of the allocator is responsible for keeping track of the set
of segments in use and which spans within each are free or in use.

The **segment tables** are a simple sparse directly-indexed data structure that
map each segment-sized granule of the virtual address space to the location of
the segment metadata for that segment if one is present.  They allow for fast
lookup for the metadata associated with any address in the virtual address
space, and the indirection they provide enables segment metadata to be kept
separate from the segment bodies.  The set of all segments can be enumerated by
walking the tables.

The **segment group** is the entity that tracks the set of free spans across the
set of segments that can be used for a particular purpose, and it does so with
an array of size-segregated **span queues**.  The span queues are maintained by
a straightforward split-and-coalesce allocator approach:

- Allocation from a segment group takes the smallest available span that will
  serve the request, splitting off any unneeded remainder size and re-enqueuing
  it on the appropriate smaller span-queue
- Deallocation to a segment group checks the state of the adjacent spans,
  coalescing with either if they're free and enqueuing the resulting coalesced
  span to the span queue of the corresponding size

For sizes smaller than the slice size, xzone malloc uses a slab allocator
design organized around an abstraction called an **xzone**:

- An xzone is a slab allocator that serves allocations of a single size
- An xzone serves allocations by splitting chunks (i.e. slabs) into
  equally-sized blocks of the xzone's block size
- Each chunk maintains metadata about which blocks within the chunk are
  allocated and free
- xzones keep track of the set of chunks that belong to them, including
  "current" chunks being used to serve new allocations, partially-full chunks
  that can be used to serve additional allocations, and empty chunks, which hold
  no allocations but must be kept isolated to the xzone in order to maintain
  type isolation
- When an xzone has no existing chunks that can be used to serve a new
  allocation request, it allocates a new chunk from the segment group it is
  configured with

The name "xzone" is short for "xnu-style zone", since they are intended to be
similar in concept to the zones in xnu's zone allocator.  The "x" qualifier is
necessary because "zone" already has a distinct meaning in Darwin's userspace
libmalloc, referring to the allocator instances that are interacted with via the
`malloc_zone*` interfaces.

The sizes served by xzones are quantized into discrete size classes referred to
as **bins**.  Each bin is served by a set of xzones according to the type
isolation policy.  On allocation, xzone malloc computes the bin for the
allocation from the requested size, and then computes which of the bin's xzones
to use based on the supplied type information.

xzones use either a **free-list** or **bitmap** approach to track the allocated
and free blocks within each of their chunks.  There are a few different types of
xzones:

- The `TINY` xzones are used to serve allocations that are <= 4KB in size, from
  single-slice chunks.  They maintain per-chunk free-lists that are manipulated
  atomically.
- The `SMALL` xzones are normally used to serve allocations where `4KB < size <=
  32KB`, from 4-slice chunks.  They maintain per-chunk bitmaps of free blocks
  and use chunk-level locking for synchronization.
- In higher-performance configurations, `SMALL_FREELIST` xzones are used to
  serve `4KB < size <= 32KB` allocations from 8-slice chunks, using the same
  atomic free-list approach as `TINY`.

## Performance features

### Deferred reclaim

One of the traditional trade-offs balanced by a memory allocator is between the
memory cost of holding on to pages that have become empty but might be used
again in the future and the CPU cost of decommitting those pages so that they
can be reused by the rest of the operating system.  Allocators prioritizing
speed tend to hold on to such pages, while allocators prioritizing memory
efficiency tend to give them back.

Darwin has introduced a new primitive for decommitting pages that aims to reduce
the need for this trade-off by reducing the cost of the operation, called
**deferred reclaim**.  Rather than a synchronous syscall like the previous
`madvise(MADV_FREE_REUSABLE)` primitive, deferred reclaim uses a shared memory
ringbuffer between the kernel and the userspace process into which userspace can
enqueue entries describing ranges of virtual address space to be decommitted.
The kernel monitors process-local and system-wide memory conditions to determine
whether and when to drain from this ringbuffer and actually decommit pages in
it.  When userspace wants to re-commit a range that was previous enqueued, it
can try to remove the entry that it placed in the ring, and if the kernel hasn't
reclaimed it yet, no syscalls were required on either the decommit or recommit
side of the operation.

xzone malloc uses this primitive to tell the kernel as eagerly as possible about
all of the pages available to be reclaimed while avoiding the expensive syscalls
traditionally required to do so, improving both CPU performance and memory
efficiency.

### Contention detection and thread caching

Memory allocators often make use of per-thread or per-CPU caches of blocks to
enable their fast paths to be simple and scale well for multi-core workloads.
However, this technique also generally results in increased fragmentation, as
opportunities for block reuse are limited by the thread/CPU separation.

xzone malloc implements two performance features to balance between these:

- **Contention detection** is applied for all xzones:
    - Each xzone starts out using a single current chunk
    - Contention on metadata updates is monitored (via metadata atomic
      compare-exchange failures), and if a threshold level of contention is
      reached, the xzone upgrades to using per-CPU (or per asymmetric
      multiprocessing cluster, on Apple Silicon hardware) multiple current
      chunks
- **Thread caching** is applied for xzones serving very small block sizes:
    - Each xzone starts out with no thread-level cache
    - Allocation volume and metadata contention are monitored for each xzone on
      each thread, and if a threshold level of either is reached, a thread-level
      cache is brought up

Both of these features default to a memory-efficient starting configuration, and
transition to a higher-performance configuration in response to observed runtime
conditions.

## Ancestry

xzone malloc is partly derived from the [mimalloc][mimalloc] allocator.  At the
beginning of its development, the design and implementation of mimalloc was used
as a starting point that provided solutions to many of the basic/fundamental
problems an allocator needs to solve.  Many of the concepts and terminology in
xzone malloc are inherited from mimalloc:

- mimalloc also reserves virtual memory in **segments** that have an associated
  **segment metadata array**
- mimalloc's finest unit of virtual memory management is also **slices**
- mimalloc's **pages** are xzone malloc's **chunks**
- mimalloc's **bins** for size classes are the same as xzone malloc's
- mimalloc's **heaps** are like **xzones**, managing a set of pages/chunks for
  allocations of a particular size
- mimalloc **tlds** are somewhat like xzone malloc's **segment groups**, in that
  they maintain **span queues** of free spans across segments

From that foundation, xzone malloc diverged by adding and changing aspects of
the design focusing on its specific security and performance goals.

At the time of this writing (09/25), most of xzone malloc's key security
features are not present in mimalloc:

- xzones and segment groups are differentiated from mimalloc's heaps and tlds by
  their support for bucketed type isolation
- xzone malloc uses a segment table rather than mimalloc's segment bitmap to
  allow its metadata to be separated from the contents of the heap
- xzone malloc's allocation fronts and guard pages features introduce further
  obstacles to exploit reliability that there are no direct analogues for in
  mimalloc
- mimalloc does not support ARM MTE

xzone malloc's security features are mostly inspired by those in the xnu
kernel's memory allocator, [kalloc\_type][kalloc_type], which can be considered
its other ancestor.

With respect to performance, the main difference between mimalloc and xzone
malloc is in the trade-offs they make between speed and fragmentation.  Because
the goal of xzone malloc was to be used in all processes on Apple OS platforms,
including small, long-running operating system services and daemons, it makes a
number of choices that prioritize memory efficiency over speed and scalability:

- mimalloc's high-level design of using independent per-thread-everything is
  excellent for speed and scalability, but generally results in higher
  fragmentation than can be achieved by using centralized structures that are
  synchronized with locking or atomics.  This is why xzones and segment groups
  in xzone malloc are global rather than per-thread.
- mimalloc's philsophy of avoiding specialization of allocation strategies for
  different ranges of sizes keeps its design simple and fast, but forgoes some
  significant memory optimization opportunities, like the ability to decommit
  individual pages within multi-page slabs when they aren't currently occupied
  by any blocks, which xzone malloc's `SMALL` allocator implements.

## Name

The name of the allocator is "xzone malloc".  Referring to it just as "xzone" is
incorrect.

"xzone" is short for "xnu-style zone", in reference to the zone abstraction in
xnu's zalloc allocator that they resemble.  Plain "zone" already had the
separate meaning of "allocator instance" in userspace libmalloc, necessitating
the "x" prefix.

[mie]: https://security.apple.com/blog/memory-integrity-enforcement/

[tmo]: https://developer.apple.com/documentation/xcode/adopting-type-aware-memory-allocation

[tmo-rfc]: https://discourse.llvm.org/t/rfc-typed-allocator-support/79720

[pac]: https://developer.arm.com/documentation/109576/0100/Pointer-Authentication-Code/Introduction-to-PAC

[mte]: https://developer.arm.com/documentation/108035/0100/Introduction-to-the-Memory-Tagging-Extension

[hardened-heap]: https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.hardened-process.hardened-heap

[enhanced-security]: https://developer.apple.com/documentation/Xcode/enabling-enhanced-security-for-your-app

[checked-allocations]: https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.hardened-process.checked-allocations

[enable-pure-data]: https://developer.apple.com/documentation/bundleresources/entitlements/com.apple.security.hardened-process.checked-allocations.enable-pure-data

[mimalloc]: https://github.com/microsoft/mimalloc

[kalloc_type]: https://security.apple.com/blog/towards-the-next-generation-of-xnu-memory-safety/
