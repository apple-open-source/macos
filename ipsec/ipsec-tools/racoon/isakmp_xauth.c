/*	$NetBSD: isakmp_xauth.c,v 1.11.6.1 2007/08/07 04:49:24 manu Exp $	*/

/* Id: isakmp_xauth.c,v 1.38 2006/08/22 18:17:17 manubsd Exp */

/*
 * Copyright (C) 2004-2005 Emmanuel Dreyfus
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/queue.h>

#include <netinet/in.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <pwd.h>
#include <grp.h>
#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif
#include <netdb.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <ctype.h>
#include <resolv.h>

#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif

#include "var.h"
#include "misc.h"
#include "vmbuf.h"
#include "plog.h"
#include "sockmisc.h"
#include "schedule.h"
#include "debug.h"
#include "fsm.h"

#include "crypto_openssl.h"
#include "isakmp_var.h"
#include "isakmp.h"
#include "handler.h"
#include "throttle.h"
#include "remoteconf.h"
#include "isakmp_inf.h"
#include "isakmp_xauth.h"
#include "isakmp_unity.h"
#include "isakmp_cfg.h"
#include "strnames.h"
#include "ipsec_doi.h"
#include "remoteconf.h"
#include "localconf.h"
#include "vpn_control.h"
#include "vpn_control_var.h"
#include "ipsecSessionTracer.h"
#include "ipsecMessageTracer.h"


void 
xauth_sendreq(iph1)
	phase1_handle_t *iph1;
{
	vchar_t *buffer;
	struct isakmp_pl_attr *attr;
	struct isakmp_data *typeattr;
	struct isakmp_data *usrattr;
	struct isakmp_data *pwdattr;
	struct xauth_state *xst = &iph1->mode_cfg->xauth;
	size_t tlen;

	/* Status checks */
	if (!FSM_STATE_IS_ESTABLISHED(iph1->status)) {
		plog(ASL_LEVEL_ERR, 
		    "Xauth request while phase 1 is not completed\n");
		return;
	}

	if (xst->status != XAUTHST_NOTYET) {
		plog(ASL_LEVEL_ERR, 
		    "Xauth request whith Xauth state %d\n", xst->status);
		return;
	}

	plog(ASL_LEVEL_INFO, "Sending Xauth request\n");

	tlen = sizeof(*attr) +
	       + sizeof(*typeattr) +
	       + sizeof(*usrattr) +
	       + sizeof(*pwdattr);
	
	if ((buffer = vmalloc(tlen)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate buffer\n");
		return;
	}
	
	attr = (struct isakmp_pl_attr *)buffer->v;
	memset(attr, 0, tlen);

	attr->h.len = htons(tlen);
	attr->type = ISAKMP_CFG_REQUEST;
	attr->id = htons(eay_random());

	typeattr = (struct isakmp_data *)(attr + 1);
	typeattr->type = htons(XAUTH_TYPE | ISAKMP_GEN_TV);
	typeattr->lorv = htons(XAUTH_TYPE_GENERIC);

	usrattr = (struct isakmp_data *)(typeattr + 1);
	usrattr->type = htons(XAUTH_USER_NAME | ISAKMP_GEN_TLV);
	usrattr->lorv = htons(0);

	pwdattr = (struct isakmp_data *)(usrattr + 1);
	pwdattr->type = htons(XAUTH_USER_PASSWORD | ISAKMP_GEN_TLV);
	pwdattr->lorv = htons(0);

	isakmp_cfg_send(iph1, buffer, 
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 1, 0, NULL);
	
	vfree(buffer);

	xst->status = XAUTHST_REQSENT;

	return;
}

