/*
 * Copyright (c) 2009-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/filedesc.h>
#include <sys/file_internal.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/errno.h>
#include <sys/protosw.h>
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_cc.h>
#include <netinet/in_tclass.h>

#include <os/log.h>

static_assert(_SO_TC_MAX == SO_TC_STATS_MAX);

struct net_qos_dscp_map {
	uint8_t        sotc_to_dscp[SO_TC_MAX];
	uint8_t        netsvctype_to_dscp[_NET_SERVICE_TYPE_COUNT];
};

struct dcsp_msc_map {
	uint8_t                 dscp;
	mbuf_svc_class_t        msc;
};
static inline int so_throttle_best_effort(struct socket *, struct ifnet *);
static void set_dscp_to_wifi_ac_map(const struct dcsp_msc_map *, int);
static errno_t dscp_msc_map_from_netsvctype_dscp_map(struct netsvctype_dscp_map *, size_t,
    struct dcsp_msc_map *);

static LCK_GRP_DECLARE(tclass_lck_grp, "tclass");
static LCK_MTX_DECLARE(tclass_lock, &tclass_lck_grp);

SYSCTL_NODE(_net, OID_AUTO, qos,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "QoS");

static int sysctl_default_netsvctype_to_dscp_map SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_net_qos, OID_AUTO, default_netsvctype_to_dscp_map,
    CTLTYPE_STRUCT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_default_netsvctype_to_dscp_map, "S", "");

static int sysctl_dscp_to_wifi_ac_map SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_net_qos, OID_AUTO, dscp_to_wifi_ac_map,
    CTLTYPE_STRUCT | CTLFLAG_RW | CTLFLAG_LOCKED,
    0, 0, sysctl_dscp_to_wifi_ac_map, "S", "");

static int sysctl_reset_dscp_to_wifi_ac_map SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_net_qos, OID_AUTO, reset_dscp_to_wifi_ac_map,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, 0, sysctl_reset_dscp_to_wifi_ac_map, "I", "");

int net_qos_verbose = 0;
SYSCTL_INT(_net_qos, OID_AUTO, verbose,
    CTLFLAG_RW | CTLFLAG_LOCKED, &net_qos_verbose, 0, "");

/*
 * Fastlane QoS policy:
 * By Default allow all apps to get traffic class to DSCP mapping
 */
SYSCTL_NODE(_net_qos, OID_AUTO, policy,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "");

int net_qos_policy_restricted = 0;
SYSCTL_INT(_net_qos_policy, OID_AUTO, restricted,
    CTLFLAG_RW | CTLFLAG_LOCKED, &net_qos_policy_restricted, 0, "");

int net_qos_policy_restrict_avapps = 0;
SYSCTL_INT(_net_qos_policy, OID_AUTO, restrict_avapps,
    CTLFLAG_RW | CTLFLAG_LOCKED, &net_qos_policy_restrict_avapps, 0, "");

int net_qos_policy_wifi_enabled = 0;
SYSCTL_INT(_net_qos_policy, OID_AUTO, wifi_enabled,
    CTLFLAG_RW | CTLFLAG_LOCKED, &net_qos_policy_wifi_enabled, 0, "");

int net_qos_policy_capable_enabled = 0;
SYSCTL_INT(_net_qos_policy, OID_AUTO, capable_enabled,
    CTLFLAG_RW | CTLFLAG_LOCKED, &net_qos_policy_capable_enabled, 0, "");

/*
 * Socket traffic class from network service type
 */
const int sotc_by_netservicetype[_NET_SERVICE_TYPE_COUNT] = {
	SO_TC_BE,       /* NET_SERVICE_TYPE_BE */
	SO_TC_BK,       /* NET_SERVICE_TYPE_BK */
	SO_TC_VI,       /* NET_SERVICE_TYPE_SIG */
	SO_TC_VI,       /* NET_SERVICE_TYPE_VI */
	SO_TC_VO,       /* NET_SERVICE_TYPE_VO */
	SO_TC_RV,       /* NET_SERVICE_TYPE_RV */
	SO_TC_AV,       /* NET_SERVICE_TYPE_AV */
	SO_TC_OAM,      /* NET_SERVICE_TYPE_OAM */
	SO_TC_RD        /* NET_SERVICE_TYPE_RD */
};

/*
 * DSCP mappings for QoS Fastlane as based on network service types
 */
static const
struct netsvctype_dscp_map fastlane_netsvctype_dscp_map[_NET_SERVICE_TYPE_COUNT] = {
	{ .netsvctype = NET_SERVICE_TYPE_BE, .dscp = _DSCP_DF },
	{ .netsvctype = NET_SERVICE_TYPE_BK, .dscp = _DSCP_AF11 },
	{ .netsvctype = NET_SERVICE_TYPE_SIG, .dscp = _DSCP_CS3 },
	{ .netsvctype = NET_SERVICE_TYPE_VI, .dscp = _DSCP_AF41 },
	{ .netsvctype = NET_SERVICE_TYPE_VO, .dscp = _DSCP_EF },
	{ .netsvctype = NET_SERVICE_TYPE_RV, .dscp = _DSCP_CS4 },
	{ .netsvctype = NET_SERVICE_TYPE_AV, .dscp = _DSCP_AF31 },
	{ .netsvctype = NET_SERVICE_TYPE_OAM, .dscp = _DSCP_CS2 },
	{ .netsvctype = NET_SERVICE_TYPE_RD, .dscp = _DSCP_AF21 },
};

/*
 * DSCP mappings for QoS RFC4594 as based on network service types
 */
static const
struct netsvctype_dscp_map rfc4594_netsvctype_dscp_map[_NET_SERVICE_TYPE_COUNT] = {
	{ .netsvctype = NET_SERVICE_TYPE_BE, .dscp = _DSCP_DF },
	{ .netsvctype = NET_SERVICE_TYPE_BK, .dscp = _DSCP_CS1 },
	{ .netsvctype = NET_SERVICE_TYPE_SIG, .dscp = _DSCP_CS5 },
	{ .netsvctype = NET_SERVICE_TYPE_VI, .dscp = _DSCP_AF41 },
	{ .netsvctype = NET_SERVICE_TYPE_VO, .dscp = _DSCP_EF },
	{ .netsvctype = NET_SERVICE_TYPE_RV, .dscp = _DSCP_CS4 },
	{ .netsvctype = NET_SERVICE_TYPE_AV, .dscp = _DSCP_AF31 },
	{ .netsvctype = NET_SERVICE_TYPE_OAM, .dscp = _DSCP_CS2 },
	{ .netsvctype = NET_SERVICE_TYPE_RD, .dscp = _DSCP_AF21 },
};

static struct net_qos_dscp_map fastlane_net_qos_dscp_map;
static struct net_qos_dscp_map rfc4594_net_qos_dscp_map;
#if (DEBUG || DEVELOPMENT)
static struct net_qos_dscp_map custom_net_qos_dscp_map;
#endif /* (DEBUG || DEVELOPMENT) */

/*
 * The size is one more than the max because DSCP start at zero
 */
#define DSCP_ARRAY_SIZE (_MAX_DSCP + 1)

/*
 * The DSCP to UP mapping (via mbuf service class) for WiFi follows is the mapping
 * that implemented at the 802.11 driver level when the mbuf service class is
 * MBUF_SC_BE.
 *
 * This clashes with the recommended mapping documented by the IETF document
 * draft-szigeti-tsvwg-ieee-802-11e-01.txt but we keep the mapping to maintain
 * binary compatibility. Applications should use the network service type socket
 * option instead to select L2 QoS marking instead of IP_TOS or IPV6_TCLASS.
 */
