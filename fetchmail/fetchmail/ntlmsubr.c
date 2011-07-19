#include "config.h"

#ifdef NTLM_ENABLE
#include "fetchmail.h"
#include "i18n.h"
#include "ntlm.h"
#include "socket.h"

#include <string.h>

int ntlm_helper(int sock, struct query *ctl, const char *proto)
{
/*
 * NTLM support by Grant Edwards.
 *
 * Handle MS-Exchange NTLM authentication method.  This is the same
 * as the NTLM auth used by Samba for SMB related services. We just
 * encode the packets in base64 instead of sending them out via a
 * network interface.
 *
 * Much source (ntlm.h, smb*.c smb*.h) was borrowed from Samba.
 */
    tSmbNtlmAuthRequest request;
    tSmbNtlmAuthChallenge challenge;
    tSmbNtlmAuthResponse response;

    char msgbuf[2048];
    int result;

    if ((result = gen_recv(sock, msgbuf, sizeof msgbuf)))
	return result;

    if (msgbuf[0] != '+' && strspn(msgbuf+1, " \t") < strlen(msgbuf+1)) {
	if (outlevel >= O_VERBOSE) {
	    report(stdout, GT_("Warning: received malformed challenge to \"AUTH(ENTICATE) NTLM\"!\n"));
	}
	result = PS_AUTHFAIL;
	goto cancelfail;
    }

    buildSmbNtlmAuthRequest(&request,ctl->remotename,NULL);

    if (outlevel >= O_DEBUG)
	dumpSmbNtlmAuthRequest(stdout, &request);

    memset(msgbuf,0,sizeof msgbuf);
    to64frombits (msgbuf, &request, SmbLength(&request));

    if (outlevel >= O_MONITOR)
	report(stdout, "%s> %s\n", proto, msgbuf);

    strcat(msgbuf,"\r\n");
    SockWrite (sock, msgbuf, strlen (msgbuf));

    if ((result = gen_recv(sock, msgbuf, sizeof msgbuf)))
	goto cancelfail;

    (void)from64tobits (&challenge, msgbuf, sizeof(challenge));

    if (outlevel >= O_DEBUG)
	dumpSmbNtlmAuthChallenge(stdout, &challenge);

    buildSmbNtlmAuthResponse(&challenge, &response,ctl->remotename,ctl->password);

    if (outlevel >= O_DEBUG)
	dumpSmbNtlmAuthResponse(stdout, &response);

    memset(msgbuf,0,sizeof msgbuf);
    to64frombits (msgbuf, &response, SmbLength(&response));

    if (outlevel >= O_MONITOR)
	report(stdout, "%s> %s\n", proto, msgbuf);

    strcat(msgbuf,"\r\n");
    SockWrite (sock, msgbuf, strlen (msgbuf));

    return PS_SUCCESS;

cancelfail: /* cancel authentication and return failure */
    {
	if (outlevel >= O_MONITOR)
	    report(stdout, "%s> *\n", proto);
	SockWrite(sock, "*\r\n", 3);
	return result;
    }
}

#endif /* NTLM_ENABLE */