int
xauth_attr_reply(iph1, attr, id)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
	int id;
{
	char **outlet = NULL;
	size_t alen = 0;
	int type;
	struct xauth_state *xst = &iph1->mode_cfg->xauth;

	if ((iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) == 0) {
		plog(ASL_LEVEL_ERR, 
		    "Xauth reply but peer did not declare "
		    "itself as Xauth capable\n");
		return -1;
	}

	if (xst->status != XAUTHST_REQSENT) {
		plog(ASL_LEVEL_ERR, 
		    "Xauth reply while Xauth state is %d\n", xst->status);
		return -1;
	}

	type = ntohs(attr->type) & ~ISAKMP_GEN_MASK;
	switch (type) {
	case XAUTH_TYPE:
		switch (ntohs(attr->lorv)) {
		case XAUTH_TYPE_GENERIC:
			xst->authtype = XAUTH_TYPE_GENERIC;
			break;
		default:
			plog(ASL_LEVEL_WARNING, 
			    "Unexpected authentication type %d\n", 
			    ntohs(type));
			return -1;
		}
		break;

	case XAUTH_USER_NAME:
		outlet = &xst->authdata.generic.usr;
		break;

	case XAUTH_USER_PASSWORD:
		outlet = &xst->authdata.generic.pwd; 
		break;

	default:
		plog(ASL_LEVEL_WARNING, 
		    "ignored Xauth attribute %d\n", type);
		break;
	}

	if (outlet != NULL) {
		alen = ntohs(attr->lorv);

		if ((*outlet = racoon_realloc(*outlet, alen + 1)) == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot allocate memory for Xauth Data\n");
			return -1;
		}

		memcpy(*outlet, attr + 1, alen);
		(*outlet)[alen] = '\0';
		outlet = NULL;
	}

	
	if ((xst->authdata.generic.usr != NULL) &&
	   (xst->authdata.generic.pwd != NULL)) {
		int port;
		int res;
		char *usr = xst->authdata.generic.usr;
		char *pwd = xst->authdata.generic.pwd;
		time_t throttle_delay = 0;

#if 0	/* Real debug, don't do that at home */
		plog(ASL_LEVEL_DEBUG, 
		    "Got username \"%s\", password \"%s\"\n", usr, pwd);
#endif
		strlcpy(iph1->mode_cfg->login, usr, sizeof(iph1->mode_cfg->login));

		res = -1;
		if ((port = isakmp_cfg_getport(iph1)) == -1) {
			plog(ASL_LEVEL_ERR, 
			    "Port pool depleted\n");
			goto skip_auth;
		}	

		switch (isakmp_cfg_config.authsource) {
		case ISAKMP_CFG_AUTH_SYSTEM:
			res = xauth_login_system(usr, pwd);
			break;

		default:
			plog(ASL_LEVEL_ERR, 
			    "Unexpected authentication source\n");
			res = -1;
			break;
		}

		/*
		 * Optional group authentication
		 */
		if (!res && (isakmp_cfg_config.groupcount))
			res = group_check(iph1,
				isakmp_cfg_config.grouplist,
				isakmp_cfg_config.groupcount);

		/*
		 * On failure, throttle the connexion for the remote host
		 * in order to make password attacks more difficult.
		 */
		throttle_delay = throttle_host(iph1->remote, res) - time(NULL);
		if (throttle_delay > 0) {
			char *str;

			str = saddrwop2str((struct sockaddr *)iph1->remote);

			plog(ASL_LEVEL_ERR, 
			    "Throttling in action for %s: delay %lds\n",
			    str, (unsigned long)throttle_delay);
			res = -1;
		} else {
			throttle_delay = 0;
		}

skip_auth:
		if (throttle_delay != 0) {
			struct xauth_reply_arg *xra;

			if ((xra = racoon_malloc(sizeof(*xra))) == NULL) {
				plog(ASL_LEVEL_ERR, 
				    "malloc failed, bypass throttling\n");
				return xauth_reply(iph1, port, id, res);
			}

			/*
			 * We need to store the ph1, but it might have
			 * disapeared when xauth_reply is called, so
			 * store the index instead.
			 */
			xra->index = iph1->index;
			xra->port = port;
			xra->id = id;
			xra->res = res;
			sched_new(throttle_delay, xauth_reply_stub, xra);
		} else {
			return xauth_reply(iph1, port, id, res);
		}
	}

	return 0;
}

