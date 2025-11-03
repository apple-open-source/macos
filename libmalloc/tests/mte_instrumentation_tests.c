#include <darwintest.h>
#include "mte_testing.h"

#if MALLOC_MTE_TESTING_SUPPORTED

#include <arm_acle.h>
#include <mach/mach.h>

// We need to pass CONFIG_MTE=1 when building this test, as otherwise the
// instrumentation file we will include will not have the code we mean to
// test.
#ifndef CONFIG_MTE
#error "CONFIG_MTE is not defined"
#elif !CONFIG_MTE
#error "CONFIG_MTE needs to be true"
#endif

// This file implements unit tests for the implementation of the instrumentation
// we have in instrumentation.c; therefore, we directly include the
// implementation file here.
#include <../src/instrumentation.c>

#define T_CHECK_TAGS(__ptr, __size, __msg) do { \
	T_QUIET; T_ASSERT_TRUE(((uintptr_t)(__ptr) & 0xf) == 0, \
			"Should be 16-bytes aligned"); \
	T_QUIET; T_ASSERT_TRUE((__size & 0xf) == 0, "Should be multiple of 16"); \
	for (size_t __s = 0; __s < __size; __s += 16) { \
		uint8_t *__p = &((uint8_t *)__ptr)[__s]; \
		uint8_t *__ldg = __arm_mte_get_tag(__p); \
		T_QUIET; T_ASSERT_EQ_PTR(__p, __ldg, \
			__msg " (%p[%zu]: %p : %p)", __ptr, __s, __p, __ldg); \
	} \
} while (0)

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true),
		T_META_TAG_VM_NOT_PREFERRED, T_META_TAG_XZONE_ONLY);

// Allocate `page_cnt` contiguous taggable pages.
// Asserts that all operations succeed.
static void
allocate_mte_pages(size_t page_cnt, vm_address_t *address)
{
	T_QUIET; T_ASSERT_GT(page_cnt, 0ul, "page_cnt must be > 0");
	vm_address_t addr = 0;
	kern_return_t kr = vm_allocate(mach_task_self(), &addr,
			PAGE_SIZE * page_cnt, VM_FLAGS_ANYWHERE | VM_FLAGS_MTE);
	T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "allocate tagged memory");
	T_QUIET; T_ASSERT_NE(addr, 0ul, "vm_allocate returned NULL");
	*address = addr;
}

// Choose a random 16-byte granule within the given pages, guaranteeing that
// there are at least `room` bytes between the start of the granule and the
// end of the last page.
static uint8_t *
get_random_granule(vm_address_t page_addr, size_t pages_cnt, size_t room)
{
	uint32_t idx = arc4random_uniform(PAGE_SIZE * pages_cnt - room);
	vm_address_t addr = (page_addr + idx) & ~0xf;
	return (uint8_t *)addr;
}

T_DECL(mte_unit_memtag_set_tag,
	"Check that memtag_set_tag works for sizes between 1 and 32768")
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	vm_address_t page_addr = 0;
	const size_t max_sz = 32 << 10;
	const size_t pages_cnt = max_sz / PAGE_SIZE ?: 1;
	allocate_mte_pages(pages_cnt, &page_addr);

	size_t step = 1;
	for (size_t sz = 1; sz <= max_sz; sz += step) {
		size_t sz = memtag_p2roundup(sz, 16);
		uint8_t *granule = get_random_granule(page_addr, pages_cnt, sz);
		uint8_t *tag_addr = __arm_mte_create_random_tag(granule, 0x0001);

		T_CHECK_TAGS(granule, sz, "Initial tag should be zero");

		memtag_set_tag(tag_addr, sz);
		T_CHECK_TAGS(tag_addr, sz, "Tag should be set correctly");

		memtag_set_tag(granule, sz);
		T_CHECK_TAGS(granule, sz, "Tag should be back to zero");

		if (sz == 1024) {
			step = 256;
		}
	}

	T_PASS("memtag_set_tag");
}

