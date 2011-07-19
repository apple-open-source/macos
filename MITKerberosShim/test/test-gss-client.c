/*
 * gssClientSample.c - gssSample client program
 *`
 * Copyright 2004-2005 Massachusetts Institute of Technology.
 * All Rights Reserved.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 */


#include <sys/types.h>
#include <sys/uio.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <Kerberos/Kerberos.h>
#include "test-gss-common.h"

/* --------------------------------------------------------------------------- */

static int Connect (const char *inHost, int inPort, int *outFD)
{
    int err = 0;
    int fd = -1;
    struct hostent *hp = NULL;
    struct sockaddr_in saddr;
	
    if (!err) {
        hp = gethostbyname (inHost);
        if (hp == NULL) { err = errno; }
    }
    
    if (!err) {
	saddr.sin_family = hp->h_addrtype;
	memcpy ((char *) &saddr.sin_addr, hp->h_addr, sizeof (saddr.sin_addr));
	saddr.sin_port = htons(inPort);
        
        fd = socket (AF_INET, SOCK_STREAM, 0);
        if (fd < 0) { err = errno; }
    }
    
    if (!err) {
        err = connect (fd, (struct sockaddr *) &saddr, sizeof (saddr));
        if (err < 0) { err = errno; }
    }
    
    if (!err) {
        printf ("connecting to host '%s' on port %d\n", inHost, inPort);
        *outFD = fd;
        fd = -1; /* don't close */
    } else {
         printError (err, "OpenConnection failed");
    }
    
    if (fd >= 0) { close (fd); }
    
    return err; 
}

/* --------------------------------------------------------------------------- */

