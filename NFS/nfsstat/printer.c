/*
 * Copyright (c) 1999-2008 Apple Inc.  All rights reserved.
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

#include <pwd.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/mount.h>
#include <nfs/rpcv2.h>

#include "json_support.h"
#include "printer.h"

/* Printf Printer */

void printf_null(void)
{

}

void printf_newline(void)
{
    printf("\n");
}

void printf_open(const char *prefix, const char *title)
{
    if (title) {
        printf("%s%s:\n", prefix ? prefix : "", title);
    } else {
        printf("%s", prefix ? prefix : "");
    }
}

void printf_open_array(const char *prefix, const char *title, int *flags)
{
    if (flags)
        printf("%s%s: 0x%x", prefix, title, *flags);
    else
        printf("%s%s:", prefix, title);
}

void printf_add_array_str(char sep, const char *flag, const char *value)
{
    printf("%c%s%s", sep, flag, value);
}

void printf_add_array_num(char sep, const char *flag, long value)
{
    printf("%c%s%ld", sep, flag, value);
}

void printf_close_array(int opened_with_flags)
{
    printf_newline();
}

void printf_open_locations(const char *prefix, const char *title, uint32_t flags, uint32_t loc, uint32_t serv, uint32_t addr, int invalid)
{
    printf("%s%s: 0x%x %d %d %d: ", prefix, title, flags, loc, serv, addr);
    if (invalid) {
        printf("<invalid>\n");
    }
}

void printf_add_locations(const char *path, const char *server, uint32_t addrcount, char **addrs)
{
    printf("%s @ %s", path, server);
    char *addrstr = NULL;
    for (uint32_t addr = 0; addr < addrcount; addr++) {
        addrstr = addrs[addr];
        if (addrstr == NULL)
            break;
        printf("%s%s", !addr ? " (" : ",", addrs[addr]);
      }
    if (addrstr)
        printf(")");
}

void printf_mount_header(const char *to, const char *from)
{
    printf("%s from %s\n", to, from);
}

void printf_mount_fh(uint32_t fh_len, unsigned char *fh_data)
{
    uint32_t i;
    printf("     fh %d ", fh_len);
    for (i=0; i < fh_len; i++)
        printf("%02x", fh_data[i] & 0xff);
    printf("\n");
}

void printf_exports(struct nfs_export_stat_rec *rec)
{
    printf("%12llu  %12llu  %12llu  %s\n", rec->ops, rec->bytes_read, rec->bytes_written, rec->path);
}

void printf_active_users(struct nfs_user_stat_user_rec *rec, const char *addr, struct passwd *pw, int printuuid, time_t hr, time_t min, time_t sec)
{
    printf("%12llu  %12llu  %12llu  %1ld:%02ld:%02ld  ",
           rec->ops, rec->bytes_read, rec->bytes_written, hr, min, sec);
    if (printuuid)
        printf("%-8u ", rec->uid);
    else
        printf("%-8.8s ", pw->pw_name);
    printf("%s\n", addr);
}

void printf_intpr(const char *title_format, const char *t1, uint64_t e1, const char *t2, uint64_t e2, const char *t3, uint64_t e3,
                   const char *t4, uint64_t e4, const char *t5, uint64_t e5, const char *t6, uint64_t e6)
{
    printf(title_format, t1 ? t1 : "", t2 ? t2 : "", t3 ? t3 : "",t4 ? t4 : "", t5 ? t5 : "", t6 ? t6 : "");
    if (t1)
        printf("%12llu ", e1);
    if (t2)
        printf("%12llu ", e2);
    if (t3)
        printf("%12llu ", e3);
    if (t4)
        printf("%12llu ", e4);
    if (t5)
        printf("%12llu ", e5);
    if (t6)
        printf("%12llu ", e6);
    printf("\n");
}

/* Printf Printer */

/* Json Printer */

CFMutableDictionaryRef json_dicts[20]; /* We support up to 20 nested dictionaries */
int json_dicts_idx = -1;
#define JSON_CURRENT_DICT json_dicts[json_dicts_idx]

CFMutableArrayRef json_arrs[20]; /* We support up to 20 nested arrays */
int json_arrs_idx = -1;
#define JSON_CURRENT_ARR json_arrs[json_arrs_idx]

void json_dump(void)
{
    if (json_dicts[0]) {
        json_print_cf_object(json_dicts[0], NULL);
    }
}

int json_title(const char * __restrict title, ...)
{
    return -1;
}

void json_open_dictionary(const char *prefix, const char *title)
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (json_dicts_idx != -1) {
        json_dict_add_dict(JSON_CURRENT_DICT, title ? title : "", dict);

    }
    json_dicts_idx++;
    JSON_CURRENT_DICT = dict;
}

void json_open_dictionary_inside_array(const char *prefix, const char *title)
{
    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (json_dicts_idx != -1) {
        json_arr_add_dict(JSON_CURRENT_ARR, dict);
    }
    json_dicts_idx++;
    JSON_CURRENT_DICT = dict;
}

void json_close_dictionary(void) {
    json_dicts_idx--;
}

void json_open_array(const char *prefix, const char *title, int *flags)
{
    CFMutableArrayRef arr = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    if (flags) {
        json_open_dictionary(PRINTER_NO_PREFIX, title);
        char bitmaskbuf[128];
        sprintf(bitmaskbuf, "0x%x", *flags);
        json_dict_add_str(JSON_CURRENT_DICT, "Bitmask", bitmaskbuf);
        json_dict_add_array(JSON_CURRENT_DICT, "Flags", arr);
    } else {
        json_dict_add_array(JSON_CURRENT_DICT, title, arr);
    }
    json_arrs_idx++;
    JSON_CURRENT_ARR = arr;
}

