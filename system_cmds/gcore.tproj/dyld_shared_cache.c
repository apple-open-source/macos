/*
 * Copyright (c) 2016 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "dyld_shared_cache.h"
#include "utils.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <fts.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <TargetConditionals.h>

static const size_t dyld_cache_header_size = sizeof (struct copied_dyld_cache_header);

/*
 * The shared cache must both contain the magic ID and
 * match the uuid we discovered via dyld's information.
 * Assumes that the dyld_cache_header grows in a binary compatible fashion.
 */
bool
get_uuid_from_shared_cache_mapping(const void *addr, size_t size, uuid_t out)
{
    const struct copied_dyld_cache_header *ch = addr;
    if (size < sizeof (*ch))
        return false;
    static const char prefix[] = "dyld_v";
    if (strncmp(ch->magic, prefix, strlen(prefix)) != 0)
        return false;
    uuid_copy(out, ch->uuid);
    return true;
}

/*
 * Look in the known places to see if we can find this one ..
 */
char *
shared_cache_filename(const uuid_t uu)
{
    assert(!uuid_is_null(uu));
    static char *sc_argv[] = {
#if TARGET_OS_OSX
        "/System/Library/dyld",
#elif TARGET_OS_IPHONE
        "/System/Library/Caches/com.apple.dyld",
#else
#error undefined
#endif
        NULL
    };
    char *nm = NULL;
    FTS *fts = fts_open(sc_argv, FTS_NOCHDIR | FTS_LOGICAL | FTS_XDEV, NULL);
    if (NULL != fts) {
        FTSENT *fe;
        while (NULL != (fe = fts_read(fts))) {
            if ((fe->fts_info & FTS_F) == 0 ||
                (fe->fts_info & FTS_ERR) != 0)
                continue;

            static const char prefix[] = "dyld_shared_cache_";
            if (strncmp(fe->fts_name, prefix, strlen(prefix)) != 0)
                continue;

            if (fe->fts_statp->st_size < (off_t)dyld_cache_header_size)
                continue;

            int d = open(fe->fts_accpath, O_RDONLY);
            if (-1 == d) {
                if (OPTIONS_DEBUG(opt, 1))
                    printf("%s: cannot open - %s\n", fe->fts_accpath, strerror(errno));
                continue;
            }
            void *addr = mmap(NULL, dyld_cache_header_size, PROT_READ, MAP_PRIVATE, d, 0);
            close(d);
            if ((void *)-1 == addr) {
                if (OPTIONS_DEBUG(opt, 1))
                    printf("%s: cannot mmap - %s\n", fe->fts_accpath, strerror(errno));
                continue;
            }
            uuid_t scuuid;
            uuid_clear(scuuid);
            if (get_uuid_from_shared_cache_mapping(addr, dyld_cache_header_size, scuuid)) {
                if (uuid_compare(uu, scuuid) == 0)
                    nm = strdup(fe->fts_accpath);
                else if (OPTIONS_DEBUG(opt, 3)) {
                    uuid_string_t scstr;
                    uuid_unparse_lower(scuuid, scstr);
                    printf("%s: shared cache mismatch (%s)\n", fe->fts_accpath, scstr);
                }
            }
            munmap(addr, dyld_cache_header_size);
            if (NULL != nm)
                break;
        }
    }
    if (fts)
        fts_close(fts);
    return nm;
}