static int Authenticate (int inSocket, const char *inClientName, const char *inServiceName, gss_ctx_id_t *outContext)
{
    int err = 0;
    OM_uint32 majorStatus;
    OM_uint32 minorStatus = 0;
    gss_name_t serverName;
    gss_name_t clientName;
    gss_cred_id_t clientCredentials = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    OM_uint32 actualFlags = 0;

    char *inputTokenBuffer = NULL;
    size_t inputTokenBufferLength = 0;
    gss_buffer_desc inputToken;  /* buffer received from the server */
    gss_buffer_t inputTokenPtr = GSS_C_NO_BUFFER;
    
    if (inSocket < 0) { err = EINVAL; }


    gss_OID_set *mech_set = malloc(sizeof(gss_OID_set));
    *mech_set = GSS_C_NO_OID_SET;
    majorStatus = gss_indicate_mechs(&minorStatus, mech_set);
    if (majorStatus != GSS_S_COMPLETE) {
        printGSSErrors ("gss_indicate_mechs(mec_set)", majorStatus, minorStatus);
        err = minorStatus ? minorStatus : majorStatus;
    }

    majorStatus = gss_release_oid_set(&minorStatus, mech_set);
    if (majorStatus != GSS_S_COMPLETE) {
        printGSSErrors ("gss_release_oid_set(mec_set)", majorStatus, minorStatus);
        err = minorStatus ? minorStatus : majorStatus;
    }

    majorStatus = gss_krb5_ccache_name(&minorStatus, inClientName, NULL);
    
    /*
     * Here is where the client picks the client principal it wants to use.  
     * We only do this if we know what client principal will get the service 
     * principal we need. Otherwise leave clientCredentials set to NULL.
     */
    
    if (!err) {
        if (inClientName != NULL) {
            gss_buffer_desc nameBuffer = { strlen (inClientName), (char *) inClientName };
            
            majorStatus = gss_import_name (&minorStatus, &nameBuffer, GSS_C_NT_USER_NAME, &clientName); 
            if (majorStatus != GSS_S_COMPLETE) { 
                printGSSErrors ("gss_import_name(inClientName)", majorStatus, minorStatus);
                err = minorStatus ? minorStatus : majorStatus; 
            }
        
            if (!err) {
                majorStatus = gss_acquire_cred (&minorStatus, clientName, GSS_C_INDEFINITE, GSS_C_NO_OID_SET, 
                                                GSS_C_INITIATE, &clientCredentials, NULL, NULL); 
                if (majorStatus != GSS_S_COMPLETE) { 
                    printGSSErrors ("gss_acquire_cred", majorStatus, minorStatus);
                    err = minorStatus ? minorStatus : majorStatus; 
                }
            }
        }
    }

    {
        OM_uint32 lt;
	int cu;
	gss_name_t name;
        gss_OID_set *mech_set = malloc(sizeof(gss_OID_set));
        *mech_set = GSS_C_NO_OID_SET;
        majorStatus = gss_inquire_cred(&minorStatus, clientCredentials, &name, &lt, &cu, mech_set);
        if (majorStatus != GSS_S_COMPLETE) {
            printGSSErrors ("gss_release_oid_set(mec_set)", majorStatus, minorStatus);
            err = minorStatus ? minorStatus : majorStatus;
        }
        majorStatus = gss_release_oid_set(&minorStatus, mech_set);
        if (majorStatus != GSS_S_COMPLETE) {
            printGSSErrors ("gss_release_oid_set(mec_set)", majorStatus, minorStatus);
            err = minorStatus ? minorStatus : majorStatus;
        }
    }
    
    /*
     * Here is where the client picks the service principal it will try to use to
     * connect to the server.  In the case of the gssClientSample, the service
     * principal is passed in on the command line, however, in a real world example,
     * this would be unacceptable from a user interface standpoint since the user
     * shouldn't need to know the server's service principal.
     * 
     * In traditional Kerberos setups, the service principal would be constructed from
     * the type of the service (eg: "imap"), the DNS hostname of the server 
     * (eg: "mailserver.domain.com") and the client's local realm (eg: "DOMAIN.COM") 
     * to form a full principal string (eg: "imap/mailserver.domain.com@DOMAIN.COM").  
     *
     * Now that many sites do not have DNS, this setup is becoming less common.  
     * However you decide to generate the service principal, you need to adhere
     * to the following constraint:  The service principal must be constructed 
     * by the client, typed in by the user or administrator, or transmitted to 
     * the client in a secure manner from a trusted third party -- such as 
     * through an encrypted connection to a directory server.  You should not
     * have the server send the client the service principal name as part of
     * the authentication negotiation -- if you do, cache the name you got
     * after the first try so that the problem below can only happen on the 
     * first connection attempt (similar to what ssh does with host keys).
     *
     * The reason you can't let the server tell the client which principal to 
     * use is that many machines at a site will have their own service principal   
     * and keytab which identifies the machine -- in a Windows Active Directory
     * environment all machines have a service principal and keytab.  Some of these
     * machines (such as a financial services server) will be more trustworthy than 
     * others (such as a random machine on a coworker's desk).  If the owner of  
     * one of these untrustworthy machines can trick the client into using the
     * untrustworthy machine's principal instead of the financial services 
     * server's principal, then he can trick the client into authenticating
     * and connecting to the untrustworthy machine.  The untrustworthy machine can
     * then harvest any confidential information the client sends -- which if the 
     * user thinks he is talking to a financial services server might be an SSN
     * or credit card information.
     *
     * Still confused?  Try thinking of a Kerberos principal as a unique name which
     * describes an entity on the network -- similar to the way a name and photo 
     * uniquely describes a person.  Server machines have service principals as their 
     * names and users have user principals as their names.  The server's keytab and 
     * user's tickets are basically KDC-issued IDs -- similar to a passport or driver's 
     * license.  All the KDC ID does is prove that the entity you are talking to has 
     * a particular principal.  Thus it is important that you know the Kerberos 
     * principal of the entity you want to talk to beforehand or you can be tricked 
     * into talking to the wrong one.  
     * 
     * This identification problem exists in the real world as well: let's say you 
     * are the executor of your grandmother's will and you need to talk to a cousin 
     * who you've never met.  You probably want to check his driver's license before 
     * talking to him and handing over his share of the inheritance.  However, since 
     * you've never met him, you ask your parents (a trusted third party) for his 
     * full name and a description of him.  Otherwise you could be tricked by a con 
     * artist posing as your cousin.
     */
    
    if (!err) {
        gss_buffer_desc nameBuffer = { strlen (inServiceName), (char *) inServiceName };
        
        majorStatus = gss_import_name (&minorStatus, &nameBuffer, (gss_OID) GSS_KRB5_NT_PRINCIPAL_NAME, &serverName); 
        if (majorStatus != GSS_S_COMPLETE) { 
            printGSSErrors ("gss_import_name(inServiceName)", majorStatus, minorStatus);
            err = minorStatus ? minorStatus : majorStatus; 
        }
    }

    /* 
     * The main authentication loop:
     *
     * GSS is a multimechanism API.  The number of packet exchanges required to authenticate 
     * varies between mechanisms.  As a result, we need to loop calling gss_init_sec_context, 
     * passing the "input tokens" received from the server and send the resulting 
     * "output tokens" back until we get GSS_S_COMPLETE or an error.
     */

    majorStatus = GSS_S_CONTINUE_NEEDED;
    while (!err && (majorStatus != GSS_S_COMPLETE)) {
        gss_buffer_desc outputToken = { 0, NULL }; /* buffer to send to the server */
        OM_uint32 requestedFlags = (GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | 
                                    GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG);
        
        printf ("Calling gss_init_sec_context...\n");
        majorStatus = gss_init_sec_context (&minorStatus, clientCredentials, &context, serverName, 
                                            GSS_C_NULL_OID /* mech_type */, requestedFlags, GSS_C_INDEFINITE, 
                                            GSS_C_NO_CHANNEL_BINDINGS, inputTokenPtr,
                                            NULL /* actual_mech_type */, &outputToken, 
                                            &actualFlags, NULL /* time_rec */);
        
        if ((outputToken.length > 0) && (outputToken.value != NULL)) {
            /* Send the output token to the server (even on error) */
            err = WriteToken (inSocket, outputToken.value, outputToken.length);
            
            /* free the output token */
            gss_release_buffer (&minorStatus, &outputToken);
        }
        
        if (!err) {
            if (majorStatus == GSS_S_CONTINUE_NEEDED) { 
                /* Protocol requires another packet exchange */
                
                /* Clean up old input buffer */
                if (inputTokenBuffer != NULL) {
                    free (inputTokenBuffer);
                    inputTokenBuffer = NULL;  /* don't double-free */
                }
                
                /* Read another input token from the server */
                err = ReadToken (inSocket, &inputTokenBuffer, &inputTokenBufferLength);
                
                if (!err) {
                    /* Set up input buffers for the next run through the loop */
                    inputToken.value = inputTokenBuffer;
                    inputToken.length = inputTokenBufferLength;
                    inputTokenPtr = &inputToken;
                }
            } else if (majorStatus != GSS_S_COMPLETE) {
                printGSSErrors ("gss_init_sec_context", majorStatus, minorStatus);
                err = minorStatus ? minorStatus : majorStatus; 
		exit(err);
            }
        }
    }

    int ret;
    majorStatus = gss_compare_name(&minorStatus, clientName, serverName, &ret);
    
    if (!err) { 
        *outContext = context;
    } else {
        printError (err, "AuthenticateToServer failed"); 
    }

    if (serverName        != NULL) { gss_release_name (&minorStatus, &serverName); }
    //if (clientName        != NULL) { gss_release_name (&minorStatus, &clientName); }
    if (inputTokenBuffer  != NULL) { free (inputTokenBuffer); }
    
    return err;
}

