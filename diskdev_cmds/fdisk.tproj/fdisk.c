/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
// fdisk.c
// divide a disk into DOS partitions
// created Mar 18, 1993 by sam streeper (sam)
// updated Feb 18, 1999 by dan markarian (markaria)
//   DKIOCINFO deprecated; using DKIOCBLKSIZE for device block size instead.


#define DRIVER_PRIVATE

#include <IOKit/storage/IOFDiskPartitionScheme.h>
#include <bsd/dev/disk.h>
#include <bsd/sys/fcntl.h>
#include <bsd/sys/file.h>
#include <stdlib.h>
#include <strings.h>
#include <stdio.h>
#include <unistd.h>
#include <stdio.h>
#include <machdep/i386/kernBootStruct.h>
#include <ctype.h>


int devblklen;
int fd;
int devNumBlks;
int diskSize;
int done;
int interactive = 1;
int useAllSectors;
int useBoot0;
int bootsectorOnly;
int megsForDos;
char *deviceName = "none";

KERNBOOTSTRUCT kernbootstruct;
struct disk_blk0 bootsector;
int heads, spt, spc, cylinders;

#define NODES 10

#define HEAD(val) (((val) % spc) / spt)
#define SECT(val) (((val % spt) + 1) | ((((val) / spc) >> 2) & 0xC0))
#define CYL(val) (((val) / spc) & 0xFF)

typedef struct {
	int start;
	int size;
	unsigned char active;
	unsigned char ident;
	int newPart;
	} block_info;

block_info zalloced[NODES];
block_info zavailable[NODES];
short availableNodes, allocedNodes;
static int zalloc_base;


void		activateUFS();
int	 	blk2meg(int x);
void		cls();
void		create_partition();
void		delete_partition();
void		eatkeys();
void		examineArgs(int argc, char *argv[]);
void 		doActionOrInquiry();
int 		diskPartitioned();
unsigned char 	getSysId();
int		idExists(int val);
int		indexOfActive();
void 		iprintf();
void		main_prompt();
int		maxFreeBlocks();
void 		readPartitionTable();
int		save_changes();
void		select_active_partition();
void		showDiskUsage(int enumerateEntries);
void		showDiskInfo();
int		zalloc(int size, unsigned char ident);
void	 	zallocate(int start, int size, unsigned char ident, 
			  unsigned char active, int newPart);
int	 	zappropriateFromFree(int start, int size);
void		zcoalesce();
void		zdelete(block_info *zp, int ndx);
void	 	zinit(int size);
void		zinsert(block_info *zp, int ndx);
int		zfree(int start);

void
usage()
{
	fprintf(stderr, "usage: fdisk <raw-device> [inquiry] [action] [flags]\n");
	fprintf(stderr, "raw-device is /dev/rdisk0 for whole disk\n");
	fprintf(stderr, "There can be no more than one inquiry or action; if none\n");
	fprintf(stderr, "is requested, fdisk is run in interactive mode.\n");
	fprintf(stderr, "--------  Inquiries  --------\n");
	fprintf(stderr, 
	"-isDiskPartitioned\n"
//	"-isThereExtendedPartition\n"
	"-isThereUFSPartition\n");
	fprintf(stderr, 
	"-freeSpace		(megabytes available)\n"
	"-freeWithoutUFS	(megs available if UFS partition deleted)\n"
	"-freeWithoutUFSorExt	(megs available if UFS and extended deleted)\n"
	"-sizeofExtended\n");
	fprintf(stderr, 
	"-diskSize\n"
	"-installSize\n");

	fprintf(stderr, "--------  Actions  --------\n");
	fprintf(stderr, 
	"-removePartitioning		(zero out bootsector)\n"
	"-dosPlusUFS <megsForDos>	(repartition entire disk for DOS plus UFS)\n"
	"-setAvailableToUFS		(reserve all free space for UFS)\n");

	fprintf(stderr, 
//	"-setExtAndAvailableToUFS	(delete current extended partition, then\n"
//	"				 reserve all free space for UFS)\n"
	"-setExtendedToUFS		(Change extended partition to UFS)\n"
	"-setUFSActive			(Make the UFS partition active)\n");

	fprintf(stderr, "--------  Flags  --------\n");
	fprintf(stderr, 
	"-useAllSectors		(don't limit disk to bios accessible sectors)\n"
	"-useBoot0		(use /usr/standalone/i386/boot0 as the boot program)\n");
	fprintf(stderr, 
	"-bootsectorOnly		(modify only the bootsector)\n");

	exit(-1);
}

