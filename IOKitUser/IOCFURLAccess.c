#include <IOKit/IOCFURLAccess.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>
#include <fcntl.h>

#include "IOKitInternal.h"

#define kIOFileURLExists	CFSTR("kIOFileURLExists")
#define kIOFileURLPOSIXMode	CFSTR("kIOFileURLPOSIXMode")
#define kIOFileURLSize		CFSTR("kIOFileURLSize")

#define CF_OPENFLGS (0)
#define thread_set_errno(V) do {errno = (V);} while (0)

static Boolean _IOFileURLCreateDataAndPropertiesFromResource(CFAllocatorRef alloc, CFURLRef url, CFDataRef *fetchedData, CFArrayRef desiredProperties, CFDictionaryRef *fetchedProperties, SInt32 *errorCode);
static CFDictionaryRef _IOFileURLCreatePropertiesFromResource(CFAllocatorRef alloc, CFURLRef url, CFArrayRef desiredProperties, SInt32 *errorCode);
static Boolean _IOFileURLWritePropertiesToResource(CFURLRef url, CFDictionaryRef propertyDict, SInt32 *errorCode);

static CFMutableArrayRef _IOContentsOfDirectory(CFAllocatorRef alloc, char *path, CFURLRef base, CFStringRef matchingAbstractType);
extern Boolean _IOReadBytesFromFile(CFAllocatorRef alloc, const char *path, void **bytes, CFIndex *length, CFIndex maxLength);
extern Boolean _IOWriteBytesToFile(const char *path, const void *bytes, CFIndex length);

CFTypeRef IOURLCreatePropertyFromResource(CFAllocatorRef alloc, CFURLRef url, CFStringRef property, SInt32 *errorCode) {
    CFArrayRef array = CFArrayCreate(alloc, (const void **)&property, 1, &kCFTypeArrayCallBacks);
    CFDictionaryRef dict;

    if (IOURLCreateDataAndPropertiesFromResource(alloc, url, NULL, &dict, array, errorCode)) {
        CFTypeRef result = CFDictionaryGetValue(dict, property);
        if (result) CFRetain(result);
        CFRelease(array);
        CFRelease(dict);
        return result;
    } else {
        if (dict) CFRelease(dict);
        CFRelease(array);
        return NULL;
    }
}

Boolean IOURLCreateDataAndPropertiesFromResource(CFAllocatorRef alloc, CFURLRef url, CFDataRef *fetchedData, CFDictionaryRef *fetchedProperties, CFArrayRef desiredProperties, SInt32 *errorCode) {

    CFStringRef scheme = CFURLCopyScheme(url);

    if (!scheme) {
        if (errorCode) *errorCode = kIOURLImproperArgumentsError;
        if (fetchedData) *fetchedData = NULL;
        if (fetchedProperties) *fetchedProperties = NULL;
        return FALSE;
    } else {
        Boolean result;
        if (CFStringCompare(scheme, CFSTR("file"), 0) == kCFCompareEqualTo) {
            result = _IOFileURLCreateDataAndPropertiesFromResource(alloc, url, fetchedData, desiredProperties, fetchedProperties, errorCode);
        } else {
            if (fetchedData) *fetchedData = NULL;
            if (fetchedProperties) *fetchedProperties = NULL;
            if (errorCode) *errorCode = kIOURLUnknownSchemeError;
            result = FALSE;
        }
        CFRelease(scheme);
        return result;
    }
}

