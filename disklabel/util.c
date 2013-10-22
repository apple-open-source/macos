/*
 * Copyright (c) 2004 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <err.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <unistd.h>
#include <zlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disk.h>

#include <CoreFoundation/CoreFoundation.h>
#include <Kernel/libkern/OSByteOrder.h>

#include <IOKit/storage/IOAppleLabelScheme.h>

#include "util.h"

#define MIN(a, b) \
	({ typeof (a) _x = (a); typeof (b) _y = (b); \
		_x < _y ? _x : _y; })

/*
 * When setting properties, a value can be forced to be
 * a string by putting it in quotes.  e.g., foo="123"
 * This function copies the string, and removes the quotes.
 * If the string does not begin with a double-quote, it is
 * simply duplicated and the duplicate returned; if it does
 * begin with a double-quote, it MUST end with a double-quote,
 * or NULL is returned.
 * The string that is returned must be free()'d by the
 * caller.
 */
static char *
RemoveQuotes(const char *s) {
	char *retval;
	int len;

	if (*s != '"')
		return strdup(s);

	retval = strdup(s + 1);
	len = strlen(retval);
	if (len < 2 || retval[len - 1] != '"') {
		warnx("Quoted string `%s' is mal-formed", s);
		return NULL;
	}
	retval[len - 1] = 0;
	return retval;
}

/*
 * Do some sanity checks on the given device name
 * (e.g., device-name=fred).  Right now, we check to
 * see if it has a '/' in it, or if the device already
 * exists in /dev.  Having a '/' is an error; already
 * existing is a warning.
 * In case of error, we return -1; in case of a warning,
 * we print it out to stderr, but still return success (0).
 */
static int 
CheckDeviceName(const char *name) {
	int len = strlen(name);
	char devname[len + 6];
	struct stat sbuf;

	if (strchr(name, '/') != NULL) {
		warnx("Device name `%s' is invalid:  cannot contain a '/'", name);
		return -1;
	}

	snprintf(devname, sizeof(devname), "/dev/%s", name);
	if (stat(devname, &sbuf) != -1) {
		warnx("Device name `%s' already exists in /dev", name);
	}

	// Any other checks to do?
	return 0;
}

/*
 * Do a basic sanity check on the user name ("user-uid=...").
 * If it's a string, and the user exists, set *uidp to it;
 * if it's a number, set *uidp to it;
 * we return a positive number in those case.
 * Otherwise, print out an appropriate warning, and return -1.
 */
static int
CheckUserName(const char *name, int asString, uid_t *uidp) {
	uid_t uid;
	struct passwd *pwd;

	if (!asString) {
		char *endptr;
		uid = strtoul(name, &endptr, 10);
		if (*name != 0 && *endptr == 0) {
			// It was a digit
			pwd = getpwuid(uid);
			if (pwd == NULL) {
				warnx("UID %d does not exist on this system", uid);
				return -1;
			}
			*uidp = uid;
			return 1;
		}
	}
	// Otherwise, treat it as a string
	pwd = getpwnam(name);
	if (pwd == NULL) {
		warnx("User-name `%s' does not exist on this system", name);
		return -1;
	}

	*uidp = pwd->pw_uid;
	return 1;
}

/*
 * Same thing as above, but for the group ("owner-group=...").
 */
static int
CheckGroupName(const char *name, int asString, gid_t *gidp) {
	gid_t uid;
	struct group *grp;

	if (!asString) {
		char *endptr;
		uid = strtoul(name, &endptr, 10);
		if (*name != 0 && *endptr == 0) {
			// It was a digit
			grp = getgrgid(uid);
			if (grp == NULL) {
				warnx("GID %d does not exist on this system", uid);
				return -1;
			}
			*gidp = uid;
			return 1;
		}
	}
	// Otherwise, treat it as a string
	grp = getgrnam(name);
	if (grp == NULL) {
		warnx("Group-name `%s' does not exist on this system", name);
		return -1;
	}

	*gidp = grp->gr_gid;
	return 1;
}

