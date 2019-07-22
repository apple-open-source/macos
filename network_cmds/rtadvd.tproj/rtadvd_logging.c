/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */
#include <assert.h>
#include <os/log_private.h>

#define kRtadvdLoggerID        "com.apple.rtadvd"
static os_log_t rtadvdLogger = NULL;                        /* Handle for Logger */

static boolean_t rtadvd_logger_create(void);

static boolean_t
rtadvd_logger_create(void)
{
    assert(rtadvdLogger == NULL);
    rtadvdLogger = os_log_create(kRtadvdLoggerID, "daemon");

    if (rtadvdLogger == NULL) {
        os_log_error(OS_LOG_DEFAULT, "Couldn't create os log object");
    }

    return (rtadvdLogger != NULL);
}

void
rtadvdLog(int level, const char *format, ...)
{
    va_list args;

    if (rtadvdLogger == NULL && !rtadvd_logger_create()) {
        return;
    }

    va_start(args, format);
    os_log_with_args(rtadvdLogger, level, format, args, __builtin_return_address(0));
    va_end(args);
    return;
}
