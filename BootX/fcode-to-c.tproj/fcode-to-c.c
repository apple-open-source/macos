/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 *  fcode-to-c.c - Takes an OF FCODE image and generates a .c file.
 *
 *  Copyright (c) 1999-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *gToolName;


int main(int argc, char **argv)
{
  char varName[256], inFileName[256], outFileName[256], *tmpStr;
  FILE *inFile, *outFile;
  long tc, cnt;
  
  gToolName = *argv;
  
  if (argc != 3) {
    fprintf(stderr, "Usage: %s var-name file.fcode\n", gToolName);
    return -1;
  }
  
  strncpy(varName, argv[1], 255);
  
  strncpy(inFileName, argv[2], 255);
  strncpy(outFileName, argv[2], 255);
  
  tmpStr = outFileName;
  while ((*tmpStr != '.') && (*tmpStr != '\0')) tmpStr++;
  if ((*tmpStr == '\0') || strncmp(tmpStr, ".fcode", 6)) {
    fprintf(stderr, "Usage: %s var-name file.fcode\n", gToolName);
    return -1;
  }
  
  tmpStr[1] = 'c';
  tmpStr[2] = '\0';
  
  inFile = fopen(inFileName, "rb");
  if (inFile == NULL) {
    fprintf(stderr, "%s: failed to open %s\n", gToolName, inFileName);
    return -1;
  }

  outFile = fopen(outFileName, "w");
  if (outFile == NULL) {
    fprintf(stderr, "%s: failed to open %s\n", gToolName, outFileName);
    return -1;
  }
  
  fprintf(outFile, "const char %s[] = {", varName);
  
  cnt = 0;
  while ((tc = fgetc(inFile)) != EOF) {
    if (cnt == 0) fputc('\n', outFile);
    fprintf(outFile, "0x%02x,", (unsigned char)tc);
    if (cnt++ == 0x20) cnt = 0;
  }
  
  fprintf(outFile, "\n};");

  fclose(inFile);
  fclose(outFile);

  return 0;
}
