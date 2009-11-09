/*
 * smtp.h -- prototypes for smtp handling code
 *
 * For license terms, see the file COPYING in this directory.
 */

#ifndef _POPSMTP_
#define _POPSMTP_

#include "config.h"

#define         SMTPBUFSIZE     256

/* SMTP error values */
#define         SM_OK              0
#define         SM_ERROR           128
#define         SM_UNRECOVERABLE   129

/* ESMTP extension option masks (not all options are listed here) */
#define ESMTP_8BITMIME	0x01
#define ESMTP_SIZE	0x02
#define ESMTP_ETRN	0x04
#define ESMTP_ATRN	0x08		/* used with ODMR, RFC 2645 */
#define ESMTP_AUTH	0x10

/* SMTP timeouts (seconds) - SHOULD clauses from RFC-5321 sec. 4.5.3.2. */
#define TIMEOUT_STARTSMTP	300
#define TIMEOUT_HELO		300 /* not in RFC-5321, also for EHLO/LHLO */
#define TIMEOUT_MAIL		300
#define TIMEOUT_RCPT		300
#define TIMEOUT_DATA		120
#define TIMEOUT_DATALINE	180
#define TIMEOUT_EOM		600
#define TIMEOUT_DEFAULT		300 /* not in RFC-5321, all other client timeouts */

extern time_t last_smtp_ok;

int SMTP_helo(int socket, char smtp_mode, const char *host);
int SMTP_ehlo(int socket, char smtp_mode, const char *host, char *name, char *passwd, int *opt);
int SMTP_from(int socket, char smtp_mode, const char *from,const char *opts);
int SMTP_rcpt(int socket, char smtp_mode, const char *to);
int SMTP_data(int socket, char smtp_mode);
int SMTP_eom(int socket, char smtp_mode);
int SMTP_rset(int socket, char smtp_mode);
int SMTP_quit(int socket, char smtp_mode);
int SMTP_ok(int socket, char smtp_mode, int mintimeout);

extern char smtp_response[MSGBUFSIZE];

#endif
