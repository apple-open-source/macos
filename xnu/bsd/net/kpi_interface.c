/*
 * Copyright (c) 2004-2024 Apple Inc. All rights reserved.
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

#include "kpi_interface.h"

#include <sys/queue.h>
#include <sys/param.h>  /* for definition of NULL */
#include <kern/debug.h> /* for panic */
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/kern_event.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/kpi_mbuf.h>
#include <sys/mcache.h>
#include <sys/protosw.h>
#include <sys/syslog.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/dlil.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/if_arp.h>
#include <net/if_llreach.h>
#include <net/if_ether.h>
#include <net/net_api_stats.h>
#include <net/route.h>
#include <net/if_ports_used.h>
#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>
#include <kern/locks.h>
#include <kern/clock.h>
#include <sys/sockio.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/in_pcb.h>
#ifdef INET
#include <netinet/igmp_var.h>
#endif
#include <netinet6/mld6_var.h>
#include <netkey/key.h>
#include <stdbool.h>

#include "net/net_str_id.h"
#include <net/sockaddr_utils.h>

#if CONFIG_MACF
#include <sys/kauth.h>
#include <security/mac_framework.h>
#endif

#if SKYWALK
#include <skywalk/os_skywalk_private.h>
#include <skywalk/nexus/netif/nx_netif.h>
#endif /* SKYWALK */

extern uint64_t if_creation_generation_count;

#undef ifnet_allocate
errno_t ifnet_allocate(const struct ifnet_init_params *init,
    ifnet_t *ifp);

static errno_t ifnet_allocate_common(const struct ifnet_init_params *init,
    ifnet_t *ifp, bool is_internal);


#define TOUCHLASTCHANGE(__if_lastchange) {                              \
	(__if_lastchange)->tv_sec = (time_t)net_uptime();               \
	(__if_lastchange)->tv_usec = 0;                                 \
}

static errno_t ifnet_defrouter_llreachinfo(ifnet_t, sa_family_t,
    struct ifnet_llreach_info *);
static void ifnet_kpi_free(ifnet_t);
static errno_t ifnet_list_get_common(ifnet_family_t, boolean_t, ifnet_t *__counted_by(*count) *list,
    u_int32_t *count);
static errno_t ifnet_set_lladdr_internal(ifnet_t,
    const void *__sized_by(lladdr_len) lladdr, size_t lladdr_len,
    u_char, int);
static errno_t ifnet_awdl_check_eflags(ifnet_t, u_int32_t *, u_int32_t *);


/*
 * Temporary work around until we have real reference counting
 *
 * We keep the bits about calling dlil_if_release (which should be
 * called recycle) transparent by calling it from our if_free function
 * pointer. We have to keep the client's original detach function
 * somewhere so we can call it.
 */
static void
ifnet_kpi_free(ifnet_t ifp)
{
	if ((ifp->if_refflags & IFRF_EMBRYONIC) == 0) {
		ifnet_detached_func detach_func;

		detach_func = ifp->if_detach;
		if (detach_func != NULL) {
			(*detach_func)(ifp);
		}
	}

	ifnet_dispose(ifp);
}

errno_t
ifnet_allocate_common(const struct ifnet_init_params *init,
    ifnet_t *ifp, bool is_internal)
{
	struct ifnet_init_eparams einit;

	bzero(&einit, sizeof(einit));

	einit.ver               = IFNET_INIT_CURRENT_VERSION;
	einit.len               = sizeof(einit);
	einit.flags             = IFNET_INIT_LEGACY | IFNET_INIT_NX_NOAUTO;
	if (!is_internal) {
		einit.flags |= IFNET_INIT_ALLOC_KPI;
	}
	einit.uniqueid          = init->uniqueid;
	einit.uniqueid_len      = init->uniqueid_len;
	einit.name              = init->name;
	einit.unit              = init->unit;
	einit.family            = init->family;
	einit.type              = init->type;
	einit.output            = init->output;
	einit.demux             = init->demux;
	einit.add_proto         = init->add_proto;
	einit.del_proto         = init->del_proto;
	einit.check_multi       = init->check_multi;
	einit.framer            = init->framer;
	einit.softc             = init->softc;
	einit.ioctl             = init->ioctl;
	einit.set_bpf_tap       = init->set_bpf_tap;
	einit.detach            = init->detach;
	einit.event             = init->event;
	einit.broadcast_addr    = init->broadcast_addr;
	einit.broadcast_len     = init->broadcast_len;

	return ifnet_allocate_extended(&einit, ifp);
}

errno_t
ifnet_allocate_internal(const struct ifnet_init_params *init, ifnet_t *ifp)
{
	return ifnet_allocate_common(init, ifp, true);
}

errno_t
ifnet_allocate(const struct ifnet_init_params *init, ifnet_t *ifp)
{
	return ifnet_allocate_common(init, ifp, false);
}

static void
ifnet_set_broadcast_addr(ifnet_t ifp,
    const void *__sized_by(broadcast_len) broadcast_addr,
    u_int32_t broadcast_len)
{
	if (ifp->if_broadcast.length != 0) {
		kfree_data_counted_by(ifp->if_broadcast.ptr,
		    ifp->if_broadcast.length);
	}
	if (broadcast_len != 0 && broadcast_addr != NULL) {
		ifp->if_broadcast.ptr = kalloc_data(broadcast_len,
		    Z_WAITOK | Z_NOFAIL);
		ifp->if_broadcast.length = broadcast_len;
		bcopy(broadcast_addr, ifp->if_broadcast.ptr,
		    broadcast_len);
	}
}

errno_t
ifnet_allocate_extended(const struct ifnet_init_eparams *einit0,
    ifnet_t *interface)
{
#if SKYWALK
	ifnet_start_func ostart = NULL;
#endif /* SKYWALK */
	struct ifnet_init_eparams einit;
	ifnet_ref_t ifp = NULL;
	char if_xname[IFXNAMSIZ] = {0};
	int error;

	einit = *einit0;

	if (einit.ver != IFNET_INIT_CURRENT_VERSION ||
	    einit.len < sizeof(einit)) {
		return EINVAL;
	}

	if (einit.family == 0 || einit.name == NULL ||
	    strlen(einit.name) >= IFNAMSIZ ||
	    (einit.type & 0xFFFFFF00) != 0 || einit.type == 0) {
		return EINVAL;
	}

#if SKYWALK
	/* headroom must be a multiple of 8 bytes */
	if ((einit.tx_headroom & 0x7) != 0) {
		return EINVAL;
	}
	if ((einit.flags & IFNET_INIT_SKYWALK_NATIVE) == 0) {
		/*
		 * Currently Interface advisory reporting is supported only
		 * for skywalk interface.
		 */
		if ((einit.flags & IFNET_INIT_IF_ADV) != 0) {
			return EINVAL;
		}
	}
#endif /* SKYWALK */

	if (einit.flags & IFNET_INIT_LEGACY) {
#if SKYWALK
		if (einit.flags & IFNET_INIT_SKYWALK_NATIVE) {
			return EINVAL;
		}
#endif /* SKYWALK */
		if (einit.output == NULL ||
		    (einit.flags & IFNET_INIT_INPUT_POLL)) {
			return EINVAL;
		}
		einit.pre_enqueue = NULL;
		einit.start = NULL;
		einit.output_ctl = NULL;
		einit.output_sched_model = IFNET_SCHED_MODEL_NORMAL;
		einit.input_poll = NULL;
		einit.input_ctl = NULL;
	} else {
#if SKYWALK
		/*
		 * For native Skywalk drivers, steer all start requests
		 * to ifp_if_start() until the netif device adapter is
		 * fully activated, at which point we will point it to
		 * nx_netif_doorbell().
		 */
		if (einit.flags & IFNET_INIT_SKYWALK_NATIVE) {
			if (einit.start != NULL) {
				return EINVAL;
			}
			/* override output start callback */
			ostart = einit.start = ifp_if_start;
		} else {
			ostart = einit.start;
		}
#endif /* SKYWALK */
		if (einit.start == NULL) {
			return EINVAL;
		}

		einit.output = NULL;
		if (einit.output_sched_model >= IFNET_SCHED_MODEL_MAX) {
			return EINVAL;
		}

		if (einit.flags & IFNET_INIT_INPUT_POLL) {
			if (einit.input_poll == NULL || einit.input_ctl == NULL) {
				return EINVAL;
			}
		} else {
			einit.input_poll = NULL;
			einit.input_ctl = NULL;
		}
	}

	if (einit.type > UCHAR_MAX) {
		return EINVAL;
	}

	if (einit.unit > SHRT_MAX) {
		return EINVAL;
	}

	/* Initialize external name (name + unit) */
	snprintf(if_xname, sizeof(if_xname), "%s%d",
	    einit.name, einit.unit);

	if (einit.uniqueid == NULL) {
		einit.uniqueid_len = (uint32_t)strbuflen(if_xname);
		einit.uniqueid = if_xname;
	}

	error = dlil_if_acquire(einit.family, einit.uniqueid,
	    einit.uniqueid_len,
	    __unsafe_null_terminated_from_indexable(if_xname), &ifp);

	if (error == 0) {
		uint64_t br;

		/*
		 * Cast ifp->if_name as non const. dlil_if_acquire sets it up
		 * to point to storage of at least IFNAMSIZ bytes. It is safe
		 * to write to this.
		 */
		char *ifname = __unsafe_forge_bidi_indexable(char *, __DECONST(char *, ifp->if_name), IFNAMSIZ);
		const char *einit_name = __unsafe_forge_bidi_indexable(const char *, einit.name, IFNAMSIZ);
		strbufcpy(ifname, IFNAMSIZ, einit_name, IFNAMSIZ);
		ifp->if_type            = (u_char)einit.type;
		ifp->if_family          = einit.family;
		ifp->if_subfamily       = einit.subfamily;
		ifp->if_unit            = (short)einit.unit;
		ifp->if_output          = einit.output;
		ifp->if_pre_enqueue     = einit.pre_enqueue;
		ifp->if_start           = einit.start;
		ifp->if_output_ctl      = einit.output_ctl;
		ifp->if_output_sched_model = einit.output_sched_model;
		ifp->if_output_bw.eff_bw = einit.output_bw;
		ifp->if_output_bw.max_bw = einit.output_bw_max;
		ifp->if_output_lt.eff_lt = einit.output_lt;
		ifp->if_output_lt.max_lt = einit.output_lt_max;
		ifp->if_input_poll      = einit.input_poll;
		ifp->if_input_ctl       = einit.input_ctl;
		ifp->if_input_bw.eff_bw = einit.input_bw;
		ifp->if_input_bw.max_bw = einit.input_bw_max;
		ifp->if_input_lt.eff_lt = einit.input_lt;
		ifp->if_input_lt.max_lt = einit.input_lt_max;
		ifp->if_demux           = einit.demux;
		ifp->if_add_proto       = einit.add_proto;
		ifp->if_del_proto       = einit.del_proto;
		ifp->if_check_multi     = einit.check_multi;
		ifp->if_framer_legacy   = einit.framer;
		ifp->if_framer          = einit.framer_extended;
		ifp->if_softc           = einit.softc;
		ifp->if_ioctl           = einit.ioctl;
		ifp->if_set_bpf_tap     = einit.set_bpf_tap;
		ifp->if_free            = (einit.free != NULL) ? einit.free : ifnet_kpi_free;
		ifp->if_event           = einit.event;
		ifp->if_detach          = einit.detach;

		/* Initialize Network ID */
		ifp->network_id_len     = 0;
		bzero(&ifp->network_id, sizeof(ifp->network_id));

		/* Initialize external name (name + unit) */
		char *ifxname = __unsafe_forge_bidi_indexable(char *, __DECONST(char *, ifp->if_xname), IFXNAMSIZ);
		snprintf(ifxname, IFXNAMSIZ, "%s", if_xname);

		/*
		 * On embedded, framer() is already in the extended form;
		 * we simply use it as is, unless the caller specifies
		 * framer_extended() which will then override it.
		 *
		 * On non-embedded, framer() has long been exposed as part
		 * of the public KPI, and therefore its signature must
		 * remain the same (without the pre- and postpend length
		 * parameters.)  We special case ether_frameout, such that
		 * it gets mapped to its extended variant.  All other cases
		 * utilize the stub routine which will simply return zeroes
		 * for those new parameters.
		 *
		 * Internally, DLIL will only use the extended callback
		 * variant which is represented by if_framer.
		 */
#if !XNU_TARGET_OS_OSX
		if (ifp->if_framer == NULL && ifp->if_framer_legacy != NULL) {
			ifp->if_framer = ifp->if_framer_legacy;
		}
#else /* XNU_TARGET_OS_OSX */
		if (ifp->if_framer == NULL && ifp->if_framer_legacy != NULL) {
			if (ifp->if_framer_legacy == ether_frameout) {
				ifp->if_framer = ether_frameout_extended;
			} else {
				ifp->if_framer = ifnet_framer_stub;
			}
		}
#endif /* XNU_TARGET_OS_OSX */

		if (ifp->if_output_bw.eff_bw > ifp->if_output_bw.max_bw) {
			ifp->if_output_bw.max_bw = ifp->if_output_bw.eff_bw;
		} else if (ifp->if_output_bw.eff_bw == 0) {
			ifp->if_output_bw.eff_bw = ifp->if_output_bw.max_bw;
		}

		if (ifp->if_input_bw.eff_bw > ifp->if_input_bw.max_bw) {
			ifp->if_input_bw.max_bw = ifp->if_input_bw.eff_bw;
		} else if (ifp->if_input_bw.eff_bw == 0) {
			ifp->if_input_bw.eff_bw = ifp->if_input_bw.max_bw;
		}

		if (ifp->if_output_bw.max_bw == 0) {
			ifp->if_output_bw = ifp->if_input_bw;
		} else if (ifp->if_input_bw.max_bw == 0) {
			ifp->if_input_bw = ifp->if_output_bw;
		}

		/* Pin if_baudrate to 32 bits */
		br = MAX(ifp->if_output_bw.max_bw, ifp->if_input_bw.max_bw);
		if (br != 0) {
			ifp->if_baudrate = (br > UINT32_MAX) ? UINT32_MAX : (uint32_t)br;
		}

		if (ifp->if_output_lt.eff_lt > ifp->if_output_lt.max_lt) {
			ifp->if_output_lt.max_lt = ifp->if_output_lt.eff_lt;
		} else if (ifp->if_output_lt.eff_lt == 0) {
			ifp->if_output_lt.eff_lt = ifp->if_output_lt.max_lt;
		}

		if (ifp->if_input_lt.eff_lt > ifp->if_input_lt.max_lt) {
			ifp->if_input_lt.max_lt = ifp->if_input_lt.eff_lt;
		} else if (ifp->if_input_lt.eff_lt == 0) {
			ifp->if_input_lt.eff_lt = ifp->if_input_lt.max_lt;
		}

		if (ifp->if_output_lt.max_lt == 0) {
			ifp->if_output_lt = ifp->if_input_lt;
		} else if (ifp->if_input_lt.max_lt == 0) {
			ifp->if_input_lt = ifp->if_output_lt;
		}

		if (ifp->if_ioctl == NULL) {
			ifp->if_ioctl = ifp_if_ioctl;
		}

		if_clear_eflags(ifp, -1);
		if (ifp->if_start != NULL) {
			if_set_eflags(ifp, IFEF_TXSTART);
			if (ifp->if_pre_enqueue == NULL) {
				ifp->if_pre_enqueue = ifnet_enqueue;
			}
			ifp->if_output = ifp->if_pre_enqueue;
		}

		if (ifp->if_input_poll != NULL) {
			if_set_eflags(ifp, IFEF_RXPOLL);
		}

		ifp->if_output_dlil = dlil_output_handler;
		ifp->if_input_dlil = dlil_input_handler;

		VERIFY(!(einit.flags & IFNET_INIT_LEGACY) ||
		    (ifp->if_pre_enqueue == NULL && ifp->if_start == NULL &&
		    ifp->if_output_ctl == NULL && ifp->if_input_poll == NULL &&
		    ifp->if_input_ctl == NULL));
		VERIFY(!(einit.flags & IFNET_INIT_INPUT_POLL) ||
		    (ifp->if_input_poll != NULL && ifp->if_input_ctl != NULL));

		ifnet_set_broadcast_addr(ifp, einit.broadcast_addr,
		    einit.broadcast_len);

		if_clear_xflags(ifp, -1);
#if SKYWALK
		ifp->if_tx_headroom = 0;
		ifp->if_tx_trailer = 0;
		ifp->if_rx_mit_ival = 0;
		ifp->if_save_start = ostart;
		if (einit.flags & IFNET_INIT_SKYWALK_NATIVE) {
			VERIFY(ifp->if_eflags & IFEF_TXSTART);
			VERIFY(!(einit.flags & IFNET_INIT_LEGACY));
			if_set_eflags(ifp, IFEF_SKYWALK_NATIVE);
			ifp->if_tx_headroom = einit.tx_headroom;
			ifp->if_tx_trailer = einit.tx_trailer;
			ifp->if_rx_mit_ival = einit.rx_mit_ival;
			/*
			 * For native Skywalk drivers, make sure packets
			 * emitted by the BSD stack get dropped until the
			 * interface is in service.  When the netif host
			 * adapter is fully activated, we'll point it to
			 * nx_netif_output().
			 */
			ifp->if_output = ifp_if_output;
			/*
			 * Override driver-supplied parameters
			 * and force IFEF_ENQUEUE_MULTI?
			 */
			if (sk_netif_native_txmodel ==
			    NETIF_NATIVE_TXMODEL_ENQUEUE_MULTI) {
				einit.start_delay_qlen = sk_tx_delay_qlen;
				einit.start_delay_timeout = sk_tx_delay_timeout;
			}
			/* netif comes with native interfaces */
			VERIFY((ifp->if_xflags & IFXF_LEGACY) == 0);
		} else if (!ifnet_needs_compat(ifp)) {
			/*
			 * If we're told not to plumb in netif compat
			 * for this interface, set IFXF_NX_NOAUTO to
			 * prevent DLIL from auto-attaching the nexus.
			 */
			einit.flags |= IFNET_INIT_NX_NOAUTO;
			/* legacy (non-netif) interface */
			if_set_xflags(ifp, IFXF_LEGACY);
		}

		ifp->if_save_output = ifp->if_output;
		if ((einit.flags & IFNET_INIT_NX_NOAUTO) != 0) {
			if_set_xflags(ifp, IFXF_NX_NOAUTO);
		}
		if ((einit.flags & IFNET_INIT_IF_ADV) != 0) {
			if_set_eflags(ifp, IFEF_ADV_REPORT);
		}
#else /* !SKYWALK */
		/* legacy interface */
		if_set_xflags(ifp, IFXF_LEGACY);
#endif /* !SKYWALK */

		if ((ifp->if_snd = ifclassq_alloc()) == NULL) {
			panic_plain("%s: ifp=%p couldn't allocate class queues",
			    __func__, ifp);
			/* NOTREACHED */
		}

		/*
		 * output target queue delay is specified in millisecond
		 * convert it to nanoseconds
		 */
		IFCQ_TARGET_QDELAY(ifp->if_snd) =
		    einit.output_target_qdelay * 1000 * 1000;
		IFCQ_MAXLEN(ifp->if_snd) = einit.sndq_maxlen;

		ifnet_enqueue_multi_setup(ifp, einit.start_delay_qlen,
		    einit.start_delay_timeout);

		IFCQ_PKT_DROP_LIMIT(ifp->if_snd) = IFCQ_DEFAULT_PKT_DROP_LIMIT;

		/*
		 * Set embryonic flag; this will be cleared
		 * later when it is fully attached.
		 */
		ifp->if_refflags = IFRF_EMBRYONIC;

		/*
		 * Count the newly allocated ifnet
		 */
		OSIncrementAtomic64(&net_api_stats.nas_ifnet_alloc_count);
		INC_ATOMIC_INT64_LIM(net_api_stats.nas_ifnet_alloc_total);
		if ((einit.flags & IFNET_INIT_ALLOC_KPI) != 0) {
			if_set_xflags(ifp, IFXF_ALLOC_KPI);
		} else {
			OSIncrementAtomic64(
				&net_api_stats.nas_ifnet_alloc_os_count);
			INC_ATOMIC_INT64_LIM(
				net_api_stats.nas_ifnet_alloc_os_total);
		}

		if (ifp->if_subfamily == IFNET_SUBFAMILY_MANAGEMENT) {
			if_set_xflags(ifp, IFXF_MANAGEMENT);
			if_management_interface_check_needed = true;
		}

		/*
		 * Increment the generation count on interface creation
		 */
		ifp->if_creation_generation_id = os_atomic_inc(&if_creation_generation_count, relaxed);

		*interface = ifp;
	}
	return error;
}

