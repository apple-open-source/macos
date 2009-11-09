#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <dispatch.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <assert.h>

int
main(int argc, char *argv[])
{
	struct stat sb;
	char **numbers;
	char *map;
	size_t i, j;
	int r, fd;

	if (argc != 2) {
		fprintf(stderr, "usage: %s <file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	fd = open(argv[1], O_RDONLY);
	assert(fd != -1);

	r = fstat(fd, &sb);
	assert(r != -1);

	if (sb.st_len == 0) {
		fprintf(stderr, "The file is zero length.\n");
		exit(EXIT_FAILURE);
	}

	map = mmap(NULL, sb.st_len, PROT_READ, MAP_FILE, fd, 0);
	assert(map != MAP_FAILED);

	numbers = malloc(sb.st_len * sizeof(void *)); /* more than enough */
	assert(numbers);

	/* XXX finish me */
	numbers[0] = map;
	j = 1;
	for (i = 0; i < sb.st_len; i++) {
		if (map[i] == '\n') {
			numbers[j] = map + i;
			j++;
			continue;
		}
		i++;
	}
	i
			; i < sb.st_len; i++) {
		if (map[i]
	}

	dispatch_apply(b, cnt);

	exit(EXIT_SUCCESS);
}
