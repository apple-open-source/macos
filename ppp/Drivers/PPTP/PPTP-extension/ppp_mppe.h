#ifndef MPPE_H
#define MPPE_H

typedef struct mppe_state {
    int    us_unit;	/* Interface unit number */
    u_char us_id;		/* Current id */
    u_char us_allowed;
    int    us_type;
    char   *us_number;    /* Telefone Number */
} mppe_state;


extern struct protent mppe_protent;

#define MPPE_CONFOPTION 18
#define MPPC		0x01
#define MPPE_40BIT	0x20
#define MPPE_128BIT	0x40

#define PPP_MPPE	0x00FD
#define PPP_MPPC	PPP_MPPE

#define MPPE_BIT_A	0x80
#define MPPE_BIT_B	0x40
#define MPPE_BIT_C	0x20
#define MPPE_BIT_D	0x10
#define MPPE_BIT_FLUSHED MPPE_BIT_A
#define MPPE_BIT_ENCRYPTED MPPE_BIT_D
#define MPPE_CCOUNT	0x0FFF

#define MPPC_BIT_RESET	MPPE_BIT_A
#define MPPC_BIT_FLUSH	MPPE_BIT_B
#define MPPC_BIT_COMP	MPPC_BIT_C

#define MPPE_40_SALT0	0xD1
#define MPPE_40_SALT1	0x26
#define MPPE_40_SALT2	0x9E

#define MPPE_MINLEN 4

#define MPPE_REQ    1
#define MPPE_RESP   2
#define MPPE_ACK    3

#define CI_MPPE		18
#define CILEN_MPPE	6


int ppp_mppe_init(void);
int ppp_mppe_dispose(void);

#endif
