/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * February 2000 - created.
 */

/* -----------------------------------------------------------------------------
includes
----------------------------------------------------------------------------- */

#include <string.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/errno.h>
#include <sys/signal.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <net/if.h>
#include <CoreFoundation/CoreFoundation.h>
#define	SYSTEMCONFIGURATION_NEW_API
#include <SystemConfiguration/SystemConfiguration.h>

#include "ppp_msg.h"
#include "ppp_privmsg.h"
#include "../Family/PPP.kmodproj/ppp.h"

#include "fsm.h"
#include "lcp.h"
#include "ipcp.h"
#include "chap.h"
#include "upap.h"
#include "auth.h"
#include "ppp_client.h"
#include "ppp_option.h"
#include "ppp_utils.h"
#include "ppp_command.h"
#include "ppp_manager.h"
#include "ppp_utils.h"
#include "link.h"

/* -----------------------------------------------------------------------------
definitions
----------------------------------------------------------------------------- */
//#define MIN(a, b) ((a) < (b) ? (a) : (b))



/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long get_long_opt (u_short id, struct opt_long *option, u_long *opt)
{

    if (option->set) {
        PRINTF(("    ---> Client %d, option is set = %ld  \n", id, option->val));
        *opt = option->val;
    }
    else {
        *opt = 0;
    }
    
    return 1;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long get_str_opt (u_short id, struct opt_str *option, char *opt, u_int32_t *maxlen)
{
    unsigned short 		len;

    if (option->set) {
        PRINTF(("    ---> Client %d, option is set = %s  \n", id, option->str));
        len = MIN(*maxlen, strlen(option->str) + 1);
        strncpy(opt, option->str, len);
        opt[len - 1] = 0;
        *maxlen = len;
    }
    else {
        *opt = 0;
        *maxlen = 0;
    }

    return 0;
}

/* -----------------------------------------------------------------------------
----------------------------------------------------------------------------- */
u_long ppp_getoption (u_short id, struct msg *msg)
{
    struct ppp_opt 		*opt = (struct ppp_opt *)&msg->data[0];
    struct options		*opts;
    u_long 			lval, lval1, err = 0, done;
    struct ppp			*ppp = ppp_findbyref(msg->hdr.m_link);

    if (!ppp) {
           msg->hdr.m_result = ENODEV;
           msg->hdr.m_len = 0;
           return 0;
       }

    if (ppp->phase == PPP_RUNNING) {

        done = 1;
        // get option negociated
        switch (opt->o_type) {
            case PPP_OPT_LCP_HDRCOMP:
                *(u_long*)(&opt->o_data[0]) = ppp->lcp_txpcomp | (ppp->lcp_txaccomp << 1);
                break;
            case PPP_OPT_LCP_MTU:
                *(u_int32_t *)&opt->o_data[0] = ppp->lcp_mtu;
                break;
            case PPP_OPT_LCP_TXACCM:
                *(u_int32_t *)&opt->o_data[0] = ppp->lcp_txasyncmap;
                break;
           case PPP_OPT_LCP_RCACCM:
               *(u_int32_t *)&opt->o_data[0] = ppp->lcp_rcasyncmap;
                break;
            case PPP_OPT_IPCP_HDRCOMP:
                *(u_int32_t *)&opt->o_data[0] = ppp->ipcp_hisoptions.neg_vj;
                break;
            case PPP_OPT_IPCP_LOCALADDR:
                err = ppp_getaddr(ppp, OSIOCGIFADDR, (u_int32_t *)&opt->o_data[0]);
                break;
            case PPP_OPT_IPCP_REMOTEADDR:
                err = ppp_getaddr(ppp, OSIOCGIFDSTADDR, (u_int32_t *)&opt->o_data[0]);
                break;
            case PPP_OPT_COMM_LOOPBACK:
                err = link_getloopback(ppp, (u_int32_t *)&opt->o_data[0]);
                break;
            case PPP_OPT_COMM_CONNECTSPEED:
                *(u_int32_t *)&opt->o_data[0] = ppp->connect_speed;
                break;
            default:
                done = 0;
        };

        if (done) {
            msg->hdr.m_len = 0;
            msg->hdr.m_result = err;
            if (!err)
                msg->hdr.m_len = sizeof(struct ppp_opt_hdr) + sizeof(long);

            return 0;
        }
    }

    // not connected, get the client options that will be used.
    opts = client_findoptset(id, msg->hdr.m_link);
    if (!opts) {
        opts = &ppp->def_options;		// first option used by client, use default set
    }
    
    switch (opt->o_type) {

        // COMM options
        case PPP_OPT_DEV_NAME:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->dev.name, &opt->o_data[0], &msg->hdr.m_len);
            break;
        case PPP_OPT_DEV_SPEED:
            get_long_opt(id, &opts->dev.speed, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_DEV_CONNECTSCRIPT:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->dev.connectscript, &opt->o_data[0], &msg->hdr.m_len);
            break;
#if 0
        case PPP_OPT_DEV_CONNECTPRGM:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->dev.connectprgm, &opt->o_data[0], &msg->hdr.m_len);
            break;
#endif
        case PPP_OPT_DEV_CAPS:
            msg->hdr.m_len = sizeof(struct ppp_caps);
            bcopy(&ppp->link_caps, &opt->o_data[0], sizeof(struct ppp_caps));
            break;

        case PPP_OPT_COMM_TERMINALMODE:
   	    get_long_opt(id, &opts->comm.terminalmode, (u_long *)&opt->o_data[0]);
	    msg->hdr.m_len = 4;
	    break;	    
        case PPP_OPT_COMM_TERMINALSCRIPT:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->comm.terminalscript, &opt->o_data[0], &msg->hdr.m_len);
            break;
