#ifndef ARCHIVE_MAC_H
#define ARCHIVE_MAC_H

#include <TargetConditionals.h>

#if TARGET_OS_MAC && !TARGET_OS_IPHONE
#define HAVE_MAC_QUARANTINE 1
#include <quarantine.h>

int archive_write_disk_set_quarantine(struct archive *, qtn_file_t);
void archive_read_get_quarantine_from_fd(struct archive *a, int fd);
#endif /* TARGET_OS_MAC */

#endif
