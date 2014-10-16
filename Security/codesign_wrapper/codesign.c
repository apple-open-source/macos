
#include "codesign.h"

#include <stdint.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <CommonCrypto/CommonDigest.h>

#define DEBUG_ASSERT_PRODUCTION_CODE 0
#include <AssertMacros.h>

/*
 * Magic numbers used by Code Signing
 */
enum {
	CSMAGIC_REQUIREMENT	= 0xfade0c00,		/* single Requirement blob */
	CSMAGIC_REQUIREMENTS = 0xfade0c01,		/* Requirements vector (internal requirements) */
	CSMAGIC_CODEDIRECTORY = 0xfade0c02,		/* CodeDirectory blob */
	CSMAGIC_EMBEDDED_SIGNATURE = 0xfade0cc0, /* embedded form of signature data */
	CSMAGIC_DETACHED_SIGNATURE = 0xfade0cc1, /* multi-arch collection of embedded signatures */

	CSSLOT_CODEDIRECTORY = 0,				/* slot index for CodeDirectory */
};


/*
 * Structure of an embedded-signature SuperBlob
 */
typedef struct __BlobIndex {
	uint32_t type;					/* type of entry */
	uint32_t offset;				/* offset of entry */
} CS_BlobIndex;

typedef struct __SuperBlob {
	uint32_t magic;					/* magic number */
	uint32_t length;				/* total length of SuperBlob */
	uint32_t count;					/* number of index entries following */
	CS_BlobIndex index[];			/* (count) entries */
	/* followed by Blobs in no particular order as indicated by offsets in index */
} CS_SuperBlob;


/*
 * C form of a CodeDirectory.
 */
typedef struct __CodeDirectory {
	uint32_t magic;					/* magic number (CSMAGIC_CODEDIRECTORY) */
	uint32_t length;				/* total length of CodeDirectory blob */
	uint32_t version;				/* compatibility version */
	uint32_t flags;					/* setup and mode flags */
	uint32_t hashOffset;			/* offset of hash slot element at index zero */
	uint32_t identOffset;			/* offset of identifier string */
	uint32_t nSpecialSlots;			/* number of special hash slots */
	uint32_t nCodeSlots;			/* number of ordinary (code) hash slots */
	uint32_t codeLimit;				/* limit to main image signature range */
	uint8_t hashSize;				/* size of each hash in bytes */
	uint8_t hashType;				/* type of hash (cdHashType* constants) */
	uint8_t spare1;					/* unused (must be zero) */
	uint8_t	pageSize;				/* log2(page size in bytes); 0 => infinite */
	uint32_t spare2;				/* unused (must be zero) */
	/* followed by dynamic content as located by offset fields above */
} CS_CodeDirectory;


	//assert(page < ntohl(cd->nCodeSlots));
	//return base + ntohl(cd->hashOffset) + page * 20;


static CFMutableDictionaryRef lc_code_sig(uint8_t *lc_code_signature, size_t lc_code_signature_len)
{
    CFDataRef codedir = NULL;
    CFDataRef message = NULL;
    CFMutableDictionaryRef code_signature =
        CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                &kCFTypeDictionaryKeyCallBacks,
                &kCFTypeDictionaryValueCallBacks);
    require(code_signature, out);

    CS_SuperBlob *sb = (CS_SuperBlob*)lc_code_signature;
    require(ntohl(sb->magic) == CSMAGIC_EMBEDDED_SIGNATURE, out);
    uint32_t count;
    for (count = 0; count < ntohl(sb->count); count++) {
        //uint32_t type = ntohl(sb->index[count].type);
        uint32_t offset = ntohl(sb->index[count].offset);
        uint8_t *bytes = lc_code_signature + offset;
        //fprintf(stderr, "blob[%d]: (type: 0x%.08x, offset: %p)\n", count, type, (void*)offset);
        uint32_t magic = ntohl(*(uint32_t*)bytes);
        uint32_t length = ntohl(*(uint32_t*)(bytes+4));
        //fprintf(stderr, "    magic: 0x%.08x length: %d\n", magic, length);
        switch(magic) {
            case 0xfade0c01: //write_data("requirements", bytes, length);
                break;
            case 0xfade0c02: //write_data("codedir", bytes, length);
                codedir = CFDataCreate(kCFAllocatorDefault, bytes, length);
                require(codedir, out);
                CFDictionarySetValue(code_signature, CFSTR("CodeDirectory"), codedir);
                CFRelease(codedir);
                uint8_t *cursor = bytes;
                require_string(htonl(*(uint32_t*)(cursor+8)) >= 0x20001, out, "incompatible version");
                require_string(htonl(*(uint32_t*)(cursor+8)) <= 0x2F000, out, "incompatible version");
                uint32_t hash_offset = htonl(*(uint32_t*)(cursor+16));
                require_string(htonl(*(uint32_t*)(cursor+24)) >= 5, out, "no entitlements slot yet");
                require_string(*(cursor+36) == 20, out, "unexpected hash size");
                require_string(*(cursor+37) == 1, out, "unexpected hash type");
                message = CFDataCreate(kCFAllocatorDefault, cursor+hash_offset-5*20, 20);
                require(message, out);
                CFDictionarySetValue(code_signature, CFSTR("EntitlementsCDHash"), message);
                CFRelease(message);
                break;
            case 0xfade0b01:  //write_data("signed", lc_code_signature, bytes-lc_code_signature);
                if (length == 8) {
                    fprintf(stderr, "Ad-hoc signed binary\n");
                    goto out;
                } else {
                    message = CFDataCreate(kCFAllocatorDefault, bytes+8, length-8);
                    require(message, out);
                    CFDictionarySetValue(code_signature, CFSTR("SignedData"), message);
                    CFRelease(message);
               }
                break;
            case 0xfade7171:
            {
                unsigned char digest[CC_SHA1_DIGEST_LENGTH];
                CCDigest(kCCDigestSHA1, bytes, length, digest);
                message = CFDataCreate(kCFAllocatorDefault, digest, sizeof(digest));
                require(message, out);
                CFDictionarySetValue(code_signature, CFSTR("EntitlementsHash"), message);
                CFRelease(message);
                message = CFDataCreate(kCFAllocatorDefault, bytes+8, length-8);
                require(message, out);
                CFDictionarySetValue(code_signature, CFSTR("Entitlements"), message);
                CFRelease(message);
                break;
            }
            default:
                fprintf(stderr, "Skipping block with magic: 0x%x\n", magic);
                break;
        }
    }
    return code_signature;
