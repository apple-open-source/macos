

#import <Foundation/Foundation.h>

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <radix_tree.h>
#include <radix_tree_internal.h>

#include <darwintest.h>

#include "../src/radix_tree_debug.c"

bool failed = false;

#if 0

#define ASSERT(x) \
	if (!(x)) {   \
		abort();  \
	}
#define ASSERT_EQ(x, y) \
	if ((x) != (y)) {   \
		abort();        \
	}

#else

#define ASSERT(x)      \
	if (!(x)) {        \
		failed = true; \
	}
#define ASSERT_EQ(x, y) \
	if ((x) != (y)) {   \
		failed = true;  \
	}

#endif

static int
log2i(uint64_t x)
{
	if (x == 1) {
		return 0;
	}
	if (x & 1) {
		abort();
	}
	return 1 + log2(x >> 1);
}

T_DECL(radix_tree_test, "radix_tree_test")
{
	bool ok;

	// size_t size = 100 * 4096;
	// void *buf = malloc(size);
	// memset(buf, 0xde, size);
	// struct radix_tree *tree = radix_tree_init(buf, size);

	struct radix_tree *tree = radix_tree_create();
	ASSERT(tree);

	const int n = 999;

	uint64_t minsize = 4096;

	for (uint64_t i = 0; i < n * minsize; i += minsize) {
		ok = radix_tree_insert(&tree, i, minsize, 3 * i);
		ASSERT(ok);

		/* printf("@@@@@@@@@@@--------\n"); */
		/* radix_tree_print(tree); */
		/* printf("@@@@@@@@@@---------\n"); */

		for (uint64_t j = 0; j < n * minsize; j += minsize) {
			// printf ("testing %lld %lld\n", i, j);
			if (j <= i) {
				ASSERT(radix_tree_lookup(tree, j) == 3 * j);
			} else {
				ASSERT(radix_tree_lookup(tree, j) == radix_tree_invalid_value);
			}
		}
	}

	T_EXPECT_FALSE(failed, "insert 1 to n");

	for (uint64_t i = 0; i < n * minsize; i += minsize) {
		ok = radix_tree_delete(&tree, i, minsize);
		ASSERT(ok);
		for (uint64_t j = 0; j < n * minsize; j += minsize) {
			if (j > i) {
				ASSERT(radix_tree_lookup(tree, j) == 3 * j);
			} else {
				ASSERT(radix_tree_lookup(tree, j) == radix_tree_invalid_value);
			}
		}
	}

	T_EXPECT_FALSE(failed, "delete 1 to n");

	for (uint64_t i = 0; i < n * minsize; i += minsize) {
		ok = radix_tree_insert(&tree, i, minsize, 3 * i);
		ASSERT(ok);
	}

	for (uint64_t i = n * minsize; i > 0;) {
		i -= minsize;

		//		 printf("@@@@@@@@@@@--------\n");
		//		 radix_tree_print(tree);
		//		 printf("@@@@@@@@@@---------\n");

		ok = radix_tree_delete(&tree, i, minsize);

		ASSERT(ok);
		for (uint64_t j = 0; j < n * minsize; j += minsize) {
			if (j < i) {
				ASSERT_EQ(radix_tree_lookup(tree, j), 3 * j);
			} else {
				ASSERT_EQ(radix_tree_lookup(tree, j), radix_tree_invalid_value);
			}
		}
	}

	T_EXPECT_FALSE(failed, "delete n to 1");

	srand(12345);

	NSMutableDictionary *d = [[NSMutableDictionary alloc] init];

	for (uint64_t i = 0; i < n; i++) {
		@autoreleasepool {
			for (int j = 0; j < 3; j++) {
				uint64_t key = minsize * (rand() + ((uint64_t)rand() << 32));
				uint64_t value = rand();

				//				printf("@@@@@@@@@@@--------\n");
				//				radix_tree_print(tree);
				//				printf("@@@@@@@@@@---------inserting %llx\n", key);

				ok = radix_tree_insert(&tree, key, minsize, value);

				//				printf("@@@@@@@@@@---------ok\n");

				ASSERT(ok);

				d[@(key)] = @(value);
			}

			NSArray *array = [d allKeys];
			id k = [array objectAtIndex:rand() % [array count]];

			ok = radix_tree_delete(&tree, [k unsignedLongLongValue], minsize);
			ASSERT(ok);
			[d removeObjectForKey:k];

			for (id k in d) {
				ASSERT_EQ(radix_tree_lookup(tree, [k unsignedLongLongValue]), [d[k] unsignedLongLongValue]);
			}

			uint64_t count = radix_tree_count(tree);

			//			for (id k in d) {
			//				printf("key %llx -> %llx\n", [k unsignedLongLongValue], [d[k] unsignedLongLongValue]);
			//			}
			//			radix_tree_print(tree);

			ASSERT_EQ((long)(count % minsize), 0l);
			ASSERT_EQ([[d allKeys] count], (long)(count / minsize));
		}
	}

	T_EXPECT_FALSE(failed, "random");

	for (id k in d) {
		radix_tree_delete(&tree, [k unsignedLongLongValue], minsize);
	}
	T_EXPECT_EQ_ULLONG(0ull, radix_tree_count(tree), "delete randoms");

	ASSERT_EQ(radix_tree_lookup(tree, 0), -1);
	ASSERT_EQ(radix_tree_lookup(tree, minsize - 1), -1);
	ASSERT_EQ(radix_tree_lookup(tree, minsize), -1);

	ok = radix_tree_insert(&tree, 0, minsize, 0xf00);
	ASSERT(ok);
	ASSERT_EQ(radix_tree_lookup(tree, 0), 0xf00);
	ASSERT_EQ(radix_tree_lookup(tree, minsize - 1), 0xf00);
	ASSERT_EQ(radix_tree_lookup(tree, minsize), -1);

	// this would abort:
	//	ok = radix_tree_insert(&tree, -minsize, minsize, 0xb00);
	//	ASSERT(!ok);

	ok = radix_tree_insert(&tree, -2 * minsize, minsize, 0xb00);
	ASSERT(ok);

	ASSERT_EQ(radix_tree_lookup(tree, -2 * minsize), 0xb00);
	ASSERT_EQ(radix_tree_lookup(tree, -2 * minsize - 1), -1);
	ASSERT_EQ(radix_tree_lookup(tree, -2 * minsize + 1), 0xb00);
	ASSERT_EQ(radix_tree_lookup(tree, -2 * minsize + minsize - 1), 0xb00);
	ASSERT_EQ(radix_tree_lookup(tree, -1 * minsize), -1);
	ASSERT_EQ(radix_tree_lookup(tree, minsize), -1);

	radix_tree_delete(&tree, -2 * minsize, minsize);
	radix_tree_delete(&tree, 0, minsize);
	ASSERT_EQ(radix_tree_count(tree), 0);

	T_EXPECT_FALSE(failed, "off by 1");

	int modelsize = 1024;
	uint32_t model[modelsize];

	while (log2i(modelsize) + log2i(minsize) <= 64) {
		memset(model, 0xff, sizeof(model));

		for (int i = 0; i < n; i++) {
			uint64_t start = rand() % modelsize;
			if (start == modelsize - 1 && log2i(modelsize) + log2i(minsize) == 64) {
				start = modelsize - 2;
			}
			uint64_t maxsize = modelsize - start;
			uint64_t size;
			if (maxsize / 4 > 1) {
				size = (rand() % (maxsize / 4)) + (rand() % (maxsize / 4)) + (rand() % (maxsize / 4)) + (rand() % (maxsize / 4));
			} else if (maxsize > 1) {
				size = rand() % maxsize;
			} else {
				size = 1;
			}
			if (size == 0) {
				size = 1;
			}
			if (minsize * start + minsize * size < minsize * start) {
				size--;
			}
			if (minsize * start + minsize * size < minsize * start) {
				abort();
			}
			if (size == 0) {
				abort();
			}
			uint32_t value = rand();

			// printf("inserting %llx, %llx\n", start*minsize, size*minsize);
			// radix_tree_print(tree);

			ok = radix_tree_insert(&tree, start * minsize, size * minsize, value);
			ASSERT(ok);

			// radix_tree_print(tree);
			ASSERT(radix_tree_fsck(tree));

			for (uint64_t i = start; i < start + size; i++) {
				model[i] = value;
			}

			for (uint64_t j = 0; j < modelsize; j++) {
				uint64_t expected;
				if (model[j] == (uint32_t)-1) {
					expected = (uint64_t)-1;
				} else {
					expected = model[j];
				}

				// radix_tree_print(tree);
				uint64_t ans = radix_tree_lookup(tree, j * minsize);
				// printf("j*minsize=%llx expected=%llx ans=%llx\n", j*minsize, expected, ans);
				ASSERT_EQ(expected, ans);
			}
		}

		T_EXPECT_FALSE(failed, "model %d", log2i(minsize) + log2i(modelsize));

		radix_tree_delete(&tree, 0, -1);
		T_EXPECT_EQ_ULLONG(0ull, radix_tree_count(tree), "delete model");

		minsize *= 2;
		tree->leaf_size_shift++;
	}
}