static Boolean _IOFileURLCreateDataAndPropertiesFromResource(CFAllocatorRef alloc, CFURLRef url, CFDataRef *fetchedData, CFArrayRef desiredProperties, CFDictionaryRef *fetchedProperties, SInt32 *errorCode) {
    char buffer[CFMaxPathSize];
    Boolean success = TRUE;

    if (!CFURLGetFileSystemRepresentation(url, TRUE, buffer, CFMaxPathSize)) {
        if (fetchedData) *fetchedData = NULL;
        if (fetchedProperties) *fetchedProperties = NULL;
        if (errorCode) *errorCode = kIOURLImproperArgumentsError;
        return FALSE;
    }

    if (errorCode) *errorCode = 0;
    if (fetchedData) {
        void *bytes;
        CFIndex length;
        Boolean releaseAlloc = FALSE;

        if (alloc == NULL) {
            // We need a real allocator to pass to _IOReadBytesFromFile
            alloc = CFRetain(CFAllocatorGetDefault());
            releaseAlloc = TRUE;
        }
        if (!_IOReadBytesFromFile(alloc, buffer, &bytes, &length, 0)) {
            if (errorCode) *errorCode = kIOURLUnknownError;
            *fetchedData = NULL;
            success = FALSE;
        } else {
            *fetchedData = CFDataCreateWithBytesNoCopy(alloc, bytes, length, alloc);
        }
        if (releaseAlloc) {
            // Now the CFData should be hanging on to it.
            CFRelease(alloc);
        }
    }

    if (fetchedProperties) {
        *fetchedProperties = _IOFileURLCreatePropertiesFromResource(alloc, url, desiredProperties, errorCode);
        if (!*fetchedProperties) success = FALSE;
    }

    return success;
}

Boolean IOURLWriteDataAndPropertiesToResource(CFURLRef url, CFDataRef data, CFDictionaryRef propertyDict, SInt32 *errorCode) {
    CFStringRef scheme = CFURLCopyScheme(url);
    if (!scheme) {
        if (errorCode) *errorCode = kIOURLImproperArgumentsError;
        return FALSE;
    } else if (CFStringCompare(scheme, CFSTR("file"), 0) == kCFCompareEqualTo) {
        Boolean success = TRUE;
        CFRelease(scheme);
        if (errorCode) *errorCode = 0;
        if (data) {
            char cPath[CFMaxPathSize];
            if (!CFURLGetFileSystemRepresentation(url, TRUE, cPath, CFMaxPathSize)) {
                if (errorCode) *errorCode = kIOURLImproperArgumentsError;
                success = FALSE;
            } else if (CFURLHasDirectoryPath(url)) {
                // Create a directory
                success = !mkdir(cPath, 0777);
                if (!success && errorCode) *errorCode = kIOURLUnknownError;
            } else {
               // Write data
                SInt32 length = CFDataGetLength(data);
                const void *bytes = (0 == length) ? (const void *)"" : CFDataGetBytePtr(data);
                success = _IOWriteBytesToFile(cPath, bytes, length);
                if (!success && errorCode) *errorCode = kIOURLUnknownError;
            }
        }
        if (propertyDict) {
            if (!_IOFileURLWritePropertiesToResource(url, propertyDict, errorCode))
                success = FALSE;
        }
        return success;
    } else {
        if (errorCode) *errorCode = kIOURLUnknownSchemeError;
        return FALSE;
    }
}

static Boolean _IOFileURLWritePropertiesToResource(CFURLRef url, CFDictionaryRef propertyDict, SInt32 *errorCode) {
    CFTypeRef buffer[16];
    void **keys;
    void **values;
    Boolean result = TRUE;
    SInt32 index, count;
    char cPath[CFMaxPathSize];

    if (!CFURLGetFileSystemRepresentation(url, TRUE, cPath, CFMaxPathSize)) {
        if (errorCode) *errorCode = kIOURLImproperArgumentsError;
        return FALSE;
    }

    count = CFDictionaryGetCount(propertyDict);
    if (count < 8) {
        keys = buffer;
        values = buffer+8;
    } else {
        keys = CFAllocatorAllocate(CFGetAllocator(url), sizeof(void *) * count * 2, 0);
        values = keys + count;
    }
    CFDictionaryGetKeysAndValues(propertyDict, keys, values);

    for (index = 0; index < count; index ++) {
        CFStringRef key = keys[index];
        CFTypeRef value = values[index];
        if (CFEqual(key, kIOFileURLPOSIXMode) || CFEqual(key, kIOURLFilePOSIXMode)) {
            SInt32 mode;
            int err;
            if (CFEqual(key, kIOURLFilePOSIXMode)) {
                CFNumberRef modeNum = (CFNumberRef)value;
                CFNumberGetValue(modeNum, kCFNumberSInt32Type, &mode);
            } else {
                const mode_t *modePtr = (const mode_t *)CFDataGetBytePtr((CFDataRef)value);
                mode = *modePtr;
            }
            err = chmod(cPath, mode);
            if (err != 0) result = FALSE;
        } else {
            result = FALSE;
        }
    }

    if (keys != buffer) CFAllocatorDeallocate(CFGetAllocator(url), keys);

    if (errorCode) *errorCode = result ? 0 : kIOURLUnknownError;
    return result;
}