static const struct dcsp_msc_map default_dscp_to_wifi_ac_map[] = {
	{ .dscp = _DSCP_DF, .msc = MBUF_SC_BE },        /* RFC 2474 Standard */
	{ .dscp = 1, .msc = MBUF_SC_BE },               /*  */
	{ .dscp = 2, .msc = MBUF_SC_BE },               /*  */
	{ .dscp = 3, .msc = MBUF_SC_BE },               /*  */
	{ .dscp = 4, .msc = MBUF_SC_BE },               /*  */
	{ .dscp = 5, .msc = MBUF_SC_BE },               /*  */
	{ .dscp = 6, .msc = MBUF_SC_BE },               /*  */
	{ .dscp = 7, .msc = MBUF_SC_BE },               /*  */

	{ .dscp = _DSCP_CS1, .msc = MBUF_SC_BK },       /* RFC 3662 Low-Priority Data */
	{ .dscp = 9, .msc = MBUF_SC_BK },               /*  */
	{ .dscp = _DSCP_AF11, .msc = MBUF_SC_BK },      /* RFC 2597 High-Throughput Data */
	{ .dscp = 11, .msc = MBUF_SC_BK },              /*  */
	{ .dscp = _DSCP_AF12, .msc = MBUF_SC_BK },      /* RFC 2597 High-Throughput Data */
	{ .dscp = 13, .msc = MBUF_SC_BK },              /*  */
	{ .dscp = _DSCP_AF13, .msc = MBUF_SC_BK },      /* RFC 2597 High-Throughput Data */
	{ .dscp = 15, .msc = MBUF_SC_BK },              /*  */

	{ .dscp = _DSCP_CS2, .msc = MBUF_SC_BK },       /* RFC 4594 OAM */
	{ .dscp = 17, .msc = MBUF_SC_BK },              /*  */
	{ .dscp = _DSCP_AF21, .msc = MBUF_SC_BK },      /* RFC 2597 Low-Latency Data */
	{ .dscp = 19, .msc = MBUF_SC_BK },              /*  */
	{ .dscp = _DSCP_AF22, .msc = MBUF_SC_BK },      /* RFC 2597 Low-Latency Data */
	{ .dscp = 21, .msc = MBUF_SC_BK },              /*  */
	{ .dscp = _DSCP_AF23, .msc = MBUF_SC_BK },      /* RFC 2597 Low-Latency Data */
	{ .dscp = 23, .msc = MBUF_SC_BK },              /*  */

	{ .dscp = _DSCP_CS3, .msc = MBUF_SC_BE },       /* RFC 2474 Broadcast Video */
	{ .dscp = 25, .msc = MBUF_SC_BE },              /*  */
	{ .dscp = _DSCP_AF31, .msc = MBUF_SC_BE },      /* RFC 2597 Multimedia Streaming */
	{ .dscp = 27, .msc = MBUF_SC_BE },              /*  */
	{ .dscp = _DSCP_AF32, .msc = MBUF_SC_BE },      /* RFC 2597 Multimedia Streaming */
	{ .dscp = 29, .msc = MBUF_SC_BE },              /*  */
	{ .dscp = _DSCP_AF33, .msc = MBUF_SC_BE },      /* RFC 2597 Multimedia Streaming */
	{ .dscp = 31, .msc = MBUF_SC_BE },              /*  */

	{ .dscp = _DSCP_CS4, .msc = MBUF_SC_VI },       /* RFC 2474 Real-Time Interactive */
	{ .dscp = 33, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = _DSCP_AF41, .msc = MBUF_SC_VI },      /* RFC 2597 Multimedia Conferencing */
	{ .dscp = 35, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = _DSCP_AF42, .msc = MBUF_SC_VI },      /* RFC 2597 Multimedia Conferencing */
	{ .dscp = 37, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = _DSCP_AF43, .msc = MBUF_SC_VI },      /* RFC 2597 Multimedia Conferencing */
	{ .dscp = 39, .msc = MBUF_SC_VI },              /*  */

	{ .dscp = _DSCP_CS5, .msc = MBUF_SC_VI },       /* RFC 2474 Signaling */
	{ .dscp = 41, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = 42, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = 43, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = _DSCP_VA, .msc = MBUF_SC_VI },        /* RFC 5865 VOICE-ADMIT */
	{ .dscp = 45, .msc = MBUF_SC_VI },              /*  */
	{ .dscp = _DSCP_EF, .msc = MBUF_SC_VI },        /* RFC 3246 Telephony */
	{ .dscp = 47, .msc = MBUF_SC_VI },              /*  */

	{ .dscp = _DSCP_CS6, .msc = MBUF_SC_VO },       /* Wi-Fi WMM Certification: Chariot */
	{ .dscp = 49, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 50, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 51, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 52, .msc = MBUF_SC_VO },              /* Wi-Fi WMM Certification: Sigma */
	{ .dscp = 53, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 54, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 55, .msc = MBUF_SC_VO },              /*  */

	{ .dscp = _DSCP_CS7, .msc = MBUF_SC_VO },       /* Wi-Fi WMM Certification: Chariot */
	{ .dscp = 57, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 58, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 59, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 60, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 61, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 62, .msc = MBUF_SC_VO },              /*  */
	{ .dscp = 63, .msc = MBUF_SC_VO },              /*  */

	{ .dscp = 255, .msc = MBUF_SC_UNSPEC }          /* invalid DSCP to mark last entry */
};

mbuf_svc_class_t wifi_dscp_to_msc_array[DSCP_ARRAY_SIZE];

/*
 * If there is no foreground activity on the interface for bg_switch_time
 * seconds, the background connections can switch to foreground TCP
 * congestion control.
 */
#define TCP_BG_SWITCH_TIME 2 /* seconds */

#if (DEVELOPMENT || DEBUG)

static int tfp_count = 0;

static TAILQ_HEAD(, tclass_for_proc) tfp_head =
    TAILQ_HEAD_INITIALIZER(tfp_head);

struct tclass_for_proc {
	TAILQ_ENTRY(tclass_for_proc)    tfp_link;
	int             tfp_class;
	pid_t           tfp_pid;
	char            tfp_pname[(2 * MAXCOMLEN) + 1];
	uint32_t        tfp_qos_mode;
};

static int get_pid_tclass(struct so_tcdbg *);
static int get_pname_tclass(struct so_tcdbg *);
static int set_pid_tclass(struct so_tcdbg *);
static int set_pname_tclass(struct so_tcdbg *);
static int flush_pid_tclass(struct so_tcdbg *);
static int purge_tclass_for_proc(void);
static int flush_tclass_for_proc(void);
static void set_tclass_for_curr_proc(struct socket *);

/*
 * Must be called with tclass_lock held
 */
static struct tclass_for_proc *
find_tfp_by_pid(pid_t pid)
{
	struct tclass_for_proc *tfp;

	TAILQ_FOREACH(tfp, &tfp_head, tfp_link) {
		if (tfp->tfp_pid == pid) {
			break;
		}
	}
	return tfp;
}

/*
 * Must be called with tclass_lock held
 */
static struct tclass_for_proc *
find_tfp_by_pname(const char *pname)
{
	struct tclass_for_proc *tfp;

	TAILQ_FOREACH(tfp, &tfp_head, tfp_link) {
		if (strncmp(pname, tfp->tfp_pname,
		    sizeof(tfp->tfp_pname)) == 0) {
			break;
		}
	}
	return tfp;
}

__private_extern__ void
set_tclass_for_curr_proc(struct socket *so)
{
	struct tclass_for_proc *tfp = NULL;
	proc_t p = current_proc();      /* Not ref counted */
	pid_t pid = proc_pid(p);
	char *pname = proc_best_name(p);

	lck_mtx_lock(&tclass_lock);

	TAILQ_FOREACH(tfp, &tfp_head, tfp_link) {
		if ((tfp->tfp_pid == pid) || (tfp->tfp_pid == -1 &&
		    strncmp(pname, tfp->tfp_pname,
		    sizeof(tfp->tfp_pname)) == 0)) {
			if (tfp->tfp_class != SO_TC_UNSPEC) {
				so->so_traffic_class = (uint16_t)tfp->tfp_class;
			}

			if (tfp->tfp_qos_mode == QOS_MODE_MARKING_POLICY_ENABLE) {
				so->so_flags1 |= SOF1_QOSMARKING_ALLOWED;
			} else if (tfp->tfp_qos_mode == QOS_MODE_MARKING_POLICY_DISABLE) {
				so->so_flags1 &= ~SOF1_QOSMARKING_ALLOWED;
			}
			break;
		}
	}

	lck_mtx_unlock(&tclass_lock);
}

/*
 * Purge entries with PIDs of exited processes
 */
int
purge_tclass_for_proc(void)
{
	int error = 0;
	struct tclass_for_proc *tfp, *tvar;

	lck_mtx_lock(&tclass_lock);

	TAILQ_FOREACH_SAFE(tfp, &tfp_head, tfp_link, tvar) {
		proc_t p;

		if (tfp->tfp_pid == -1) {
			continue;
		}
		if ((p = proc_find(tfp->tfp_pid)) == NULL) {
			tfp_count--;
			TAILQ_REMOVE(&tfp_head, tfp, tfp_link);

			kfree_type(struct tclass_for_proc, tfp);
		} else {
			proc_rele(p);
		}
	}

	lck_mtx_unlock(&tclass_lock);

	return error;
}

/*
 * Remove one entry
 * Must be called with tclass_lock held
 */
static void
free_tclass_for_proc(struct tclass_for_proc *tfp)
{
	if (tfp == NULL) {
		return;
	}
	tfp_count--;
	TAILQ_REMOVE(&tfp_head, tfp, tfp_link);
	kfree_type(struct tclass_for_proc, tfp);
}

/*
 * Remove all entries
 */
int
flush_tclass_for_proc(void)
{
	int error = 0;
	struct tclass_for_proc *tfp, *tvar;

	lck_mtx_lock(&tclass_lock);

	TAILQ_FOREACH_SAFE(tfp, &tfp_head, tfp_link, tvar) {
		free_tclass_for_proc(tfp);
	}

	lck_mtx_unlock(&tclass_lock);

	return error;
}

/*
 * Must be called with tclass_lock held
 */
static struct tclass_for_proc *
alloc_tclass_for_proc(pid_t pid, const char *pname, size_t pnamelen)
{
	struct tclass_for_proc *tfp;

	if (pid == -1 && pname == NULL) {
		return NULL;
	}

	tfp = kalloc_type(struct tclass_for_proc, Z_NOWAIT | Z_ZERO);
	if (tfp == NULL) {
		return NULL;
	}

	tfp->tfp_pid = pid;
	/*
	 * Add per pid entries before per proc name so we can find
	 * a specific instance of a process before the general name base entry.
	 */
	if (pid != -1) {
		TAILQ_INSERT_HEAD(&tfp_head, tfp, tfp_link);
	} else {
		if (pname != NULL) {
			strncpy(tfp->tfp_pname, pname, pnamelen);
			tfp->tfp_pname[sizeof(tfp->tfp_pname) - 1] = '\0';
		} else {
			tfp->tfp_pname[0] = '\0';
		}
		TAILQ_INSERT_TAIL(&tfp_head, tfp, tfp_link);
	}

	tfp_count++;

	return tfp;
}

/*
 * SO_TC_UNSPEC for tclass means to remove the entry
 */
