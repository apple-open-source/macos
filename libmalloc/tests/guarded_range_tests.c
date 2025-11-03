#include <darwintest.h>
#include <darwintest_utils.h>
#include <stdlib.h>
#include <malloc/malloc.h>
#include <../src/internal.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED);

#define NUM_EXECUTIONS 64
#define BUFFER_SIZE 256

static void
check_unique_slide_count(char *arg)
{
	uint64_t *results = calloc(NUM_EXECUTIONS, sizeof(uint64_t));
	char *cmd[] = {
		"/AppleInternal/Tests/libmalloc/assets/guarded_range_test_tool",
		arg,
		NULL
	};

	for (int i = 0; i < NUM_EXECUTIONS; i++) {
		dt_spawn_t spawn = dt_spawn_create(NULL);
		dt_spawn(spawn, cmd,
				^(char *line, __unused size_t size){
					results[i] = (uint64_t)strtoull(line, NULL, 16);
					T_LOG("Slide for child (%d): 0x%llx\n", i, results[i]);
				},
				^(__unused char *line, __unused size_t size){ });

		bool exited, signaled;
		int status, signal;
		dt_spawn_wait(spawn, &exited, &signaled, &status, &signal);

		T_QUIET; T_EXPECT_TRUE(exited, "helper tool should have exited");
		T_QUIET; T_EXPECT_FALSE(signaled, "helper tool should not have been signaled");
		T_QUIET; T_EXPECT_EQ(status, 0, "helper tool should have succeeded");
	}

	qsort_b(results, NUM_EXECUTIONS, sizeof(results[0]),
			^(const void *a, const void *b){
				uint64_t val_a = *(const uint64_t*)a;
				uint64_t val_b = *(const uint64_t*)b;

				if (val_a < val_b) return -1;
				if (val_a > val_b) return 1;
				return 0;
			});

	size_t unique_count = 0;
	for (size_t i = 1; i < NUM_EXECUTIONS; i++) {
		if (results[i] != results[i-1]) {
			unique_count++;
		}
	}
	free(results);

	T_LOG("Unique slide values: %zu\n", unique_count);
	T_ASSERT_GT(unique_count, (unsigned long)(NUM_EXECUTIONS / 2), NULL);
}


T_DECL(guarded_range_check_data_offset_zone,
	"Check that the main zone is placed at a random enough offset from __DATA",
	T_META_ENVVAR("MallocProbGuard=0"),
	T_META_TAG_XZONE_ONLY)
{
	check_unique_slide_count("zone");
	T_PASS("Main zone is at a random offset form __DATA");
}

T_DECL(guarded_range_check_data_offset_zone_array,
	"Check that the zone array is placed at a random enough offset from __DATA",
	T_META_ENVVAR("MallocProbGuard=0"),
	T_META_TAG_XZONE_ONLY)
{
	check_unique_slide_count("array");
	T_PASS("Zone array is at a random offset form __DATA");
}