void
bomb(s1, a1)
	char *s1;
	char *a1;
{
	fprintf(stderr,s1,a1);
	exit(-1);
}

static char *devs[1] = {
	"/dev/rdisk0"
	};

void
main(int argc, char *argv[])
{
	int kmfd, tfd;
	int diskInfo, ndx = 0;
	int i;
	char *cp = argv[1];

	if (argc < 2)
	{
		for (i=0; i<1; i++)
		{
			cp = devs[i];
			if ((fd = open (cp, O_RDWR)) >= 0)
			{
				close(fd);
				goto fbegin;
			}
		}
		bomb("fdisk: unable to access IDE or SCSI drive\n"
			"    (Must be run as root)\n");
	}
	else if (strncmp(cp, "/dev/rdisk", 10) || (cp[11] != '\0'))
	{
		usage();
	}

fbegin:
	deviceName = cp;

	examineArgs(argc, argv);

	iprintf("fdisk v1.02\n");

	if ((fd = open (cp, O_RDWR)) < 0)
	{
		bomb("fdisk: unable to open %s\n",cp);
	}

	if (ioctl (fd, DKIOCBLKSIZE, &devblklen) < 0)
	{
		bomb("fdisk: unable to get disk block size\n");
	}

	if (devblklen != 512)
	{
		bomb("fdisk: DOS partitioning requires 512 byte block size\n");
	}

	if ((kmfd = open ("/dev/kmem", O_RDONLY)) < 0)
	{
		bomb("fdisk: can't get kernel boot structure\n");
	}

	lseek(kmfd, (off_t)KERNSTRUCT_ADDR, L_SET);
	read(kmfd, &kernbootstruct, sizeof(KERNBOOTSTRUCT)-CONFIG_SIZE);
	if (kernbootstruct.magicCookie != KERNBOOTMAGIC)
	{
		bomb("fdisk: kernbootstruct invalid\n");
	}

	ndx += cp[10] - '0';
	if (ndx < 0 || ndx > 3)
	{
		bomb("fdisk: no bios info for this device\n");
	}

	diskInfo = kernbootstruct.diskInfo[ndx];
	heads = ((diskInfo >> 8) & 0xff) + 1;
	spt = diskInfo & 0xff;
	spc = spt * heads;
	cylinders = (diskInfo >> 16) + 1;

	if (heads == 1 || spt == 0)
	{
		bomb("fdisk: Bogus disk information in BIOS.\n"
		    "    You probably need to check your SCSI or IDE card setup to make\n"
		    "    sure that the BIOS is enabled.  If the BIOS is disabled,\n"
		    "    UFS will be unable to get proper disk information.\n");
	}

	if (ioctl(fd, DKIOCNUMBLKS, &devNumBlks) < 0)
	{
		bomb("fdisk: no bios info for this device\n");
	}

	// read the current bootsector
	read(fd, &bootsector, sizeof(struct disk_blk0));

	// if no signature, we'll read in boot0 to use as the boot code
	if (bootsector.signature != DISK_SIGNATURE || useBoot0 || (!diskPartitioned()))
	{
		char *tcp = "/usr/standalone/i386/boot0";
		if ((tfd = open (tcp, O_RDONLY)) < 0)
		{
			bomb("fdisk: can't read %s\n",tcp);
		}

		if (bootsector.signature == DISK_SIGNATURE && diskPartitioned())
		{
			read(tfd, &bootsector, DISK_BOOTSZ);
		}
		else
		{
			read(tfd, &bootsector, DISK_BLK0SZ);
		}

		close(tfd);
		bootsector.signature = DISK_SIGNATURE;
	}

	if ((cylinders * spc) > devNumBlks)
	{
		fprintf(stderr, "fdisk: bios reports more sectors (%d) than device (%d)\n",
			(cylinders * spc), devNumBlks);
		exit(-1);
	}

	if (useAllSectors) diskSize = devNumBlks;
	else diskSize = (cylinders * spc);

	// initialize allocator
	zinit(diskSize);
	readPartitionTable();

	if (!interactive)
	{
		doActionOrInquiry();
		exit(0);
	}

	do
	{
		main_prompt();
	}
	while (!done);

	exit(0);
}

