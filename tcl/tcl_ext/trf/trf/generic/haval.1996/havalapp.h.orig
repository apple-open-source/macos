/*
 *  havalapp.h:  specifies the following three constants needed to
 *               compile the HAVAL hashing library:
 *                     LITTLE_ENDIAN, PASS and FPTLEN
 *
 *  Descriptions:
 *
 *   LITTLE_ENDIAN  define this only if your machine is little-endian
 *                  (such as 80X86 family). 
 *
 *         Note:
 *            1. In general, HAVAL is faster on a little endian
 *               machine than on a big endian one.
 *
 *            2. The test program "havaltest.c" provides an option
 *               for testing the endianity of your machine.
 *
 *            3. The speed of HAVAL is even more remarkable on a
 *               machine that has a large number of internal registers.
 *
 *   PASS     define the number of passes        (3, 4, or 5)
 *   FPTLEN   define the length of a fingerprint (128, 160, 192, 224 or 256)
 */

#undef LITTLE_ENDIAN

#ifndef PASS
#define PASS       3        /* 3, 4, or 5 */
#endif

#ifndef FPTLEN  
#define FPTLEN     256      /* 128, 160, 192, 224 or 256 */
#endif