/* --------------------------------------------------------------------------- */

static void Usage (const char *argv[])
{
    fprintf (stderr, "Usage: %s [--port portNumber] [--server serverHostName]\n"
             "\t[--sprinc servicePrincipal] [--cprinc clientPrincipal]\n", argv[0]);
    exit (1);
}

/* --------------------------------------------------------------------------- */

int main (int argc, const char *argv[]) 
{
    int err = 0;
    int fd = -1;
    int port = kDefaultPort;
    const char *server = "127.0.0.1";
    const char *clientName = NULL;
    const char *serviceName = "host";
    gss_ctx_id_t context = GSS_C_NO_CONTEXT;
    unsigned int i = 0;

    gss_buffer_desc buffer;
    gss_oid_to_str(&i, GSS_C_NT_EXPORT_NAME, &buffer);

    for (i = 1; (i < argc) && !err; i++) {
        if ((strcmp (argv[i], "--port") == 0) && (i < (argc - 1))) {
            port = strtol (argv[++i], NULL, 0);
            if (port == 0) { err = errno; }
        } else if ((strcmp (argv[i], "--server") == 0) && (i < (argc - 1))) {
            server = argv[++i];
        } else if ((strcmp(argv[i], "--cprinc") == 0) && (i < (argc - 1))) {
            clientName = argv[++i];
        } else if ((strcmp(argv[i], "--sprinc") == 0) && (i < (argc - 1))) {
            serviceName = argv[++i];
        } else {
            err = EINVAL;
        }
    }
    
    if (!err) {
        printf ("%s: Starting up...\n", argv[0]);
        
        err = Connect (server, port, &fd);
    }
    
    if (!err) {
        err = Authenticate (fd, clientName, serviceName, &context);
    }
    
    if (!err) {
        char *buffer = NULL;
        size_t bufferLength = 0;

        /* 
         * Here is where your protocol would go.  This sample client just
         * reads a nul terminated string from the server.
         */
        err = ReadEncryptedToken (fd, context, &buffer, &bufferLength);

        if (!err) {
            printf ("Server message: '%s'\n", buffer);
        }
        
        if (buffer != NULL) { free (buffer); }
    }
    
    
    if (err) {
        if (err == EINVAL) {
            Usage (argv);
        } else {
            printError (err, "Client failed");
        }
    }

    {
        OM_uint32 majorStatus, minorStatus;
        gss_ctx_id_t back;
        gss_cred_id_t *cid;

        majorStatus = gss_export_sec_context(&minorStatus, &context, &buffer);
        majorStatus = gss_import_sec_context(&minorStatus, &buffer, &back);

	cid = malloc(sizeof(gss_cred_id_t));
	*cid = GSS_C_NO_CREDENTIAL;
        majorStatus = gss_release_cred(&minorStatus, cid);
    }
    
    if (fd >= 0) {  printf ("Closing socket.\n"); close (fd); }

    return err ? 1 : 0;
}


