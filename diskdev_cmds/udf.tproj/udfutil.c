
/* 
 * Copyright 2000 Apple Computer, Inc.
 *
 * udfutil.c
 * - probes for the existence of UDF and return a name
 */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h> 
#include <string.h> 
#include <stdlib.h> 

#include <sys/loadable_fs.h>
#include <sys/wait.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/mount.h>

#include <dev/disk.h>

#include <machine/byte_order.h>
#include <CoreFoundation/CFString.h>

#define fromLE16 NXSwapLittleShortToHost
#define fromLE32 NXSwapLittleIntToHost

#define FS_TYPE			"udf"
#define FS_NAME_FILE		"UDF"
#define DEV_PREFIX		"/dev/"
#define RAWDEV_PREFIX		"/dev/r"
#define MOUNT_COMMAND		"/sbin/mount"
#define UMOUNT_COMMAND		"/sbin/umount"
#define KEXTLOAD_COMMAND	"/sbin/kextload"
#define FS_KEXT_DIR		"/System/Library/Extensions/udf.kext"
#define ISO_BLOCKSIZE		2048
#define VOL_ID_OFFSET 		24
#define VOL_ID_LENGTH		32
#define OSTA_COMPRESSED		8
#define OSTA_UNCOMPRESSED	16

/*
 * Unicode to UTF-8 conversion determines the MAXLABEL size here. To figure out
 * just what that value is we used the following information:
 *
 * Typically one of the decomposition chars is basic ASCII so it takes up 1
 * byte in UTF-8.
 *
 * The worst case won't happen in practice.  For the languages we support
 * (Tier 1 and Tier 2) the worst cases are shown below.
 *
 * The worst case Latin expansion for a single Unicode char would be...
 *     Encoding     Size                       Example
 *     ===============================================
 *     Unicode:     1 char (2 bytes)           0x01D7
 *     Decomposed:  3 chars (6 bytes)          0x0055 0x0308 0x0301
 *     UTF-8:       5 bytes (1 + 2 + 2)        0x55 0xCC 0x88 0xCC 0x81
 * 
 * The worst case Japanese expansion for a single Unicode char would be...
 *     Unicode:     1 char (2 bytes)           0x30D7
 *     Decomposed:  2 chars (4 bytes)          0x30D5 0x309A
 *     UTF-8:       6 bytes (3 + 3)            0xDY 0x8Y 0x8Y 0xDY 0x8Y 0x8Y
 *
 * The worst case Hanguk expansion for a single Unicode char would be ..
 *     Unicode:     1 char  (2 bytes)          0xAC01
 *     Decomposed:  3 chars (6 bytes)          0x1100 0x1161 0x11A8
 *     UTF-8:       9 bytes                    0xDY 0x8Y 0x8Y 0xDY 0x8Y 0x8Y 0xDY 0x8Y 0x8Y
 *
 *     So the actual worst case is (# Unicode chars) * 9
 *
 * The volume label on a UDF disk is stored in a 32 byte DString. This leaves
 * space for 15 unicode characters ((32 - length byte - compression byte)/2).
 *
 * So MAXLABEL needs to be 15 * 9 to take care of the worst case possible,
 * plus one additional byte for the null terminator.
 *
 */
 #define MAX_LABEL		136

/* globals */
const char	*progname;	/* our program name, from argv[0] */
int	debug;	/* use -D to enable debug printfs */

static void 
usage()
{
	fprintf(stderr, "usage: %s action_arg device_arg [mount_point_arg]\n",
		progname);
	fprintf(stderr, "action_arg:\n");
	fprintf(stderr, "	-%c (Probe)\n", FSUC_PROBE);
	fprintf(stderr, "	-%c (Mount)\n", FSUC_MOUNT);
	fprintf(stderr, "	-%c (Unmount)\n", FSUC_UNMOUNT);
	fprintf(stderr, "Example:\n");
	fprintf(stderr, "	%s.util -%c disk2 /mnt\n", FS_TYPE, FSUC_MOUNT);
	exit(FSUR_INVAL);
}


