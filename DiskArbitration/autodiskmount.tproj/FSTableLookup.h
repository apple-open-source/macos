#include "DiskVolume.h"

/*****
 *
 *	Read the fstab entries into memory, if it has changed since the last
 *	time it was read.
 *
 *	Return Codes:
 *
 *	1		fstab was successfully read
 *	0		fstab unchanged since last access.
 *	-1		Error occurred reading fstab.
 *
 *****/
int FSTableLookup_readFstab();

/*****
 *
 *	All three of the following routines will partially fill out a DiskVolume
 *	structure, from information contained in /etc/fstab, based on different
 *	search criteria.
 *
 *	All three routines use the DiskVolume structure as follows:
 *
 *	fs_type:		Input.  If non-zero, then only entries whose fs type
 *					(column 3) matches this value will be accepted.
 *	mount_point:	Output.  Set to the mount point for the file system
 *					(column 2) if a match is found.
 *	writable:		Output.  Non-zero if the fstab etry has the rw option 
 *					set (in column 4), zero otherwise.
 *
 *	All other fields are ignored.  The string value stored in mount_point
 *	must be freed by a call to free(3).
 *
 *	Any columns in fstab beyond the fourth are ignored.  Entries whose 
 *	mount options include xx are ignored. Mount options other than rw or
 *	xx are ignored.
 *
 *	Return Codes:
 *
 *	0		Search complete, the first match was returned.
 *	-1		Search complete, no match was found, and diskVolume was not
 *			modified.
 *	-2		Invalid argument.  Either the search parameter was zero, 
 *			diskVolume was zero, or the mount_point field was non-zero.
 *
 *****/

/*
 *	Find an fstab entry whose first column matches UUID=<uuid>.
 */
int FSTableLookup_byUUID(const char* uuid, DiskVolumePtr diskVolume);

/*
 *	Find an fstab entry whose first column matches LABEL=<label>.
 */
int FSTableLookup_byLabel(const char* label, DiskVolumePtr diskVolume);

/*
 *	Find an fstab entry whose first column matches DEVICE=<device>.
 */
int FSTableLookup_byDevice(io_service_t device, DiskVolumePtr diskVolume);
