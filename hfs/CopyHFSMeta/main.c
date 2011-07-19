#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <err.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/disk.h>
#include <sys/sysctl.h>
#include <hfs/hfs_mount.h>
#include "hfsmeta.h"
#include "Data.h"
#include "Sparse.h"

/*
 * Used to automatically run a corruption program after the
 * copying is done.  Only used during development.  Uncomment
 * to use.
 */
//#define TESTINJECT	1

static const char *kAppleInternal = "/AppleInternal";
static const char *kTestProgram = "HC-Inject-Errors";

int verbose;
int debug;
int printProgress;

/*
 * Exit status values.  We use some errno values because
 * they are convenient.
 * kGoodExit:  we finished copying, and no problems.
 * kNoSpaceExit:  Not enough space for the skeleton copy.
 * kCopyIOExit:  An I/O error occurred.  This may not be fatal,
 *	as it may just mean the source device went away.  We
 *	can continue later, perhaps.
 * kIntrExit:  The copying was interrupted.  As above, this may
 *	not be fatal.
 * kBadExit:  Any other problem.
 */
enum {
	kGoodExit = 0,
	kNoSpaceExit = ENOSPC,
	kCopyIOExit = EIO,
	kIntrExit = EINTR,
	kBadExit = 1,
};

/*
 * Open the source device.  In addition to opening the device,
 * this also attempts to flush the journal, and then sets up a
 * DeviceInfo_t object that will be used when doing the actual
 * reading.
 */
static DeviceInfo_t *
OpenDevice(const char *devname)
{
	char *rawname;
	DeviceInfo_t *retval = NULL;
	int fd;
	DeviceInfo_t dev = { 0 };
	struct stat sb;
	struct vfsconf vfc;

	if (stat(devname, &sb) == -1) {
		err(kBadExit, "cannot open device %s", devname);
	}
	/*
	 * We only allow a character device (e.g., /dev/rdisk1s2)
	 * If we're given a non-character device, we'll try to turn
	 * into a character device assuming a name pattern of /dev/rdisk*
	 */
	if ((sb.st_mode & S_IFMT) == S_IFCHR) {
		dev.devname = strdup(devname);
	} else if (strncmp(devname, "/dev/disk", 9) == 0) {
		// Turn "/dev/diskFoo" into "/dev/rdiskFoo"
		char tmpname[strlen(devname) + 2];
		(void)snprintf(tmpname, sizeof(tmpname), "/dev/rdisk%s", devname + sizeof("/dev/disk") - 1);
		if (stat(tmpname, &sb) == -1) {
			err(kBadExit, "cannot open raw device %s", tmpname);
		}
		if ((sb.st_mode & S_IFMT) != S_IFCHR) {
			errx(kBadExit, "raw device %s is not a raw device", tmpname);
		}
		dev.devname = strdup(tmpname);
	} else {
		errx(kBadExit, "device name `%s' does not fit pattern", devname);
	}
	// Only use an exclusive open if we're not debugging.
	fd = open(dev.devname, O_RDWR | (debug ? 0 : O_EXLOCK));
	if (fd == -1) {
		err(kBadExit, "cannot open raw device %s", dev.devname);
	}
	// Get the block size and counts for the device.
	if (ioctl(fd, DKIOCGETBLOCKSIZE, &dev.blockSize) == -1) {
		dev.blockSize = 512;	// Sane default, I hope
	}
	if (ioctl(fd, DKIOCGETBLOCKCOUNT, &dev.blockCount) == -1) {
		err(kBadExit, "cannot get size of device %s", dev.devname);
	}
	/*
	 * Attempt to flush the journal.  If it fails, we just warn, but don't abort.
	 */
	if (getvfsbyname("hfs", &vfc) == 0) {
		int rv;
		int mib[4];

		mib[0] = CTL_VFS;
		mib[1] = vfc.vfc_typenum;
		mib[2] = HFS_REPLAY_JOURNAL;
		mib[3] = fd;
		if (debug)
			fprintf(stderr, "about to replay journal\n");
		rv = sysctl(mib, 4, NULL, NULL, NULL, 0);
		if (rv == -1) {
			warn("cannot replay journal");
		}
		/* This is probably not necessary, but we couldn't prove it. */
		(void)fcntl(fd, F_FULLFSYNC, 0);
	}

	dev.size = dev.blockCount * dev.blockSize;
	dev.fd = fd;

	retval = malloc(sizeof(*retval));
	if (retval == NULL) {
		err(kBadExit, "cannot allocate device info structure");
	}
	*retval = dev;
	return retval;
}