void
main_prompt()
{
	int choice;

	printf("\nDevice: %s\n", deviceName);

	showDiskUsage(0);

	printf("\nFdisk main menu\n");
	printf(  "----------------\n");
	printf("1) Create a new partition\n");
	printf("2) Delete a partition\n");
	printf("3) Set the active partition\n");
	printf("4) Show disk information\n");
	printf("5) Quit without saving changes\n");
	printf("6) Save changes and quit\n\n");
	printf("Enter 1-6: "); fflush(stdout);

	choice = -1;
	scanf("%d",&choice);
	switch(choice)
	{
	case 1:
		create_partition();
		break;
	case 2:
		delete_partition();
		break;
	case 3:
		select_active_partition();
		break;
	case 4:
		showDiskInfo();
		break;
	case 5:
		done = 1;
		break;
	case 6:
		if (save_changes() == 0) done = 1;
		break;
	default:
		printf("Invalid selection\n");
		sleep(1);
		break;
	}

	if (!done)
	{
		eatkeys();
		cls();
	}
}

void
showDiskInfo()
{
	int i;
	struct fdisk_part *fp;

	cls();
	printf("Partition Table\n");
	printf("----------------\n");
	printf("  Act  H  S Cyl   Id   H  S Cyl     Begin      Size\n");
	printf("  ---  -  - ---   --   -  - ---     -----      ----\n");

	for (i=0; i<4; i++)
	{
		fp = &bootsector.parts[i];
		printf("  %2x  %2x %2x %3x   ", fp->bootid, fp->beghead, fp->begsect & 0x3f,
			((unsigned)fp->begcyl | (((unsigned)fp->begsect & 0xc0) << 2)));
		printf("%2x  %2x %2x %3x  ", fp->systid, fp->endhead, fp->endsect & 0x3f,
			((unsigned)fp->endcyl | (((unsigned)fp->endsect & 0xc0) << 2)));
		printf("%8lx  %8lx\n",fp->relsect, fp->numsect);
	}
	
	printf("\n\nDisk Information\n");
	printf(  "-----------------\n");
	printf("Disk statistics according to device driver and bios:\n");
	printf("    device: %d Megabytes, %d sectors\n", blk2meg(devNumBlks), devNumBlks);
	printf("    bios:   %d Megabytes, %d sectors\n", 
		blk2meg(cylinders * spc), (cylinders * spc));
	printf("        cylinders = %d, heads = %d, sectors/track = %d\n", 
		cylinders, heads, spt);

	printf("\nPress Return to continue\n");
	eatkeys();
	getchar();
}

int
maxFreeBlocks()
{
	int maxBlockSize = 0, i;
	if (allocedNodes >= 4 || availableNodes == 0) return 0;

	for (i=0; i<availableNodes; i++)
	{
		block_info * bp = &zavailable[i];
		if (bp->size > maxBlockSize) maxBlockSize = bp->size;
	}

	return maxBlockSize;
}

void
create_partition()
{
	int maxBlockSize, choice;
	int requestedMeg, desiredSize, i;
	unsigned char sysId;

	if (allocedNodes >= 4)
	{
		printf("This disk already has 4 partitions.  No more are allowed.\n");
		sleep(2);
		return;
	}

	if (availableNodes == 0)
	{
		printf("This disk has no available space.\n");
		sleep(2);
		return;
	}

	cls();
	printf("Create Partition\n");
	printf("-----------------\n");
	printf("1) Create an Apple UFS partition\n");
	printf("2) Create a non-UFS partition\n");
	printf("\nEnter 1 or 2: "); fflush(stdout);

	choice = -1;
	scanf("%d",&choice);
	switch(choice)
	{
	case 1:
		sysId = 0xA7;
		break;
	case 2:
		sysId = getSysId();
		break;
	default:
		sysId = 0;
		break;
	}

	if (sysId == 0xA7 && idExists(0xA7))
	{
		printf("There can be only one UFS partition.\n");
		sleep(2);
		return;
	}

	if (sysId == 0) return;

	eatkeys();

	maxBlockSize = maxFreeBlocks();

	printf("\nThe maximum available size for a new partition\nis %d megabytes.\n",
		blk2meg(maxBlockSize));
	printf("Enter the desired size, or 0 to cancel: "); fflush(stdout);
	requestedMeg = 0;
	scanf("%d", &requestedMeg);
	
	eatkeys();

	if (requestedMeg <= 0) return;
	desiredSize = requestedMeg * 2048;

	i = zalloc(desiredSize, sysId);

	if (i < 0)
	{
		printf("Unable to allocate such a partition\n");
		sleep(2);
		return;
	}

	if ((sysId == 0xA7) && (indexOfActive() == 0)) activateUFS();
}

