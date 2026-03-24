// Copyright (c) 2025 Apple Inc. All Rights Reserved.

#import <Foundation/Foundation.h>
#import <sys/mman.h>
#import <sys/stat.h>
#import <fcntl.h>
#import <unistd.h>
#import <security/pam_types.h>
#import <security/pam_appl.h>
#import <security/pam_modules.h>
#import "Logging.h"

#ifdef PAM_USE_OS_LOG
PAM_DEFINE_LOG(SharedMem)
#define PAM_LOG PAM_LOG_SharedMem()
#endif

#define PAM_SHM_PATH_PREFIX @"/pam"

const char *pam_shm_write(const void *data, size_t len) {
    // Check for NULL data or excessive length
    if (data == NULL || len == 0) {
        _LOG_ERROR("Invalid data or length");
        return NULL;
    }
    
    // Check for potential overflow and maximum size (consistent with existing PATH_MAX usage)
    if (len > PATH_MAX || len > SIZE_MAX - sizeof(uint32_t)) {
        _LOG_ERROR("Data too large for shared memory");
        return NULL;
    }
    
    NSUUID *uuid = [NSUUID UUID];
    NSString *segmentName = [[PAM_SHM_PATH_PREFIX stringByAppendingString:[[uuid.UUIDString componentsSeparatedByString:@"-"] componentsJoinedByString:@""]] substringToIndex:16];

    size_t totalSize = sizeof(uint32_t) + len;

    int fd = shm_open([segmentName UTF8String], O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        _LOG_ERROR("createSegmentWithName: createSegmentWithName shm_open %s", segmentName.UTF8String);
        return nil;
    }

    if (ftruncate(fd, totalSize) == -1) {
        _LOG_ERROR("createSegmentWithName: ftruncate");
        close(fd);
        shm_unlink([segmentName UTF8String]);
        return nil;
    }

    void *ptr = mmap(NULL, totalSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        _LOG_ERROR("createSegmentWithName: mmap");
        close(fd);
        shm_unlink([segmentName UTF8String]);
        return nil;
    }

    memcpy(ptr, &len, sizeof(uint32_t));
    memcpy((char *)ptr + sizeof(uint32_t), data, len);

    munmap(ptr, totalSize);
    close(fd);
    _LOG_DEBUG("Data written to SHM");

    return segmentName.UTF8String;
}

ssize_t pam_shm_read(const char *segName, void *buf, size_t bufLen) {
    if (segName == NULL) {
        _LOG_ERROR("readSegmentWithName: NULL segment name");
        return -1;
    }
    
    int fd = shm_open(segName, O_RDONLY, 0);
    if (fd == -1) {
        _LOG_ERROR("readSegmentWithName: shm_open");
        return -1;
    }

    uint32_t len;
    void *ptr = mmap(NULL, sizeof(uint32_t), PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        _LOG_ERROR("readSegmentWithName: mmap");
        close(fd);
        return -1;
    }
    memcpy(&len, ptr, sizeof(uint32_t));
    munmap(ptr, sizeof(uint32_t));
    
    // Check for excessive size (consistent with existing PATH_MAX usage)
    if (len > PATH_MAX) {
        _LOG_ERROR("readSegmentWithName: excessive data length %u", len);
        close(fd);
        return -1;
    }
    
    // Check for potential overflow
    if (len > SIZE_MAX - sizeof(uint32_t)) {
        _LOG_ERROR("readSegmentWithName: potential integer overflow");
        close(fd);
        return -1;
    }
    
    size_t totalSize = sizeof(uint32_t) + len;
    ptr = mmap(NULL, totalSize, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        _LOG_ERROR("readSegmentWithName: mmap");
        close(fd);
        return -1;
    }

    // Get pointer to the actual data (skip the length prefix)
    const char *dataPtr = (const char *)ptr + sizeof(uint32_t);

    // If buf is NULL, caller just wants the data length
    if (!buf) {
        munmap(ptr, totalSize);
        close(fd);
        _LOG_DEBUG("Data length read from SHM");
        return (ssize_t)len;
    }

    // Copy data directly to the output buffer
    size_t toCopy = MIN(bufLen, len);
    memcpy(buf, dataPtr, toCopy);

    munmap(ptr, totalSize);
    close(fd);
    _LOG_DEBUG("Data read from SHM");

    return (ssize_t)toCopy;
}

void pam_shm_cleanup(const char *segName) {
    shm_unlink(segName);
    _LOG_DEBUG("Unlinked SHM");
}

char *pam_copy_secret_from_shm(struct pam_handle *pamh, const char *env_name) {
    char *retval = NULL;
    const char *shm_handle = pam_getenv(pamh, env_name);
    if (NULL == shm_handle) {
        _LOG_ERROR("Unable to retrieve the ENV value %s.", env_name);
        goto fin;
    }
    
    size_t secret_len = pam_shm_read(shm_handle, NULL, 0);
    if (secret_len == 0) {
        _LOG_ERROR("Unable to retrieve the secret length.");
        goto fin;
    }
    
    if (secret_len > PATH_MAX) {
        _LOG_ERROR("Wrong secret len %zu.", secret_len);
        goto fin;
    }
    
    retval = malloc(secret_len + 1);
    if (retval == NULL) {
        _LOG_ERROR("Failed to allocate memory for secret.");
        goto fin;
    }
    
    ssize_t read_bytes = pam_shm_read(shm_handle, retval, secret_len);
    if (read_bytes < 0 || (size_t)read_bytes != secret_len) {
        _LOG_ERROR("Failed to read secret from shared memory.");
        free(retval);
        retval = NULL;
        goto fin;
    }
    
    retval[secret_len] = '\0';
    
    _LOG_DEBUG("Read secret");

fin:
    if (shm_handle) {
        pam_shm_cleanup(shm_handle);
    }
    return retval;
}

char *pam_copy_secret_with_fallback(struct pam_handle *pamh, const char *original_env_name, const char *shm_env_name) {
    char *retval = NULL;
    
    // First try to get the secret directly from the original PAM environment (backward compatibility)
    const char *direct_secret = pam_getenv(pamh, original_env_name);
    if (direct_secret != NULL) {
        _LOG_DEBUG("Found secret in original PAM environment %s", original_env_name);
        retval = strdup(direct_secret);
        return retval;
    }
    
    // Fall back to shared memory approach using the new environment variable
    _LOG_DEBUG("Original PAM environment %s not found, trying shared memory approach with %s", original_env_name, shm_env_name);
    retval = pam_copy_secret_from_shm(pamh, shm_env_name);
    
    return retval;
}

void pam_safe_string_release(char *str) {
    if (!str) {
        return;
    }
    size_t len = strlen(str);
    memset_s(str, len, 0, len);
    free(str);
}
