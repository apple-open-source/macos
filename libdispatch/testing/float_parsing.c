#include <dispatch/dispatch.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#define MAXLAPS (512 * 1024)

static void
test(size_t LAPS, char *nums)
{
	uint64_t concurrent_cycles;
	uint64_t serial_cycles;
	char **result_strings;
	char *nums_off;
	double *results;
	size_t i;

	result_strings = calloc(1, sizeof(char *) * LAPS);
	assert(result_strings);

	results = calloc(1, sizeof(double) * LAPS);
	assert(results);

	printf("%zu random floats\n", LAPS);

	i = 0;
	nums_off = nums;
	do {
		result_strings[i] = nums_off;
		do {
			nums_off++;
			assert(*nums_off);
		} while (*nums_off != '\n');
		i++;
		nums_off++;
	} while (i < LAPS);

	for (i = 0; i < LAPS; i++) {
		assert(result_strings[i]);
	}

	concurrent_cycles = dispatch_benchmark(10, ^{
		dispatch_apply(LAPS, dispatch_get_concurrent_queue(0), ^(size_t idx) {
			results[idx] = strtod(result_strings[idx], NULL);
		});
	});

	for (i = 0; i < LAPS; i++) {
		assert(results[i]);
	}

	serial_cycles = dispatch_benchmark(10, ^{
		size_t k = 0;
		do {
			results[k] = strtod(result_strings[k], NULL);
			k++;
		} while (k < LAPS);
	});

	for (i = 0; i < LAPS; i++) {
		assert(results[i]);
	}

	printf(	"\tserial cycles:\t%llu\n"
			"\tapply() cycles:\t%llu\n"
			"\tserial / concurrent: %.2Lf\n",
		serial_cycles, concurrent_cycles,
		(long double)serial_cycles / (long double)concurrent_cycles);

	free(result_strings);
	free(results);
}

int
main(void)
{
	char path[PATH_MAX] = "/tmp/random_floats_XXXXXX";
	struct stat sb;
	double words[1000];
	char buf[1024];
	char *nums;
	int fd, rfd;
	size_t i, j;
	ssize_t r;

	rfd = open("/dev/random", O_RDONLY);
	assert(rfd != -1);

	fd = mkstemp(path);
	assert(fd != -1);

	r = unlink(path);
	assert(r != -1);

	i = 0;
	do {
		r = read(rfd, words, sizeof(words));
		assert(r == sizeof(words));
		for (j = 0; j < 1000; j++) {
			if (isnormal(words[j])) {
				r = write(fd, buf, snprintf(buf, sizeof(buf), "%.20e\n", words[j]));
				assert(r != -1);
				i++;
			}
		}
	} while (i < MAXLAPS);

	r = close(rfd);
	assert(r != -1);

	r = fstat(fd, &sb);
	assert(r != -1);

	nums = mmap(NULL, sb.st_size, PROT_READ, MAP_FILE|MAP_SHARED, fd, 0);
	assert(nums != MAP_FAILED);

	for (i = MAXLAPS; i > 0; i /= 2) {
		test(i, nums);
	}

	r = munmap(nums, sb.st_size);
	assert(r != -1);

	r = close(fd);
	assert(r != -1);

	return 0;
}