/*
 * As above, but check the device mode ("owner-mode=...").
 * Right now, it must be an integer, but it could conceivably
 * be made into a string later (e.g., "owner-mode=rw-rw---",
 * perhaps?).  It returns 0 on success, and -1 on failure.
 */
static int
CheckDeviceMode(const char *mode, int asString) {
	unsigned long m;
	char *endptr;

	if (asString) {
		warnx("Device mode must be an integer");
		return -1;
	}

	m = strtoul(mode, &endptr, 0);

	if (*mode != 0 && *endptr == 0) {
		return 0;
	}
	warnx("Device mode `%s' is invalid", mode);
	return -1;
}

/*
 * A routine to get the applelabel structure.  This will be at
 * the beginning of the device.  On error, it returns NULL; on
 * success, it returns a pointer to allocated memory.  It also
 * swaps the values of the structure around to host order, so
 * the caller does not need to.
 */
static struct applelabel *
getDeviceLabel(const char *dev) {
	int fd;
	struct applelabel *retval = NULL;

	fd = open(dev, O_RDONLY);
	if (fd == -1) {
		warn("getDeviceLabel:  unable to open device %s for reading", dev);
		return NULL;
	}
	retval = malloc(sizeof(*retval));
	if (retval == NULL) {
		warnx("getDeviceLabel: unable to allocate memory for device label");
		goto done;
	}
	if (read(fd, retval, sizeof(*retval)) != sizeof(*retval)) {
		warnx("getDeviceLabel: unable to read a full label for device %s", dev);
		free(retval);
		retval = NULL;
		goto done;
	}
	retval->al_magic	= OSSwapBigToHostInt16(retval->al_magic);
	retval->al_type		= OSSwapBigToHostInt16(retval->al_type);
	retval->al_flags	= OSSwapBigToHostInt32(retval->al_flags);
	retval->al_offset	= OSSwapBigToHostInt64(retval->al_offset);
	retval->al_size		= OSSwapBigToHostInt32(retval->al_size);
	retval->al_checksum	= OSSwapBigToHostInt32(retval->al_checksum);

done:
	close(fd);
	return retval;
}

/*
 * The opposite of the above, this sets the device label.
 * It returns -1 on failure.  It assumes the various fields
 * are in host-order, and swaps them around as appropriate.
 */
