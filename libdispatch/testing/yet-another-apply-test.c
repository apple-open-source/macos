#include <dispatch/dispatch.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <assert.h>

static uint64_t total;

#define LAPS (256 * 1024 * 1024)
#define SIZE (LAPS * sizeof(int))

int
main(int argc, char *argv[])
{
	dispatch_queue_t cq = dispatch_get_concurrent_queue(0);
	struct stat sb;
	long double cycles;
	uint64_t s, e, d;
	uint64_t tmp_total;
	int r, fd;
	const int *nums;
	size_t i, stride;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	// make sure to have 2GB + of RAM installed and run this before hand:
	// dd if=/dev/random bs=1024k count=1024 of=/tmp/testfile
	fd = open(argv[1], O_RDONLY);
	assert(fd != -1);

	r = fstat(fd, &sb);
	assert(r != -1);
	assert(sb.st_size >= (off_t)SIZE);

	nums = mmap(NULL, SIZE, PROT_READ, MAP_FILE|MAP_PRIVATE, fd, 0);
	assert(nums != MAP_FAILED);

	// force pages to be faulted in
	for (i = 0; i < LAPS; i++) {
		total += nums[i];
	}

	for (stride = 1; stride < (LAPS + 1); stride <<= 1) {
		total = 0;
		s = mach_absolute_time();
		dispatch_apply(LAPS / stride, cq, ^(size_t idx) {
			const int *nums2 = nums + (idx * stride);
			uint64_t ptotal = 0;
			size_t idx2 = 0;

			// assert(stride > 0);
			do {
				ptotal += nums2[idx2++];
			} while (idx2 < stride);

			__sync_fetch_and_add(&total, ptotal);
		});
		e = mach_absolute_time();
		d = e - s;
		cycles = d;
		cycles /= LAPS;
		printf("da%lu:\t%Lf ns\n", stride, cycles);
	}

	tmp_total = 0;
	total = 0;
	s = mach_absolute_time();
	for (i = 0; i < LAPS; i++) {
		tmp_total += nums[i];
	}
	total = tmp_total;
	e = mach_absolute_time();
	d = e - s;
	cycles = d;
	cycles /= LAPS;
	printf("naïve:\t%Lf ns\n", cycles);


	tmp_total = 0;
	total = 0;
	s = mach_absolute_time();
#pragma omp parallel for reduction(+:tmp_total)
	for (i = 0; i < LAPS; i++) {
		tmp_total += nums[i];
	}
	total = tmp_total;
	e = mach_absolute_time();
	d = e - s;
	cycles = d;
	cycles /= LAPS;
	printf("OpenMP:\t%Lf ns\n", cycles);

	exit(EXIT_SUCCESS);
}