void
delete_partition()
{
	int choice;

	if (allocedNodes == 0)
	{
		printf("No partitions to delete\n");
		sleep(1);
		return;
	}

	cls();
	showDiskUsage(1);
	printf("\nDelete Partition\n");
	printf(  "-----------------\n");
	printf("Enter partition to delete, or 0 to do nothing: "); fflush(stdout);
	choice = 0;
	scanf("%d",&choice);

	if (choice >=1 && choice <= allocedNodes)
	{
		zfree(zalloced[choice-1].start);
	}
}

int
save_changes()
{
	struct fdisk_part *fp;
	block_info *bip;
	int i;
	char c;

	if (interactive)
	{
		eatkeys();
		printf("Really write changes? "); fflush(stdout);
		scanf("%c",&c);
		if ((c | 0x20) != 'y') return -1;

		printf("Saving changes\n");
	}

	for (i=0; i<allocedNodes; i++)
	{
		int last;
		fp = &bootsector.parts[i];
		bip = &zalloced[i];
		last = bip->start + bip->size - 1;
		fp->bootid = bip->active;
		fp->beghead = HEAD(bip->start);
		fp->begsect = SECT(bip->start);
		fp->begcyl = CYL(bip->start);
		fp->systid = bip->ident;
		fp->endhead = HEAD(last);
		fp->endsect = SECT(last);
		fp->endcyl = CYL(last);
		fp->relsect = bip->start;
		fp->numsect = bip->size;
	}

	for (i=allocedNodes; i<4; i++)
	{
		int j;
		char *cp = (char *)&bootsector.parts[i];
		for (j=0; j<16; j++) *cp++ = 0;
	}

	// write bootsector
	lseek(fd, 0, L_SET);
	write(fd, &bootsector, sizeof(struct disk_blk0));

	if (!bootsectorOnly)
	{
		// trash the first sector of the newly created partitions
		bzero((char *)&bootsector, sizeof(struct disk_blk0));
		for (i=0; i<allocedNodes; i++)
		{
			bip = &zalloced[i];
			if (bip->newPart)
			{
				lseek(fd, (off_t)(bip->start * 512), L_SET);
				write(fd, &bootsector, sizeof(struct disk_blk0));
			}
		}
	}

	return 0;
}

void
readPartitionTable()
{
	struct fdisk_part *fp;
	int i;
	for (i=0; i<4; i++)
	{
		fp = &bootsector.parts[i];

		// if the partition table entry is in use and we can remove its blocks
		// from the free list, add the partition to the allocated list

		if (fp->systid && zappropriateFromFree(fp->relsect, fp->numsect))
		{
			zallocate(fp->relsect, fp->numsect, fp->systid, fp->bootid, 0);
		}
	}
}

//---------------- begin allocation code -----------------
// sam's simple allocator


int
evenCyl(int sector)
{
	return (sector/spc)*spc;
}

// define the blocks that the allocator will use
void
zinit(int size)
{
	zavailable[0].start = spt;
	zavailable[0].size = evenCyl(size) - spt;
	availableNodes = 1;
	allocedNodes = 0;
}

