#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/mman.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <mach/mach.h>
#include <mach/mach_error.h>

#include <architecture/byte_order.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFBundlePriv.h>
#include <IOKit/kext/KXKext.h>

#include <Kernel/libsa/mkext.h>
// In compression.c
__private_extern__ u_int8_t *
encodeLZSS(u_int8_t *dstP, long dstLen, u_int8_t *srcP, long srcLen);
__private_extern__ void
checkLZSS(u_int8_t *codeP, u_int8_t *srcEnd, u_int8_t *textP, u_int32_t tLen);
__private_extern__ u_int32_t
local_adler32(u_int8_t *buffer, int32_t length);

// In arch.c
__private_extern__ void
find_arch(u_int8_t **dataP, off_t *sizeP, cpu_type_t in_cpu,
    cpu_subtype_t in_cpu_subtype, u_int8_t *data_ptr, off_t filesize);
__private_extern__ int
get_arch_from_flag(char *name, cpu_type_t *cpuP, cpu_subtype_t *subcpuP);

// in kextcache_main.c
extern char * CFURLCopyCString(CFURLRef anURL);

extern const char * progname;

/* Open Firmware has an upper limit of 8MB on file transfers,
 * so we'll limit ourselves just beneath that.
 */
#define kMkxtDefaultSize ((8 * 1024 * 1024) - 256)



/*******************************************************************************
*
*******************************************************************************/
// compress one file and fill in its mkext_file structure
// However if we have no work, i.e. the filename is empty, the file isn't
// readable or the size is 0, then just return immediately
static int compressFile(
    const char *fileName,
    u_int8_t * archiveStart,
    u_int8_t * archiveEnd,
    mkext_file * file,
    u_int8_t ** compression_dst,
    const char * archName,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    int verbose_level)
{
    int result = 1;
    struct stat statbuf;
    off_t size;
    u_int8_t *dstend, *src, *data;  // don't free
    u_int8_t *chkbuf = NULL;   // must free
    int fd = -1;

    memset(file, '\0', sizeof(*file));

    if (!fileName) {
        fprintf(stderr, "no filename given to compress\n");
        result = -1;
        goto finish;
    }

    if (!*fileName) {
        // this is allowed and means add an empty entry
        goto finish;
    }

    if (!compression_dst || !*compression_dst) {
        fprintf(stderr, "no data to compress\n");
        result = 0;
        goto finish;
    }

    fd = open(fileName, O_RDONLY, 0);
    if (-1 == fd) {
        fprintf(stderr, "can't open file %s - %s\n", fileName, strerror(errno));
        result = 0;
        goto finish;
    }
        
    if (-1 == fstat(fd, &statbuf)) {
        fprintf(stderr, "can't stat file %s - %s\n", fileName, strerror(errno));
        result = 0;
        goto finish; // FIXME: mkextcache used to exit with EX_NOINPUT
    }

    if (!statbuf.st_size) {
        // FIXME: should this be an error?
        goto finish;
    }

    src = mmap(0, (size_t) statbuf.st_size, PROT_READ, MAP_FILE, fd, 0);
    if (-1 == (int) src) {
        fprintf(stderr, "can't map file %s - %s\n", fileName, strerror(errno));
        result = 0;
        goto finish; // FIXME: mkextcache used to exit with EX_SOFTWARE
    }

    find_arch(&data, &size, archCPU, archSubtype, src, statbuf.st_size);
    if (!size) {
        // At this point not finding the requested arch is more significant
        if (verbose_level >= 1) {
            fprintf(stdout, "can't find architecture %s in %s\n",
                archName, fileName);
        }
        munmap(src, statbuf.st_size);
        result = 0;
        goto finish;
    }

    if (verbose_level >= 2) {
        fprintf(stdout, "compressing %s %ld => ", fileName, (size_t)size);
    }

    dstend = compress_lzss(*compression_dst, archiveEnd - *compression_dst,
        data, size);
    if (!dstend) {
        if (verbose_level >= 2) {
            fprintf(stdout, "\n\n");
        }
        fprintf(stderr, "resulting mkext file would be too large; aborting\n");
        result = -1;
        goto finish; // FIXME: mkextcache used to exit with EX_SOFTWARE
    }
    file->offset       = NXSwapHostLongToBig(*compression_dst - archiveStart);
    file->realsize     = NXSwapHostLongToBig((size_t) size);
    file->compsize     = NXSwapHostLongToBig(dstend - *compression_dst);
    file->modifiedsecs = NXSwapHostLongToBig(statbuf.st_mtimespec.tv_sec);

    if (dstend >= *compression_dst + size) {
        // Compression grew the code so copy in clear - already compressed?
        file->compsize = 0;
        memcpy(*compression_dst, data, (size_t) size);
        dstend = *compression_dst + size;
    }

    if (file->compsize && verbose_level >= 3) {
        size_t chklen;

        chkbuf = (u_int8_t *)malloc((size_t)size);

        chklen = decompress_lzss(chkbuf, *compression_dst,
            dstend - *compression_dst);
        if (chklen != (size_t)size) {
            if (verbose_level >= 2) {
                fprintf(stdout, "\n\n");
            }
            fprintf(stderr,
                "internal error; decompressed size %ld differs "
                "from original size %ld\n",
                (size_t)size, chklen);
            result = 0;
            goto finish; // FIXME: mkextcache used to exit with EX_SOFTWARE
        }
        if (0 != memcmp(chkbuf, data, (size_t) size)) {
            if (verbose_level >= 2) {
                fprintf(stdout, "\n\n");
            }
            fprintf(stderr,
                "internal error; decompressed data differs from input\n");
            result = 0;
            goto finish; // FIXME: mkextcache used to exit with EX_SOFTWARE
        }
    }

    if (verbose_level >= 2) {
        if (0 == NXSwapBigLongToHost(file->compsize)) {
            fprintf(stdout, "same (compression did not reduce size; copied as-is)\n");
        } else {
            fprintf(stdout, "%ld\n", NXSwapBigLongToHost(file->compsize));
        }
    }

    if (-1 == munmap(src, (size_t) statbuf.st_size)) {
        fprintf(stderr, "can't unmap memory - %s", strerror(errno));
        result = 0;
        goto finish; // FIXME: mkextcache use to exit with EX_SOFTWARE
    }

    *compression_dst = dstend;

finish:
    if (fd != -1) {
        close(fd);
    }

    if (chkbuf) {
        free(chkbuf);
    }

    return result;
}