int
set_pid_tclass(struct so_tcdbg *so_tcdbg)
{
	int error = EINVAL;
	proc_t p = NULL;
	struct tclass_for_proc *tfp;
	pid_t pid = so_tcdbg->so_tcdbg_pid;
	int tclass = so_tcdbg->so_tcdbg_tclass;
	int netsvctype = so_tcdbg->so_tcdbg_netsvctype;
	uint8_t ecn_val = so_tcdbg->so_tcdbg_ecn_val;

	p = proc_find(pid);
	if (p == NULL) {
		printf("%s proc_find(%d) failed\n", __func__, pid);
		goto done;
	}

	/* Need a tfp */
	lck_mtx_lock(&tclass_lock);

	tfp = find_tfp_by_pid(pid);
	if (tfp == NULL) {
		tfp = alloc_tclass_for_proc(pid, NULL, 0);
		if (tfp == NULL) {
			lck_mtx_unlock(&tclass_lock);
			error = ENOBUFS;
			goto done;
		}
	}
	tfp->tfp_class = tclass;
	tfp->tfp_qos_mode = so_tcdbg->so_tcbbg_qos_mode;

	lck_mtx_unlock(&tclass_lock);

	if (tfp != NULL) {
		struct fileproc *fp;
		proc_fdlock(p);
		fdt_foreach(fp, p) {
			struct socket *so;

			if (FILEGLOB_DTYPE(fp->fp_glob) != DTYPE_SOCKET) {
				continue;
			}

			so = (struct socket *)fp_get_data(fp);
			if (SOCK_DOM(so) != PF_INET && SOCK_DOM(so) != PF_INET6) {
				continue;
			}

			socket_lock(so, 1);
			if (tfp->tfp_qos_mode == QOS_MODE_MARKING_POLICY_ENABLE) {
				so->so_flags1 |= SOF1_QOSMARKING_ALLOWED;
			} else if (tfp->tfp_qos_mode == QOS_MODE_MARKING_POLICY_DISABLE) {
				so->so_flags1 &= ~SOF1_QOSMARKING_ALLOWED;
			}

			struct inpcb *inp = so ? sotoinpcb(so) : NULL;
			struct tcpcb *tp = inp ? intotcpcb(inp) : NULL;

			if (tp != NULL) {
				if (ecn_val == IPTOS_ECN_ECT1 || ecn_val == IPTOS_ECN_ECT0) {
					tp->ecn_flags |= (ecn_val == IPTOS_ECN_ECT1) ?
					    TE_FORCE_ECT1 : TE_FORCE_ECT0;
				} else {
					tp->ecn_flags &= ~(TE_FORCE_ECT1 | TE_FORCE_ECT0);
				}
			}
			socket_unlock(so, 1);

			if (netsvctype != _NET_SERVICE_TYPE_UNSPEC) {
				error = sock_setsockopt(so, SOL_SOCKET,
				    SO_NET_SERVICE_TYPE, &netsvctype, sizeof(int));
			}
			if (tclass != SO_TC_UNSPEC) {
				error = sock_setsockopt(so, SOL_SOCKET,
				    SO_TRAFFIC_CLASS, &tclass, sizeof(int));
			}
		}

		proc_fdunlock(p);
	}

	error = 0;
done:
	if (p != NULL) {
		proc_rele(p);
	}

	return error;
}

int
set_pname_tclass(struct so_tcdbg *so_tcdbg)
{
	int error = EINVAL;
	struct tclass_for_proc *tfp;

	lck_mtx_lock(&tclass_lock);

	tfp = find_tfp_by_pname(so_tcdbg->so_tcdbg_pname);
	if (tfp == NULL) {
		tfp = alloc_tclass_for_proc(-1, so_tcdbg->so_tcdbg_pname,
		    sizeof(so_tcdbg->so_tcdbg_pname));
		if (tfp == NULL) {
			lck_mtx_unlock(&tclass_lock);
			error = ENOBUFS;
			goto done;
		}
	}
	tfp->tfp_class = so_tcdbg->so_tcdbg_tclass;
	tfp->tfp_qos_mode = so_tcdbg->so_tcbbg_qos_mode;

	lck_mtx_unlock(&tclass_lock);

	error = 0;
done:

	return error;
}

static int
flush_pid_tclass(struct so_tcdbg *so_tcdbg)
{
	pid_t pid = so_tcdbg->so_tcdbg_pid;
	int tclass = so_tcdbg->so_tcdbg_tclass;
	struct fileproc *fp;
	proc_t p;
	int error;

	p = proc_find(pid);
	if (p == PROC_NULL) {
		printf("%s proc_find(%d) failed\n", __func__, pid);
		return EINVAL;
	}

	proc_fdlock(p);

	fdt_foreach(fp, p) {
		struct socket *so;

		if (FILEGLOB_DTYPE(fp->fp_glob) != DTYPE_SOCKET) {
			continue;
		}

		so = (struct socket *)fp_get_data(fp);
		error = sock_setsockopt(so, SOL_SOCKET, SO_FLUSH, &tclass,
		    sizeof(tclass));
		if (error != 0) {
			printf("%s: setsockopt(SO_FLUSH) (so=0x%llx, fd=%d, "
			    "tclass=%d) failed %d\n", __func__,
			    (uint64_t)VM_KERNEL_ADDRPERM(so), fdt_foreach_fd(), tclass,
			    error);
		}
	}

	proc_fdunlock(p);

	proc_rele(p);
	return 0;
}

int
get_pid_tclass(struct so_tcdbg *so_tcdbg)
{
	int error = EINVAL;
	proc_t p = NULL;
	struct tclass_for_proc *tfp;
	pid_t pid = so_tcdbg->so_tcdbg_pid;

	so_tcdbg->so_tcdbg_tclass = SO_TC_UNSPEC; /* Means not set */

	p = proc_find(pid);
	if (p == NULL) {
		printf("%s proc_find(%d) failed\n", __func__, pid);
		goto done;
	}

	/* Need a tfp */
	lck_mtx_lock(&tclass_lock);

	tfp = find_tfp_by_pid(pid);
	if (tfp != NULL) {
		so_tcdbg->so_tcdbg_tclass = tfp->tfp_class;
		so_tcdbg->so_tcbbg_qos_mode = tfp->tfp_qos_mode;
		error = 0;
	}
	lck_mtx_unlock(&tclass_lock);
done:
	if (p != NULL) {
		proc_rele(p);
	}

	return error;
}

int
get_pname_tclass(struct so_tcdbg *so_tcdbg)
{
	int error = EINVAL;
	struct tclass_for_proc *tfp;

	so_tcdbg->so_tcdbg_tclass = SO_TC_UNSPEC; /* Means not set */

	/* Need a tfp */
	lck_mtx_lock(&tclass_lock);

	tfp = find_tfp_by_pname(so_tcdbg->so_tcdbg_pname);
	if (tfp != NULL) {
		so_tcdbg->so_tcdbg_tclass = tfp->tfp_class;
		so_tcdbg->so_tcbbg_qos_mode = tfp->tfp_qos_mode;
		error = 0;
	}
	lck_mtx_unlock(&tclass_lock);

	return error;
}

static int
delete_tclass_for_pid_pname(struct so_tcdbg *so_tcdbg)
{
	int error = EINVAL;
	pid_t pid = so_tcdbg->so_tcdbg_pid;
	struct tclass_for_proc *tfp = NULL;

	lck_mtx_lock(&tclass_lock);

	if (pid != -1) {
		tfp = find_tfp_by_pid(pid);
	} else {
		tfp = find_tfp_by_pname(so_tcdbg->so_tcdbg_pname);
	}

	if (tfp != NULL) {
		free_tclass_for_proc(tfp);
		error = 0;
	}

	lck_mtx_unlock(&tclass_lock);

	return error;
}

/*
 * Setting options requires privileges
 */
__private_extern__ int
so_set_tcdbg(struct socket *so, struct so_tcdbg *so_tcdbg)
{
	int error = 0;

	if ((so->so_state & SS_PRIV) == 0) {
		return EPERM;
	}

	socket_unlock(so, 0);

	switch (so_tcdbg->so_tcdbg_cmd) {
	case SO_TCDBG_PID:
		error = set_pid_tclass(so_tcdbg);
		break;

	case SO_TCDBG_PNAME:
		error = set_pname_tclass(so_tcdbg);
		break;

	case SO_TCDBG_PURGE:
		error = purge_tclass_for_proc();
		break;

	case SO_TCDBG_FLUSH:
		error = flush_tclass_for_proc();
		break;

	case SO_TCDBG_DELETE:
		error = delete_tclass_for_pid_pname(so_tcdbg);
		break;

	case SO_TCDBG_TCFLUSH_PID:
		error = flush_pid_tclass(so_tcdbg);
		break;

	default:
		error = EINVAL;
		break;
	}

	socket_lock(so, 0);

	return error;
}

/*
 * Not required to be privileged to get
 */
