/*
 * Copyright (c) 2010 Apple Inc. All rights reserved.
 */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>

/* from sys/cprotect.h */
#define PROTECTION_CLASS_A 1
#define PROTECTION_CLASS_B 2
#define PROTECTION_CLASS_C 3
#define PROTECTION_CLASS_D 4
#define PROTECTION_CLASS_E 5
#define PROTECTION_CLASS_F 6

void 
usage(void)
{
	printf("usage: setclass <path> [A-F]\n");
	printf("\tsets <path> to a protection class from A to F.\n");
	printf("\tIf no class is specified, reports the current class for <path>.\n");
	exit(0);
}

int
chartoclass(char c)
{
	switch (c) {
	case 'A':
	case 'a':
		return PROTECTION_CLASS_A;
	case 'B':
	case 'b':
		return PROTECTION_CLASS_B;
	case 'C':
	case 'c':
		return PROTECTION_CLASS_C;
	case 'D':
	case 'd':
		return PROTECTION_CLASS_D;
	case 'E':
	case 'e': 
		return PROTECTION_CLASS_E;
	case 'F':
	case 'f':
		return PROTECTION_CLASS_F;
	default:
		usage();
		exit(0);
	}
	return 0;
}

char
classtochar(int class)
{
	return 'A' + (class - 1);
}

int 
main(int argc, char **argv)
{
	int error = 0, class = 0, do_set = 0, fd = 0;
	DIR *dp = NULL;
	struct stat buf = {0};

	if ((argc < 2) || (argc > 3))
		usage();
	
	error = stat(argv[1], &buf);
	if (error) {
		printf("Error - could not stat path %s\n", argv[1]);
		exit(0);		
	}
	
	if (S_ISDIR(buf.st_mode)) {
		if((dp = opendir(argv[1])) == NULL) {
			printf("Error - could not open directory %s\n", argv[1]);
			exit(0);
		}
		
		fd = dirfd(dp);		
	} else if (S_ISREG(buf.st_mode)) {
		fd = open(argv[1], O_RDWR);
		if (fd < 0) {
			printf("Error - could not open file %s\n", argv[1]);
			exit(0);
		}		
	} else {
		printf("Error - path is not a regular file or directory %s\n", argv[1]);
		exit(0);
	}
		
	if (argv[2]) {
		do_set = 1;
		class = chartoclass(*argv[2]);
	}

	if (do_set) {

		error = fcntl(fd, F_SETPROTECTIONCLASS, class);
		if (error)
			printf("could not set protection class %c: %s\n", classtochar(class), strerror(errno));

	} else {

		class = fcntl(fd, F_GETPROTECTIONCLASS);
		if (class < 0) {
			if (dp != NULL && errno == EFAULT) {
				printf("%s has no protection class set\n", argv[1]);
			} else {
				printf("could not get protection class: %s\n", strerror(errno));
				error = class;
			}
		} else {
			printf("%s is in protection class %c\n", argv[1], classtochar(class));
		}

	}
	
	if(dp)
		closedir(dp);

	if(fd)
		close(fd);

	return error;
}
