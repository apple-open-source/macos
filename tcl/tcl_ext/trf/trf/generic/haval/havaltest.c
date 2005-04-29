/*
 *  havaltest.c:  specifies a test program for the HAVAL hashing library.
 *
 *  Arguments for the test program:
 *
 *      (none)    - hash input from stdin
 *      ? or -?   - show help menu
 *      -c        - hash certification data
 *      -e        - test whether your machine is little-endian
 *      -mstring  - hash message (string of chars)
 *      -s        - test speed
 *      file_name - hash file
 *
 *  Makefile for the testing program:
 * 
 *         CC=acc
 *         CFLAGS=-fast
 *         
 *         haval: haval.o havaltest.o 
 *                ${CC} ${CFLAGS} haval.o havaltest.o -o $@
 *         haval.o havaltest.o: havalapp.h
 *         
 *         clean:
 *                /usr/bin/rm -f *.o haval
 *
 *  Author:     Yuliang Zheng
 *              Department of Computer Science
 *              University of Wollongong
 *              Wollongong, NSW 2522, Australia
 *              Email: yuliang@cs.uow.edu.au
 *              Voice: +61 42 21 4331 (office)
 *
 *  Date:       June 1993
 *
 *      Copyright (C) 1993 by C^3SR. All rights reserved. 
 *      This program may not be sold or used as inducement to
 *      buy a product without the written permission of C^3SR.
 */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include "havalapp.h"
#include "haval.h"

#define NUMBER_OF_BLOCKS 5000               /* number of test blocks */
#define BLOCK_SIZE       1000               /* number of bytes in a block */

static void haval_speed (void);             /* test the speed of HAVAL */
static void haval_cert (void);              /* hash test data set */
static void haval_print (unsigned char *);  /* print a fingerprint */
static int  little_endian (void);           /* test endianity */

int main (argc, argv)
int  argc;
char *argv[];
{
  int           i;
  unsigned char fingerprint[FPTLEN >> 3];

  if (argc <= 1) {
    haval_stdin ();                                /* filter */
  }
  for (i = 1; i < argc; i++) {
    if ((argv[i][0] == '?') ||                      /* show help info */
        (argv[i][0] == '-' && argv[i][1] == '?')) {
      printf (" (none)     hash input from stdin\n");
      printf (" ? or -?    show help menu\n");
      printf (" -c         hash certification data\n");
      printf (" -e         test endianity\n");
      printf (" -mstring   hash message\n");
      printf (" -s         test speed\n");
      printf (" file_name  hash file\n");
    } else if (argv[i][0] == '-' && argv[i][1] == 'm') {  /* hash string */
      haval_string (argv[i]+2, fingerprint);
      printf ("HAVAL(\"%s\") = ", argv[i]+2);
      haval_print (fingerprint);
      printf ("\n");
    } else if (strcmp (argv[i], "-c") == 0) {      /* hash test set */
      haval_cert ();
    } else if (strcmp (argv[i], "-s") == 0) {      /* test speed */
      haval_speed ();
    } else if (strcmp (argv[i], "-e") == 0) {      /* test endianity */
      if (little_endian()) {
        printf ("Your machine is little-endian.\n");
        printf ("You may define LITTLE_ENDIAN to speed up processing.\n");
      } else {
        printf ("Your machine is NOT little-endian.\n");
        printf ("You must NOT define LITTLE_ENDIAN.\n");
      }
    } else {                                       /* hash file */
      if (haval_file (argv[i], fingerprint)) {
        printf ("%s can not be opened !\n= ", argv[i]);
      } else {
        printf ("HAVAL(File %s) = ", argv[i]);
        haval_print (fingerprint);
        printf ("\n");
      }
    }
  }
  return (0);
}

/* test the speed of HAVAL */
static void haval_speed (void)
{
  haval_state   state;
  unsigned char buff[BLOCK_SIZE];
  unsigned char fingerprint[FPTLEN >> 3];
  time_t        start_time, end_time;
  double        elapsed_time;
  unsigned int  i;

  printf ("Test the speed of HAVAL (PASS = %d, FPTLEN = %d bits).\n", PASS, FPTLEN);
  printf ("Hashing %d %d-byte blocks ...\n", NUMBER_OF_BLOCKS, BLOCK_SIZE);

  /* initialize test block */
  for (i = 0; i < BLOCK_SIZE; i++) {
    buff[i] = ~0;
  }

  /* get start time */
  time (&start_time);

  /* hash */
  haval_start (&state);
  for (i = 0; i < NUMBER_OF_BLOCKS; i++) {
    haval_hash (&state, buff, BLOCK_SIZE);
  }
  haval_end (&state, fingerprint);

  /* get end time */
  time (&end_time);

  /* get elapsed time */
  elapsed_time = difftime(end_time, start_time);

  if (elapsed_time > 0.0) {
    printf ("Elapsed Time = %3.1f seconds\n", elapsed_time);
    printf ("       Speed = %4.2f MBPS (megabits/second)\n",
    (NUMBER_OF_BLOCKS * BLOCK_SIZE * 8)/(1.0E6 * elapsed_time));
  } else {
    printf ("not enough blocks !\n");
  }
}

/* hash a set of certification data and print the results.  */
static void haval_cert (void)
{
  unsigned int  i;
  char          *str;
  unsigned char fingerprint[FPTLEN >> 3];

  printf ("\n");
  printf ("HAVAL certification data (PASS=%d, FPTLEN=%d):", PASS, FPTLEN);
  printf ("\n");

  str = "";
  haval_string (str, fingerprint);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint);
  printf ("\n");

  str = "a";
  haval_string (str, fingerprint);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint);
  printf ("\n");

  str = "HAVAL";
  haval_string (str, fingerprint);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint);
  printf ("\n");

  str = "0123456789";
  haval_string (str, fingerprint);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint);
  printf ("\n");

  str = "abcdefghijklmnopqrstuvwxyz";
  haval_string (str, fingerprint);
  printf ("HAVAL(\"%s\") = ", str);
  haval_print (fingerprint);
  printf ("\n");

  str = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  haval_string (str, fingerprint);
  printf ("HAVAL(\"%s\")\n      = ", str);
  haval_print (fingerprint);
  printf ("\n");

  str = "pi.frac";
  if (haval_file (str, fingerprint)) {
    printf ("%s can not be opened !\n", str);
  } else {
    printf ("HAVAL(File %s) = ", str);
    haval_print (fingerprint);
    printf ("\n");
  }
}

/* test endianity */
static int little_endian(void)
{
  unsigned long *wp;
  unsigned char str[4] = {'A', 'B', 'C', 'D'};

  wp = (unsigned long *)str;
  if (str[0] == (unsigned char)( *wp & 0xFF)) {
    return (1);                       /* little endian */
  } else {
    return (0);                       /* big endian */
  }
}

/* print a fingerprint in hexadecimal */
static void haval_print (unsigned char fingerprint[FPTLEN >> 3])
{
  int i;

  for (i = 0; i < FPTLEN >> 3; i++) {
    printf ("%02X", fingerprint[i]);
  }
}




