#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/attr.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

struct attr_buffer {
	uint32_t	length;
	uint32_t	mount_flags;
};

int
main(int argc, char **argv)
{
	int i;
	struct attrlist attrs;
	struct attr_buffer attrbuf;

	if (argc < 2) {
		fprintf(stderr, "Usage: checktrigger <pathname>...\n");
		return 1;
	}
	argv++;
	argc--;
	for (i = 0; i < argc; i++) {
		memset(&attrs, 0, sizeof(attrs));
		attrs.bitmapcount = ATTR_BIT_MAP_COUNT;
		attrs.dirattr = ATTR_DIR_MOUNTSTATUS;
		if (getattrlist(argv[i], &attrs, &attrbuf, sizeof attrbuf,
		    FSOPT_NOFOLLOW) == -1) {
			fprintf(stderr, "checktrigger: getattrlist of %s failed: %s\n",
			    argv[i], strerror(errno));
			return 2;
		}
		printf("%s %s a trigger\n", argv[i],
		    (attrbuf.mount_flags & DIR_MNTSTATUS_TRIGGER) ?
		        "is" : "is not");
	}
	return 0;
}