static CFDictionaryRef _IOFileURLCreatePropertiesFromResource(CFAllocatorRef alloc, CFURLRef url, CFArrayRef desiredProperties, SInt32 *errorCode) {
    static CFArrayRef _allProps = NULL;
    char cPath[CFMaxPathSize];
    SInt32 index, count, statResult = 0;
    CFMutableDictionaryRef propertyDict = NULL;
    struct stat statBuf;
    Boolean statCompleted = FALSE;

    if (!CFURLGetFileSystemRepresentation(url, TRUE, cPath, CFMaxPathSize)) {
        if (errorCode) *errorCode = kIOURLImproperArgumentsError;
        return NULL;
    }
    if (errorCode) *errorCode = 0;
    if (!desiredProperties) {
        // Cheap and dirty hack to make this work for the moment; ultimately we need to do something more sophisticated.  This will result in an error return whenever a property key is defined which isn't applicable to all file URLs.  REW, 3/2/99
        if (!_allProps) {
            const void *values[9];
            values[0] = kIOURLFileExists;
            values[1] = kIOURLFilePOSIXMode;
            values[2] = kIOURLFileDirectoryContents;
            values[3] = kIOURLFileLength;
            values[4] = kIOURLFileLastModificationTime;
            values[5] = kIOURLFileOwnerID;
            values[6] = kIOFileURLExists;
            values[7] = kIOFileURLPOSIXMode;
            values[8] = kIOFileURLSize;
            _allProps = CFArrayCreate(NULL, values, 8, &kCFTypeArrayCallBacks);
        }
        desiredProperties = _allProps;
    }

    count = CFArrayGetCount(desiredProperties);
    propertyDict = CFDictionaryCreateMutable(alloc, 0, & kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks);
    if (count == 0) return propertyDict;
    for (index = 0; index < count; index ++) {
        CFStringRef key = (CFMutableStringRef )CFArrayGetValueAtIndex(desiredProperties, index);
        if (!statCompleted && (CFEqual(key, kIOURLFilePOSIXMode) || CFEqual(key, kIOURLFileDirectoryContents) || CFEqual(key, kIOURLFileLength) ||  CFEqual(key, kIOURLFileLastModificationTime) || CFEqual(key, kIOURLFileExists) || CFEqual(key, kIOFileURLExists) || CFEqual(key, kIOFileURLPOSIXMode) || CFEqual(key, kIOFileURLSize) || CFEqual(key, kIOURLFileOwnerID))) {
            statResult = stat(cPath, &statBuf);
            if (statResult != 0) statResult = thread_errno();
            statCompleted = TRUE;
        } else if (errorCode) {
            *errorCode = kIOURLUnknownError;
        }
        if (CFEqual(key, kIOFileURLPOSIXMode)) {
            if (statResult == 0) {
                CFDataRef modeData = CFDataCreate(alloc, (void *)(&(statBuf.st_mode)), sizeof(statBuf.st_mode));
                CFDictionarySetValue(propertyDict, kIOFileURLPOSIXMode, modeData);
                CFRelease(modeData);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOURLFilePOSIXMode)) {
            if (statResult == 0) {
                SInt32 value = statBuf.st_mode;
                CFNumberRef num = CFNumberCreate(alloc, kCFNumberSInt32Type, &value);
                CFDictionarySetValue(propertyDict, kIOURLFilePOSIXMode, num);
                CFRelease(num);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOURLFileDirectoryContents)) {
            if (statResult == 0 && (statBuf.st_mode & S_IFMT) == S_IFDIR) {
                CFMutableArrayRef contents = _IOContentsOfDirectory(alloc, cPath, url, NULL);
                if (contents) {
                    CFDictionarySetValue(propertyDict, kIOURLFileDirectoryContents, contents);
                    CFRelease(contents);
                } else if (errorCode) {
                    *errorCode = kIOURLUnknownError;
                }
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOFileURLSize)) {
            if (statResult == 0) {
                UInt64 length = statBuf.st_size;
                CFDataRef tmpData = CFDataCreate(alloc, (void *)(&length), sizeof(UInt64));
                CFDictionarySetValue(propertyDict, kIOFileURLSize, tmpData);
                CFRelease(tmpData);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOURLFileLength)) {
            if (statResult == 0) {
                SInt64 length = statBuf.st_size;
                CFNumberRef num = CFNumberCreate(alloc, kCFNumberSInt64Type, &length);
                CFDictionarySetValue(propertyDict, kIOURLFileLength, num);
                CFRelease(num);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOURLFileLastModificationTime)) {
            if (statResult == 0) {
                CFDateRef date = CFDateCreate(alloc, statBuf.st_mtime - kCFAbsoluteTimeIntervalSince1970);
                CFDictionarySetValue(propertyDict, kIOURLFileLastModificationTime, date);
                CFRelease(date);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOURLFileExists)) {
            if (statResult == 0) {
                CFDictionarySetValue(propertyDict, kIOURLFileExists, kCFBooleanTrue);
            } else if (statResult == ENOENT) {
                CFDictionarySetValue(propertyDict, kIOURLFileExists, kCFBooleanFalse);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOFileURLExists)) {
            if (statResult == 0) {
                CFDictionarySetValue(propertyDict, kIOFileURLExists, kIOFileURLExists);
            } else if (statResult == ENOENT) {
                CFDictionarySetValue(propertyDict, kIOFileURLExists, kCFBooleanFalse); // Choose any value other than kIOFileURLExists to designate non-existance.  Note that we cannot use NULL, since the dictionary has value callbacks kCFTypeDictionaryValueCallBacks, which is not tolerant of NULL values.
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        } else if (CFEqual(key, kIOURLFileOwnerID)) {
            if (statResult == 0) {
                SInt32 uid = statBuf.st_uid;
                CFNumberRef num  = CFNumberCreate(alloc, kCFNumberSInt32Type, &uid);
                CFDictionarySetValue(propertyDict, kIOURLFileOwnerID, num);
                CFRelease(num);
            } else if (errorCode) {
                *errorCode = kIOURLUnknownError;
            }
        // Add more properties here
        } else if (errorCode) {
            *errorCode = kIOURLUnknownPropertyKeyError;
        }
    }
    return propertyDict;
}


// File Utilities

CFMutableArrayRef _IOContentsOfDirectory(CFAllocatorRef alloc, char *path, CFURLRef base, CFStringRef matchingAbstractType) {
    // NOTE - we assume path points to a buffer of at least length CFMaxPathLength, and we do potentially write in to it!
    CFMutableArrayRef files;
    Boolean releaseBase = FALSE;
    CFIndex pathLength = strlen(path);
    // MF:!!! Need to use four-letter type codes where appropriate.
    CFStringRef extension = (matchingAbstractType ? CFRetain(matchingAbstractType) : NULL);
    CFIndex extLen = (extension ? CFStringGetLength(extension) : 0);
    char extBuff[CFMaxPathSize];

    int fd, numread;
    long basep;
    char dirge[8192];

    if (extLen > 0) {
        CFStringGetBytes(extension, CFRangeMake(0, extLen), kCFStringEncodingMacRoman, 0, FALSE, extBuff, CFMaxPathSize, &extLen);
        extBuff[extLen] = '\0';
    }
    
    fd = open(path, O_RDONLY, 0777);
    if (fd < 0) {
        if (extension) {
            CFRelease(extension);
        }
        return NULL;
    }
    files = CFArrayCreateMutable(alloc, 0, &kCFTypeArrayCallBacks);

    while ((numread = getdirentries(fd, dirge, sizeof(dirge), &basep)) > 0) {
        struct dirent *dent;
        for (dent = (struct dirent *)dirge; dent < (struct dirent *)(dirge + numread); dent = (struct dirent *)((char *)dent + dent->d_reclen)) {
            CFURLRef fileURL;
            CFIndex nameLen;

            nameLen = dent->d_namlen;
            // skip . & ..; they cause descenders to go berserk
            if (0 == dent->d_fileno || (dent->d_name[0] == '.' && (nameLen == 1 || (nameLen == 2 && dent->d_name[1] == '.')))) {
                continue;
            }
            if (extLen > 0) {
                // Check to see if it matches the extension we're looking for.
                if (strncmp(&(dent->d_name[nameLen - extLen]), extBuff, extLen) != 0) {
                    continue;
                }
            }
            if (base == NULL) {
                base = CFURLCreateFromFileSystemRepresentation(alloc, path, pathLength, TRUE);
                releaseBase = TRUE;
            }

            if (dent->d_type == DT_DIR || dent->d_type == DT_UNKNOWN) {
                Boolean isDir = (dent->d_type == DT_DIR);
                if (!isDir) {
                    // Ugh; must stat.
                    char subdirPath[CFMaxPathLength];
                    struct stat statBuf;
                    strncpy(subdirPath, path, pathLength);
                    subdirPath[pathLength] = '/';
                    strncpy(subdirPath + pathLength + 1, dent->d_name, nameLen);
                    subdirPath[pathLength + nameLen + 1] = '\0';
                    if (stat(subdirPath, &statBuf) == 0) {
                        isDir = ((statBuf.st_mode & S_IFMT) == S_IFDIR);
                    }
                }
                fileURL = CFURLCreateFromFileSystemRepresentationRelativeToBase(alloc, dent->d_name, nameLen, isDir, base);
            } else {
                fileURL = CFURLCreateFromFileSystemRepresentationRelativeToBase (alloc, dent->d_name, nameLen, FALSE, base);
            }
            CFArrayAppendValue(files, fileURL);
            CFRelease(fileURL);
        }
    }
    close(fd);
    if  (-1 == numread) {
        CFRelease(files);
        if (releaseBase) {
            CFRelease(base);
        }
        if (extension) {
            CFRelease(extension);
        }
        return NULL;
    }

    if (extension) {
        CFRelease(extension);
    }
    if (releaseBase) {
        CFRelease(base);
    }
    return files;
}

SInt32 _IOGetFileProperties(CFAllocatorRef alloc, CFURLRef pathURL, Boolean *exists, SInt32 *posixMode, SInt64 *size, CFDateRef *modTime, SInt32 *ownerID, CFArrayRef *dirContents) {
    Boolean fileExists;
    Boolean isDirectory = FALSE;
    struct stat statBuf;
    char path[CFMaxPathLength];

    if ((exists == NULL) && (posixMode == NULL) && (size == NULL) && (modTime == NULL) && (ownerID == NULL) && (dirContents == NULL)) {
        // Nothing to do.
        return 0;
    }

    if (!CFURLGetFileSystemRepresentation(pathURL, TRUE, path, CFMaxPathLength)) {
        return -1;
    }

    if (stat(path, &statBuf) != 0) {
        // stat failed, but why?
        if (thread_errno() == ENOENT) {
            fileExists = FALSE;
        } else {
            return thread_errno();
        }
    } else {
        fileExists = TRUE;
        isDirectory = ((statBuf.st_mode & S_IFMT) == S_IFDIR);
    }

    if (exists != NULL) {
        *exists = fileExists;
    }

    if (posixMode != NULL) {
        if (fileExists) {
            *posixMode = statBuf.st_mode;
        } else {
            *posixMode = 0;
        }
    }

    if (size != NULL) {
        if (fileExists) {
            *size = statBuf.st_size;
        } else {
            *size = 0;
        }
    }

    if (modTime != NULL) {
        if (fileExists) {
            CFTimeInterval theTime;
            theTime = kCFAbsoluteTimeIntervalSince1970 + statBuf.st_mtime;
            *modTime = CFDateCreate(alloc, theTime);
        } else {
            *modTime = NULL;
        }
    }

    if (ownerID != NULL) {
        if (fileExists) {
            *ownerID = statBuf.st_uid;
        } else {
            *ownerID = -1;
        }
    }
    
    if (dirContents != NULL) {
        if (fileExists && isDirectory) {
            CFMutableArrayRef contents = _IOContentsOfDirectory(alloc, path, pathURL, NULL);
            if (contents) {
                *dirContents = contents;
            } else {
                *dirContents = NULL;
            }
        } else {
            *dirContents = NULL;
        }
    }
    return 0;
}

Boolean _IOReadBytesFromFile(CFAllocatorRef alloc, const char *path, void **bytes, CFIndex *length, CFIndex maxLength) {
    // alloc must be a valid allocator.
    // maxLength is the number of bytes desired, or 0 if the whole file is desired regardless of length.
    struct stat statBuf;
    int fd = -1;

    if (!alloc) {
        // MF:!!! This is no good.  This function needs to require a non-NULL allocator.  We should probably log or assert or something.
        return FALSE;
    }

    *bytes = NULL;
    fd = open(path, O_RDONLY|CF_OPENFLGS, 0666);
    if (fd < 0) {
        return FALSE;
    }
    if (fstat(fd, &statBuf) < 0) {
        int saveerr = thread_errno();
        close(fd);
        thread_set_errno(saveerr);
        return FALSE;
    }
    if ((statBuf.st_mode & S_IFMT) != S_IFREG) {
        close(fd);
        thread_set_errno(EACCES);
        return FALSE;
    }
    if (statBuf.st_size == 0) {
        *bytes = CFAllocatorAllocate(alloc, 4, 0); // don't return constant string -- it's freed!
        *length = 0;
    } else {
        CFIndex desiredLength;
        if ((maxLength >= statBuf.st_size) || (maxLength == 0)) {
            desiredLength = statBuf.st_size;
        } else {
            desiredLength = maxLength;
        }
        *bytes = CFAllocatorAllocate(alloc, desiredLength, 0);
        if (read(fd, *bytes, desiredLength) < 0) {
            CFAllocatorDeallocate(alloc, *bytes);
            close(fd);
            return FALSE;
        }
        *length = desiredLength;
    }
    close(fd);
    return TRUE;
}

Boolean _IOWriteBytesToFile(const char *path, const void *bytes, CFIndex length) {
    struct stat statBuf;
    int fd = -1;
    int mode, mask;

    mask = umask(0);
    umask(mask);
    mode = 0666 & ~mask;
    if (0 == stat(path, &statBuf)) {
        mode = statBuf.st_mode;
    } else if (thread_errno() != ENOENT) {
        return FALSE;
    }
    fd = open(path, O_WRONLY|O_CREAT|O_TRUNC|CF_OPENFLGS, 0666);
    if (fd < 0) {
        return FALSE;
    }
    if (length && write(fd, bytes, length) != length) {
        int saveerr = thread_errno();
        close(fd);
        thread_set_errno(saveerr);
        return FALSE;
    }
    fsync(fd);
    close(fd);
    return TRUE;
}
