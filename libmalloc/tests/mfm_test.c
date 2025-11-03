#define MFM_TESTING 1
#include <malloc/malloc.h>
#include <stdlib.h>
#include <darwintest.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED,
		T_META_TAG_NO_ALLOCATOR_OVERRIDE);

#if defined(__LP64__)

#include "../src/internal.h" // MALLOC_TARGET_EXCLAVES

#if !MALLOC_TARGET_EXCLAVES
#include <mach/mach_init.h>
#include <mach/vm_map.h>
#endif // !MALLOC_TARGET_EXCLAVES

static void *
test_mvm_allocate_pages(size_t size, int vm_page_label, plat_map_t *map_out)
{
#if MALLOC_TARGET_EXCLAVES
	return mmap_plat(map_out, 0, size,
			LIBLIBC_MAP_PERM_READ | LIBLIBC_MAP_PERM_WRITE,
			LIBLIBC_MAP_TYPE_PRIVATE, 0, vm_page_label);
#else
	vm_address_t vm_addr;
	kern_return_t kr;

	kr = vm_allocate(mach_task_self(), &vm_addr, size,
			VM_FLAGS_ANYWHERE | VM_MAKE_TAG(vm_page_label));
	return kr == KERN_SUCCESS ? (void *)vm_addr : NULL;
#endif // MALLOC_TARGET_EXCLAVES
}
#define mvm_allocate_pages_plat(size, align, debug_flags, vm_page_label, plat) \
	test_mvm_allocate_pages(size, vm_page_label, plat)

static void *
test_mvm_allocate_plat(uintptr_t addr, size_t size, int flags,
		uint32_t debug_flags, int vm_page_label, plat_map_t *map_out)
{
#if MALLOC_TARGET_EXCLAVES
	const _liblibc_map_type_t type = LIBLIBC_MAP_TYPE_PRIVATE |
			((debug_flags & MALLOC_CAN_FAULT) ? LIBLIBC_MAP_TYPE_FAULTABLE : LIBLIBC_MAP_TYPE_NONE) |
			((debug_flags & MALLOC_NO_POPULATE) ? LIBLIBC_MAP_TYPE_NOCOMMIT : LIBLIBC_MAP_TYPE_NONE) |
			((flags & VM_FLAGS_ANYWHERE) ? 0 : LIBLIBC_MAP_TYPE_FIXED);
	return mmap_plat(map_out, addr, size,
			LIBLIBC_MAP_PERM_READ | LIBLIBC_MAP_PERM_WRITE, type, 0,
			(unsigned)vm_page_label);
#else
	vm_address_t vm_addr;
	kern_return_t kr;

	kr = vm_allocate(mach_task_self(), &vm_addr, size,
			VM_FLAGS_ANYWHERE | VM_MAKE_TAG(vm_page_label));
	return kr == KERN_SUCCESS ? (void *)vm_addr : NULL;
#endif // MALLOC_TARGET_EXCLAVES
}
#define mvm_allocate_plat(addr, size, align, flags, debug_flags, vm_page_label, plat) \
	test_mvm_allocate_plat(addr, size, flags, debug_flags, vm_page_label, plat)

static int
test_mvm_madvise_plat(void *addr, size_t sz, int advice, unsigned debug_flags, plat_map_t *map)
{
	kern_return_t kr;

#if MALLOC_TARGET_EXCLAVES
	kr = !madvise_plat(map, addr, sz, advice) ? KERN_SUCCESS : errno;
#else
	kr = !madvise(addr, sz, advice) ? KERN_SUCCESS : errno;
#endif // MALLOC_TARGET_EXCLAVES

	return !(kr == KERN_SUCCESS);
}
#define mvm_madvise_plat(addr, size, advice, debug_flags, map) \
		test_mvm_madvise_plat(addr, size, advice, debug_flags, map)

