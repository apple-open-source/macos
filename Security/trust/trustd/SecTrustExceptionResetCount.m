//
//  SecTrustExceptionResetCount.m
//  Security_ios
//

#import <Foundation/Foundation.h>
#import "SecTrustExceptionResetCount.h"

#import <utilities/SecCFWrappers.h>
#import <utilities/SecFileLocations.h>

static NSString *kExceptionResetCountKey = @"ExceptionResetCount";
static NSString *exceptionResetCounterFile = @"com.apple.security.exception_reset_counter.plist";

/* Returns the path to the, existing or internally-created, 'exceptionResetCounterFile' file. */
static NSString *SecPlistFileExistsInKeychainDirectory(CFErrorRef *error) {
    NSString *status = NULL;

    NSString *path = [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)exceptionResetCounterFile) path];
    if (!path) {
        secerror("Unable to address permanent storage for '%{public}@'.", exceptionResetCounterFile);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENOENT, NULL);
        }
        return status;
    }
    secinfo("trust", "'%{public}@' is at '%{public}@'.", exceptionResetCounterFile, path);

    NSFileManager *fm = [NSFileManager defaultManager];
    if (!fm) {
        secerror("Failed to initialize the file manager in '%{public}s'.", __func__);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENOMEM, NULL);
        }
        return status;
    }

    BOOL isDir = false;
    bool fileExists = [fm fileExistsAtPath:path isDirectory:&isDir];
    if (isDir) {
        secerror("'%{public}@' is a directory. (not a file)", path);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EISDIR, NULL);
        }
        return status;
    }
    if (fileExists) {
        secdebug("trust", "'%{public}@' already exists.", path);
        status = path;
        return status;
    }

    if (![fm createFileAtPath:path contents:nil attributes:nil]) {
        secerror("Failed to create permanent storage at '%{public}@'.", path);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EIO, NULL);
        }
        return status;
    }
    secinfo("trust", "'%{public}@' has been created.", path);
    status = path;

    return status;
}

static uint64_t SecReadPlistFromFileInKeychainDirectory(CFErrorRef *error) {
    uint64_t value = 0;

    CFErrorRef localError = NULL;
    NSString *path = SecPlistFileExistsInKeychainDirectory(&localError);
    if (localError) {
        if (error) {
            *error = localError;
        }
        secerror("Permanent storage for the exceptions epoch is unavailable.");
        return value;
    }
    if (!path) {
        secinfo("trust", "Permanent storage for the exceptions epoch is missing. Defaulting to value %llu.", value);
        return value;
    }

    NSMutableDictionary *plDict = [NSMutableDictionary dictionaryWithContentsOfFile:path];
    if (!plDict) {
        secerror("Failed to read from permanent storage at '%{public}@' or the data is bad. Defaulting to value %llu.", path, value);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENXIO, NULL);
        }
        return value;
    }

    id valueObject = [plDict objectForKey:kExceptionResetCountKey];
    if (!valueObject) {
        secinfo("trust", "Could not find key '%{public}@'. Defaulting to value %llu.", kExceptionResetCountKey, value);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENXIO, NULL);
        }
        return value;
    }
    if (![valueObject isKindOfClass:[NSNumber class]]) {
        secerror("The value for key '%{public}@' is not a number.", kExceptionResetCountKey);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EDOM, NULL);
        }
        return value;
    }

    value = [valueObject unsignedIntValue];

    secinfo("trust", "'%{public}@' is %llu.", kExceptionResetCountKey, value);
    return value;
}

static bool SecWritePlistToFileInKeychainDirectory(uint64_t exceptionResetCount, CFErrorRef *error) {
    bool status = false;

    CFErrorRef localError = NULL;
    SecPlistFileExistsInKeychainDirectory(&localError);
    if (localError) {
        if (error) {
            *error = localError;
        }
        secerror("Permanent storage for the exceptions epoch is unavailable.");
        return status;
    }

    NSString *path = [(__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)exceptionResetCounterFile) path];
    if (!path) {
        secerror("Unable to address permanent storage for '%{public}@'.", exceptionResetCounterFile);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EIO, NULL);
        }
        return status;
    }

    NSMutableDictionary *dataToSave = [NSMutableDictionary new];
    if (!dataToSave) {
        secerror("Failed to allocate memory for the exceptions epoch structure.");
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ENOMEM, NULL);
        }
        return status;
    }
    dataToSave[@"Version"] = [NSNumber numberWithUnsignedInteger:1];
    dataToSave[kExceptionResetCountKey] = [NSNumber numberWithUnsignedInteger:exceptionResetCount];

    status = [dataToSave writeToFile:path atomically:YES];
    if (!status) {
        secerror("Failed to write to permanent storage at '%{public}@'.", path);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, EIO, NULL);
        }
        return status;
    }

    secinfo("trust", "'%{public}@' has been committed to permanent storage at '%{public}@'.", kExceptionResetCountKey, path);
    return status;
}

uint64_t SecTrustServerGetExceptionResetCount(CFErrorRef *error) {
    CFErrorRef localError = NULL;
    uint64_t exceptionResetCount = SecReadPlistFromFileInKeychainDirectory(&localError);
    /* Treat ENXIO as a transient error; I/O seems to be working but we have failed to read the current epoch.
     * That's expected when epoch is still 0 and there is nothing to store in permanent storage. (and later read)
     */
    if (localError && CFEqualSafe(CFErrorGetDomain(localError), kCFErrorDomainPOSIX) && CFErrorGetCode(localError) == ENXIO) {
        CFRelease(localError);
        localError = NULL;
    }
    if (error && localError) {
        *error = localError;
    }
    secinfo("trust", "exceptionResetCount: %llu (%s)", exceptionResetCount, error ? (*error ? "Error" : "OK") : "N/A");
    return exceptionResetCount;
}

bool SecTrustServerIncrementExceptionResetCount(CFErrorRef *error) {
    bool status = false;

    uint64_t currentExceptionResetCount = SecTrustServerGetExceptionResetCount(error);
    if (error && *error) {
        secerror("Failed to increment the extensions epoch.");
        return status;
    }
    if (currentExceptionResetCount >= INT64_MAX) {
        secerror("Current exceptions epoch value is too large. (%llu) Won't increment.", currentExceptionResetCount);
        if (error) {
            *error = CFErrorCreate(NULL, kCFErrorDomainPOSIX, ERANGE, NULL);
        }
        return status;
    }

    return SecWritePlistToFileInKeychainDirectory(currentExceptionResetCount + 1, error);
}
