#include <darwintest.h>

#include "xzone_testing.h"

#if CONFIG_XZONE_MALLOC && CONFIG_VM_USER_RANGES

#include "../src/xzone/xzone_segment.c"

struct ptr_range_test {
	const char *desc;
	struct {
		struct mach_vm_range left_void;
		struct mach_vm_range right_void;
		uint64_t ptr_range_size;
		uint64_t entropy;
	} input;
	struct {
		struct mach_vm_range expected_ranges[2];
		size_t range_count_out;
	} range_output;
	struct {
		struct xzm_range_group_s expected_range_groups[XZM_RANGE_GROUP_PTR + 2];
	} range_group_output;
};

static void
test_fake_range_groups_init(struct xzm_range_group_s *range_groups)
{
	// Copied from main xzone setup
	size_t rg_idx = 0;
	size_t allocation_front_count = 2;
	for (size_t i = 0; i < XZM_RANGE_GROUP_COUNT; i++) {
		xzm_range_group_id_t rgid = (xzm_range_group_id_t)i;
		size_t rg_fronts = (rgid == XZM_RANGE_GROUP_PTR) ?
				allocation_front_count : 1;
		for (size_t j = 0; j < rg_fronts; j++) {
			xzm_range_group_t rg = &range_groups[rg_idx];
			rg->xzrg_id = rgid;
			rg->xzrg_front = (xzm_front_index_t)j;
			rg->xzrg_main_ref = NULL;
			_malloc_lock_init(&rg->xzrg_lock);

			rg_idx++;
		}
	}
}