/*******************************************************************************
*
*******************************************************************************/
static Boolean addKextToMkextArchive(KXKextRef aKext,
    mkext_header *mkextArchive,
    u_int8_t *archiveEnd,
    mkext_kext *curkext,
    u_int8_t **compression_dst,
    const char * archName,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    int verbose_level)
{
    Boolean result = true;
    CFBundleRef kextBundle = NULL;    // don't release
    CFURLRef infoDictURL = NULL;      // must release
    CFURLRef executableURL = NULL;    // must release
    char * info_dict_path = NULL;     // must free
    const char * executable_path = NULL;    // don't free; set to one of following
    const char * empty_executable_path = "";    // don't free
    char * alloced_executable_path = NULL; // must free

    kextBundle = KXKextGetBundle(aKext);
    if (!kextBundle) {
        fprintf(stderr, "can't get bundle for kext\n");
        result = false;
        goto finish;
    }

    infoDictURL = _CFBundleCopyInfoPlistURL(kextBundle);
    if (!infoDictURL) {
        fprintf(stderr, "can't get info dict URL for kext\n");
        result = false;
        goto finish;
    }

    info_dict_path = CFURLCopyCString(infoDictURL);
    if (!info_dict_path) {
        fprintf(stderr, "string conversion or memory allocation failure\n");
        result = false;
        goto finish;
    }

    switch (compressFile(info_dict_path, 
        (u_int8_t *)mkextArchive, archiveEnd, &curkext->plist, compression_dst,
        archName, archCPU, archSubtype, verbose_level)) {

      case -1:
        /* terminating error; log nothing, just wrap up */
        result = false;
        goto finish;
        break;

      case 0:
        fprintf(stderr, "can't archive info dict file %s - %s",
            info_dict_path, strerror(errno));
        result = false;
        goto finish;  // FIXME: mkextcache used to exit with EX_NOPERM
        break;

      default:
        /* do nothing */
        break;
    }

    if (!KXKextGetDeclaresExecutable(aKext)) {
        executable_path = empty_executable_path;  // No executable
    } else {
        executableURL = CFBundleCopyExecutableURL(kextBundle);
        if (!executableURL) {
            fprintf(stderr, "can't get executable URL for kext\n");
            result = false;
            goto finish;
        }

        alloced_executable_path = CFURLCopyCString(executableURL);
        if (!alloced_executable_path) {
           fprintf(stderr, "string conversion or memory allocation failure\n");
            result = false;
            goto finish;
        }
        executable_path = alloced_executable_path;
    }

   /* Note this function must be called even with no executable file in
    * order to set up the mkext file's data structures.
    */
    switch (compressFile(executable_path,
        (u_int8_t *)mkextArchive, archiveEnd, &curkext->module, compression_dst,
        archName, archCPU, archSubtype, verbose_level)) {

      case -1:
        /* terminating error; log nothing, just wrap up */
        result = false;
        goto finish;
        break;

      case 0:
        fprintf(stderr, "can't archive kext binary file %s - %s",
            executable_path, strerror(errno));
        result = false;
        goto finish;  // FIXME: mkextcache used to exit with EX_NOPERM
        break;

      default:
        /* do nothing */
        break;
    }

finish:
    if (infoDictURL)             CFRelease(infoDictURL);
    if (executableURL)           CFRelease(executableURL);
    if (info_dict_path)          free(info_dict_path);
    if (alloced_executable_path) free(alloced_executable_path);
    return result;
}

