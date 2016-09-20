/*
 * Copyright (c) 2015 Apple Inc.  All rights reserved.
 */

#include "options.h"
#include "utils.h"
#include "region.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <libutil.h>

void
err_mach(kern_return_t kr, const struct region *r, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (0 != kr)
        printf("%s: ", pgm);
    if (NULL != r)
        printf("%llx-%llx ", R_ADDR(r), R_ENDADDR(r));
    vprintf(fmt, ap);
    va_end(ap);

    if (0 != kr) {
        printf(": %s (%x)", mach_error_string(kr), kr);
        switch (err_get_system(kr)) {
            case err_get_system(err_mach_ipc):
                /* 0x10000000  == (4 << 26) */
                printf(" => fatal\n");
                exit(127);
            default:
                putchar('\n');
                break;
        }
    } else
        putchar('\n');
}

void
printr(const struct region *r, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    if (NULL != r)
        printf("%llx-%llx ", R_ADDR(r), R_ENDADDR(r));
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

/*
 * Print power-of-1024 sizes in human-readable form
 */
const char *
str_hsize(hsize_str_t hstr, uint64_t size)
{
    humanize_number(hstr, sizeof (hsize_str_t) - 1, size, "",
                    HN_AUTOSCALE, HN_B | HN_NOSPACE | HN_DECIMAL | HN_IEC_PREFIXES);
    return hstr;
}

/*
 * Put two strings together separated by a '+' sign
 * If the string gets too long, then add an elipsis and
 * stop concatenating it.
 */
char *
strconcat(const char *s0, const char *s1, size_t maxlen)
{
    const char ellipsis[] = "...";
    const char junction[] = ", ";
    const size_t s0len = strlen(s0);
    size_t nmlen = s0len + strlen(s1) + strlen(junction) + 1;
    if (maxlen > strlen(ellipsis) && nmlen > maxlen) {
        if (strcmp(s0 + s0len - strlen(ellipsis), ellipsis) == 0)
            return strdup(s0);
        s1 = ellipsis;
        nmlen = s0len + strlen(s1) + strlen(junction) + 1;
    }
    char *p = malloc(nmlen);
    if (p) {
        strlcpy(p, s0, nmlen);
        strlcat(p, junction, nmlen);
        strlcat(p, s1, nmlen);
    }
    return p;
}
