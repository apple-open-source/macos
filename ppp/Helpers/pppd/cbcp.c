/*
 * cbcp - Call Back Configuration Protocol.
 *
 * Copyright (c) 1995 Pedro Roque Marques
 * All rights reserved.
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

#define RCSID	"$Id: cbcp.c,v 1.2 2002/03/13 22:44:25 callie Exp $"

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>

#include "pppd.h"
#include "cbcp.h"
#include "fsm.h"
#include "lcp.h"

static const char rcsid[] = RCSID;

void network_phase __P((int));

/*
 * Options.
 */
static int setcbcp __P((char **));
static int setcbcpreq __P((char **));
static int setnocbcp __P((char **));

static option_t cbcp_option_list[] = {
    { "callback", o_special, setcbcp,
      "Ask for callback" },
    { "+callback", o_special_noarg, setcbcpreq,
      "Ask for callback" },
    { "nocallback", o_special, setnocbcp,
      "Don't allow callbacks" },
    { "-callback", o_special, setnocbcp,
      "Don't allow callbacks" },
    { NULL }
};

/*
 * Protocol entry points.
 */
static void cbcp_init      __P((int unit));
static void cbcp_open      __P((int unit));
static void cbcp_close     __P((int unit, char *));	
static void cbcp_lowerup   __P((int unit));
static void cbcp_lowerdown __P((int unit));
static void cbcp_input     __P((int unit, u_char *pkt, int len));
static void cbcp_protrej   __P((int unit));
static int  cbcp_printpkt  __P((u_char *pkt, int len,
				void (*printer) __P((void *, char *, ...)),
				void *arg));