static int
setDeviceLabel(const char *dev, const struct applelabel *lbl) {
	int fd;
	int retval = -1;
	struct applelabel tmp;

	if (lbl == NULL)
		return retval;
	
	fd = open(dev, O_WRONLY);
	if (fd == -1) {
		warn("setDeviceLabel: unable to open device %s", dev);
		return retval;
	}

	tmp = *lbl;
	tmp.al_magic	= OSSwapHostToBigInt16(tmp.al_magic);
	tmp.al_type	= OSSwapHostToBigInt16(tmp.al_type);
	tmp.al_flags	= OSSwapHostToBigInt32(tmp.al_flags);
	tmp.al_offset	= OSSwapHostToBigInt64(tmp.al_offset);
	tmp.al_size	= OSSwapHostToBigInt32(tmp.al_size);
	tmp.al_checksum	= OSSwapHostToBigInt32(tmp.al_checksum);

	if (write(fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
		warn("setDeviceLabel: unable to write a full label to device %s", dev);
		goto done;
	}
	retval = 0;
done:
	close(fd);
	return retval;
}

/*
 * Apply the crc32 checksum function to the metadata.
 * It needs to know the size so that it can fill out the
 * pad area as needed.  It returns 0 on error, although
 * that's not great.
 */
static uint32_t
ChecksumData(CFDictionaryRef dict, int32_t size) {
	uint32_t retval = 0;
	unsigned char *bytes = NULL;
	int len;
	CFDataRef data = nil;

	bytes = malloc(size);
	if (bytes == NULL) {
		warnx("ChecksumData:  cannot allocate %d bytes of data\n", size);
		return 0;
	}
	memset(bytes, 0, size);

	data = CFPropertyListCreateXMLData(nil, (CFPropertyListRef)dict);
	if (data == nil) {
		warnx("ChecksumData:  cannot create data from dictionary");
		goto done;
	}

	len = CFDataGetLength(data);
	memcpy(bytes, (void*)CFDataGetBytePtr(data), len);

	if (gDebug && gVerbose) {
		fprintf(stderr, "ChecksumData:  calling crc32(0, %p, %d)\n", bytes, size);
	}

	retval = crc32(0, bytes, size);

done:
	if (data)
		CFRelease(data);
	free(bytes);

	return retval;
}

/*
 * Given a C string property (e.g., "owner-uid=fred" or "owner-mode"),
 * turn it into a CFStringRef key, and optionally CFStringRef or CFNumberRef value.
 * Return 0 on error, 1 on success.
 * If the input string has an '=' in it, and defines a value, and the input
 * pointer valuePtr is not NULL, then it will set valuePtr to a CFTypeRef
 * (either a CFStringRef or a CFNumberRef).  If the value is being set, and
 * is one of a few known properties, it will also do some basic sanity checking
 * on the values (see the static functions above).
 */
int
parseProperty(const char *term, CFStringRef *namePtr, CFTypeRef *valuePtr) {
	CFStringRef k = nil;
	CFTypeRef v = nil;
	char *tag;
	char *value = strdup(term);
	char *endptr;
	unsigned int tInt = -1;
	char forceInt = 0;
	char forceString = 0;

	tag = strsep(&value, "=");

	if (gDebug > 1)
		fprintf(stderr, "parseProperty: property `%s' = value `%s'\n",
			tag, value);
	k = CFStringCreateWithCString(nil, tag, kCFStringEncodingASCII);
	if (value) {
		if (*value == '"') {	// force string
			endptr = RemoveQuotes(value);
			free(tag);
			tag = value = endptr;
			forceString = 1;
		}

		if (!strcmp(tag, "dev-name")) {
			forceString = 1;	// just in case
			if (CheckDeviceName(value) == -1) {
				goto bad;
			}
		}
		if (!strcmp(tag, "owner-uid")) {
			if (CheckUserName(value, forceString, &tInt) == -1) {
				// An illegal name
				goto bad;
			}			
		}
		if (!strcmp(tag, "owner-gid")) {
			if (CheckGroupName(value, forceString, &tInt) == -1) {
				// An illegal group
				goto bad;
			}
		}
		if (!strcmp(tag, "owner-mode")) {
			if (CheckDeviceMode(value, forceString) == -1) {
				// An illegal mode
				goto bad;
			}
		}

		if (tInt == -1) {
			tInt = strtol(value, &endptr, 0);
		} else {
			forceInt = 1;
		}
		if (!forceInt && (forceString || (endptr == value ||
			(*endptr != 0 && *value != 0)))) {
			if (gDebug)
				fprintf(stderr, "Property `%s' has value `%s'\n",
					tag, value);
			v = CFStringCreateWithCString(nil, value, kCFStringEncodingASCII);
		} else {
			if (gDebug)
				fprintf(stderr, "Property `%s' has int value %d\n",
					tag, tInt);
			v = CFNumberCreate(nil, kCFNumberSInt32Type, &tInt);
		}
	}
	if (k) {
		if (namePtr) {
			*namePtr = k;
		} else {
			CFRelease(k);
		}
	}
	if (v) {
		if (valuePtr) {
			*valuePtr = v;
		} else {
			CFRelease(v);
		}
	}
	free(tag);

	return 1;

bad:
	if (v)
		CFRelease(v);
	if (k)
		CFRelease(k);
	if (tag)
		free(tag);
	return 0;
}

/*
 * Read the metadata from the device.  It returns NULL on error,
 * and a CFDictionaryRef on success.  It opens up the device, gets
 * the applelabel from it, and then uses that to read in the data.
 */
CFDictionaryRef
ReadMetadata(const char *dev) {
	int fd;
	struct stat sbuf;
	int64_t off;
	struct applelabel *lbl = NULL;
	CFDictionaryRef retval = nil;
	CFStringRef errStr;
	CFDataRef data = nil;
	void *buf = NULL;
	int len;

	if ((fd = open(dev, O_RDONLY)) == -1) {
		warn("cannot open device %s", dev);
		return nil;
	}
	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warnx("ReadMetadata:  cannot get label for device %s", dev);
		goto done;
	}
	off = lbl->al_offset;
	len = lbl->al_size;

	fstat(fd, &sbuf);

	if (lseek(fd, off, SEEK_SET) == -1) {
		warn("ReadMetadata:  cannot seek to metadata offset for device %s", dev);
		goto done;
	}

	if (gDebug) {
		fprintf(stderr, "For device %s, metadata len (max) = %d\n", dev, len);
	}
	buf = malloc(len);
	if (buf == NULL) {
		warnx("cannot allocate %d bytes to read metadata for device %s", len, dev);
		goto done;
	}
	read(fd, buf, len);

	data = CFDataCreate(nil, buf, len);
	if (data == nil) {
		warnx("cannot create CFData instance of XML");
		goto done;
	}
	retval = (CFPropertyListRef)CFPropertyListCreateFromXMLData(nil,
		data,
		kCFPropertyListImmutable, &errStr);

	if (retval == NULL) {
		int l = CFStringGetLength(errStr);
		char buf[l * 2];
		if (CFStringGetCString(errStr, buf, sizeof(buf), kCFStringEncodingASCII)) {
			warnx("cannot create property list: %s", buf);
		}
		CFRelease(errStr);
	}
done:
	if (buf)
		free(buf);
	if (data)
		CFRelease(data);
	close(fd);
	if (lbl)
		free(lbl);

	return retval;
}