void 
xauth_reply_stub(args)
	void *args;
{
	struct xauth_reply_arg *xra = (struct xauth_reply_arg *)args;
	phase1_handle_t *iph1;

	if ((iph1 = ike_session_getph1byindex(NULL, &xra->index)) != NULL)
		(void)xauth_reply(iph1, xra->port, xra->id, xra->res);
	else
		plog(ASL_LEVEL_ERR, 
		    "Delayed Xauth reply: phase 1 no longer exists.\n"); 

	racoon_free(xra);
	return;
}

int
xauth_reply(iph1, port, id, res)
	phase1_handle_t *iph1;
	int port;
	int id;
{
	struct xauth_state *xst = &iph1->mode_cfg->xauth;
	char *usr = xst->authdata.generic.usr;

	if (iph1->is_dying) {
		plog(ASL_LEVEL_INFO, 
			 "dropped login for user \"%s\"\n", usr);
		return -1;
	}

	if (res != 0) {
		if (port != -1)
			isakmp_cfg_putport(iph1, port);

		plog(ASL_LEVEL_INFO, 
		    "login failed for user \"%s\"\n", usr);
		
		xauth_sendstatus(iph1, XAUTH_STATUS_FAIL, id);
		xst->status = XAUTHST_NOTYET;

		/* Delete Phase 1 SA */
		if (FSM_STATE_IS_ESTABLISHED(iph1->status))
			isakmp_info_send_d1(iph1);
		isakmp_ph1expire(iph1);

		return -1;
	}

	xst->status = XAUTHST_OK;
	plog(ASL_LEVEL_INFO, 
	    "login succeeded for user \"%s\"\n", usr);

	xauth_sendstatus(iph1, XAUTH_STATUS_OK, id);

	return 0;
}

void
xauth_sendstatus(iph1, status, id)
	phase1_handle_t *iph1;
	int status;
	int id;
{
	vchar_t *buffer;
	struct isakmp_pl_attr *attr;
	struct isakmp_data *stattr;
	size_t tlen;

	tlen = sizeof(*attr) +
	       + sizeof(*stattr); 
	
	if ((buffer = vmalloc(tlen)) == NULL) {
		plog(ASL_LEVEL_ERR, "Cannot allocate buffer\n");
		return;
	}
	
	attr = (struct isakmp_pl_attr *)buffer->v;
	memset(attr, 0, tlen);

	attr->h.len = htons(tlen);
	attr->type = ISAKMP_CFG_SET;
	attr->id = htons(id);

	stattr = (struct isakmp_data *)(attr + 1);
	stattr->type = htons(XAUTH_STATUS | ISAKMP_GEN_TV);
	stattr->lorv = htons(status);

	isakmp_cfg_send(iph1, buffer, 
	    ISAKMP_NPTYPE_ATTR, ISAKMP_FLAG_E, 1, 0, NULL);
	
	vfree(buffer);

	return;	
}


int
xauth_login_system(usr, pwd)
	char *usr;
	char *pwd;
{
	struct passwd *pw;
	char *cryptpwd;
	char *syscryptpwd;
#ifdef HAVE_SHADOW_H
	struct spwd *spw;

	if ((spw = getspnam(usr)) == NULL)
		return -1;

	syscryptpwd = spw->sp_pwdp;
#endif

	if ((pw = getpwnam(usr)) == NULL)
		return -1;

#ifndef HAVE_SHADOW_H
	syscryptpwd = pw->pw_passwd;
#endif

	/* No root login. Ever. */
	if (pw->pw_uid == 0)
		return -1;

	if ((cryptpwd = crypt(pwd, syscryptpwd)) == NULL)
		return -1;

	if (strcmp(cryptpwd, syscryptpwd) == 0)
		return 0;

	return -1;
}