/*
 * Get the header and alternate header for a device.
 */
VolumeDescriptor_t *
VolumeInfo(DeviceInfo_t *devp)
{
	uint8_t buffer[devp->blockSize];
	VolumeDescriptor_t *vdp = NULL, vd = { 0 };
	ssize_t rv;

	vd.priOffset = 1024;	// primary volume header is at 1024 bytes
	vd.altOffset = devp->size - 1024;	// alternate header is 1024 bytes from the end

	rv = GetBlock(devp, vd.priOffset, buffer);
	if (rv == -1) {
		err(kBadExit, "cannot get primary volume header for device %s", devp->devname);
	}
	vd.priHeader = *(HFSPlusVolumeHeader*)buffer;

	rv = GetBlock(devp, vd.altOffset, buffer);
	if (rv == -1) {
		err(kBadExit, "cannot get alternate volume header for device %s", devp->devname);
	}
	vd.altHeader = *(HFSPlusVolumeHeader*)buffer;

	vdp = malloc(sizeof(*vdp));
	*vdp = vd;

	return vdp;
}

/*
 * Compare two volume headers to see if they're the same.  Some fields
 * we may not care about, so we only compare specific fields.  Note that
 * since we're looking for equality, we don't need to byte swap.
 */
int
CompareVolumeHeaders(HFSPlusVolumeHeader *left, HFSPlusVolumeHeader *right)
{
	if (left->signature != right->signature ||
	    left->version != right->version ||
	    left->modifyDate != right->modifyDate ||
	    left->fileCount != right->fileCount ||
	    left->folderCount != right->folderCount ||
	    left->nextAllocation != right->nextAllocation ||
	    left->nextCatalogID != right->nextCatalogID ||
	    left->writeCount != right->writeCount)
		return 0;
	return 1;
}

/*
 * Only two (currently) types of signatures are valid: H+ and HX.
 */
static int
IsValidSigWord(uint16_t word) {
	if (word == kHFSPlusSigWord ||
	    word == kHFSXSigWord)
		return 1;
	return 0;
}

/*
 * Add the volume headers to the in-core volume information list.
 */
int
AddHeaders(VolumeObjects_t *vop)
{
	int retval = 1;
	HFSPlusVolumeHeader *hp;
	uint8_t buffer[vop->devp->blockSize];
	ssize_t rv;

	hp = &vop->vdp->priHeader;

	if (IsValidSigWord(S16(hp->signature)) == 0) {
		warnx("primary volume header signature = %x, invalid", S16(hp->signature));
		retval = 0;
	}
	AddExtent(vop, 1024, 512);

	hp = &vop->vdp->altHeader;

	if (IsValidSigWord(S16(hp->signature)) == 0) {
		warnx("alternate volume header signature = %x, invalid", S16(hp->signature));
		retval = 0;
	}
	AddExtent(vop, vop->vdp->altOffset, 512);

done:
	return retval;
}

/*
 * Add the journal information to the in-core volume list.
 * This means the journal info block, the journal itself, and
 * the contents of the same as described by the alternate volume
 * header (if it's different from the primary volume header).
 */