errno_t
ifnet_reference(ifnet_t ifp)
{
	return dlil_if_ref(ifp);
}

void
ifnet_dispose(ifnet_t ifp)
{
	dlil_if_release(ifp);
}

errno_t
ifnet_release(ifnet_t ifp)
{
	return dlil_if_free(ifp);
}

errno_t
ifnet_interface_family_find(const char *module_string,
    ifnet_family_t *family_id)
{
	if (module_string == NULL || family_id == NULL) {
		return EINVAL;
	}

	return net_str_id_find_internal(module_string, family_id,
	           NSI_IF_FAM_ID, 1);
}

void *
ifnet_softc(ifnet_t interface)
{
	return (interface == NULL) ? NULL : interface->if_softc;
}

const char *
ifnet_name(ifnet_t interface)
{
	return (interface == NULL) ? NULL : interface->if_name;
}

ifnet_family_t
ifnet_family(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_family;
}

ifnet_subfamily_t
ifnet_subfamily(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_subfamily;
}

u_int32_t
ifnet_unit(ifnet_t interface)
{
	return (interface == NULL) ? (u_int32_t)0xffffffff :
	       (u_int32_t)interface->if_unit;
}

u_int32_t
ifnet_index(ifnet_t interface)
{
	return (interface == NULL) ? (u_int32_t)0xffffffff :
	       interface->if_index;
}

errno_t
ifnet_set_flags(ifnet_t interface, u_int16_t new_flags, u_int16_t mask)
{
	bool set_IFF_UP;
	bool change_IFF_UP;
	uint16_t old_flags;

	if (interface == NULL) {
		return EINVAL;
	}
	set_IFF_UP = (new_flags & IFF_UP) != 0;
	change_IFF_UP = (mask & IFF_UP) != 0;
#if SKYWALK
	if (set_IFF_UP && change_IFF_UP) {
		/*
		 * When a native skywalk interface is marked IFF_UP, ensure
		 * the flowswitch is attached.
		 */
		ifnet_attach_native_flowswitch(interface);
	}
#endif /* SKYWALK */

	ifnet_lock_exclusive(interface);

	/* If we are modifying the up/down state, call if_updown */
	if (change_IFF_UP) {
		if_updown(interface, set_IFF_UP);
	}

	old_flags = interface->if_flags;
	interface->if_flags = (new_flags & mask) | (interface->if_flags & ~mask);
	/* If we are modifying the multicast flag, set/unset the silent flag */
	if ((old_flags & IFF_MULTICAST) !=
	    (interface->if_flags & IFF_MULTICAST)) {
#if INET
		if (IGMP_IFINFO(interface) != NULL) {
			igmp_initsilent(interface, IGMP_IFINFO(interface));
		}
#endif /* INET */
		if (MLD_IFINFO(interface) != NULL) {
			mld6_initsilent(interface, MLD_IFINFO(interface));
		}
	}

	ifnet_lock_done(interface);

	return 0;
}

u_int16_t
ifnet_flags(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_flags;
}

/*
 * This routine ensures the following:
 *
 * If IFEF_AWDL is set by the caller, also set the rest of flags as
 * defined in IFEF_AWDL_MASK.
 *
 * If IFEF_AWDL has been set on the interface and the caller attempts
 * to clear one or more of the associated flags in IFEF_AWDL_MASK,
 * return failure.
 *
 * If IFEF_AWDL_RESTRICTED is set by the caller, make sure IFEF_AWDL is set
 * on the interface.
 *
 * All other flags not associated with AWDL are not affected.
 *
 * See <net/if.h> for current definition of IFEF_AWDL_MASK.
 */
static errno_t
ifnet_awdl_check_eflags(ifnet_t ifp, u_int32_t *new_eflags, u_int32_t *mask)
{
	u_int32_t eflags;

	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_EXCLUSIVE);

	eflags = (*new_eflags & *mask) | (ifp->if_eflags & ~(*mask));

	if (ifp->if_eflags & IFEF_AWDL) {
		if (eflags & IFEF_AWDL) {
			if ((eflags & IFEF_AWDL_MASK) != IFEF_AWDL_MASK) {
				return EINVAL;
			}
		} else {
			*new_eflags &= ~IFEF_AWDL_MASK;
			*mask |= IFEF_AWDL_MASK;
		}
	} else if (eflags & IFEF_AWDL) {
		*new_eflags |= IFEF_AWDL_MASK;
		*mask |= IFEF_AWDL_MASK;
	} else if (eflags & IFEF_AWDL_RESTRICTED &&
	    !(ifp->if_eflags & IFEF_AWDL)) {
		return EINVAL;
	}

	return 0;
}

errno_t
ifnet_set_eflags(ifnet_t interface, u_int32_t new_flags, u_int32_t mask)
{
	uint32_t oeflags;
	struct kev_msg ev_msg;
	struct net_event_data ev_data;

	if (interface == NULL) {
		return EINVAL;
	}

	bzero(&ev_msg, sizeof(ev_msg));
	ifnet_lock_exclusive(interface);
	/*
	 * Sanity checks for IFEF_AWDL and its related flags.
	 */
	if (ifnet_awdl_check_eflags(interface, &new_flags, &mask) != 0) {
		ifnet_lock_done(interface);
		return EINVAL;
	}
	/*
	 * Currently Interface advisory reporting is supported only for
	 * skywalk interface.
	 */
	if ((((new_flags & mask) & IFEF_ADV_REPORT) != 0) &&
	    ((interface->if_eflags & IFEF_SKYWALK_NATIVE) == 0)) {
		ifnet_lock_done(interface);
		return EINVAL;
	}
	oeflags = interface->if_eflags;
	if_clear_eflags(interface, mask);
	if (new_flags != 0) {
		if_set_eflags(interface, (new_flags & mask));
	}
	ifnet_lock_done(interface);
	if (interface->if_eflags & IFEF_AWDL_RESTRICTED &&
	    !(oeflags & IFEF_AWDL_RESTRICTED)) {
		ev_msg.event_code = KEV_DL_AWDL_RESTRICTED;
		/*
		 * The interface is now restricted to applications that have
		 * the entitlement.
		 * The check for the entitlement will be done in the data
		 * path, so we don't have to do anything here.
		 */
	} else if (oeflags & IFEF_AWDL_RESTRICTED &&
	    !(interface->if_eflags & IFEF_AWDL_RESTRICTED)) {
		ev_msg.event_code = KEV_DL_AWDL_UNRESTRICTED;
	}
	/*
	 * Notify configd so that it has a chance to perform better
	 * reachability detection.
	 */
	if (ev_msg.event_code) {
		bzero(&ev_data, sizeof(ev_data));
		ev_msg.vendor_code = KEV_VENDOR_APPLE;
		ev_msg.kev_class = KEV_NETWORK_CLASS;
		ev_msg.kev_subclass = KEV_DL_SUBCLASS;
		strlcpy(ev_data.if_name, interface->if_name, IFNAMSIZ);
		ev_data.if_family = interface->if_family;
		ev_data.if_unit = interface->if_unit;
		ev_msg.dv[0].data_length = sizeof(struct net_event_data);
		ev_msg.dv[0].data_ptr = &ev_data;
		ev_msg.dv[1].data_length = 0;
		dlil_post_complete_msg(interface, &ev_msg);
	}

	return 0;
}

u_int32_t
ifnet_eflags(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_eflags;
}

errno_t
ifnet_set_idle_flags_locked(ifnet_t ifp, u_int32_t new_flags, u_int32_t mask)
{
	if (ifp == NULL) {
		return EINVAL;
	}
	ifnet_lock_assert(ifp, IFNET_LCK_ASSERT_EXCLUSIVE);

	/*
	 * If this is called prior to ifnet attach, the actual work will
	 * be done at attach time.  Otherwise, if it is called after
	 * ifnet detach, then it is a no-op.
	 */
	if (!ifnet_is_attached(ifp, 0)) {
		ifp->if_idle_new_flags = new_flags;
		ifp->if_idle_new_flags_mask = mask;
		return 0;
	} else {
		ifp->if_idle_new_flags = ifp->if_idle_new_flags_mask = 0;
	}

	ifp->if_idle_flags = (new_flags & mask) | (ifp->if_idle_flags & ~mask);
	return 0;
}

errno_t
ifnet_set_idle_flags(ifnet_t ifp, u_int32_t new_flags, u_int32_t mask)
{
	errno_t err;

	ifnet_lock_exclusive(ifp);
	err = ifnet_set_idle_flags_locked(ifp, new_flags, mask);
	ifnet_lock_done(ifp);

	return err;
}