int
xauth_group_system(usr, grp)
	char * usr;
	char * grp;
{
	struct group * gr;
	char * member;
	int index = 0;

	gr = getgrnam(grp);
	if (gr == NULL) {
		plog(ASL_LEVEL_ERR, 
			"the system group name \'%s\' is unknown\n",
			grp);
		return -1;
	}

	while ((member = gr->gr_mem[index++])!=NULL) {
		if (!strcmp(member,usr)) {
			plog(ASL_LEVEL_INFO, 
		                "membership validated\n");
			return 0;
		}
	}

	return -1;
}

int 
xauth_check(iph1)
	phase1_handle_t *iph1;
{
	struct xauth_state *xst = &iph1->mode_cfg->xauth;

	/* 
 	 * Only the server side (edge device) really check for Xauth 
	 * status. It does it if the chose authmethod is using Xauth.
	 * On the client side (roadwarrior), we don't check anything.
	 */
	switch (AUTHMETHOD(iph1)) {
	case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
	/* The following are not yet implemented */
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_R:
	case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_R:
		if ((iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) == 0) {
			plog(ASL_LEVEL_ERR, 
			    "Hybrid auth negotiated but peer did not "
			    "announced as Xauth capable\n");
			return -1;
		}

		if (xst->status != XAUTHST_OK) {
			plog(ASL_LEVEL_ERR, 
			    "Hybrid auth negotiated but peer did not "
			    "succeed Xauth exchange\n");
			return -1;
		}

		return 0;
		break;
	default:
		return 0;
		break;
	}

	return 0;
}

int
group_check(iph1, grp_list, grp_count)
	phase1_handle_t *iph1;
	char **grp_list;
	int grp_count;
{
	int res = -1;
	int grp_index = 0;
	char * usr = NULL;

	/* check for presence of modecfg data */

	if(iph1->mode_cfg == NULL) {
		plog(ASL_LEVEL_ERR, 
			"xauth group specified but modecfg not found\n");
		return res;
	}

	/* loop through our group list */

	for(; grp_index < grp_count; grp_index++) {

		/* check for presence of xauth data */

		usr = iph1->mode_cfg->xauth.authdata.generic.usr;

		if(usr == NULL) {
			plog(ASL_LEVEL_ERR, 
				"xauth group specified but xauth not found\n");
			return res;
		}

		/* call appropriate group validation funtion */

		switch (isakmp_cfg_config.groupsource) {

			case ISAKMP_CFG_GROUP_SYSTEM:
				res = xauth_group_system(
					usr,
					grp_list[grp_index]);
				break;

			default:
				/* we should never get here */
				plog(ASL_LEVEL_ERR, 
				    "Unknown group auth source\n");
				break;
		}

		if( !res ) {
			plog(ASL_LEVEL_INFO, 
				"user \"%s\" is a member of group \"%s\"\n",
				usr,
				grp_list[grp_index]);
			break;
		} else {
			plog(ASL_LEVEL_INFO, 
				"user \"%s\" is not a member of group \"%s\"\n",
				usr,
				grp_list[grp_index]);
		}
	}

	return res;
}

