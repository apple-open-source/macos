
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufsutil.c
 * - program to probe for the existence of a UFS filesystem
 *   and return its name
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */

#include <sys/loadable_fs.h>
#include <servers/netname.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/errno.h>


#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 
#include <sys/stat.h>
#include <sys/time.h> 
#include <sys/mount.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <sys/param.h>

#include <ufs/ffs/fs.h>

#include "ufslabel.h"

#define UFS_FS_NAME		"ufs"
#define UFS_FS_NAME_FILE	"UFS"

static void 
usage(const char * progname)
{
    fprintf(stderr, "usage: %s -p device_arg\n", progname);
    fprintf(stderr, "device_arg:\n");
    fprintf(stderr, "       device we are acting upon (for example, 'sd2')\n");
    return;

}

union sbunion {
    struct fs	sb;
    char	raw[SBSIZE];
};

boolean_t
read_superblock(int fd, char * dev)
{
    union sbunion	superblock;
    
    if (lseek(fd, SBOFF, SEEK_SET) != SBOFF) {
	fprintf(stderr, "read_superblock: lseek %s failed, %s\n", dev,
		strerror(errno));
	goto fail;
    }
    if (read(fd, &superblock, SBSIZE) != SBSIZE) {
#ifdef DEBUG
		fprintf(stderr, "read_superblock: read %s failed, %s\n", dev,
			strerror(errno));
#endif DEBUG
	goto fail;
    }
    if (ntohl(superblock.sb.fs_magic) == FS_MAGIC) {
#ifdef DEBUG
	fprintf(stderr, "%x (big endian)\n", ntohl(superblock.sb.fs_magic));
#endif DEBUG
    }
    else if (superblock.sb.fs_magic == FS_MAGIC) {
#ifdef DEBUG
	fprintf(stderr, "%x (little endian)\n", superblock.sb.fs_magic);
#endif DEBUG
    }
    else
	goto fail;
    
    return (TRUE);
    
 fail:
    return (FALSE);
}

int 
main(int argc, const char *argv[])
{
    char		dev[512];
    char		opt;
    struct stat		sb;

    if (argc < 3 || argv[1][0] != '-') {
	usage(argv[0]);
	exit(FSUR_INVAL);
    }
    opt = argv[1][1];
    if (opt != FSUC_PROBE && opt != 'n') {
	usage(argv[0]);
	exit(FSUR_INVAL);
    }

    sprintf(dev, "/dev/r%s", argv[2]);
    if (stat(dev, &sb) != 0) {
	fprintf(stderr, "%s: stat %s failed, %s\n", argv[0], dev,
		strerror(errno));
	exit(FSUR_INVAL);
    }
    switch (opt) {
    case FSUC_PROBE: {
	FILE *		f;
	char		filename[MAXPATHLEN];
	int 		fd;
	int		len;
	u_char		name[UFS_MAX_LABEL_NAME + 1];

	sprintf(filename, "%s/ufs%s/ufs.label", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	unlink(filename);
	sprintf(filename, "%s/ufs%s/ufs.name", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	unlink(filename);

	fd = open(dev, O_RDONLY, 0);
	if (fd <= 0) {
	    fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
		    strerror(errno));
	    exit(FSUR_UNRECOGNIZED);
	}
	if (read_superblock(fd, dev) == FALSE) {
	    exit(FSUR_UNRECOGNIZED);
	}
	len = sizeof(name) - 1;
	if (ufslabel_get(fd, name, &len) == FALSE) {
	    fprintf(stderr, "%s: couldn't read the label\n",
		    argv[0]);
	    exit(FSUR_RECOGNIZED);
	}
	name[len] = '\0';
	close(fd);

	/* write the ufs.label file */
	sprintf(filename, "%s/ufs%s/ufs.label", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	f = fopen(filename, "w");
	if (f == NULL) {
	    fprintf(stderr, "%s: fopen %s failed, %s\n", argv[0], filename,
		    strerror(errno));
	    exit (FSUR_RECOGNIZED);
	}
	fprintf(f, "%s", name);
	fclose(f);

	/* write the ufs.name file */
	sprintf(filename, "%s/ufs%s/ufs.name", FS_DIR_LOCATION,
		FS_DIR_SUFFIX);
	f = fopen(filename, "w");
	if (f == NULL) {
	    fprintf(stderr, "%s: fopen %s failed, %s\n", argv[0], filename,
		    strerror(errno));
	    exit (FSUR_RECOGNIZED);
	}
	fprintf(f, UFS_FS_NAME_FILE);
	fclose(f);
	break;
    }
    case 'n': {
	int 	fd;
	char * 	name;

	if (argc < 4) {
	    usage(argv[0]);
	    exit(FSUR_INVAL);
	}
	name = (char *)argv[3];
	if (strchr(name, '/') || strchr(name, ':')) {
	    fprintf(stderr, 
		    "%s: '%s' contains invalid characters '/' or ':'\n",
		    argv[0], name);
	    exit(FSUR_INVAL);
	}
	fd = open(dev, O_RDWR, 0);
	if (fd <= 0) {
	    fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
		    strerror(errno));
	    exit(FSUR_UNRECOGNIZED);
	}
	if (read_superblock(fd, dev) == FALSE) {
	    exit(FSUR_UNRECOGNIZED);
	}
	if (ufslabel_set(fd, (char *)argv[3], strlen(argv[3])) == FALSE) {
	    fprintf(stderr, "%s: couldn't update the name\n", 
		    argv[0]);
	    exit(FSUR_IO_FAIL);
	}
	break;
    }
    default:
	break;
    }
    exit (FSUR_RECOGNIZED);
    return (0);
}