u_int32_t
ifnet_idle_flags(ifnet_t ifp)
{
	return (ifp == NULL) ? 0 : ifp->if_idle_flags;
}

errno_t
ifnet_set_link_quality(ifnet_t ifp, int quality)
{
	errno_t err = 0;

	if (ifp == NULL || quality < IFNET_LQM_MIN || quality > IFNET_LQM_MAX) {
		err = EINVAL;
		goto done;
	}

	if (!ifnet_is_attached(ifp, 0)) {
		err = ENXIO;
		goto done;
	}

	if_lqm_update(ifp, quality, 0);

done:
	return err;
}

int
ifnet_link_quality(ifnet_t ifp)
{
	int lqm;

	if (ifp == NULL) {
		return IFNET_LQM_THRESH_OFF;
	}

	ifnet_lock_shared(ifp);
	lqm = ifp->if_interface_state.lqm_state;
	ifnet_lock_done(ifp);

	return lqm;
}

errno_t
ifnet_set_interface_state(ifnet_t ifp,
    struct if_interface_state *if_interface_state)
{
	errno_t err = 0;

	if (ifp == NULL || if_interface_state == NULL) {
		err = EINVAL;
		goto done;
	}

	if (!ifnet_is_attached(ifp, 0)) {
		err = ENXIO;
		goto done;
	}

	if_state_update(ifp, if_interface_state);

done:
	return err;
}

errno_t
ifnet_get_interface_state(ifnet_t ifp,
    struct if_interface_state *if_interface_state)
{
	errno_t err = 0;

	if (ifp == NULL || if_interface_state == NULL) {
		err = EINVAL;
		goto done;
	}

	if (!ifnet_is_attached(ifp, 0)) {
		err = ENXIO;
		goto done;
	}

	if_get_state(ifp, if_interface_state);

done:
	return err;
}


static errno_t
ifnet_defrouter_llreachinfo(ifnet_t ifp, sa_family_t af,
    struct ifnet_llreach_info *iflri)
{
	if (ifp == NULL || iflri == NULL) {
		return EINVAL;
	}

	VERIFY(af == AF_INET || af == AF_INET6);

	return ifnet_llreach_get_defrouter(ifp, af, iflri);
}

errno_t
ifnet_inet_defrouter_llreachinfo(ifnet_t ifp, struct ifnet_llreach_info *iflri)
{
	return ifnet_defrouter_llreachinfo(ifp, AF_INET, iflri);
}

errno_t
ifnet_inet6_defrouter_llreachinfo(ifnet_t ifp, struct ifnet_llreach_info *iflri)
{
	return ifnet_defrouter_llreachinfo(ifp, AF_INET6, iflri);
}

errno_t
ifnet_set_capabilities_supported(ifnet_t ifp, u_int32_t new_caps,
    u_int32_t mask)
{
	errno_t error = 0;
	int tmp;

	if (ifp == NULL) {
		return EINVAL;
	}

	ifnet_lock_exclusive(ifp);
	tmp = (new_caps & mask) | (ifp->if_capabilities & ~mask);
	if ((tmp & ~IFCAP_VALID)) {
		error = EINVAL;
	} else {
		ifp->if_capabilities = tmp;
	}
	ifnet_lock_done(ifp);

	return error;
}

u_int32_t
ifnet_capabilities_supported(ifnet_t ifp)
{
	return (ifp == NULL) ? 0 : ifp->if_capabilities;
}


errno_t
ifnet_set_capabilities_enabled(ifnet_t ifp, u_int32_t new_caps,
    u_int32_t mask)
{
	errno_t error = 0;
	int tmp;
	struct kev_msg ev_msg;
	struct net_event_data ev_data;

	if (ifp == NULL) {
		return EINVAL;
	}

	ifnet_lock_exclusive(ifp);
	tmp = (new_caps & mask) | (ifp->if_capenable & ~mask);
	if ((tmp & ~IFCAP_VALID) || (tmp & ~ifp->if_capabilities)) {
		error = EINVAL;
	} else {
		ifp->if_capenable = tmp;
	}
	ifnet_lock_done(ifp);

	/* Notify application of the change */
	bzero(&ev_data, sizeof(struct net_event_data));
	bzero(&ev_msg, sizeof(struct kev_msg));
	ev_msg.vendor_code      = KEV_VENDOR_APPLE;
	ev_msg.kev_class        = KEV_NETWORK_CLASS;
	ev_msg.kev_subclass     = KEV_DL_SUBCLASS;

	ev_msg.event_code       = KEV_DL_IFCAP_CHANGED;
	strlcpy(&ev_data.if_name[0], ifp->if_name, IFNAMSIZ);
	ev_data.if_family       = ifp->if_family;
	ev_data.if_unit         = (u_int32_t)ifp->if_unit;
	ev_msg.dv[0].data_length = sizeof(struct net_event_data);
	ev_msg.dv[0].data_ptr = &ev_data;
	ev_msg.dv[1].data_length = 0;
	dlil_post_complete_msg(ifp, &ev_msg);

	return error;
}

u_int32_t
ifnet_capabilities_enabled(ifnet_t ifp)
{
	return (ifp == NULL) ? 0 : ifp->if_capenable;
}

static const ifnet_offload_t offload_mask =
    (IFNET_CSUM_IP | IFNET_CSUM_TCP | IFNET_CSUM_UDP | IFNET_CSUM_FRAGMENT |
    IFNET_IP_FRAGMENT | IFNET_CSUM_TCPIPV6 | IFNET_CSUM_UDPIPV6 |
    IFNET_IPV6_FRAGMENT | IFNET_CSUM_PARTIAL | IFNET_CSUM_ZERO_INVERT |
    IFNET_VLAN_TAGGING | IFNET_VLAN_MTU | IFNET_MULTIPAGES |
    IFNET_TSO_IPV4 | IFNET_TSO_IPV6 | IFNET_TX_STATUS | IFNET_HW_TIMESTAMP |
    IFNET_SW_TIMESTAMP);

static const ifnet_offload_t any_offload_csum = IFNET_CHECKSUMF;

static errno_t
ifnet_set_offload_common(ifnet_t interface, ifnet_offload_t offload, boolean_t set_both)
{
	u_int32_t ifcaps = 0;

	if (interface == NULL) {
		return EINVAL;
	}

	ifnet_lock_exclusive(interface);
	interface->if_hwassist = (offload & offload_mask);

#if SKYWALK
	/* preserve skywalk capability */
	if ((interface->if_capabilities & IFCAP_SKYWALK) != 0) {
		ifcaps |= IFCAP_SKYWALK;
	}
#endif /* SKYWALK */
	if (dlil_verbose) {
		log(LOG_DEBUG, "%s: set offload flags=0x%x\n",
		    if_name(interface),
		    interface->if_hwassist);
	}
	ifnet_lock_done(interface);

	if ((offload & any_offload_csum)) {
		ifcaps |= IFCAP_HWCSUM;
	}
	if ((offload & IFNET_TSO_IPV4)) {
		ifcaps |= IFCAP_TSO4;
	}
	if ((offload & IFNET_TSO_IPV6)) {
		ifcaps |= IFCAP_TSO6;
	}
	if ((offload & IFNET_LRO)) {
		ifcaps |= IFCAP_LRO;
	}
	if ((offload & IFNET_VLAN_MTU)) {
		ifcaps |= IFCAP_VLAN_MTU;
	}
	if ((offload & IFNET_VLAN_TAGGING)) {
		ifcaps |= IFCAP_VLAN_HWTAGGING;
	}
	if ((offload & IFNET_TX_STATUS)) {
		ifcaps |= IFCAP_TXSTATUS;
	}
	if ((offload & IFNET_HW_TIMESTAMP)) {
		ifcaps |= IFCAP_HW_TIMESTAMP;
	}
	if ((offload & IFNET_SW_TIMESTAMP)) {
		ifcaps |= IFCAP_SW_TIMESTAMP;
	}
	if ((offload & IFNET_CSUM_PARTIAL)) {
		ifcaps |= IFCAP_CSUM_PARTIAL;
	}
	if ((offload & IFNET_CSUM_ZERO_INVERT)) {
		ifcaps |= IFCAP_CSUM_ZERO_INVERT;
	}
	if (ifcaps != 0) {
		if (set_both) {
			(void) ifnet_set_capabilities_supported(interface,
			    ifcaps, IFCAP_VALID);
		}
		(void) ifnet_set_capabilities_enabled(interface, ifcaps,
		    IFCAP_VALID);
	}

	return 0;
}

errno_t
ifnet_set_offload(ifnet_t interface, ifnet_offload_t offload)
{
	return ifnet_set_offload_common(interface, offload, TRUE);
}

errno_t
ifnet_set_offload_enabled(ifnet_t interface, ifnet_offload_t offload)
{
	return ifnet_set_offload_common(interface, offload, FALSE);
}

ifnet_offload_t
ifnet_offload(ifnet_t interface)
{
	return (interface == NULL) ?
	       0 : (interface->if_hwassist & offload_mask);
}

errno_t
ifnet_set_tso_mtu(ifnet_t interface, sa_family_t family, u_int32_t mtuLen)
{
	errno_t error = 0;

	if (interface == NULL || mtuLen < interface->if_mtu) {
		return EINVAL;
	}
	if (mtuLen > IP_MAXPACKET) {
		return EINVAL;
	}

	switch (family) {
	case AF_INET:
		if (interface->if_hwassist & IFNET_TSO_IPV4) {
			interface->if_tso_v4_mtu = mtuLen;
		} else {
			error = EINVAL;
		}
		break;

	case AF_INET6:
		if (interface->if_hwassist & IFNET_TSO_IPV6) {
			interface->if_tso_v6_mtu = mtuLen;
		} else {
			error = EINVAL;
		}
		break;

	default:
		error = EPROTONOSUPPORT;
		break;
	}

	if (error == 0) {
		struct ifclassq *ifq = interface->if_snd;
		ASSERT(ifq != NULL);
		/* Inform all transmit queues about the new TSO MTU */
		IFCQ_LOCK(ifq);
		ifnet_update_sndq(ifq, CLASSQ_EV_LINK_MTU);
		IFCQ_UNLOCK(ifq);
	}

	return error;
}

errno_t
ifnet_get_tso_mtu(ifnet_t interface, sa_family_t family, u_int32_t *mtuLen)
{
	errno_t error = 0;

	if (interface == NULL || mtuLen == NULL) {
		return EINVAL;
	}

	switch (family) {
	case AF_INET:
		if (interface->if_hwassist & IFNET_TSO_IPV4) {
			*mtuLen = interface->if_tso_v4_mtu;
		} else {
			error = EINVAL;
		}
		break;

	case AF_INET6:
		if (interface->if_hwassist & IFNET_TSO_IPV6) {
			*mtuLen = interface->if_tso_v6_mtu;
		} else {
			error = EINVAL;
		}
		break;

	default:
		error = EPROTONOSUPPORT;
		break;
	}

	return error;
}

errno_t
ifnet_set_wake_flags(ifnet_t interface, u_int32_t properties, u_int32_t mask)
{
	struct kev_msg ev_msg;
	struct net_event_data ev_data;

	bzero(&ev_data, sizeof(struct net_event_data));
	bzero(&ev_msg, sizeof(struct kev_msg));

	if (interface == NULL) {
		return EINVAL;
	}

	/* Do not accept wacky values */
	if ((properties & mask) & ~IF_WAKE_VALID_FLAGS) {
		return EINVAL;
	}

	if ((mask & IF_WAKE_ON_MAGIC_PACKET) != 0) {
		if ((properties & IF_WAKE_ON_MAGIC_PACKET) != 0) {
			if_set_xflags(interface, IFXF_WAKE_ON_MAGIC_PACKET);
		} else {
			if_clear_xflags(interface, IFXF_WAKE_ON_MAGIC_PACKET);
		}
	}

	(void) ifnet_touch_lastchange(interface);

	/* Notify application of the change */
	ev_msg.vendor_code      = KEV_VENDOR_APPLE;
	ev_msg.kev_class        = KEV_NETWORK_CLASS;
	ev_msg.kev_subclass     = KEV_DL_SUBCLASS;

	ev_msg.event_code       = KEV_DL_WAKEFLAGS_CHANGED;
	strlcpy(&ev_data.if_name[0], interface->if_name, IFNAMSIZ);
	ev_data.if_family       = interface->if_family;
	ev_data.if_unit         = (u_int32_t)interface->if_unit;
	ev_msg.dv[0].data_length = sizeof(struct net_event_data);
	ev_msg.dv[0].data_ptr   = &ev_data;
	ev_msg.dv[1].data_length = 0;
	dlil_post_complete_msg(interface, &ev_msg);

	return 0;
}

u_int32_t
ifnet_get_wake_flags(ifnet_t interface)
{
	u_int32_t flags = 0;

	if (interface == NULL) {
		return 0;
	}

	if ((interface->if_xflags & IFXF_WAKE_ON_MAGIC_PACKET) != 0) {
		flags |= IF_WAKE_ON_MAGIC_PACKET;
	}

	return flags;
}

/*
 * Should MIB data store a copy?
 */
errno_t
ifnet_set_link_mib_data(ifnet_t interface, void *__sized_by(mibLen) mibData, uint32_t mibLen)
{
	if (interface == NULL) {
		return EINVAL;
	}

	ifnet_lock_exclusive(interface);
	interface->if_linkmib = (void*)mibData;
	interface->if_linkmiblen = mibLen;
	ifnet_lock_done(interface);
	return 0;
}

errno_t
ifnet_get_link_mib_data(ifnet_t interface, void *__sized_by(*mibLen) mibData, uint32_t *mibLen)
{
	errno_t result = 0;

	if (interface == NULL) {
		return EINVAL;
	}

	ifnet_lock_shared(interface);
	if (*mibLen < interface->if_linkmiblen) {
		result = EMSGSIZE;
	}
	if (result == 0 && interface->if_linkmib == NULL) {
		result = ENOTSUP;
	}

	if (result == 0) {
		*mibLen = interface->if_linkmiblen;
		bcopy(interface->if_linkmib, mibData, *mibLen);
	}
	ifnet_lock_done(interface);

	return result;
}

uint32_t
ifnet_get_link_mib_data_length(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_linkmiblen;
}

errno_t
ifnet_output(ifnet_t interface, protocol_family_t protocol_family,
    mbuf_t m, void *route, const struct sockaddr *dest)
{
	if (interface == NULL || protocol_family == 0 || m == NULL) {
		if (m != NULL) {
			mbuf_freem_list(m);
		}
		return EINVAL;
	}
	return dlil_output(interface, protocol_family, m, route, dest,
	           DLIL_OUTPUT_FLAGS_NONE, NULL);
}

