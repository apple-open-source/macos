#include "FSTableLookup.h"

#include <assert.h>
#include <fstab.h>
#include <sys/stat.h>

/*
 *	Constants
 */
#define UUID_PREFIX		"UUID="
#define LABEL_PREFIX	"LABEL="
#define DEVICE_PREFIX	"DEVICE="

#define MATCH_FOUND		0
#define	NO_MATCH_FOUND	-1
#define	BAD_ARGUMENT	-2

#define	FSTAB_READ			1
#define	FSTAB_UNCHANGED		0
#define FSTAB_ERROR			-1

/*
 *	Local Types
 */
typedef struct fstab_cache_entry {
	struct fstab				fstab;
	struct fstab_cache_entry*	next;
} fstab_cache_entry;

/*
 *	Local Variables
 */
struct timespec		lastFstabModTime	= { 0, -1 };
fstab_cache_entry*	fstabCache			= NULL;

/*
 *	Local Function Prototypes
 */
static int		fstabLookup		(const char* prefix, const char* key, DiskVolumePtr diskVolume);
static char *	getfsmntopt		(struct fstab * fs_entry, char * fs_mntopt);

/*
 *	Reload our cached fstab data, if necessary.
 */
int
FSTableLookup_readFstab(void)
{
	struct stat			sb;
	struct fstab*		line;
	fstab_cache_entry*	entry;
	fstab_cache_entry*	lastEntry = NULL;

	if (stat(_PATH_FSTAB, &sb))
		return FSTAB_ERROR;
	
	if (sb.st_mtimespec.tv_sec == lastFstabModTime.tv_sec &&
		sb.st_mtimespec.tv_nsec == lastFstabModTime.tv_nsec)
		return FSTAB_UNCHANGED;
	
	/* Update our last mod time. */
	lastFstabModTime.tv_sec	 = sb.st_mtimespec.tv_sec;
	lastFstabModTime.tv_nsec = sb.st_mtimespec.tv_nsec;
	
	/*	Clear the current cache entries. */
	for (entry = fstabCache; entry != NULL; ) {
		fstab_cache_entry* temp = entry;
		
		free(temp->fstab.fs_spec);
		free(temp->fstab.fs_vfstype);
		free(temp->fstab.fs_file);
		free(temp->fstab.fs_type);
		
		entry = temp->next;
		
		free(temp);
	}
	
	fstabCache = NULL;
	
	/* Walk the fstab entries one by one. */
	if (!setfsent())
		return FSTAB_ERROR;
	
	while ((line = getfsent()) != NULL) {
		entry = calloc(1, sizeof (fstab_cache_entry));
		
		/* Copy the fields we use from line to entry. */
		entry->fstab.fs_spec	= strdup(line->fs_spec    ? line->fs_spec    : "");
		entry->fstab.fs_file	= strdup(line->fs_file    ? line->fs_file    : "");
		entry->fstab.fs_vfstype	= strdup(line->fs_vfstype ? line->fs_vfstype : "");
		entry->fstab.fs_mntops	= strdup(line->fs_mntops  ? line->fs_mntops  : "");
		entry->fstab.fs_type	= strdup(line->fs_type    ? line->fs_type    : "");
		
		assert(entry->fstab.fs_spec);
		assert(entry->fstab.fs_file);
		assert(entry->fstab.fs_vfstype);
		assert(entry->fstab.fs_mntops);
		assert(entry->fstab.fs_type);
		
		if (lastEntry)
			lastEntry->next = entry;
		else
			fstabCache = entry;

		lastEntry = entry;
	}
	
	endfsent();
		
	return FSTAB_READ;
}

/*
 *	Find an fstab entry whose first column matches UUID=<uuid>.
 */
int
FSTableLookup_byUUID(const char* uuid, DiskVolumePtr diskVolume)
{
	return fstabLookup(UUID_PREFIX, uuid, diskVolume);
}

/*
 *	Find an fstab entry whose first column matches LABEL=<label>.
 */
int
FSTableLookup_byLabel(const char* label, DiskVolumePtr diskVolume)
{
	return fstabLookup(LABEL_PREFIX, label, diskVolume);
}

/*
 *	Find an fstab entry whose first column matches DEVICE=<device>.
 */
