/*
 * Copyright (c) 2008,2010,2013-2014 Apple Inc. All Rights Reserved.
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

#include <TargetConditionals.h>
#if TARGET_OS_EMBEDDED

#include "SecurityCommands.h"

#include <AssertMacros.h>
#include <mach-o/loader.h>
#include <mach-o/fat.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCMS.h>
#include <Security/SecPolicyPriv.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <utilities/SecCFRelease.h>

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

#if 0
static void debug_data(uint8_t *data, size_t length)
{
    uint32_t i, j;
    for (i = 0; i < length; i+=16) {
        fprintf(stderr, "%p   ", (void*)(data+i));
        for (j = 0; (j < 16) && (j+i < length); j++) {
            uint8_t byte = *(uint8_t*)(data+i+j);
            fprintf(stderr, "%.02x %c|", byte, isprint(byte) ? byte : '?');
        }
        fprintf(stderr, "\n");
    }
}

static void write_data(const char *path, uint8_t *data, size_t length)
{
    int fd = open(path, O_RDWR|O_TRUNC|O_CREAT, 0644);
    require(fd>0, out);
    write(fd, data, length);
    close(fd);
out:
    return;
}
#endif

static void fprint_digest(FILE *file, unsigned char *digest, size_t length) {
    size_t ix;
    for (ix = 0; ix < length; ++ix) {
        fprintf(file, "%02x", digest[ix]);
    }
}

static CFMutableDictionaryRef lc_code_sig(uint8_t *lc_code_signature, size_t lc_code_signature_len)
{
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
            {
                const CS_CodeDirectory *cd = (const CS_CodeDirectory *)bytes;
                CFDataRef codedir = CFDataCreate(kCFAllocatorDefault, bytes, length);
                require(codedir, out);
                CFDictionarySetValue(code_signature, CFSTR("CodeDirectory"), codedir);
                CFRelease(codedir);
                require_string(ntohl(cd->version) >= 0x20001, out, "incompatible version");
                require_string(ntohl(cd->version) <= 0x2F000, out, "incompatible version");
                require_string(cd->hashSize == 20, out, "unexpected hash size");
                require_string(cd->hashType == 1, out, "unexpected hash type");

                uint32_t hash_offset = ntohl(cd->hashOffset);
                uint32_t entitlement_slot = 5;

                if (ntohl(cd->nSpecialSlots) >= entitlement_slot) {
                    CFDataRef message = CFDataCreate(kCFAllocatorDefault, bytes+hash_offset-entitlement_slot*cd->hashSize, cd->hashSize);
                    require(message, out);
                    CFDictionarySetValue(code_signature, CFSTR("EntitlementsCDHash"), message);
                    CFRelease(message);
                } else
                    fprintf(stderr, "no entitlements slot yet\n");
            }
                break;
            case 0xfade0b01:  //write_data("signed", lc_code_signature, bytes-lc_code_signature);
                if (length != 8) {
                    CFDataRef message = CFDataCreate(kCFAllocatorDefault, bytes+8, length-8);
                    require(message, out);
                    CFDictionarySetValue(code_signature, CFSTR("SignedData"), message);
                    CFRelease(message);
                }
                break;
            case 0xfade7171:
            {
                unsigned char digest[CC_SHA1_DIGEST_LENGTH];
                CCDigest(kCCDigestSHA1, bytes, length, digest);

                CFDataRef message = CFDataCreate(kCFAllocatorDefault, digest, sizeof(digest));
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

static FILE *
open_bundle(const char * path, const char * mode)
{
    char full_path[1024] = {};
    CFStringRef path_cfstring = NULL;
    CFURLRef path_url = NULL;
    CFBundleRef bundle = NULL;
    CFURLRef exec = NULL;

    path_cfstring = CFStringCreateWithFileSystemRepresentation(kCFAllocatorDefault, path);
    require_quiet(path_cfstring, out);
    path_url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, path_cfstring, kCFURLPOSIXPathStyle, true);
    require_quiet(path_url, out);
	bundle =  CFBundleCreate(kCFAllocatorDefault, path_url);
    require_quiet(bundle, out);
    exec = CFBundleCopyExecutableURL(bundle);
    require(exec, out);
    require(CFURLGetFileSystemRepresentation(exec, true, (uint8_t*)full_path, sizeof(full_path)), out);
out:
    CFReleaseSafe(path_cfstring);
    CFReleaseSafe(path_url);
    CFReleaseSafe(bundle);
    CFReleaseSafe(exec);

    return fopen(full_path, "r");
}

static CFMutableDictionaryRef load_code_signature(FILE *binary, size_t slice_offset)
{
    bool signature_found = false;
    CFMutableDictionaryRef result = NULL;
    struct load_command lc;
    do {
        require(1 == fread(&lc, sizeof(lc), 1, binary), out);
        if (lc.cmd == LC_CODE_SIGNATURE) {
            struct { uint32_t offset; uint32_t size; } sig;
            require(1 == fread(&sig, sizeof(sig), 1, binary), out);
            require_noerr(fseek(binary, slice_offset+sig.offset, SEEK_SET), out);
            size_t length = sig.size;
            uint8_t *data = malloc(length);
            require(length && data, out);
            require(1 == fread(data, length, 1, binary), out);
            signature_found = true;
            result = lc_code_sig(data, length);
            free(data);
            break;
        }
        require_noerr(fseek(binary, lc.cmdsize-sizeof(lc), SEEK_CUR), out);
    } while(lc.cmd || lc.cmdsize); /* count lc */
