/*
 * Copyright (c) 2010-2020 Apple Inc. All rights reserved.
 */
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/attr.h>

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
	int error = 0, class = 0, do_set = 0;
	static struct attrlist req = {
		.bitmapcount = ATTR_BIT_MAP_COUNT,
		.commonattr = ATTR_CMN_DATA_PROTECT_FLAGS
	};

	if ((argc < 2) || (argc > 3))
		usage();

	if (argv[2]) {
		do_set = 1;
		class = chartoclass(*argv[2]);
	}

	/*
	 * setclass and getclass for `argv[1]` using setattrlist(2) and
	 * getattrlist(2) respectively.
	 */
	if (do_set) {
		struct {
			uint32_t prot_class;
		} __attribute__((packed, aligned(4))) attrs = {
			.prot_class = class
		};

		error = setattrlist(argv[1], (void *)&req, &attrs, sizeof(attrs),
							 FSOPT_NOFOLLOW);
		if (error) {
			char new_class = classtochar(class);
			if (new_class == 0) {
				warn("could not set protection class of %s to (directory none)",
					 argv[1]);
			}
			else {
				warn("could not set protection class of %s to %c", argv[1],
					 new_class);
			}
		}
	}
	else {
		req.commonattr |= ATTR_CMN_RETURNED_ATTRS;

		struct {
			uint32_t len;
			attribute_set_t returned;
			uint32_t prot_class;
		} __attribute__((packed, aligned(4))) attrs;

		error = getattrlist(argv[1], (void *)&req, &attrs, sizeof(attrs),
							 FSOPT_NOFOLLOW);
		if (error == -1 || attrs.len != sizeof(attrs) ||
			attrs.returned.commonattr != req.commonattr) {
			if (error == -1) {
				error = errno;
			}
			else {
				error = EINVAL;
			}
			err(error, "could not get protection class");
		}
		else {
			class = attrs.prot_class;
			char new_class = classtochar(class);
			if (new_class == 0) {
				printf("%s is in protection class (directory none) \n", argv[1]);
			}
			else {
				printf("%s is in protection class %c\n", argv[1], new_class);
			}
		}
	}

	return error;
}
