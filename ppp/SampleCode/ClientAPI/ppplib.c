/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 * Feb 2000 - Christophe Allie - created.
 *
 */


/* -----------------------------------------------------------------------------
ppplib Includes
----------------------------------------------------------------------------- */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>

#include <sys/signal.h>
#include <sys/un.h>


#include "../../Controller/ppp_msg.h"
#include "ppplib.h"


/* -----------------------------------------------------------------------------
ppplib definitions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
Forward declarations
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
ppplib globals
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
PPPLib private functions
----------------------------------------------------------------------------- */


/* -----------------------------------------------------------------------------
register a client to the ppp library
parameters :
     ref : ppplib maintains a reference for each client, it will return the ref
           to the client upon successful registration.
           this ref will be use through all ppplib calls
     connection : connection number (ppp0, 1, ...)
     notifier : the client can provide a callback function to receive
                event asynchronously
return code
     errPPPNoError : no problem !
     errPPPTooManyRefs : number of clients already reached
----------------------------------------------------------------------------- */
int PPPInit(int *ref, u_short reserved, u_long reserved1)
{
    int 		s, err;
    struct sockaddr_un	adr;

    s = socket(AF_LOCAL, SOCK_STREAM, 0);

    bzero(&adr, sizeof(adr));
    adr.sun_family = AF_LOCAL;
    strcpy(adr.sun_path, PPP_PATH);

    if (err = connect(s,  (struct sockaddr *)&adr, sizeof(adr)) < 0) {
        return errno;
    }

    *ref = s;
    return 0;
}

/* -----------------------------------------------------------------------------
deregister a client from the ppp library
parameters :
     ref : reference for this client
return code
     errPPPNoError : no problem !
     errPPPBadRef : reference invalid
----------------------------------------------------------------------------- */
int PPPDispose(int ref)
{
   if (close(ref) < 0)
       return errno;
    return 0;
}


/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
static int SendCmd(int ref, u_int32_t cmd)
{
    struct ppp_msg_hdr	msg;
    u_char		*data;

    bzero(&msg, sizeof(msg));
    msg.m_type = cmd;

    if (write(ref, &msg, sizeof(msg)) != sizeof(msg))     //  send now the command
        return errno;

   if (read(ref, &msg, sizeof(msg)) != sizeof(msg))	  // always expect a reply
        return errno;
    else {
        if (msg.m_len) {
            data = malloc(msg.m_len);
            if (data) {
                if (read(ref, &data, msg.m_len) != msg.m_len) // read data
                    return errno;
                free(data);
            }
        }
    }

    return msg.m_result;
}

/* -----------------------------------------------------------------------------
start the ppp connection
parameters :
     ref : reference for this client
return code
     errPPPNoError : no problem, the ppp connection has been initiated.
                     poll with PPPGetState, or wait to be notified to know
                     about the actual result of the connection
----------------------------------------------------------------------------- */
int PPPConnect(int ref)
{
    return SendCmd(ref, PPP_CONNECT);
}

/* -----------------------------------------------------------------------------
stop the ppp connection
parameters :
     ref : reference for this client
return code
     errPPPNoError : no problem, the ppp connection has been stopped.
                     poll with PPPGetState, or wait to be notified to know
                     about the actual result of the disconnection
----------------------------------------------------------------------------- */
int PPPDisconnect(int ref)
{
    return SendCmd(ref, PPP_DISCONNECT);
}

/* -----------------------------------------------------------------------------
set options for the connection
parameters :
     ref : reference for this client
     data : pointer to a list of options
return code
     errPPPNoError :
----------------------------------------------------------------------------- */
int PPPSetOption(int ref, u_short option, void *data, u_short len)
{
    struct ppp_msg_hdr	msg;
    u_char		*p;
    struct ppp_opt_hdr 	opt;
    u_char d[300]; // temp hack

    bzero(&msg, sizeof(msg));
    msg.m_type = PPP_SETOPTION;
    msg.m_len = len + sizeof(opt);

    opt.o_type = option;

    if (write(ref, &msg, sizeof(msg)) != sizeof(msg))
        return errno;

    *((u_long*)&d[0]) = option;
    bcopy(data, d+4, len);
//    if (write(ref, &opt, sizeof(opt)) != sizeof(opt))
//        return errno;

    //    if (write(ref, data, len) != len)
    //        return errno;

    if (write(ref, d, msg.m_len) != msg.m_len)
            return errno;

    if (read(ref, &msg, sizeof(msg)) != sizeof(msg))	  // always expect a reply
        return errno;
    else {
        if (msg.m_len) {
            printf("11\n");
            p = malloc(msg.m_len);
            if (p) {
                if (read(ref, &p, msg.m_len) != msg.m_len) // read data
                    return errno;
                free(p);
            }
        }
    }

    return msg.m_result;
}

/* -----------------------------------------------------------------------------
get options for the connection
parameters :
     ref : reference for this client
     data : pointer to a list of options
return code
     errPPPNoError :
----------------------------------------------------------------------------- */
int PPPGetOption(int ref, u_short option, void *data, u_short *len)
{
    struct ppp_msg_hdr	msg;
    u_char		p[100];
    u_long		l;
    struct ppp_opt_hdr 	opt;

    bzero(&msg, sizeof(msg));
    msg.m_type = PPP_GETOPTION;
    msg.m_len = sizeof(opt);

    opt.o_type = option;

    if (write(ref, &msg, sizeof(msg)) != sizeof(msg))
        return errno;

    if (write(ref, &opt, sizeof(opt)) != sizeof(opt))
        return errno;

    if (read(ref, &msg, sizeof(msg)) != sizeof(msg))	  // always expect a reply
        return errno;
    else {
       if (msg.m_len) {
            //p = malloc(msg.m_len);
            if (p) {
                if (read(ref, &p, msg.m_len) != msg.m_len) // read data
                    return errno;
                l = msg.m_len - sizeof(opt);
                if (*len >=  l) {
                   bcopy(p + sizeof(opt), data, l);
                    *len = l;
                }
             //  free(p);
            }
        }
    }
    return msg.m_result;
}

/* -----------------------------------------------------------------------------
get the state of the connection
parameters :
     ref : reference for this client
     stat : pointer to status structure
return code :
     all the possible socket errors, in particular :
     ENXIO : pppk not loaded
----------------------------------------------------------------------------- */
int PPPStatus(int ref, struct ppp_status *stat)
{
    struct ppp_msg_hdr	msg;

    bzero(&msg, sizeof(msg));
    msg.m_type = PPP_STATUS;

    if (write(ref, &msg, sizeof(msg)) != sizeof(msg)) {
       return errno;
    }
    if (read(ref, &msg, sizeof(msg)) != sizeof(msg)) {	  // always expect a reply
       return errno;
    }
    else {
        if (msg.m_len == sizeof(struct ppp_status)) {
            if (read(ref, stat, sizeof(struct ppp_status)) != sizeof(struct ppp_status)) // read data
                return errno;
        }
        // empty the socket ???
    }
    return msg.m_result;
}