out:
    if (!signature_found)
        fprintf(stderr, "No LC_CODE_SIGNATURE segment found\n");
    return result;
}

static CF_RETURNS_RETAINED CFArrayRef load_code_signatures(const char *path)
{
    bool fully_parsed_binary = false;
    CFMutableDictionaryRef result = NULL;
    CFMutableArrayRef results = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    FILE *binary = open_bundle(path, "r");
    if (!binary) binary = fopen(path, "r");
    require(binary, out);

    struct mach_header header;
    require(1 == fread(&header, sizeof(header), 1, binary), out);
    if ((header.magic == MH_MAGIC) || (header.magic == MH_MAGIC_64)) {
	if (header.magic == MH_MAGIC_64)
		fseek(binary, sizeof(struct mach_header_64) - sizeof(struct mach_header), SEEK_CUR);
        result = load_code_signature(binary, 0 /*non fat*/);
        require(result, out);
        CFStringRef type = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("CPU type: (%d,%d)"), header.cputype, header.cpusubtype);
        CFDictionarySetValue(result, CFSTR("ARCH"), type);
        CFRelease(type);
        CFArrayAppendValue(results, result);
    }
    else
    {
        struct fat_header fat;
        require(!fseek(binary, 0L, SEEK_SET), out);
        require(1 == fread(&fat, sizeof(fat), 1, binary), out);
        require(ntohl(fat.magic) == FAT_MAGIC, out);
        uint32_t slice, slices = ntohl(fat.nfat_arch);
        struct fat_arch *archs = calloc(slices, sizeof(struct fat_arch));
        require(slices == fread(archs, sizeof(struct fat_arch), slices, binary), out);
        for (slice = 0; slice < slices; slice++) {
            uint32_t slice_offset = ntohl(archs[slice].offset);
            require(!fseek(binary, slice_offset, SEEK_SET), out);
            require(1 == fread(&header, sizeof(header), 1, binary), out);
	    require((header.magic == MH_MAGIC) || (header.magic == MH_MAGIC_64), out);
	    if (header.magic == MH_MAGIC_64)
		    fseek(binary, sizeof(struct mach_header_64) - sizeof(struct mach_header), SEEK_CUR);
            result = load_code_signature(binary, slice_offset);
            require(result, out);
            CFStringRef type = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("CPU type: (%d,%d)"), header.cputype, header.cpusubtype);
            CFDictionarySetValue(result, CFSTR("ARCH"), type);
            CFRelease(type);
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


extern int codesign_util(int argc, char * const *argv)
{
    int             result = 1, verbose = 0;
    char            ch;

    while ((ch = getopt(argc, argv, "v")) != -1)
    {
        switch (ch)
        {
        case 'v':
            verbose++;
            break;
        default:
            return 2; /* Trigger usage message. */
        }
    }
    
	argc -= optind;
	argv += optind;

    if (argc != 1)
        return 2; /* Trigger usage message. */

    CFArrayRef sigs = load_code_signatures(argv[0]);
    require(sigs, out);

    if (verbose >= 2)
        CFShow(sigs);

    CFIndex i, count = CFArrayGetCount(sigs);
    
    for (i = 0; i < count; i++) {
        CFDictionaryRef signature = CFArrayGetValueAtIndex(sigs, i);

        CFDataRef code_dir = CFDictionaryGetValue(signature, CFSTR("CodeDirectory"));
        const CS_CodeDirectory *cd = (CS_CodeDirectory *)CFDataGetBytePtr(code_dir);

        CFDataRef signed_data = CFDictionaryGetValue(signature, CFSTR("SignedData"));

        CFDataRef entitlements = CFDictionaryGetValue(signature, CFSTR("Entitlements"));
        CFDataRef entitlements_cd_hash = CFDictionaryGetValue(signature, CFSTR("EntitlementsCDHash"));
        CFDataRef entitlements_hash = CFDictionaryGetValue(signature, CFSTR("EntitlementsHash"));

        CFStringRef arch = CFDictionaryGetValue(signature, CFSTR("ARCH"));
        
        CFShow(arch);

        SecPolicyRef policy = SecPolicyCreateiPhoneApplicationSigning();

        if (signed_data) {
            if (SecCMSVerify(signed_data, code_dir, policy, NULL, NULL)) {
                fprintf(stderr, "Failed to verify signature\n");
                result = -1;
            } else
                fprintf(stderr, "Signature ok\n");

        } else
            fprintf(stderr, "Ad-hoc signed binary\n");

        if (entitlements_cd_hash) {
            if (entitlements_hash && entitlements_cd_hash && CFEqual(entitlements_hash, entitlements_cd_hash))
                fprintf(stderr, "Entitlements ok\n");
            else
                fprintf(stderr, "Entitlements modified\n");
        }

        if (verbose >= 2) {
            fprintf(stderr, "magic: 0x%x length: %u(%lu)\n", ntohl(cd->magic), ntohl(cd->length), CFDataGetLength(code_dir));
            fprintf(stderr, "code directory version/flags: 0x%x/0x%x special/code hash slots: %u/%u\n"
                "codelimit: %u hash size/type: %u/%u hash/ident offset: %u/%u\n",
                ntohl(cd->version), ntohl(cd->flags), ntohl(cd->nSpecialSlots), ntohl(cd->nCodeSlots),
                ntohl(cd->codeLimit), cd->hashSize, cd->hashType, ntohl(cd->hashOffset), ntohl(cd->identOffset));
            fprintf(stderr, "ident: '%s'\n", CFDataGetBytePtr(code_dir) + ntohl(cd->identOffset));

            uint32_t ix;
            uint8_t *hashes = (uint8_t *)CFDataGetBytePtr(code_dir) + ntohl(cd->hashOffset);
            for (ix = 0; ix < ntohl(cd->nSpecialSlots); ++ix) {
                fprint_digest(stderr, hashes, cd->hashSize);
                fprintf(stderr, "\n");
                hashes += cd->hashSize;
            }
        }

        if (verbose >= 1) {
            if (entitlements)
                fprintf(stderr, "Entitlements\n%.*s", (int)CFDataGetLength(entitlements)-8, CFDataGetBytePtr(entitlements)+8);
        }

        if (verbose >= 2) {
            if (entitlements_hash) {
                fprintf(stderr, "digest: ");
                fprint_digest(stderr, (uint8_t *)CFDataGetBytePtr(entitlements_hash), CC_SHA1_DIGEST_LENGTH);
                fprintf(stderr, "\n");
            }
        }
    }
    
    CFReleaseSafe(sigs);

    return result;
out:
    return -1;
}

#endif // TARGET_OS_EMBEDDED
