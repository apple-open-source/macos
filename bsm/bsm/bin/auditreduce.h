#ifndef _AUDITREDUCE_H_
#define _AUDITREDUCE_H_


#define OPT_a	0x00000001
#define OPT_b	0x00000002
#define OPT_c	0x00000004
#define OPT_d 	(OPT_a | OPT_b)	
#define OPT_e	0x00000010
#define OPT_f	0x00000020
#define OPT_g	0x00000040
#define OPT_j	0x00000080
#define OPT_m	0x00000100
#define OPT_of	0x00000200
#define OPT_om	0x00000400
#define OPT_op	0x00000800
#define OPT_ose	0x00001000
#define OPT_osh	0x00002000
#define OPT_oso	0x00004000
#define OPT_r	0x00008000
#define OPT_u	0x00010000
#define OPT_A	0x00020000

#define FILEOBJ "file"
#define MSGQIDOBJ "msgqid"
#define PIDOBJ "pid"
#define SEMIDOBJ "semid"
#define SHMIDOBJ "shmid"
#define SOCKOBJ "sock"


#define SETOPT(optmask, bit)	(optmask |= bit)
#define ISOPTSET(optmask, bit)	(optmask & bit)


#endif /* !_AUDITREDUCE_H_ */