#if 0
        case PPP_OPT_COMM_TERMINALPRGM:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->comm.terminalprgm, &opt->o_data[0], &msg->hdr.m_len);
            break;
#endif
        case PPP_OPT_COMM_IDLETIMER:
            get_long_opt(id, &opts->comm.idletimer, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_COMM_SESSIONTIMER:
            get_long_opt(id, &opts->comm.sessiontimer, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_COMM_REMOTEADDR:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->comm.remoteaddr, &opt->o_data[0], &msg->hdr.m_len);
            break;
        case PPP_OPT_COMM_LISTENFILTER:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->comm.listenfilter, &opt->o_data[0], &msg->hdr.m_len);
            break;
        case PPP_OPT_COMM_CONNECTDELAY:
            get_long_opt(id, &opts->comm.connectdelay, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;

            // LCP options
        case PPP_OPT_LCP_HDRCOMP:
            get_long_opt(id, &opts->lcp.pcomp, &lval);
            get_long_opt(id, &opts->lcp.accomp, &lval1);
            *(u_long*)(&opt->o_data[0]) = lval1 | (lval << 1);
           msg->hdr.m_len = 4;
            break;
        case PPP_OPT_LCP_MRU:
            get_long_opt(id, &opts->lcp.mru, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_LCP_MTU:
            get_long_opt(id, &opts->lcp.mtu, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_LCP_RCACCM:
            get_long_opt(id, &opts->lcp.rcaccm, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_LCP_TXACCM:
            get_long_opt(id, &opts->lcp.txaccm, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_LCP_ECHO:
            get_long_opt(id, &opts->lcp.echo, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;

            // SEC options
        case PPP_OPT_AUTH_PROTO:
            get_long_opt(id, &opts->auth.proto, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_AUTH_NAME:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->auth.name, &opt->o_data[0], &msg->hdr.m_len);
            break;
        case PPP_OPT_AUTH_PASSWD:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->auth.passwd, &opt->o_data[0], &msg->hdr.m_len);
            break;

            // IPCP options
        case PPP_OPT_IPCP_HDRCOMP:
            get_long_opt(id, &opts->ipcp.hdrcomp, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_IPCP_LOCALADDR:
            get_long_opt(id, &opts->ipcp.localaddr, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_IPCP_REMOTEADDR:
            get_long_opt(id, &opts->ipcp.remoteaddr, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_IPCP_USESERVERDNS:
            get_long_opt(id, &opts->ipcp.useserverdns, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_IPCP_IN_DNS1:
            // private call, manipulate default options...
            get_long_opt(id, &ppp->def_options.ipcp.serverdns1, (u_long *)&opt->o_data[0]);
            //get_long_opt(id, &opts->ipcp.serverdns1, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_IPCP_IN_DNS2:
            // private call, manipulate default options...
            get_long_opt(id, &ppp->def_options.ipcp.serverdns1, (u_long *)&opt->o_data[0]);
            //get_long_opt(id, &opts->ipcp.serverdns2, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
           // MISC options
        case PPP_OPT_LOGFILE:
            msg->hdr.m_len = OPT_STR_LEN;
            get_str_opt(id, &opts->misc.logfile, &opt->o_data[0], &msg->hdr.m_len);
            break;
        case PPP_OPT_REMINDERTIMER:
            get_long_opt(id, &opts->misc.remindertimer, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_ALERTENABLE:
            get_long_opt(id, &opts->misc.alertenable, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            // COMM options
        case PPP_OPT_COMM_LOOPBACK:
            get_long_opt(id, &opts->comm.loopback, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_AUTOLISTEN:
            // private call, manipulate default options...
            get_long_opt(id, &ppp->def_options.misc.autolisten, (u_long *)&opt->o_data[0]);
            //get_long_opt(id, &opts->misc.autolisten, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_AUTOCONNECT:
            get_long_opt(id, &opts->misc.autoconnect, (u_long *)&opt->o_data[0]);
            msg->hdr.m_len = 4;
            break;
        case PPP_OPT_SERVICEID:
            sprintf(&opt->o_data[0], "%ld", ppp->serviceID);
            msg->hdr.m_len = strlen(&opt->o_data[0]);
            break;


        default:
            msg->hdr.m_result = EOPNOTSUPP;
            msg->hdr.m_len = 0;
            return 0;
    };

    msg->hdr.m_result = 0;
    msg->hdr.m_len += sizeof(struct ppp_opt_hdr);
    return 0;
}