struct protent cbcp_protent = {
    PPP_CBCP,
    cbcp_init,
    cbcp_input,
    cbcp_protrej,
    cbcp_lowerup,
    cbcp_lowerdown,
    cbcp_open,
    cbcp_close,
    cbcp_printpkt,
    NULL,
    1,
    "CBCP",
    NULL,
    cbcp_option_list,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

cbcp_state cbcp[NUM_PPP];	

/* internal prototypes */

static void cbcp_recvreq __P((cbcp_state *us, char *pckt, int len));
static void cbcp_recvack __P((cbcp_state *us, char *pckt, int len));
static void cbcp_recvresp __P((cbcp_state *us, char *pckt, int len));
static void cbcp_resp __P((cbcp_state *us));
static void cbcp_req __P((cbcp_state *us));
static void cbcp_ack __P((cbcp_state *us));
static void cbcp_send __P((cbcp_state *us, u_char code, u_char *buf, int len));
static void cbcp_up __P((cbcp_state *us));

/* option processing */
static int
setcbcp(argv)
    char **argv;
{
    lcp_allowoptions[0].neg_cbcp = 1;
    cbcp[0].us_number = strdup(*argv);
    if (cbcp[0].us_number == 0)
	novm("callback number");
    if (cbcp[0].us_number[0] == '-')
	cbcp[0].us_type = (1 << CB_CONF_NO);
    else
    {
	cbcp[0].us_type = (1 << CB_CONF_USER);
	cbcp[0].us_type |= (1 << CB_CONF_ADMIN);
    }
    return (1);
}

static int
setnocbcp(argv)
    char **argv;
{
    lcp_allowoptions[0].neg_cbcp = lcp_wantoptions[0].neg_cbcp = 0;
    return (1);
}

static int
setcbcpreq(argv)
    char **argv;
{
    lcp_wantoptions[0].neg_cbcp = 1;
    return (1);
}

/* init state */
static void
cbcp_init(iface)
    int iface;
{
    cbcp_state *us;

    us = &cbcp[iface];
    memset(us, 0, sizeof(cbcp_state));
    us->us_unit = iface;
    us->us_type = (1 << CB_CONF_NO);
    us->us_id = 1;
}

/* lower layer is up */
static void
cbcp_lowerup(iface)
    int iface;
{
    cbcp_state *us = &cbcp[iface];

    if (debug)
    {
        dbglog("cbcp_lowerup");
        dbglog("want: %d", us->us_type);

        if (us->us_type & (1 << CB_CONF_USER))
            dbglog("phone no: %s", us->us_number);
    }
}

static void
cbcp_lowerdown(iface)
    int iface;
{
    if(debug)
        dbglog("cbcp_lowerdown");
}

static void
cbcp_open(unit)
    int unit;
{
    lcp_options *ho = &lcp_hisoptions[unit];
    lcp_options *ao = &lcp_allowoptions[unit];
    lcp_options *wo = &lcp_wantoptions[unit];
    lcp_options *go = &lcp_gotoptions[unit];
    cbcp_state *us = &cbcp[unit];

    if(debug)
        dbglog("cbcp_open");
    if(ao->neg_cbcp)
    {
        if(ho->neg_cbcp)
        {
           cbcp_req(us);
           return;
        }
    }
    else if(wo->neg_cbcp)
    {
       if(!go->neg_cbcp)
           lcp_close(0, "Callback required");
       return;
    }
    cbcp_up(us);
}

static void
cbcp_close(unit, reason)
    int unit;
    char *reason;
{
    if(debug)
        dbglog("cbcp_close: %s", reason);
}

/* process an incomming packet */
static void
cbcp_input(unit, inpacket, pktlen)
    int unit;
    u_char *inpacket;
    int pktlen;
{
    u_char *inp;
    u_char code, id;
    u_short len;

    cbcp_state *us = &cbcp[unit];

    inp = inpacket;

    if (pktlen < CBCP_MINLEN) {
        error("CBCP packet is too small");
	return;
    }

    GETCHAR(code, inp);
    GETCHAR(id, inp);
    GETSHORT(len, inp);

#if 0
    if (len > pktlen) {
        error("CBCP packet: invalid length");
        return;
    }
#endif

    len -= CBCP_MINLEN;
 
    switch(code) {
    case CBCP_REQ:
        us->us_id = id;
	cbcp_recvreq(us, inp, len);
	break;

    case CBCP_RESP:
        if (id != us->us_id)
            dbglog("id doesn't match: expected %d recv %d", us->us_id, id);
        cbcp_recvresp(us, inp, len);
	break;

    case CBCP_ACK:
	if (id != us->us_id)
	    dbglog("id doesn't match: expected %d recv %d",
		   us->us_id, id);

	cbcp_recvack(us, inp, len);
	break;

    default:
	break;
    }
}

/* protocol was rejected by foe */
void cbcp_protrej(int iface)
{
}

char *cbcp_codenames[] = {
    "Request", "Response", "Ack"
};

char *cbcp_optionnames[] = {
    "NoCallback",
    "UserDefined",
    "AdminDefined",
    "List"
};

/* pretty print a packet */
static int
cbcp_printpkt(p, plen, printer, arg)
    u_char *p;
    int plen;
    void (*printer) __P((void *, char *, ...));
    void *arg;
{
    int code, opt, id, len, olen, delay;
    u_char *pstart;

    if (plen < HEADERLEN)
	return 0;
    pstart = p;
    GETCHAR(code, p);
    GETCHAR(id, p);
    GETSHORT(len, p);
    if (len < HEADERLEN || len > plen)
	return 0;

    if (code >= 1 && code <= sizeof(cbcp_codenames) / sizeof(char *))
	printer(arg, " %s", cbcp_codenames[code-1]);
    else
	printer(arg, " code=0x%x", code); 

    printer(arg, " id=0x%x", id);
    len -= HEADERLEN;

    switch (code) {
    case CBCP_REQ:
    case CBCP_RESP:
    case CBCP_ACK:
        while(len >= 2) {
	    GETCHAR(opt, p);
	    GETCHAR(olen, p);

	    if (olen < 2 || olen > len) {
	        break;
	    }

	    printer(arg, " <");
	    len -= olen;

	    if (opt >= 1 && opt <= sizeof(cbcp_optionnames) / sizeof(char *))
	    	printer(arg, " %s", cbcp_optionnames[opt-1]);
	    else
	        printer(arg, " option=0x%x", opt); 

	    if (olen > 2) {
	        GETCHAR(delay, p);
		printer(arg, " delay = %d", delay);
	    }

	    if (olen > 3) {
	        int addrt;
		char str[256];

		GETCHAR(addrt, p);
		memcpy(str, p, olen - 4);
		str[olen - 4] = 0;
		printer(arg, " number = %s", str);
	    }
	    printer(arg, ">");
	    break;
	}

    default:
	break;
    }

    for (; len > 0; --len) {
	GETCHAR(code, p);
	printer(arg, " %.2x", code);
    }

    return p - pstart;
}

/* received CBCP request */
static void
cbcp_recvreq(us, pckt, pcktlen)
    cbcp_state *us;
    char *pckt;
    int pcktlen;
{
    u_char type, opt_len, delay, addr_type;
    char address[256];
    int len = pcktlen;

    address[0] = 0;

    while (len) {
        dbglog("length: %d", len);

	GETCHAR(type, pckt);
	GETCHAR(opt_len, pckt);

	if (opt_len > 2)
	    GETCHAR(delay, pckt);

	us->us_allowed |= (1 << type);

	switch(type) {
	case CB_CONF_NO:
	    dbglog("Callback: none");
	    break;

	case CB_CONF_USER:
	    if (opt_len > 4) {
	        GETCHAR(addr_type, pckt);
		memcpy(address, pckt, opt_len - 4);
		address[opt_len - 4] = 0;
	        dbglog("Callback: user callback, address: '%s'", address);
	    }
            else
                dbglog("Callback: user callback");
	    break;

	case CB_CONF_ADMIN:
	    dbglog("Callback: user admin defined");
	    break;

	case CB_CONF_LIST:
	    break;
	}
	len -= opt_len;
    }

    cbcp_resp(us);
}

static void
cbcp_resp(us)
    cbcp_state *us;
{
    u_char cb_type;
    u_char buf[256];
    u_char *bufp = buf;
    int len = 0;

    cb_type = us->us_allowed & us->us_type;
    dbglog("cbcp_resp cb_type=%d", cb_type);

    if (!cb_type) {
        dbglog("Your remote side wanted a callback-type you don't allow -> doing no callback");
        cb_type = 1 << CB_CONF_NO;
#if 0
        lcp_down(us->us_unit);
#endif
    }
    
    if (cb_type & ( 1 << CB_CONF_USER ) ) {
	dbglog("cbcp_resp CONF_USER");
	PUTCHAR(CB_CONF_USER, bufp);
	len = 2 + 1 + strlen(us->us_number);
	PUTCHAR(len , bufp);
	PUTCHAR(5, bufp); /* delay */
	PUTCHAR(1, bufp);
	BCOPY(us->us_number, bufp, strlen(us->us_number));
	cbcp_send(us, CBCP_RESP, buf, len);
	return;
    }

    if (cb_type & ( 1 << CB_CONF_ADMIN ) ) {
	dbglog("cbcp_resp CONF_ADMIN");
        PUTCHAR(CB_CONF_ADMIN, bufp);
	len = 2 + 1;
	PUTCHAR(len, bufp);
	PUTCHAR(5, bufp); /* delay */
	cbcp_send(us, CBCP_RESP, buf, len);
	return;
    }

    if (cb_type & ( 1 << CB_CONF_NO ) ) {
        dbglog("cbcp_resp CONF_NO");
	PUTCHAR(CB_CONF_NO, bufp);
	len = 2;
	PUTCHAR(len , bufp);
	cbcp_send(us, CBCP_RESP, buf, len);
	start_networks();
	return;
    }
}

static void
cbcp_ack(us)
    cbcp_state *us;
{
    u_char cb_type;
    u_char buf[256];
    u_char *bufp = buf;
    int len = 0;

    cb_type = us->us_allowed & us->us_type;
    dbglog("cbcp_ack cb_type=%d", cb_type);

    if (!cb_type) {
        dbglog("Your remote side wanted a callback-type you don't allow -> doing no callback");
        cb_type = 1 << CB_CONF_NO;
        lcp_close(us->us_unit, "Invalid callback requested");
        return;
    }

    if (cb_type & ( 1 << CB_CONF_USER ) ) {
        dbglog("cbcp_ack CONF_USER");
	PUTCHAR(CB_CONF_USER, bufp);
	len = 2 + 1 + strlen(us->us_number);
	PUTCHAR(len , bufp);
 	PUTCHAR(5, bufp); /* delay */
 	PUTCHAR(1, bufp);
 	BCOPY(us->us_number, bufp, strlen(us->us_number));
 	cbcp_send(us, CBCP_ACK, buf, len);
 	cbcp_up(us);
 	return;
     }
 
     if (cb_type & ( 1 << CB_CONF_ADMIN ) ) {
        dbglog("cbcp_ack CONF_ADMIN");
        PUTCHAR(CB_CONF_ADMIN, bufp);
 	len = 2 + 1;
 	PUTCHAR(len , bufp);
 	PUTCHAR(5, bufp); /* delay */
 	PUTCHAR(0, bufp);
 	cbcp_send(us, CBCP_ACK, buf, len);
 	cbcp_up(us);
 	return;
     }
 
     if (cb_type & ( 1 << CB_CONF_NO ) ) {
        dbglog("cbcp_ack CONF_NO");
 	PUTCHAR(CB_CONF_NO, bufp);
 	len = 2;
 	PUTCHAR(len , bufp);
 	cbcp_send(us, CBCP_ACK, buf, len);
 	cbcp_up(us);
 	return;
     }
}
 
static void
cbcp_req(us)
    cbcp_state *us;
{
    u_char cb_type;
    u_char buf[256];
    u_char *bufp = buf;
    int len = 0;
    
    cb_type = us->us_type;

    if (cb_type & ( 1 << CB_CONF_USER ) ) {
        dbglog("cbcp_req CONF_USER");
	PUTCHAR(CB_CONF_USER, bufp);
 	len = 2 + 1 + strlen(us->us_number);
 	PUTCHAR(len , bufp);
 	PUTCHAR(5, bufp); /* delay */
 	PUTCHAR(1, bufp);
 	BCOPY(us->us_number, bufp, strlen(us->us_number));
 	cbcp_send(us, CBCP_REQ, buf, len);
 	return;
    }
 
    if (cb_type & ( 1 << CB_CONF_ADMIN ) ) {
        dbglog("cbcp_req CONF_ADMIN");
        PUTCHAR(CB_CONF_ADMIN, bufp);
 	len = 2 + 1;
 	PUTCHAR(len , bufp);
 	PUTCHAR(5, bufp); /* delay */
 	PUTCHAR(0, bufp);
 	cbcp_send(us, CBCP_REQ, buf, len);
 	return;
     }
 
     if (cb_type & ( 1 << CB_CONF_NO ) ) {
        dbglog("cbcp_req CONF_NO");
 	PUTCHAR(CB_CONF_NO, bufp);
 	len = 2;
 	PUTCHAR(len , bufp);
 	cbcp_send(us, CBCP_REQ, buf, len);
 	return;
    }
}
 
/* received CBCP request */
static void
cbcp_recvresp(us, pckt, pcktlen)
    cbcp_state *us;
    char *pckt;
    int pcktlen;
{
    u_char type, opt_len, delay, addr_type;
    char address[256];
    int len = pcktlen;

    address[0] = 0;

    dbglog("CBCP_RESP received");
 
    while (len) {
        dbglog("length: %d", len);

	GETCHAR(type, pckt);
 	GETCHAR(opt_len, pckt);
 
 	if (opt_len > 2)
 	    GETCHAR(delay, pckt);
 
 	us->us_allowed |= (1 << type);
 
 	switch(type) {
 	case CB_CONF_NO:
 	    dbglog("Callback: none");
 	    break;
 
 	case CB_CONF_USER:
 	    if (opt_len > 4) {
 	        GETCHAR(addr_type, pckt);
 		memcpy(address, pckt, opt_len - 4);
 		address[opt_len - 4] = 0;
 	        dbglog("Callback: user callback, address: '%s'", address);
 	    }
 	    else
 	        dbglog("Callback: user callback");
 	    break;
 
 	case CB_CONF_ADMIN:
	    dbglog("Callback: user admin defined");
	    break;
 
 	case CB_CONF_LIST:
 	    break;
 	}
 	len -= opt_len;
    }
 
    cbcp_ack(us);
}

static void
cbcp_send(us, code, buf, len)
    cbcp_state *us;
    u_char code;
    u_char *buf;
    int len;
{
    u_char *outp;
    int outlen;

    outp = outpacket_buf;

    outlen = 4 + len;
    
    MAKEHEADER(outp, PPP_CBCP);

    PUTCHAR(code, outp);
    PUTCHAR(us->us_id, outp);
    PUTSHORT(outlen, outp);
    
    if (len)
        BCOPY(buf, outp, len);

    output(us->us_unit, outpacket_buf, outlen + PPP_HDRLEN);
}

static void
cbcp_recvack(us, pckt, len)
    cbcp_state *us;
    char *pckt;
    int len;
{
    u_char type, delay, addr_type;
    int opt_len;
    char address[256];

    if (len) {
        GETCHAR(type, pckt);
	GETCHAR(opt_len, pckt);
     
	if (opt_len > 2)
	    GETCHAR(delay, pckt);

	if (opt_len > 4) {
	    GETCHAR(addr_type, pckt);
	    memcpy(address, pckt, opt_len - 4);
	    address[opt_len - 4] = 0;
	    if (address[0])
	        dbglog("Callback: peer will call: %s", address);
	}
	if (type != CB_CONF_NO)
	{
	    persist = 0;
	    lcp_close(0, "Call me back, please");
 	    return;
	}
    }

    cbcp_up(us);
}

/* ok peer will do callback */
static void
cbcp_up(us)
    cbcp_state *us;
{
    network_phase(us->us_unit);
    status = EXIT_CALLBACK;
}
