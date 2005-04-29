/*
 * Copyright (c) 2003-2005 Apple Computer, Inc. All rights reserved.
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

#include <stdio.h>
#include <stdlib.h>
#include <err.h>

int main(int argc, char *argv[]) {

  FILE *f;
  int ch;

  char *varname = NULL;

  if(argc > 3) {
    fprintf(stderr, "Usage: %s varname [ input ]\n", getprogname());
    exit(1);
  }

  if(argc == 3) {
    f = fopen(argv[2], "r");
    if(f == NULL) {
      err(1, "Could not open %s", argv[2]);
    }
  } else {
    f = stdin;
  }

  varname = argv[1];

  printf("const char %s[] = {\n", varname);

  while(EOF != (ch= fgetc(f)) && !feof(f)) {

    printf("%#x, ", ch);
  }

  fclose(f);

  printf("\n};\n");

  return 0;
}
