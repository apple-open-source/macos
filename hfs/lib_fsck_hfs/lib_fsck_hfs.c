//
//  lib_fsck_hfs.c
//
//  Created by Adam Hijaze on 02/11/2022.
//

#include "check.h"
#include "lib_fsck_hfs.h"

void fsck_init_state(void) {
    memset(&state, 0, sizeof(state));
    state.chkLev = kAlwaysCheck;
    state.gBlockSize = 512;
    state.fd = -1;
    state.fsreadfd = -1;
    state.fswritefd = -1;
#if    !TARGET_OS_IPHONE
    state.embedded = 0;
#else
    state.embedded = 1;
#endif
}

void fsck_set_context_properties(fsck_hfs_print_msg_type_funct_t print_type,
                                 fsck_exfat_print_msg_num_func_t print_num,
                                 fsck_exfat_print_debug_func_t print_debug,
                                 fsck_client_ctx_t client,
                                 fsck_ctx_t messages_context)
{
    ctx.print_msg_type = print_type;
    ctx.print_msg_num = print_num;
    ctx.client = client;
    ctx.messages_context = messages_context;
    ctx.print_debug = print_debug;
}

int fsckPrintFormat(lib_fsck_ctx_t c, int msgNum, ...)
{
    int ret = -1;
    if (c.print_msg_num) {
        va_list ap;
        va_start(ap, msgNum);
        ret = c.print_msg_num(c.messages_context, msgNum, ap);
        va_end(ap);
    }
    return ret;
}

void fsck_print(lib_fsck_ctx_t c, LogMessageType type, const char *fmt, ...)
{
    if (c.print_msg_type) {
        va_list ap;
        va_start(ap, fmt);
        c.print_msg_type(c.messages_context, type, fmt, ap);
        va_end(ap);
    }
}

void fsck_debug_print(lib_fsck_ctx_t c, int type, const char *fmt, ...) {
    if (c.print_debug) {
        va_list ap;
        va_start(ap, fmt);
        c.print_debug(c.messages_context, type, fmt, ap);
        va_end(ap);
    }
}


void fsck_set_fd(int fd) {
    state.fd = fd;
}

int fsck_get_fd() {
    return state.fd;
}

void fsck_set_debug_level(unsigned long val) {
    state.cur_debug_level = val;
}

unsigned long fsck_get_debug_level() {
    return state.cur_debug_level;
}

char* fsck_get_progname() {
    return state.progname;
}

void fsck_set_progname(char* progname) {
    state.progname = progname;
}

void fsck_set_fsmodified(int fd) {
    state.fsmodified = fd;
}

int fsck_get_fsmodified() {
    return state.fsmodified;
}

void fsck_set_fsreadfd(int fd) {
    state.fsreadfd = fd;
}

int fsck_get_fsreadfd() {
    return state.fsreadfd;
}

void fsck_set_fswritefd(int fd) {
    state.fswritefd = fd;
}

int fsck_get_fswritefd() {
    return state.fswritefd;
}

void fsck_set_hotroot(char val) {
    state.hotroot = val;
}

char fsck_get_hotroot() {
    return state.hotroot;
}

void fsck_set_hotmount(char val) {
    state.hotmount = val;
}

char fsck_get_hotmount() {
    return state.hotmount;
}

void fsck_set_xmlcontrol(char val) {
    state.xmlControl = val;
}

char fsck_get_xmlcontrol() {
    return state.xmlControl;
}

void fsck_set_guicontrol(char val) {
    state.guiControl = val;
}

char fsck_get_guicontrol() {
    return state.guiControl;
}

void fsck_set_rebuild_btree(char val) {
    state.rebuildBTree = val;
}

char fsck_get_rebuild_btree() {
    return state.rebuildBTree;
}

void fsck_set_rebuild_options(int val) {
    state.rebuildOptions = val;
}

int fsck_get_rebuild_options() {
    return state.rebuildOptions;
}

void fsck_set_mode_setting(char val) {
    state.modeSetting = val;
}

char fsck_get_mode_setting() {
    return state.modeSetting;
}

void fsck_set_error_on_exit(char val) {
    state.errorOnExit = val;
}

char fsck_get_error_on_exit() {
    return state.errorOnExit;
}

void fsck_set_upgrading(int val) {
    state.upgrading = val;
}

int  fsck_get_upgrading() {
    return state.upgrading;
}

void fsck_set_lostAndFoundMode(int val) {
    state.lostAndFoundMode = val;
}

int fsck_get_lostAndFoundMode() {
    return state.lostAndFoundMode;
}

void fsck_set_cache_size(uint64_t val) {
    state.reqCacheSize = val;
}

uint64_t fsck_get_cachesize() {
    return state.reqCacheSize;
}

void fsck_set_block_size(long val) {
    state.gBlockSize = val;
}

long fsck_get_block_size() {
    return state.gBlockSize;
}

void fsck_set_detonator_run(int val) {
    state.detonatorRun = val;
}

int fsck_get_detonator_run() {
    return state.detonatorRun;
}

void fsck_set_cdevname(const char* val) {
    state.cdevname = val;
}

const char* fsck_get_cdevname() {
    return state.cdevname;
}

void fsck_set_lflag(char val) {
    state.lflag = val;
}
char fsck_get_lflag() {
    return state.lflag;
}

void fsck_set_nflag(char val) {
    state.nflag = val;
}

char fsck_get_nflag() {
    return state.nflag;
}

void fsck_set_yflag(char val) {
    state.yflag = val;
}

char fsck_get_yflag() {
    return state.yflag;
}

void fsck_set_preen(char val) {
    state.preen = val;
}

char fsck_get_preen() {
    return state.preen;
}

void fsck_set_force(char val) {
    state.force = val;
}

char fsck_get_force() {
    return state.force;
}

void fsck_set_quick(char val) {
    state.quick = val;
}

char fsck_get_quick() {
    return state.quick;
}

void fsck_set_debug(char val) {
    state.debug = val;
}

char fsck_get_debug() {
    return state.debug;
}

void fsck_set_disable_journal(char val) {
    state.disable_journal = val;
}

char fsck_get_disable_journal() {
    return state.disable_journal;
}

void fsck_set_scanflag(char val) {
    state.scanflag = val;
}

char fsck_get_scanflag() {
    return state.scanflag;
}

void fsck_set_embedded(char val) {
    state.embedded = val;
}

char fsck_get_embedded() {
    return state.embedded;
}

void fsck_set_repair_level(char val) {
    state.repLev = val;
}

char fsck_get_repair_level() {
    return state.repLev;
}

void fsck_set_chek_level(char val) {
    state.chkLev = val;
}

char fsck_get_chek_level() {
    return state.chkLev;
}

void fsck_set_dev_block_size(uint32_t val) {
    state.devBlockSize = val;
}

uint32_t fsck_get_dev_block_size() {
    return state.devBlockSize;
}

void fsck_set_dev_block_count(uint64_t val) {
    state.blockCount = val;
}

uint64_t fsck_get_dev_block_count() {
    return state.blockCount;
}

void fsck_set_mount_point(char* val) {
    state.mountpoint = val;
}

char* fsck_get_mount_point() {
    return state.mountpoint;
}

void fsck_set_device_writable(int val) {
    state.canWrite = val;
}

int fsck_get_writable() {
    return state.canWrite;
}

int fsck_get_verbosity_level () {
    return state.verbosityLevel;
}

void fsck_set_verbosity_level (int val) {
    state.verbosityLevel = val;
}
