#ifndef __XZONE_TESTING_H__
#define __XZONE_TESTING_H__

#include <darwintest.h>

#include <../src/internal.h>

#if CONFIG_XZONE_MALLOC

extern malloc_zone_t **malloc_zones;

static inline void
assert_zone_is_xzone_malloc(malloc_zone_t *zone)
{
	T_ASSERT_GE(zone->version, 14, "zone version");
	T_ASSERT_EQ(zone->introspect->zone_type, MALLOC_ZONE_TYPE_XZONE,
			"zone is xzone malloc");
}

static inline xzm_malloc_zone_t
get_default_xzone_zone(void)
{
	malloc_zone_t *dz = malloc_zones[0];
	assert_zone_is_xzone_malloc(dz);

	return (xzm_malloc_zone_t)dz;
}

#endif // CONFIG_XZONE_MALLOC

#endif // __XZONE_TESTING_H__