int
zalloc(int size, unsigned char ident)
{
	int i;
	int ret = -1;
	int tsize;

	if (allocedNodes >= NODES) return ret;

	for (i=0; i<availableNodes; i++)
	{
		// make this size request end on a cylinder boundary
		tsize = evenCyl(zavailable[i].start + size) - zavailable[i].start;

		// if we didn't request any space or the request wouldn't be
		// within 300K of desired size, rount up to nearest cyl
		if ((tsize <= 0) || ((size-tsize) > 600)) tsize += spc;

		// uses first possible node, doesn't try to find best fit
		if (zavailable[i].size == tsize)
		{
			zallocate(ret = zavailable[i].start, tsize, ident, 0, 1);
			zdelete(zavailable, i); availableNodes--;
			break;
		}
		else if (zavailable[i].size > tsize)
		{
			zallocate(ret = zavailable[i].start, tsize, ident, 0, 1);
			zavailable[i].start += tsize;
			zavailable[i].size -= tsize;
			break;
		}
	}

	return ret;
}

int zappropriateFromFree(int start, int size)
{
	int i;

	for (i=0; i<availableNodes; i++)
	{
		int end;

		if (zavailable[i].start == start)
		{
			if (zavailable[i].size == size)
			{
				zdelete(zavailable, i); availableNodes--;
				return 1;
			}
			if (zavailable[i].size > size)
			{
				zavailable[i].start += size;
				zavailable[i].size -= size;
				return 1;
			}
			return 0;
		}

		end = zavailable[i].start + zavailable[i].size;
		if ((start > zavailable[i].start) && ((start+size) <= end))
		{
			if ((start+size) == end)
			{
				zavailable[i].size -= size;
				return 1;
			}

			// else we must split the available block
			zinsert(zavailable, i); availableNodes++;
			zavailable[i].size = start - zavailable[i].start;
			i++;
			zavailable[i].start = (start+size);
			zavailable[i].size = end - zavailable[i].start;

			return 1;
		}
	}

	return 0;
}

int
zfree(int start)
{
	int i, tsize = 0, found = 0;

	if (!start) return -1;

	for (i=0; i<allocedNodes; i++)
	{
		if (zalloced[i].start == start)
		{
			tsize = zalloced[i].size;
			zdelete(zalloced, i); allocedNodes--;
			found = 1;
			break;
		}
	}
	if (!found) return -1;

	for (i=0; i<availableNodes; i++)
	{
		if ((start+tsize) == zavailable[i].start)  // merge it in
		{
			zavailable[i].start = start;
			zavailable[i].size += tsize;
			zcoalesce();
			return 0;
		}

		if (i>0 && (zavailable[i-1].start + zavailable[i-1].size == start))
		{
			zavailable[i-1].size += tsize;
			zcoalesce();
			return 0;
		}

		if ((start+tsize) < zavailable[i].start)
		{
			zinsert(zavailable, i); availableNodes++;
			zavailable[i].start = start;
			zavailable[i].size = tsize;
			return 0;
		}
	}

	zavailable[i].start = start;
	zavailable[i].size = tsize;
	availableNodes++;
	zcoalesce();
	return 0;
}

void
zallocate(int start, int size, unsigned char ident, unsigned char active, 
	      int newPart)
{
	int i;
	for (i=0; i<allocedNodes; i++)
	{
		if (start < zalloced[i].start) break;
	}
	zinsert(zalloced, i);

	zalloced[i].start = start;
	zalloced[i].size = size;
	zalloced[i].ident = ident;
	zalloced[i].active = active;
	zalloced[i].newPart = newPart;
	allocedNodes++;
}

void
zinsert(block_info *zp, int ndx)
{
	int i;
	block_info *z1, *z2;

	i=NODES-2;
	z1 = zp + i;
	z2 = z1+1;

	for (; i>= ndx; i--, z1--, z2--)
	{
		*z2 = *z1;
	}
}

void
zdelete(block_info *zp, int ndx)
{
	int i;
	block_info *z1, *z2;

	z1 = zp + ndx;
	z2 = z1+1;

	for (i=ndx; i<NODES-1; i++, z1++, z2++)
	{
		*z1 = *z2;
	}
}

void
zcoalesce()
{
	int i;
	for (i=0; i<availableNodes-1; i++)
	{
		if (zavailable[i].start + zavailable[i].size == zavailable[i+1].start)
		{
			zavailable[i].size += zavailable[i+1].size;
			zdelete(zavailable, i+1); availableNodes--;
			return;
		}
	}	
}