__private_extern__ int
sogetopt_tcdbg(struct socket *so, struct sockopt *sopt)
{
	int error = 0;
	struct so_tcdbg so_tcdbg;
	void *buf = NULL;
	size_t len = sopt->sopt_valsize;

	error = sooptcopyin(sopt, &so_tcdbg, sizeof(struct so_tcdbg),
	    sizeof(struct so_tcdbg));
	if (error != 0) {
		return error;
	}

	sopt->sopt_valsize = len;

	socket_unlock(so, 0);

	switch (so_tcdbg.so_tcdbg_cmd) {
	case SO_TCDBG_PID:
		error = get_pid_tclass(&so_tcdbg);
		break;

	case SO_TCDBG_PNAME:
		error = get_pname_tclass(&so_tcdbg);
		break;

	case SO_TCDBG_COUNT:
		lck_mtx_lock(&tclass_lock);
		so_tcdbg.so_tcdbg_count = tfp_count;
		lck_mtx_unlock(&tclass_lock);
		break;

	case SO_TCDBG_LIST: {
		struct tclass_for_proc *tfp;
		int n, alloc_count;
		struct so_tcdbg *ptr;

		lck_mtx_lock(&tclass_lock);
		if ((alloc_count = tfp_count) == 0) {
			lck_mtx_unlock(&tclass_lock);
			error = EINVAL;
			break;
		}
		len = alloc_count * sizeof(struct so_tcdbg);
		lck_mtx_unlock(&tclass_lock);

		buf = kalloc_data(len, Z_WAITOK | Z_ZERO);
		if (buf == NULL) {
			error = ENOBUFS;
			break;
		}

		lck_mtx_lock(&tclass_lock);
		n = 0;
		ptr = (struct so_tcdbg *)buf;
		TAILQ_FOREACH(tfp, &tfp_head, tfp_link) {
			if (++n > alloc_count) {
				break;
			}
			if (tfp->tfp_pid != -1) {
				ptr->so_tcdbg_cmd = SO_TCDBG_PID;
				ptr->so_tcdbg_pid = tfp->tfp_pid;
			} else {
				ptr->so_tcdbg_cmd = SO_TCDBG_PNAME;
				ptr->so_tcdbg_pid = -1;
				strncpy(ptr->so_tcdbg_pname,
				    tfp->tfp_pname,
				    sizeof(ptr->so_tcdbg_pname));
				ptr->so_tcdbg_pname[sizeof(ptr->so_tcdbg_pname) - 1] = '\0';
			}
			ptr->so_tcdbg_tclass = tfp->tfp_class;
			ptr->so_tcbbg_qos_mode = tfp->tfp_qos_mode;
			ptr++;
		}

		lck_mtx_unlock(&tclass_lock);
	}
	break;

	default:
		error = EINVAL;
		break;
	}

	socket_lock(so, 0);

	if (error == 0) {
		if (buf == NULL) {
			error = sooptcopyout(sopt, &so_tcdbg,
			    sizeof(struct so_tcdbg));
		} else {
			error = sooptcopyout(sopt, buf, len);
			kfree_data(buf, len);
		}
	}
	return error;
}

#endif /* (DEVELOPMENT || DEBUG) */

int
so_get_netsvc_marking_level(struct socket *so)
{
	int marking_level = NETSVC_MRKNG_UNKNOWN;
	struct ifnet *ifp = NULL;

	switch (SOCK_DOM(so)) {
	case PF_INET: {
		struct inpcb *inp = sotoinpcb(so);

		if (inp != NULL) {
			ifp = inp->inp_last_outifp;
		}
		break;
	}
	case PF_INET6: {
		struct in6pcb *in6p = sotoin6pcb(so);

		if (in6p != NULL) {
			ifp = in6p->in6p_last_outifp;
		}
		break;
	}
	default:
		break;
	}
	if (ifp != NULL) {
		if ((ifp->if_eflags & IFEF_QOSMARKING_ENABLED) != 0) {
			if ((so->so_flags1 & SOF1_QOSMARKING_ALLOWED)) {
				marking_level = NETSVC_MRKNG_LVL_L3L2_ALL;
			} else {
				marking_level = NETSVC_MRKNG_LVL_L3L2_BK;
			}
		} else {
			marking_level = NETSVC_MRKNG_LVL_L2;
		}
	}
	return marking_level;
}

__private_extern__ int
so_set_traffic_class(struct socket *so, int optval)
{
	int error = 0;

	if (optval < SO_TC_BE || optval > SO_TC_CTL) {
		error = EINVAL;
	} else {
		switch (optval) {
		case _SO_TC_BK:
			optval = SO_TC_BK;
			break;
		case _SO_TC_VI:
			optval = SO_TC_VI;
			break;
		case _SO_TC_VO:
			optval = SO_TC_VO;
			break;
		default:
			if (!SO_VALID_TC(optval)) {
				error = EINVAL;
			}
			break;
		}

		if (error == 0) {
			int oldval = so->so_traffic_class;

			VERIFY(SO_VALID_TC(optval));
			so->so_traffic_class = (uint16_t)optval;

			if ((SOCK_DOM(so) == PF_INET ||
			    SOCK_DOM(so) == PF_INET6) &&
			    SOCK_TYPE(so) == SOCK_STREAM) {
				set_tcp_stream_priority(so);
			}

			if ((SOCK_DOM(so) == PF_INET ||
			    SOCK_DOM(so) == PF_INET6) &&
			    optval != oldval && (optval == SO_TC_BK_SYS ||
			    oldval == SO_TC_BK_SYS)) {
				/*
				 * If the app switches from BK_SYS to something
				 * else, resume the socket if it was suspended.
				 */
				if (oldval == SO_TC_BK_SYS) {
					inp_reset_fc_state(so->so_pcb);
				}

				SOTHROTTLELOG("throttle[%d]: so 0x%llx "
				    "[%d,%d] opportunistic %s\n", so->last_pid,
				    (uint64_t)VM_KERNEL_ADDRPERM(so),
				    SOCK_DOM(so), SOCK_TYPE(so),
				    (optval == SO_TC_BK_SYS) ? "ON" : "OFF");
			}
		}
	}
	return error;
}

__private_extern__ int
so_set_net_service_type(struct socket *so, int netsvctype)
{
	int sotc;
	int error;

	if (!IS_VALID_NET_SERVICE_TYPE(netsvctype)) {
		return EINVAL;
	}

	sotc = sotc_by_netservicetype[netsvctype];
	error = so_set_traffic_class(so, sotc);
	if (error != 0) {
		return error;
	}
	so->so_netsvctype = (int8_t)netsvctype;
	so->so_flags1 |= SOF1_TC_NET_SERV_TYPE;

	return 0;
}

__private_extern__ void
so_set_default_traffic_class(struct socket *so)
{
	so->so_traffic_class = SO_TC_BE;

	if ((SOCK_DOM(so) == PF_INET || SOCK_DOM(so) == PF_INET6)) {
		if (net_qos_policy_restricted == 0) {
			so->so_flags1 |= SOF1_QOSMARKING_ALLOWED;
		}
#if (DEVELOPMENT || DEBUG)
		if (tfp_count > 0) {
			set_tclass_for_curr_proc(so);
		}
#endif /* (DEVELOPMENT || DEBUG) */
	}
}

__private_extern__ int
so_set_opportunistic(struct socket *so, int optval)
{
	return so_set_traffic_class(so, (optval == 0) ?
	           SO_TC_BE : SO_TC_BK_SYS);
}

__private_extern__ int
so_get_opportunistic(struct socket *so)
{
	return so->so_traffic_class == SO_TC_BK_SYS;
}

__private_extern__ int
so_tc_from_control(struct mbuf *control, int *out_netsvctype)
{
	struct cmsghdr *cm;
	int sotc = SO_TC_UNSPEC;

	*out_netsvctype = _NET_SERVICE_TYPE_UNSPEC;

	for (cm = M_FIRST_CMSGHDR(control);
	    is_cmsg_valid(control, cm);
	    cm = M_NXT_CMSGHDR(control, cm)) {
		int val;

		if (cm->cmsg_level != SOL_SOCKET ||
		    cm->cmsg_len != CMSG_LEN(sizeof(int))) {
			continue;
		}
		val = *(int *)(void *)CMSG_DATA(cm);
		/*
		 * The first valid option wins
		 */
		switch (cm->cmsg_type) {
		case SO_TRAFFIC_CLASS:
			if (SO_VALID_TC(val)) {
				sotc = val;
				return sotc;
				/* NOT REACHED */
			} else if (val < SO_TC_NET_SERVICE_OFFSET) {
				break;
			}
			/*
			 * Handle the case SO_NET_SERVICE_TYPE values are
			 * passed using SO_TRAFFIC_CLASS
			 */
			val = val - SO_TC_NET_SERVICE_OFFSET;
			OS_FALLTHROUGH;
		case SO_NET_SERVICE_TYPE:
			if (!IS_VALID_NET_SERVICE_TYPE(val)) {
				break;
			}
			*out_netsvctype = val;
			sotc = sotc_by_netservicetype[val];
			return sotc;
		/* NOT REACHED */
		default:
			break;
		}
	}

	return sotc;
}

__private_extern__ int
so_tos_from_control(struct mbuf *control)
{
	struct cmsghdr *cm;
	int tos = IPTOS_UNSPEC;

	for (cm = M_FIRST_CMSGHDR(control);
	    is_cmsg_valid(control, cm);
	    cm = M_NXT_CMSGHDR(control, cm)) {
		if (cm->cmsg_len != CMSG_LEN(sizeof(int))) {
			continue;
		}

		if ((cm->cmsg_level == IPPROTO_IP &&
		    cm->cmsg_type == IP_TOS) ||
		    (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_TCLASS)) {
			tos = *(int *)(void *)CMSG_DATA(cm) & IPTOS_MASK;
			/* The first valid option wins */
			break;
		}
	}

	return tos;
}

