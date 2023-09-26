//
//  malloc_create_purgeable_zone.c
//  libmalloc
//
//  Test for creating a purgeable zone while concurrently adding/removing other zones
//

#include <darwintest.h>
#include <malloc/malloc.h>

#define N_ZONE_CREATION_THREADS 8

static void *
make_purgeable_thread(void *arg)
{
	T_LOG("enable PGM");
	malloc_zone_t *purgeable_zone = malloc_default_purgeable_zone();
	T_ASSERT_NOTNULL(purgeable_zone, "malloc_default_purgeable_zone returned NULL");

	return NULL;
}

static void *
zone_thread(void *arg)
{
	vm_size_t start_size = (vm_size_t)arg;
	while (1) {
		malloc_zone_t *zone = malloc_create_zone(start_size, 0);
		malloc_destroy_zone(zone);
	}
	return NULL;
}

T_DECL(malloc_create_purgeable_zone, "create a purgeable zone while constantly registering zones",
		T_META_TAG_XZONE)
{
	pthread_t zone_threads[N_ZONE_CREATION_THREADS];
	for (int i = 0; i < N_ZONE_CREATION_THREADS; i++) {
		vm_size_t zone_start_size = 1000;
		pthread_create(&zone_threads[i], NULL, zone_thread, (void *)zone_start_size);
	}

	usleep(50);

	pthread_t purgeable_thread;
	pthread_create(&purgeable_thread, NULL, make_purgeable_thread, NULL);
	pthread_join(purgeable_thread, NULL);

	usleep(500);

	T_PASS("finished without crashing");
}
