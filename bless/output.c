/*
 *  output.c
 *  bless
 *
 *  Created by ssen on Fri Apr 20 2001.
 *  Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include "bless.h"

int verboseprintf(char const *fmt, ...) {
  int ret = 0;
  va_list ap;

  va_start(ap, fmt);

  if(config.verbose) {
    ret = vfprintf(stdout, fmt, ap);
  }
  va_end(ap);

  return ret;
}


int regularprintf(char const *fmt, ...) {
  int ret = 0;
  va_list ap;

  va_start(ap, fmt);

  if(config.verbose || !config.quiet) {
    ret = vfprintf(stdout, fmt, ap);
  }
  va_end(ap);

  return ret;
}


int errorprintf(char const *fmt, ...) {
  int ret = 0;
  va_list ap;

  va_start(ap, fmt);

  fprintf(stderr, "ERROR(%s):", PROGRAM);
  ret = vfprintf(stderr, fmt, ap);

  va_end(ap);

  return ret;
}


int warningprintf(char const *fmt, ...) {
  int ret = 0;
  va_list ap;

  va_start(ap, fmt);

  fprintf(stderr, "WARNING(%s):", PROGRAM);
  ret = vfprintf(stderr, fmt, ap);

  va_end(ap);

  return ret;
}