void
AddJournal(VolumeObjects_t *vop)
{
	DeviceInfo_t *devp = vop->devp;
	uint8_t buffer[devp->blockSize];
	ssize_t rv;
	HFSPlusVolumeHeader *php, *ahp;
	JournalInfoBlock *jib;

	php = &vop->vdp->priHeader;
	ahp = &vop->vdp->altHeader;

	if (php->journalInfoBlock) {
		off_t jOffset = (off_t)S32(php->journalInfoBlock) * S32(php->blockSize);
		rv = GetBlock(devp, jOffset, buffer);
		if (rv == -1) {
			err(kBadExit, "cannot get primary header's copy of journal info block");
		}
		AddExtent(vop, jOffset, sizeof(buffer));
		jib = (JournalInfoBlock*)buffer;
		if (S32(jib->flags) & kJIJournalInFSMask) {
			AddExtent(vop, S64(jib->offset), S64(jib->size));
		}
	}

	if (ahp->journalInfoBlock &&
	    ahp->journalInfoBlock != php->journalInfoBlock) {
		off_t jOffset = (off_t)S32(ahp->journalInfoBlock) * S32(ahp->blockSize);
		rv = GetBlock(devp, jOffset, buffer);
		if (rv == -1) {
			err(kBadExit, "cannot get alternate header's copy of journal info block");
		}
		AddExtent(vop, jOffset, sizeof(buffer));
		jib = (JournalInfoBlock*)buffer;
		if (S32(jib->flags) & kJIJournalInFSMask) {
			AddExtent(vop, S64(jib->offset), S64(jib->size));
		}
	}

}

/*
 * Add the extents for the special files in the volume header.  Compare
 * them with the alternate volume header's versions, and if they're different,
 * add that as well.
 */
