/*
 * Copyright (c) 2010-2013 Apple Inc. All rights reserved.
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
		/* directory 'unset' operation */
	case '0':
		return 0;
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
	if (class < 0) {
		/* negative classes are invalid */
		return -1;
	}

	/* otherwise, it must be >= 0... */
	if (class == 0) {
		/* Directories are allowed to be "unset" */
		return 0;
	}
	return 'A' + (class - 1);
}

int 
main(int argc, char **argv)
{
	int error = 0, class = 0, do_set = 0, fd = 0;
	struct stat buf = {0};

	if ((argc < 2) || (argc > 3))
		usage();

	if (argv[2]) {
		do_set = 1;
		class = chartoclass(*argv[2]);
	}

	error = stat(argv[1], &buf);
	if (error) {
		printf("Error - could not stat path %s\n", argv[1]);
		exit(0);		
	}

	/*
	 * If we're trying to set the protection class, go through normal open(2).
	 * This will deny opens on protected files if the device is locked.
	 */
	if (do_set) {
		fd = open (argv[1], O_RDONLY);

		if (fd < 0) {
			if (S_ISDIR(buf.st_mode)) {
				printf("Error - could not open directory %s\n", argv[1]);
			}
			else if (S_ISREG(buf.st_mode)){
				printf("Error - could not open file %s\n", argv[1]);
			}
			else {
				printf("Error - path is not a regular file or directory %s\n", argv[1]);
			}
			exit(0);
		}
	}
	else {
		/*
		 * The open_dprotected_np syscall allows us to acquire an FD to query the
		 * protection class even if the device is locked.
		 */
		fd = open_dprotected_np (argv[1], O_RDONLY, 0, O_DP_GETRAWENCRYPTED);

		if (fd < 0) {
			if (S_ISDIR(buf.st_mode)) {
				printf("Error - could not open directory %s\n", argv[1]);
			}
			else if (S_ISREG(buf.st_mode)){
				printf("Error - could not open file %s\n", argv[1]);
			}
			else {
				printf("Error - path is not a regular file or directory %s\n", argv[1]);
			}
			exit(0);
		}
	}

	/* Now make the fcntl call */
	if (do_set) {
		error = fcntl(fd, F_SETPROTECTIONCLASS, class);
		if (error) {
			char new_class = classtochar(class);
			if (new_class == 0) {
				printf("could not set protection class (directory none):  %s\n", strerror(errno));
			}
			else {
				printf("could not set protection class %c: %s\n", new_class, strerror(errno));
			}
		}
	} 
	else {
		class = fcntl(fd, F_GETPROTECTIONCLASS);
		if (class < 0) {
			if ((errno == EFAULT) && (S_ISDIR(buf.st_mode))) {
				/* Directories are allowed to not have a class set. */
				printf("%s has no protection class set\n", argv[1]);
			} 
			else {
				printf("could not get protection class: %s\n", strerror(errno));
				error = class;
			}
		} 
		else {
			char new_class = classtochar(class);
			if (new_class == 0) {
				printf("%s is in protection class (directory none) \n", argv[1]);
			}
			else {
				printf("%s is in protection class %c\n", argv[1], new_class);
			}
		}
	}
	
	if (fd >= 0) {
		close(fd);
	}

	return error;
}