__private_extern__ void
so_recv_data_stat(struct socket *so, struct mbuf *m, size_t off)
{
	uint32_t mtc = m_get_traffic_class(m);

	if (mtc >= SO_TC_STATS_MAX) {
		mtc = MBUF_TC_BE;
	}

	so->so_tc_stats[mtc].rxpackets += 1;
	so->so_tc_stats[mtc].rxbytes +=
	    ((m->m_flags & M_PKTHDR) ? m->m_pkthdr.len : 0) + off;
}

__private_extern__ void
so_inc_recv_data_stat(struct socket *so, size_t pkts, size_t bytes,
    uint32_t mtc)
{
	if (mtc >= SO_TC_STATS_MAX) {
		mtc = MBUF_TC_BE;
	}

	so->so_tc_stats[mtc].rxpackets += pkts;
	so->so_tc_stats[mtc].rxbytes += bytes;
}

static inline int
so_throttle_best_effort(struct socket *so, struct ifnet *ifp)
{
	uint32_t uptime = (uint32_t)net_uptime();
	return soissrcbesteffort(so) &&
	       net_io_policy_throttle_best_effort == 1 &&
	       ifp->if_rt_sendts > 0 &&
	       (int)(uptime - ifp->if_rt_sendts) <= TCP_BG_SWITCH_TIME;
}

__private_extern__ void
set_tcp_stream_priority(struct socket *so)
{
	struct inpcb *inp = sotoinpcb(so);
	struct tcpcb *tp = intotcpcb(inp);
	struct ifnet *outifp;
	u_char old_cc = tp->tcp_cc_index;
	int recvbg = IS_TCP_RECV_BG(so);
	bool is_local = false, fg_active = false;
	uint32_t uptime;

	VERIFY((SOCK_CHECK_DOM(so, PF_INET) ||
	    SOCK_CHECK_DOM(so, PF_INET6)) &&
	    SOCK_CHECK_TYPE(so, SOCK_STREAM) &&
	    SOCK_CHECK_PROTO(so, IPPROTO_TCP));

	/* Return if the socket is in a terminal state */
	if (inp->inp_state == INPCB_STATE_DEAD) {
		return;
	}

	outifp = inp->inp_last_outifp;
	uptime = (uint32_t)net_uptime();

	/*
	 * If the socket was marked as a background socket or if the
	 * traffic class is set to background with traffic class socket
	 * option then make both send and recv side of the stream to be
	 * background. The variable sotcdb which can be set with sysctl
	 * is used to disable these settings for testing.
	 */
	if (outifp == NULL || (outifp->if_flags & IFF_LOOPBACK)) {
		is_local = true;
	}

	/* Check if there has been recent foreground activity */
	if (outifp != NULL) {
		/*
		 * If the traffic source is background, check if
		 * there is recent foreground activity which should
		 * continue to keep the traffic source as background.
		 * Otherwise, we can switch the traffic source to
		 * foreground.
		 */
		if (soissrcbackground(so) && outifp->if_fg_sendts > 0 &&
		    (int)(uptime - outifp->if_fg_sendts) <= TCP_BG_SWITCH_TIME) {
			fg_active = true;
		}

		/*
		 * The traffic source is best-effort -- check if
		 * the policy to throttle best effort is enabled
		 * and there was realtime activity on this
		 * interface recently. If this is true, enable
		 * algorithms that respond to increased latency
		 * on best-effort traffic.
		 */
		if (so_throttle_best_effort(so, outifp)) {
			fg_active = true;
		}
	}

	/*
	 * System initiated background traffic like cloud uploads should
	 * always use background delay sensitive algorithms. This will
	 * make the stream more responsive to other streams on the user's
	 * network and it will minimize latency induced.
	 */
	if (fg_active || IS_SO_TC_BACKGROUNDSYSTEM(so->so_traffic_class)) {
		/*
		 * If the interface that the connection is using is
		 * loopback, do not use background congestion
		 * control algorithm.
		 *
		 * If there has been recent foreground activity or if there
		 * was an indication that a real time foreground application
		 * is going to use networking (net_io_policy_throttled),
		 * switch the background and best effort streams to use background
		 * congestion control algorithm.
		 */
		if ((sotcdb & SOTCDB_NO_SENDTCPBG) != 0 || is_local) {
			if (old_cc == TCP_CC_ALGO_BACKGROUND_INDEX) {
				tcp_set_foreground_cc(so);
			}
		} else {
			if (old_cc != TCP_CC_ALGO_BACKGROUND_INDEX) {
				tcp_set_background_cc(so);
			}
		}

		/* Set receive side background flags */
		if ((sotcdb & SOTCDB_NO_RECVTCPBG) != 0 || is_local) {
			tcp_clear_recv_bg(so);
		} else {
			tcp_set_recv_bg(so);
		}
	} else {
		/*
		 * If there is no recent foreground activity, even the
		 * background flows can use foreground congestion controller.
		 */
		tcp_clear_recv_bg(so);
		if (old_cc == TCP_CC_ALGO_BACKGROUND_INDEX) {
			tcp_set_foreground_cc(so);
		}
	}

	if (old_cc != tp->tcp_cc_index || recvbg != IS_TCP_RECV_BG(so)) {
		SOTHROTTLELOG("throttle[%d]: so 0x%llx [%d,%d] TCP %s send; "
		    "%s recv\n", so->last_pid,
		    (uint64_t)VM_KERNEL_ADDRPERM(so),
		    SOCK_DOM(so), SOCK_TYPE(so),
		    (tp->tcp_cc_index == TCP_CC_ALGO_BACKGROUND_INDEX) ?
		    "background" : "foreground",
		    IS_TCP_RECV_BG(so) ? "background" : "foreground");
	}
}

/*
 * Set traffic class to an IPv4 or IPv6 packet
 * - mark the mbuf
 * - set the DSCP code following the WMM mapping
 */
__private_extern__ void
set_packet_service_class(struct mbuf *m, struct socket *so,
    int sotc, uint32_t flags)
{
	mbuf_svc_class_t msc = MBUF_SC_BE;         /* Best effort by default */
	struct inpcb *inp = sotoinpcb(so); /* in6pcb and inpcb are the same */

	if (!(m->m_flags & M_PKTHDR)) {
		return;
	}

	/*
	 * Here is the precedence:
	 * 1) TRAFFIC_MGT_SO_BACKGROUND trumps all
	 * 2) Traffic class passed via ancillary data to sendmsdg(2)
	 * 3) Traffic class socket option last
	 */
	if (sotc != SO_TC_UNSPEC) {
		VERIFY(SO_VALID_TC(sotc));
		msc = so_tc2msc(sotc);
		/* Assert because tc must have been valid */
		VERIFY(MBUF_VALID_SC(msc));
	}

	/*
	 * If TRAFFIC_MGT_SO_BACKGROUND is set or policy to throttle
	 * best effort is set, depress the priority.
	 */
	if (!IS_MBUF_SC_BACKGROUND(msc) && soisthrottled(so)) {
		msc = MBUF_SC_BK;
	}

	if (IS_MBUF_SC_BESTEFFORT(msc) && inp->inp_last_outifp != NULL &&
	    so_throttle_best_effort(so, inp->inp_last_outifp)) {
		msc = MBUF_SC_BK;
	}

	if (soissrcbackground(so)) {
		m->m_pkthdr.pkt_flags |= PKTF_SO_BACKGROUND;
	}

	if (soissrcrealtime(so) || IS_MBUF_SC_REALTIME(msc)) {
		m->m_pkthdr.pkt_flags |= PKTF_SO_REALTIME;
	}
	/*
	 * Set the traffic class in the mbuf packet header svc field
	 */
	if (sotcdb & SOTCDB_NO_MTC) {
		goto no_mbtc;
	}

	/*
	 * Elevate service class if the packet is a pure TCP ACK.
	 * We can do this only when the flow is not a background
	 * flow and the outgoing interface supports
	 * transmit-start model.
	 */
	if (!IS_MBUF_SC_BACKGROUND(msc) &&
	    (flags & (PKT_SCF_TCP_ACK | PKT_SCF_TCP_SYN)) != 0) {
		msc = MBUF_SC_CTL;
	}

	(void) m_set_service_class(m, msc);

	/*
	 * Set the privileged traffic auxiliary flag if applicable,
	 * or clear it.
	 */
	if (!(sotcdb & SOTCDB_NO_PRIVILEGED) && soisprivilegedtraffic(so) &&
	    msc != MBUF_SC_UNSPEC) {
		m->m_pkthdr.pkt_flags |= PKTF_PRIO_PRIVILEGED;
	} else {
		m->m_pkthdr.pkt_flags &= ~PKTF_PRIO_PRIVILEGED;
	}

no_mbtc:
	/*
	 * For TCP with background traffic class switch CC algo based on sysctl
	 */
	if (so->so_type == SOCK_STREAM) {
		set_tcp_stream_priority(so);
	}

	so_tc_update_stats(m, so, msc);
}

__private_extern__ void
so_tc_update_stats(struct mbuf *m, struct socket *so, mbuf_svc_class_t msc)
{
	mbuf_traffic_class_t mtc;

	/*
	 * Assume socket and mbuf traffic class values are the same
	 * Also assume the socket lock is held.  Note that the stats
	 * at the socket layer are reduced down to the legacy traffic
	 * classes; we could/should potentially expand so_tc_stats[].
	 */
	mtc = MBUF_SC2TC(msc);
	VERIFY(mtc < SO_TC_STATS_MAX);
	so->so_tc_stats[mtc].txpackets += 1;
	so->so_tc_stats[mtc].txbytes += m->m_pkthdr.len;
}