//---------------- end allocation code -----------------

void
cls()
{
	int i;
	for (i=0;i<20;i++) printf("\n");
}

int blk2meg(x)
{
	return (x + 1024) / 2048;
}

struct part_type
{
	unsigned char type;
	char *name;
}part_types[] =
{
	 {0x00, "unused"}   
	,{0x01, "DOS, 12 bit FAT"}   
	,{0x02, "XENIX"}  
	,{0x03, "XENIX"}  
	,{0x04, "DOS, 16 bit FAT"}   
	,{0x05, "Extended DOS"}   
	,{0x06, "DOS, 16 bit FAT"}   
	,{0x07, "OS/2 HPFS, QNX"}  
	,{0x08, "AIX filesystem"}   
	,{0x09, "AIX, Coherent"}  
	,{0x0A, "OS/2 Boot Mgr"}  
	,{0x10, "OPUS"}   
	,{0x52, "CP/M, SysV/AT"}  
	,{0x64, "Novell 2.xx"}      
	,{0x65, "Novell 3.xx"} 
	,{0x75, "PCIX"}  
	,{0x80, "Minix"} 
	,{0x81, "Minix"}   
	,{0x82, "Linux"}   
	,{0xA5, "386BSD"} 
	,{0xA7, "Apple UFS"}
	,{0xB7, "BSDI"} 
	,{0xB8, "BSDI swap"} 
	,{0xFF, "Bad Block Table"}  
};

char *get_type(type)
int	type;
{
	int	numentries = (sizeof(part_types)/sizeof(struct part_type));
	int	counter = 0;
	struct	part_type *ptr = part_types;

	
	while(counter < numentries)
	{
		if(ptr->type == type)
		{
			return(ptr->name);
		}
		ptr++;
		counter++;
	}
	return 0;
}

void
showDiskUsage(int enumerateEntries)
{
	int i;
	char *cp;

	if (allocedNodes > 0)
	{
		printf("\n   Type              Start   Size    Status\n");
		printf("--------------------------------------------\n");
		for (i=0; i<allocedNodes; i++)
		{
			block_info * bp = &zalloced[i];
			if (enumerateEntries) printf("%d) ",i+1);
			else printf("   ");

			if (cp = get_type(bp->ident))
				printf("%-17s",cp);
			else printf("Type %02X          ",bp->ident);

			printf("%5d   %5d    ",
				blk2meg(bp->start), blk2meg(bp->size));

			printf("%s\n", (bp->active == 0x80)?"Active":"  -   ");
		}
	}
	else
	{
		printf("No partitions in use\n");
	}

	if (!enumerateEntries && (availableNodes > 0))
	{
		printf("\n   Unused Blocks     Start   Size\n");
		printf("   -------------------------------\n");
		for (i=0; i<availableNodes; i++)
		{
			block_info * bp = &zavailable[i];
			printf("   Free Space       %5d   %5d\n",
				blk2meg(bp->start), blk2meg(bp->size));
		}
	}
}

unsigned char getSysId()
{
	int val;
	printf("\nYou must specify the system identification value.\n");
	printf("Common system identification values include:\n");
	printf("    01 - DOS small FAT filesystem\n");
	printf("    05 - DOS extended partition\n");
	printf("    06 - DOS large FAT filesystem\n");
	printf("    07 - OS/2 HPFS or QNX\n");
	printf("    82 - Linux\n");
	printf("    A5 - 386BSD\n");
	printf("    A7 - Apple UFS\n");
	printf("However, any hexidecimal value may be used.\n\n");
	printf("Enter the new partition's system identification ");
	printf("value, or 0 to cancel: "); fflush(stdout);

	val = 0;
	scanf("%x",&val);
	return (val & 0xff);

}

int
indexOfActive()
{
	int i;
	for (i=0; i<allocedNodes; i++)
	{
		block_info * bp = &zalloced[i];
		if (bp->active == 0x80) return i+1;
	}
	return 0;
}

void
activateByIndex(ndx)
{
	int i;
	for (i=0; i<allocedNodes; i++)
	{
		block_info * bp = &zalloced[i];
		if (bp->active == 0x80) bp->active = 0;
	}
	zalloced[ndx].active = 0x80;
}