static void test_malloc_lock_lock(_malloc_lock_s *lock) {
#if MALLOC_HAS_OS_LOCK
	os_unfair_lock_lock(lock);
#else
	T_QUIET; T_ASSERT_EQ(pthread_mutex_lock(lock), 0, "Lock lock");
#endif // MALLOC_HAS_OS_LOCK
}
#define _malloc_lock_lock(lock) test_malloc_lock_lock(lock);

static void test_malloc_lock_unlock(_malloc_lock_s *lock) {
#if MALLOC_HAS_OS_LOCK
	os_unfair_lock_unlock(lock);
#else
	T_QUIET; T_ASSERT_EQ(pthread_mutex_unlock(lock), 0, "Unlock lock");
#endif // MALLOC_HAS_OS_LOCK
}
#define _malloc_lock_unlock(lock) test_malloc_lock_unlock(lock);

#include "../src/early_malloc.c"

T_DECL(mfm_basic, "mfm basic tests")
{
	void *ptrs[10];
	size_t index, size;

	mfm_initialize();

	T_ASSERT_NOTNULL(mfm_arena, "mfm_initialize worked");

	T_ASSERT_NULL(mfm_alloc(MFM_ALLOC_SIZE_MAX + 1),
	   "allocations larger than %zd bytes should fail",
	   MFM_ALLOC_SIZE_MAX);

	T_ASSERT_EQ(0ul, mfm_arena->mfmh_bump * MFM_QUANTUM,
	    "The bump should be at 0");

	ptrs[0] = mfm_alloc(10);
	T_EXPECT_EQ(16ul, mfm_alloc_size(ptrs[0]),
	    "allocation should be 16 bytes");
	index = __mfm_block_index(mfm_arena, ptrs[0]);
	size  = __mfm_block_size(mfm_arena, index);
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index + size), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_prev_block_is_allocated(mfm_arena, index + size), "allocated");

	ptrs[1] = mfm_alloc(10);
	T_EXPECT_EQ(16ul, mfm_alloc_size(ptrs[1]),
	    "allocation should be 16 bytes");
	index = __mfm_block_index(mfm_arena, ptrs[1]);
	size  = __mfm_block_size(mfm_arena, index);
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index + size), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_prev_block_is_allocated(mfm_arena, index + size), "allocated");

	ptrs[2] = mfm_alloc(128);
	T_EXPECT_EQ(128ul, mfm_alloc_size(ptrs[2]),
	    "allocation should be 128 bytes");
	index = __mfm_block_index(mfm_arena, ptrs[2]);
	size  = __mfm_block_size(mfm_arena, index);
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index + size), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_prev_block_is_allocated(mfm_arena, index + size), "allocated");

	ptrs[3] = mfm_alloc(1024);
	T_EXPECT_EQ(1024ul, mfm_alloc_size(ptrs[3]),
	    "allocation should be 1024 bytes");
	index = __mfm_block_index(mfm_arena, ptrs[3]);
	size  = __mfm_block_size(mfm_arena, index);
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index + size), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_prev_block_is_allocated(mfm_arena, index + size), "allocated");

	T_ASSERT_EQ(1184ul, mfm_arena->mfmh_bump * MFM_QUANTUM,
	    "The bump should be at 1184");

	mfm_free(ptrs[1]);
	mfm_free(ptrs[2]);

	index = __mfm_block_index(mfm_arena, ptrs[1]);
	size  = __mfm_block_size(mfm_arena, index);
	T_EXPECT_EQ(size * MFM_QUANTUM, 144ul, "freed block should be 144 bytes");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index + size), "allocated");

	ptrs[4] = mfm_alloc(64);
	T_EXPECT_EQ(64ul, mfm_alloc_size(ptrs[4]),
	    "allocation should be 64 bytes");
	index = __mfm_block_index(mfm_arena, ptrs[4]);
	size  = __mfm_block_size(mfm_arena, index);
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index + size), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_prev_block_is_allocated(mfm_arena, index + size), "allocated");

	index += size;
	size   = __mfm_block_size(mfm_arena, index);
	T_EXPECT_EQ(size * MFM_QUANTUM, 80ul, "freed block should be 80 bytes");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index + size), "allocated");

	mfm_free(ptrs[3]);

	T_ASSERT_EQ(80ul, mfm_arena->mfmh_bump * MFM_QUANTUM,
	    "The bump should be at 80");

	ptrs[5] = mfm_alloc(64);
	T_EXPECT_EQ(64ul, mfm_alloc_size(ptrs[5]),
	    "allocation should be 64 bytes");
	index = __mfm_block_index(mfm_arena, ptrs[5]);
	size  = __mfm_block_size(mfm_arena, index);
	T_QUIET; T_EXPECT_TRUE(__mfm_block_is_allocated(mfm_arena, index), "allocated");
	T_QUIET; T_EXPECT_TRUE(!__mfm_block_is_allocated(mfm_arena, index + size), "allocated");
	T_QUIET; T_EXPECT_TRUE(__mfm_prev_block_is_allocated(mfm_arena, index + size), "allocated");

	T_ASSERT_EQ(144ul, mfm_arena->mfmh_bump * MFM_QUANTUM,
	    "The bump should be at 144");

	print_mfm_arena(mfm_arena, true, (print_task_printer_t *)printf);
}

