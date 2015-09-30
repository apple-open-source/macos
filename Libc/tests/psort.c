#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

#include <bsdtests.h>

typedef unsigned long T;

static int
comparT(const void* a, const void* b) {
	const T x = *(T*)a, y = *(T*)b;
	return x < y ? -1 : x > y ? 1 : 0;
}

static void
test_psort(void)
{
	struct timeval tv_start, tv_stop;
	struct rusage ru_start, ru_stop;
	unsigned long pwt, put, qwt, qut;

	T *buf, *sorted;
	const size_t nel = 20480000;
	const size_t width = sizeof(T), bufsiz = nel * width;

	buf = malloc(bufsiz);
	arc4random_buf(buf, bufsiz);
	sorted = malloc(bufsiz);
	memcpy(sorted, buf, bufsiz);

	getrusage(RUSAGE_SELF, &ru_start);
	gettimeofday(&tv_start, NULL);
	psort(sorted, nel, width, comparT);
	gettimeofday(&tv_stop, NULL);
	getrusage(RUSAGE_SELF, &ru_stop);

	pwt = ((uint64_t)tv_stop.tv_sec * USEC_PER_SEC + tv_stop.tv_usec) -
			((uint64_t)tv_start.tv_sec * USEC_PER_SEC + tv_start.tv_usec);
	put = ((uint64_t)ru_stop.ru_utime.tv_sec * USEC_PER_SEC + ru_stop.ru_utime.tv_usec) -
			((uint64_t)ru_start.ru_utime.tv_sec * USEC_PER_SEC + ru_start.ru_utime.tv_usec);

	getrusage(RUSAGE_SELF, &ru_start);
	gettimeofday(&tv_start, NULL);
	qsort(buf, nel, width, comparT);
	gettimeofday(&tv_stop, NULL);
	getrusage(RUSAGE_SELF, &ru_stop);

	qwt = ((uint64_t)tv_stop.tv_sec * USEC_PER_SEC + tv_stop.tv_usec) -
			((uint64_t)tv_start.tv_sec * USEC_PER_SEC + tv_start.tv_usec);
	qut = ((uint64_t)ru_stop.ru_utime.tv_sec * USEC_PER_SEC + ru_stop.ru_utime.tv_usec) -
			((uint64_t)ru_start.ru_utime.tv_sec * USEC_PER_SEC + ru_start.ru_utime.tv_usec);

	bool match = true;
	for (size_t i = 0; i < nel; i++) {
		if (!(match = (buf[i] == sorted[i]))) break;
	}

	free(sorted);
	free(buf);

	test_double_less_than_or_equal("psort/qsort wall time", (double)pwt/qwt, 1.0);
	test_double_less_than_or_equal("qsort/psort user time", (double)qut/put, 1.0);
	test_long("psort matches qsort", match, true);
}

int main(void)
{
	test_start("psort");
	test_psort();
	test_stop();

	return 0;
}
