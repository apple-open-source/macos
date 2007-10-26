/*
 * Copyright 2004-2006 Massachusetts Institute of Technology.
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

#include "common.h"

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
        fd = -1; /* takes ownership */
    } else {
         printError (err, "OpenConnection failed");
    }
    
    if (fd >= 0) { close (fd); }
    
    return err; 
}

/* --------------------------------------------------------------------------- */

static int Authenticate (int           inSocket, 
                         const char   *inClientName, 
                         const char   *inServiceName, 
                         gss_ctx_id_t *outGSSContext)
{
    int err = 0;
    OM_uint32 majorStatus;
    OM_uint32 minorStatus = 0;
    gss_name_t serviceName = NULL;
    gss_name_t clientName = NULL;
    gss_cred_id_t clientCredentials = GSS_C_NO_CREDENTIAL;
    gss_ctx_id_t gssContext = GSS_C_NO_CONTEXT;
    OM_uint32 actualFlags = 0;

    char *inputTokenBuffer = NULL;
    size_t inputTokenBufferLength = 0;
    gss_buffer_desc inputToken;  /* buffer received from the server */
    gss_buffer_t inputTokenPtr = GSS_C_NO_BUFFER;
    
    if (inSocket < 0  ) { err = EINVAL; }
    if (!inServiceName) { err = EINVAL; }
    if (!outGSSContext) { err = EINVAL; }
    
    /*
     * Here is where the client picks the client principal it wants to use.  
     * If your application knows what the user's client principal should be,
     * it should set that here. Otherwise leave clientCredentials set to NULL
     * and Kerberos will attempt to use the client principal in the default
     * ccache.
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
     * the authentication negotiation.
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
     * then harvest any confidential information the client sends to it, such as
     * credit card information or social security numbers.
     *
     * If your protocol already involves sending the service principal as part of
     * your authentication negotiation, your client should cache the name it gets
     * after the first successful authentication so that the problem above can 
     * only happen on the first connection attempt -- similar to what ssh does with 
     * host keys. 
     */
    
    if (!err) {
        gss_buffer_desc nameBuffer = { strlen (inServiceName), (char *) inServiceName };
        
        majorStatus = gss_import_name (&minorStatus, &nameBuffer, (gss_OID) GSS_KRB5_NT_PRINCIPAL_NAME, &serviceName); 
        if (majorStatus != GSS_S_COMPLETE) { 
            printGSSErrors ("gss_import_name(inServiceName)", majorStatus, minorStatus);
            err = minorStatus ? minorStatus : majorStatus; 
        }
    }
    
    /* 
     * The main authentication loop:
     *
     * GSS is a multimechanism API.  Because the number of packet exchanges required to  
     * authenticate can vary between mechanisms, we need to loop calling 
     * gss_init_sec_context,  passing the "input tokens" received from the server and 
     * send the resulting "output tokens" back until we get GSS_S_COMPLETE or an error.
     */

    majorStatus = GSS_S_CONTINUE_NEEDED;
    while (!err && (majorStatus != GSS_S_COMPLETE)) {
        gss_buffer_desc outputToken = { 0, NULL }; /* buffer to send to the server */
        OM_uint32 requestedFlags = (GSS_C_MUTUAL_FLAG | GSS_C_REPLAY_FLAG | GSS_C_SEQUENCE_FLAG | 
                                    GSS_C_CONF_FLAG | GSS_C_INTEG_FLAG);
        
        printf ("Calling gss_init_sec_context...\n");
        majorStatus = gss_init_sec_context (&minorStatus, 
                                            clientCredentials, 
                                            &gssContext, 
                                            serviceName, 
                                            GSS_C_NULL_OID /* mech_type */, 
                                            requestedFlags, 
                                            GSS_C_INDEFINITE, 
                                            GSS_C_NO_CHANNEL_BINDINGS, 
                                            inputTokenPtr,
                                            NULL /* actual_mech_type */, 
                                            &outputToken, 
                                            &actualFlags, 
                                            NULL /* time_rec */);
        
        /* Send the output token to the server (even on error) */
        if ((outputToken.length > 0) && (outputToken.value != NULL)) {
            err = WriteToken (inSocket, outputToken.value, outputToken.length);
            
            /* free the output token */
            gss_release_buffer (&minorStatus, &outputToken);
        }
        
        if (!err) {
            if (majorStatus == GSS_S_CONTINUE_NEEDED) { 
                /* Protocol requires another packet exchange */
                
                /* Clean up old input buffer */
                if (inputTokenBuffer) {
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
            }
        }
    }
    
    if (!err) { 
        *outGSSContext = gssContext;
        gssContext = NULL;
    } else {
        printError (err, "AuthenticateToServer failed"); 
    }

    if (inputTokenBuffer) { free (inputTokenBuffer); }
    if (serviceName     ) { gss_release_name (&minorStatus, &serviceName); }
    if (clientName      ) { gss_release_name (&minorStatus, &clientName); }
    
    if (clientCredentials != GSS_C_NO_CREDENTIAL) { 
        gss_release_cred (&minorStatus, &clientCredentials); }
    if (gssContext != GSS_C_NO_CONTEXT) { 
        gss_delete_sec_context (&minorStatus, &gssContext, GSS_C_NO_BUFFER); }

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
    gss_ctx_id_t gssContext = GSS_C_NO_CONTEXT;
    OM_uint32 minorStatus = 0;
    int i = 0;
        
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
        err = Authenticate (fd, clientName, serviceName, &gssContext);
    }
    
    if (!err) {
        char *buffer = NULL;
        size_t bufferLength = 0;

        /* 
         * Here is where your protocol would go.  This sample client just
         * reads a nul terminated string from the server.
         */
        err = ReadEncryptedToken (fd, gssContext, &buffer, &bufferLength);

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
    
    if (fd >= 0) { printf ("Closing socket.\n"); close (fd); }
    if (gssContext != GSS_C_NO_CONTEXT) { 
        gss_delete_sec_context (&minorStatus, &gssContext, GSS_C_NO_BUFFER); }
    
    return err ? 1 : 0;
}


