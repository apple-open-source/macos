/*
 * smtp.h -- prototypes for smtp handling code
 *
 * For license terms, see the file COPYING in this directory.
 */

#ifndef _POPSMTP_
#define _POPSMTP_

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

void SMTP_setmode(char);
int SMTP_helo(int socket,const char *host);
int SMTP_ehlo(int socket,const char *host,int *opt);
int SMTP_from(int socket,const char *from,const char *opts);
int SMTP_rcpt(int socket,const char *to);
int SMTP_data(int socket);
int SMTP_eom(int socket);
int SMTP_rset(int socket);
int SMTP_quit(int socket);
int SMTP_ok(int socket);

extern char smtp_response[MSGBUFSIZE];

#endif