T_DECL(mte_unit_memtag_exclude_tag,
	"Check that _memtag_exclude_tag works")
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	uint8_t *page_addr;
	const uint64_t mask = 0x0001;
	allocate_mte_pages(1, (vm_address_t *)&page_addr);

	// Choose a non-canonical tag.
	uint8_t *tagged_addr = __arm_mte_create_random_tag(page_addr, mask);
	// Set the tag for the granule.
	__arm_mte_set_tag(tagged_addr);

	// Test _memtag_exclude_tag with a pointer with the right logical tag.
	uint64_t m = _memtag_exclude_tag(tagged_addr, mask);
	T_QUIET; T_ASSERT_EQ(__builtin_popcount(m), 2,
			"Two tags should be excluded (tagged pointer)");

	// Test _memtag_exclude_tag with a pointer without the right logical tag.
	m = _memtag_exclude_tag(page_addr, mask);
	T_QUIET; T_ASSERT_EQ(__builtin_popcount(m), 2,
			"Two tags should be excluded (canonical pointer)");
	T_PASS("_memtag_exclude_tag");
}

typedef enum {
	block_placement_in_page,
	block_placement_end,
	block_placement_beginning,
	block_placement_across_pages,
} block_placement_t;

#define kBlockTagLeft 0xa
#define kBlockTagCenter 0x1
#define kBlockTagRight 0xb

static void
_check_memtag_assign_tag_loop(uint8_t *block_ptr, size_t sz,
		block_placement_t placement)
{
	for (size_t i = 0; i < 1024; i++) {
		uint8_t *new_g = memtag_assign_tag(block_ptr, sz);
		uint8_t new_tag = PTR_EXTRACT_TAG(new_g);
		T_QUIET; T_ASSERT_NE(new_tag, 0, "Should not be the canonical tag");
		T_QUIET; T_ASSERT_NE(new_tag, kBlockTagCenter,
				"Should not be equal to the current tag");

		switch (placement) {
		case block_placement_in_page:
		case block_placement_across_pages:
			// We should be able to exclude both the left and right tags.
			// This applies also when the block spans across pages.
			T_QUIET; T_ASSERT_NE(new_tag, kBlockTagLeft,
					"Should not be equal to the adjacent tag (left)");
			T_QUIET; T_ASSERT_NE(new_tag, kBlockTagRight,
					"Should not be equal to the adjacent tag (right)");
			break;

		case block_placement_end:
			// We are only able to exclude the left neighbour's tag.
			T_QUIET; T_ASSERT_NE(new_tag, kBlockTagLeft,
					"Should not be equal to the adjacent tag (left)");
			break;

		case block_placement_beginning:
			// We are only able to exclude the right neighbour's tag.
			T_QUIET; T_ASSERT_NE(new_tag, kBlockTagRight,
					"Should not be equal to the adjacent tag (right)");
			break;
		}
	}
}

static void
_unit_test_memtag_assign_tag(block_placement_t placement)
{
	uint8_t *page_addr;
	const uint64_t mask = 0x0001;
	const size_t num_pages = (placement == block_placement_in_page) ? 1 : 2;
	allocate_mte_pages(num_pages, (vm_address_t *)&page_addr);

	for (size_t sz = 16; sz < PAGE_SIZE / 4; sz *= 2) {
		// Choose 3 blocks: [l][g][r]
		uint8_t *l = NULL;
		uint8_t *g = NULL;
		uint8_t *r = NULL;

		switch (placement) {
		case block_placement_in_page:
			// Get 3 blocks within the page
			l = get_random_granule((vm_address_t)page_addr, num_pages, 3 * sz);
			g = l + sz;
			r = g + sz;
			T_QUIET; T_ASSERT_TRUE((r + sz) <= (page_addr + PAGE_SIZE),
				"Right granule within the first page");
			break;

		case block_placement_end:
			// The center block should be at the end of the first page
			g = page_addr + PAGE_SIZE - sz;
			l = g - sz;
			r = g + sz;
			break;

		case block_placement_beginning:
			// The center block should be at the beginning of the second page
			g = page_addr + PAGE_SIZE;
			l = g - sz;
			r = g + sz;
			break;

		case block_placement_across_pages:
			// The center block should span across the two pages
			g = page_addr + PAGE_SIZE - (sz / 2);
			l = g - sz;
			r = g + sz;
			break;
		}
		T_QUIET; T_ASSERT_TRUE(l >= page_addr, "Left is within the first page");

		// Tag them with fixed tags [a][1][b]
		uint8_t *tg = PTR_SET_TAG(g, kBlockTagCenter);
		uint8_t *tl = PTR_SET_TAG(l, kBlockTagLeft);
		uint8_t *tr = PTR_SET_TAG(r, kBlockTagRight);
		for (size_t i = 0; i < sz; i += 16) {
			__arm_mte_set_tag(tg + i);
			__arm_mte_set_tag(tl + i);
			__arm_mte_set_tag(tr + i);
		}
		// Verify the tags on the first granule of each block.
		T_QUIET; T_ASSERT_EQ_PTR(tg, __arm_mte_get_tag(g), "Tagged (g)");
		T_QUIET; T_ASSERT_EQ_PTR(tl, __arm_mte_get_tag(l), "Tagged (l)");
		T_QUIET; T_ASSERT_EQ_PTR(tr, __arm_mte_get_tag(r), "Tagged (r)");

		// Assign a new tag to g, using the tagged pointer
		_check_memtag_assign_tag_loop(tg, sz, placement);

		// Assign a new tag to g, using the canonical pointer
		_check_memtag_assign_tag_loop(g, sz, placement);
	}
}