errno_t
ifnet_output_raw(ifnet_t interface, protocol_family_t protocol_family, mbuf_t m)
{
	if (interface == NULL || m == NULL) {
		if (m != NULL) {
			mbuf_freem_list(m);
		}
		return EINVAL;
	}
	return dlil_output(interface, protocol_family, m, NULL, NULL,
	           DLIL_OUTPUT_FLAGS_RAW, NULL);
}

errno_t
ifnet_set_mtu(ifnet_t interface, u_int32_t mtu)
{
	if (interface == NULL) {
		return EINVAL;
	}

	interface->if_mtu = mtu;
	return 0;
}

u_int32_t
ifnet_mtu(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_mtu;
}

u_char
ifnet_type(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_data.ifi_type;
}

errno_t
ifnet_set_addrlen(ifnet_t interface, u_char addrlen)
{
	if (interface == NULL) {
		return EINVAL;
	}

	interface->if_data.ifi_addrlen = addrlen;
	return 0;
}

u_char
ifnet_addrlen(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_data.ifi_addrlen;
}

errno_t
ifnet_set_hdrlen(ifnet_t interface, u_char hdrlen)
{
	if (interface == NULL) {
		return EINVAL;
	}

	interface->if_data.ifi_hdrlen = hdrlen;
	return 0;
}

u_char
ifnet_hdrlen(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_data.ifi_hdrlen;
}

errno_t
ifnet_set_metric(ifnet_t interface, u_int32_t metric)
{
	if (interface == NULL) {
		return EINVAL;
	}

	interface->if_data.ifi_metric = metric;
	return 0;
}

u_int32_t
ifnet_metric(ifnet_t interface)
{
	return (interface == NULL) ? 0 : interface->if_data.ifi_metric;
}

errno_t
ifnet_set_baudrate(struct ifnet *ifp, uint64_t baudrate)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	ifp->if_output_bw.max_bw = ifp->if_input_bw.max_bw =
	    ifp->if_output_bw.eff_bw = ifp->if_input_bw.eff_bw = baudrate;

	/* Pin if_baudrate to 32 bits until we can change the storage size */
	ifp->if_baudrate = (baudrate > UINT32_MAX) ? UINT32_MAX : (uint32_t)baudrate;

	return 0;
}

u_int64_t
ifnet_baudrate(struct ifnet *ifp)
{
	return (ifp == NULL) ? 0 : ifp->if_baudrate;
}

errno_t
ifnet_set_bandwidths(struct ifnet *ifp, struct if_bandwidths *output_bw,
    struct if_bandwidths *input_bw)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	/* set input values first (if any), as output values depend on them */
	if (input_bw != NULL) {
		(void) ifnet_set_input_bandwidths(ifp, input_bw);
	}

	if (output_bw != NULL) {
		(void) ifnet_set_output_bandwidths(ifp, output_bw, FALSE);
	}

	return 0;
}

static void
ifnet_set_link_status_outbw(struct ifnet *ifp)
{
	struct if_wifi_status_v1 *sr;
	sr = &ifp->if_link_status->ifsr_u.ifsr_wifi.if_wifi_u.if_status_v1;
	if (ifp->if_output_bw.eff_bw != 0) {
		sr->valid_bitmask |=
		    IF_WIFI_UL_EFFECTIVE_BANDWIDTH_VALID;
		sr->ul_effective_bandwidth =
		    ifp->if_output_bw.eff_bw > UINT32_MAX ?
		    UINT32_MAX :
		    (uint32_t)ifp->if_output_bw.eff_bw;
	}
	if (ifp->if_output_bw.max_bw != 0) {
		sr->valid_bitmask |=
		    IF_WIFI_UL_MAX_BANDWIDTH_VALID;
		sr->ul_max_bandwidth =
		    ifp->if_output_bw.max_bw > UINT32_MAX ?
		    UINT32_MAX :
		    (uint32_t)ifp->if_output_bw.max_bw;
	}
}

errno_t
ifnet_set_output_bandwidths(struct ifnet *ifp, struct if_bandwidths *bw,
    boolean_t locked)
{
	struct if_bandwidths old_bw;
	struct ifclassq *ifq;
	u_int64_t br;

	VERIFY(ifp != NULL && bw != NULL);

	ifq = ifp->if_snd;
	if (!locked) {
		IFCQ_LOCK(ifq);
	}
	IFCQ_LOCK_ASSERT_HELD(ifq);

	old_bw = ifp->if_output_bw;
	if (bw->eff_bw != 0) {
		ifp->if_output_bw.eff_bw = bw->eff_bw;
	}
	if (bw->max_bw != 0) {
		ifp->if_output_bw.max_bw = bw->max_bw;
	}
	if (ifp->if_output_bw.eff_bw > ifp->if_output_bw.max_bw) {
		ifp->if_output_bw.max_bw = ifp->if_output_bw.eff_bw;
	} else if (ifp->if_output_bw.eff_bw == 0) {
		ifp->if_output_bw.eff_bw = ifp->if_output_bw.max_bw;
	}

	/* Pin if_baudrate to 32 bits */
	br = MAX(ifp->if_output_bw.max_bw, ifp->if_input_bw.max_bw);
	if (br != 0) {
		ifp->if_baudrate = (br > UINT32_MAX) ? UINT32_MAX : (uint32_t)br;
	}

	/* Adjust queue parameters if needed */
	if (old_bw.eff_bw != ifp->if_output_bw.eff_bw ||
	    old_bw.max_bw != ifp->if_output_bw.max_bw) {
		ifnet_update_sndq(ifq, CLASSQ_EV_LINK_BANDWIDTH);
	}

	if (!locked) {
		IFCQ_UNLOCK(ifq);
	}

	/*
	 * If this is a Wifi interface, update the values in
	 * if_link_status structure also.
	 */
	if (IFNET_IS_WIFI(ifp) && ifp->if_link_status != NULL) {
		lck_rw_lock_exclusive(&ifp->if_link_status_lock);
		ifnet_set_link_status_outbw(ifp);
		lck_rw_done(&ifp->if_link_status_lock);
	}

	return 0;
}

static void
ifnet_set_link_status_inbw(struct ifnet *ifp)
{
	struct if_wifi_status_v1 *sr;

	sr = &ifp->if_link_status->ifsr_u.ifsr_wifi.if_wifi_u.if_status_v1;
	if (ifp->if_input_bw.eff_bw != 0) {
		sr->valid_bitmask |=
		    IF_WIFI_DL_EFFECTIVE_BANDWIDTH_VALID;
		sr->dl_effective_bandwidth =
		    ifp->if_input_bw.eff_bw > UINT32_MAX ?
		    UINT32_MAX :
		    (uint32_t)ifp->if_input_bw.eff_bw;
	}
	if (ifp->if_input_bw.max_bw != 0) {
		sr->valid_bitmask |=
		    IF_WIFI_DL_MAX_BANDWIDTH_VALID;
		sr->dl_max_bandwidth = ifp->if_input_bw.max_bw > UINT32_MAX ?
		    UINT32_MAX :
		    (uint32_t)ifp->if_input_bw.max_bw;
	}
}

errno_t
ifnet_set_input_bandwidths(struct ifnet *ifp, struct if_bandwidths *bw)
{
	struct if_bandwidths old_bw;

	VERIFY(ifp != NULL && bw != NULL);

	old_bw = ifp->if_input_bw;
	if (bw->eff_bw != 0) {
		ifp->if_input_bw.eff_bw = bw->eff_bw;
	}
	if (bw->max_bw != 0) {
		ifp->if_input_bw.max_bw = bw->max_bw;
	}
	if (ifp->if_input_bw.eff_bw > ifp->if_input_bw.max_bw) {
		ifp->if_input_bw.max_bw = ifp->if_input_bw.eff_bw;
	} else if (ifp->if_input_bw.eff_bw == 0) {
		ifp->if_input_bw.eff_bw = ifp->if_input_bw.max_bw;
	}

	if (IFNET_IS_WIFI(ifp) && ifp->if_link_status != NULL) {
		lck_rw_lock_exclusive(&ifp->if_link_status_lock);
		ifnet_set_link_status_inbw(ifp);
		lck_rw_done(&ifp->if_link_status_lock);
	}

	if (old_bw.eff_bw != ifp->if_input_bw.eff_bw ||
	    old_bw.max_bw != ifp->if_input_bw.max_bw) {
		ifnet_update_rcv(ifp, CLASSQ_EV_LINK_BANDWIDTH);
	}

	return 0;
}

u_int64_t
ifnet_output_linkrate(struct ifnet *ifp)
{
	struct ifclassq *ifq = ifp->if_snd;
	u_int64_t rate;

	IFCQ_LOCK_ASSERT_HELD(ifq);

	rate = ifp->if_output_bw.eff_bw;
	if (IFCQ_TBR_IS_ENABLED(ifq)) {
		u_int64_t tbr_rate = ifq->ifcq_tbr.tbr_rate_raw;
		VERIFY(tbr_rate > 0);
		rate = MIN(rate, ifq->ifcq_tbr.tbr_rate_raw);
	}

	return rate;
}

u_int64_t
ifnet_input_linkrate(struct ifnet *ifp)
{
	return ifp->if_input_bw.eff_bw;
}

errno_t
ifnet_bandwidths(struct ifnet *ifp, struct if_bandwidths *output_bw,
    struct if_bandwidths *input_bw)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	if (output_bw != NULL) {
		*output_bw = ifp->if_output_bw;
	}
	if (input_bw != NULL) {
		*input_bw = ifp->if_input_bw;
	}

	return 0;
}

errno_t
ifnet_set_latencies(struct ifnet *ifp, struct if_latencies *output_lt,
    struct if_latencies *input_lt)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	if (output_lt != NULL) {
		(void) ifnet_set_output_latencies(ifp, output_lt, FALSE);
	}

	if (input_lt != NULL) {
		(void) ifnet_set_input_latencies(ifp, input_lt);
	}

	return 0;
}

errno_t
ifnet_set_output_latencies(struct ifnet *ifp, struct if_latencies *lt,
    boolean_t locked)
{
	struct if_latencies old_lt;
	struct ifclassq *ifq;

	VERIFY(ifp != NULL && lt != NULL);

	ifq = ifp->if_snd;
	if (!locked) {
		IFCQ_LOCK(ifq);
	}
	IFCQ_LOCK_ASSERT_HELD(ifq);

	old_lt = ifp->if_output_lt;
	if (lt->eff_lt != 0) {
		ifp->if_output_lt.eff_lt = lt->eff_lt;
	}
	if (lt->max_lt != 0) {
		ifp->if_output_lt.max_lt = lt->max_lt;
	}
	if (ifp->if_output_lt.eff_lt > ifp->if_output_lt.max_lt) {
		ifp->if_output_lt.max_lt = ifp->if_output_lt.eff_lt;
	} else if (ifp->if_output_lt.eff_lt == 0) {
		ifp->if_output_lt.eff_lt = ifp->if_output_lt.max_lt;
	}

	/* Adjust queue parameters if needed */
	if (old_lt.eff_lt != ifp->if_output_lt.eff_lt ||
	    old_lt.max_lt != ifp->if_output_lt.max_lt) {
		ifnet_update_sndq(ifq, CLASSQ_EV_LINK_LATENCY);
	}

	if (!locked) {
		IFCQ_UNLOCK(ifq);
	}

	return 0;
}

errno_t
ifnet_set_input_latencies(struct ifnet *ifp, struct if_latencies *lt)
{
	struct if_latencies old_lt;

	VERIFY(ifp != NULL && lt != NULL);

	old_lt = ifp->if_input_lt;
	if (lt->eff_lt != 0) {
		ifp->if_input_lt.eff_lt = lt->eff_lt;
	}
	if (lt->max_lt != 0) {
		ifp->if_input_lt.max_lt = lt->max_lt;
	}
	if (ifp->if_input_lt.eff_lt > ifp->if_input_lt.max_lt) {
		ifp->if_input_lt.max_lt = ifp->if_input_lt.eff_lt;
	} else if (ifp->if_input_lt.eff_lt == 0) {
		ifp->if_input_lt.eff_lt = ifp->if_input_lt.max_lt;
	}

	if (old_lt.eff_lt != ifp->if_input_lt.eff_lt ||
	    old_lt.max_lt != ifp->if_input_lt.max_lt) {
		ifnet_update_rcv(ifp, CLASSQ_EV_LINK_LATENCY);
	}

	return 0;
}

errno_t
ifnet_latencies(struct ifnet *ifp, struct if_latencies *output_lt,
    struct if_latencies *input_lt)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	if (output_lt != NULL) {
		*output_lt = ifp->if_output_lt;
	}
	if (input_lt != NULL) {
		*input_lt = ifp->if_input_lt;
	}

	return 0;
}

errno_t
ifnet_set_poll_params(struct ifnet *ifp, struct ifnet_poll_params *p)
{
	errno_t err;

	if (ifp == NULL) {
		return EINVAL;
	} else if (!ifnet_is_attached(ifp, 1)) {
		return ENXIO;
	}

#if SKYWALK
	if (SKYWALK_CAPABLE(ifp)) {
		err = netif_rxpoll_set_params(ifp, p, FALSE);
		ifnet_decr_iorefcnt(ifp);
		return err;
	}
#endif /* SKYWALK */
	err = dlil_rxpoll_set_params(ifp, p, FALSE);

	/* Release the io ref count */
	ifnet_decr_iorefcnt(ifp);

	return err;
}

errno_t
ifnet_poll_params(struct ifnet *ifp, struct ifnet_poll_params *p)
{
	errno_t err;

	if (ifp == NULL || p == NULL) {
		return EINVAL;
	} else if (!ifnet_is_attached(ifp, 1)) {
		return ENXIO;
	}

	err = dlil_rxpoll_get_params(ifp, p);

	/* Release the io ref count */
	ifnet_decr_iorefcnt(ifp);

	return err;
}

errno_t
ifnet_stat_increment(struct ifnet *ifp,
    const struct ifnet_stat_increment_param *s)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	if (s->packets_in != 0) {
		os_atomic_add(&ifp->if_data.ifi_ipackets, s->packets_in, relaxed);
	}
	if (s->bytes_in != 0) {
		os_atomic_add(&ifp->if_data.ifi_ibytes, s->bytes_in, relaxed);
	}
	if (s->errors_in != 0) {
		os_atomic_add(&ifp->if_data.ifi_ierrors, s->errors_in, relaxed);
	}

	if (s->packets_out != 0) {
		os_atomic_add(&ifp->if_data.ifi_opackets, s->packets_out, relaxed);
	}
	if (s->bytes_out != 0) {
		os_atomic_add(&ifp->if_data.ifi_obytes, s->bytes_out, relaxed);
	}
	if (s->errors_out != 0) {
		os_atomic_add(&ifp->if_data.ifi_oerrors, s->errors_out, relaxed);
	}

	if (s->collisions != 0) {
		os_atomic_add(&ifp->if_data.ifi_collisions, s->collisions, relaxed);
	}
	if (s->dropped != 0) {
		os_atomic_add(&ifp->if_data.ifi_iqdrops, s->dropped, relaxed);
	}

	/* Touch the last change time. */
	TOUCHLASTCHANGE(&ifp->if_lastchange);

	if (ifp->if_data_threshold != 0) {
		ifnet_notify_data_threshold(ifp);
	}

	return 0;
}

