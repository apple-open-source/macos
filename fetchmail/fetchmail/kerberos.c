/*
 * kerberos.c -- Kerberos authentication (see RFC 1731).
 *
 * For license terms, see the file COPYING in this directory.
 */
#include  "config.h"

#ifdef KERBEROS_V4

#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include  "fetchmail.h"
#include  "socket.h"
#include  "kerberos.h"

#include <sys/types.h>
#include <netinet/in.h>  /* for htonl/ntohl */

#include  "i18n.h"

#if SIZEOF_INT == 4
typedef	int	int32;
#elif SIZEOF_SHORT == 4
typedef	short	int32;
#elif SIZEOF_LONG == 4
typedef	long	int32;
#else
#error Cannot deduce a 32-bit-type
#endif

int do_rfc1731(int sock, char *command, char *truename)
/* authenticate as per RFC1731 -- note 32-bit integer requirement here */
{
    int result = 0, len;
    char buf1[4096], buf2[4096];
    union {
      int32 cint;
      char cstr[4];
    } challenge1, challenge2;
    char srvinst[INST_SZ];
    char *p;
    char srvrealm[REALM_SZ];
    KTEXT_ST authenticator;
    CREDENTIALS credentials;
    char tktuser[MAX_K_NAME_SZ+1+INST_SZ+1+REALM_SZ+1];
    char tktinst[INST_SZ];
    char tktrealm[REALM_SZ];
    des_cblock session;
    des_key_schedule schedule;

    gen_send(sock, "%s KERBEROS_V4", command);

    /* The data encoded in the first ready response contains a random
     * 32-bit number in network byte order.  The client should respond
     * with a Kerberos ticket and an authenticator for the principal
     * "imap.hostname@realm", where "hostname" is the first component
     * of the host name of the server with all letters in lower case
     * and where "realm" is the Kerberos realm of the server.  The
     * encrypted checksum field included within the Kerberos
     * authenticator should contain the server provided 32-bit number
     * in network byte order.
     */

    if ((result = gen_recv(sock, buf1, sizeof buf1)) != 0) {
	return result;
    }

    len = from64tobits(challenge1.cstr, buf1, sizeof(challenge1.cstr));
    if (len < 0) {
	report(stderr, GT_("could not decode initial BASE64 challenge\n"));
	return PS_AUTHFAIL;
    }

    /* this patch by Dan Root <dar@thekeep.org> solves an endianess
     * problem. */
    {
	char tmp[4];

	*(int *)tmp = ntohl(*(int *) challenge1.cstr);
	memcpy(challenge1.cstr, tmp, sizeof(tmp));
    }

    /* Client responds with a Kerberos ticket and an authenticator for
     * the principal "imap.hostname@realm" where "hostname" is the
     * first component of the host name of the server with all letters
     * in lower case and where "realm" is the Kerberos realm of the
     * server.  The encrypted checksum field included within the
     * Kerberos authenticator should contain the server-provided
     * 32-bit number in network byte order.
     */

    strncpy(srvinst, truename, (sizeof srvinst)-1);
    srvinst[(sizeof srvinst)-1] = '\0';
    for (p = srvinst; *p; p++) {
      if (isupper((unsigned char)*p)) {
	*p = tolower((unsigned char)*p);
      }
    }

    strncpy(srvrealm, (char *)krb_realmofhost(srvinst), (sizeof srvrealm)-1);
    srvrealm[(sizeof srvrealm)-1] = '\0';
    if ((p = strchr(srvinst, '.')) != NULL) {
      *p = '\0';
    }

    result = krb_mk_req(&authenticator, "imap", srvinst, srvrealm, 0);
    if (result) {
	report(stderr, "krb_mq_req: %s\n", krb_get_err_text(result));
	return PS_AUTHFAIL;
    }

    result = krb_get_cred("imap", srvinst, srvrealm, &credentials);
    if (result) {
	report(stderr, "krb_get_cred: %s\n", krb_get_err_text(result));
	return PS_AUTHFAIL;
    }

    memcpy(session, credentials.session, sizeof session);
    memset(&credentials, 0, sizeof credentials);
    des_key_sched(&session, schedule);

    result = krb_get_tf_fullname(TKT_FILE, tktuser, tktinst, tktrealm);
    if (result) {
	report(stderr, "krb_get_tf_fullname: %s\n", krb_get_err_text(result));
	return PS_AUTHFAIL;
    }

#ifdef __UNUSED__
    /*
     * Andrew H. Chatham <andrew.chatham@duke.edu> alleges that this check
     * is not necessary and has consistently been messing him up.
     */
    if (strcmp(tktuser, user) != 0) {
	report(stderr, 
	       GT_("principal %s in ticket does not match -u %s\n"), tktuser,
		user);
	return PS_AUTHFAIL;
    }
#endif /* __UNUSED__ */

    if (tktinst[0]) {
	report(stderr, 
	       GT_("non-null instance (%s) might cause strange behavior\n"),
		tktinst);
	strlcat(tktuser, ".", sizeof(tktuser));
	strlcat(tktuser, tktinst, sizeof(tktuser));
    }

    if (strcmp(tktrealm, srvrealm) != 0) {
	strlcat(tktuser, "@", sizeof(tktuser));
	strlcat(tktuser, tktrealm, sizeof(tktuser));
    }

    result = krb_mk_req(&authenticator, "imap", srvinst, srvrealm,
	    challenge1.cint);
    if (result) {
	report(stderr, "krb_mq_req: %s\n", krb_get_err_text(result));
	return PS_AUTHFAIL;
    }

    to64frombits(buf1, authenticator.dat, authenticator.length);
    if (outlevel >= O_MONITOR) {
	report(stdout, "IMAP> %s\n", buf1);
    }
    strcat(buf1, "\r\n");
    SockWrite(sock, buf1, strlen(buf1));

    /* Upon decrypting and verifying the ticket and authenticator, the
     * server should verify that the contained checksum field equals
     * the original server provided random 32-bit number.  Should the
     * verification be successful, the server must add one to the
     * checksum and construct 8 octets of data, with the first four
     * octets containing the incremented checksum in network byte
     * order, the fifth octet containing a bit-mask specifying the
     * protection mechanisms supported by the server, and the sixth
     * through eighth octets containing, in network byte order, the
     * maximum cipher-text buffer size the server is able to receive.
     * The server must encrypt the 8 octets of data in the session key
     * and issue that encrypted data in a second ready response.  The
     * client should consider the server authenticated if the first
     * four octets the un-encrypted data is equal to one plus the
     * checksum it previously sent.
     */
    
    if ((result = gen_recv(sock, buf1, sizeof buf1)) != 0)
	return result;

    /* The client must construct data with the first four octets
     * containing the original server-issued checksum in network byte
     * order, the fifth octet containing the bit-mask specifying the
     * selected protection mechanism, the sixth through eighth octets
     * containing in network byte order the maximum cipher-text buffer
     * size the client is able to receive, and the following octets
     * containing a user name string.  The client must then append
     * from one to eight octets so that the length of the data is a
     * multiple of eight octets. The client must then PCBC encrypt the
     * data with the session key and respond to the second ready
     * response with the encrypted data.  The server decrypts the data
     * and verifies the contained checksum.  The username field
     * identifies the user for whom subsequent IMAP operations are to
     * be performed; the server must verify that the principal
     * identified in the Kerberos ticket is authorized to connect as
     * that user.  After these verifications, the authentication
     * process is complete.
     */

    len = from64tobits(buf2, buf1, sizeof(buf2));
    if (len < 0) {
	report(stderr, GT_("could not decode BASE64 ready response\n"));
	return PS_AUTHFAIL;
    }

    des_ecb_encrypt((des_cblock *)buf2, (des_cblock *)buf2, schedule, 0);
    memcpy(challenge2.cstr, buf2, 4);
    if ((int32)ntohl(challenge2.cint) != challenge1.cint + 1) {
	report(stderr, GT_("challenge mismatch\n"));
	return PS_AUTHFAIL;
    }	    

    memset(authenticator.dat, 0, sizeof authenticator.dat);

    result = htonl(challenge1.cint);
    memcpy(authenticator.dat, &result, sizeof result);

    /* The protection mechanisms and their corresponding bit-masks are as
     * follows:
     *
     * 1 No protection mechanism
     * 2 Integrity (krb_mk_safe) protection
     * 4 Privacy (krb_mk_priv) protection
     */
    authenticator.dat[4] = 1;

    len = strlen(tktuser);
    strncpy((char *)authenticator.dat+8, tktuser, len);
    authenticator.length = len + 8 + 1;
    while (authenticator.length & 7) {
	authenticator.length++;
    }
    des_pcbc_encrypt((const unsigned char *)authenticator.dat,
	    (unsigned char *)authenticator.dat, authenticator.length, schedule,
	    &session, 1);

    to64frombits(buf1, authenticator.dat, authenticator.length);

    /* ship down the response, accept the server's error/ok indication */
    suppress_tags = TRUE;
    result = gen_transact(sock, buf1, strlen(buf1));
    suppress_tags = FALSE;
    if (result)
	return(result);
    else
	return(PS_SUCCESS);
}
#endif /* KERBEROS_V4 */

/* kerberos.c ends here */

