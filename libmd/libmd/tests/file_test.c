#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "sha512.h"

static void
usage(void)
{

	fprintf(stderr, "usage: %s file [offset] [length]\n", getprogname());
	exit(1);
}

void
parse_num(const char *num, unsigned long *valp)
{
	char *endp;

	errno = 0;
	*valp = strtoul(num, &endp, 10);
	if (*endp != '\0' || errno != 0)
		usage();
}

int
main(int argc, char *argv[])
{
	char buf[SHA512_DIGEST_STRING_LENGTH];
	const char *file;
	unsigned long ofs, length;

	if (argc < 2 || argc > 4)
		usage();

	file = argv[1];
	if (*file == '\0')
		usage();

	ofs = length = 0;
	if (argc >= 3)
		parse_num(argv[2], &ofs);
	if (argc >= 4)
		parse_num(argv[3], &length);

	if (ofs == 0 && length == 0)
		SHA512_File(file, buf);
	else
		SHA512_FileChunk(file, buf, ofs, length);
	printf("%s\n", buf);
	return (0);
}
