/*
 * mppe - Mucking with PpP Encription
 *
 * Copyright (c) 1995 Árpád Magossányi
 * All rights reserved.
 *
 * Copyright (c) 1999 Tim Hockin, Cobalt Networks Inc.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by Pedro Roque Marques.  The name of the author may not be used to
 * endorse or promote products derived from this software without
 * specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */


#ifndef __MPPE_INCLUDE__

typedef struct mppe_state {
    int    us_unit;	/* Interface unit number */
    u_char us_id;		/* Current id */
    u_char us_allowed;
    int    us_type;
    char   *us_number;    /* Telefone Number */
} mppe_state;


extern struct protent mppe_protent;

#define MPPE_CONFOPTION 18		/* p[0] */
#define MPPE_STATELESS  0x01		/* p[2] */
#define MPPE_40BIT	0x20		/* p[5] */
#define MPPE_128BIT	0x40		/* p[5] */

#define PPP_MPPE	0x00FD

#define MPPE_BIT_A	0x80
#define MPPE_BIT_B	0x40
#define MPPE_BIT_C	0x20
#define MPPE_BIT_D	0x10
#define MPPE_BIT_FLUSHED MPPE_BIT_A
#define MPPE_BIT_ENCRYPTED MPPE_BIT_D
#define MPPE_CCOUNT	0x0FFF

#define MPPE_40_SALT0	0xD1
#define MPPE_40_SALT1	0x26
#define MPPE_40_SALT2	0x9E

#define MPPE_MINLEN 4

#define MPPE_REQ    1
#define MPPE_RESP   2
#define MPPE_ACK    3

extern char mppe_master_send_key_40[8];
extern char mppe_master_send_key_128[16];
extern char mppe_master_recv_key_40[8];
extern char mppe_master_recv_key_128[16];
extern unsigned int mppe_allowed;

void mppe_gen_master_key __P((char *, int, unsigned char *));
void mppe_gen_master_key_v2 __P((char *, int, unsigned char *, int));

int setmppe_40(char **);
int setnomppe_40(char **);
int setmppe_128(char **);
int setnomppe_128(char **);
int setmppe_stateless(char **);
int setnomppe_stateless(char **);

#define __MPPE_INCLUDE__
#endif /* __MPPE_INCLUDE__ */

