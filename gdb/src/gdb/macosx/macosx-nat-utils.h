#ifndef __GDB_MACOSX_NAT_UTILS_H__
#define __GDB_MACOSX_NAT_UTILS_H__

const void *macosx_parse_plist (const char *plist_path);

void macosx_free_plist (const void **plist);

const char *macosx_get_plist_posix_value (const void *plist, const char* key);

const char *macosx_get_plist_string_value (const void *plist, const char* key);

char * macosx_filename_in_bundle (const char *filename, int mainline);


#endif /* __GDB_MACOSX_NAT_UTILS_H__ */