T_DECL(mte_unit_memtag_assign_tag_in_page,
	"Check that memtag_assign_tag (block_placement_in_page)")
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	_unit_test_memtag_assign_tag(block_placement_in_page);
	T_PASS("memtag_assign_tag (block_placement_in_page)");
}

T_DECL(mte_unit_memtag_assign_tag_end,
	"Check that memtag_assign_tag (block_placement_end)")
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	_unit_test_memtag_assign_tag(block_placement_end);
	T_PASS("memtag_assign_tag (block_placement_end)");
}

T_DECL(mte_unit_memtag_assign_tag_beginning,
	"Check that memtag_assign_tag (block_placement_beginning)")
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	_unit_test_memtag_assign_tag(block_placement_beginning);
	T_PASS("memtag_assign_tag (block_placement_beginning)");
}

T_DECL(mte_unit_memtag_init_chunk,
	"Check that memtag_init_chunk works")
{
	T_SKIP_REQUIRES_SEC_TRANSITION();

	uint8_t *page_addr = NULL;
	allocate_mte_pages(1, (vm_address_t *)&page_addr);

	for (size_t sz = 16; sz < PAGE_SIZE / 4; sz += 16) {
		const size_t remainder = PAGE_SIZE % sz;
		// Reset the tags on the page
		for (size_t i = 0; i < PAGE_SIZE; i += 16) {
			__arm_mte_set_tag(&page_addr[i]);
		}

		memtag_init_chunk(page_addr, PAGE_SIZE, sz);

		uint8_t prev_tag = 0;
		for (size_t i = 0; i < PAGE_SIZE - remainder; i += sz) {
			uint8_t *p = &page_addr[i];
			uint8_t *block_ldg = __arm_mte_get_tag(p);
			uint16_t cur_tag = PTR_EXTRACT_TAG(block_ldg);

			T_QUIET; T_ASSERT_NE(cur_tag, prev_tag,
				"Adjacent blocks should have different tags");
			T_QUIET; T_ASSERT_NE(cur_tag, 0,
				"Block should have a non-canonical tag");

			// Check that all the granules of the block have the same tag
			for (size_t j = 0; j < sz; j += 16) {
				uint8_t *g = &page_addr[i + j];
				uint8_t *granule_ldg = __arm_mte_get_tag(g);
				uint16_t granule_tag = PTR_EXTRACT_TAG(granule_ldg);
				T_QUIET; T_ASSERT_EQ(cur_tag, granule_tag,
						"Granule should have a consistent tag", sz, i);
			}
		}

		// Check that the remainder is canonically tagged
		for (size_t i = PAGE_SIZE - remainder; i < PAGE_SIZE; i += 16) {
			uint8_t *p = &page_addr[i];
			uint8_t *ldg = __arm_mte_get_tag(p);
			T_QUIET; T_ASSERT_EQ_PTR(p, ldg,
					"Remainder (sz=%zu, %zu) should have canonical tag",
					sz, remainder);
		}
	}
	T_PASS("memtag_init_chunk");
}
#else // MALLOC_MTE_TESTING_SUPPORTED

T_DECL(mte_unit_unsupported_target, "Skip testing on unsupported targets",
		T_META_TAG_VM_PREFERRED, T_META_TAG_NO_ALLOCATOR_OVERRIDE)
{
	T_SKIP("MTE unit tests are only implemented for arm64 targets");
}

#endif // MALLOC_MTE_TESTING_SUPPORTED
