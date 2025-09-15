#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ext.h"


typedef struct {
    uint8_t* data;
    size_t size;
} fuzzing_input;


int checkfilesys (const char* fname, check_context* context);
int LLVMFuzzerInitialize (int* argc, char*** argv);
int LLVMFuzzerTestOneInput (uint8_t* data, size_t size);

check_context CTX = { 0 };
static int TOCTOU_MODE_SET = 0;

static size_t
ctx_rw (fuzzing_input* resource, void* buffer, size_t nbytes, off_t offset, bool isWrite) {
    if (offset >= resource->size) {
        return 0;
    }

    // Determine the number of bytes to copy, ensuring we don't go beyond the resource's size.
    size_t bytes_to_copy =
    (resource->size - offset < nbytes) ? (resource->size - offset) : nbytes;


    // Perform either read or write operation based on 'isWrite' flag.
    if (isWrite) {
        memcpy (resource->data + offset, buffer, bytes_to_copy);
    } else {
        memcpy (buffer, resource->data + offset, bytes_to_copy);
        if (TOCTOU_MODE_SET) {
            memset(resource->data + offset, 0xFF, bytes_to_copy);
        }
    }

    return bytes_to_copy;
}

static size_t ctx_read (fuzzing_input* resource, void* buffer, size_t nbytes, off_t offset) {
    return ctx_rw (resource, buffer, nbytes, offset, false);
}

static size_t ctx_write (fuzzing_input* resource, void* buffer, size_t nbytes, off_t offset) {
    return ctx_rw (resource, buffer, nbytes, offset, true);
}

static int ctx_fstat (fuzzing_input* resource, struct stat* buffer) {
    // Fail if resource is empty
    if (resource->size == 0) {
        return 1;
    }
    return 0;
}


int LLVMFuzzerInitialize (int* argc, char*** argv) {
    CTX.resource    = 0;
    CTX.readHelper  = (size_t (*)(void*, void*, size_t, off_t)) ctx_read;
    CTX.writeHelper = (size_t (*)(void*, void*, size_t, off_t)) ctx_write;
    CTX.fstatHelper = (int (*)(void *, struct stat *)) ctx_fstat;
    CTX.resource    = 0;
    return 0;
}


int LLVMFuzzerTestOneInput (uint8_t* data, size_t size) {
    fuzzing_input resource;

    // msdosfs actually writes back to the buffer for some reason ¯\_(ツ)_/¯
    uint8_t* writeable_data = calloc (1, size);
    memcpy (writeable_data, data, size);
    // Make sure we are not using the const data anymore
    data = (uint8_t*)0xdeadbeef;

    // Prepare resource for fuzzing
    resource.data = writeable_data;
    resource.size = size;
    CTX.resource  = (void*)&resource;
    // fuzz
    checkfilesys ((const char*)"/dev/disk", &CTX);
    // cleanup (might not even be needed)
    CTX.resource = 0;
    free (writeable_data);
    return 0;
}