void
activateUFS()
{
	int ret;
	if (ret = idExists(0xA7)) activateByIndex(ret-1);
}

void
select_active_partition()
{
	int choice;

	if (allocedNodes == 0)
	{
		printf("No partitions to activate\n");
		sleep(1);
		return;
	}

	cls();
	showDiskUsage(1);
	printf("\nActivate Partition\n");
	printf(  "-------------------\n");
	printf("Enter partition to activate, or 0 to do nothing: "); fflush(stdout);
	choice = 0;
	scanf("%d",&choice);

	if (choice >=1 && choice <= allocedNodes)
	{
		activateByIndex(choice-1);
	}
}

void
iprintf(a1,a2,a3,a4)
{
	if (interactive) printf((char *)a1,a2,a3,a4);
}

int
idExists(val)
{
	int i;
	for (i=0; i<allocedNodes; i++)
	{
		block_info * bp = &zalloced[i];
		if (bp->ident == (val & 0xff)) return i+1;
	}
	return 0;
}

void
eatkeys()
{
	fseek(stdin, 0, SEEK_END);
}

int
diskPartitioned()
{
	int i;
	struct fdisk_part *fp;

	if (bootsector.signature != DISK_SIGNATURE) return 0;

	// if we find one good entry, assume it's partitioned

	for (i=0; i<4; i++)
	{
		fp = &bootsector.parts[i];

		if (CYL(fp->relsect) == fp->begcyl &&
				HEAD(fp->relsect) == fp->beghead &&
				SECT(fp->relsect) == fp->begsect &&
				fp->endhead == (heads-1) &&
				(fp->endsect & 0x3f) == spt)
			return 1;
	}
	return 0;
}

void
nukeID(val)
{
	int ret;
	while (ret = idExists(val)) zfree(zalloced[ret-1].start);
}

void
nukeAll()
{
	while (allocedNodes) zfree(zalloced[0].start);
}

enum {
	LOOKFORNEXT,
	LOOKFOREXTENDED,
	LOOKFORPARTITIONING,
	REPORTFREESPACE,
	REPORTFREEWONEXT,
	REPORTFREEWOEXT,
	REPORTEXTENDEDSIZE,
	REMOVEPARTITIONING,
	DOSPLUSNEXT,
	SETAVAILABLETONEXT,
	SETEXTANDAVAILTONEXT,
	SETEXTTONEXT,
	SETNEXTACTIVE,
	DISKSIZE,
	INSTALLSIZE
	} whichAction;

void
strtolower(char * cp)
{
        char c;

	while (c = *cp)
	    *cp++ = tolower(c);
	return;
}

void
examineArgs(int argc, char *argv[])
{
	int i;
	for(i=2; i<argc; i++)
	{
		strtolower(argv[i]);

		if (!strcmp(argv[i],"-useallsectors")) useAllSectors = 1;
		else if (!strcmp(argv[i],"-useboot0")) useBoot0 = 1;
		else if (!strcmp(argv[i],"-bootsectoronly")) bootsectorOnly = 1;
		else if (!strcmp(argv[i],"-isthereufspartition"))
		{
			interactive--;
			whichAction = LOOKFORNEXT;
		}
		else if (!strcmp(argv[i],"-isthereextendedpartition"))
		{
			interactive--;
			whichAction = LOOKFOREXTENDED;
		}
		else if (!strcmp(argv[i],"-isdiskpartitioned"))
		{
			interactive--;
			whichAction = LOOKFORPARTITIONING;
		}
		else if (!strcmp(argv[i],"-freespace"))
		{
			interactive--;
			whichAction = REPORTFREESPACE;
		}
		else if (!strcmp(argv[i],"-freewithoutufs"))
		{
			interactive--;
			whichAction = REPORTFREEWONEXT;
		}
		else if (!strcmp(argv[i],"-freewithoutufsorext"))
		{
			interactive--;
			whichAction = REPORTFREEWOEXT;
		}
		else if (!strcmp(argv[i],"-sizeofextended"))
		{
			interactive--;
			whichAction = REPORTEXTENDEDSIZE;
		}
		else if (!strcmp(argv[i],"-disksize"))
		{
			interactive--;
			whichAction = DISKSIZE;
		}
		else if (!strcmp(argv[i],"-installsize"))
		{
			interactive--;
			whichAction = INSTALLSIZE;
		}

	// Actions
		else if (!strcmp(argv[i],"-removepartitioning"))
		{
			interactive--;
			whichAction = REMOVEPARTITIONING;
		}
		else if (!strcmp(argv[i],"-dosplusufs"))
		{
			interactive--;
			whichAction = DOSPLUSNEXT;
			if (++i >= argc)
				bomb("fdisk: must specify how many megabytes for DOS\n");
			sscanf(argv[i],"%d",&megsForDos);
		}
		else if (!strcmp(argv[i],"-setavailabletoufs"))
		{
			interactive--;
			whichAction = SETAVAILABLETONEXT;
		}
		else if (!strcmp(argv[i],"-setextandavailabletoufs"))
		{
			interactive--;
			whichAction = SETEXTANDAVAILTONEXT;
		}
		else if (!strcmp(argv[i],"-setextendedtoufs"))
		{
			interactive--;
			whichAction = SETEXTTONEXT;
		}
		else if (!strcmp(argv[i],"-setufsactive"))
		{
			interactive--;
			whichAction = SETNEXTACTIVE;
		}

		else {
		    usage();
		}
	}

	if (interactive < 0)
	    bomb("fdisk: only one action or inquiry allowed\n");
}