/*
 * Used to create the initial metadata area.  It sets up an
 * applelabel structure at the beginning of the file, computes the
 * initial checksum, and writes out the metadata.  It returns -1
 * on failure.
 */
int
InitialMetadata(const char *dev, CFDictionaryRef dict, uint64_t size) {
	int fd;
	int retval = -1;
	uint32_t bs;
	uint64_t dSize;
	struct applelabel lbl = { { 0 } };

	if (gDebug && gVerbose) {
		fprintf(stderr, "InitialMetadata(%s, dict, %qu)\n", dev, size);
	}

	fd = open(dev, O_RDWR);
	if (fd == -1) {
		warn("InitialMetadata:  cannot open device file %s", dev);
		return -1;
	}

	bs = GetBlockSize(dev);
	if (bs == 0) {
		warnx("InitialMetadata:  cannot get block size for device %s", dev);
		goto done;
	}

	dSize = GetDiskSize(dev);
	if (dSize == 0) {
		warnx("InitialMetadata:  cannot get disk size for device %s", dev);
		goto done;
	}

	if (dSize <= size) {
		warnx("InitialMetadata:  Disk device size (%qu) is not large enough "
			"for metadata size (%qu) for device %s",
			dSize, size, dev);
		goto done;
	}

	lbl.al_magic	= AL_MAGIC;
	lbl.al_type	= AL_TYPE_DEFAULT;
	lbl.al_flags	= AL_FLAG_DEFAULT;
	lbl.al_offset	= bs;	// start at block #1
	lbl.al_size	= (uint32_t)size - bs;
	lbl.al_checksum	= ChecksumData(dict, lbl.al_size);

	if (gDebug) {
		fprintf(stderr, "lbl = {\n");
		fprintf(stderr, "\tal_magic = 0x%x\n", lbl.al_magic);
		fprintf(stderr, "\tal_type = 0x%x\n", lbl.al_type);
		fprintf(stderr, "\tal_flags = 0x%x\n", lbl.al_flags);
		fprintf(stderr, "\tal_offset = %qu\n", lbl.al_offset);
		fprintf(stderr, "\tal_size = %d\n", lbl.al_size);
		fprintf(stderr, "\tal_checksum = 0x%x\n};\n", lbl.al_checksum);
	}

	if (setDeviceLabel(dev, &lbl) == -1) {
		warnx("InitialMetadata:  cannot write header for device %s", dev);
		goto done;
	}

	if (WriteMetadata(dev, dict) == -1) {
		warnx("InitialMetadata:  cannot write metadata");
		goto done;
	}

	retval = 0;
done:
	close(fd);
	return retval;
}

