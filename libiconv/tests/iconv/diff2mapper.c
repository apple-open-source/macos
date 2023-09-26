#include <sys/queue.h>

#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

#include <getopt.h>

struct range {
	LIST_ENTRY(range)	rentry;
	unsigned int		start;
	unsigned int		end;
	unsigned int		rhs;
	bool			fixed;
};

#define	RANGE_START(r)	(r)->start
#define	RANGE_END(r)	(r)->end
#define	RANGE_SIZE(r)	(RANGE_END(r) - RANGE_START(r))
#define	RANGE_RIGHT_START(r)	(r)->rhs
#define	RANGE_RIGHT_END(r)	\
    (RANGE_RIGHT_START(r) + ((r)->fixed ? 0 : RANGE_SIZE(r)))

LIST_HEAD(, range) rlist = LIST_HEAD_INITIALIZER(rlist);

static int
insert_entry(unsigned int lhs, unsigned int rhs)
{
	struct range *rng, *last, *higher;

	rng = last = higher = NULL;
	LIST_FOREACH(rng, &rlist, rentry) {
		last = rng;

		if (RANGE_START(rng) > lhs) {
			higher = rng;
			break;
		}

		/* XXX: Maybe have to coalesce again after? */
		if (lhs != RANGE_END(rng) + 1)
			/* Not a continuation of this range. */
			continue;

		if (rng->fixed && rhs == RANGE_RIGHT_END(rng)) {
			rng->end++;
			return (0);
		}

		/* Fixed ranges > 1 cannot be converted to non-fixed. */
		if (rng->fixed && RANGE_SIZE(rng) > 1)
			continue;

		/* Non-fixed or size 1 fixed, we can convert it to non-fixed. */
		if (rhs == RANGE_RIGHT_END(rng) + 1) {
			rng->fixed = false;
			rng->end++;
			return (0);
		}
	}

	/* New entry needed. */
	rng = calloc(1, sizeof(*rng));
	if (rng == NULL)
		return (ENOMEM);
	RANGE_START(rng) = RANGE_END(rng) = lhs;
	RANGE_RIGHT_START(rng) = rhs;

	/*
	 * All ranges start out as fixed unless we see a subsequent entry that
	 * isn't fixed.  The rules for breaking up a fixed mapping are described
	 * above.
	 */
	rng->fixed = true;

	if (higher != NULL)
		LIST_INSERT_BEFORE(higher, rng, rentry);
	else if (last != NULL)
		LIST_INSERT_AFTER(last, rng, rentry);
	else
		LIST_INSERT_HEAD(&rlist, rng, rentry);
	return (0);
}

static void
dump_range(unsigned int start, unsigned int end, unsigned int rhs, bool fixed)
{

	if (start == end)
		printf("0x%.04X = 0x%.04X\n", start, rhs);
	else
		printf("0x%.04X - 0x%.04X = 0x%.04X%s\n", start, end, rhs,
		    fixed ? "" : " -");
}

static void
dump_ranges(void)
{
	struct range *rng;

	rng = NULL;
	LIST_FOREACH(rng, &rlist, rentry) {
		dump_range(RANGE_START(rng), RANGE_END(rng),
		    RANGE_RIGHT_START(rng), rng->fixed);
	}
}

int
main(int argc, char *argv[])
{
	char *line;
	size_t linecap;
	ssize_t len;
	unsigned int lhs, rhs, mask;
	int ch, error;
	bool swapleft, swapright;

	swapleft = swapright = false;

	/*
	 * Take lines of the form:
	 * -0xABCD = <value>
	 * Convert to:
	 * 0xCDAB = <value>
	 */
	while ((ch = getopt(argc, argv, "lr")) != -1) {
		switch (ch) {
		case 'l':
			swapleft = true;
			break;
		case 'r':
			swapright = true;
			break;
		default:
			fprintf(stderr, "usage: %s [-lr]\n", getprogname());
			exit(1);
			break;
		}
	}

	line = NULL;
	linecap = 0;
	while ((len = getline(&line, &linecap, stdin)) > 0) {
		if (sscanf(line, "-0x%x = 0x%x", &lhs, &rhs) != 2) {
			continue;
		}

		if (swapleft) {
			mask = ((lhs & 0xffff0000) >> 16);
			lhs = __builtin_bswap32(lhs);
			if ((mask & 0x00ff) == 0)
				lhs >>= 8;
			if ((mask & 0xff00) == 0)
				lhs >>= 8;
		}
		if (swapright) {
			mask = ((rhs & 0xffff0000) >> 16);
			rhs = __builtin_bswap32(rhs);
			if ((mask & 0x00ff) == 0)
				rhs >>= 8;
			if ((mask & 0xff00) == 0)
				rhs >>= 8;
		}

		if ((error = insert_entry(lhs, rhs)) != 0)
			errc(1, error, "insert_entry");
	}

	dump_ranges();

	return (0);
}
