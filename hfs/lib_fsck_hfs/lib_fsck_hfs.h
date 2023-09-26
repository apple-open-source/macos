//
//  lib_fsck_hfs.h
//
//  Created by Adam Hijaze on 02/11/2022.
//

#ifndef lib_fsck_hfs_h
#define lib_fsck_hfs_h

#import <os/log.h>

#include "dfalib/CheckHFS.h"
#include "fsck_hfs_msgnums.h"

#include <stdio.h>
#include <string.h>

typedef void* fsck_client_ctx_t;
typedef void* fsck_ctx_t;

typedef enum {
    LOG_TYPE_ERROR,
    LOG_TYPE_WARNING,
    LOG_TYPE_INFO,
    LOG_TYPE_FATAL,
    LOG_TYPE_TERMINATE,
    LOG_TYPE_FILE_STDERR,
    LOG_TYPE_STDERR,
    LOG_TYPE_WARN
} LogMessageType;

/*
 * void (*fsck_hfs_print_msg_type_funct_t)(fsck_client_ctx_t, LogMessageType type, const char *fmt, va_list ap)
 *
 * This routine is used to print out message identified by type, using ap data.
 * A valid msg type is values from LogMessageType enum.
 */
typedef void (*fsck_hfs_print_msg_type_funct_t)(fsck_client_ctx_t, LogMessageType type, const char *fmt, va_list ap);

/*
 * int (*fsck_exfat_print_msg_num_func_t)(fsck_client_ctx_t, int msgNum, va_list ap);
 *
 * This routine is used to print out a message identified by msgnum, using ap data.
 * A valid msgnum is any value enumarated in fsck_msgnums.h and fsck_hfs_msgnums.h.
 *
 * Return 0 on success, and -1 on failure.
 */
typedef int (*fsck_exfat_print_msg_num_func_t)(fsck_client_ctx_t, int msgNum, va_list ap);

/*
 * void (*fsck_exfat_print_debug_func_t)(fsck_client_ctx_t, unsigned long type, , const char *fmt, va_list ap);
 *
 * This routine is used to print debug messages based in the current debug level and the type of message.
 * The message type passed to DPRINTF can be one or combination (OR-ed value) of pre-defined
 * debug message types (see fsck_debug.h).  Only the messages whose type have one or more similar
 * bits set in comparison with current global debug level are printed.
 *
 */
typedef void (*fsck_exfat_print_debug_func_t)(fsck_client_ctx_t, unsigned long int type, const char *fmt, va_list ap);

void fsck_init_state(void);

void fsck_set_context_properties(fsck_hfs_print_msg_type_funct_t print_type,
                                 fsck_exfat_print_msg_num_func_t print_num,
                                 fsck_exfat_print_debug_func_t print_debug,
                                 fsck_client_ctx_t client,
                                 fsck_ctx_t messages_context);

/*
 *
 * Main routine to run a check on a file system.
 * Client must use call fsck_init_state  and fsck_set_context_properties before running this routine.
 */
int     checkfilesys(char * filesys);

/* Destroy the cache used in checkfilesys routine */
void DestroyCache();

void AddBlockToList(long long block);


/*
 * Setters and Getters routines for check options and device properties
 */
void fsck_set_fd(int fd);
int fsck_get_fd();

void fsck_set_fsmodified(int fd);
int fsck_get_fsmodified();

void fsck_set_fsreadfd(int fd);
int fsck_get_fsreadfd();

void fsck_set_fswritefd(int fd);
int fsck_get_fswritefd();

void fsck_set_hotroot(char val);
char fsck_get_hotroot();

void fsck_set_hotmount(char val);
char fsck_get_hotmount();

void fsck_set_xmlcontrol(char val);
char fsck_get_xmlcontrol();

void fsck_set_guicontrol(char val);
char fsck_get_guicontrol();

void fsck_set_rebuild_btree(char val);
char fsck_get_rebuild_btree();

void fsck_set_rebuild_options(int val);
int fsck_get_rebuild_options();

void fsck_set_debug_level(unsigned long val);
unsigned long fsck_get_debug_level();

void fsck_set_mode_setting(char val);
char fsck_get_mode_setting();

void fsck_set_error_on_exit(char val);
char fsck_get_error_on_exit();

void fsck_set_upgrading(int val);
int  fsck_get_upgrading();

void fsck_set_lostAndFoundMode(int val);
int fsck_get_lostAndFoundMode();

void fsck_set_cache_size(uint64_t val);
uint64_t fsck_get_cachesize();

void fsck_set_block_size(long val);
long fsck_get_block_size();

void fsck_set_detonator_run(int val);
int fsck_get_detonator_run();

void fsck_set_cdevname(const char* val);
const char* fsck_get_cdevname();

void fsck_set_progname(char* val);
char* fsck_get_progname();

void fsck_set_lflag(char val);
char fsck_get_lflag();

void fsck_set_nflag(char val);
char fsck_get_nflag();

void fsck_set_yflag(char val);
char fsck_get_yflag();

void fsck_set_preen(char val);
char fsck_get_preen();

void fsck_set_force(char val);
char fsck_get_force();

void fsck_set_quick(char val);
char fsck_get_quick();

void fsck_set_debug(char val);
char fsck_get_debug();

void fsck_set_disable_journal(char val);
char fsck_get_disable_journal();

void fsck_set_scanflag(char val);
char fsck_get_scanflag();

void fsck_set_embedded(char val);
char fsck_get_embedded();

void fsck_set_repair_level(char val);
char fsck_get_repair_level();

void fsck_set_chek_level(char val);
char fsck_get_chek_level();

void fsck_set_dev_block_size(uint32_t val);
uint32_t fsck_get_dev_block_size();

void fsck_set_dev_block_count(uint64_t val);
uint64_t fsck_get_dev_block_count();

void fsck_set_mount_point(char* val);
char* fsck_get_mount_point();

void fsck_set_write_only_fd(int val);
int fsck_get_write_only_fd();

void fsck_set_device_writable(int val);
int fsck_get_writable();

int fsck_get_verbosity_level();
void fsck_set_verbosity_level(int val);

#endif /* lib_fsck_hfs_h */