/*******************************************************************************
*
*******************************************************************************/
Boolean createMkextArchive(CFDictionaryRef kextDict,
    const char * mkextFilename,
    const char * archName,
    cpu_type_t archCPU,
    cpu_subtype_t archSubtype,
    int verbose_level)
{
    Boolean result = true;
    CFIndex count = 0;
    CFIndex i;
    int fd = -1;

    mkext_header * mkextArchive = 0;
    u_int8_t * mkextArchiveEnd = 0;
    u_int8_t * adler_point = 0;

    KXKextRef * kexts = NULL;  // must release
    kern_return_t kern_result;
    vm_address_t archive_vm_addr = 0;  // must vm_deallocate

    u_int8_t * compression_dst;

    const char * output_filename = NULL; // don't free

    if (!mkextFilename) {
        output_filename = "(file unspecified)";
    } else {
        output_filename = mkextFilename;
    }

    fd = open(output_filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
    if (-1 == fd) {
        fprintf(stderr, "can't create %s - %s\n", output_filename,
            strerror(errno));
        result = false;
        goto finish;  // FIXME: mkextcache used to exit with EX_CANTCREAT
    }

    count = CFDictionaryGetCount(kextDict);
    if (!count) {
        fprintf(stderr, "couldn't find any valid bundles to archive\n");
        result = false;
        goto finish;  // FIXME: mkextcache used to exit with EX_NOINPUT
    }

    kern_result = vm_allocate(mach_task_self(),
                      &archive_vm_addr, kMkxtDefaultSize, VM_FLAGS_ANYWHERE);
    if (kern_result != KERN_SUCCESS) {
        fprintf(stderr, "failed to allocate address space - %s\n",
            mach_error_string(kern_result));
        result = false;
        goto finish;  // FIXME: mkextcache used to exit with EX_OSERR
    }

    mkextArchive = (mkext_header *)archive_vm_addr;
    mkextArchiveEnd = (u_int8_t *)(archive_vm_addr + kMkxtDefaultSize);
    mkextArchive->magic = NXSwapHostIntToBig(MKEXT_MAGIC);
    mkextArchive->signature = NXSwapHostIntToBig(MKEXT_SIGN);
    mkextArchive->version = NXSwapHostIntToBig(0x01008000);   // 'vers' 1.0.0
    mkextArchive->numkexts = NXSwapHostIntToBig(count);
    mkextArchive->cputype = NXSwapHostIntToBig(archCPU);
    mkextArchive->cpusubtype = NXSwapHostIntToBig(archSubtype);

    adler_point = (UInt8 *)&mkextArchive->version;

    // Set the pointer for the compressed data stream to the
    // first byte after the kext list in the header section.
    compression_dst = (u_int8_t *)&mkextArchive->kext[count];

   /* Prepare to iterate over the kexts in the dictionary.
    */
    kexts = (KXKextRef *)malloc(count * sizeof(KXKextRef));
    if (!kexts) {
        fprintf(stderr, "memory allocation failure\n");
        result = true;
        goto finish;
    }
    CFDictionaryGetKeysAndValues(kextDict, NULL, (const void **)kexts);

    for (i = 0; i < count; i++) {
        KXKextRef thisKext = kexts[i];

        result = addKextToMkextArchive(thisKext,
            mkextArchive, mkextArchiveEnd,
            &mkextArchive->kext[i], &compression_dst,
            archName, archCPU, archSubtype, verbose_level);
        if (!result) {
            // addKextToMkextArchive() printed an error message
            goto finish;
        }
    }

    mkextArchive->length = NXSwapHostIntToBig(compression_dst -
        (u_int8_t *)mkextArchive);
    mkextArchive->adler32 = NXSwapHostIntToBig(local_adler32(adler_point,
        compression_dst - adler_point));

    if (fd != -1) {
        ssize_t bytes_written = 0;
        ssize_t bytes_length = 0;

        bytes_length = NXSwapBigIntToHost(mkextArchive->length);
        while (bytes_written < bytes_length) {
            int write_result;
            write_result = write(fd, mkextArchive + bytes_written,
                bytes_length - bytes_written);
            if (write_result < 0) {
                fprintf(stderr, "write failed for %s - %s\n", output_filename,
                    strerror(errno));
                result = false;
                goto finish;  // FIXME: mkextcache used to exit with EX_IOERR
            }
            bytes_written += write_result;
        }
    }

    if (verbose_level >= 1) {
        fprintf(stdout, "%s: %s contains %d kexts for %d bytes with crc 0x%x\n",
            progname, 
            output_filename,
            NXSwapBigIntToHost(mkextArchive->numkexts),
            NXSwapBigIntToHost(mkextArchive->length),
            NXSwapBigIntToHost(mkextArchive->adler32));
    }

finish:
    if (!result && fd != -1) {
        if (-1 == unlink(output_filename)) {
            fprintf(stderr, "can't remove file %s - %s\n", output_filename,
                strerror(errno));
        }
    }

    if (fd != -1) {
        close(fd);
    }
    if (kexts)  free(kexts);
    if (archive_vm_addr) {
        kern_result = vm_deallocate(mach_task_self(), archive_vm_addr,
            kMkxtDefaultSize);
    }

    return result;
}
