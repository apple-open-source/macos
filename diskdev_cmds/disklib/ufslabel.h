
/* 
 * Copyright 1999 Apple Computer, Inc.
 *
 * ufslabel.h
 * - library routines to read/write the UFS disk label
 */

/*
 * Modification History:
 * 
 * Dieter Siegmund (dieter@apple.com)	Fri Nov  5 12:48:55 PST 1999
 * - created
 */
#ifndef _UFSLABEL
#define _UFSLABEL

#ifdef linux
typedef int boolean_t;
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif

boolean_t	ufslabel_get(int fd, void * name, int * len);
boolean_t	ufslabel_set(int fd, void * name, int len);
#endif
