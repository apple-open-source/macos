#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <malloc/malloc.h>

extern malloc_zone_t **malloc_zones;

uint8_t data[256];

int main(int argc, const char *argv[])
{
	const char *pgm = getenv("MallocProbGuard");
	const char *bypass_pgm_check = getenv("GuardedRangeTestBypassPGMCheck");
	uintptr_t addr;

	assert(argc == 2);
	assert(pgm || bypass_pgm_check);
	if (strcmp(argv[1], "zone") == 0) {
		addr = (uintptr_t)malloc_zones[0];
	} else if (strcmp(argv[1], "array") == 0) {
		addr = (uintptr_t)malloc_zones;
	} else {
		assert(false && "Argument 1 should be either 'zone' or 'array'");
	}

	printf("0x%lx\n", (uintptr_t)data - addr);
	return 0;
}
