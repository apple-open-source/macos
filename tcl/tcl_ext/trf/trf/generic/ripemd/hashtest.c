/********************************************************************\
 *
 *      FILE:     hashtest.c
 *
 *      CONTENTS: test file for sample C-implementation of
 *                RIPEMD-160 and RIPEMD128
 *        * command line arguments:                                         
 *           filename  -- compute hash code of file read binary     
 *           -sstring  -- print string & hashcode                   
 *           -t        -- perform time trial                        
 *           -x        -- execute standard test suite, ASCII input
 *        * for linkage with rmd128.c: define RMDsize as 128
 *          for linkage with rmd160.c: define RMDsize as 160 (default)
 *      TARGET:   any computer with an ANSI C compiler
 *
 *      AUTHOR:   Antoon Bosselaers, ESAT-COSIC
 *      DATE:     18 April 1996
 *      VERSION:  1.1
 *      HISTORY:  bug in RMDonemillion() corrected
 *
 *      Copyright (c) Katholieke Universiteit Leuven
 *      1996, All Rights Reserved
 *
\********************************************************************/
#ifndef RMDsize
#define RMDsize 160
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#if RMDsize == 128
#include "rmd128.h"
#elif RMDsize == 160
#include "rmd160.h"
#endif

#define TEST_BLOCK_SIZE 8000
#define TEST_BLOCKS 1250
#define TEST_BYTES ((long)TEST_BLOCK_SIZE * (long)TEST_BLOCKS)

/********************************************************************/

byte *RMD(byte *message)
/*
 * returns RMD(message)
 * message should be a string terminated by '\0'
 */
{
   dword         MDbuf[RMDsize/32];   /* contains (A, B, C, D(, E))   */
   static byte   hashcode[RMDsize/8]; /* for final hash-value         */
   dword         X[16];               /* current 16-word chunk        */
   word          i;                   /* counter                      */
   dword         length;              /* length in bytes of message   */
   dword         nbytes;              /* # of bytes not yet processed */

   /* initialize */
   MDinit(MDbuf);
   length = (dword)strlen((char *)message);

   /* process message in 16-word chunks */
   for (nbytes=length; nbytes > 63; nbytes-=64) {
      for (i=0; i<16; i++) {
         X[i] = BYTES_TO_DWORD(message);
         message += 4;
      }
      compress(MDbuf, X);
   }                                    /* length mod 64 bytes left */

   /* finish: */
   MDfinish(MDbuf, message, length, 0);

   for (i=0; i<RMDsize/8; i+=4) {
      hashcode[i]   =  MDbuf[i>>2];         /* implicit cast to byte  */
      hashcode[i+1] = (MDbuf[i>>2] >>  8);  /*  extracts the 8 least  */
      hashcode[i+2] = (MDbuf[i>>2] >> 16);  /*  significant bits.     */
      hashcode[i+3] = (MDbuf[i>>2] >> 24);
   }

   return (byte *)hashcode;
}

/********************************************************************/

byte *RMDbinary(char *fname)
/*
 * returns RMD(message in file fname)
 * fname is read as binary data.
 */
{
   FILE         *mf;                  /* pointer to file <fname>      */
   byte          data[1024];          /* contains current mess. block */
   dword         nbytes;              /* length of this block         */
   dword         MDbuf[RMDsize/32];   /* contains (A, B, C, D(, E))   */
   static byte   hashcode[RMDsize/8]; /* for final hash-value         */
   dword         X[16];               /* current 16-word chunk        */
   word          i, j;                /* counters                     */
   dword         length[2];           /* length in bytes of message   */
   dword         offset;              /* # of unprocessed bytes at    */
                                      /*          call of MDfinish    */

   /* initialize */
   if ((mf = fopen(fname, "rb")) == NULL) {
      fprintf(stderr, "\nRMDbinary: cannot open file \"%s\".\n",
              fname);
      exit(1);
   }
   MDinit(MDbuf);
   length[0] = 0;
   length[1] = 0;

   while ((nbytes = fread(data, 1, 1024, mf)) != 0) {
      /* process all complete blocks */
      for (i=0; i<(nbytes>>6); i++) {
         for (j=0; j<16; j++)
            X[j] = BYTES_TO_DWORD(data+64*i+4*j);
         compress(MDbuf, X);
      }
      /* update length[] */
      if (length[0] + nbytes < length[0])
         length[1]++;                  /* overflow to msb of length */
      length[0] += nbytes;
   }

   /* finish: */
   offset = length[0] & 0x3C0;   /* extract bytes 6 to 10 inclusive */
   MDfinish(MDbuf, data+offset, length[0], length[1]);

   for (i=0; i<RMDsize/8; i+=4) {
      hashcode[i]   =  MDbuf[i>>2];
      hashcode[i+1] = (MDbuf[i>>2] >>  8);
      hashcode[i+2] = (MDbuf[i>>2] >> 16);
      hashcode[i+3] = (MDbuf[i>>2] >> 24);
   }

   fclose(mf);

   return (byte *)hashcode;
}

/********************************************************************/