errno_t
ifnet_stat_increment_in(struct ifnet *ifp, u_int32_t packets_in,
    u_int32_t bytes_in, u_int32_t errors_in)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	if (packets_in != 0) {
		os_atomic_add(&ifp->if_data.ifi_ipackets, packets_in, relaxed);
	}
	if (bytes_in != 0) {
		os_atomic_add(&ifp->if_data.ifi_ibytes, bytes_in, relaxed);
	}
	if (errors_in != 0) {
		os_atomic_add(&ifp->if_data.ifi_ierrors, errors_in, relaxed);
	}

	TOUCHLASTCHANGE(&ifp->if_lastchange);

	if (ifp->if_data_threshold != 0) {
		ifnet_notify_data_threshold(ifp);
	}

	return 0;
}

errno_t
ifnet_stat_increment_out(struct ifnet *ifp, u_int32_t packets_out,
    u_int32_t bytes_out, u_int32_t errors_out)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	if (packets_out != 0) {
		os_atomic_add(&ifp->if_data.ifi_opackets, packets_out, relaxed);
	}
	if (bytes_out != 0) {
		os_atomic_add(&ifp->if_data.ifi_obytes, bytes_out, relaxed);
	}
	if (errors_out != 0) {
		os_atomic_add(&ifp->if_data.ifi_oerrors, errors_out, relaxed);
	}

	TOUCHLASTCHANGE(&ifp->if_lastchange);

	if (ifp->if_data_threshold != 0) {
		ifnet_notify_data_threshold(ifp);
	}

	return 0;
}

errno_t
ifnet_set_stat(struct ifnet *ifp, const struct ifnet_stats_param *s)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	os_atomic_store(&ifp->if_data.ifi_ipackets, s->packets_in, release);
	os_atomic_store(&ifp->if_data.ifi_ibytes, s->bytes_in, release);
	os_atomic_store(&ifp->if_data.ifi_imcasts, s->multicasts_in, release);
	os_atomic_store(&ifp->if_data.ifi_ierrors, s->errors_in, release);

	os_atomic_store(&ifp->if_data.ifi_opackets, s->packets_out, release);
	os_atomic_store(&ifp->if_data.ifi_obytes, s->bytes_out, release);
	os_atomic_store(&ifp->if_data.ifi_omcasts, s->multicasts_out, release);
	os_atomic_store(&ifp->if_data.ifi_oerrors, s->errors_out, release);

	os_atomic_store(&ifp->if_data.ifi_collisions, s->collisions, release);
	os_atomic_store(&ifp->if_data.ifi_iqdrops, s->dropped, release);
	os_atomic_store(&ifp->if_data.ifi_noproto, s->no_protocol, release);

	/* Touch the last change time. */
	TOUCHLASTCHANGE(&ifp->if_lastchange);

	if (ifp->if_data_threshold != 0) {
		ifnet_notify_data_threshold(ifp);
	}

	return 0;
}

errno_t
ifnet_stat(struct ifnet *ifp, struct ifnet_stats_param *s)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	s->packets_in = os_atomic_load(&ifp->if_data.ifi_ipackets, relaxed);
	s->bytes_in = os_atomic_load(&ifp->if_data.ifi_ibytes, relaxed);
	s->multicasts_in = os_atomic_load(&ifp->if_data.ifi_imcasts, relaxed);
	s->errors_in = os_atomic_load(&ifp->if_data.ifi_ierrors, relaxed);

	s->packets_out = os_atomic_load(&ifp->if_data.ifi_opackets, relaxed);
	s->bytes_out = os_atomic_load(&ifp->if_data.ifi_obytes, relaxed);
	s->multicasts_out = os_atomic_load(&ifp->if_data.ifi_omcasts, relaxed);
	s->errors_out = os_atomic_load(&ifp->if_data.ifi_oerrors, relaxed);

	s->collisions = os_atomic_load(&ifp->if_data.ifi_collisions, relaxed);
	s->dropped = os_atomic_load(&ifp->if_data.ifi_iqdrops, relaxed);
	s->no_protocol = os_atomic_load(&ifp->if_data.ifi_noproto, relaxed);

	if (ifp->if_data_threshold != 0) {
		ifnet_notify_data_threshold(ifp);
	}

	return 0;
}

errno_t
ifnet_touch_lastchange(ifnet_t interface)
{
	if (interface == NULL) {
		return EINVAL;
	}

	TOUCHLASTCHANGE(&interface->if_lastchange);

	return 0;
}

errno_t
ifnet_lastchange(ifnet_t interface, struct timeval *last_change)
{
	if (interface == NULL) {
		return EINVAL;
	}

	*last_change = interface->if_data.ifi_lastchange;
	/* Crude conversion from uptime to calendar time */
	last_change->tv_sec += boottime_sec();

	return 0;
}

errno_t
ifnet_touch_lastupdown(ifnet_t interface)
{
	if (interface == NULL) {
		return EINVAL;
	}

	TOUCHLASTCHANGE(&interface->if_lastupdown);

	return 0;
}

errno_t
ifnet_updown_delta(ifnet_t interface, struct timeval *updown_delta)
{
	if (interface == NULL) {
		return EINVAL;
	}

	/* Calculate the delta */
	updown_delta->tv_sec = (time_t)net_uptime();
	if (updown_delta->tv_sec > interface->if_data.ifi_lastupdown.tv_sec) {
		updown_delta->tv_sec -= interface->if_data.ifi_lastupdown.tv_sec;
	} else {
		updown_delta->tv_sec = 0;
	}
	updown_delta->tv_usec = 0;

	return 0;
}

errno_t
ifnet_get_address_list(ifnet_t interface, ifaddr_t *__null_terminated *addresses)
{
	return addresses == NULL ? EINVAL :
	       ifnet_get_address_list_family(interface, addresses, 0);
}

errno_t
ifnet_get_address_list_with_count(ifnet_t interface,
    ifaddr_t *__counted_by(*addresses_count) * addresses,
    uint16_t *addresses_count)
{
	return ifnet_get_address_list_family_internal(interface, addresses,
	           addresses_count, 0, 0, Z_WAITOK, 0);
}

struct ifnet_addr_list {
	SLIST_ENTRY(ifnet_addr_list)    ifal_le;
	struct ifaddr                   *ifal_ifa;
};

errno_t
ifnet_get_address_list_family(ifnet_t interface, ifaddr_t *__null_terminated *ret_addresses,
    sa_family_t family)
{
	uint16_t addresses_count = 0;
	ifaddr_t *__counted_by(addresses_count) addresses = NULL;
	errno_t error;

	error = ifnet_get_address_list_family_internal(interface, &addresses,
	    &addresses_count, family, 0, Z_WAITOK, 0);
	if (addresses_count > 0) {
		*ret_addresses = __unsafe_null_terminated_from_indexable(addresses,
		    &addresses[addresses_count - 1]);
	} else {
		*ret_addresses = NULL;
	}

	return error;
}

errno_t
ifnet_get_address_list_family_with_count(ifnet_t interface,
    ifaddr_t *__counted_by(*addresses_count) *addresses,
    uint16_t *addresses_count, sa_family_t family)
{
	return ifnet_get_address_list_family_internal(interface, addresses,
	           addresses_count, family, 0, Z_WAITOK, 0);
}

errno_t
ifnet_get_inuse_address_list(ifnet_t interface, ifaddr_t *__null_terminated *ret_addresses)
{
	uint16_t addresses_count = 0;
	ifaddr_t *__counted_by(addresses_count) addresses = NULL;
	errno_t error;

	error = ifnet_get_address_list_family_internal(interface, &addresses,
	    &addresses_count, 0, 0, Z_WAITOK, 1);
	if (addresses_count > 0) {
		*ret_addresses = __unsafe_null_terminated_from_indexable(addresses,
		    &addresses[addresses_count - 1]);
	} else {
		*ret_addresses = NULL;
	}

	return error;
}

extern uint32_t tcp_find_anypcb_byaddr(struct ifaddr *ifa);
extern uint32_t udp_find_anypcb_byaddr(struct ifaddr *ifa);

__private_extern__ errno_t
ifnet_get_address_list_family_internal(ifnet_t interface,
    ifaddr_t *__counted_by(*addresses_count) *addresses,
    uint16_t *addresses_count, sa_family_t family, int detached, int how,
    int return_inuse_addrs)
{
	SLIST_HEAD(, ifnet_addr_list) ifal_head;
	struct ifnet_addr_list *ifal, *ifal_tmp;
	struct ifnet *ifp;
	uint16_t count = 0;
	errno_t err = 0;
	int usecount = 0;
	int index = 0;

	SLIST_INIT(&ifal_head);

	if (addresses == NULL || addresses_count == NULL) {
		err = EINVAL;
		goto done;
	}
	*addresses = NULL;
	*addresses_count = 0;

	if (detached) {
		/*
		 * Interface has been detached, so skip the lookup
		 * at ifnet_head and go directly to inner loop.
		 */
		ifp = interface;
		if (ifp == NULL) {
			err = EINVAL;
			goto done;
		}
		goto one;
	}

	ifnet_head_lock_shared();
	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		if (interface != NULL && ifp != interface) {
			continue;
		}
one:
		ifnet_lock_shared(ifp);
		if (interface == NULL || interface == ifp) {
			struct ifaddr *ifa;
			TAILQ_FOREACH(ifa, &ifp->if_addrhead, ifa_link) {
				IFA_LOCK(ifa);
				if (family != 0 &&
				    ifa->ifa_addr->sa_family != family) {
					IFA_UNLOCK(ifa);
					continue;
				}
				ifal = kalloc_type(struct ifnet_addr_list, how);
				if (ifal == NULL) {
					IFA_UNLOCK(ifa);
					ifnet_lock_done(ifp);
					if (!detached) {
						ifnet_head_done();
					}
					err = ENOMEM;
					goto done;
				}
				ifal->ifal_ifa = ifa;
				ifa_addref(ifa);
				SLIST_INSERT_HEAD(&ifal_head, ifal, ifal_le);
				IFA_UNLOCK(ifa);
				if (__improbable(os_inc_overflow(&count))) {
					ifnet_lock_done(ifp);
					if (!detached) {
						ifnet_head_done();
					}
					err = EINVAL;
					goto done;
				}
			}
		}
		ifnet_lock_done(ifp);
		if (detached) {
			break;
		}
	}
	if (!detached) {
		ifnet_head_done();
	}

	if (count == 0) {
		err = ENXIO;
		goto done;
	}

	uint16_t allocation_size = 0;
	if (__improbable(os_add_overflow(count, 1, &allocation_size))) {
		err = EINVAL;
		goto done;
	}
	ifaddr_t *allocation = kalloc_type(ifaddr_t, allocation_size, how | Z_ZERO);
	if (allocation == NULL) {
		err = ENOMEM;
		goto done;
	}
	*addresses = allocation;
	*addresses_count = allocation_size;

done:
	SLIST_FOREACH_SAFE(ifal, &ifal_head, ifal_le, ifal_tmp) {
		SLIST_REMOVE(&ifal_head, ifal, ifnet_addr_list, ifal_le);
		if (err == 0) {
			if (return_inuse_addrs) {
				usecount = tcp_find_anypcb_byaddr(ifal->ifal_ifa);
				usecount += udp_find_anypcb_byaddr(ifal->ifal_ifa);
				if (usecount) {
					(*addresses)[index] = ifal->ifal_ifa;
					index++;
				} else {
					ifa_remref(ifal->ifal_ifa);
				}
			} else {
				(*addresses)[--count] = ifal->ifal_ifa;
			}
		} else {
			ifa_remref(ifal->ifal_ifa);
		}
		kfree_type(struct ifnet_addr_list, ifal);
	}

	VERIFY(err == 0 || *addresses == NULL);
	if ((err == 0) && (count) && ((*addresses)[0] == NULL)) {
		VERIFY(return_inuse_addrs == 1);
		kfree_type_counted_by(ifaddr_t, *addresses_count, *addresses);
		err = ENXIO;
	}
	return err;
}

void
ifnet_free_address_list(ifaddr_t *__null_terminated addresses)
{
	int i = 0;

	if (addresses == NULL) {
		return;
	}

	for (ifaddr_t *__null_terminated ptr = addresses; *ptr != NULL; ++ptr, i++) {
		ifa_remref(*ptr);
	}

	ifaddr_t *free_addresses = __unsafe_null_terminated_to_indexable(addresses);
	kfree_type(ifaddr_t, i + 1, free_addresses);
}

void
ifnet_address_list_free_counted_by_internal(ifaddr_t *__counted_by(addresses_count) addresses,
    uint16_t addresses_count)
{
	if (addresses == NULL) {
		return;
	}
	for (int i = 0; i < addresses_count; i++) {
		if (addresses[i] != NULL) {
			ifa_remref(addresses[i]);
		}
	}
	kfree_type_counted_by(ifaddr_t, addresses_count, addresses);
}

void *
ifnet_lladdr(ifnet_t interface)
{
	struct ifaddr *ifa;
	void *lladdr;

	if (interface == NULL) {
		return NULL;
	}

	/*
	 * if_lladdr points to the permanent link address of
	 * the interface and it never gets deallocated; internal
	 * code should simply use IF_LLADDR() for performance.
	 */
	ifa = interface->if_lladdr;
	IFA_LOCK_SPIN(ifa);
	struct sockaddr_dl *sdl = SDL(ifa->ifa_addr);
	lladdr = LLADDR(sdl);
	IFA_UNLOCK(ifa);

	return lladdr;
}

errno_t
ifnet_llbroadcast_copy_bytes(ifnet_t interface, void *__sized_by(buffer_len) addr,
    size_t buffer_len, size_t *out_len)
{
	if (interface == NULL || addr == NULL || out_len == NULL) {
		return EINVAL;
	}

	*out_len = interface->if_broadcast.length;

	if (buffer_len < interface->if_broadcast.length) {
		return EMSGSIZE;
	}

	if (interface->if_broadcast.length == 0) {
		return ENXIO;
	}

	bcopy(interface->if_broadcast.ptr, addr,
	    interface->if_broadcast.length);

	return 0;
}