__private_extern__ mbuf_svc_class_t
so_tc2msc(int tc)
{
	mbuf_svc_class_t msc;

	switch (tc) {
	case SO_TC_BK_SYS:
		msc = MBUF_SC_BK_SYS;
		break;
	case SO_TC_BK:
	case _SO_TC_BK:
		msc = MBUF_SC_BK;
		break;
	case SO_TC_BE:
		msc = MBUF_SC_BE;
		break;
	case SO_TC_RD:
		msc = MBUF_SC_RD;
		break;
	case SO_TC_OAM:
		msc = MBUF_SC_OAM;
		break;
	case SO_TC_AV:
		msc = MBUF_SC_AV;
		break;
	case SO_TC_RV:
		msc = MBUF_SC_RV;
		break;
	case SO_TC_VI:
	case _SO_TC_VI:
		msc = MBUF_SC_VI;
		break;
	case SO_TC_NETSVC_SIG:
		msc = MBUF_SC_SIG;
		break;
	case SO_TC_VO:
	case _SO_TC_VO:
		msc = MBUF_SC_VO;
		break;
	case SO_TC_CTL:
		msc = MBUF_SC_CTL;
		break;
	case SO_TC_ALL:
	default:
		msc = MBUF_SC_UNSPEC;
		break;
	}

	return msc;
}

__private_extern__ int
so_svc2tc(mbuf_svc_class_t svc)
{
	switch (svc) {
	case MBUF_SC_BK_SYS:
		return SO_TC_BK_SYS;
	case MBUF_SC_BK:
		return SO_TC_BK;
	case MBUF_SC_BE:
		return SO_TC_BE;
	case MBUF_SC_RD:
		return SO_TC_RD;
	case MBUF_SC_OAM:
		return SO_TC_OAM;
	case MBUF_SC_AV:
		return SO_TC_AV;
	case MBUF_SC_RV:
		return SO_TC_RV;
	case MBUF_SC_VI:
		return SO_TC_VI;
	case MBUF_SC_SIG:
		return SO_TC_NETSVC_SIG;
	case MBUF_SC_VO:
		return SO_TC_VO;
	case MBUF_SC_CTL:
		return SO_TC_CTL;
	case MBUF_SC_UNSPEC:
	default:
		return SO_TC_BE;
	}
}

static size_t
sotc_index(int sotc)
{
	switch (sotc) {
	case SO_TC_BK_SYS:
		return SOTCIX_BK_SYS;
	case _SO_TC_BK:
	case SO_TC_BK:
		return SOTCIX_BK;

	case SO_TC_BE:
		return SOTCIX_BE;
	case SO_TC_RD:
		return SOTCIX_RD;
	case SO_TC_OAM:
		return SOTCIX_OAM;

	case SO_TC_AV:
		return SOTCIX_AV;
	case SO_TC_RV:
		return SOTCIX_RV;
	case _SO_TC_VI:
	case SO_TC_VI:
		return SOTCIX_VI;

	case _SO_TC_VO:
	case SO_TC_VO:
		return SOTCIX_VO;
	case SO_TC_CTL:
		return SOTCIX_CTL;

	default:
		break;
	}
	/*
	 * Unknown traffic class value
	 */
	return SIZE_T_MAX;
}

uint8_t
fastlane_sc_to_dscp(uint32_t svc_class)
{
	uint8_t dscp = _DSCP_DF;

	switch (svc_class) {
	case MBUF_SC_BK_SYS:
	case MBUF_SC_BK:
		dscp = _DSCP_AF11;
		break;

	case MBUF_SC_BE:
		dscp = _DSCP_DF;
		break;
	case MBUF_SC_RD:
		dscp = _DSCP_AF21;
		break;
	case MBUF_SC_OAM:
		dscp = _DSCP_CS2;
		break;

	case MBUF_SC_AV:
		dscp = _DSCP_AF31;
		break;
	case MBUF_SC_RV:
		dscp = _DSCP_CS4;
		break;
	case MBUF_SC_VI:
		dscp = _DSCP_AF41;
		break;
	case MBUF_SC_SIG:
		dscp = _DSCP_CS3;
		break;

	case MBUF_SC_VO:
		dscp = _DSCP_EF;
		break;
	case MBUF_SC_CTL:
		dscp = _DSCP_DF;
		break;
	default:
		dscp = _DSCP_DF;
		break;
	}

	return dscp;
}

uint8_t
rfc4594_sc_to_dscp(uint32_t svc_class)
{
	uint8_t dscp = _DSCP_DF;

	switch (svc_class) {
	case MBUF_SC_BK_SYS:            /* Low-Priority Data */
	case MBUF_SC_BK:
		dscp = _DSCP_CS1;
		break;

	case MBUF_SC_BE:                /* Standard */
		dscp = _DSCP_DF;
		break;
	case MBUF_SC_RD:                /* Low-Latency Data */
		dscp = _DSCP_AF21;
		break;

	/* SVC_CLASS Not Defined:  High-Throughput Data */

	case MBUF_SC_OAM:               /* OAM */
		dscp = _DSCP_CS2;
		break;

	/* SVC_CLASS Not Defined:  Broadcast Video */

	case MBUF_SC_AV:                /* Multimedia Streaming */
		dscp = _DSCP_AF31;
		break;
	case MBUF_SC_RV:                /* Real-Time Interactive */
		dscp = _DSCP_CS4;
		break;
	case MBUF_SC_VI:                /* Multimedia Conferencing */
		dscp = _DSCP_AF41;
		break;
	case MBUF_SC_SIG:               /* Signaling */
		dscp = _DSCP_CS5;
		break;

	case MBUF_SC_VO:                /* Telephony */
		dscp = _DSCP_EF;
		break;
	case MBUF_SC_CTL:               /* Network Control*/
		dscp = _DSCP_CS6;
		break;
	default:
		dscp = _DSCP_DF;
		break;
	}

	return dscp;
}

mbuf_traffic_class_t
rfc4594_dscp_to_tc(uint8_t dscp)
{
	mbuf_traffic_class_t tc = MBUF_TC_BE;

	switch (dscp) {
	case _DSCP_CS1:
		tc = MBUF_TC_BK;
		break;
	case _DSCP_DF:
	case _DSCP_AF21:
	case _DSCP_CS2:
		tc = MBUF_TC_BE;
		break;
	case _DSCP_AF31:
	case _DSCP_CS4:
	case _DSCP_AF41:
	case _DSCP_CS5:
		tc = MBUF_TC_VI;
		break;
	case _DSCP_EF:
	case _DSCP_CS6:
		tc = MBUF_TC_VO;
		break;
	default:
		tc = MBUF_TC_BE;
		break;
	}

	return tc;
}

/*
 * Pass NULL ifp for default map
 */
static errno_t
set_netsvctype_dscp_map(struct net_qos_dscp_map *net_qos_dscp_map,
    const struct netsvctype_dscp_map *netsvctype_dscp_map)
{
	size_t i;
	int netsvctype;

	/*
	 * Do not accept more that max number of distinct DSCPs
	 */
	if (net_qos_dscp_map == NULL || netsvctype_dscp_map == NULL) {
		return EINVAL;
	}

	/*
	 * Validate input parameters
	 */
	for (i = 0; i < _NET_SERVICE_TYPE_COUNT; i++) {
		if (!IS_VALID_NET_SERVICE_TYPE(netsvctype_dscp_map[i].netsvctype)) {
			return EINVAL;
		}
		if (netsvctype_dscp_map[i].dscp > _MAX_DSCP) {
			return EINVAL;
		}
	}

	for (i = 0; i < _NET_SERVICE_TYPE_COUNT; i++) {
		netsvctype = netsvctype_dscp_map[i].netsvctype;

		net_qos_dscp_map->netsvctype_to_dscp[netsvctype] =
		    netsvctype_dscp_map[i].dscp;
	}
	for (netsvctype = 0; netsvctype < _NET_SERVICE_TYPE_COUNT; netsvctype++) {
		switch (netsvctype) {
		case NET_SERVICE_TYPE_BE:
		case NET_SERVICE_TYPE_BK:
		case NET_SERVICE_TYPE_VI:
		case NET_SERVICE_TYPE_VO:
		case NET_SERVICE_TYPE_RV:
		case NET_SERVICE_TYPE_AV:
		case NET_SERVICE_TYPE_OAM:
		case NET_SERVICE_TYPE_RD: {
			size_t sotcix;

			sotcix = sotc_index(sotc_by_netservicetype[netsvctype]);
			if (sotcix != SIZE_T_MAX) {
				net_qos_dscp_map->sotc_to_dscp[sotcix]  =
				    netsvctype_dscp_map[netsvctype].dscp;
			}
			break;
		}
		case  NET_SERVICE_TYPE_SIG:
			/* Signaling does not have its own traffic class */
			break;
		default:
			/* We should not be here */
			ASSERT(0);
		}
	}
	if (net_qos_dscp_map == &fastlane_net_qos_dscp_map) {
		/* Network control socket traffic class is always best effort for fastlane*/
		net_qos_dscp_map->sotc_to_dscp[SOTCIX_CTL] = _DSCP_DF;
	} else {
		net_qos_dscp_map->sotc_to_dscp[SOTCIX_CTL] = _DSCP_CS6;
	}

	/* Background system socket traffic class DSCP same as background */
	net_qos_dscp_map->sotc_to_dscp[SOTCIX_BK_SYS] =
	    net_qos_dscp_map->sotc_to_dscp[SOTCIX_BK];

	return 0;
}