T_DECL(mfm_bits, "mfm bitmaps tests")
{
	mfm_initialize();

	T_ASSERT_NOTNULL(mfm_arena, "mfm_initialize worked");

	for (size_t n = 0; n < 2 * 128 * 128; n++) {
		size_t i, j, k, t;
		void *ptrs[3];
		size_t idx[3];

		t = n;
		i = 1 + (t % 128); t /= 128;
		j = 1 + (t % 128); t /= 128;
		k = t;

		ptrs[0] = mfm_alloc(i * MFM_QUANTUM);
		ptrs[1] = mfm_alloc(j * MFM_QUANTUM);
		ptrs[2] = mfm_alloc(MFM_QUANTUM);
		idx[0]  = __mfm_block_index(mfm_arena, ptrs[0]);
		idx[1]  = __mfm_block_index(mfm_arena, ptrs[1]);
		idx[2]  = __mfm_block_index(mfm_arena, ptrs[2]);

		T_QUIET; T_ASSERT_EQ(i, __mfm_block_size(mfm_arena, idx[0]), NULL);
		T_QUIET; T_ASSERT_EQ(i, __mfm_prev_block_size(mfm_arena, idx[1]), NULL);

		T_QUIET; T_ASSERT_EQ(j, __mfm_block_size(mfm_arena, idx[1]), NULL);
		T_QUIET; T_ASSERT_EQ(j, __mfm_prev_block_size(mfm_arena, idx[2]), NULL);

		mfm_free(ptrs[k]);

		T_QUIET; T_ASSERT_EQ(i, __mfm_block_size(mfm_arena, idx[0]), NULL);
		T_QUIET; T_ASSERT_EQ(i, __mfm_prev_block_size(mfm_arena, idx[1]), NULL);

		T_QUIET; T_ASSERT_EQ(j, __mfm_block_size(mfm_arena, idx[1]), NULL);
		T_QUIET; T_ASSERT_EQ(j, __mfm_prev_block_size(mfm_arena, idx[2]), NULL);

		mfm_free(ptrs[1 - k]);

		T_QUIET; T_ASSERT_EQ(i + j, __mfm_block_size(mfm_arena, idx[0]), NULL);
		T_QUIET; T_ASSERT_EQ(i + j, __mfm_prev_block_size(mfm_arena, idx[2]), NULL);

		mfm_free(ptrs[2]);

		T_ASSERT_EQ(mfm_arena->mfmh_bump, 0ul,
		    "alloc(%zd * 16), alloc(%zd * 16), mfm_alloc(16), free(%zd), free(%zd))",
		    i, j, k, 1 - k);
	}
}


struct record {
	ssize_t   size;
	uintptr_t addr;
};

struct alloc_state {
	size_t         count;
	struct record *allocs;
};

static void
alloc_add(struct alloc_state *state, size_t size, uintptr_t addr)
{
	state->allocs[state->count].size = (ssize_t)size;
	state->allocs[state->count].addr = addr;
	state->count++;
}