out:
    if (code_signature) CFRelease(code_signature);
    return NULL;
}

#if 1
static FILE *
open_bundle(const char * path, const char * mode)
{
    char full_path[1024] = {};
    CFStringRef path_cfstring = NULL;
    CFURLRef path_url = NULL;
    CFBundleRef bundle = NULL;

    path_cfstring = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
    require(path_cfstring, out);
    path_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_cfstring, kCFURLPOSIXPathStyle, true);
    require(path_url, out);
	bundle =  CFBundleCreate(kCFAllocatorDefault, path_url);
    require(bundle, out);
    CFURLRef exec = CFBundleCopyExecutableURL(bundle);
    require(exec, out);
    require(CFURLGetFileSystemRepresentation(exec, true, (uint8_t*)full_path, sizeof(full_path)), out);
out:
    if (path_cfstring) CFRelease(path_cfstring);
    if (path_url) CFRelease(path_url);
    if (bundle) CFRelease(bundle);

    return fopen(full_path, "r");
}
#else
static FILE *
open_bundle(const char *path, const char *mode)
{
    char full_path[1024] = {};
    const char *slash;
    char *dot;
    slash = rindex(path, '/');
    if (!slash) slash = path;
    require(strlcpy(full_path, path, sizeof(full_path)) < sizeof(full_path), out);
    require(strlcat(full_path, "/", sizeof(full_path)), out);
    require(strlcat(full_path, slash, sizeof(full_path)), out);
    require(dot = rindex(full_path, '.'), out);
    *dot = '\0';
    return fopen(full_path, "r");
out:
    return NULL;
}
#endif

CFMutableDictionaryRef load_code_signature(FILE *binary, size_t slice_offset)
{
    bool signature_found = false;
    CFMutableDictionaryRef result = NULL;
    union {
        struct load_command lc;
        struct linkedit_data_command le_lc;
    } cmd;
    do {
        require(1 == fread(&cmd.lc, sizeof(cmd.lc), 1, binary), out);
        if (cmd.lc.cmd == LC_CODE_SIGNATURE) {
            require(1 == fread((uint8_t *)&cmd.lc + sizeof(cmd.lc), sizeof(cmd.le_lc) - sizeof(cmd.lc), 1, binary), out);
            require_noerr(fseek(binary, slice_offset+cmd.le_lc.dataoff, SEEK_SET), out);
            size_t length = cmd.le_lc.datasize;
            uint8_t *data = malloc(length);
            require(length && data, out);
            require(1 == fread(data, length, 1, binary), out);
            signature_found = true;
            result = lc_code_sig(data, length);
            free(data);
            break;
        }
        require_noerr(fseek(binary, cmd.lc.cmdsize-sizeof(cmd.lc), SEEK_CUR), out);
    } while(cmd.lc.cmd || cmd.lc.cmdsize); /* count lc */
out:
    if (!signature_found)
        fprintf(stderr, "No LC_CODE_SIGNATURE segment found\n");
    return result;
}

CFArrayRef load_code_signatures(const char *path)
{
    bool fully_parsed_binary = false;
    CFMutableDictionaryRef result = NULL;
    CFMutableArrayRef results = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    FILE *binary = open_bundle(path, "r");
    require(binary, out);

    struct mach_header header;
    require(1 == fread(&header, sizeof(header), 1, binary), out);
    if (header.magic == 0xfeedface) {
        result = load_code_signature(binary, 0 /*non fat*/);
        require(result, out);
        CFArrayAppendValue(results, result);
    }
    else
    {
        struct fat_header fat;
        require(!fseek(binary, 0L, SEEK_SET), out);
        require(1 == fread(&fat, sizeof(fat), 1, binary), out);
        require(htonl(fat.magic) == FAT_MAGIC, out);
        uint32_t slice, slices = htonl(fat.nfat_arch);
        struct fat_arch *archs = calloc(slices, sizeof(struct fat_arch));
        require(slices == fread(archs, sizeof(struct fat_arch), slices, binary), out);
        for (slice = 0; slice < slices; slice++) {
            uint32_t slice_offset = htonl(archs[slice].offset);
            require(!fseek(binary, slice_offset, SEEK_SET), out);
            require(1 == fread(&header, sizeof(header), 1, binary), out);
            require(header.magic == 0xfeedface, out);
            result = load_code_signature(binary, slice_offset);
            require(result, out);
            CFArrayAppendValue(results, result);
            CFRelease(result);
        }
    }
    fully_parsed_binary = true;
out:
    if (!fully_parsed_binary) {
        if (results) {
            CFRelease(results);
            results = NULL;
        }
    }
    if (binary)
        fclose(binary);
    return results;
}