/*
 * Write out the metadata to the file.  It also recomputes
 * the checksum.  It returns -1 on error.
 */
int
WriteMetadata(const char *dev, CFDictionaryRef dict) {
	int fd;
	int64_t off;
	int len;
	CFDataRef data = nil;
	int retval = -1;
	void *bytes;
	uint32_t cksum = 0;
	struct applelabel *lbl;
	uint32_t mSize;

	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warnx("cannot get label for device %s", dev);
		return -1;
	}

	mSize = GetMetadataSize(dev);
	bytes = malloc(mSize);
	memset(bytes, 0, mSize);

	if (bytes == NULL) {
		warnx("WriteMetadata: cannot allocate %u bytes\n", mSize);
		return -1;
	}

	fd = open(dev, O_RDWR);
	if (fd == -1) {
		warn("cannot open device %s", dev);
		return -1;
	}
	if (GetDiskSize(dev) < lbl->al_size) {
		warnx("device %s is too small for metadata size", dev);
		return -1;
	}

	off = lbl->al_offset;

	if (lseek(fd, off, SEEK_SET) == -1) {
		warn("WriteMetadata: cannot seek to offset %qu for device %s", off, dev);
		goto done;
	}

	data = CFPropertyListCreateXMLData(nil, (CFPropertyListRef)dict);
	if (data == nil) {
		warnx("cannot create CFData from dictionary");
		goto done;
	}
	len = CFDataGetLength(data);
	memcpy(bytes, CFDataGetBytePtr(data), len);

	if (write(fd, bytes, mSize) != mSize) {
		warn("cannot write %d bytes to metadata area", mSize);
		goto done;
	}

	cksum = ChecksumData(dict, mSize);

	lbl->al_checksum = cksum;
	if (setDeviceLabel(dev, lbl) == -1) {
		warnx("unable to update label for device %s", dev);
		goto done;
	}

	retval = 1;
done:
	close(fd);
	if (data)
		CFRelease(data);
	return retval;
}

/*
 * Return the checksum of the metadata area *from the disk*.
 * It returns 0 on error (not great, I know).
 */
uint32_t
ChecksumMetadata(const char *dev) {
	int fd = -1;
	struct stat sbuf;
	int64_t off;
	int32_t cksum = 0;
	unsigned char *buf = NULL;
	int len;
	struct applelabel *lbl = NULL;

	if ((fd = open(dev, O_RDONLY)) == -1) {
		warn("cannot open device %s", dev);
		return 0;
	}

	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warnx("ChecksumMetadata:  cannot get label for device %s", dev);
		goto done;
	}
	fstat(fd, &sbuf);
	off = lbl->al_offset;
	len = lbl->al_size;

	if (lseek(fd, off, SEEK_SET) == -1) {
		warn("ChecksumMetadata:  cannot seek to %qu for device %s", off, dev);
		goto done;
	}

	if (gDebug) {
		fprintf(stderr, "ChecksumMetadata: For device %s, metadata len (max) = %d\n", dev, len);
	}
	buf = malloc(len);
	if (buf == NULL) {
		warnx("ChecksumMetadata: cannot allocate %d bytes to read metadata for device %s", len, dev);
		goto done;
	}
	read(fd, buf, len);

	if (gDebug && gVerbose) {
		fprintf(stderr, "ChecksumMetadata:  calling crc32(0, %p, %d)\n", buf, len);
	}

	cksum = crc32(0, buf, len);

	if (gDebug) {
		fprintf(stderr, "ChecksumMetadata: For device %s, checksum = %u\n", dev, cksum);
	}

done:
	if (buf)
		free(buf);
	close(fd);
	if (lbl)
		free(lbl);

	return cksum;
}

/*
 * Update the applelabel's value of the checksum to the indicated
 * value.  It returns -1 on error.
 */
