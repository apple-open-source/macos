/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */



#include <mach/mach_types.h>
#include <machine/spl.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <netinet/ip_fw.h>
#include <netinet6/ip6_fw.h>
#include <string.h>

#include "OldInterface.h"


extern LIST_HEAD (ip_fw_head, ip_fw_chain) ip_fw_chain_head;
extern LIST_HEAD (ip6_fw_head, ip6_fw_chain) ip6_fw_chain;
extern void remove_dyn_rule(struct ip_fw_chain *chain, int force);
extern struct sysctl_oid sysctl__net_inet_ip_fw;
extern struct sysctl_oid sysctl__net_inet_ip_fw_enable;
extern struct sysctl_oid sysctl__net_inet_ip_fw_one_pass;
extern struct sysctl_oid sysctl__net_inet_ip_fw_debug;
extern struct sysctl_oid sysctl__net_inet_ip_fw_verbose;
extern struct sysctl_oid sysctl__net_inet_ip_fw_verbose_limit;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_buckets;
extern struct sysctl_oid sysctl__net_inet_ip_fw_curr_dyn_buckets;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_count;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_max;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_ack_lifetime;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_syn_lifetime;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_fin_lifetime;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_rst_lifetime;
extern struct sysctl_oid sysctl__net_inet_ip_fw_dyn_short_lifetime;
extern struct sysctl_oid sysctl__net_inet6_ip6_fw;
extern struct sysctl_oid sysctl__net_inet6_ip6_fw_enable;
extern struct sysctl_oid sysctl__net_inet6_ip6_fw_debug;
extern struct sysctl_oid sysctl__net_inet6_ip6_fw_verbose;
extern struct sysctl_oid sysctl__net_inet6_ip6_fw_verbose_limit;


static ip_fw_chk_t  *old_chk_ptr;
static ip_fw_ctl_t  *old_ctl_ptr;
static ip6_fw_chk_t *old_chk_ptr6;
static ip6_fw_ctl_t *old_ctl_ptr6;



/* Called when the firewall is loaded.
 */
kern_return_t IPFirewall_start(struct kmod_info *ki, void *data)
{
	int s;
	int funnel_state;

	funnel_state = thread_funnel_set(network_flock, TRUE);
	s = splnet();

	sysctl_register_oid(&sysctl__net_inet_ip_fw);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_enable);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_one_pass);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_debug);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_verbose);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_verbose_limit);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_buckets);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_curr_dyn_buckets);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_count);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_max);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_ack_lifetime);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_syn_lifetime);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_fin_lifetime);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_rst_lifetime);
	sysctl_register_oid(&sysctl__net_inet_ip_fw_dyn_short_lifetime);
	sysctl_register_oid(&sysctl__net_inet6_ip6_fw);
	sysctl_register_oid(&sysctl__net_inet6_ip6_fw_enable);
	sysctl_register_oid(&sysctl__net_inet6_ip6_fw_debug);
	sysctl_register_oid(&sysctl__net_inet6_ip6_fw_verbose);
	sysctl_register_oid(&sysctl__net_inet6_ip6_fw_verbose_limit);

	old_chk_ptr  = ip_fw_chk_ptr;
	old_ctl_ptr  = ip_fw_ctl_ptr;
	old_chk_ptr6 = ip6_fw_chk_ptr;
	old_ctl_ptr6 = ip6_fw_ctl_ptr;

	ip_fw_init();
	ip6_fw_init();

	splx(s);
	thread_funnel_set(network_flock, funnel_state);
        wakeup(&ip_fw_ctl_ptr);

	printf("IP firewall loaded\n");

	return KERN_SUCCESS;
}


/* Called when the firewall is unloaded.
 */