int
safe_open(char *path, int flags, mode_t mode)
{
	int fd = open(path, flags, mode);

	if (fd < 0) {
		fprintf(stderr, "%s: open %s failed, %s\n", progname, path,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
	return(fd);
}


void
safe_close(int fd)
{
	if (close(fd)) {
		fprintf(stderr, "%s: close failed, %s\n", progname,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
}


void
safe_write(int fd, char *data, int len)
{
	if (write(fd, data, len) != len) {
		fprintf(stderr, "%s: write failed, %s\n", progname,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
}


void
safe_execv(char *args[])
{
	int		pid;
	union wait	status;

	pid = fork();
	if (pid == 0) {
		(void)execv(args[0], args);
		fprintf(stderr, "%s: execv %s failed, %s\n", progname, args[0],
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
	if (pid == -1) {
		fprintf(stderr, "%s: fork failed, %s\n", progname,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
	if (wait4(pid, (int *)&status, 0, NULL) != pid) {
		fprintf(stderr, "%s: BUG executing %s command\n", progname,
			args[0]);
		exit(FSUR_IO_FAIL);
	} else if (!WIFEXITED(status)) {
		fprintf(stderr, "%s: %s command aborted by signal %d\n",
			progname, args[0], WTERMSIG(status));
		exit(FSUR_IO_FAIL);
	} else if (WEXITSTATUS(status)) {
		fprintf(stderr, "%s: %s command failed, exit status %d: %s\n",
			progname, args[0], WEXITSTATUS(status),
			strerror(WEXITSTATUS(status)));
		exit(FSUR_IO_FAIL);
	}
}


void
safe_unlink(char *path)
{
	if (unlink(path) && errno != ENOENT) {
		fprintf(stderr, "%s: unlink %s failed, %s\n", progname, path,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
}


void
safe_read(int fd, char *buf, int nbytes, off_t off)
{
	if (lseek(fd, off, SEEK_SET) == -1) {
		fprintf(stderr, "%s: device seek error @ %qu, %s\n", progname,
			off, strerror(errno));
		exit(FSUR_IO_FAIL);
	}
	if (read(fd, buf, nbytes) != nbytes) {
		if (debug)
			fprintf(stderr, "%s: device read error @ %qu, %s\n",
				progname, off, strerror(errno));
		exit(FSUR_IO_FAIL);
	}
}


int
bsum(unsigned char *s, unsigned len)
{	/* sum a series of bytes */
	int	sum = 0;

	while (len--)
		sum += *s++;
	return(sum);
}


/*
 * skip over any illegal characters in the string adjusting the name 
 * pointer and length of the string as you go.
 */
void
consumeIllegal( char **name, int * maxLength )
{
	char *s = *name;
	int len = *maxLength;
	
	while ( len ) {
		if ((*s != '/') && (*s != '\0'))
			break;
		s++; len--;
	}
	*name = s; *maxLength = len;
}


/*
 * Check to see if the name byte of the DString passed in is correct
 * Some of the early disks we have seen had a zero in the length field
 * If we find a zero length, we do a backward crawl to see where the 
 * name actually ends and return that value.
 */
int
validateLength( char *bytes, int fieldLen )
{
	char *end = &bytes[fieldLen - 1];
	int length = *end;
	
	
	if ( length == 0 ) {
		bytes++;	// Skip over the OSTA compression byte
		while ( end != bytes ) {
			if (*end != '\0') break;
			end--;
		}
		length = end + 1 - bytes;
	} 
	else length -= 1; 	// remove OSTA compression byte from length
	
	return length;
}



int
label_get(int fd, unsigned sectorsize, unsigned sector, char *label)
{
	unsigned char	*buf = malloc(sectorsize);
	int	found = 0;
	CFStringRef volumeName;
	char *defaultName = "Unknown";
	char *src, *dst;
	char theChar;
	int namelength;

	if (debug) printf("looking for AVDP @ sector %d\n", sector);

	if (!buf) {
		fprintf(stderr, "%s: malloc %d failed\n", progname, sectorsize);
		exit(FSUR_IO_FAIL);
	}
	safe_read(fd, buf, sectorsize, (off_t)(sector * sectorsize));
	if (debug) {	/* dump out "descriptor tag" */
		unsigned char *bp = buf;
		while (bp < buf+16)
			printf("%.2x ", *bp++);
		printf("\n");
	}
	/*
	 * ensure descriptor tag is for AVDP and
	 * verify descriptor tag checksum
	 * per ECMA-167 3/7.2
	 */
	if (fromLE16(*(unsigned short *)buf) == 2 &&
	    buf[4] == (bsum(buf, 4) + bsum(buf+5, 11)) % 256) {
		/* get VDS length (bytes) and location (sector#) */
		unsigned len = fromLE32(*(unsigned *)(buf+16));
		unsigned loc = fromLE32(*(unsigned *)(buf+20));

		if (debug) printf("VDS %d long at sector %d\n", len, loc);

		for (; len >= sectorsize; len -= sectorsize, loc++) {
			safe_read(fd, buf, sectorsize,
				  (off_t)(loc * sectorsize));
			if (fromLE16(*(unsigned short *)buf) != 1)
				continue;
			/*
			 * XXX CSM what about charset?
			 * charset of label is buf[24], and
			 * defined in ECMA-167 1/7.2.1.1
			 */
			if (debug) {	/* dump out volume identifier */
				unsigned char *bp = buf+24;
				printf("Volume id, hex (buf[24]-buf[55])\n\t");
				while (bp < buf+56)
					printf("%.2x ", *bp++);
				printf("\n");
				printf("Volume id, interpreted:\n\t");
				bp = buf+24;
				printf("CS:%d STR:", *bp++);
				while (bp < buf+55)
					printf("%c", *bp++);
				printf(" LEN:%d\n", *bp);
			}
			/*
			 * The following would just be:
			 *	memcpy(label, buf+25, 30)
			 * except that we need to collapse out internal
			 * illegal characters null and slash.  They
			 * are collapsed to underscores for consistency
			 * with UDF filename mangling rules for unix.
			 * Note that if we don't do something like this
			 * then autodiskmount's mkdir will fail.
			 * Also note we don't trust the length byte.
			 */

			namelength = validateLength( &buf[VOL_ID_OFFSET], VOL_ID_LENGTH ); // Get the number of bytes in Volume Identifier
		
			/*
			 * Now check to see if the label is unicode.
			 */
			if ( buf[VOL_ID_OFFSET] == OSTA_UNCOMPRESSED ) {
				volumeName = CFStringCreateWithCharacters(NULL, (UniChar*) &buf[VOL_ID_OFFSET+1], namelength/2 );
				if ( volumeName != NULL ) {
					(void) CFStringGetCString(volumeName, label, MAX_LABEL-1, kCFStringEncodingUTF8);
					CFRelease(volumeName);
					namelength = strlen(label);
				}
			}
			else if ( buf[VOL_ID_OFFSET] == OSTA_COMPRESSED ) {
				memcpy(label, &buf[VOL_ID_OFFSET+1], namelength);
				label[namelength] = '\0';
			}
			else {
				namelength = strlen(defaultName);
				strcpy(label, defaultName);
			}

			// Now replace runs of '/' or '\0' with a single '_'
			src = dst = label;
			while ( namelength-- > 0 ) {
				theChar = *src++;
				if ( (theChar == '/') || (theChar == '\0') ) {
					consumeIllegal(&src, &namelength);
					theChar = '_';
				}
				*(dst++) = theChar;
			}
			*dst = '\0';
			found = 1;
			break;
		}
	}
	free(buf);
	return(found);
}


void
safe_ioctl(int fd, unsigned req, char *argp, char *reqstr)
{
	if (ioctl(fd, req, argp) < 0) {
		fprintf(stderr, "%s: %s failed, %s\n", progname, reqstr,
			strerror(errno));
		exit(FSUR_IO_FAIL);
	}
}


int 
main(int argc, const char *argv[])
{
	char		devpath[MAXPATHLEN];
	char		opt;
	struct stat	sb;
	int		ret = FSUR_INVAL;

	/* save & strip off program name */
	progname = argv[0];
	argc--;
	argv++;

	/* secret debug flag - must be 1st flag */
	debug = (argc > 0 && !strcmp(argv[0], "-D"));
	if (debug) { /* strip off debug flag argument */
		argc--;
		argv++;
	}

	if (argc < 2 || argv[0][0] != '-')
		usage();
	opt = argv[0][1];
	if (opt != FSUC_PROBE && opt != FSUC_MOUNT && opt != FSUC_UNMOUNT)
		usage();
	if ((opt == FSUC_MOUNT || opt == FSUC_UNMOUNT) && argc < 3) 
		usage(); /* mountpoint arg missing! */

	sprintf(devpath, "%s%s", RAWDEV_PREFIX, argv[1]);
	if (stat(devpath, &sb) != 0) {
		fprintf(stderr, "%s: stat %s failed, %s\n", progname, devpath,
			strerror(errno));
		exit(FSUR_INVAL);
	}
	switch (opt) {
	case FSUC_PROBE: {
		unsigned char	filename[MAXPATHLEN];
		int 		fd, n;
		unsigned char	label[MAX_LABEL];
		unsigned char	buf[ISO_BLOCKSIZE];
		unsigned	numsectors, sectorsize;

		sprintf(filename, "%s/%s%s/%s.label", FS_DIR_LOCATION,
			FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
		safe_unlink(filename);
		sprintf(filename, "%s/%s%s/%s.name", FS_DIR_LOCATION,
			FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
		safe_unlink(filename);

		fd = safe_open(devpath, O_RDONLY, 0);
		/* scan for ISO NSR02 or NSR03 descriptor */
		for (n = 16; n < 32; n++) {
			safe_read(fd, buf, ISO_BLOCKSIZE,
				  (off_t)(n * ISO_BLOCKSIZE));
			if (buf[0] == '\0' && !memcmp(&buf[1], "NSR", 3))
				break;
		}
		if (n >= 32)
			exit(FSUR_UNRECOGNIZED);

		if (debug) printf("found NSR @ sector %d\n", n);

		safe_ioctl(fd, DKIOCBLKSIZE, (char *)&sectorsize,
			   "DKIOCBLKSIZE");
		safe_ioctl(fd, DKIOCNUMBLKS, (char *)&numsectors,
			   "DKIOCNUMBLKS");
		if (debug) printf("sectorsize = %d, numsectors = %d\n",
				  sectorsize, numsectors);

                /* We try the native values first, if that fails we assume the	*/
                /* volume was created as a 2K logical sector size on a device	*/
                /* that had a sector size other than 2K and we retry.		*/

                if (!label_get(fd, sectorsize, 256, label) &&
                    !label_get(fd, sectorsize, numsectors-256, label) &&
                    !label_get(fd, sectorsize, numsectors-1, label)) {
                        fprintf(stderr, "%s: no volume descriptor found using native %d byte sector size.\n",
                                progname, sectorsize);
				
			if (sectorsize != ISO_BLOCKSIZE) {
                                fprintf(stderr, "%s: trying %d logical sector size.\n",
                                        progname, ISO_BLOCKSIZE);
                                numsectors =  ((off_t)(sectorsize * numsectors)) / ISO_BLOCKSIZE;
                                sectorsize = ISO_BLOCKSIZE;

                                if (!label_get(fd, sectorsize, 256, label) &&
                                    !label_get(fd, sectorsize, numsectors-256, label) &&
                                    !label_get(fd, sectorsize, numsectors-1, label)) {
                                        fprintf (stderr, "%s: no volume descriptor found using simulated sector size!\n",
                                                progname);
                                        exit(FSUR_UNRECOGNIZED);
                                    }

                        } else exit(FSUR_UNRECOGNIZED);
		}
		safe_close(fd);

		/* write the .label file */
		sprintf(filename, "%s/%s%s/%s.label", FS_DIR_LOCATION,
			FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
		fd = safe_open(filename, O_WRONLY|O_CREAT|O_EXCL, 0755);
		safe_write(fd, label, strlen(label) + 1);
		safe_close(fd);
	
		/* write the .name file */
		sprintf(filename, "%s/%s%s/%s.name", FS_DIR_LOCATION,
			FS_TYPE, FS_DIR_SUFFIX, FS_TYPE);
		fd = safe_open(filename, O_WRONLY|O_CREAT|O_EXCL, 0755);
		safe_write(fd, FS_NAME_FILE, 1 + strlen(FS_NAME_FILE));
		safe_close(fd);
		ret = FSUR_RECOGNIZED;
		break;
	}
	
	case FSUC_MOUNT: {
		const char *kextargs[] = {KEXTLOAD_COMMAND, FS_KEXT_DIR, NULL};
		const char *mountargs[11];

		mountargs[0] = MOUNT_COMMAND;
		mountargs[1] = "-t";
		mountargs[2] = FS_TYPE;
		mountargs[3] = "-r";
		if (argc >= 4 && !strcmp(argv[4], DEVICE_WRITABLE))
			mountargs[3] = "-w";

                //  UDF should not be mounted setuid
                mountargs[4] = "-o";
                mountargs[5] = "nosuid";

                //  UDF should not be mounted dev
                mountargs[6] = "-o";
                mountargs[7] = "nodev";

                mountargs[8] = devpath;
                mountargs[9] = argv[2];
                mountargs[10] = NULL;

		sprintf(devpath, "%s%s", DEV_PREFIX, argv[1]);
		safe_execv(kextargs); /* better here than in mount_udf */
		safe_execv(mountargs);
		ret = FSUR_IO_SUCCESS;
		break;
	}

	case FSUC_UNMOUNT: {
		const char *umountargs[] = {UMOUNT_COMMAND, argv[2], NULL};

		safe_execv(umountargs);
		ret = FSUR_IO_SUCCESS;
		break;
	}
				
	default: /* argument validation should preclude getting here */
		exit(FSUR_INVAL);
		break;
	}
	exit(ret);
	return(ret);
}