static void
alloc_rm(struct alloc_state *state, uintptr_t addr)
{
	for (size_t i = 0; i < state->count; i++) {
		if (state->allocs[i].addr != addr) {
			continue;
		}
		if (i + 1 <= state->count) {
			state->allocs[i] = state->allocs[state->count - 1];
			state->allocs[state->count - 1].addr = 0;
			state->allocs[state->count - 1].size = 0;
		}
		state->count--;
		return;
	}

	T_FAIL("Couldn't find address %p", (void *)addr);
}

static size_t
block_size(size_t alloc_size)
{
	return roundup(alloc_size ?: 1, MFM_QUANTUM);
}

static void
run_corruption_test(const struct record *recs, size_t count)
{
	uintptr_t rec_base, mfm_base;
	struct alloc_state state;

	state.count  = 0;
	state.allocs = calloc(count, sizeof(struct record));
	T_ASSERT_NOTNULL(state.allocs, "could allocate state");

	mfm_initialize();
	T_ASSERT_NOTNULL(mfm_arena, "mfm_initialize worked");

	rec_base = recs[0].addr;
	mfm_base = (uintptr_t)mfm_arena->mfm_blocks;

	for (size_t i = 0; i < count; i++) {
		const struct record *rec = &recs[i];

		if (rec->size >= 0) {
			void *ptr = mfm_alloc(rec->size);
			size_t size = mfm_alloc_size(ptr);
			size_t want_size = block_size(rec->size);

#if MFM_TRACE
			T_LOG("[%zd] mfm_alloc(%zd) = %p", i, rec->size, ptr);
			print_mfm_arena(mfm_arena, true, (print_task_printer_t *)printf);
#endif

			T_QUIET; T_ASSERT_EQ(want_size, size, "size for %zd", i);
			T_QUIET; T_ASSERT_EQ((uintptr_t)ptr - mfm_base,
			   rec->addr - rec_base, "ptr for %zd", i);
			alloc_add(&state, size, (uintptr_t)ptr);
		} else {
			void *ptr = (void *)(rec->addr + mfm_base - rec_base);

#if MFM_TRACE
			T_LOG("[%zd] mfm_free(%p, %zd)", i, ptr, mfm_alloc_size(ptr));
#endif

			mfm_free(ptr);

#if MFM_TRACE
			print_mfm_arena(mfm_arena, true, (print_task_printer_t *)printf);
#endif

			alloc_rm(&state, (uintptr_t)ptr);
		}

		for (size_t j = 0; j < state.count; j++) {
			struct record *record = &state.allocs[j];

			T_QUIET; T_EXPECT_EQ(mfm_alloc_size((void *)record->addr),
			    block_size(record->size),
			    "alloc %p is live", (void *)record->addr);


		}
	}

	print_mfm_arena(mfm_arena, true, (print_task_printer_t *)printf);
	T_PASS("Made it to the end");

	free(state.allocs);
}

T_DECL(mfm_corruption, "mfm corruption test")
{
	static const struct record recs[] = {
#include "mfm/trace_1.in"
	};

	run_corruption_test(recs, sizeof(recs) / sizeof(struct record));
}

T_DECL(mfm_corruption2, "mfm corruption test")
{
	static const struct record recs[] = {
#include "mfm/trace_2.in"
	};

	run_corruption_test(recs, sizeof(recs) / sizeof(struct record));
}

T_DECL(mfm_corruption3, "mfm corruption test")
{
	static const struct record recs[] = {
#include "mfm/trace_3.in"
	};

	run_corruption_test(recs, sizeof(recs) / sizeof(struct record));
}

T_DECL(mfm_corruption4, "mfm corruption test")
{
	static const struct record recs[] = {
#include "mfm/trace_4.in"
	};

	run_corruption_test(recs, sizeof(recs) / sizeof(struct record));
}

#else // defined(__LP64__)

T_DECL(mfm_not_supported, "mfm not supported")
{
	T_SKIP("mfm not supported on this platform");
}

#endif // defined(__LP64__)