static size_t
get_netsvctype_dscp_map(struct netsvctype_dscp_map *netsvctype_dscp_map)
{
	struct net_qos_dscp_map *net_qos_dscp_map;
	int i;

	net_qos_dscp_map = &fastlane_net_qos_dscp_map;

	for (i = 0; i < _NET_SERVICE_TYPE_COUNT; i++) {
		netsvctype_dscp_map[i].netsvctype = i;
		netsvctype_dscp_map[i].dscp = net_qos_dscp_map->netsvctype_to_dscp[i];
	}

	return i * sizeof(struct netsvctype_dscp_map);
}

void
net_qos_map_init()
{
	errno_t error;

	error = set_netsvctype_dscp_map(&fastlane_net_qos_dscp_map,
	    fastlane_netsvctype_dscp_map);
	ASSERT(error == 0);

	error = set_netsvctype_dscp_map(&rfc4594_net_qos_dscp_map,
	    rfc4594_netsvctype_dscp_map);
	ASSERT(error == 0);

#if (DEBUG || DEVELOPMENT)
	error = set_netsvctype_dscp_map(&custom_net_qos_dscp_map,
	    rfc4594_netsvctype_dscp_map);
	ASSERT(error == 0);

#endif /* (DEBUG || DEVELOPMENT) */

	set_dscp_to_wifi_ac_map(default_dscp_to_wifi_ac_map, 1);
}

int
sysctl_default_netsvctype_to_dscp_map SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;

	if (req->oldptr == USER_ADDR_NULL) {
		req->oldidx =
		    _NET_SERVICE_TYPE_COUNT * sizeof(struct netsvctype_dscp_map);
	} else if (req->oldlen > 0) {
		struct netsvctype_dscp_map netsvctype_dscp_map[_NET_SERVICE_TYPE_COUNT] = {};
		size_t len;

		len = get_netsvctype_dscp_map(netsvctype_dscp_map);

		error = SYSCTL_OUT(req, netsvctype_dscp_map,
		    MIN(len, req->oldlen));
		if (error != 0) {
			goto done;
		}
	}

	if (req->newptr != USER_ADDR_NULL) {
		error = EPERM;
	}
done:
	return error;
}

__private_extern__ errno_t
set_packet_qos(struct mbuf *m, struct ifnet *ifp, boolean_t qos_allowed,
    int sotc, int netsvctype, uint8_t *dscp_inout)
{
	if (ifp == NULL || dscp_inout == NULL) {
		return EINVAL;
	}

	if ((ifp->if_eflags & IFEF_QOSMARKING_ENABLED) != 0 &&
	    ifp->if_qosmarking_mode != IFRTYPE_QOSMARKING_MODE_NONE) {
		uint8_t dscp;
		const struct net_qos_dscp_map *net_qos_dscp_map = NULL;

		switch (ifp->if_qosmarking_mode) {
		case IFRTYPE_QOSMARKING_FASTLANE:
			net_qos_dscp_map = &fastlane_net_qos_dscp_map;
			break;
		case IFRTYPE_QOSMARKING_RFC4594:
			net_qos_dscp_map = &rfc4594_net_qos_dscp_map;
			break;
#if (DEBUG || DEVELOPMENT)
		case IFRTYPE_QOSMARKING_CUSTOM:
			net_qos_dscp_map = &custom_net_qos_dscp_map;
			break;
#endif /* (DEBUG || DEVELOPMENT) */
		default:
			panic("invalid QoS marking type");
			/* NOTREACHED */
		}

		/*
		 * When on a Fastlane network, IP_TOS/IPV6_TCLASS are no-ops
		 */
		dscp = _DSCP_DF;

		/*
		 * For DSCP use the network service type is specified, otherwise
		 * use the socket traffic class
		 *
		 * When not whitelisted by the policy, set DSCP only for best
		 * effort and background, and set the mbuf service class to
		 * best effort as well so the packet will be queued and
		 * scheduled at a lower priority.
		 * We still want to prioritize control traffic on the interface
		 * so we do not change the mbuf service class for SO_TC_CTL
		 */
		if (IS_VALID_NET_SERVICE_TYPE(netsvctype) &&
		    netsvctype != NET_SERVICE_TYPE_BE) {
			dscp = net_qos_dscp_map->netsvctype_to_dscp[netsvctype];

			if (qos_allowed == FALSE &&
			    netsvctype != NET_SERVICE_TYPE_BE &&
			    netsvctype != NET_SERVICE_TYPE_BK) {
				dscp = _DSCP_DF;
				if (sotc != SO_TC_CTL) {
					m_set_service_class(m, MBUF_SC_BE);
				}
			}
		} else if (sotc != SO_TC_UNSPEC) {
			size_t sotcix = sotc_index(sotc);
			if (sotcix != SIZE_T_MAX) {
				dscp = net_qos_dscp_map->sotc_to_dscp[sotcix];

				if (qos_allowed == FALSE && sotc != SO_TC_BE &&
				    sotc != SO_TC_BK && sotc != SO_TC_BK_SYS &&
				    sotc != SO_TC_CTL) {
					dscp = _DSCP_DF;
					if (sotc != SO_TC_CTL) {
						m_set_service_class(m, MBUF_SC_BE);
					}
				}
			}
		}
		if (net_qos_verbose != 0) {
			printf("%s qos_allowed %d sotc %u netsvctype %u dscp %u\n",
			    __func__, qos_allowed, sotc, netsvctype, dscp);
		}

		if (*dscp_inout != dscp) {
			*dscp_inout = dscp;
		}
	} else if (*dscp_inout != _DSCP_DF && IFNET_IS_WIFI_INFRA(ifp)) {
		mbuf_svc_class_t msc = m_get_service_class(m);

		/*
		 * For WiFi infra, when the mbuf service class is best effort
		 * and the DSCP is not default, set the service class based
		 * on DSCP
		 */
		if (msc == MBUF_SC_BE) {
			msc = wifi_dscp_to_msc_array[*dscp_inout];

			if (msc != MBUF_SC_BE) {
				m_set_service_class(m, msc);

				if (net_qos_verbose != 0) {
					printf("%s set msc %u for dscp %u\n",
					    __func__, msc, *dscp_inout);
				}
			}
		}
	}

	return 0;
}

static void
set_dscp_to_wifi_ac_map(const struct dcsp_msc_map *map, int clear)
{
	int i;

	if (clear) {
		bzero(wifi_dscp_to_msc_array, sizeof(wifi_dscp_to_msc_array));
	}

	for (i = 0; i < DSCP_ARRAY_SIZE; i++) {
		const struct dcsp_msc_map *elem = map + i;

		if (elem->dscp > _MAX_DSCP || elem->msc == MBUF_SC_UNSPEC) {
			break;
		}
		switch (elem->msc) {
		case MBUF_SC_BK_SYS:
		case MBUF_SC_BK:
			wifi_dscp_to_msc_array[elem->dscp] = MBUF_SC_BK;
			break;
		default:
		case MBUF_SC_BE:
		case MBUF_SC_RD:
		case MBUF_SC_OAM:
			wifi_dscp_to_msc_array[elem->dscp] = MBUF_SC_BE;
			break;
		case MBUF_SC_AV:
		case MBUF_SC_RV:
		case MBUF_SC_VI:
			wifi_dscp_to_msc_array[elem->dscp] = MBUF_SC_VI;
			break;
		case MBUF_SC_VO:
		case MBUF_SC_CTL:
			wifi_dscp_to_msc_array[elem->dscp] = MBUF_SC_VO;
			break;
		}
	}
}

static errno_t
dscp_msc_map_from_netsvctype_dscp_map(struct netsvctype_dscp_map *netsvctype_dscp_map,
    size_t count, struct dcsp_msc_map *dcsp_msc_map)
{
	errno_t error = 0;
	uint32_t i;

	/*
	 * Validate input parameters
	 */
	for (i = 0; i < count; i++) {
		if (!SO_VALID_TC(netsvctype_dscp_map[i].netsvctype)) {
			error = EINVAL;
			goto done;
		}
		if (netsvctype_dscp_map[i].dscp > _MAX_DSCP) {
			error = EINVAL;
			goto done;
		}
	}

	bzero(dcsp_msc_map, DSCP_ARRAY_SIZE * sizeof(struct dcsp_msc_map));

	for (i = 0; i < count; i++) {
		dcsp_msc_map[i].dscp = netsvctype_dscp_map[i].dscp;
		dcsp_msc_map[i].msc = so_tc2msc(netsvctype_dscp_map[i].netsvctype);
	}
done:
	return error;
}