static void
test_exhaust_range_group(xzm_range_group_t rg)
{
	uintptr_t last_allocated = 0;
	uint64_t total_allocated = 0;
	while (true) {
		uintptr_t addr = _xzm_range_group_bump_alloc_segment(rg,
				XZM_SEGMENT_SIZE);
		if (!addr) {
			T_EXPECT_GE(total_allocated, (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					"allocated at least expected amount");
			break;
		}

		T_QUIET; T_EXPECT_EQ(addr % XZM_SEGMENT_SIZE, 0ull,
				"segment alignment");
		if (rg->xzrg_direction == XZM_FRONT_INCREASING) {
			T_QUIET; T_EXPECT_GE((uint64_t)addr,
					last_allocated + XZM_SEGMENT_SIZE, "address increased");
			T_QUIET; T_EXPECT_LE(addr + XZM_SEGMENT_SIZE,
					rg->xzrg_base + rg->xzrg_size + rg->xzrg_skip_size,
					"address in bounds");
			if (rg->xzrg_skip_addr && addr >= rg->xzrg_skip_addr) {
				T_QUIET; T_EXPECT_GE((uint64_t)addr,
						rg->xzrg_skip_addr + rg->xzrg_skip_size,
						"skip respected");
			}
		} else {
			T_QUIET; T_EXPECT_LE((uint64_t)addr,
					last_allocated - XZM_SEGMENT_SIZE, "address decreased");
			T_QUIET; T_EXPECT_GE((uint64_t)addr,
					rg->xzrg_base - (rg->xzrg_size + rg->xzrg_skip_size),
					"address in bounds");
			if (rg->xzrg_skip_addr && addr < rg->xzrg_skip_addr) {
				T_QUIET; T_EXPECT_LE((uint64_t)addr,
						rg->xzrg_skip_addr -
								(rg->xzrg_skip_size + XZM_SEGMENT_SIZE),
						"skip respected");
			}
		}

		last_allocated = addr;
		total_allocated += XZM_SEGMENT_SIZE;
	}
}

static void
test_ptr_range_setup(struct ptr_range_test *test)
{
	T_LOG("testing %s", test->desc);

	struct mach_vm_range ranges[2];
	size_t range_count = 2;

	_xzm_main_malloc_zone_choose_ptr_ranges(test->input.left_void,
			test->input.right_void, test->input.ptr_range_size,
			test->input.entropy, ranges, &range_count);

	T_ASSERT_EQ(range_count, test->range_output.range_count_out,
			"range_count_out");
	for (size_t i = 0; i < range_count; i++) {
		T_EXPECT_EQ(ranges[i].min_address,
				test->range_output.expected_ranges[i].min_address,
				"expected min address");
		T_EXPECT_EQ(ranges[i].max_address,
				test->range_output.expected_ranges[i].max_address,
				"expected max address");
	}

	struct xzm_range_group_s range_groups[XZM_RANGE_GROUP_PTR + 2] = { 0 };
	test_fake_range_groups_init(range_groups);

	_xzm_main_malloc_zone_init_ptr_fronts(range_groups, 2,
			(struct xzm_vm_range *)ranges, range_count, NULL);

	for (size_t i = XZM_RANGE_GROUP_PTR; i < XZM_RANGE_GROUP_PTR + 2; i++) {
		xzm_range_group_t actual = &range_groups[i];
		xzm_range_group_t expected =
				&test->range_group_output.expected_range_groups[i];
		T_EXPECT_EQ(actual->xzrg_id, expected->xzrg_id, "xzrg_id");
		T_EXPECT_EQ_INT(actual->xzrg_front, expected->xzrg_front, "xzrg_front");
		T_EXPECT_EQ(actual->xzrg_base, expected->xzrg_base, "xzrg_base");
		T_EXPECT_EQ(actual->xzrg_size, expected->xzrg_size, "xzrg_size");
		T_EXPECT_EQ(actual->xzrg_skip_addr, expected->xzrg_skip_addr,
				"xzrg_skip_addr");
		T_EXPECT_EQ(actual->xzrg_skip_size, expected->xzrg_skip_size,
				"xzrg_skip_size");
		T_EXPECT_EQ(actual->xzrg_next, expected->xzrg_next, "xzrg_next");
		T_EXPECT_EQ(actual->xzrg_direction, expected->xzrg_direction,
				"xzrg_direction");

		test_exhaust_range_group(actual);
	}
}

T_DECL(xzone_segment_ptr_range_setup, "set up ptr ranges")
{
	struct ptr_range_test empty_left_beginning = {
		.desc = "empty left void, ptr range at the beginning",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(16),
			},
			.right_void = {
				.min_address = GiB(26),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = 0,
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(30),
					.max_address = GiB(30) + XZM_POINTER_RANGE_SIZE,
				},
			},
			.range_count_out = 1,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(38) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(38) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(38) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(38) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&empty_left_beginning);

	// (63 - 16) - ((10 + 4) + 16) == 17
	empty_left_beginning.input.entropy +=
			((GiB(17) / XZM_PAGE_TABLE_GRANULE) + 1) * 3;
	empty_left_beginning.desc =
			"empty left void, ptr range at beginning (entropy offset)";
	test_ptr_range_setup(&empty_left_beginning);

	struct ptr_range_test empty_left_middle = {
		.desc = "empty left void, ptr range in the middle",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(16),
			},
			.right_void = {
				.min_address = GiB(26),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(17) / XZM_PAGE_TABLE_GRANULE) + 1) * 42) +
					(GiB(5) / XZM_PAGE_TABLE_GRANULE),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(35),
					.max_address = GiB(35) + XZM_POINTER_RANGE_SIZE,
				},
			},
			.range_count_out = 1,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(43) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(43) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(43) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(43) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&empty_left_middle);

	struct ptr_range_test empty_left_end = {
		.desc = "empty left void, ptr range at the end",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(16),
			},
			.right_void = {
				.min_address = GiB(26),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(17) / XZM_PAGE_TABLE_GRANULE) + 1) * 42) +
					(GiB(17) / XZM_PAGE_TABLE_GRANULE),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(47),
					.max_address = GiB(47) + XZM_POINTER_RANGE_SIZE,
				},
			},
			.range_count_out = 1,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(55) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(55) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(55) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(55) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&empty_left_end);

	struct ptr_range_test left_void_too_small_beginning = {
		.desc = "small left void, ptr range at the beginning",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(17),
			},
			.right_void = {
				.min_address = GiB(27),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(16) / XZM_PAGE_TABLE_GRANULE) + 1) * 42),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(31),
					.max_address = GiB(31) + XZM_POINTER_RANGE_SIZE,
				},
			},
			.range_count_out = 1,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(39) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(39) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(39) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(39) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&left_void_too_small_beginning);

	struct ptr_range_test left_void_split_decreasing = {
		.desc = "small left void, ptr range split on left",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(23),
			},
			.right_void = {
				.min_address = GiB(33),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(13) / XZM_PAGE_TABLE_GRANULE) + 1) * 42) +
					(GiB(1) / XZM_PAGE_TABLE_GRANULE),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(17),
					.max_address = GiB(19),
				},
				{
					.min_address = GiB(37),
					.max_address = GiB(51),
				},
			},
			.range_count_out = 2,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(43) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(43) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(43) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = GiB(37),
					.xzrg_skip_size = GiB(18),
					.xzrg_next = GiB(43) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&left_void_split_decreasing);

	struct ptr_range_test split_exact_middle_left = {
		.desc = "split almost exactly down the middle on the left",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(30),
			},
			.right_void = {
				.min_address = GiB(40),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(13) / XZM_PAGE_TABLE_GRANULE) + 1) * 42) +
					(GiB(2) / XZM_PAGE_TABLE_GRANULE),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(18),
					.max_address = GiB(26),
				},
				{
					.min_address = GiB(44),
					.max_address = GiB(52),
				},
			},
			.range_count_out = 2,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(44) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(44) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(44) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = GiB(44),
					.xzrg_skip_size = GiB(18),
					.xzrg_next = GiB(44) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&split_exact_middle_left);

	struct ptr_range_test split_exact_middle_right = {
		.desc = "split almost exactly down the middle on the right",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(30),
			},
			.right_void = {
				.min_address = GiB(40),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(13) / XZM_PAGE_TABLE_GRANULE) + 1) * 42) +
					((GiB(2) - MiB(32)) / XZM_PAGE_TABLE_GRANULE),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(18) - MiB(32),
					.max_address = GiB(26),
				},
				{
					.min_address = GiB(44),
					.max_address = GiB(52) - MiB(32),
				},
			},
			.range_count_out = 2,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = (GiB(26) - MiB(32)) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = GiB(26),
					.xzrg_skip_size = GiB(18),
					.xzrg_next = (GiB(26) - MiB(32)) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = (GiB(26) - MiB(32)) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = (GiB(26) - MiB(32)) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&split_exact_middle_right);

	struct ptr_range_test empty_right_void_end = {
		.desc = "empty right void, last possible position",
		.input = {
			.left_void = {
				.min_address = GiB(16),
				.max_address = GiB(53),
			},
			.right_void = {
				.min_address = GiB(63),
				.max_address = GiB(63),
			},
			.ptr_range_size = XZM_POINTER_RANGE_SIZE,
			.entropy = (((GiB(17) / XZM_PAGE_TABLE_GRANULE) + 1) * 42) +
					(GiB(17) / XZM_PAGE_TABLE_GRANULE),
		},
		.range_output = {
			.expected_ranges = {
				{
					.min_address = GiB(33),
					.max_address = GiB(49),
				},
			},
			.range_count_out = 1,
		},
		.range_group_output = {
			.expected_range_groups = {
				[XZM_RANGE_GROUP_PTR + 0] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 0,
					.xzrg_base = GiB(41) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(41) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) - MiB(16),
					.xzrg_direction = XZM_FRONT_INCREASING,
				},
				[XZM_RANGE_GROUP_PTR + 1] = {
					.xzrg_id = XZM_RANGE_GROUP_PTR,
					.xzrg_front = 1,
					.xzrg_base = GiB(41) + MiB(16),
					.xzrg_size = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_skip_addr = 0,
					.xzrg_skip_size = 0,
					.xzrg_next = GiB(41) + MiB(16),
					.xzrg_remaining = (XZM_POINTER_RANGE_SIZE / 2) + MiB(16),
					.xzrg_direction = XZM_FRONT_DECREASING,
				},
			}
		}
	};

	test_ptr_range_setup(&empty_right_void_end);
}

#else // CONFIG_XZONE_MALLOC && CONFIG_VM_USER_RANGES

T_DECL(xzm_segment_not_supported, "xzone segment tests not supported",
		T_META_ENABLED(false))
{
	T_SKIP("xzone segment tests not supported on this platform");
}

#endif // CONFIG_XZONE_MALLOC && CONFIG_VM_USER_RANGES