int
FSTableLookup_byDevice(io_service_t device, DiskVolumePtr diskVolume)
{
	boolean_t			match			= FALSE;
	const char*			prefix			= DEVICE_PREFIX;
	size_t				prefixLen		= strlen(prefix);
	int					result			= NO_MATCH_FOUND;
	fstab_cache_entry*	entry			= NULL;
	
	/*	Check our arguments. */
	if (!device || !diskVolume || diskVolume->mount_point) {
		return BAD_ARGUMENT;
	}
	
	/* Scan fstab */
	for (entry = fstabCache; entry != NULL; entry = entry->next) {
		CFDictionaryRef	description;
		CFDataRef		descriptionAsData;
		CFStringRef		descriptionAsString;

		/* If the prefix doesn't match the criteria, skip the entry. */
		if (strncmp(prefix, entry->fstab.fs_spec, prefixLen) != 0)
			continue;
			
		descriptionAsString = CFStringCreateWithCString(
		                      /* allocator */ kCFAllocatorDefault,
		                      /* string    */ entry->fstab.fs_spec + prefixLen,
		                      /* encoding  */ kCFStringEncodingMacRoman );

		if (descriptionAsString)
		{
			descriptionAsData = CFStringCreateExternalRepresentation(
			                    /* allocator */ kCFAllocatorDefault,
			                    /* string    */ descriptionAsString,
			                    /* encoding  */ kCFStringEncodingMacRoman,
			                    /* loss      */ 0 );

			if (descriptionAsData)
			{
				description = CFPropertyListCreateFromXMLData(
				              /* allocator */ kCFAllocatorDefault,
				              /* data      */ descriptionAsData,
				              /* options   */ kCFPropertyListImmutable,
				              /* error     */ NULL );

				if (description)
				{
					IOServiceMatchPropertyTable(device, description, &match);

					CFRelease(description);
				}

				CFRelease(descriptionAsData);
			}

			CFRelease(descriptionAsString);
		}

		/* If the device doesn't match the criteria, skip the entry. */
		if (match == FALSE)
			continue;

		/* If diskVolume has an fs_type that doesn't match the entry's, skip it. */
		if (diskVolume->fs_type && strcmp(diskVolume->fs_type, entry->fstab.fs_vfstype) != 0)
			continue;
			
		/* We have a match, so fill out the mount point. */
		if (getfsmntopt(&entry->fstab, "noauto"))
			diskVolume->mount_point	= strdup("");
		else
			diskVolume->mount_point	= strdup(entry->fstab.fs_file);

		/* We only treat ro file systems as non-writable. */
		if (!strcmp(entry->fstab.fs_type, FSTAB_RO))
			diskVolume->writable	= FALSE;
			
		/*	We found a match, so break out of the loop. */
		result = MATCH_FOUND;
		break;
	}

	return result;
}

/*
 *	Find an fstab entry whose first column matches <prefix><key>
 */
static int
fstabLookup(const char* prefix, const char* key, DiskVolumePtr diskVolume) 
{
	size_t				prefixLen		= strlen(prefix);
	int					result			= NO_MATCH_FOUND;
	fstab_cache_entry*	entry			= NULL;
	
	/*	Check our arguments. */
	if (!key || !diskVolume || diskVolume->mount_point) {
		return BAD_ARGUMENT;
	}
	
	/* Scan fstab */
	for (entry = fstabCache; entry != NULL; entry = entry->next) {
		/* If the prefix doesn't match the criteria, skip the entry. */
		if (strncmp(prefix, entry->fstab.fs_spec, prefixLen) != 0)
			continue;
			
		/* If the key doesn't match the criteria, skip the entry. */
		if (strcmp(key, entry->fstab.fs_spec + prefixLen) != 0)
			continue;
			
		/* If diskVolume has an fs_type that doesn't match the entry's, skip it. */
		if (diskVolume->fs_type && strcmp(diskVolume->fs_type, entry->fstab.fs_vfstype) != 0)
			continue;
			
		/* We have a match, so fill out the mount point. */
		if (getfsmntopt(&entry->fstab, "noauto"))
			diskVolume->mount_point	= strdup("");
		else
			diskVolume->mount_point	= strdup(entry->fstab.fs_file);

		/* We only treat ro file systems as non-writable. */
		if (!strcmp(entry->fstab.fs_type, FSTAB_RO))
			diskVolume->writable	= FALSE;
			
		/*	We found a match, so break out of the loop. */
		result = MATCH_FOUND;
		break;
	}

	return result;
}

static char *
getfsmntopt(struct fstab * fs_entry, char * fs_mntopt)
{
    #define	MAXLINELENGTH	1024

    char *cp, *p;
    char subline[MAXLINELENGTH];

    strcpy(subline, fs_entry->fs_mntops);
    p = subline;
    for (cp = strsep(&p, ","); cp;
         cp = strsep(&p, ",")) {
        if (!strcmp(cp, fs_mntopt)) {
            break;
        }
    }

    return cp;
}