void speedtest(void)
/*
 * A time trial routine, to measure the speed of ripemd.
 * Measures processor time required to process TEST_BLOCKS times
 *  a message of TEST_BLOCK_SIZE characters.
 */
{
   clock_t    t0, t1;
   byte      *data;
   byte       hashcode[RMDsize/8];
   dword      X[16];
   dword      MDbuf[RMDsize/32];
   word       i, j, k;

   srand(time(NULL));

   /* allocate and initialize test data */
   if ((data = (byte*)malloc(TEST_BLOCK_SIZE)) == NULL) {
      fprintf(stderr, "speedtest: allocation error\n");
      exit(1);
   }
   for (i=0; i<TEST_BLOCK_SIZE; i++)
      data[i] = (byte)(rand() >> 7);

   /* start timer */
   printf("RIPEMD time trial. Processing %ld characters...\n",
          TEST_BYTES);
   t0 = clock();

   /* process data */
   MDinit(MDbuf);
   for (i=0; i<TEST_BLOCKS; i++) {
      for (j=0; j<TEST_BLOCK_SIZE; j+=64) {
         for (k=0; k<16; k++)
            X[k] = BYTES_TO_DWORD(data+j+4*k);
         compress(MDbuf, X);
      }
   }
   MDfinish(MDbuf, data, TEST_BYTES, 0);

   /* stop timer, get time difference */
   t1 = clock();
   printf("\nTest input processed in %g seconds.\n",
          (double)(t1-t0)/(double)CLOCKS_PER_SEC);
   printf("Characters processed per second: %g\n",
          (double)CLOCKS_PER_SEC*TEST_BYTES/((double)t1-t0));

   for (i=0; i<RMDsize/8; i+=4) {
      hashcode[i]   =  MDbuf[i>>2];
      hashcode[i+1] = (MDbuf[i>>2] >>  8);
      hashcode[i+2] = (MDbuf[i>>2] >> 16);
      hashcode[i+3] = (MDbuf[i>>2] >> 24);
   }
   printf("\nhashcode: ");
   for (i=0; i<RMDsize/8; i++)
      printf("%02x", hashcode[i]);

   free(data);
   return;
}

/********************************************************************/

void RMDonemillion(void)
/*
 * returns RMD() of message consisting of 1 million 'a' characters
 */
{
   dword         MDbuf[RMDsize/32];   /* contains (A, B, C, D(, E)) */
   static byte   hashcode[RMDsize/8]; /* for final hash-value       */
   dword         X[16];               /* current 16-word chunk      */
   word          i;                   /* counter                    */

   MDinit(MDbuf);
   memcpy(X, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 32);
   memcpy(X+8, X, 32);
   for (i=15625; i>0; i--)
      compress(MDbuf, X);
   MDfinish(MDbuf, NULL, 1000000UL, 0);
   for (i=0; i<RMDsize/8; i+=4) {
      hashcode[i]   =  MDbuf[i>>2];
      hashcode[i+1] = (MDbuf[i>>2] >>  8);
      hashcode[i+2] = (MDbuf[i>>2] >> 16);
      hashcode[i+3] = (MDbuf[i>>2] >> 24);
   }
   printf("\n* message: 1 million times \"a\"\n  hashcode: ");
   for (i=0; i<RMDsize/8; i++)
      printf("%02x", hashcode[i]);

}

/********************************************************************/

void RMDstring(char *message, char *print)
{
   int i;
   byte *hashcode;

   hashcode = RMD((byte *)message);
   printf("\n* message: %s\n  hashcode: ", print);
   for (i=0; i<RMDsize/8; i++)
      printf("%02x", hashcode[i]);
}

/********************************************************************/

void testsuite (void)
/*
 *   standard test suite
 */
{
   printf("\nRIPEMD-%u test suite results (ASCII):\n", RMDsize);

   RMDstring("", "\"\" (empty string)");
   RMDstring("a", "\"a\"");
   RMDstring("abc", "\"abc\"");
   RMDstring("message digest", "\"message digest\"");
   RMDstring("abcdefghijklmnopqrstuvwxyz", "\"abcdefghijklmnopqrstuvwxyz\"");
   RMDstring("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
             "\"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq\"");
   RMDstring("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789",
             "\"A...Za...z0...9\"");
   RMDstring("1234567890123456789012345678901234567890"
             "1234567890123456789012345678901234567890", 
             "8 times \"1234567890\"");
   RMDonemillion();

   return;
}

/********************************************************************/

main (int argc, char *argv[])
/*
 *  main program. calls one or more of the test routines depending
 *  on command line arguments. see the header of this file.
 *
 */
{
  int   i, j;
  byte *hashcode;

   if (argc == 1) {
      printf("For each command line argument in turn:\n");
      printf("  filename  -- compute hash code of file binary read\n");
      printf("  -sstring  -- print string & hashcode\n");
      printf("  -t        -- perform time trial\n");
      printf("  -x        -- execute standard test suite, ASCII input\n");
   }
   else {
      for (i = 1; i < argc; i++) {
         if (argv[i][0] == '-' && argv[i][1] == 's') {
            printf("\n\nmessage: %s", argv[i]+2);
            hashcode = RMD((byte *)argv[i] + 2);
            printf("\nhashcode: ");
            for (j=0; j<RMDsize/8; j++)
               printf("%02x", hashcode[j]);
         }
         else if (strcmp (argv[i], "-t") == 0)
            speedtest ();
         else if (strcmp (argv[i], "-x") == 0)
            testsuite ();
         else {
            hashcode = RMDbinary (argv[i]);
            printf("\n\nmessagefile (binary): %s", argv[i]);
            printf("\nhashcode: ");
            for (j=0; j<RMDsize/8; j++)
               printf("%02x", hashcode[j]);
         }
      }
   }
   printf("\n");

   return 0;
}

/********************** end of file hashtest.c **********************/

