/* -*- Mode: c++ -*-
 * $Id: xotclUtil.c,v 1.3 2006/02/18 22:17:33 neumann Exp $
 *  
 *  Extended Object Tcl (XOTcl)
 *
 *  Copyright (C) 1999-2008 Gustaf Neumann, Uwe Zdun
 *
 *
 *  xotclUtil.c --
 *  
 *  Utility functions
 *  
 */

#include "xotclInt.h"

char *
XOTcl_ltoa(char *buf, long i, int *len)  /* fast version of sprintf(buf,"%ld",l); */ {
  int nr_written, negative;
  char tmp[LONG_AS_STRING], *pointer = &tmp[1], *string, *p;
  *tmp = 0;
  
  if (i<0) {
    i = -i;
    negative = nr_written = 1;
  } else 
    nr_written = negative = 0;
  
  do {
    nr_written++;
    *pointer++ = i%10 + '0';
    i/=10;
  } while (i);
  
  p = string = buf;
  if (negative)
    *p++ = '-';
  
  while ((*p++ = *--pointer));   /* copy number (reversed) from tmp to buf */
  if (len) *len = nr_written;
  return string;
}


static char *alphabet = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
static int blockIncrement = 8;
/*
static char *alphabet = "ab";
static int blockIncrement = 2;
*/
static unsigned char chartable[255] = {0};


char *
XOTclStringIncr(XOTclStringIncrStruct *iss) {
  char newch, *currentChar;

  currentChar = iss->buffer + iss->bufSize - 2;
  newch = *(alphabet + chartable[(unsigned)*currentChar]);
    
  while (1) {
    if (newch) { /* no overflow */
      *currentChar = newch;
      break;
    } else {     /* overflow */
      *currentChar = *alphabet; /* use first char from alphabet */
      currentChar--;
      assert(currentChar >= iss->buffer);

      newch = *(alphabet + chartable[(unsigned)*currentChar]);
      if (currentChar < iss->start) {
	iss->length++;
	if (currentChar == iss->buffer) {
	  size_t newBufSize = iss->bufSize + blockIncrement;
	  char *newBuffer = ckalloc(newBufSize);
	  currentChar = newBuffer+blockIncrement;
	  /*memset(newBuffer, 0, blockIncrement);*/
	  memcpy(currentChar, iss->buffer, iss->bufSize);
	  *currentChar = newch;
	  iss->start = currentChar;
	  ckfree(iss->buffer);
	  iss->buffer = newBuffer;
	  iss->bufSize = newBufSize;
	} else {
	  iss->start = currentChar;
	}
      }
    }
  }
  assert(iss->buffer[iss->bufSize-1] == 0);
  assert(iss->buffer[iss->bufSize-2] != 0);
  assert(iss->length < iss->bufSize);
  assert(iss->start + iss->length + 1 == iss->buffer + iss->bufSize);

  return iss->start;
}


void
XOTclStringIncrInit(XOTclStringIncrStruct *iss) {
  char *p;
  int i = 0;
  const size_t bufSize = blockIncrement>2 ? blockIncrement : 2;

  for (p=alphabet; *p; p++) {
    chartable[(int)*p] = ++i;
  }

  iss->buffer = ckalloc(bufSize);
  memset(iss->buffer, 0, bufSize);
  iss->start    = iss->buffer + bufSize-2;
  iss->bufSize  = bufSize;
  iss->length   = 1;
  /*
    for (i=1; i<50; i++) {
      XOTclStringIncr(iss);
      fprintf(stderr, "string '%s' (%d)\n",  iss->start, iss->length);
    }
  */
}

void
XOTclStringIncrFree(XOTclStringIncrStruct *iss) {
  ckfree(iss->buffer);
}