vchar_t *
isakmp_xauth_req(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	int type;
	size_t dlen = 0;
	int ashort = 0;
	int value = 0;
	vchar_t *buffer = NULL;
	char* mraw = NULL;
	vchar_t *mdata = NULL;
	char *data;
	vchar_t *usr = NULL;
	vchar_t *pwd = NULL;
	size_t skip = 0;
	int freepwd = 0;

	if ((iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) == 0) {
		plog(ASL_LEVEL_ERR, 
		    "Xauth mode config request but peer "
		    "did not declare itself as Xauth capable\n");
		return NULL;
	}

	type = ntohs(attr->type) & ~ISAKMP_GEN_MASK;

	/* Sanity checks */
	switch(type) {
	case XAUTH_TYPE:
		if ((ntohs(attr->type) & ISAKMP_GEN_TV) == 0) {
			plog(ASL_LEVEL_ERR, 
			    "Unexpected long XAUTH_TYPE attribute\n");
			return NULL;
		}
		if (ntohs(attr->lorv) != XAUTH_TYPE_GENERIC) {
			plog(ASL_LEVEL_ERR, 
			    "Unsupported Xauth authentication %d\n", 
			    ntohs(attr->lorv));
			return NULL;
		}
		ashort = 1;
		dlen = 0;
		value = XAUTH_TYPE_GENERIC;
		break;

	case XAUTH_USER_NAME:
		if (!iph1->rmconf->xauth || !iph1->rmconf->xauth->login) {
			plog(ASL_LEVEL_ERR, "Xauth performed "
			    "with no login supplied\n");
			return NULL;
		}

		dlen = iph1->rmconf->xauth->login->l - 1;
		iph1->rmconf->xauth->state |= XAUTH_SENT_USERNAME;
		break;

	case XAUTH_USER_PASSWORD:
	case XAUTH_PASSCODE:
		if (!iph1->rmconf->xauth || !iph1->rmconf->xauth->login)
			return NULL;

		skip = sizeof(struct ipsecdoi_id_b);
		usr = vmalloc(iph1->rmconf->xauth->login->l - 1 + skip);
		if (usr == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "Cannot allocate memory\n");
			return NULL;
		}
		memset(usr->v, 0, skip);
		memcpy(usr->v + skip, 
		    iph1->rmconf->xauth->login->v, 
		    iph1->rmconf->xauth->login->l - 1);

		if (iph1->rmconf->xauth->pass) {
			/* A key given through racoonctl */
			pwd = iph1->rmconf->xauth->pass;
		} else {
			if ((pwd = getpskbyname(usr)) == NULL) {
				plog(ASL_LEVEL_ERR, 
				    "No password was found for login %s\n", 
				    iph1->rmconf->xauth->login->v);
				vfree(usr);
				return NULL;
			}
			/* We have to free it before returning */
			freepwd = 1;
		}
		vfree(usr);

		iph1->rmconf->xauth->state |= XAUTH_SENT_PASSWORD;
		dlen = pwd->l - 1;

		break;
		
	case XAUTH_MESSAGE:
		if ((ntohs(attr->type) & ISAKMP_GEN_TV) == 0) {
			dlen = ntohs(attr->lorv);
			if (dlen > 0) {
				mraw = (char*)(attr + 1);
				if ((mdata = vmalloc(dlen)) == NULL) {
					plog(ASL_LEVEL_ERR,
					    "Cannot allocate memory\n");
					return NULL;
				}
				memcpy(mdata->v, mraw, mdata->l);
				plog(ASL_LEVEL_NOTICE, "XAUTH Message: '%s'.\n",
					binsanitize(mdata->v, mdata->l));
				vfree(mdata);
			}
		}
		return NULL;
	default:
		plog(ASL_LEVEL_WARNING, 
		    "Ignored attribute %s\n", s_isakmp_cfg_type(type));
		return NULL;
		break;
	}

	if ((buffer = vmalloc(sizeof(*attr) + dlen)) == NULL) {
		plog(ASL_LEVEL_ERR, 
		    "Cannot allocate memory\n");
		goto out;
	}

	attr = (struct isakmp_data *)buffer->v;
	if (ashort) {
		attr->type = htons(type | ISAKMP_GEN_TV);
		attr->lorv = htons(value);
		goto out;
	}

	attr->type = htons(type | ISAKMP_GEN_TLV);
	attr->lorv = htons(dlen);
	data = (char *)(attr + 1);

	switch(type) {
	case XAUTH_USER_NAME:
		/* 
		 * iph1->rmconf->xauth->login->v is valid, 
		 * we just checked it in the previous switch case 
		 */
		memcpy(data, iph1->rmconf->xauth->login->v, dlen);
		break;
	case XAUTH_USER_PASSWORD:
	case XAUTH_PASSCODE:
		memcpy(data, pwd->v, dlen);
		break;
	default:
		break;
	}

out:
	if (freepwd)
		vfree(pwd);

	return buffer;
}