void
AddFileExtents(VolumeObjects_t *vop)
{
	int useAlt = 0;
#define ADDEXTS(vop, file) \
	do { \
		off_t pSize = S32(vop->vdp->priHeader.blockSize);	\
		off_t aSize = S32(vop->vdp->altHeader.blockSize);	\
		int i;							\
		if (debug) printf("Adding " #file " extents\n");		\
		for (i = 0; i < kHFSPlusExtentDensity; i++) {		\
			HFSPlusExtentDescriptor *ep = &vop->vdp->priHeader. file .extents[i]; \
			HFSPlusExtentDescriptor *ap = &vop->vdp->altHeader. file .extents[i]; \
			if (debug) printf("\tExtent <%u, %u>\n", S32(ep->startBlock), S32(ep->blockCount)); \
			if (ep->startBlock && ep->blockCount) {		\
				AddExtent(vop, S32(ep->startBlock) * pSize, S32(ep->blockCount) * pSize); \
				if (memcmp(ep, ap, sizeof(*ep)) != 0) { \
					AddExtent(vop, S32(ap->startBlock) * aSize, S32(ap->blockCount) * aSize); \
					useAlt = 1;			\
				}					\
			}						\
		}							\
	} while (0)

	ADDEXTS(vop, allocationFile);
	ADDEXTS(vop, extentsFile);
	ADDEXTS(vop, catalogFile);
	ADDEXTS(vop, attributesFile);
	ADDEXTS(vop, startupFile);

#undef ADDEXTS

	ScanExtents(vop, 0);
	if (useAlt)
		ScanExtents(vop, useAlt);

	return;
}

static void
usage(const char *progname)
{

	errx(kBadExit, "usage: %s [-vdpS] [-g gatherFile] [-r <bytes>] <src device> <destination>", progname);
}


main(int ac, char **av)
{
	char *src = NULL;
	char *dst = NULL;
	DeviceInfo_t *devp = NULL;
	VolumeDescriptor_t *vdp = NULL;
	VolumeObjects_t *vop = NULL;
	IOWrapper_t *wrapper = NULL;
	int ch;
	off_t restart = 0;
	int printEstimate = 0;
	const char *progname = av[0];
	char *gather = NULL;
	int force = 0;
	int retval = kGoodExit;

	while ((ch = getopt(ac, av, "fvdg:Spr:")) != -1) {
		switch (ch) {
		case 'v':	verbose++; break;
		case 'd':	debug++; verbose++; break;
		case 'S':	printEstimate = 1; break;
		case 'p':	printProgress = 1; break;
		case 'r':	restart = strtoull(optarg, NULL, 0); break;
		case 'g':	gather = strdup(optarg); break;
		case 'f':	force = 1; break;
		default:	usage(progname);
		}
	}

	ac -= optind;
	av += optind;

	if (ac == 0 || ac > 2) {
		usage(progname);
	}
	src = av[0];
	if (ac == 2)
		dst = av[1];

	// Start by opening the input device
	devp = OpenDevice(src);
	if (devp == NULL) {
		errx(kBadExit, "cannot get device information for %s", src);
	}

	// Get the volume information.
	vdp = VolumeInfo(devp);

	// Start creating the in-core volume list
	vop = InitVolumeObject(devp, vdp);

	// Add the volume headers
	if (AddHeaders(vop) == 0) {
		errx(kBadExit, "Invalid volume header(s) for %s", src);
	}
	// Add the journal and file extents
	AddJournal(vop);
	AddFileExtents(vop);

	if (debug)
		PrintVolumeObject(vop);

	if (printEstimate) {
		printf("Estimate %llu\n", vop->byteCount);
	}

	// Create a gatherHFS-compatible file, if requested.
	if (gather) {
		WriteGatheredData(gather, vop);
	}

	/*
	 * If we're given a destination, initialize it.
 	 */
	if (dst) {
		wrapper = InitSparseBundle(dst, devp);
	}

	if (wrapper) {
		// See if we're picking up from a previous copy
		if (restart == 0) {
			restart = wrapper->getprog(wrapper);
			if (debug) {
				fprintf(stderr, "auto-restarting at offset %lld\n", restart);
			}
		}
		// "force" in this case means try even if the space estimate says we won't succeed.
		if (force == 0) {
			struct statfs sfs;
			if (statfs(dst, &sfs) != -1) {
				off_t freeSpace = (off_t)sfs.f_bsize * (off_t)sfs.f_bfree;
				if (freeSpace < (vop->byteCount - restart)) {
					errx(kNoSpaceExit, "free space (%lld) < required space (%lld)", freeSpace, vop->byteCount - restart);
				}
			}
		}

		/*
		 * If we're restarting, we need to compare the volume headers and see if
		 * they're the same.  If they're not, we need to start from the beginning.
		 */
		if (restart) {
			HFSPlusVolumeHeader priHeader, altHeader;

			if (wrapper->reader(wrapper, 1024, &priHeader, sizeof(priHeader)) != -1) {
				if (CompareVolumeHeaders(&priHeader, &vop->vdp->priHeader) == 0) {
					restart = 0;
				} else {
					if (wrapper->reader(wrapper, vop->vdp->altOffset, &altHeader, sizeof(altHeader)) != -1) {
						if (CompareVolumeHeaders(&altHeader, &vop->vdp->altHeader) == 0) {
							restart = 0;
						}
					}
				}
			}
			if (restart == 0) {
				if (verbose)
					warnx("Destination volume does not match source, starting from beginning");
			}
		}

		// And start copying the objects.
		if (CopyObjectsToDest(vop, wrapper, restart) == -1) {
			if (errno == EIO)
				retval = kCopyIOExit;
			else if (errno == EINTR)
				retval = kIntrExit;
			else
				retval = kBadExit;
			err(retval, "CopyObjectsToDest failed");
		} else {
#if TESTINJECT
			// Copy finished, let's see if we should run a test program
			if (access(kAppleInternal, 0) != -1) {
				char *home = getenv("HOME");
				if (home) {
					char *pName;
					pName = malloc(strlen(home) + strlen(kTestProgram) + 2);	// '/' and NUL
					if (pName) {
						sprintf(pName, "%s/%s", home, kTestProgram);
						execl(pName, kTestProgram, dst, NULL);
					}
				}
			}
#endif
		}
	}

	return retval;
}