void
doActionOrInquiry()
{
	int ret, writeBoot = 0;
	char *cp;

	switch(whichAction)
	{
	case LOOKFORNEXT:
		if (idExists(0xA7)) printf("Yes\n");
		else printf("No\n");
		break;
	case LOOKFOREXTENDED:
		if (idExists(0x05)) printf("Yes\n");
		else printf("No\n");
		break;
	case LOOKFORPARTITIONING:
		if (diskPartitioned()) printf("Yes\n");
		else printf("No\n");
		break;
	case REPORTFREESPACE:
		printf("%d\n", blk2meg(maxFreeBlocks()));
		break;
	case REPORTFREEWONEXT:
		nukeID(0xa7);
		printf("%d\n", blk2meg(maxFreeBlocks()));
		break;
	case REPORTFREEWOEXT:
		nukeID(0x05);
		nukeID(0xa7);
		printf("%d\n", blk2meg(maxFreeBlocks()));
		break;
	case REPORTEXTENDEDSIZE:
		if (ret = idExists(0x05))
			printf("%d\n", blk2meg(zalloced[ret-1].size));
		else printf("0\n");
		break;
	case DISKSIZE:
		printf("%d\n", blk2meg(diskSize));
		break;
	case INSTALLSIZE:
		if (!diskPartitioned())	printf("%d\n", blk2meg(diskSize));
		else if (ret = idExists(0xa7))
			printf("%d\n", blk2meg(zalloced[ret-1].size));
		else printf("0\n");
		break;

	// Actions

	case REMOVEPARTITIONING:
		nukeAll();
		cp = (char *)(&bootsector);
		for (ret = 0; ret < 512; ret++) *cp++ = 0;
		writeBoot = 1;
		break;
	case DOSPLUSNEXT:
		nukeAll();
		zalloc(megsForDos * 2048, 0x06);
		zalloc(maxFreeBlocks(), 0xA7);
		activateUFS();
		writeBoot = 1;
		break;
	case SETAVAILABLETONEXT:
		nukeID(0xa7);
		zalloc(maxFreeBlocks(), 0xA7);
		activateUFS();
		writeBoot = 1;
		break;
	case SETEXTANDAVAILTONEXT:
		nukeID(0x05);
		nukeID(0xa7);
		zalloc(maxFreeBlocks(), 0xA7);
		activateUFS();
		writeBoot = 1;
		break;
	case SETEXTTONEXT:
		if (ret = idExists(0x05)) zalloced[ret-1].ident = 0xA7;
		activateUFS();
		writeBoot = 1;
		break;
	case SETNEXTACTIVE:
		activateUFS();
		writeBoot = 1;
		break;
	}

	if (writeBoot)
	{
		save_changes();
	}
}