vchar_t *
isakmp_xauth_set(iph1, attr)
	phase1_handle_t *iph1;
	struct isakmp_data *attr;
{
	int type;
	vchar_t *buffer = NULL;
	struct xauth_state *xst;
	size_t dlen = 0;
	char* mraw = NULL;
	vchar_t *mdata = NULL;

	if ((iph1->mode_cfg->flags & ISAKMP_CFG_VENDORID_XAUTH) == 0) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_XAUTH_DROP,
								CONSTSTR("XAUTH is not supported by peer"),
								CONSTSTR("XAUTH dropped (not supported by peer)"));
		plog(ASL_LEVEL_ERR, 
		    "Xauth mode config set but peer "
		    "did not declare itself as Xauth capable\n");
		return NULL;
	}

	type = ntohs(attr->type) & ~ISAKMP_GEN_MASK;

	switch(type) {
	case XAUTH_STATUS:
		/* 
		 * We should only receive ISAKMP mode_cfg SET XAUTH_STATUS
		 * when running as a client (initiator).
		 */
		xst = &iph1->mode_cfg->xauth;
		switch(AUTHMETHOD(iph1)) {
        case OAKLEY_ATTR_AUTH_METHOD_XAUTH_PSKEY_R:
            if (!iph1->is_rekey) {
                IPSECSESSIONTRACEREVENT(iph1->parent_session,
                                        IPSECSESSIONEVENTCODE_IKEV1_XAUTH_DROP,
                                        CONSTSTR("Unexpected XAUTH Status"),
                                        CONSTSTR("Xauth dropped (unexpected Xauth status)... not a Phase 1 rekey"));
                plog(ASL_LEVEL_ERR, 
                     "Unexpected XAUTH_STATUS_OK... not a Phase 1 rekey\n");
                return NULL;
            }
		case OAKLEY_ATTR_AUTH_METHOD_HYBRID_RSA_I:
		case FICTIVE_AUTH_METHOD_XAUTH_PSKEY_I:
		case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSASIG_I:
		/* Not implemented ... */
		case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAENC_I:
		case OAKLEY_ATTR_AUTH_METHOD_XAUTH_RSAREV_I:
			break;
		default:
			IPSECSESSIONTRACEREVENT(iph1->parent_session,
									IPSECSESSIONEVENTCODE_IKEV1_XAUTH_DROP,
									CONSTSTR("Unexpected XAUTH Status"),
									CONSTSTR("Xauth dropped (unexpected Xauth status)"));
			plog(ASL_LEVEL_ERR, 
			    "Unexpected XAUTH_STATUS_OK\n");
			return NULL;
			break;
		}

		/* If we got a failure, delete iph1 */
		if (ntohs(attr->lorv) != XAUTH_STATUS_OK) {
			IPSECSESSIONTRACEREVENT(iph1->parent_session,
									IPSECSESSIONEVENTCODE_IKEV1_XAUTH_FAIL,
									CONSTSTR("XAUTH Status is not OK"),
									CONSTSTR("Xauth Failed (status not ok)"));
			plog(ASL_LEVEL_ERR, 
			    "Xauth authentication failed\n");
				
			vpncontrol_notify_ike_failed(VPNCTL_NTYPE_AUTHENTICATION_FAILED, FROM_LOCAL,
				((struct sockaddr_in*)iph1->remote)->sin_addr.s_addr, 0, NULL);

			iph1->mode_cfg->flags |= ISAKMP_CFG_DELETE_PH1;

			IPSECLOGASLMSG("IPSec Extended Authentication Failed.\n");
		} else {
			IPSECSESSIONTRACEREVENT(iph1->parent_session,
									IPSECSESSIONEVENTCODE_IKEV1_XAUTH_SUCC,
									CONSTSTR("XAUTH Status is OK"),
									CONSTSTR(NULL));
            if (iph1->is_rekey) {
                xst->status = XAUTHST_OK;
            }

			IPSECLOGASLMSG("IPSec Extended Authentication Passed.\n");
		}


		/* We acknowledge it */
		break;
	case XAUTH_MESSAGE:
		if ((ntohs(attr->type) & ISAKMP_GEN_TV) == 0) {
			dlen = ntohs(attr->lorv);
			if (dlen > 0) {
				mraw = (char*)(attr + 1);
				if ((mdata = vmalloc(dlen)) == NULL) {
					plog(ASL_LEVEL_ERR,
					    "Cannot allocate memory\n");
					return NULL;
				}
				memcpy(mdata->v, mraw, mdata->l);
				plog(ASL_LEVEL_NOTICE, "XAUTH Message: '%s'.\n",
					binsanitize(mdata->v, mdata->l));
				vfree(mdata);
			}
		}

	default:
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_XAUTH_DROP,
								CONSTSTR("ignored attribute"),
								CONSTSTR("Xauth dropped (ignored attribute)"));
		plog(ASL_LEVEL_WARNING, 
		    "Ignored attribute %s\n", s_isakmp_cfg_type(type));
		return NULL;
		break;
	}

	if ((buffer = vmalloc(sizeof(*attr))) == NULL) {
		IPSECSESSIONTRACEREVENT(iph1->parent_session,
								IPSECSESSIONEVENTCODE_IKEV1_XAUTH_DROP,
								CONSTSTR("Failed to allocate attribute"),
								CONSTSTR("Xauth dropped (failed to allocate attribute)"));
		plog(ASL_LEVEL_ERR, 
		    "Cannot allocate memory\n");
		return NULL;
	}

	attr = (struct isakmp_data *)buffer->v;
	attr->type = htons(type | ISAKMP_GEN_TV);
	attr->lorv = htons(0);

	return buffer;
}