int
sysctl_dscp_to_wifi_ac_map SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;
	size_t len = DSCP_ARRAY_SIZE * sizeof(struct netsvctype_dscp_map);
	struct netsvctype_dscp_map netsvctype_dscp_map[DSCP_ARRAY_SIZE] = {};
	struct dcsp_msc_map dcsp_msc_map[DSCP_ARRAY_SIZE];
	size_t count;

	if (req->oldptr == USER_ADDR_NULL) {
		req->oldidx = len;
	} else if (req->oldlen > 0) {
		uint8_t i;

		for (i = 0; i < DSCP_ARRAY_SIZE; i++) {
			netsvctype_dscp_map[i].dscp = i;
			netsvctype_dscp_map[i].netsvctype =
			    so_svc2tc(wifi_dscp_to_msc_array[i]);
		}
		error = SYSCTL_OUT(req, netsvctype_dscp_map,
		    MIN(len, req->oldlen));
		if (error != 0) {
			goto done;
		}
	}

	if (req->newptr == USER_ADDR_NULL) {
		goto done;
	}

	error = proc_suser(current_proc());
	if (error != 0) {
		goto done;
	}

	/*
	 * Check input length
	 */
	if (req->newlen > len) {
		error = EINVAL;
		goto done;
	}
	/*
	 * Cap the number of entries to copy from input buffer
	 */
	if (len > req->newlen) {
		len = req->newlen;
	}
	error = SYSCTL_IN(req, netsvctype_dscp_map, len);
	if (error != 0) {
		goto done;
	}
	count = len / sizeof(struct netsvctype_dscp_map);
	bzero(dcsp_msc_map, sizeof(dcsp_msc_map));
	error = dscp_msc_map_from_netsvctype_dscp_map(netsvctype_dscp_map, count,
	    dcsp_msc_map);
	if (error != 0) {
		goto done;
	}
	set_dscp_to_wifi_ac_map(dcsp_msc_map, 0);
done:
	return error;
}

int
sysctl_reset_dscp_to_wifi_ac_map SYSCTL_HANDLER_ARGS
{
#pragma unused(oidp, arg1, arg2)
	int error = 0;
	int val = 0;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr) {
		return error;
	}
	if (req->newptr == USER_ADDR_NULL) {
		return 0;
	}
	error = proc_suser(current_proc());
	if (error != 0) {
		return error;
	}

	set_dscp_to_wifi_ac_map(default_dscp_to_wifi_ac_map, 1);

	return 0;
}

/*
 * Returns whether a large upload or download transfer should be marked as
 * BK service type for network activity. This is a system level
 * hint/suggestion to classify application traffic based on statistics
 * collected from the current network attachment
 *
 * Returns 1 for BK and 0 for default
 */

int
net_qos_guideline(struct proc *p, struct net_qos_guideline_args *arg,
    int *retval)
{
#pragma unused(p)
#define RETURN_USE_BK   1
#define RETURN_USE_DEFAULT      0
	struct net_qos_param qos_arg;
	struct ifnet *ipv4_primary, *ipv6_primary;
	int err = 0;

	if (arg->param == USER_ADDR_NULL || retval == NULL ||
	    arg->param_len != sizeof(qos_arg)) {
		return EINVAL;
	}
	err = copyin(arg->param, (caddr_t) &qos_arg, sizeof(qos_arg));
	if (err != 0) {
		return err;
	}

	*retval = RETURN_USE_DEFAULT;
	ipv4_primary = ifindex2ifnet[get_primary_ifscope(AF_INET)];
	ipv6_primary = ifindex2ifnet[get_primary_ifscope(AF_INET6)];

	/*
	 * If either of the interfaces is in Low Internet mode, enable
	 * background delay based algorithms on this transfer
	 */
	if (qos_arg.nq_uplink) {
		if ((ipv4_primary != NULL &&
		    (ipv4_primary->if_xflags & IFXF_LOW_INTERNET_UL)) ||
		    (ipv6_primary != NULL &&
		    (ipv6_primary->if_xflags & IFXF_LOW_INTERNET_UL))) {
			*retval = RETURN_USE_BK;
			return 0;
		}
	} else {
		if ((ipv4_primary != NULL &&
		    (ipv4_primary->if_xflags & IFXF_LOW_INTERNET_DL)) ||
		    (ipv6_primary != NULL &&
		    (ipv6_primary->if_xflags & IFXF_LOW_INTERNET_DL))) {
			*retval = RETURN_USE_BK;
			return 0;
		}
	}

	/*
	 * Some times IPv4 and IPv6 primary interfaces can be different.
	 * In this case, if either of them is non-cellular, we should mark
	 * the transfer as BK as it can potentially get used based on
	 * the host name resolution
	 */
	if (ipv4_primary != NULL && IFNET_IS_EXPENSIVE(ipv4_primary) &&
	    ipv6_primary != NULL && IFNET_IS_EXPENSIVE(ipv6_primary)) {
		if (qos_arg.nq_use_expensive) {
			return 0;
		} else {
			*retval = RETURN_USE_BK;
			return 0;
		}
	}
	if (ipv4_primary != NULL && IFNET_IS_CONSTRAINED(ipv4_primary) &&
	    ipv6_primary != NULL && IFNET_IS_CONSTRAINED(ipv6_primary)) {
		if (qos_arg.nq_use_constrained) {
			return 0;
		} else {
			*retval = RETURN_USE_BK;
			return 0;
		}
	}
	if (qos_arg.nq_transfer_size >= 5 * 1024 * 1024) {
		*retval = RETURN_USE_BK;
		return 0;
	}


#undef  RETURN_USE_BK
#undef  RETURN_USE_DEFAULT
	return 0;
}

#if (DEBUG || DEVELOPMENT)
/*
 * Customizable QoS mapping table
 * By default it uses the mapping table for RFC 4594
 *
 * Notes:
 *   BK_SYS is the same as BK
 *   CTL cannot be changed and is always _DSCP_CS6
 */
SYSCTL_NODE(_net_qos, OID_AUTO, custom,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "");

SYSCTL_NODE(_net_qos_custom, OID_AUTO, netsvctype_to_dscp,
    CTLFLAG_RW | CTLFLAG_LOCKED, 0, "");

static int sysctl_net_qos_custom_netsvctype_to_dscp SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, be,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_BE, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, bk,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_BK, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, sig,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_SIG, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, vi,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_VI, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, vo,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_VO, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, rv,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_RV, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, av,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_AV, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, oam,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_OAM, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");
SYSCTL_PROC(_net_qos_custom_netsvctype_to_dscp, OID_AUTO, rd,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, NET_SERVICE_TYPE_RD, sysctl_net_qos_custom_netsvctype_to_dscp, "I", "");

static int sysctl_net_qos_custom_reset SYSCTL_HANDLER_ARGS;
SYSCTL_PROC(_net_qos_custom, OID_AUTO, reset,
    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_LOCKED,
    0, 0, sysctl_net_qos_custom_reset, "I", "");

int
sysctl_net_qos_custom_netsvctype_to_dscp SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1)
	int error = 0;

	switch (arg2) {
	case NET_SERVICE_TYPE_BE:
	case NET_SERVICE_TYPE_BK:
	case NET_SERVICE_TYPE_SIG:
	case NET_SERVICE_TYPE_VI:
	case NET_SERVICE_TYPE_VO:
	case NET_SERVICE_TYPE_RV:
	case NET_SERVICE_TYPE_AV:
	case NET_SERVICE_TYPE_OAM:
	case NET_SERVICE_TYPE_RD:
		break;
	default:
		os_log(OS_LOG_DEFAULT, "%s: unexpected netsvctype %d",
		    __func__, arg2);
		return EINVAL;
	}

	int val = custom_net_qos_dscp_map.netsvctype_to_dscp[arg2];
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == USER_ADDR_NULL) {
		return error;
	}
	if (req->newptr == USER_ADDR_NULL) {
		return 0;
	}
	error = proc_suser(current_proc());
	if (error != 0) {
		return error;
	}
	if (val < 0 || val > _MAX_DSCP) {
		os_log(OS_LOG_DEFAULT, "%s: unexpected DSCP %d",
		    __func__, val);
		return EINVAL;
	}

	struct netsvctype_dscp_map netsvctype_dscp_map[_NET_SERVICE_TYPE_COUNT] = {};

	for (int i = 0; i < _NET_SERVICE_TYPE_COUNT; i++) {
		netsvctype_dscp_map[i].netsvctype = i;
		netsvctype_dscp_map[i].dscp = custom_net_qos_dscp_map.netsvctype_to_dscp[i];
	}
	netsvctype_dscp_map[arg2].dscp = (uint8_t) val;

	error = set_netsvctype_dscp_map(&custom_net_qos_dscp_map,
	    netsvctype_dscp_map);

	return 0;
}

int
sysctl_net_qos_custom_reset SYSCTL_HANDLER_ARGS
{
#pragma unused(arg1, arg2)
	int error = 0;
	int val = 0;

	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr) {
		return error;
	}
	if (req->newptr == USER_ADDR_NULL) {
		return 0;
	}
	error = proc_suser(current_proc());
	if (error != 0) {
		return error;
	}

	error = set_netsvctype_dscp_map(&custom_net_qos_dscp_map,
	    rfc4594_netsvctype_dscp_map);

	return error;
}

uint8_t
custom_sc_to_dscp(uint32_t svc_class)
{
	uint8_t dscp = _DSCP_DF;

	switch (svc_class) {
	case MBUF_SC_BK_SYS:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_BK_SYS];
		break;
	case MBUF_SC_BK:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_BK];
		break;

	case MBUF_SC_BE:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_BE];
		break;
	case MBUF_SC_RD:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_RD];
		break;
	case MBUF_SC_OAM:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_OAM];
		break;

	case MBUF_SC_AV:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_AV];
		break;
	case MBUF_SC_RV:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_RV];
		break;
	case MBUF_SC_VI:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_VI];
		break;
	case MBUF_SC_SIG:
		dscp = custom_net_qos_dscp_map.netsvctype_to_dscp[NET_SERVICE_TYPE_SIG];
		break;

	case MBUF_SC_VO:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_VO];
		break;
	case MBUF_SC_CTL:
		dscp = custom_net_qos_dscp_map.sotc_to_dscp[SOTCIX_CTL];
		break;
	default:
		break;
	}
	return dscp;
}
#endif /* (DEBUG || DEVELOPMENT) */
