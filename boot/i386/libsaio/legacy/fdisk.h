/*
 * Copyright (c) 1992 NeXT Computer, Inc.
 *
 * IBM PC disk partitioning data structures.
 *
 * HISTORY
 *
 * 8 July 1992 David E. Bohman at NeXT
 *	Created.
 */
 
#ifdef	DRIVER_PRIVATE

#define DISK_BLK0	0		/* blkno of boot block */
#define DISK_BLK0SZ	512		/* size of boot block */
#define DISK_BOOTSZ	446		/* size of boot code in boot block */
#define	DISK_SIGNATURE	0xAA55		/* signature of the boot record */
#define FDISK_NPART	4		/* number of entries in fdisk table */
#define FDISK_ACTIVE	0x80		/* indicator of active partition */
#define FDISK_NEXTNAME	0xA7		/* indicator of NeXT partition */
#define FDISK_DOS12	0x01            /* 12-bit fat < 10MB dos partition */
#define FDISK_DOS16S	0x04            /* 16-bit fat < 32MB dos partition */
#define FDISK_DOSEXT	0x05            /* extended dos partition */
#define FDISK_DOS16B	0x06            /* 16-bit fat >= 32MB dos partition */

/*
 * Format of fdisk partion entry (if present).
 */
struct fdisk_part {
    unsigned char	bootid;		/* bootable or not */
    unsigned char	beghead;	/* begining head, sector, cylinder */
    unsigned char	begsect;	/* begcyl is a 10-bit number */
    unsigned char	begcyl;		/* High 2 bits are in begsect */
    unsigned char	systid;		/* OS type */
    unsigned char	endhead;	/* ending head, sector, cylinder */
    unsigned char	endsect;	/* endcyl is a 10-bit number */
    unsigned char	endcyl;		/* High 2 bits are in endsect */
    unsigned long	relsect;	/* partion physical offset on disk */
    unsigned long	numsect;	/* number of sectors in partition */
};

/*
 * Format of boot block.
 */
struct disk_blk0 {
    unsigned char	bootcode[DISK_BOOTSZ];
    unsigned char	parts[FDISK_NPART][sizeof (struct fdisk_part)];
    unsigned short	signature;
};

#endif	/* DRIVER_PRIVATE */