static errno_t
ifnet_lladdr_copy_bytes_internal(ifnet_t interface, void *__sized_by(lladdr_len) lladdr,
    size_t lladdr_len, kauth_cred_t *credp)
{
	size_t bytes_len;
	const u_int8_t *bytes;
	struct ifaddr *ifa;
	uint8_t sdlbuf[SOCK_MAXADDRLEN + 1];
	errno_t error = 0;

	/*
	 * Make sure to accomodate the largest possible
	 * size of SA(if_lladdr)->sa_len.
	 */
	_CASSERT(sizeof(sdlbuf) == (SOCK_MAXADDRLEN + 1));

	if (interface == NULL || lladdr == NULL) {
		return EINVAL;
	}

	ifa = interface->if_lladdr;
	IFA_LOCK_SPIN(ifa);
	const struct sockaddr_dl *sdl = SDL(sdlbuf);
	SOCKADDR_COPY(ifa->ifa_addr, sdl, SA(ifa->ifa_addr)->sa_len);
	IFA_UNLOCK(ifa);

	bytes = dlil_ifaddr_bytes_indexable(SDL(sdlbuf), &bytes_len, credp);
	if (bytes_len != lladdr_len) {
		bzero(lladdr, lladdr_len);
		error = EMSGSIZE;
	} else {
		bcopy(bytes, lladdr, bytes_len);
	}

	return error;
}

errno_t
ifnet_lladdr_copy_bytes(ifnet_t interface, void *__sized_by(length) lladdr, size_t length)
{
	return ifnet_lladdr_copy_bytes_internal(interface, lladdr, length,
	           NULL);
}

errno_t
ifnet_guarded_lladdr_copy_bytes(ifnet_t interface, void *__sized_by(length) lladdr, size_t length)
{
#if CONFIG_MACF
	kauth_cred_t __single cred;
	net_thread_marks_t __single marks;
#endif
	kauth_cred_t *__single credp;
	errno_t error;

#if CONFIG_MACF
	marks = net_thread_marks_push(NET_THREAD_CKREQ_LLADDR);
	cred  = current_cached_proc_cred(PROC_NULL);
	credp = &cred;
#else
	credp = NULL;
#endif

	error = ifnet_lladdr_copy_bytes_internal(interface, lladdr, length,
	    credp);

#if CONFIG_MACF
	net_thread_marks_pop(marks);
#endif

	return error;
}

static errno_t
ifnet_set_lladdr_internal(ifnet_t interface, const void *__sized_by(lladdr_len) lladdr,
    size_t lladdr_len, u_char new_type, int apply_type)
{
	struct ifaddr *ifa;
	errno_t error = 0;

	if (interface == NULL) {
		return EINVAL;
	}

	ifnet_head_lock_shared();
	ifnet_lock_exclusive(interface);
	if (lladdr_len != 0 &&
	    (lladdr_len != interface->if_addrlen || lladdr == 0)) {
		ifnet_lock_done(interface);
		ifnet_head_done();
		return EINVAL;
	}
	/* The interface needs to be attached to add an address */
	if (interface->if_refflags & IFRF_EMBRYONIC) {
		ifnet_lock_done(interface);
		ifnet_head_done();
		return ENXIO;
	}

	ifa = ifnet_addrs[interface->if_index - 1];
	if (ifa != NULL) {
		struct sockaddr_dl *sdl;

		IFA_LOCK_SPIN(ifa);
		sdl = (struct sockaddr_dl *)(void *)ifa->ifa_addr;
		if (lladdr_len != 0) {
			bcopy(lladdr, LLADDR(sdl), lladdr_len);
		} else {
			bzero(LLADDR(sdl), interface->if_addrlen);
		}
		/* lladdr_len-check with if_addrlen makes sure it fits in u_char */
		sdl->sdl_alen = (u_char)lladdr_len;

		if (apply_type) {
			sdl->sdl_type = new_type;
		}
		IFA_UNLOCK(ifa);
	} else {
		error = ENXIO;
	}
	ifnet_lock_done(interface);
	ifnet_head_done();

	/* Generate a kernel event */
	if (error == 0) {
		intf_event_enqueue_nwk_wq_entry(interface, NULL,
		    INTF_EVENT_CODE_LLADDR_UPDATE);
		dlil_post_msg(interface, KEV_DL_SUBCLASS,
		    KEV_DL_LINK_ADDRESS_CHANGED, NULL, 0, FALSE);
	}

	return error;
}

errno_t
ifnet_set_lladdr(ifnet_t interface, const void *__sized_by(lladdr_len) lladdr, size_t lladdr_len)
{
	return ifnet_set_lladdr_internal(interface, lladdr, lladdr_len, 0, 0);
}

errno_t
ifnet_set_lladdr_and_type(ifnet_t interface, const void *__sized_by(lladdr_len) lladdr,
    size_t lladdr_len, u_char type)
{
	return ifnet_set_lladdr_internal(interface, lladdr,
	           lladdr_len, type, 1);
}

errno_t
ifnet_add_multicast(ifnet_t interface, const struct sockaddr *maddr,
    ifmultiaddr_t *ifmap)
{
	if (interface == NULL || maddr == NULL) {
		return EINVAL;
	}

	/* Don't let users screw up protocols' entries. */
	switch (maddr->sa_family) {
	case AF_LINK: {
		const struct sockaddr_dl *sdl = SDL(maddr);
		if (sdl->sdl_len < sizeof(struct sockaddr_dl) ||
		    (sdl->sdl_nlen + sdl->sdl_alen + sdl->sdl_slen +
		    offsetof(struct sockaddr_dl, sdl_data) > sdl->sdl_len)) {
			return EINVAL;
		}
		break;
	}
	case AF_UNSPEC:
		if (maddr->sa_len < ETHER_ADDR_LEN +
		    offsetof(struct sockaddr, sa_data)) {
			return EINVAL;
		}
		break;
	default:
		return EINVAL;
	}

	return if_addmulti_anon(interface, maddr, ifmap);
}

errno_t
ifnet_remove_multicast(ifmultiaddr_t ifma)
{
	struct sockaddr *maddr;

	if (ifma == NULL) {
		return EINVAL;
	}

	maddr = ifma->ifma_addr;
	/* Don't let users screw up protocols' entries. */
	if (maddr->sa_family != AF_UNSPEC && maddr->sa_family != AF_LINK) {
		return EINVAL;
	}

	return if_delmulti_anon(ifma->ifma_ifp, maddr);
}

errno_t
ifnet_get_multicast_list(ifnet_t ifp, ifmultiaddr_t *__null_terminated *ret_addresses)
{
	int count = 0;
	int cmax = 0;
	struct ifmultiaddr *addr;

	if (ifp == NULL || ret_addresses == NULL) {
		return EINVAL;
	}
	*ret_addresses = NULL;

	ifnet_lock_shared(ifp);
	LIST_FOREACH(addr, &ifp->if_multiaddrs, ifma_link) {
		cmax++;
	}

	ifmultiaddr_t *addresses = kalloc_type(ifmultiaddr_t, cmax + 1, Z_WAITOK);
	if (addresses == NULL) {
		ifnet_lock_done(ifp);
		return ENOMEM;
	}

	LIST_FOREACH(addr, &ifp->if_multiaddrs, ifma_link) {
		if (count + 1 > cmax) {
			break;
		}
		addresses[count] = (ifmultiaddr_t)addr;
		ifmaddr_reference(addresses[count]);
		count++;
	}
	addresses[cmax] = NULL;
	ifnet_lock_done(ifp);

	*ret_addresses = __unsafe_null_terminated_from_indexable(addresses, &addresses[cmax]);

	return 0;
}

void
ifnet_free_multicast_list(ifmultiaddr_t *__null_terminated addresses)
{
	int i = 0;

	if (addresses == NULL) {
		return;
	}

	for (ifmultiaddr_t *__null_terminated ptr = addresses; *ptr != NULL; ptr++, i++) {
		ifmaddr_release(*ptr);
	}

	ifmultiaddr_t *free_addresses = __unsafe_null_terminated_to_indexable(addresses);
	kfree_type(ifmultiaddr_t, i + 1, free_addresses);
}

errno_t
ifnet_find_by_name(const char *ifname, ifnet_t *ifpp)
{
	struct ifnet *ifp;
	size_t namelen;

	if (ifname == NULL) {
		return EINVAL;
	}

	namelen = strlen(ifname);

	*ifpp = NULL;

	ifnet_head_lock_shared();
	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		struct ifaddr *ifa;
		struct sockaddr_dl *ll_addr;

		ifa = ifnet_addrs[ifp->if_index - 1];
		if (ifa == NULL) {
			continue;
		}

		IFA_LOCK(ifa);
		ll_addr = SDL(ifa->ifa_addr);

		if (namelen == ll_addr->sdl_nlen &&
		    strlcmp(ll_addr->sdl_data, ifname, namelen) == 0) {
			IFA_UNLOCK(ifa);
			*ifpp = ifp;
			ifnet_reference(*ifpp);
			break;
		}
		IFA_UNLOCK(ifa);
	}
	ifnet_head_done();

	return (ifp == NULL) ? ENXIO : 0;
}

errno_t
ifnet_list_get(ifnet_family_t family, ifnet_t *__counted_by(*count) *list,
    u_int32_t *count)
{
	return ifnet_list_get_common(family, FALSE, list, count);
}

__private_extern__ errno_t
ifnet_list_get_all(ifnet_family_t family, ifnet_t *__counted_by(*count) *list,
    u_int32_t *count)
{
	return ifnet_list_get_common(family, TRUE, list, count);
}

struct ifnet_list {
	SLIST_ENTRY(ifnet_list) ifl_le;
	struct ifnet            *ifl_ifp;
};

static errno_t
ifnet_list_get_common(ifnet_family_t family, boolean_t get_all,
    ifnet_t *__counted_by(*count) *list, u_int32_t *count)
{
#pragma unused(get_all)
	SLIST_HEAD(, ifnet_list) ifl_head;
	struct ifnet_list *ifl, *ifl_tmp;
	struct ifnet *ifp;
	ifnet_t *tmp_list = NULL;
	int cnt = 0;
	errno_t err = 0;

	SLIST_INIT(&ifl_head);

	if (list == NULL || count == NULL) {
		err = EINVAL;
		goto done;
	}
	*list = NULL;
	*count = 0;

	ifnet_head_lock_shared();
	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		if (family == IFNET_FAMILY_ANY || ifp->if_family == family) {
			ifl = kalloc_type(struct ifnet_list, Z_WAITOK | Z_ZERO);
			if (ifl == NULL) {
				ifnet_head_done();
				err = ENOMEM;
				goto done;
			}
			ifl->ifl_ifp = ifp;
			ifnet_reference(ifp);
			SLIST_INSERT_HEAD(&ifl_head, ifl, ifl_le);
			++cnt;
		}
	}
	ifnet_head_done();

	if (cnt == 0) {
		err = ENXIO;
		goto done;
	}

	tmp_list = kalloc_type(ifnet_t, cnt + 1, Z_WAITOK | Z_ZERO);
	if (tmp_list == NULL) {
		err = ENOMEM;
		goto done;
	}
	*list = tmp_list;
	*count = cnt;

done:
	SLIST_FOREACH_SAFE(ifl, &ifl_head, ifl_le, ifl_tmp) {
		SLIST_REMOVE(&ifl_head, ifl, ifnet_list, ifl_le);
		if (err == 0) {
			(*list)[--cnt] = ifl->ifl_ifp;
		} else {
			ifnet_release(ifl->ifl_ifp);
		}
		kfree_type(struct ifnet_list, ifl);
	}

	return err;
}

void
ifnet_list_free(ifnet_t *__null_terminated interfaces)
{
	int i = 0;

	if (interfaces == NULL) {
		return;
	}

	for (ifnet_t *__null_terminated ptr = interfaces; *ptr != NULL; ptr++, i++) {
		ifnet_release(*ptr);
	}

	ifnet_t *free_interfaces = __unsafe_null_terminated_to_indexable(interfaces);
	kfree_type(ifnet_t, i + 1, free_interfaces);
}

void
ifnet_list_free_counted_by_internal(ifnet_t *__counted_by(count) interfaces, uint32_t count)
{
	if (interfaces == NULL) {
		return;
	}
	for (int i = 0; i < count; i++) {
		ifnet_release(interfaces[i]);
	}

	/*
	 * When we allocated the ifnet_list, we returned only the number
	 * of ifnet_t pointers without the null terminator in the `count'
	 * variable, so we cheat here by freeing everything.
	 */
	ifnet_t *free_interfaces = interfaces;
	kfree_type(ifnet_t, count + 1, free_interfaces);
	interfaces = NULL;
	count = 0;
}

/*************************************************************************/
/* ifaddr_t accessors						*/
/*************************************************************************/

errno_t
ifaddr_reference(ifaddr_t ifa)
{
	if (ifa == NULL) {
		return EINVAL;
	}

	ifa_addref(ifa);
	return 0;
}

errno_t
ifaddr_release(ifaddr_t ifa)
{
	if (ifa == NULL) {
		return EINVAL;
	}

	ifa_remref(ifa);
	return 0;
}

sa_family_t
ifaddr_address_family(ifaddr_t ifa)
{
	sa_family_t family = 0;

	if (ifa != NULL) {
		IFA_LOCK_SPIN(ifa);
		if (ifa->ifa_addr != NULL) {
			family = ifa->ifa_addr->sa_family;
		}
		IFA_UNLOCK(ifa);
	}
	return family;
}

errno_t
ifaddr_address(ifaddr_t ifa, struct sockaddr *out_addr, u_int32_t addr_size)
{
	u_int32_t copylen;

	if (ifa == NULL || out_addr == NULL) {
		return EINVAL;
	}

	IFA_LOCK_SPIN(ifa);
	if (ifa->ifa_addr == NULL) {
		IFA_UNLOCK(ifa);
		return ENOTSUP;
	}

	copylen = (addr_size >= ifa->ifa_addr->sa_len) ?
	    ifa->ifa_addr->sa_len : addr_size;
	SOCKADDR_COPY(ifa->ifa_addr, out_addr, copylen);

	if (ifa->ifa_addr->sa_len > addr_size) {
		IFA_UNLOCK(ifa);
		return EMSGSIZE;
	}

	IFA_UNLOCK(ifa);
	return 0;
}