T_DECL(radix_tree_holes, "radix_tree_holes_test")
{
	bool ok;
	struct radix_tree *tree = radix_tree_create();
	T_ASSERT_NOTNULL(tree, "radix_tree_create()");

	uint64_t size = 0xff00000;
	uint64_t minsize = 0x1000;
	uint64_t start = 0x10303c000;

	ok = radix_tree_insert(&tree, start, size, 0xf00ba);

	T_ASSERT_TRUE(ok, "created region");

	ok = radix_tree_fsck(tree);
	T_QUIET;
	T_ASSERT_TRUE(ok, "fsck");

	for (uint64_t addr = start; addr < start + size; addr += minsize) {
		T_QUIET;
		T_ASSERT_EQ_ULLONG(radix_tree_lookup(tree, addr), 0xf00ball, "stackid");
		uint64_t index = (addr - start) / minsize;
		if (index % 2) {
			ok = radix_tree_delete(&tree, addr, minsize);
			T_QUIET;
			T_ASSERT_TRUE(ok, "deleted odd %lld", index);
		}
	}

	for (uint64_t addr = start; addr < start + size; addr += minsize) {
		uint64_t index = (addr - start) / minsize;
		T_QUIET;
		T_ASSERT_EQ_ULLONG(radix_tree_lookup(tree, addr), index % 2 ? -1 : 0xf00ball, "stackid");
		if (!(index % 2)) {
			ok = radix_tree_delete(&tree, addr, minsize);
			T_QUIET;
			T_ASSERT_TRUE(ok, "deleted even, %lld", index);
		}
	}

	ok = radix_tree_fsck(tree);
	T_ASSERT_TRUE(ok, "fsck");
}