kern_return_t IPFirewall_stop(struct kmod_info *ki, void *data)
{
	int s;
	int funnel_state;
	struct ip_fw_chain *fcp;

	funnel_state = thread_funnel_set(network_flock, TRUE);
	s = splnet();

	ip_fw_chk_ptr  = old_chk_ptr;
	ip_fw_ctl_ptr  = old_ctl_ptr;
	ip6_fw_chk_ptr = old_chk_ptr6;
	ip6_fw_ctl_ptr = old_ctl_ptr6;
	remove_dyn_rule(NULL, 1 /* force delete */);

	while ( (fcp = LIST_FIRST(&ip_fw_chain_head)) != NULL) {
		LIST_REMOVE(fcp, next);
		#ifdef DUMMYNET
		dn_rule_delete(fcp);
		#endif
		FREE(fcp->rule, M_IPFW);
		FREE(fcp, M_IPFW);
	}

	while (LIST_FIRST(&ip6_fw_chain) != NULL) {
		struct ip6_fw_chain *fcp = LIST_FIRST(&ip6_fw_chain);
		LIST_REMOVE(LIST_FIRST(&ip6_fw_chain), chain);
		FREE(fcp->rule, M_IP6FW);
		FREE(fcp, M_IP6FW);
	}

	sysctl_unregister_oid(&sysctl__net_inet_ip_fw);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_enable);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_one_pass);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_debug);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_verbose);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_verbose_limit);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_buckets);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_curr_dyn_buckets);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_count);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_max);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_ack_lifetime);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_syn_lifetime);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_fin_lifetime);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_rst_lifetime);
	sysctl_unregister_oid(&sysctl__net_inet_ip_fw_dyn_short_lifetime);
	sysctl_unregister_oid(&sysctl__net_inet6_ip6_fw);
	sysctl_unregister_oid(&sysctl__net_inet6_ip6_fw_enable);
	sysctl_unregister_oid(&sysctl__net_inet6_ip6_fw_debug);
	sysctl_unregister_oid(&sysctl__net_inet6_ip6_fw_verbose);
	sysctl_unregister_oid(&sysctl__net_inet6_ip6_fw_verbose_limit);

	splx(s);
	thread_funnel_set(network_flock, funnel_state);

	printf("IP firewall unloaded\n");

	return KERN_SUCCESS;
}


/* Adapts the rule structure from clients that were built for the old ipfirewall
 * interface.  Marshalls parameters from the old-style structure into a new-style
 * structure, which can then be used by the new firewall code.
 */
void ConvertOldInterfaceToNew(struct ip_old_fw *oldStyleRule, struct ip_fw *newStyleRule)
{
	memset(newStyleRule, 0, sizeof(*newStyleRule));
	memcpy(&newStyleRule->fw_uar, &oldStyleRule->fw_uar, sizeof(oldStyleRule->fw_uar));
	memcpy(&newStyleRule->fw_in_if, &oldStyleRule->fw_in_if, sizeof(oldStyleRule->fw_in_if));
	memcpy(&newStyleRule->fw_out_if, &oldStyleRule->fw_out_if, sizeof(oldStyleRule->fw_out_if));
	memcpy(&newStyleRule->fw_un, &oldStyleRule->fw_un, sizeof(oldStyleRule->fw_un));

	newStyleRule->version       = 10;
	newStyleRule->fw_pcnt       = oldStyleRule->fw_pcnt;
	newStyleRule->fw_bcnt       = oldStyleRule->fw_bcnt;
	newStyleRule->fw_src        = oldStyleRule->fw_src;
	newStyleRule->fw_dst        = oldStyleRule->fw_dst;
	newStyleRule->fw_smsk       = oldStyleRule->fw_smsk;
	newStyleRule->fw_dmsk       = oldStyleRule->fw_dmsk;
	newStyleRule->fw_number     = oldStyleRule->fw_number;
	newStyleRule->fw_flg        = oldStyleRule->fw_flg;
	newStyleRule->fw_ipopt      = oldStyleRule->fw_ipopt;
	newStyleRule->fw_ipnopt     = oldStyleRule->fw_ipnopt;
	newStyleRule->fw_tcpf       = oldStyleRule->fw_tcpf & ~IP_OLD_FW_TCPF_ESTAB;
	newStyleRule->fw_tcpnf      = oldStyleRule->fw_tcpnf;
	newStyleRule->timestamp     = oldStyleRule->timestamp;
	newStyleRule->fw_prot       = oldStyleRule->fw_prot;
	newStyleRule->fw_nports     = oldStyleRule->fw_nports;
	newStyleRule->pipe_ptr      = oldStyleRule->pipe_ptr;
	newStyleRule->next_rule_ptr = oldStyleRule->next_rule_ptr;
	newStyleRule->fw_ipflg      = (oldStyleRule->fw_tcpf & IP_OLD_FW_TCPF_ESTAB) ? IP_FW_IF_TCPEST : 0;
}


/* Anti-particle of ConvertOldInterfaceToNew.  Converts back to the old interface
 * for operations that return information to clients.
 */
