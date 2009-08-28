/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include <sys/loadable_fs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h> 
#include <sys/mount.h>
#include <sys/param.h>

#include <servers/netname.h>

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 

#include <ufs/ufs/dinode.h>
#include <ufs/ufs/dir.h>
#include <ufs/ffs/fs.h>

/*
 * CommonCrypto is meant to be a more stable API than OpenSSL.
 * Defining COMMON_DIGEST_FOR_OPENSSL gives API-compatibility
 * with OpenSSL, so we don't have to change the code.
 */
#define COMMON_DIGEST_FOR_OPENSSL
#include <CommonCrypto/CommonDigest.h>

#include <System/uuid/uuid.h>
#include <System/uuid/namespace.h>

#include "ufslabel.h"

#define UFS_FS_NAME		"ufs"
#define UFS_FS_NAME_FILE	"UFS"
#define MOUNT_COMMAND "/sbin/mount"

static void uuid_create_md5_from_name(uuid_t result_uuid, const uuid_t namespace, const void *name, int namelen);

static void 
usage(const char * progname)
{
    fprintf(stderr, "usage: %s [-m mountflag1 mountflag2 mountflag3 mountflag4] device node\n", progname);
    fprintf(stderr, "       %s [-p mountflag1 mountflag2 mountflag3 mountflag4] device\n", progname);
    fprintf(stderr, "       %s [-ksu] device\n", progname);
    fprintf(stderr, "       %s [-n] device name\n", progname);

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
	char dev[512];
	char opt;
	struct stat	sb;
	int devopt = 0;

	if (argc < 3 || argv[1][0] != '-') {
		usage(argv[0]);
		exit(FSUR_INVAL);
	}
    opt = argv[1][1];
    
	if ((opt != FSUC_PROBE) && 
		(opt != FSUC_MOUNT) &&
		(opt != 's') &&
		(opt != 'k') &&
		(opt != 'n')) {
			usage(argv[0]);
			exit(FSUR_INVAL);
	}
	
	/* device node should be the 3rd arg */
	devopt = 2;

	snprintf(dev, sizeof(dev), "/dev/r%s", argv[devopt]);
	if (stat(dev, &sb) != 0) {
		fprintf(stderr, "%s: stat %s failed, %s\n", argv[0], dev,
				strerror(errno));
		exit(FSUR_INVAL);
	}

	switch (opt) {
		case FSUC_MOUNT:
			if (argc < 4) {
				usage(argv[0]);
				exit(FSUR_INVAL);
			}
			else {
				/*
				 * The mount_args are, at a minimum:  
				 * /sbin/mount, -t, ufs, /dev/<diskname>, mountpoint, NULL.
				 * So that's 5 + 1 (for the NULL).  Each mount option is an additional two.
				 */
				const char *mount_args[5 + 2 * (argc - 4) + 1];
				const char *mount_point = argv[devopt+1];
				const char *device = argv[devopt];
				const char **map = mount_args;
				int i;

				*map++ = MOUNT_COMMAND;
				*map++ = "-t";
				*map++ = UFS_FS_NAME;

				for (i = 3; i < argc - 1; i++) {
					*map++ = "-o";
					*map++ = argv[i];
				}
				snprintf(dev, sizeof(dev), "/dev/%s", device);
				*map++ = dev;
				*map++ = mount_point;
				*map++ = 0;

#ifdef DEBUG
				printf("execv(");
				for (map = mount_args; *map; map++) {
					printf("`%s', ", *map);
				}
				printf("NULL)\n");
#endif
				execv(MOUNT_COMMAND, (char* const*) mount_args);
				exit(FSUR_UNRECOGNIZED);
			}
			break;

	case FSUC_PROBE: 
			{
				FILE *		f;
				char		filename[MAXPATHLEN + 1];
				int 		fd;
				int		len;
				u_char		name[UFS_MAX_LABEL_NAME + 1];
				struct ufslabel	ul;

				snprintf(filename, sizeof(filename), "%s/ufs%s/ufs.label", FS_DIR_LOCATION,
						FS_DIR_SUFFIX);
				unlink(filename);
				snprintf(filename, sizeof(filename), "%s/ufs%s/ufs.name", FS_DIR_LOCATION,
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
				if (ufslabel_get(fd, &ul) == FALSE) {
					exit(FSUR_RECOGNIZED);
				}
				ufslabel_get_name(&ul, (char *)name, &len);
				name[len] = '\0';
				close(fd);

				/* write the ufs.label file */
				snprintf(filename, sizeof(filename), "%s/ufs%s/ufs" FS_LABEL_SUFFIX, FS_DIR_LOCATION,
						FS_DIR_SUFFIX);
				f = fopen(filename, "w");
				if (f != NULL) {
					fprintf(f, "%s", name);
					fclose(f);
				}

				/* dump the name to stdout */
				write(1, name, strlen((char *)name));

				/* write the ufs.name file */
				snprintf(filename, sizeof(filename), "%s/ufs%s/ufs.name", FS_DIR_LOCATION,
						FS_DIR_SUFFIX);
				f = fopen(filename, "w");
				if (f != NULL) {
					fprintf(f, UFS_FS_NAME_FILE);
					fclose(f);
				}
				break;
			}
		case 'n': 
			{
				int		fd;
				char *		name;
				struct ufslabel ul;

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
				if(ufslabel_get(fd, &ul) == FALSE)
					ufslabel_init(&ul);
				if (ufslabel_set_name(&ul, (char *)argv[3], strlen(argv[3])) == FALSE) {
					fprintf(stderr, "%s: couldn't update the name\n",
							argv[0]);
					exit(FSUR_IO_FAIL);
				}
				if (ufslabel_set(fd, &ul) == FALSE) {
					fprintf(stderr, "%s: couldn't update the name\n",
							argv[0]);
					exit(FSUR_IO_FAIL);
				}
				break;
			}
		case 's': 
			{
				int		fd;
				struct ufslabel ul;

				if (argc < 3) {
					usage(argv[0]);
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
				if(ufslabel_get(fd, &ul) == FALSE)
					ufslabel_init(&ul);
				ufslabel_set_uuid(&ul);
				if (ufslabel_set(fd, &ul) == FALSE) {
					fprintf(stderr, "%s: couldn't update the uuid\n",
							argv[0]);
					exit(FSUR_IO_FAIL);
				}

				exit (FSUR_IO_SUCCESS);
				break;
			}
		case 'k': 
			{
				int 		fd;
				char		uuid[UFS_MAX_LABEL_UUID + 1];
				struct ufslabel	ul;
				uuid_t		newuuid;
				char		uuidline[40];
				char		name[8];

				fd = open(dev, O_RDONLY, 0);
				if (fd <= 0) {
					fprintf(stderr, "%s: open %s failed, %s\n", argv[0], dev,
							strerror(errno));
					exit(FSUR_UNRECOGNIZED);
				}
				if (read_superblock(fd, dev) == FALSE) {
					exit(FSUR_UNRECOGNIZED);
				}
				if (ufslabel_get(fd, &ul) == FALSE) {
					fprintf(stderr, "%s: couldn't read the uuid\n",
							argv[0]);
					exit(FSUR_IO_FAIL);
				}
				close(fd);
				ufslabel_get_uuid(&ul, uuid);
				sscanf(uuid, "%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
						&name[0], &name[1], &name[2], &name[3],
						&name[4], &name[5], &name[6], &name[7]);
				uuid_create_md5_from_name(newuuid, kFSUUIDNamespaceSHA1, &name, 8);
				uuid_unparse(newuuid, uuidline);

				/* dump the uuid to stdout */
				write(1, uuidline, strlen(uuidline));

				exit (FSUR_IO_SUCCESS);
				break;
			}
		default:
			break;
	}
	exit (FSUR_RECOGNIZED);
    return (0);
}

static void
uuid_create_md5_from_name(uuid_t result_uuid, const uuid_t namespace, const void *name, int namelen)
{
	MD5_CTX c;

	MD5_Init(&c);
	MD5_Update(&c, namespace, sizeof(uuid_t));
	MD5_Update(&c, name, namelen);
	MD5_Final(result_uuid, &c);

	result_uuid[6] = (result_uuid[6] & 0x0F) | 0x30;
	result_uuid[8] = (result_uuid[8] & 0x3F) | 0x80;
}

