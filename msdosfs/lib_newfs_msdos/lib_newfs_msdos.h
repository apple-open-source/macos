//
//  lib_newfs_msdos.h
//  msdosfs
//
//  Created by Kujan Lauz on 04/09/2022.
//

#ifndef lib_newfs_msdos_h
#define lib_newfs_msdos_h

#include <stdarg.h>
#include <CoreFoundation/CoreFoundation.h>
#include <sys/syslog.h>
#include <sys/stat.h>

typedef void* newfs_client_ctx_t;

/** Struct containing fields required for running newfs, some of these fields can only be collected by a root level process, or by an entitled process.
    The fields in the struct are expected to initialized to 0 (memset), so if a field is not set it will be 0.
 */
typedef struct {
    int fd;                     /* File descriptor */
    const char *devName;        /* Device name */
    uint32_t physBlockSize;     /* Physical block size */
    uint64_t partitionBase;     /* Partition offset */
    uint64_t blockCount;        /* Block count */
    uint32_t blockSize;         /* Block size */
    const char *bname;          /* Bootstrap file */
    int bootFD;                 /* Bootstrap file descriptor */
    struct stat sb;             /* File stats */
} NewfsProperties;

typedef struct {
    int     fd;
    size_t  block_size;
    u_int   except_block_start;
    u_int   except_block_length;
} WipeFSProperties;

/** Prints message */
typedef void (*newfs_msdos_print_funct_t)(newfs_client_ctx_t, int level, const char *fmt, va_list ap);

/** WipeFS function, wipes a resource by given properties */
typedef int (*newfs_msdos_wipefs_func_t)(newfs_client_ctx_t ctx, WipeFSProperties props);

/** Struct containing pointer functions to our print functions */
typedef struct {
    newfs_msdos_print_funct_t print;
    newfs_msdos_wipefs_func_t wipefs;
    newfs_client_ctx_t client_ctx;
} lib_newfs_ctx_t;

extern lib_newfs_ctx_t newfs_ctx;

void newfs_set_context_properties(newfs_msdos_print_funct_t print, newfs_msdos_wipefs_func_t wipefs, newfs_client_ctx_t client);
void newfs_print(lib_newfs_ctx_t c, int level, const char *fmt, ...) __printflike(3, 4);

/* Setters and getters for library properties */
newfs_msdos_wipefs_func_t newfs_get_wipefs_function_callback(void);
void newfs_set_wipefs_function_callback(newfs_msdos_wipefs_func_t func);

newfs_msdos_print_funct_t newfs_get_print_function_callback(void);
void newfs_set_print_function_callback(newfs_msdos_print_funct_t func);

newfs_client_ctx_t newfs_get_client(void);
void newfs_set_client (newfs_client_ctx_t c);

/* Wipes target device by calling directly to wipefs library */
int wipefs(newfs_client_ctx_t ctx, WipeFSProperties props);

#endif /* lib_newfs_msdos_h */
