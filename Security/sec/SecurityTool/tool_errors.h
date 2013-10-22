//
//  tool_errors.h
//  sec
//
//  Created by Mitch Adler on 1/9/13.
//
//

//
// These functions should be deprectaed!
// Try to find a better way instead of using them.
//

#ifndef _TOOL_ERRORS_H_
#define _TOOL_ERRORS_H_

#include <stdarg.h>
#include <stdio.h>
#include "SecurityTool/SecurityTool.h"

static inline const char *
sec_errstr(int err)
{
    const char *errString;
    static char buffer[12];
    
    sprintf(buffer, "%d", err);
    errString = buffer;
#if 0
    if (IS_SEC_ERROR(err))
        errString = SECErrorString(err);
    else
        errString = cssmErrorString(err);
#endif
    return errString;
}

static inline void
sec_error(const char *msg, ...)
{
    va_list args;
    
    fprintf(stderr, "%s: ", prog_name);
    
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);
    
    fprintf(stderr, "\n");
}

static inline void
sec_perror(const char *msg, int err)
{
    sec_error("%s: %s", msg, sec_errstr(err));
}



#endif
