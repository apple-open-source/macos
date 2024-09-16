//
//  malloc_zone_unregister_test.c
//  libmalloc
//
//  Tests for malloc_zone_unregister().
//

#include <darwintest.h>

#include <mach/mach.h>  // mach_task_self()
#include <malloc_private.h>
#include <malloc/malloc.h>
#include <stdlib.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true), T_META_TAG_XZONE);

extern int32_t malloc_num_zones;
extern malloc_zone_t **malloc_zones;

static malloc_zone_t *
get_wrapped_zone(malloc_zone_t *zone)
{
  malloc_zone_t *wrapped_zone;
  kern_return_t kr = malloc_get_wrapped_zone(mach_task_self(),
      /*memory_reader=*/NULL, (vm_address_t)zone, (vm_address_t *)&wrapped_zone);
  T_QUIET; T_ASSERT_EQ(kr, KERN_SUCCESS, "malloc_get_wrapped_zone() failed");
  return wrapped_zone;
}

T_DECL(malloc_zone_unregister_establish_custom_default_zone,
		"Unregister all initial zones and register a custom default zone",
		T_META_ENVVAR("MallocNanoZone=1"), T_META_TAG_VM_NOT_PREFERRED)
{
  void *ptr = malloc(7);
  T_EXPECT_NOTNULL(malloc_zone_from_ptr(ptr), "can find zone for allocation");
  T_EXPECT_TRUE(malloc_claimed_address(ptr), "ptr is claimed");

  T_ASSERT_LE(malloc_num_zones, 10, "at most 10 initial zones");
  malloc_zone_t *initial_zones[10];
  uint32_t initial_zone_count = malloc_num_zones;

  // Unregister initial zones
  for (uint32_t i = 0; i < initial_zone_count; i++) {
    initial_zones[i] = malloc_zones[0];
    malloc_zone_unregister(malloc_zones[0]);
  }
  T_EXPECT_EQ(malloc_num_zones, 0, "unregistered initial zones");

  // No zones, no results, no crash
  T_EXPECT_NULL(malloc_zone_from_ptr(ptr), "cannot find zone");

  // Create and register custom default zone
  malloc_zone_t *custom_zone = malloc_create_zone(0, 0);

  // Custom default zone only, no results, no crash
  T_EXPECT_NULL(malloc_zone_from_ptr(ptr), "cannot find zone");

  // Re-register initial zones
  for (uint32_t i = 0; i < initial_zone_count; i++) {
    malloc_zone_register(initial_zones[i]);
  }
  uint32_t additional_zone_count = get_wrapped_zone(custom_zone) ? 2 : 1;
  T_EXPECT_EQ(malloc_num_zones, initial_zone_count + additional_zone_count, "re-registered initial zones");

  // Custom default zone plus initial zones
  T_EXPECT_NOTNULL(malloc_zone_from_ptr(ptr), "can find zone for allocation");
  T_EXPECT_TRUE(malloc_claimed_address(ptr), "ptr is claimed");

  // Check that the custom zone is the default zone
  void *ptr2 = malloc(7);
  T_EXPECT_EQ(malloc_zones[0], custom_zone, "custom zone is zone 0");
  T_EXPECT_EQ(malloc_zone_from_ptr(ptr2), malloc_default_zone(), "we lookup the virtual zone for the custom zone");
  T_EXPECT_TRUE(malloc_zone_claimed_address(custom_zone, ptr2), "custom zone claims ptr");

  free(ptr2);
  free(ptr);
}