int
UpdateChecksum(const char *dev, uint32_t sum) {
	struct applelabel *lbl;

	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warnx("UpdateChecksum:  cannot get label for device %s", dev);
		return -1;
	}

	lbl->al_checksum = sum;
	if (setDeviceLabel(dev, lbl) == -1) {
		warnx("UpdateChecksum:  cannot update label for device %s", dev);
		return -1;
	}
	free(lbl);

	return 0;
}

/*
 * A simpler version to get teh checksum, this returns
 * the value that is in the applelabel structure.
 */
uint32_t
GetChecksum(const char *dev) {
	struct applelabel *lbl;
	uint32_t retval = 0;

	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warnx("GetChecksum:  cannot get label for device %s", dev);
		return retval;
	}
	retval = lbl->al_checksum;
	free(lbl);
	return retval;
}

/*
 * Compare the applelabel structure's copy of the checksum against
 * a computed value of the checksum.  Return the difference -- it should
 * be 0, obviously, on success!
 */
int
VerifyChecksum(const char *dev) {
	int32_t written, data;

	if ((data = ChecksumMetadata(dev)) < 1) {
		warnx("VerifyChecksum:  Metadata checksum is 0x%x", data);
	}
	if ((written = GetChecksum(dev)) < 1) {
		warnx("VerifyChecksum:  Metadata written checksum is 0x%x", written);
	}
	return data - written;
}

/*
 * Return the size of the metadata area, as defined by
 * the applelabel structure.
 */
uint32_t
GetMetadataSize(const char *dev) {
	struct applelabel *lbl = NULL;
	uint32_t retval;
	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warnx("GetMetadataSize:  cannot get label for device %s", dev);
		return 0;
	}

	retval = lbl->al_size;
	free(lbl);
	return retval;
}

/*
 * Get the device label (if possible), and see if it's
 * got the right magic value.  If so, return 1; otherwise,
 * return 0;
 */
int
IsAppleLabel(const char *dev) {
	struct applelabel *lbl = NULL;
	int retval = 0;

	lbl = getDeviceLabel(dev);
	if (lbl == NULL) {
		warn("IsAppleLabel:  cannot get label for device %s", dev);
		return 0;
	}

	retval = lbl->al_magic == AL_MAGIC;
	free(lbl);
	return retval;
}

/*
 * Return the blocksize for the requested device.
 * If the device is not a disk, we return an error (0),
 * UNLESS debugging is turned on (gDebug, -D command line option).
 * In that case, if we can't get teh block size for the "device,"
 * we assume 4k.
 */
uint32_t
GetBlockSize(const char *dev) {
	uint32_t retval = 0;
	int fd;

	fd = open(dev, O_RDONLY);
	if (fd == -1) {
		warn("BlockSize:  cannot open %s", dev);
		return 0;
	}
	if (ioctl(fd, DKIOCGETBLOCKSIZE, &retval) == -1) {
		if (gDebug) {
			retval = 4 * 1024;	// a default blocksize 
		} else {
			retval = 0;
			warn("BlockSize:  cannot get blocksize for device %s", dev);
		}
	}
	close(fd);
	return retval;
}

/*
 * Similar to above, but it returns the size (in bytes)
 * of the device.  If debugging is enabled, and it is
 * a file, it just returns the size of the file.
 */
uint64_t
GetDiskSize(const char *dev) {
	int bs;
	uint64_t bc;
	int fd;
	uint64_t retval = 0;

	fd = open(dev, O_RDONLY);
	if (fd == -1) {
		warn("GetDiskSize:  cannot open %s", dev);
		return 0;
	}
	bs = GetBlockSize(dev);
	if (bs == 0) {
		return 0;
	}

	if (ioctl(fd, DKIOCGETBLOCKCOUNT, &bc) == -1) {
		if (gDebug) {
			struct stat sbuf;
			if (fstat(fd, &sbuf) == -1) {
				warn("GetDiskSize:  cannot fstat %s", dev);
			} else {
				retval = sbuf.st_size;
			}
		} else {
			warn("GetDiskSize:  cannot get block count for device %s", dev);
		}
	} else {
		retval = bs * bc;
	}
	close(fd);
	return retval;
}

