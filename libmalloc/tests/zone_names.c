//
//  zone_names.c
//  libmalloc
//
//  Documents default zone names and tests malloc_set_zone_name().
//

#include <darwintest.h>

#include <malloc/malloc.h>
#include <stdlib.h>  // free()

#include "../src/platform.h"  // CONFIG_NANOZONE

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

static void
check_zone_names(malloc_zone_t **zones, const char **names, uint32_t count)
{
	for (uint32_t i = 0; i < count; i++) {
		const char *zone_name = malloc_get_zone_name(zones[i]);
		T_EXPECT_NOTNULL(zone_name, "zone name not null");
		T_EXPECT_EQ_STR(zone_name, names[i], "zone name %u", i);
	}
}

extern int32_t malloc_num_zones;
extern malloc_zone_t **malloc_zones;
static void
check_default_zone_names(const char **names, uint32_t count) {
	T_EXPECT_EQ(malloc_num_zones, count, "zone count");
	check_zone_names(malloc_zones, names, count);
}

T_DECL(default_zone, "Zone names: default",
		T_META_ENVVAR("MallocNanoZone=0"))
{
	const char *names[] = {"DefaultMallocZone"};
	check_default_zone_names(names, 1);
}

T_DECL(default_zone_and_nano, "Zone names: default + nano",
		T_META_ENVVAR("MallocNanoZone=1"))
{
#if CONFIG_NANOZONE
	const char *names[] = {"DefaultMallocZone", "MallocHelperZone"};
	check_default_zone_names(names, 2);
#else
	T_SKIP("Nano allocator not configured");
#endif
}

T_DECL(default_zone_and_pgm, "Zone names: default + ProbGuard",
		T_META_ENVVAR("MallocProbGuard=1"), T_META_ENVVAR("MallocNanoZone=0"))
{
	const char *names[] = {"ProbGuardMallocZone", "DefaultMallocZone"};
	check_default_zone_names(names, 2);
}

T_DECL(zone_singletons, "Zone singletons")
{
	malloc_zone_t *zones[] = {
		malloc_default_zone(),
		malloc_default_purgeable_zone()
	};
	const char *names[] = {
		"DefaultMallocZone",
		"DefaultPurgeableMallocZone"
	};
	check_zone_names(zones, names, 2);
}

static malloc_zone_t *
call_malloc_zone_from_ptr(void)
{
	void *ptr = malloc(5);
	malloc_zone_t *zone = malloc_zone_from_ptr(ptr);
	free(ptr);
	return zone;
}

T_DECL(virtual_zone0, "Ensure we return the virtual zone in place of zone 0")
{
	malloc_zone_t *virtual_zone = malloc_default_zone();
	T_EXPECT_NE(virtual_zone, malloc_zones[0], NULL);
	T_EXPECT_EQ(call_malloc_zone_from_ptr(), virtual_zone, NULL);
}

T_DECL(zone_creation, "Zone creation")
{
	T_EXPECT_NULL(malloc_create_zone(0, 0)->zone_name, "No name");
}

static void
check_set_zone_name(malloc_zone_t* zone)
{
	#define zone_name malloc_get_zone_name(zone)

	char *literal_str = "string literal";
	malloc_set_zone_name(zone, literal_str);
	T_EXPECT_EQ(zone_name, literal_str, "skip copy for immutable string");

	char *heap_str = strdup("heap string");
	malloc_set_zone_name(zone, heap_str);
	T_EXPECT_NE(zone_name, heap_str, "defensive copy");
	T_EXPECT_EQ_STR(zone_name, heap_str, "same contents");
	free(heap_str);

	const char *copy = zone_name;
	malloc_set_zone_name(zone, NULL);
	T_EXPECT_NULL(zone_name, "zone name set to NULL");
	T_EXPECT_EQ(malloc_size(copy), 0ul, "copy freed");
}

T_DECL(malloc_set_zone_name, "malloc_set_zone_name")
{
	malloc_zone_t *zones[] = {
		malloc_default_zone(),
		malloc_create_zone(0, 0)
	};

	size_t count = sizeof(zones) / sizeof(zones[0]);
	for (uint32_t i = 0; i < count; i++) {
		check_set_zone_name(zones[i]);
	}
}