void json_add_array_str(char sep, const char *flag, const char* value)
{
    char valbuf[128];
    sprintf(valbuf, "%s%s", flag, value);
    json_arr_add_str(JSON_CURRENT_ARR, valbuf);
}

void json_add_array_num(char sep, const char *flag, long value)
{
    char valbuf[128];
    sprintf(valbuf, "%s%ld", flag, value);
    json_arr_add_str(JSON_CURRENT_ARR, valbuf);
}

void json_close_array(int opened_with_flags)
{
    json_arrs_idx--;
    if (opened_with_flags) {
        json_close_dictionary();
    }
}

void json_open_locations(const char *prefix, const char *title, uint32_t flags, uint32_t loc, uint32_t serv, uint32_t addr, int invalid)
{
    json_open_dictionary(PRINTER_NO_PREFIX, title);
    json_dict_add_num(JSON_CURRENT_DICT, "Flags", &flags, sizeof(flags));
    json_dict_add_num(JSON_CURRENT_DICT, "Loc", &loc, sizeof(loc));
    json_dict_add_num(JSON_CURRENT_DICT, "Serv", &serv, sizeof(serv));
    json_dict_add_num(JSON_CURRENT_DICT, "Addr", &addr, sizeof(addr));
    json_dict_add_str(JSON_CURRENT_DICT, "Status", invalid ? "invalid" : "valid");
}

void json_add_locations(const char *path, const char *server, uint32_t addrcount, char **addrs)
{
    json_dict_add_str(JSON_CURRENT_DICT, "Export", path);
    json_dict_add_str(JSON_CURRENT_DICT, "Server", server);
    json_open_array(PRINTER_NO_PREFIX, "Locations", NULL);
    for (uint32_t addr = 0; addr < addrcount; addr++) {
        if (addrs[addr] == NULL)
            break;
        json_arr_add_str(JSON_CURRENT_ARR, addrs[addr]);
    }
    json_close_array(0);
}

void json_mount_header(const char *to, const char *from)
{
    json_open_dictionary(PRINTER_NO_PREFIX, from);
    json_dict_add_str(JSON_CURRENT_DICT, "Server", to);
}

void json_mount_fh(uint32_t fh_len, unsigned char *fh_data)
{
    uint32_t i, n = 0;
    char handlebuf[NFS_MAX_FH_SIZE];
    json_open_dictionary(PRINTER_NO_PREFIX, "filehandle");
    json_dict_add_num(JSON_CURRENT_DICT, "Length", &fh_len, sizeof(fh_len));
    for (i = 0; i < fh_len; i++)
        n += sprintf(handlebuf + n, "%02x", fh_data[i] & 0xff);
    json_dict_add_str(JSON_CURRENT_DICT, "Handle", handlebuf);
    json_close_dictionary();
}

void json_exports(struct nfs_export_stat_rec *rec)
{
    json_open_dictionary(PRINTER_NO_PREFIX, rec->path);
    json_dict_add_num(JSON_CURRENT_DICT, "Requests", &rec->ops, sizeof(rec->ops));
    json_dict_add_num(JSON_CURRENT_DICT, "Read Bytes", &rec->bytes_read, sizeof(rec->bytes_read));
    json_dict_add_num(JSON_CURRENT_DICT, "Write Bytes", &rec->bytes_written, sizeof(rec->bytes_written));
    json_close_dictionary();
}

void json_active_users(struct nfs_user_stat_user_rec *rec, const char *addr, struct passwd *pw, int printuuid, time_t hr, time_t min, time_t sec)
{
    char timebuf[128], dictbuf[128];
    if (printuuid) {
        sprintf(dictbuf, "%u@%s", rec->uid, addr);
        json_open_dictionary(PRINTER_NO_PREFIX, dictbuf);
        json_dict_add_num(JSON_CURRENT_DICT, "Uuid", &rec->uid, sizeof(rec->uid));
    } else {
        sprintf(dictbuf, "%s@%s", pw->pw_name, addr);
        json_open_dictionary(PRINTER_NO_PREFIX, dictbuf);
        json_dict_add_str(JSON_CURRENT_DICT, "User", pw->pw_name);
    }
    json_dict_add_num(JSON_CURRENT_DICT, "Requests", &rec->ops, sizeof(rec->ops));
    json_dict_add_num(JSON_CURRENT_DICT, "Read Bytes", &rec->bytes_read, sizeof(rec->bytes_read));
    json_dict_add_num(JSON_CURRENT_DICT, "Write Bytes", &rec->bytes_written, sizeof(rec->bytes_written));
    sprintf(timebuf, "%1ld:%02ld:%02ld", hr, min, sec);
    json_dict_add_str(JSON_CURRENT_DICT, "Idle", timebuf);
    json_close_dictionary();
}

void json_intpr(const char *title_format, const char *t1, uint64_t e1, const char *t2, uint64_t e2, const char *t3, uint64_t e3,
                   const char *t4, uint64_t e4, const char *t5, uint64_t e5, const char *t6, uint64_t e6)
{
    if (t1) {
        json_dict_add_num(JSON_CURRENT_DICT, t1, &e1, sizeof(e1));
    }
    if (t2) {
        json_dict_add_num(JSON_CURRENT_DICT, t2, &e2, sizeof(e2));
    }
    if (t3) {
        json_dict_add_num(JSON_CURRENT_DICT, t3, &e3, sizeof(e3));
    }
    if (t4) {
        json_dict_add_num(JSON_CURRENT_DICT, t4, &e4, sizeof(e4));
    }
    if (t5) {
        json_dict_add_num(JSON_CURRENT_DICT, t5, &e5, sizeof(e5));
    }
    if (t6) {
        json_dict_add_num(JSON_CURRENT_DICT, t6, &e6, sizeof(e6));
    }
}

/* Json Printer */
