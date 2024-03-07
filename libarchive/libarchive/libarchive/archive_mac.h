#include "archive.h"
#include <TargetConditionals.h>

#ifndef ARCHIVE_MAC_H
#define ARCHIVE_MAC_H

#if TARGET_OS_MAC && !TARGET_OS_IPHONE
#define HAVE_MAC_QUARANTINE 1

#include <quarantine.h>
#endif /* TARGET_OS_MAC */

void archive_read_get_quarantine_from_fd(struct archive *a, int fd);

#endif