errno_t
ifaddr_dstaddress(ifaddr_t ifa, struct sockaddr *out_addr, u_int32_t addr_size)
{
	u_int32_t copylen;

	if (ifa == NULL || out_addr == NULL) {
		return EINVAL;
	}

	IFA_LOCK_SPIN(ifa);
	if (ifa->ifa_dstaddr == NULL) {
		IFA_UNLOCK(ifa);
		return ENOTSUP;
	}

	copylen = (addr_size >= ifa->ifa_dstaddr->sa_len) ?
	    ifa->ifa_dstaddr->sa_len : addr_size;
	SOCKADDR_COPY(ifa->ifa_dstaddr, out_addr, copylen);

	if (ifa->ifa_dstaddr->sa_len > addr_size) {
		IFA_UNLOCK(ifa);
		return EMSGSIZE;
	}

	IFA_UNLOCK(ifa);
	return 0;
}

errno_t
ifaddr_netmask(ifaddr_t ifa, struct sockaddr *out_addr, u_int32_t addr_size)
{
	u_int32_t copylen;

	if (ifa == NULL || out_addr == NULL) {
		return EINVAL;
	}

	IFA_LOCK_SPIN(ifa);
	if (ifa->ifa_netmask == NULL) {
		IFA_UNLOCK(ifa);
		return ENOTSUP;
	}

	copylen = addr_size >= ifa->ifa_netmask->sa_len ?
	    ifa->ifa_netmask->sa_len : addr_size;
	SOCKADDR_COPY(ifa->ifa_netmask, out_addr, copylen);

	if (ifa->ifa_netmask->sa_len > addr_size) {
		IFA_UNLOCK(ifa);
		return EMSGSIZE;
	}

	IFA_UNLOCK(ifa);
	return 0;
}

ifnet_t
ifaddr_ifnet(ifaddr_t ifa)
{
	struct ifnet *ifp;

	if (ifa == NULL) {
		return NULL;
	}

	/* ifa_ifp is set once at creation time; it is never changed */
	ifp = ifa->ifa_ifp;

	return ifp;
}

ifaddr_t
ifaddr_withaddr(const struct sockaddr *address)
{
	if (address == NULL) {
		return NULL;
	}

	return ifa_ifwithaddr(address);
}

ifaddr_t
ifaddr_withdstaddr(const struct sockaddr *address)
{
	if (address == NULL) {
		return NULL;
	}

	return ifa_ifwithdstaddr(address);
}

ifaddr_t
ifaddr_withnet(const struct sockaddr *net)
{
	if (net == NULL) {
		return NULL;
	}

	return ifa_ifwithnet(net);
}

ifaddr_t
ifaddr_withroute(int flags, const struct sockaddr *destination,
    const struct sockaddr *gateway)
{
	if (destination == NULL || gateway == NULL) {
		return NULL;
	}

	return ifa_ifwithroute(flags, destination, gateway);
}

ifaddr_t
ifaddr_findbestforaddr(const struct sockaddr *addr, ifnet_t interface)
{
	if (addr == NULL || interface == NULL) {
		return NULL;
	}

	return ifaof_ifpforaddr_select(addr, interface);
}

errno_t
ifaddr_get_ia6_flags(ifaddr_t ifa, u_int32_t *out_flags)
{
	sa_family_t family = 0;

	if (ifa == NULL || out_flags == NULL) {
		return EINVAL;
	}

	IFA_LOCK_SPIN(ifa);
	if (ifa->ifa_addr != NULL) {
		family = ifa->ifa_addr->sa_family;
	}
	IFA_UNLOCK(ifa);

	if (family != AF_INET6) {
		return EINVAL;
	}

	*out_flags = ifatoia6(ifa)->ia6_flags;
	return 0;
}

errno_t
ifmaddr_reference(ifmultiaddr_t ifmaddr)
{
	if (ifmaddr == NULL) {
		return EINVAL;
	}

	IFMA_ADDREF(ifmaddr);
	return 0;
}

errno_t
ifmaddr_release(ifmultiaddr_t ifmaddr)
{
	if (ifmaddr == NULL) {
		return EINVAL;
	}

	IFMA_REMREF(ifmaddr);
	return 0;
}

errno_t
ifmaddr_address(ifmultiaddr_t ifma, struct sockaddr *out_addr,
    u_int32_t addr_size)
{
	u_int32_t copylen;

	if (ifma == NULL || out_addr == NULL) {
		return EINVAL;
	}

	IFMA_LOCK(ifma);
	if (ifma->ifma_addr == NULL) {
		IFMA_UNLOCK(ifma);
		return ENOTSUP;
	}

	copylen = (addr_size >= ifma->ifma_addr->sa_len ?
	    ifma->ifma_addr->sa_len : addr_size);
	SOCKADDR_COPY(ifma->ifma_addr, out_addr, copylen);

	if (ifma->ifma_addr->sa_len > addr_size) {
		IFMA_UNLOCK(ifma);
		return EMSGSIZE;
	}
	IFMA_UNLOCK(ifma);
	return 0;
}

errno_t
ifmaddr_lladdress(ifmultiaddr_t ifma, struct sockaddr *out_addr,
    u_int32_t addr_size)
{
	struct ifmultiaddr *ifma_ll;

	if (ifma == NULL || out_addr == NULL) {
		return EINVAL;
	}
	if ((ifma_ll = ifma->ifma_ll) == NULL) {
		return ENOTSUP;
	}

	return ifmaddr_address(ifma_ll, out_addr, addr_size);
}

ifnet_t
ifmaddr_ifnet(ifmultiaddr_t ifma)
{
	return (ifma == NULL) ? NULL : ifma->ifma_ifp;
}

/**************************************************************************/
/* interface cloner						*/
/**************************************************************************/

errno_t
ifnet_clone_attach(struct ifnet_clone_params *cloner_params,
    if_clone_t *ifcloner)
{
	errno_t error = 0;
	struct if_clone *ifc = NULL;
	size_t namelen;

	if (cloner_params == NULL || ifcloner == NULL ||
	    cloner_params->ifc_name == NULL ||
	    cloner_params->ifc_create == NULL ||
	    cloner_params->ifc_destroy == NULL ||
	    (namelen = strlen(cloner_params->ifc_name)) >= IFNAMSIZ) {
		error = EINVAL;
		goto fail;
	}

	if (if_clone_lookup(__terminated_by_to_indexable(cloner_params->ifc_name),
	    namelen, NULL) != NULL) {
		printf("%s: already a cloner for %s\n", __func__,
		    cloner_params->ifc_name);
		error = EEXIST;
		goto fail;
	}

	ifc = kalloc_type(struct if_clone, Z_WAITOK | Z_ZERO | Z_NOFAIL);
	strlcpy(ifc->ifc_name, cloner_params->ifc_name, IFNAMSIZ + 1);
	ifc->ifc_namelen = (uint8_t)namelen;
	ifc->ifc_maxunit = IF_MAXUNIT;
	ifc->ifc_create = cloner_params->ifc_create;
	ifc->ifc_destroy = cloner_params->ifc_destroy;

	error = if_clone_attach(ifc);
	if (error != 0) {
		printf("%s: if_clone_attach failed %d\n", __func__, error);
		goto fail;
	}
	*ifcloner = ifc;

	return 0;
fail:
	if (ifc != NULL) {
		kfree_type(struct if_clone, ifc);
	}
	return error;
}

errno_t
ifnet_clone_detach(if_clone_t ifcloner)
{
	errno_t error = 0;
	struct if_clone *ifc = ifcloner;

	if (ifc == NULL) {
		return EINVAL;
	}

	if ((if_clone_lookup(ifc->ifc_name, ifc->ifc_namelen, NULL)) == NULL) {
		printf("%s: no cloner for %s\n", __func__, ifc->ifc_name);
		error = EINVAL;
		goto fail;
	}

	if_clone_detach(ifc);

	kfree_type(struct if_clone, ifc);

fail:
	return error;
}

/**************************************************************************/
/* misc							*/
/**************************************************************************/

static errno_t
ifnet_get_local_ports_extended_inner(ifnet_t ifp, protocol_family_t protocol,
    u_int32_t flags, u_int8_t bitfield[bitstr_size(IP_PORTRANGE_SIZE)])
{
	u_int32_t ifindex;

	/* no point in continuing if no address is assigned */
	if (ifp != NULL && TAILQ_EMPTY(&ifp->if_addrhead)) {
		return 0;
	}

	if_ports_used_update_wakeuuid(ifp);

#if SKYWALK
	if (netns_is_enabled()) {
		netns_get_local_ports(ifp, protocol, flags, bitfield);
	}
#endif /* SKYWALK */

	ifindex = (ifp != NULL) ? ifp->if_index : 0;

	if (!(flags & IFNET_GET_LOCAL_PORTS_TCPONLY)) {
		udp_get_ports_used(ifp, protocol, flags,
		    bitfield);
	}

	if (!(flags & IFNET_GET_LOCAL_PORTS_UDPONLY)) {
		tcp_get_ports_used(ifp, protocol, flags,
		    bitfield);
	}

	return 0;
}

errno_t
ifnet_get_local_ports_extended(ifnet_t ifp, protocol_family_t protocol,
    u_int32_t flags, u_int8_t bitfield[IP_PORTRANGE_BITFIELD_LEN])
{
	ifnet_ref_t parent_ifp = NULL;

	if (bitfield == NULL) {
		return EINVAL;
	}

	switch (protocol) {
	case PF_UNSPEC:
	case PF_INET:
	case PF_INET6:
		break;
	default:
		return EINVAL;
	}

	/* bit string is long enough to hold 16-bit port values */
	bzero(bitfield, bitstr_size(IP_PORTRANGE_SIZE));

	ifnet_get_local_ports_extended_inner(ifp, protocol, flags, bitfield);

	/* get local ports for parent interface */
	if (ifp != NULL && ifnet_get_delegate_parent(ifp, &parent_ifp) == 0) {
		ifnet_get_local_ports_extended_inner(parent_ifp, protocol,
		    flags, bitfield);
		ifnet_release_delegate_parent(ifp);
	}

	return 0;
}

errno_t
ifnet_get_local_ports(ifnet_t ifp, u_int8_t bitfield[IP_PORTRANGE_BITFIELD_LEN])
{
	u_int32_t flags = IFNET_GET_LOCAL_PORTS_WILDCARDOK;

	return ifnet_get_local_ports_extended(ifp, PF_UNSPEC, flags,
	           bitfield);
}

errno_t
ifnet_notice_node_presence(ifnet_t ifp, struct sockaddr *sa, int32_t rssi,
    int lqm, int npm, u_int8_t srvinfo[48])
{
	if (ifp == NULL || sa == NULL || srvinfo == NULL) {
		return EINVAL;
	}
	if (sa->sa_len > sizeof(struct sockaddr_storage)) {
		return EINVAL;
	}
	if (sa->sa_family != AF_LINK && sa->sa_family != AF_INET6) {
		return EINVAL;
	}

	return dlil_node_present(ifp, sa, rssi, lqm, npm, srvinfo);
}

errno_t
ifnet_notice_node_presence_v2(ifnet_t ifp, struct sockaddr *sa, struct sockaddr_dl *sdl,
    int32_t rssi, int lqm, int npm, u_int8_t srvinfo[48])
{
	/* Support older version if sdl is NULL */
	if (sdl == NULL) {
		return ifnet_notice_node_presence(ifp, sa, rssi, lqm, npm, srvinfo);
	}

	if (ifp == NULL || sa == NULL || srvinfo == NULL) {
		return EINVAL;
	}
	if (sa->sa_len > sizeof(struct sockaddr_storage)) {
		return EINVAL;
	}

	if (sa->sa_family != AF_INET6) {
		return EINVAL;
	}

	if (sdl->sdl_family != AF_LINK) {
		return EINVAL;
	}

	return dlil_node_present_v2(ifp, sa, sdl, rssi, lqm, npm, srvinfo);
}

errno_t
ifnet_notice_node_absence(ifnet_t ifp, struct sockaddr *sa)
{
	if (ifp == NULL || sa == NULL) {
		return EINVAL;
	}
	if (sa->sa_len > sizeof(struct sockaddr_storage)) {
		return EINVAL;
	}
	if (sa->sa_family != AF_LINK && sa->sa_family != AF_INET6) {
		return EINVAL;
	}

	dlil_node_absent(ifp, sa);
	return 0;
}

errno_t
ifnet_notice_primary_elected(ifnet_t ifp)
{
	if (ifp == NULL) {
		return EINVAL;
	}

	dlil_post_msg(ifp, KEV_DL_SUBCLASS, KEV_DL_PRIMARY_ELECTED, NULL, 0, FALSE);
	return 0;
}

errno_t
ifnet_tx_compl_status(ifnet_t ifp, mbuf_t m, tx_compl_val_t val)
{
#pragma unused(val)

	m_do_tx_compl_callback(m, ifp);

	return 0;
}

errno_t
ifnet_tx_compl(ifnet_t ifp, mbuf_t m)
{
	m_do_tx_compl_callback(m, ifp);

	return 0;
}

errno_t
ifnet_report_issues(ifnet_t ifp, u_int8_t modid[IFNET_MODIDLEN],
    u_int8_t info[IFNET_MODARGLEN])
{
	if (ifp == NULL || modid == NULL) {
		return EINVAL;
	}

	dlil_report_issues(ifp, modid, info);
	return 0;
}

