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
#include <os/log.h>
extern void rtadvdLog(int level, const char *format, ...);

#define errorlog(__format, ...)            \
rtadvdLog(OS_LOG_TYPE_DEFAULT, __format, ## __VA_ARGS__)

#define noticelog(__format, ...)            \
rtadvdLog(OS_LOG_TYPE_DEFAULT, __format, ## __VA_ARGS__)

#define infolog(__format, ...)            \
rtadvdLog(OS_LOG_TYPE_INFO, __format, ## __VA_ARGS__)

#define debuglog(__format, ...)            \
rtadvdLog(OS_LOG_TYPE_DEBUG, __format, ## __VA_ARGS__)