void ConvertNewInterfaceToOld(struct ip_fw *newStyleRule, struct ip_old_fw *oldStyleRule)
{
	memset(oldStyleRule, 0, sizeof(*oldStyleRule));
	memcpy(&oldStyleRule->fw_uar, &newStyleRule->fw_uar, sizeof(newStyleRule->fw_uar));
	memcpy(&oldStyleRule->fw_in_if, &newStyleRule->fw_in_if, sizeof(newStyleRule->fw_in_if));
	memcpy(&oldStyleRule->fw_out_if, &newStyleRule->fw_out_if, sizeof(newStyleRule->fw_out_if));
	memcpy(&oldStyleRule->fw_un, &newStyleRule->fw_un, sizeof(newStyleRule->fw_un));

	oldStyleRule->fw_pcnt       = newStyleRule->fw_pcnt;
	oldStyleRule->fw_bcnt       = newStyleRule->fw_bcnt;
	oldStyleRule->fw_src        = newStyleRule->fw_src;
	oldStyleRule->fw_dst        = newStyleRule->fw_dst;
	oldStyleRule->fw_smsk       = newStyleRule->fw_smsk;
	oldStyleRule->fw_dmsk       = newStyleRule->fw_dmsk;
	oldStyleRule->fw_number     = newStyleRule->fw_number;
	oldStyleRule->fw_flg        = newStyleRule->fw_flg;
	oldStyleRule->fw_ipopt      = newStyleRule->fw_ipopt;
	oldStyleRule->fw_ipnopt     = newStyleRule->fw_ipnopt;
	oldStyleRule->fw_tcpf       = newStyleRule->fw_tcpf;
	oldStyleRule->fw_tcpnf      = newStyleRule->fw_tcpnf;
	oldStyleRule->timestamp     = newStyleRule->timestamp;
	oldStyleRule->fw_prot       = newStyleRule->fw_prot;
	oldStyleRule->fw_nports     = newStyleRule->fw_nports;
	oldStyleRule->pipe_ptr      = newStyleRule->pipe_ptr;
	oldStyleRule->next_rule_ptr = newStyleRule->next_rule_ptr;

	if (newStyleRule->fw_ipflg && IP_FW_IF_TCPEST) oldStyleRule->fw_tcpf |= IP_OLD_FW_TCPF_ESTAB;
}


/* Copies in the client's rule structure, converts it from the old to the new style
 * if necessary, and checks to make sure that we support its API version.  Returns the
 * firewall API version in outVersion and the firewall API command in outCommand.
 */
int CopyInRule(struct sockopt *sopt, struct ip_fw *newStyleRule, u_int32_t *outVersion, int *outCommand)
{
	int error  = 0;
	int oldAPI;
	int command;

	if      (sopt->sopt_name == IP_OLD_FW_GET)       { oldAPI = 1;  command = IP_FW_GET;       }
	else if (sopt->sopt_name == IP_OLD_FW_FLUSH)     { oldAPI = 1;  command = IP_FW_FLUSH;     }
	else if (sopt->sopt_name == IP_OLD_FW_ZERO)      { oldAPI = 1;  command = IP_FW_ZERO;      }
	else if (sopt->sopt_name == IP_OLD_FW_ADD)       { oldAPI = 1;  command = IP_FW_ADD;       }
	else if (sopt->sopt_name == IP_OLD_FW_DEL)       { oldAPI = 1;  command = IP_FW_DEL;       }
	else if (sopt->sopt_name == IP_OLD_FW_RESETLOG)  { oldAPI = 1;  command = IP_FW_RESETLOG;  }
	else                                             { oldAPI = 0;  command = sopt->sopt_name; }

	if (oldAPI)
	{
		if (command == IP_FW_GET || command == IP_FW_FLUSH || sopt->sopt_val == NULL)
		{
			/* In the old-style API, it was legal to not pass in a rule structure for certain firewall
			 * operations (e.g. flush, reset log).  If that's the situation, we pretend we received a
			 * blank structure. */
			memset(newStyleRule, 0, sizeof *newStyleRule);
			newStyleRule->version = 10;
		}
		else
		{
			struct ip_old_fw oldStyleRule;

			if (!sopt->sopt_val || sopt->sopt_valsize < sizeof oldStyleRule) return EINVAL;
	
			if (sopt->sopt_p != 0)
			{
				error = copyin(sopt->sopt_val, &oldStyleRule, sizeof oldStyleRule);
				if (error) return error;
			}
			else memcpy(&oldStyleRule, sopt->sopt_val, sizeof oldStyleRule);

			ConvertOldInterfaceToNew(&oldStyleRule, newStyleRule);
		}
	}
	else
	{
		/* We ALWAYS expect the client to pass in a rule structure so that we can check the
		 * version of the API that they are using.  In the case of a IP_FW_GET operation, the
		 * first rule of the output buffer passed to us must have the version set. */
		if (!sopt->sopt_val || sopt->sopt_valsize < sizeof *newStyleRule) return EINVAL;

		if (sopt->sopt_p != 0)
		{
			error = copyin(sopt->sopt_val, newStyleRule, sizeof *newStyleRule);
			if (error) return error;
		}
		else memcpy(newStyleRule, sopt->sopt_val, sizeof *newStyleRule);

		if (newStyleRule->version != IP_FW_CURRENT_API_VERSION) return EINVAL;
	}

	*outCommand = command;
	*outVersion = newStyleRule->version;
	newStyleRule->version = 0xFFFFFFFF;		/* version is meaningless once rules "make it in the door". */

	return error;
}


