
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>

const char *progname;

int
statit(const char *path)
{
	struct stat sb;

	printf("%s:\n", path);
	if (stat(path, &sb) != 0) {
		warn("stat: %s", path);
		return (-1);
	}
#define sf(feild)	printf("  " #feild "=%ld\n", (long)sb.feild);
	sf(st_dev);
	sf(st_ino);
	printf("  st_mode=%06o\n", sb.st_mode);
	sf(st_nlink);
	sf(st_uid);
	sf(st_gid);
	sf(st_rdev);
	sf(st_size);
	sf(st_atime);
	sf(st_mtime);
	sf(st_ctime);
	sf(st_blksize);
	sf(st_blocks);
	sf(st_flags);
	return (0);
}

int
main(int argc, char **argv)
{
	int error;

	error = 0;

	progname = argv[0];
	argv++;
	if (argv[0] == NULL) {
		fprintf(stderr, "usage: %s file [files...]\n", progname);
		exit(EXIT_FAILURE);
	}
	while (*argv) {
		if (statit(*argv) != 0)
			error = 1;
		argv++;
	}
	return (error == 0 ? EXIT_SUCCESS : EXIT_FAILURE);
}

