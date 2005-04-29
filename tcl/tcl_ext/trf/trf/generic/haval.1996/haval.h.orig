
/*
 *  haval.h:  specifies the interface to the HAVAL (V.1) hashing library.
 *
 *      HAVAL is a one-way hashing algorithm with the following
 *      collision-resistant property:
 *             It is computationally infeasible to find two or more
 *             messages that are hashed into the same fingerprint.
 *
 *  Reference:
 *       Y. Zheng, J. Pieprzyk and J. Seberry:
 *       ``HAVAL --- a one-way hashing algorithm with variable
 *       length of output'', Advances in Cryptology --- AUSCRYPT'92,
 *       Lecture Notes in Computer Science,  Vol.718, pp.83-104, 
 *       Springer-Verlag, 1993.
 *
 *      This library provides routines to hash
 *        -  a string,
 *        -  a file,
 *        -  input from the standard input device,
 *        -  a 32-word block, and
 *        -  a string of specified length.
 *
 *  Author:     Yuliang Zheng
 *              School of Computing and Information Technology
 *              Monash University
 *              McMahons Road, Frankston 3199, Australia
 *              Email: yzheng@fcit.monash.edu.au
 *              URL:   http://pscit-www.fcit.monash.edu.au:/~yuliang/
 *              Voice: +61 3 9904 4196
 *              Fax  : +61 3 9904 4124
 *
 *  Date:
 *              First release: June  1993
 *              Revised:       April 1996
 *                Many thanks to Ross Williams (ross@rocksoft.com)
 *                for his invaluable comments on the previous
 *                version of the code.
 *
 *      Copyright (C) 1996 by Yuliang Zheng.  All rights reserved. 
 *      This program may not be sold or used as inducement to sell
 *      a  product without the  written  permission of the author.
 */

typedef unsigned long int haval_word; /* a HAVAL word = 32 bits */

typedef struct {
  haval_word    count[2];                /* number of bits in a message */
  haval_word    fingerprint[8];          /* current state of fingerprint */    
  haval_word    block[32];               /* buffer for a 32-word block */ 
  unsigned char remainder[32*4];         /* unhashed chars (No.<128) */   
} haval_state;

void haval_string (char *, unsigned char *); /* hash a string */
int  haval_file (char *, unsigned char *);   /* hash a file */
void haval_stdin (void);                     /* filter -- hash input from stdin */
void haval_start (haval_state *);            /* initialization */
void haval_hash (haval_state *, unsigned char *,
                 unsigned int);              /* updating routine */
void haval_end (haval_state *, unsigned char *); /* finalization */
void haval_hash_block (haval_state *);       /* hash a 32-word block */


