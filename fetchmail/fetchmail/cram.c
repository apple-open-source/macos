/*
 * cram.c -- CRAM-MD5 authentication (see RFC 2195)
 *
 * For license terms, see the file COPYING in this directory.
 */

#include  "config.h"
#include  <stdio.h>
#include  <string.h>
#include  <ctype.h>
#if defined(STDC_HEADERS)
#include  <stdlib.h>
#endif
#include  "fetchmail.h"
#include  "socket.h"

#include  "i18n.h"
#include "fm_md5.h"

void hmac_md5 (const unsigned char *password,  size_t pass_len,
               const unsigned char *challenge, size_t chal_len,
               unsigned char *response,  size_t resp_len)
{
    int i;
    unsigned char ipad[64];
    unsigned char opad[64];
    unsigned char hash_passwd[16];

    MD5_CTX ctx;
    
    if (resp_len != 16)
        return;

    if (pass_len > sizeof (ipad))
    {
        MD5Init (&ctx);
        MD5Update (&ctx, password, pass_len);
        MD5Final (hash_passwd, &ctx);
        password = hash_passwd; pass_len = sizeof (hash_passwd);
    }

    memset (ipad, 0, sizeof (ipad));
    memset (opad, 0, sizeof (opad));
    memcpy (ipad, password, pass_len);
    memcpy (opad, password, pass_len);

    for (i=0; i<64; i++) {
        ipad[i] ^= 0x36;
        opad[i] ^= 0x5c;
    }

    MD5Init (&ctx);
    MD5Update (&ctx, ipad, sizeof (ipad));
    MD5Update (&ctx, challenge, chal_len);
    MD5Final (response, &ctx);

    MD5Init (&ctx);
    MD5Update (&ctx, opad, sizeof (opad));
    MD5Update (&ctx, response, resp_len);
    MD5Final (response, &ctx);
}

int do_cram_md5 (int sock, const char *command, struct query *ctl, const char *strip)
/* authenticate as per RFC2195 */
{
    int result;
    int len;
    char buf1[1024];
    char msg_id[768];
    unsigned char response[16];
    char reply[1024];
    char *respdata;

    gen_send (sock, "%s CRAM-MD5", command);

    /* From RFC2195:
     * The data encoded in the first ready response contains an
     * presumptively arbitrary string of random digits, a timestamp, and the
     * fully-qualified primary host name of the server.  The syntax of the
     * unencoded form must correspond to that of an RFC 822 'msg-id'
     * [RFC822] as described in [POP3].
     */

    if ((result = gen_recv (sock, buf1, sizeof (buf1)))) {
	return result;
    }

    /* caller may specify a response prefix we should strip if present */
    respdata = buf1;
    if (strip && strncmp(buf1, strip, strlen(strip)) == 0)
	respdata += strlen(strip);
    len = from64tobits (msg_id, respdata, sizeof(msg_id));

    if (len < 0) {
	report (stderr, GT_("could not decode BASE64 challenge\n"));
	return PS_AUTHFAIL;
    } else if ((size_t)len < sizeof (msg_id)) {
        msg_id[len] = 0;
    } else {
        msg_id[sizeof (msg_id)-1] = 0;
    }
    if (outlevel >= O_DEBUG) {
        report (stdout, GT_("decoded as %s\n"), msg_id);
    }

    /* The client makes note of the data and then responds with a string
     * consisting of the user name, a space, and a 'digest'.  The latter is
     * computed by applying the keyed MD5 algorithm from [KEYED-MD5] where
     * the key is a shared secret and the digested text is the timestamp
     * (including angle-brackets).
     */

    hmac_md5((unsigned char *)ctl->password, strlen(ctl->password),
              (unsigned char *)msg_id, strlen (msg_id),
              response, sizeof (response));

    snprintf (reply, sizeof(reply),
              "%s %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x", 
              ctl->remotename,
              response[0], response[1], response[2], response[3],
              response[4], response[5], response[6], response[7],
              response[8], response[9], response[10], response[11],
              response[12], response[13], response[14], response[15]);

    to64frombits (buf1, reply, strlen(reply));

    /* ship the authentication back, accept the server's responses */
    /* PMDF5.2 IMAP has a bug that requires this to be a single write */
    suppress_tags = TRUE;
    result = gen_transact(sock, "%s", buf1);
    suppress_tags = FALSE;
    if (result)
	return(result);
    else
	return(PS_SUCCESS);
}

/* cram.c ends here */
