#ifndef HAVAL_H
#define HAVAL_H

#include <tcl.h> /* to surely have _ANSI_ARGS_ */

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
 *       Lecture Notes in Computer Science, Springer-Verlag, 1993.
 *
 *      This library provides routines to hash
 *        -  a string,
 *        -  a file,
 *        -  input from the standard input device,
 *        -  a 32-word block, and
 *        -  a string of specified length.
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

#ifdef __alpha /* aku, Sep 23, 1996 */
typedef unsigned int      haval_word; /* a HAVAL word = 32 bits */
#else
typedef unsigned long int haval_word; /* a HAVAL word = 32 bits */
#endif

typedef struct {
  haval_word    count[2];                /* number of bits in a message */
  haval_word    fingerprint[8];          /* current state of fingerprint */    
  haval_word    block[32];               /* buffer for a 32-word block */ 
  unsigned char remainder[32*4];         /* unhashed chars (No.<128) */   
} haval_state;

void haval_string     _ANSI_ARGS_((char *, unsigned char *));  /* hash a string */
int  haval_file       _ANSI_ARGS_((char *, unsigned char *));  /* hash a file */
void haval_stdin      _ANSI_ARGS_((void));                     /* filter -- hash input from stdin */
void haval_start      _ANSI_ARGS_((haval_state *));            /* initialization */
void haval_hash       _ANSI_ARGS_((haval_state *, unsigned char *,
				   unsigned int));              /* updating routine */
void haval_end        _ANSI_ARGS_((haval_state *, unsigned char *)); /* finalization */
void haval_hash_block _ANSI_ARGS_((haval_state *));            /* hash a 32-word block */

#endif
