#ifndef res_9_private_h
#define res_9_private_h

typedef struct {
	unsigned	id :16;		/* query identification number */
#if BYTE_ORDER == BIG_ENDIAN
			/* fields in third byte */
	unsigned	qr: 1;		/* response flag */
	unsigned	opcode: 4;	/* purpose of message */
	unsigned	aa: 1;		/* authoritive answer */
	unsigned	tc: 1;		/* truncated message */
	unsigned	rd: 1;		/* recursion desired */
			/* fields in fourth byte */
	unsigned	ra: 1;		/* recursion available */
	unsigned	unused :3;	/* unused bits (MBZ as of 4.9.3a3) */
	unsigned	rcode :4;	/* response code */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN || BYTE_ORDER == PDP_ENDIAN
			/* fields in third byte */
	unsigned	rd :1;		/* recursion desired */
	unsigned	tc :1;		/* truncated message */
	unsigned	aa :1;		/* authoritive answer */
	unsigned	opcode :4;	/* purpose of message */
	unsigned	qr :1;		/* response flag */
			/* fields in fourth byte */
	unsigned	rcode :4;	/* response code */
	unsigned	unused :3;	/* unused bits (MBZ as of 4.9.3a3) */
	unsigned	ra :1;		/* recursion available */
#endif
			/* remaining bytes */
	unsigned	qdcount :16;	/* number of question entries */
	unsigned	ancount :16;	/* number of answer entries */
	unsigned	nscount :16;	/* number of authority entries */
	unsigned	arcount :16;	/* number of resource entries */
} HEADER;

#ifndef __res_state_ext
#define __res_state_ext __res_9_res_state_ext
#endif

struct __res_state_ext {
	union res_sockaddr_union nsaddrs[MAXNS];
	struct sort_list {
		int     af;
		union {
			struct in_addr  ina;
			struct in6_addr in6a;
		} addr, mask;
	} sort_list[MAXRESOLVSORT];
	char nsuffix[64];
	char bsuffix[64];
	char nsuffix2[64];
	unsigned int search_order;
};

#define get_nsaddr res_9_get_nsaddr
struct sockaddr *get_nsaddr __P((res_state, size_t));

#define res_nsend_2 res_9_nsend_2
int res_nsend_2(res_state, const u_char *, int, u_char *, int, struct sockaddr *, int *);

#define res_ourserver_p res_9_ourserver_p
int res_ourserver_p(const res_state, const struct sockaddr *);

#endif