void 
xauth_rmstate(xst)
	struct xauth_state *xst;
{
	switch (xst->authtype) {
	case XAUTH_TYPE_GENERIC:
		if (xst->authdata.generic.usr)
			racoon_free(xst->authdata.generic.usr);

		if (xst->authdata.generic.pwd)
			racoon_free(xst->authdata.generic.pwd);

		break;

	case XAUTH_TYPE_CHAP:
	case XAUTH_TYPE_OTP:
	case XAUTH_TYPE_SKEY:
		plog(ASL_LEVEL_WARNING, 
		    "Unsupported authtype %d\n", xst->authtype);
		break;

	default:
		plog(ASL_LEVEL_WARNING, 
		    "Unexpected authtype %d\n", xst->authtype);
		break;
	}

	return;
}

int
xauth_rmconf_used(xauth_rmconf)
	struct xauth_rmconf **xauth_rmconf;
{
	if (*xauth_rmconf == NULL) {
		*xauth_rmconf = racoon_malloc(sizeof(**xauth_rmconf));
		if (*xauth_rmconf == NULL) {
			plog(ASL_LEVEL_ERR, 
			    "xauth_rmconf_used: malloc failed\n");
			return -1;
		}

		(*xauth_rmconf)->login = NULL;
		(*xauth_rmconf)->pass = NULL;
		(*xauth_rmconf)->state = 0;
	} else {
		if ((*xauth_rmconf)->login) {
			vfree((*xauth_rmconf)->login);
			(*xauth_rmconf)->login = NULL;
		}
		if ((*xauth_rmconf)->pass != NULL) {
			vfree((*xauth_rmconf)->pass);
			(*xauth_rmconf)->pass = NULL;
		}
		(*xauth_rmconf)->state = 0;
	}

	return 0;
}

void 
xauth_rmconf_delete(xauth_rmconf)
	struct xauth_rmconf **xauth_rmconf;
{
	if (*xauth_rmconf != NULL) { 
		if ((*xauth_rmconf)->login != NULL)
			vfree((*xauth_rmconf)->login);
		if ((*xauth_rmconf)->pass != NULL)
			vfree((*xauth_rmconf)->pass);

		racoon_free(*xauth_rmconf);
		*xauth_rmconf = NULL;
	}

	return;
}