errno_t
ifnet_set_delegate(ifnet_t ifp, ifnet_t delegated_ifp)
{
	ifnet_t odifp = NULL;

	if (ifp == NULL) {
		return EINVAL;
	} else if (!ifnet_is_attached(ifp, 1)) {
		return ENXIO;
	}

	ifnet_lock_exclusive(ifp);
	odifp = ifp->if_delegated.ifp;
	if (odifp != NULL && odifp == delegated_ifp) {
		/* delegate info is unchanged; nothing more to do */
		ifnet_lock_done(ifp);
		goto done;
	}
	// Test if this delegate interface would cause a loop
	ifnet_t delegate_check_ifp = delegated_ifp;
	while (delegate_check_ifp != NULL) {
		if (delegate_check_ifp == ifp) {
			printf("%s: delegating to %s would cause a loop\n",
			    ifp->if_xname, delegated_ifp->if_xname);
			ifnet_lock_done(ifp);
			goto done;
		}
		delegate_check_ifp = delegate_check_ifp->if_delegated.ifp;
	}
	bzero(&ifp->if_delegated, sizeof(ifp->if_delegated));
	if (delegated_ifp != NULL && ifp != delegated_ifp) {
		uint32_t        set_eflags;

		ifp->if_delegated.ifp = delegated_ifp;
		ifnet_reference(delegated_ifp);
		ifp->if_delegated.type = delegated_ifp->if_type;
		ifp->if_delegated.family = delegated_ifp->if_family;
		ifp->if_delegated.subfamily = delegated_ifp->if_subfamily;
		ifp->if_delegated.expensive =
		    delegated_ifp->if_eflags & IFEF_EXPENSIVE ? 1 : 0;
		ifp->if_delegated.constrained =
		    delegated_ifp->if_xflags & IFXF_CONSTRAINED ? 1 : 0;
		ifp->if_delegated.ultra_constrained =
		    delegated_ifp->if_xflags & IFXF_ULTRA_CONSTRAINED ? 1 : 0;

		/*
		 * Propogate flags related to ECN from delegated interface
		 */
		if_clear_eflags(ifp, IFEF_ECN_ENABLE | IFEF_ECN_DISABLE);
		set_eflags = (delegated_ifp->if_eflags &
		    (IFEF_ECN_ENABLE | IFEF_ECN_DISABLE));
		if_set_eflags(ifp, set_eflags);
		printf("%s: is now delegating %s (type 0x%x, family %u, "
		    "sub-family %u)\n", ifp->if_xname, delegated_ifp->if_xname,
		    delegated_ifp->if_type, delegated_ifp->if_family,
		    delegated_ifp->if_subfamily);
	}

	ifnet_lock_done(ifp);

	if (odifp != NULL) {
		if (odifp != delegated_ifp) {
			printf("%s: is no longer delegating %s\n",
			    ifp->if_xname, odifp->if_xname);
		}
		ifnet_release(odifp);
	}

	/* Generate a kernel event */
	dlil_post_msg(ifp, KEV_DL_SUBCLASS, KEV_DL_IFDELEGATE_CHANGED, NULL, 0, FALSE);

done:
	/* Release the io ref count */
	ifnet_decr_iorefcnt(ifp);

	return 0;
}

errno_t
ifnet_get_delegate(ifnet_t ifp, ifnet_t *pdelegated_ifp)
{
	if (ifp == NULL || pdelegated_ifp == NULL) {
		return EINVAL;
	} else if (!ifnet_is_attached(ifp, 1)) {
		return ENXIO;
	}

	ifnet_lock_shared(ifp);
	if (ifp->if_delegated.ifp != NULL) {
		ifnet_reference(ifp->if_delegated.ifp);
	}
	*pdelegated_ifp = ifp->if_delegated.ifp;
	ifnet_lock_done(ifp);

	/* Release the io ref count */
	ifnet_decr_iorefcnt(ifp);

	return 0;
}

errno_t
ifnet_get_keepalive_offload_frames(ifnet_t ifp,
    struct ifnet_keepalive_offload_frame *__counted_by(frames_array_count) frames_array,
    u_int32_t frames_array_count, size_t frame_data_offset,
    u_int32_t *used_frames_count)
{
	u_int32_t i;

	if (frames_array == NULL || used_frames_count == NULL ||
	    frame_data_offset >= IFNET_KEEPALIVE_OFFLOAD_FRAME_DATA_SIZE) {
		return EINVAL;
	}

	/* frame_data_offset should be 32-bit aligned */
	if (P2ROUNDUP(frame_data_offset, sizeof(u_int32_t)) !=
	    frame_data_offset) {
		return EINVAL;
	}

	*used_frames_count = 0;
	if (frames_array_count == 0) {
		return 0;
	}


	for (i = 0; i < frames_array_count; i++) {
		struct ifnet_keepalive_offload_frame *frame = frames_array + i;
		bzero(frame, sizeof(struct ifnet_keepalive_offload_frame));
	}

	/* First collect IPsec related keep-alive frames */
	*used_frames_count = key_fill_offload_frames_for_savs(ifp,
	    frames_array, frames_array_count, frame_data_offset);

	/* Keep-alive offload not required for TCP/UDP on CLAT interface */
	if (IS_INTF_CLAT46(ifp)) {
		return 0;
	}

	/* If there is more room, collect other UDP keep-alive frames */
	if (*used_frames_count < frames_array_count) {
		udp_fill_keepalive_offload_frames(ifp, frames_array,
		    frames_array_count, frame_data_offset,
		    used_frames_count);
	}

	/* If there is more room, collect other TCP keep-alive frames */
	if (*used_frames_count < frames_array_count) {
		tcp_fill_keepalive_offload_frames(ifp, frames_array,
		    frames_array_count, frame_data_offset,
		    used_frames_count);
	}

	VERIFY(*used_frames_count <= frames_array_count);

	return 0;
}

errno_t
ifnet_notify_tcp_keepalive_offload_timeout(ifnet_t ifp,
    struct ifnet_keepalive_offload_frame *frame)
{
	errno_t error = 0;

	if (ifp == NULL || frame == NULL) {
		return EINVAL;
	}

	if (frame->type != IFNET_KEEPALIVE_OFFLOAD_FRAME_TCP) {
		return EINVAL;
	}
	if (frame->ether_type != IFNET_KEEPALIVE_OFFLOAD_FRAME_ETHERTYPE_IPV4 &&
	    frame->ether_type != IFNET_KEEPALIVE_OFFLOAD_FRAME_ETHERTYPE_IPV6) {
		return EINVAL;
	}
	if (frame->local_port == 0 || frame->remote_port == 0) {
		return EINVAL;
	}

	error = tcp_notify_kao_timeout(ifp, frame);

	return error;
}

errno_t
ifnet_link_status_report(ifnet_t ifp, const void *__sized_by(buffer_len) buffer,
    size_t buffer_len)
{
	struct if_link_status ifsr = {};
	errno_t err = 0;

	if (ifp == NULL || buffer == NULL || buffer_len == 0) {
		return EINVAL;
	}

	ifnet_lock_shared(ifp);

	/*
	 * Make sure that the interface is attached but there is no need
	 * to take a reference because this call is coming from the driver.
	 */
	if (!ifnet_is_attached(ifp, 0)) {
		ifnet_lock_done(ifp);
		return ENXIO;
	}

	lck_rw_lock_exclusive(&ifp->if_link_status_lock);

	/*
	 * If this is the first status report then allocate memory
	 * to store it.
	 */
	if (ifp->if_link_status == NULL) {
		ifp->if_link_status = kalloc_type(struct if_link_status, Z_ZERO);
		if (ifp->if_link_status == NULL) {
			err = ENOMEM;
			goto done;
		}
	}

	memcpy(&ifsr, buffer, MIN(sizeof(ifsr), buffer_len));
	if (ifp->if_type == IFT_CELLULAR) {
		struct if_cellular_status_v1 *if_cell_sr, *new_cell_sr;
		/*
		 * Currently we have a single version -- if it does
		 * not match, just return.
		 */
		if (ifsr.ifsr_version !=
		    IF_CELLULAR_STATUS_REPORT_CURRENT_VERSION) {
			err = ENOTSUP;
			goto done;
		}

		if (ifsr.ifsr_len != sizeof(*if_cell_sr)) {
			err = EINVAL;
			goto done;
		}

		if_cell_sr =
		    &ifp->if_link_status->ifsr_u.ifsr_cell.if_cell_u.if_status_v1;
		new_cell_sr = &ifsr.ifsr_u.ifsr_cell.if_cell_u.if_status_v1;
		/* Check if we need to act on any new notifications */
		if ((new_cell_sr->valid_bitmask &
		    IF_CELL_UL_MSS_RECOMMENDED_VALID) &&
		    new_cell_sr->mss_recommended !=
		    if_cell_sr->mss_recommended) {
			os_atomic_or(&tcbinfo.ipi_flags, INPCBINFO_UPDATE_MSS, relaxed);
			inpcb_timer_sched(&tcbinfo, INPCB_TIMER_FAST);
#if NECP
			necp_update_all_clients();
#endif
		}

		/* Finally copy the new information */
		ifp->if_link_status->ifsr_version = ifsr.ifsr_version;
		ifp->if_link_status->ifsr_len = ifsr.ifsr_len;
		if_cell_sr->valid_bitmask = 0;
		bcopy(new_cell_sr, if_cell_sr, sizeof(*if_cell_sr));
	} else if (IFNET_IS_WIFI(ifp)) {
		struct if_wifi_status_v1 *if_wifi_sr, *new_wifi_sr;

		/* Check version */
		if (ifsr.ifsr_version !=
		    IF_WIFI_STATUS_REPORT_CURRENT_VERSION) {
			err = ENOTSUP;
			goto done;
		}

		if (ifsr.ifsr_len != sizeof(*if_wifi_sr)) {
			err = EINVAL;
			goto done;
		}

		if_wifi_sr =
		    &ifp->if_link_status->ifsr_u.ifsr_wifi.if_wifi_u.if_status_v1;
		new_wifi_sr =
		    &ifsr.ifsr_u.ifsr_wifi.if_wifi_u.if_status_v1;
		ifp->if_link_status->ifsr_version = ifsr.ifsr_version;
		ifp->if_link_status->ifsr_len = ifsr.ifsr_len;
		if_wifi_sr->valid_bitmask = 0;
		bcopy(new_wifi_sr, if_wifi_sr, sizeof(*if_wifi_sr));

		/*
		 * Update the bandwidth values if we got recent values
		 * reported through the other KPI.
		 */
		if (!(new_wifi_sr->valid_bitmask &
		    IF_WIFI_UL_MAX_BANDWIDTH_VALID) &&
		    ifp->if_output_bw.max_bw > 0) {
			if_wifi_sr->valid_bitmask |=
			    IF_WIFI_UL_MAX_BANDWIDTH_VALID;
			if_wifi_sr->ul_max_bandwidth =
			    ifp->if_output_bw.max_bw > UINT32_MAX ?
			    UINT32_MAX :
			    (uint32_t)ifp->if_output_bw.max_bw;
		}
		if (!(new_wifi_sr->valid_bitmask &
		    IF_WIFI_UL_EFFECTIVE_BANDWIDTH_VALID) &&
		    ifp->if_output_bw.eff_bw > 0) {
			if_wifi_sr->valid_bitmask |=
			    IF_WIFI_UL_EFFECTIVE_BANDWIDTH_VALID;
			if_wifi_sr->ul_effective_bandwidth =
			    ifp->if_output_bw.eff_bw > UINT32_MAX ?
			    UINT32_MAX :
			    (uint32_t)ifp->if_output_bw.eff_bw;
		}
		if (!(new_wifi_sr->valid_bitmask &
		    IF_WIFI_DL_MAX_BANDWIDTH_VALID) &&
		    ifp->if_input_bw.max_bw > 0) {
			if_wifi_sr->valid_bitmask |=
			    IF_WIFI_DL_MAX_BANDWIDTH_VALID;
			if_wifi_sr->dl_max_bandwidth =
			    ifp->if_input_bw.max_bw > UINT32_MAX ?
			    UINT32_MAX :
			    (uint32_t)ifp->if_input_bw.max_bw;
		}
		if (!(new_wifi_sr->valid_bitmask &
		    IF_WIFI_DL_EFFECTIVE_BANDWIDTH_VALID) &&
		    ifp->if_input_bw.eff_bw > 0) {
			if_wifi_sr->valid_bitmask |=
			    IF_WIFI_DL_EFFECTIVE_BANDWIDTH_VALID;
			if_wifi_sr->dl_effective_bandwidth =
			    ifp->if_input_bw.eff_bw > UINT32_MAX ?
			    UINT32_MAX :
			    (uint32_t)ifp->if_input_bw.eff_bw;
		}
	}

done:
	lck_rw_done(&ifp->if_link_status_lock);
	ifnet_lock_done(ifp);
	return err;
}

/*************************************************************************/
/* Fastlane QoS Ca						*/
/*************************************************************************/

errno_t
ifnet_set_fastlane_capable(ifnet_t interface, boolean_t capable)
{
	if (interface == NULL) {
		return EINVAL;
	}

	if_set_qosmarking_mode(interface,
	    capable ? IFRTYPE_QOSMARKING_FASTLANE : IFRTYPE_QOSMARKING_MODE_NONE);

	return 0;
}

errno_t
ifnet_get_fastlane_capable(ifnet_t interface, boolean_t *capable)
{
	if (interface == NULL || capable == NULL) {
		return EINVAL;
	}
	if (interface->if_qosmarking_mode == IFRTYPE_QOSMARKING_FASTLANE) {
		*capable = true;
	} else {
		*capable = false;
	}
	return 0;
}

errno_t
ifnet_get_unsent_bytes(ifnet_t interface, int64_t *unsent_bytes)
{
	int64_t bytes;

	if (interface == NULL || unsent_bytes == NULL) {
		return EINVAL;
	}

	bytes = *unsent_bytes = 0;

	if (!IF_FULLY_ATTACHED(interface)) {
		return ENXIO;
	}

	bytes = interface->if_sndbyte_unsent;

	if (interface->if_eflags & IFEF_TXSTART) {
		bytes += IFCQ_BYTES(interface->if_snd);
	}
	*unsent_bytes = bytes;

	return 0;
}

errno_t
ifnet_get_buffer_status(const ifnet_t ifp, ifnet_buffer_status_t *buf_status)
{
	if (ifp == NULL || buf_status == NULL) {
		return EINVAL;
	}

	bzero(buf_status, sizeof(*buf_status));

	if (!IF_FULLY_ATTACHED(ifp)) {
		return ENXIO;
	}

	if (ifp->if_eflags & IFEF_TXSTART) {
		buf_status->buf_interface = IFCQ_BYTES(ifp->if_snd);
	}

	buf_status->buf_sndbuf = ((buf_status->buf_interface != 0) ||
	    (ifp->if_sndbyte_unsent != 0)) ? 1 : 0;

	return 0;
}

void
ifnet_normalise_unsent_data(void)
{
	struct ifnet *ifp;

	ifnet_head_lock_shared();
	TAILQ_FOREACH(ifp, &ifnet_head, if_link) {
		ifnet_lock_exclusive(ifp);
		if (!IF_FULLY_ATTACHED(ifp)) {
			ifnet_lock_done(ifp);
			continue;
		}
		if (!(ifp->if_eflags & IFEF_TXSTART)) {
			ifnet_lock_done(ifp);
			continue;
		}

		if (ifp->if_sndbyte_total > 0 ||
		    IFCQ_BYTES(ifp->if_snd) > 0) {
			ifp->if_unsent_data_cnt++;
		}

		ifnet_lock_done(ifp);
	}
	ifnet_head_done();
}

errno_t
ifnet_set_low_power_mode(ifnet_t ifp, boolean_t on)
{
	errno_t error;

	error = if_set_low_power(ifp, on);

	return error;
}

errno_t
ifnet_get_low_power_mode(ifnet_t ifp, boolean_t *on)
{
	if (ifp == NULL || on == NULL) {
		return EINVAL;
	}

	*on = ((ifp->if_xflags & IFXF_LOW_POWER) != 0);
	return 0;
}
