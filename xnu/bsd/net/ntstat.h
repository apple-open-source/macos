/*
 * Copyright (c) 2010-2024 Apple Inc. All rights reserved.
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
#ifndef __NTSTAT_H__
#define __NTSTAT_H__
#include <stdbool.h>
#include <netinet/in.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/net_api_stats.h>
#include <netinet/in_stat.h>
#include <netinet/tcp.h>

#ifdef PRIVATE
#pragma mark -- Common Data Structures --

#define __NSTAT_REVISION__      9

typedef u_int32_t       nstat_provider_id_t;
typedef u_int64_t       nstat_src_ref_t;
typedef u_int64_t       nstat_event_flags_t;

// The following event definitions are very provisional..
enum{
	NSTAT_EVENT_SRC_ADDED                    = 0x00000001
	, NSTAT_EVENT_SRC_REMOVED                = 0x00000002
	, NSTAT_EVENT_SRC_QUERIED                = 0x00000004
	, NSTAT_EVENT_SRC_QUERIED_ALL            = 0x00000008
	, NSTAT_EVENT_SRC_WILL_CHANGE_STATE      = 0x00000010
	, NSTAT_EVENT_SRC_DID_CHANGE_STATE       = 0x00000020
	, NSTAT_EVENT_SRC_WILL_CHANGE_OWNER      = 0x00000040
	, NSTAT_EVENT_SRC_DID_CHANGE_OWNER       = 0x00000080
	, NSTAT_EVENT_SRC_WILL_CHANGE_PROPERTY   = 0x00000100
	, NSTAT_EVENT_SRC_DID_CHANGE_PROPERTY    = 0x00000200
	, NSTAT_EVENT_SRC_ENTER_CELLFALLBACK     = 0x00000400
	, NSTAT_EVENT_SRC_EXIT_CELLFALLBACK      = 0x00000800
	, NSTAT_EVENT_SRC_FLOW_STATE_LISTEN      = 0x00001000
	, NSTAT_EVENT_SRC_FLOW_STATE_OUTBOUND    = 0x00002000
	, NSTAT_EVENT_SRC_FLOW_UUID_ASSIGNED     = 0x00004000
	, NSTAT_EVENT_SRC_FLOW_UUID_CHANGED      = 0x00008000
#if (DEBUG || DEVELOPMENT)
	, NSTAT_EVENT_SRC_RESERVED_1             = 0x00010000
	, NSTAT_EVENT_SRC_RESERVED_2             = 0x00020000
#endif /* (DEBUG || DEVELOPMENT) */
	, NSTAT_EVENT_SRC_PREV_EVENT_DISCARDED   = 0x80000000
};

typedef struct nstat_counts {
	/* Counters */
	u_int64_t       nstat_rxpackets __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_rxbytes   __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_txpackets __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_txbytes   __attribute__((aligned(sizeof(u_int64_t))));

	u_int64_t       nstat_cell_rxbytes      __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_cell_txbytes      __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_wifi_rxbytes      __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_wifi_txbytes      __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_wired_rxbytes     __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       nstat_wired_txbytes     __attribute__((aligned(sizeof(u_int64_t))));

	u_int32_t       nstat_rxduplicatebytes;
	u_int32_t       nstat_rxoutoforderbytes;
	u_int32_t       nstat_txretransmit;

	u_int32_t       nstat_connectattempts;
	u_int32_t       nstat_connectsuccesses;

	u_int32_t       nstat_min_rtt;
	u_int32_t       nstat_avg_rtt;
	u_int32_t       nstat_var_rtt;
} nstat_counts;

#define NSTAT_SYSINFO_KEYVAL_STRING_MAXSIZE     24
typedef struct nstat_sysinfo_keyval {
	u_int32_t       nstat_sysinfo_key;
	u_int32_t       nstat_sysinfo_flags;
	union {
		int64_t nstat_sysinfo_scalar;
		double  nstat_sysinfo_distribution;
		u_int8_t nstat_sysinfo_string[NSTAT_SYSINFO_KEYVAL_STRING_MAXSIZE];
	} u;
	u_int32_t       nstat_sysinfo_valsize;
	u_int8_t        reserved[4];
}  nstat_sysinfo_keyval;

#define NSTAT_SYSINFO_FLAG_SCALAR       0x0001
#define NSTAT_SYSINFO_FLAG_DISTRIBUTION 0x0002
#define NSTAT_SYSINFO_FLAG_STRING       0x0004

#define NSTAT_MAX_MSG_SIZE      4096

typedef struct nstat_sysinfo_counts {
	/* Counters */
	u_int32_t       nstat_sysinfo_len;
	u_int32_t       pad;
	nstat_sysinfo_keyval        nstat_sysinfo_keyvals[];
}  nstat_sysinfo_counts;

enum{
	NSTAT_SYSINFO_KEY_MBUF_256B_TOTAL        = 1
	, NSTAT_SYSINFO_KEY_MBUF_2KB_TOTAL       = 2
	, NSTAT_SYSINFO_KEY_MBUF_4KB_TOTAL       = 3
	, NSTAT_SYSINFO_KEY_SOCK_MBCNT           = 4
	, NSTAT_SYSINFO_KEY_SOCK_ATMBLIMIT       = 5
	, NSTAT_SYSINFO_KEY_IPV4_AVGRTT          = 6
	, NSTAT_SYSINFO_KEY_IPV6_AVGRTT          = 7
	, NSTAT_SYSINFO_KEY_SEND_PLR             = 8
	, NSTAT_SYSINFO_KEY_RECV_PLR             = 9
	, NSTAT_SYSINFO_KEY_SEND_TLRTO           = 10
	, NSTAT_SYSINFO_KEY_SEND_REORDERRATE     = 11
	, NSTAT_SYSINFO_CONNECTION_ATTEMPTS      = 12
	, NSTAT_SYSINFO_CONNECTION_ACCEPTS       = 13
	, NSTAT_SYSINFO_ECN_CLIENT_SETUP         = 14
	, NSTAT_SYSINFO_ECN_SERVER_SETUP         = 15
	, NSTAT_SYSINFO_ECN_CLIENT_SUCCESS       = 16
	, NSTAT_SYSINFO_ECN_SERVER_SUCCESS       = 17
	, NSTAT_SYSINFO_ECN_NOT_SUPPORTED        = 18
	, NSTAT_SYSINFO_ECN_LOST_SYN             = 19
	, NSTAT_SYSINFO_ECN_LOST_SYNACK          = 20
	, NSTAT_SYSINFO_ECN_RECV_CE              = 21
	, NSTAT_SYSINFO_ECN_RECV_ECE             = 22
	, NSTAT_SYSINFO_ECN_SENT_ECE             = 23
	, NSTAT_SYSINFO_ECN_CONN_RECV_CE         = 24
	, NSTAT_SYSINFO_ECN_CONN_PLNOCE          = 25
	, NSTAT_SYSINFO_ECN_CONN_PL_CE           = 26
	, NSTAT_SYSINFO_ECN_CONN_NOPL_CE         = 27
	, NSTAT_SYSINFO_MBUF_16KB_TOTAL          = 28
	, NSTAT_SYSINFO_ECN_CLIENT_ENABLED       = 29
	, NSTAT_SYSINFO_ECN_SERVER_ENABLED       = 30
	, NSTAT_SYSINFO_ECN_CONN_RECV_ECE        = 31
	, NSTAT_SYSINFO_MBUF_MEM_RELEASED        = 32
	, NSTAT_SYSINFO_MBUF_DRAIN_CNT           = 33
	, NSTAT_SYSINFO_TFO_SYN_DATA_RCV         = 34
	, NSTAT_SYSINFO_TFO_COOKIE_REQ_RCV       = 35
	, NSTAT_SYSINFO_TFO_COOKIE_SENT          = 36
	, NSTAT_SYSINFO_TFO_COOKIE_INVALID       = 37
	, NSTAT_SYSINFO_TFO_COOKIE_REQ           = 38
	, NSTAT_SYSINFO_TFO_COOKIE_RCV           = 39
	, NSTAT_SYSINFO_TFO_SYN_DATA_SENT        = 40
	, NSTAT_SYSINFO_TFO_SYN_DATA_ACKED       = 41
	, NSTAT_SYSINFO_TFO_SYN_LOSS             = 42
	, NSTAT_SYSINFO_TFO_BLACKHOLE            = 43
	, NSTAT_SYSINFO_ECN_FALLBACK_SYNLOSS     = 44
	, NSTAT_SYSINFO_ECN_FALLBACK_REORDER     = 45
	, NSTAT_SYSINFO_ECN_FALLBACK_CE          = 46
	, NSTAT_SYSINFO_ECN_IFNET_TYPE           = 47
	, NSTAT_SYSINFO_ECN_IFNET_PROTO          = 48
	, NSTAT_SYSINFO_ECN_IFNET_CLIENT_SETUP   = 49
	, NSTAT_SYSINFO_ECN_IFNET_SERVER_SETUP   = 50
	, NSTAT_SYSINFO_ECN_IFNET_CLIENT_SUCCESS = 51
	, NSTAT_SYSINFO_ECN_IFNET_SERVER_SUCCESS = 52
	, NSTAT_SYSINFO_ECN_IFNET_PEER_NOSUPPORT = 53
	, NSTAT_SYSINFO_ECN_IFNET_SYN_LOST       = 54
	, NSTAT_SYSINFO_ECN_IFNET_SYNACK_LOST    = 55
	, NSTAT_SYSINFO_ECN_IFNET_RECV_CE        = 56
	, NSTAT_SYSINFO_ECN_IFNET_RECV_ECE       = 57
	, NSTAT_SYSINFO_ECN_IFNET_SENT_ECE       = 58
	, NSTAT_SYSINFO_ECN_IFNET_CONN_RECV_CE   = 59
	, NSTAT_SYSINFO_ECN_IFNET_CONN_RECV_ECE  = 60
	, NSTAT_SYSINFO_ECN_IFNET_CONN_PLNOCE    = 61
	, NSTAT_SYSINFO_ECN_IFNET_CONN_PLCE      = 62
	, NSTAT_SYSINFO_ECN_IFNET_CONN_NOPLCE    = 63
	, NSTAT_SYSINFO_ECN_IFNET_FALLBACK_SYNLOSS = 64
	, NSTAT_SYSINFO_ECN_IFNET_FALLBACK_REORDER = 65
	, NSTAT_SYSINFO_ECN_IFNET_FALLBACK_CE    = 66
	, NSTAT_SYSINFO_ECN_IFNET_ON_RTT_AVG     = 67
	, NSTAT_SYSINFO_ECN_IFNET_ON_RTT_VAR     = 68
	, NSTAT_SYSINFO_ECN_IFNET_ON_OOPERCENT   = 69
	, NSTAT_SYSINFO_ECN_IFNET_ON_SACK_EPISODE = 70
	, NSTAT_SYSINFO_ECN_IFNET_ON_REORDER_PERCENT = 71
	, NSTAT_SYSINFO_ECN_IFNET_ON_RXMIT_PERCENT = 72
	, NSTAT_SYSINFO_ECN_IFNET_ON_RXMIT_DROP  = 73
	, NSTAT_SYSINFO_ECN_IFNET_OFF_RTT_AVG    = 74
	, NSTAT_SYSINFO_ECN_IFNET_OFF_RTT_VAR    = 75
	, NSTAT_SYSINFO_ECN_IFNET_OFF_OOPERCENT  = 76
	, NSTAT_SYSINFO_ECN_IFNET_OFF_SACK_EPISODE = 77
	, NSTAT_SYSINFO_ECN_IFNET_OFF_REORDER_PERCENT = 78
	, NSTAT_SYSINFO_ECN_IFNET_OFF_RXMIT_PERCENT = 79
	, NSTAT_SYSINFO_ECN_IFNET_OFF_RXMIT_DROP = 80
	, NSTAT_SYSINFO_ECN_IFNET_ON_TOTAL_TXPKTS = 81
	, NSTAT_SYSINFO_ECN_IFNET_ON_TOTAL_RXMTPKTS = 82
	, NSTAT_SYSINFO_ECN_IFNET_ON_TOTAL_RXPKTS = 83
	, NSTAT_SYSINFO_ECN_IFNET_ON_TOTAL_OOPKTS = 84
	, NSTAT_SYSINFO_ECN_IFNET_ON_DROP_RST = 85
	, NSTAT_SYSINFO_ECN_IFNET_OFF_TOTAL_TXPKTS = 86
	, NSTAT_SYSINFO_ECN_IFNET_OFF_TOTAL_RXMTPKTS = 87
	, NSTAT_SYSINFO_ECN_IFNET_OFF_TOTAL_RXPKTS = 88
	, NSTAT_SYSINFO_ECN_IFNET_OFF_TOTAL_OOPKTS = 89
	, NSTAT_SYSINFO_ECN_IFNET_OFF_DROP_RST = 90
	, NSTAT_SYSINFO_ECN_IFNET_TOTAL_CONN = 91
	, NSTAT_SYSINFO_TFO_COOKIE_WRONG = 92
	, NSTAT_SYSINFO_TFO_NO_COOKIE_RCV = 93
	, NSTAT_SYSINFO_TFO_HEURISTICS_DISABLE = 94
	, NSTAT_SYSINFO_TFO_SEND_BLACKHOLE = 95
	, NSTAT_SYSINFO_KEY_SOCK_MBFLOOR = 96
	, NSTAT_SYSINFO_IFNET_UNSENT_DATA = 97
	, NSTAT_SYSINFO_ECN_IFNET_FALLBACK_DROPRST = 98
	, NSTAT_SYSINFO_ECN_IFNET_FALLBACK_DROPRXMT = 99
	, NSTAT_SYSINFO_LIM_IFNET_SIGNATURE = 100
	, NSTAT_SYSINFO_LIM_IFNET_DL_MAX_BANDWIDTH = 101
	, NSTAT_SYSINFO_LIM_IFNET_UL_MAX_BANDWIDTH = 102
	, NSTAT_SYSINFO_LIM_IFNET_PACKET_LOSS_PERCENT = 103
	, NSTAT_SYSINFO_LIM_IFNET_PACKET_OOO_PERCENT = 104
	, NSTAT_SYSINFO_LIM_IFNET_RTT_VARIANCE = 105
	, NSTAT_SYSINFO_LIM_IFNET_RTT_MIN = 106
	, NSTAT_SYSINFO_LIM_IFNET_RTT_AVG = 107
	, NSTAT_SYSINFO_LIM_IFNET_CONN_TIMEOUT_PERCENT = 108
	, NSTAT_SYSINFO_LIM_IFNET_DL_DETECTED = 109
	, NSTAT_SYSINFO_LIM_IFNET_UL_DETECTED = 110
	, NSTAT_SYSINFO_LIM_IFNET_TYPE = 111

	, NSTAT_SYSINFO_API_IF_FLTR_ATTACH = 112
	, NSTAT_SYSINFO_API_IF_FLTR_ATTACH_OS = 113
	, NSTAT_SYSINFO_API_IP_FLTR_ADD = 114
	, NSTAT_SYSINFO_API_IP_FLTR_ADD_OS = 115
	, NSTAT_SYSINFO_API_SOCK_FLTR_ATTACH = 116
	, NSTAT_SYSINFO_API_SOCK_FLTR_ATTACH_OS = 117

	, NSTAT_SYSINFO_API_SOCK_ALLOC_TOTAL = 118
	, NSTAT_SYSINFO_API_SOCK_ALLOC_KERNEL = 119
	, NSTAT_SYSINFO_API_SOCK_ALLOC_KERNEL_OS = 120
	, NSTAT_SYSINFO_API_SOCK_NECP_CLIENTUUID = 121

	, NSTAT_SYSINFO_API_SOCK_DOMAIN_LOCAL = 122
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_ROUTE = 123
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_INET = 124
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_INET6 = 125
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_SYSTEM = 126
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_MULTIPATH = 127
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_KEY = 128
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_NDRV = 129
	, NSTAT_SYSINFO_API_SOCK_DOMAIN_OTHER = 130

	, NSTAT_SYSINFO_API_SOCK_INET_STREAM= 131
	, NSTAT_SYSINFO_API_SOCK_INET_DGRAM = 132
	, NSTAT_SYSINFO_API_SOCK_INET_DGRAM_CONNECTED = 133
	, NSTAT_SYSINFO_API_SOCK_INET_DGRAM_DNS = 134
	, NSTAT_SYSINFO_API_SOCK_INET_DGRAM_NO_DATA = 135

	, NSTAT_SYSINFO_API_SOCK_INET6_STREAM= 136
	, NSTAT_SYSINFO_API_SOCK_INET6_DGRAM = 137
	, NSTAT_SYSINFO_API_SOCK_INET6_DGRAM_CONNECTED = 138
	, NSTAT_SYSINFO_API_SOCK_INET6_DGRAM_DNS = 139
	, NSTAT_SYSINFO_API_SOCK_INET6_DGRAM_NO_DATA = 140

	, NSTAT_SYSINFO_API_SOCK_INET_MCAST_JOIN = 141
	, NSTAT_SYSINFO_API_SOCK_INET_MCAST_JOIN_OS = 142

	, NSTAT_SYSINFO_API_SOCK_INET6_STREAM_EXTHDR_IN = 143
	, NSTAT_SYSINFO_API_SOCK_INET6_STREAM_EXTHDR_OUT = 144
	, NSTAT_SYSINFO_API_SOCK_INET6_DGRAM_EXTHDR_IN = 145
	, NSTAT_SYSINFO_API_SOCK_INET6_DGRAM_EXTHDR_OUT = 146

	, NSTAT_SYSINFO_API_NEXUS_FLOW_INET_STREAM = 147
	, NSTAT_SYSINFO_API_NEXUS_FLOW_INET_DATAGRAM = 148

	, NSTAT_SYSINFO_API_NEXUS_FLOW_INET6_STREAM = 149
	, NSTAT_SYSINFO_API_NEXUS_FLOW_INET6_DATAGRAM = 150

	, NSTAT_SYSINFO_API_IFNET_ALLOC = 151
	, NSTAT_SYSINFO_API_IFNET_ALLOC_OS = 152

	, NSTAT_SYSINFO_API_PF_ADDRULE = 153
	, NSTAT_SYSINFO_API_PF_ADDRULE_OS = 154

	, NSTAT_SYSINFO_API_VMNET_START = 155

	, NSTAT_SYSINFO_API_IF_NETAGENT_ENABLED = 156

	, NSTAT_SYSINFO_API_REPORT_INTERVAL = 157

	, NSTAT_SYSINFO_MPTCP_HANDOVER_ATTEMPT = 158
	, NSTAT_SYSINFO_MPTCP_INTERACTIVE_ATTEMPT = 159
	, NSTAT_SYSINFO_MPTCP_AGGREGATE_ATTEMPT = 160
	, NSTAT_SYSINFO_MPTCP_FP_HANDOVER_ATTEMPT = 161 /* _FP_ stands for first-party */
	, NSTAT_SYSINFO_MPTCP_FP_INTERACTIVE_ATTEMPT = 162
	, NSTAT_SYSINFO_MPTCP_FP_AGGREGATE_ATTEMPT = 163
	, NSTAT_SYSINFO_MPTCP_HEURISTIC_FALLBACK = 164
	, NSTAT_SYSINFO_MPTCP_FP_HEURISTIC_FALLBACK = 165
	, NSTAT_SYSINFO_MPTCP_HANDOVER_SUCCESS_WIFI = 166
	, NSTAT_SYSINFO_MPTCP_HANDOVER_SUCCESS_CELL = 167
	, NSTAT_SYSINFO_MPTCP_INTERACTIVE_SUCCESS = 168
	, NSTAT_SYSINFO_MPTCP_AGGREGATE_SUCCESS = 169
	, NSTAT_SYSINFO_MPTCP_FP_HANDOVER_SUCCESS_WIFI = 170
	, NSTAT_SYSINFO_MPTCP_FP_HANDOVER_SUCCESS_CELL = 171
	, NSTAT_SYSINFO_MPTCP_FP_INTERACTIVE_SUCCESS = 172
	, NSTAT_SYSINFO_MPTCP_FP_AGGREGATE_SUCCESS = 173
	, NSTAT_SYSINFO_MPTCP_HANDOVER_CELL_FROM_WIFI = 174
	, NSTAT_SYSINFO_MPTCP_HANDOVER_WIFI_FROM_CELL = 175
	, NSTAT_SYSINFO_MPTCP_INTERACTIVE_CELL_FROM_WIFI = 176
	, NSTAT_SYSINFO_MPTCP_HANDOVER_CELL_BYTES = 177
	, NSTAT_SYSINFO_MPTCP_INTERACTIVE_CELL_BYTES = 178
	, NSTAT_SYSINFO_MPTCP_AGGREGATE_CELL_BYTES = 179
	, NSTAT_SYSINFO_MPTCP_HANDOVER_ALL_BYTES = 180
	, NSTAT_SYSINFO_MPTCP_INTERACTIVE_ALL_BYTES = 181
	, NSTAT_SYSINFO_MPTCP_AGGREGATE_ALL_BYTES = 182
	, NSTAT_SYSINFO_MPTCP_BACK_TO_WIFI = 183
	, NSTAT_SYSINFO_MPTCP_WIFI_PROXY = 184
	, NSTAT_SYSINFO_MPTCP_CELL_PROXY = 185
	, NSTAT_SYSINFO_ECN_IFNET_FALLBACK_SYNRST = 186
	, NSTAT_SYSINFO_MPTCP_TRIGGERED_CELL = 187

// NSTAT_SYSINFO_ENUM_VERSION must be updated any time a value is added
#define NSTAT_SYSINFO_ENUM_VERSION      20180416
};

#define NSTAT_SYSINFO_API_FIRST NSTAT_SYSINFO_API_IF_FLTR_ATTACH
#define NSTAT_SYSINFO_API_LAST  NSTAT_SYSINFO_API_REPORT_INTERVAL

#pragma mark -- Network Statistics Providers --


// Interface properties.  These are constrained to fit in a 32 bit word

#define NSTAT_IFNET_IS_UNKNOWN_TYPE                 0x00000001
#define NSTAT_IFNET_IS_LOOPBACK                     0x00000002
#define NSTAT_IFNET_IS_CELLULAR                     0x00000004
#define NSTAT_IFNET_IS_WIFI                         0x00000008
#define NSTAT_IFNET_IS_WIRED                        0x00000010
#define NSTAT_IFNET_IS_AWDL                         0x00000020
#define NSTAT_IFNET_IS_EXPENSIVE                    0x00000040
#define NSTAT_IFNET_IS_VPN                          0x00000080
#define NSTAT_IFNET_VIA_CELLFALLBACK                0x00000100
#define NSTAT_IFNET_IS_COMPANIONLINK                0x00000200
#define NSTAT_IFNET_IS_CONSTRAINED                  0x00000400

// The following local and non-local flags are set only if fully known
// They are mutually exclusive but there is no guarantee that one or the other will be set
#define NSTAT_IFNET_IS_LOCAL                        0x00000800
#define NSTAT_IFNET_IS_NON_LOCAL                    0x00001000

// Properties relating to userland providers
#define NSTAT_IFNET_ROUTE_VALUE_UNOBTAINABLE        0x00002000
#define NSTAT_IFNET_FLOWSWITCH_VALUE_UNOBTAINABLE   0x00004000

// Additional interface properties
#define NSTAT_IFNET_IS_LLW                          0x00008000
#define NSTAT_IFNET_IS_WIFI_INFRA                   0x00010000
#define NSTAT_IFNET_PEEREGRESSINTERFACE_IS_CELLULAR 0x00020000
#define NSTAT_IFNET_IS_COMPANIONLINK_BT             0x00040000

// Not interface properties, but used for filtering in similar fashion
#define NSTAT_NECP_CONN_HAS_NET_ACCESS              0x01000000

// Not interface properties but conveniently handled in the same flags word
#define NSTAT_SOURCE_IS_LISTENER                    0x02000000
#define NSTAT_SOURCE_IS_INBOUND                     0x04000000
#define NSTAT_SOURCE_IS_OUTBOUND                    0x08000000


typedef enum {
	NSTAT_PROVIDER_NONE           = 0
	, NSTAT_PROVIDER_ROUTE        = 1
	, NSTAT_PROVIDER_TCP_KERNEL   = 2
	, NSTAT_PROVIDER_TCP_USERLAND = 3
	, NSTAT_PROVIDER_UDP_KERNEL   = 4
	, NSTAT_PROVIDER_UDP_USERLAND = 5
	, NSTAT_PROVIDER_IFNET        = 6
	, NSTAT_PROVIDER_SYSINFO      = 7
	, NSTAT_PROVIDER_QUIC_USERLAND = 8
	, NSTAT_PROVIDER_CONN_USERLAND = 9
	, NSTAT_PROVIDER_UDP_SUBFLOW   = 10
} nstat_provider_type_t;
#define NSTAT_PROVIDER_LAST NSTAT_PROVIDER_UDP_SUBFLOW
#define NSTAT_PROVIDER_COUNT (NSTAT_PROVIDER_LAST+1)

typedef struct nstat_route_add_param {
	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} dst;
	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} mask;
	u_int32_t       ifindex;
} nstat_route_add_param;

typedef struct nstat_tcp_add_param {
	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} local;
	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} remote;
} nstat_tcp_add_param;

typedef struct nstat_interface_counts {
	u_int64_t       nstat_rxpackets;
	u_int64_t       nstat_rxbytes;
	u_int64_t       nstat_txpackets;
	u_int64_t       nstat_txbytes;
} nstat_interface_counts;

#define NSTAT_MAX_DOMAIN_NAME_LENGTH           256 /* As per RFC 2181 for full domain name */
#define NSTAT_MAX_DOMAIN_OWNER_LENGTH          256
#define NSTAT_MAX_DOMAIN_TRACKER_CONTEXT       256
#define NSTAT_MAX_DOMAIN_ATTR_BUNDLE_ID        256

typedef struct nstat_domain_info {
	char            domain_name[NSTAT_MAX_DOMAIN_NAME_LENGTH];
	char            domain_owner[NSTAT_MAX_DOMAIN_OWNER_LENGTH];
	char            domain_tracker_ctxt[NSTAT_MAX_DOMAIN_TRACKER_CONTEXT];
	char            domain_attributed_bundle_id[NSTAT_MAX_DOMAIN_ATTR_BUNDLE_ID];
	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} remote;
	bool            is_tracker;
	bool            is_non_app_initiated;
	bool            is_silent;
	uint8_t         reserved[1];
} nstat_domain_info __attribute__((aligned(sizeof(u_int64_t))));

typedef struct nstat_tcp_descriptor {
	u_int64_t       upid __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       eupid __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       start_timestamp __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       timestamp __attribute__((aligned(sizeof(u_int64_t))));

	u_int64_t       rx_transfer_size __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       tx_transfer_size __attribute__((aligned(sizeof(u_int64_t))));

	activity_bitmap_t activity_bitmap;

	u_int32_t       ifindex;
	u_int32_t       state;

	u_int32_t       sndbufsize;
	u_int32_t       sndbufused;
	u_int32_t       rcvbufsize;
	u_int32_t       rcvbufused;
	u_int32_t       txunacked;
	u_int32_t       txwindow;
	u_int32_t       txcwindow;
	u_int32_t       traffic_class;
	u_int32_t       traffic_mgt_flags;

	u_int32_t       pid;
	u_int32_t       epid;

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} local;

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} remote;

	char            cc_algo[16];
	char            pname[64];

	uuid_t          uuid;
	uuid_t          euuid;
	uuid_t          vuuid;
	uuid_t          fuuid;
	uid_t           persona_id;
	uid_t           uid;
	union {
		struct tcp_conn_status connstatus;
		// On armv7k, tcp_conn_status is 1 byte instead of 4
		uint8_t                                 __pad_connstatus[4];
	};
	uint32_t        ifnet_properties        __attribute__((aligned(4)));
	uint8_t         fallback_mode;
	uint8_t         reserved[3];
} nstat_tcp_descriptor;

typedef struct nstat_tcp_add_param      nstat_udp_add_param;

typedef struct nstat_udp_descriptor {
	u_int64_t       upid __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       eupid __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       start_timestamp __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       timestamp __attribute__((aligned(sizeof(u_int64_t))));

	activity_bitmap_t activity_bitmap;

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} local;

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
	} remote;

	u_int32_t       ifindex;

	u_int32_t       rcvbufsize;
	u_int32_t       rcvbufused;
	u_int32_t       traffic_class;

	u_int32_t       pid;
	char            pname[64];
	u_int32_t       epid;

	uuid_t          uuid;
	uuid_t          euuid;
	uuid_t          vuuid;
	uuid_t          fuuid;
	uid_t           persona_id;
	uid_t           uid;
	uint32_t        ifnet_properties;
	uint8_t         fallback_mode;
	uint8_t         reserved[3];
} nstat_udp_descriptor;

/*
 * XXX For now just typedef'ing TCP Nstat descriptor to nstat_quic_descriptor
 * as for now they report very similar data.
 * Later when we extend the QUIC descriptor we can just declare its own
 * descriptor struct.
 */
typedef struct nstat_tcp_add_param      nstat_quic_add_param;
typedef struct nstat_tcp_descriptor     nstat_quic_descriptor;

typedef struct nstat_connection_descriptor {
	u_int64_t       start_timestamp __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       timestamp;
	u_int64_t       upid;
	u_int64_t       eupid;

	u_int32_t       pid;
	u_int32_t       epid;
	u_int32_t       ifnet_properties;
	char            pname[64];
	uuid_t          uuid;   /* UUID of the app */
	uuid_t          euuid;  /* Effective UUID */
	uuid_t          cuuid;  /* Connection UUID */
	uuid_t          puuid;  /* Parent UUID */
	uuid_t          fuuid;  /* Flow UUID */
	uid_t           persona_id;
	uid_t           uid;
	uint8_t         reserved[4];
} nstat_connection_descriptor;

typedef struct nstat_route_descriptor {
	u_int64_t       id __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       parent_id __attribute__((aligned(sizeof(u_int64_t))));
	u_int64_t       gateway_id __attribute__((aligned(sizeof(u_int64_t))));

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
		struct sockaddr         sa;
	} dst;

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
		struct sockaddr         sa;
	} mask;

	union{
		struct sockaddr_in      v4;
		struct sockaddr_in6     v6;
		struct sockaddr         sa;
	} gateway;

	u_int32_t       ifindex;
	u_int32_t       flags;

	u_int8_t        reserved[4];
} nstat_route_descriptor;

typedef struct nstat_ifnet_add_param {
	u_int64_t       threshold __attribute__((aligned(sizeof(u_int64_t))));
	u_int32_t       ifindex;

	u_int8_t        reserved[4];
} nstat_ifnet_add_param;

typedef struct nstat_ifnet_desc_cellular_status {
	u_int32_t valid_bitmask; /* indicates which fields are valid */
#define NSTAT_IFNET_DESC_CELL_LINK_QUALITY_METRIC_VALID         0x1
#define NSTAT_IFNET_DESC_CELL_UL_EFFECTIVE_BANDWIDTH_VALID      0x2
#define NSTAT_IFNET_DESC_CELL_UL_MAX_BANDWIDTH_VALID            0x4
#define NSTAT_IFNET_DESC_CELL_UL_MIN_LATENCY_VALID              0x8
#define NSTAT_IFNET_DESC_CELL_UL_EFFECTIVE_LATENCY_VALID        0x10
#define NSTAT_IFNET_DESC_CELL_UL_MAX_LATENCY_VALID              0x20
#define NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_VALID              0x40
#define NSTAT_IFNET_DESC_CELL_UL_BYTES_LOST_VALID               0x80
#define NSTAT_IFNET_DESC_CELL_UL_MIN_QUEUE_SIZE_VALID           0x100
#define NSTAT_IFNET_DESC_CELL_UL_AVG_QUEUE_SIZE_VALID           0x200
#define NSTAT_IFNET_DESC_CELL_UL_MAX_QUEUE_SIZE_VALID           0x400
#define NSTAT_IFNET_DESC_CELL_DL_EFFECTIVE_BANDWIDTH_VALID      0x800
#define NSTAT_IFNET_DESC_CELL_DL_MAX_BANDWIDTH_VALID            0x1000
#define NSTAT_IFNET_DESC_CELL_CONFIG_INACTIVITY_TIME_VALID      0x2000
#define NSTAT_IFNET_DESC_CELL_CONFIG_BACKOFF_TIME_VALID         0x4000
#define NSTAT_IFNET_DESC_CELL_MSS_RECOMMENDED_VALID             0x8000
	u_int32_t link_quality_metric;
	u_int32_t ul_effective_bandwidth; /* Measured uplink bandwidth based on
	                                   *  current activity (bps) */
	u_int32_t ul_max_bandwidth; /* Maximum supported uplink bandwidth
	                             *  (bps) */
	u_int32_t ul_min_latency; /* min expected uplink latency for first hop
	                           *  (ms) */
	u_int32_t ul_effective_latency; /* current expected uplink latency for
	                                 *  first hop (ms) */
	u_int32_t ul_max_latency; /* max expected uplink latency first hop
	                           *  (ms) */
	u_int32_t ul_retxt_level; /* Retransmission metric */
#define NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_NONE       1
#define NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_LOW        2
#define NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_MEDIUM     3
#define NSTAT_IFNET_DESC_CELL_UL_RETXT_LEVEL_HIGH       4

	u_int32_t ul_bytes_lost; /* % of total bytes lost on uplink in Q10
	                          *  format */
	u_int32_t ul_min_queue_size; /* minimum bytes in queue */
	u_int32_t ul_avg_queue_size; /* average bytes in queue */
	u_int32_t ul_max_queue_size; /* maximum bytes in queue */
	u_int32_t dl_effective_bandwidth; /* Measured downlink bandwidth based
	                                   *  on current activity (bps) */
	u_int32_t dl_max_bandwidth; /* Maximum supported downlink bandwidth
	                             *  (bps) */
	u_int32_t config_inactivity_time; /* ms */
	u_int32_t config_backoff_time; /* new connections backoff time in ms */
#define NSTAT_IFNET_DESC_MSS_RECOMMENDED_NONE   0x0
#define NSTAT_IFNET_DESC_MSS_RECOMMENDED_MEDIUM 0x1
#define NSTAT_IFNET_DESC_MSS_RECOMMENDED_LOW    0x2
	u_int16_t mss_recommended; /* recommended MSS */
	u_int8_t        reserved[2];
} nstat_ifnet_desc_cellular_status;

typedef struct nstat_ifnet_desc_wifi_status {
	u_int32_t valid_bitmask;
#define NSTAT_IFNET_DESC_WIFI_LINK_QUALITY_METRIC_VALID         0x1
#define NSTAT_IFNET_DESC_WIFI_UL_EFFECTIVE_BANDWIDTH_VALID      0x2
#define NSTAT_IFNET_DESC_WIFI_UL_MAX_BANDWIDTH_VALID            0x4
#define NSTAT_IFNET_DESC_WIFI_UL_MIN_LATENCY_VALID              0x8
#define NSTAT_IFNET_DESC_WIFI_UL_EFFECTIVE_LATENCY_VALID        0x10
#define NSTAT_IFNET_DESC_WIFI_UL_MAX_LATENCY_VALID              0x20
#define NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_VALID              0x40
#define NSTAT_IFNET_DESC_WIFI_UL_ERROR_RATE_VALID               0x80
#define NSTAT_IFNET_DESC_WIFI_UL_BYTES_LOST_VALID               0x100
#define NSTAT_IFNET_DESC_WIFI_DL_EFFECTIVE_BANDWIDTH_VALID      0x200
#define NSTAT_IFNET_DESC_WIFI_DL_MAX_BANDWIDTH_VALID            0x400
#define NSTAT_IFNET_DESC_WIFI_DL_MIN_LATENCY_VALID              0x800
#define NSTAT_IFNET_DESC_WIFI_DL_EFFECTIVE_LATENCY_VALID        0x1000
#define NSTAT_IFNET_DESC_WIFI_DL_MAX_LATENCY_VALID              0x2000
#define NSTAT_IFNET_DESC_WIFI_DL_ERROR_RATE_VALID               0x4000
#define NSTAT_IFNET_DESC_WIFI_CONFIG_FREQUENCY_VALID            0x8000
#define NSTAT_IFNET_DESC_WIFI_CONFIG_MULTICAST_RATE_VALID       0x10000
#define NSTAT_IFNET_DESC_WIFI_CONFIG_SCAN_COUNT_VALID           0x20000
#define NSTAT_IFNET_DESC_WIFI_CONFIG_SCAN_DURATION_VALID        0x40000
	u_int32_t link_quality_metric; /* link quality metric */
	u_int32_t ul_effective_bandwidth; /* Measured uplink bandwidth based on
	                                   *  current activity (bps) */
	u_int32_t ul_max_bandwidth; /* Maximum supported uplink bandwidth
	                             *  (bps) */
	u_int32_t ul_min_latency; /* min expected uplink latency for first hop
	                           *  (ms) */
	u_int32_t ul_effective_latency; /* current expected uplink latency for
	                                 *  first hop (ms) */
	u_int32_t ul_max_latency; /* max expected uplink latency for first hop
	                           *  (ms) */
	u_int32_t ul_retxt_level; /* Retransmission metric */
#define NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_NONE       1
#define NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_LOW        2
#define NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_MEDIUM     3
#define NSTAT_IFNET_DESC_WIFI_UL_RETXT_LEVEL_HIGH       4

	u_int32_t ul_bytes_lost; /* % of total bytes lost on uplink in Q10
	                          *  format */
	u_int32_t ul_error_rate; /* % of bytes dropped on uplink after many
	                          *  retransmissions in Q10 format */
	u_int32_t dl_effective_bandwidth; /* Measured downlink bandwidth based
	                                   *  on current activity (bps) */
	u_int32_t dl_max_bandwidth; /* Maximum supported downlink bandwidth
	                             *  (bps) */
	/*
	 * The download latency values indicate the time AP may have to wait
	 * for the  driver to receive the packet. These values give the range
	 * of expected latency mainly due to co-existence events and channel
	 * hopping where the interface becomes unavailable.
	 */
	u_int32_t dl_min_latency; /* min expected latency for first hop in ms */
	u_int32_t dl_effective_latency; /* current expected latency for first
	                                 *  hop in ms */
	u_int32_t dl_max_latency; /* max expected latency for first hop in ms */
	u_int32_t dl_error_rate; /* % of CRC or other errors in Q10 format */
	u_int32_t config_frequency; /* 2.4 or 5 GHz */
#define NSTAT_IFNET_DESC_WIFI_CONFIG_FREQUENCY_2_4_GHZ  1
#define NSTAT_IFNET_DESC_WIFI_CONFIG_FREQUENCY_5_0_GHZ  2
	u_int32_t config_multicast_rate; /* bps */
	u_int32_t scan_count; /* scan count during the previous period */
	u_int32_t scan_duration; /* scan duration in ms */
} nstat_ifnet_desc_wifi_status;

enum{
	NSTAT_IFNET_DESC_LINK_STATUS_TYPE_NONE = 0
	, NSTAT_IFNET_DESC_LINK_STATUS_TYPE_CELLULAR = 1
	, NSTAT_IFNET_DESC_LINK_STATUS_TYPE_WIFI = 2
	, NSTAT_IFNET_DESC_LINK_STATUS_TYPE_ETHERNET = 3
};

typedef struct nstat_ifnet_desc_link_status {
	u_int32_t       link_status_type;
	union {
		nstat_ifnet_desc_cellular_status        cellular;
		nstat_ifnet_desc_wifi_status            wifi;
	} u;
} nstat_ifnet_desc_link_status;

#ifndef IF_DESCSIZE
#define IF_DESCSIZE 128
#endif
typedef struct nstat_ifnet_descriptor {
	u_int64_t                       threshold __attribute__((aligned(sizeof(u_int64_t))));
	u_int32_t                       ifindex;
	nstat_ifnet_desc_link_status    link_status;
	unsigned int            type;
	char                            description[IF_DESCSIZE];
	char                            name[IFNAMSIZ + 1];
	u_int8_t                        reserved[3];
} nstat_ifnet_descriptor;

typedef struct nstat_sysinfo_descriptor {
	u_int32_t       flags;
} nstat_sysinfo_descriptor;

typedef struct nstat_sysinfo_add_param {
	/* To indicate which system level information should be collected */
	u_int32_t       flags;
} nstat_sysinfo_add_param;

/* 0x0001 is unused */
#define NSTAT_SYSINFO_TCP_STATS         0x0002
#define NSTAT_SYSINFO_IFNET_ECN_STATS   0x0003
#define NSTAT_SYSINFO_LIM_STATS         0x0004  /* Low Internet mode stats */
#define NSTAT_SYSINFO_NET_API_STATS     0x0005  /* API and KPI stats */

#pragma mark -- Network Statistics User Client --

#define NET_STAT_CONTROL_NAME   "com.apple.network.statistics"

enum{
	// generic response messages
	NSTAT_MSG_TYPE_SUCCESS               = 0
	, NSTAT_MSG_TYPE_ERROR               = 1

	    // Requests
	, NSTAT_MSG_TYPE_ADD_SRC             = 1001
	, NSTAT_MSG_TYPE_ADD_ALL_SRCS        = 1002
	, NSTAT_MSG_TYPE_REM_SRC             = 1003
	, NSTAT_MSG_TYPE_QUERY_SRC           = 1004
	, NSTAT_MSG_TYPE_GET_SRC_DESC        = 1005
	, NSTAT_MSG_TYPE_SET_FILTER          = 1006 // Obsolete
	, NSTAT_MSG_TYPE_GET_UPDATE          = 1007
	, NSTAT_MSG_TYPE_SUBSCRIBE_SYSINFO   = 1008

	    // Responses/Notfications
	, NSTAT_MSG_TYPE_SRC_ADDED           = 10001
	, NSTAT_MSG_TYPE_SRC_REMOVED         = 10002
	, NSTAT_MSG_TYPE_SRC_DESC            = 10003
	, NSTAT_MSG_TYPE_SRC_COUNTS          = 10004
	, NSTAT_MSG_TYPE_SYSINFO_COUNTS      = 10005
	, NSTAT_MSG_TYPE_SRC_UPDATE          = 10006
	, NSTAT_MSG_TYPE_SRC_EXTENDED_UPDATE = 10007
};

enum{
	NSTAT_SRC_REF_ALL       = 0xffffffffffffffffULL
	, NSTAT_SRC_REF_INVALID  = 0
};

/* Source-level filters */
enum{
	NSTAT_FILTER_NOZEROBYTES             = 0x00000001
};


/* Types of extended update information, used in setting initial filters as well as to identify returned extensions */
/* A contiguous range currently limited 1..31 due to being passed as the top 32 bits of filter */
enum{
	NSTAT_EXTENDED_UPDATE_TYPE_UNKNOWN              = 0
	, NSTAT_EXTENDED_UPDATE_TYPE_DOMAIN             = 1
	, NSTAT_EXTENDED_UPDATE_TYPE_NECP_TLV           = 2
	, NSTAT_EXTENDED_UPDATE_TYPE_ORIGINAL_NECP_TLV  = 3
	, NSTAT_EXTENDED_UPDATE_TYPE_ORIGINAL_DOMAIN    = 4
	, NSTAT_EXTENDED_UPDATE_TYPE_FUUID              = 5
	, NSTAT_EXTENDED_UPDATE_TYPE_BLUETOOTH_COUNTS   = 6
};

#define NSTAT_EXTENDED_UPDATE_TYPE_MIN  NSTAT_EXTENDED_UPDATE_TYPE_DOMAIN
#define NSTAT_EXTENDED_UPDATE_TYPE_MAX  NSTAT_EXTENDED_UPDATE_TYPE_BLUETOOTH_COUNTS


#define NSTAT_EXTENDED_UPDATE_FLAG_MASK    0x00ffffffull    /* Maximum of 24 extension types allowed due to restrictions on specifying via filter flags */

#define NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT   40  /* With extensions expediently passed as the top 24 bits of filters supplied by client, this shift is for extraction */

/* Provider-level filters */
#define NSTAT_FILTER_ACCEPT_UNKNOWN          0x0000000000000001ull
#define NSTAT_FILTER_ACCEPT_LOOPBACK         0x0000000000000002ull
#define NSTAT_FILTER_ACCEPT_CELLULAR         0x0000000000000004ull
#define NSTAT_FILTER_ACCEPT_WIFI             0x0000000000000008ull
#define NSTAT_FILTER_ACCEPT_WIRED            0x0000000000000010ull
#define NSTAT_FILTER_ACCEPT_AWDL             0x0000000000000020ull
#define NSTAT_FILTER_ACCEPT_EXPENSIVE        0x0000000000000040ull
#define NSTAT_FILTER_ACCEPT_CELLFALLBACK     0x0000000000000100ull
#define NSTAT_FILTER_ACCEPT_COMPANIONLINK    0x0000000000000200ull
#define NSTAT_FILTER_ACCEPT_IS_CONSTRAINED   0x0000000000000400ull
#define NSTAT_FILTER_ACCEPT_IS_LOCAL         0x0000000000000800ull
#define NSTAT_FILTER_ACCEPT_IS_NON_LOCAL     0x0000000000001000ull
#define NSTAT_FILTER_ACCEPT_ROUTE_VAL_ERR    0x0000000000002000ull
#define NSTAT_FILTER_ACCEPT_FLOWSWITCH_ERR   0x0000000000004000ull
#define NSTAT_FILTER_ACCEPT_WIFI_LLW         0x0000000000008000ull
#define NSTAT_FILTER_ACCEPT_WIFI_INFRA       0x0000000000010000ull
#define NSTAT_FILTER_ACCEPT_PEERIFEGRESS_CELL 0x0000000000020000ull
#define NSTAT_FILTER_ACCEPT_COMPANIONLINK_BT 0x0000000000040000ull
#define NSTAT_FILTER_IFNET_FLAGS             0x000000000007FFFFull

#define NSTAT_FILTER_UDP_INTERFACE_ATTACH    0x0000000000020000ull  // Subject to removal, do not use
#define NSTAT_FILTER_UDP_FLAGS               0x0000000000020000ull

#define NSTAT_FILTER_TCP_INTERFACE_ATTACH    0x0000000000040000ull  // Subject to removal, do not use
#define NSTAT_FILTER_TCP_NO_EARLY_CLOSE      0x0000000000080000ull
#define NSTAT_FILTER_TCP_FLAGS               0x00000000000C0000ull

#define NSTAT_FILTER_SUPPRESS_SRC_ADDED      0x0000000000100000ull
#define NSTAT_FILTER_USE_UPDATE_FOR_ADD      0x0000000000200000ull
#define NSTAT_FILTER_PROVIDER_NOZEROBYTES    0x0000000000400000ull
#define NSTAT_FILTER_PROVIDER_NOZERODELTAS   0x0000000000800000ull

#define NSTAT_FILTER_CONN_HAS_NET_ACCESS     0x0000000001000000ull
#define NSTAT_FILTER_CONN_FLAGS              0x0000000001000000ull

#define NSTAT_FILTER_SOURCE_IS_LISTENER      0x0000000002000000ull  // NSTAT_SOURCE_IS_LISTENER
#define NSTAT_FILTER_SOURCE_IS_INBOUND       0x0000000004000000ull  // NSTAT_SOURCE_IS_INBOUND
#define NSTAT_FILTER_SOURCE_IS_OUTBOUND      0x0000000008000000ull  // NSTAT_SOURCE_IS_OUTBOUND
#define NSTAT_FILTER_SOURCE_ROLE_FLAGS       0x000000000E000000ull  // All three of the above

/* In this context, boring == no change from previous report */
#define NSTAT_FILTER_SUPPRESS_BORING_CLOSE   0x0000000010000000ull  /* No final update, only NSTAT_MSG_TYPE_SRC_REMOVED */
#define NSTAT_FILTER_SUPPRESS_BORING_POLL    0x0000000020000000ull  /* Only for poll-all, not poll specific source */
#define NSTAT_FILTER_SUPPRESS_BORING_FLAGS   (NSTAT_FILTER_SUPPRESS_BORING_CLOSE|NSTAT_FILTER_SUPPRESS_BORING_POLL)

#define NSTAT_FILTER_FLAGS_RESERVED_30       0x0000000040000000ull
#define NSTAT_FILTER_FLAGS_RESERVED_31       0x0000000080000000ull

/* The filtering for specific user is a speculative option that hasn't been exploited.  It may be removed */
#define NSTAT_FILTER_SPECIFIC_USER_BY_PID    0x0000000100000000ull
#define NSTAT_FILTER_SPECIFIC_USER_BY_EPID   0x0000000200000000ull
#define NSTAT_FILTER_SPECIFIC_USER_BY_UUID   0x0000000400000000ull
#define NSTAT_FILTER_SPECIFIC_USER_BY_EUUID  0x0000000800000000ull
#define NSTAT_FILTER_SPECIFIC_USER           0x0000000F00000000ull

#define NSTAT_FILTER_INITIAL_PROPERTIES      0x0000001000000000ull  /* For providers that give "properties" on open, apply the filter to the properties */
                                                                    /* and permanently discard unless the filter allows through */
#define NSTAT_FILTER_USE_LARGE_BUFFERS       0x0000002000000000ull  /* Not really a filter, place here until we have other mechanisms to pass this */
#define NSTAT_FILTER_DELIVER_ONCE            0x0000004000000000ull  /* Single shot delivery */
#define NSTAT_FILTER_VERSION_2_PROTOCOL      0x0000008000000000ull


#define NSTAT_FILTER_IFNET_AND_CONN_FLAGS    (NSTAT_FILTER_IFNET_FLAGS|NSTAT_FILTER_CONN_FLAGS)

#define NSTAT_EXTENSION_FILTER_DOMAIN_INFO              (1ull << (NSTAT_EXTENDED_UPDATE_TYPE_DOMAIN + NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT))
#define NSTAT_EXTENSION_FILTER_NECP_TLV                 (1ull << (NSTAT_EXTENDED_UPDATE_TYPE_NECP_TLV + NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT))
#define NSTAT_EXTENSION_FILTER_ORIGINAL_NECP_TLV        (1ull << (NSTAT_EXTENDED_UPDATE_TYPE_ORIGINAL_NECP_TLV + NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT))
#define NSTAT_EXTENSION_FILTER_ORIGINAL_DOMAIN_INFO     (1ull << (NSTAT_EXTENDED_UPDATE_TYPE_ORIGINAL_DOMAIN + NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT))
#define NSTAT_EXTENSION_FILTER_BLUETOOTH_COUNTS         (1ull << (NSTAT_EXTENDED_UPDATE_TYPE_BLUETOOTH_COUNTS + NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT))
#define NSTAT_EXTENSION_FILTER_MASK                     (NSTAT_EXTENDED_UPDATE_FLAG_MASK << NSTAT_FILTER_ALLOWED_EXTENSIONS_SHIFT)

// Version one is constrained to use only the following
#define NSTAT_FILTER_FLAGS_V1_USAGE     \
    (NSTAT_FILTER_ACCEPT_UNKNOWN|       \
    NSTAT_FILTER_ACCEPT_LOOPBACK|       \
    NSTAT_FILTER_ACCEPT_CELLULAR|       \
    NSTAT_FILTER_ACCEPT_WIFI|           \
    NSTAT_FILTER_ACCEPT_WIRED|          \
    NSTAT_FILTER_ACCEPT_AWDL|           \
    NSTAT_FILTER_ACCEPT_EXPENSIVE|      \
    NSTAT_FILTER_ACCEPT_CELLFALLBACK|   \
    NSTAT_FILTER_ACCEPT_COMPANIONLINK|  \
    NSTAT_FILTER_ACCEPT_IS_CONSTRAINED| \
    NSTAT_FILTER_ACCEPT_IS_LOCAL|       \
    NSTAT_FILTER_ACCEPT_IS_NON_LOCAL)


// A note on the header flags
//
// NSTAT_MSG_HDR_FLAG_SUPPORTS_AGGREGATE was used to indicate that user level code could cope with
// multiple counts or descriptor messages within a single overall message on the control socket.
// This ability is now mandatory for user level clients.  They may or may not choose to still set
// NSTAT_MSG_HDR_FLAG_SUPPORTS_AGGREGATE, but they must support aggregate responses
//
// For messages from the client, NSTAT_MSG_HDR_FLAG_CONTINUATION was used to indicate that the results
// of any NSTAT_SRC_REF_ALL poll could be returned in fragments, each fragment except the last one
// having NSTAT_MSG_HDR_FLAG_CONTINUATION set and each intermediate fragment intended to elicit
// a further NSTAT_SRC_REF_ALL poll with the same context as the initial one.  This pacing is intended
// to prevent overload on what amounts to a producer/consumer interface.  This style is now mandatory,
// whether or not NSTAT_MSG_HDR_FLAG_CONTINUATION is set, any polls may result in data being returned
// in fragments which each contain just a portion of the requested counts/descriptors/updates.
// The user level clients may choose whether or not to set NSTAT_MSG_HDR_FLAG_CONTINUATION in any polls
// but they must support getting poll responses in multiple chunks
//
enum{
	NSTAT_MSG_HDR_FLAG_SUPPORTS_AGGREGATE   = 1 << 0,
	NSTAT_MSG_HDR_FLAG_CONTINUATION         = 1 << 1,
	NSTAT_MSG_HDR_FLAG_CLOSING              = 1 << 2,
	NSTAT_MSG_HDR_FLAG_CLOSED_AFTER_DROP    = 1 << 3,
	NSTAT_MSG_HDR_FLAG_CLOSED_AFTER_FILTER  = 1 << 4,
	NSTAT_MSG_HDR_FLAG_CLOSED_AFTER_GONE    = 1 << 6,
};

#define DEFINE_NTSTAT_DATA_ACCESSOR(NTSTAT_TYPE)                            \
static inline                                                               \
__attribute__((always_inline))                                              \
__attribute__((overloadable))                                               \
uint8_t * __header_indexable                                                \
nstat_get_data(NTSTAT_TYPE *__header_indexable desc)                        \
{                                                                           \
    if (desc) {                                                             \
	_Pragma("clang diagnostic push");                                       \
	_Pragma("clang diagnostic ignored \"-Wunsafe-buffer-usage\"");          \
	return (uint8_t *)desc + sizeof(NTSTAT_TYPE);                           \
	_Pragma("clang diagnostic pop");                                        \
    } else {                                                                \
	return NULL;                                                            \
    }                                                                       \
}

typedef struct nstat_msg_hdr {
	u_int64_t       context __attribute__((aligned(sizeof(u_int64_t))));
	u_int32_t       type;
	u_int16_t       length;
	u_int16_t       flags;
} nstat_msg_hdr;

#define MAX_NSTAT_MSG_HDR_LENGTH    65532

typedef struct nstat_msg_error {
	nstat_msg_hdr   hdr;
	u_int32_t               error;  // errno error
	u_int8_t                reserved[4];
} nstat_msg_error;

#define NSTAT_ADD_SRC_FIELDS            \
	nstat_msg_hdr		hdr;            \
	nstat_provider_id_t	provider;       \
	u_int8_t			reserved[4]     \

typedef struct nstat_msg_add_src {
	NSTAT_ADD_SRC_FIELDS;
	u_int8_t        param[];
} nstat_msg_add_src_req;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_add_src)

typedef struct nstat_msg_add_src_header {
	NSTAT_ADD_SRC_FIELDS;
} nstat_msg_add_src_header;

typedef struct nstat_msg_add_src_convenient {
	nstat_msg_add_src_header        hdr;
	union {
		nstat_route_add_param   route;
		nstat_tcp_add_param     tcp;
		nstat_udp_add_param     udp;
		nstat_ifnet_add_param   ifnet;
		nstat_sysinfo_add_param sysinfo;
	};
} nstat_msg_add_src_convenient;

#undef NSTAT_ADD_SRC_FIELDS

typedef struct nstat_msg_add_all_srcs {
	nstat_msg_hdr           hdr;
	u_int64_t               filter __attribute__((aligned(sizeof(u_int64_t))));
	nstat_event_flags_t     events __attribute__((aligned(sizeof(u_int64_t))));
	nstat_provider_id_t     provider;
	pid_t                   target_pid;
	uuid_t                  target_uuid;
} nstat_msg_add_all_srcs;

typedef struct nstat_msg_src_added {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
	nstat_provider_id_t     provider;
	u_int8_t                reserved[4];
} nstat_msg_src_added;

typedef struct nstat_msg_rem_src {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
} nstat_msg_rem_src_req;

typedef struct nstat_msg_get_src_description {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
} nstat_msg_get_src_description;

typedef struct nstat_msg_set_filter {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
	u_int32_t               filter;
	u_int8_t                reserved[4];
} nstat_msg_set_filter;

#define NSTAT_SRC_DESCRIPTION_FIELDS                                                \
	nstat_msg_hdr		hdr;                                                        \
	nstat_src_ref_t		srcref __attribute__((aligned(sizeof(u_int64_t))));         \
	nstat_event_flags_t	event_flags __attribute__((aligned(sizeof(u_int64_t))));    \
	nstat_provider_id_t	provider;                                                   \
	u_int8_t			reserved[4]

typedef struct nstat_msg_src_description {
	NSTAT_SRC_DESCRIPTION_FIELDS;
	u_int8_t        data[];
} nstat_msg_src_description;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_description)

typedef struct nstat_msg_src_description_header {
	NSTAT_SRC_DESCRIPTION_FIELDS;
} nstat_msg_src_description_header;

typedef struct nstat_msg_src_description_convenient {
	nstat_msg_src_description_header    hdr;
	union {
		nstat_tcp_descriptor            tcp;
		nstat_udp_descriptor            udp;
		nstat_route_descriptor          route;
		nstat_ifnet_descriptor          ifnet;
		nstat_sysinfo_descriptor        sysinfo;
		nstat_quic_descriptor           quic;
	};
} nstat_msg_src_description_convenient;

#undef NSTAT_SRC_DESCRIPTION_FIELDS

typedef struct nstat_msg_query_src {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
} nstat_msg_query_src_req;

typedef struct nstat_msg_src_counts {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
	nstat_event_flags_t     event_flags __attribute__((aligned(sizeof(u_int64_t))));
	nstat_counts            counts;
} nstat_msg_src_counts;

#define NSTAT_SRC_UPDATE_FIELDS                                                     \
	nstat_msg_hdr		hdr;                                                        \
	nstat_src_ref_t		srcref __attribute__((aligned(sizeof(u_int64_t))));         \
	nstat_event_flags_t	event_flags __attribute__((aligned(sizeof(u_int64_t))));    \
	nstat_counts		counts;                                                     \
	nstat_provider_id_t	provider;                                                   \
	u_int8_t			reserved[4]

typedef struct nstat_msg_src_update {
	NSTAT_SRC_UPDATE_FIELDS;
	u_int8_t        data[];
} nstat_msg_src_update;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_update)

typedef struct nstat_msg_src_update_hdr {
	NSTAT_SRC_UPDATE_FIELDS;
} nstat_msg_src_update_hdr;

typedef struct nstat_msg_src_update_tcp {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_tcp_descriptor            tcp_desc;
} nstat_msg_src_update_tcp;

typedef struct nstat_msg_src_update_udp {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_udp_descriptor            udp_desc;
} nstat_msg_src_update_udp;

typedef struct nstat_msg_src_update_quic {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_quic_descriptor           quic_desc;
} nstat_msg_src_update_quic;

typedef struct nstat_msg_src_update_conn {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_connection_descriptor     conn_desc;
} nstat_msg_src_update_conn;


typedef struct nstat_msg_src_update_convenient {
	nstat_msg_src_update_hdr                hdr;
	union {
		nstat_tcp_descriptor            tcp;
		nstat_udp_descriptor            udp;
		nstat_route_descriptor          route;
		nstat_ifnet_descriptor          ifnet;
		nstat_sysinfo_descriptor        sysinfo;
		nstat_quic_descriptor           quic;
		nstat_connection_descriptor     conn;
	};
} nstat_msg_src_update_convenient;

typedef struct nstat_msg_src_extended_item_hdr {
	u_int32_t       type;
	u_int32_t       length;
} nstat_msg_src_extended_item_hdr __attribute__((aligned(sizeof(u_int64_t))));;

typedef struct nstat_msg_src_extended_item {
	nstat_msg_src_extended_item_hdr         hdr;
	u_int8_t                                data[];
} nstat_msg_src_extended_item;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_extended_item)

typedef struct nstat_msg_src_extended_tcp_update {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_tcp_descriptor                    tcp;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	u_int8_t                                data[];
} nstat_msg_src_extended_tcp_update;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_extended_tcp_update)

typedef struct nstat_msg_src_extended_udp_update {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_udp_descriptor                    udp;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	u_int8_t                                data[];
} nstat_msg_src_extended_udp_update;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_extended_udp_update)

typedef struct nstat_msg_src_extended_quic_update {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_quic_descriptor                   quic;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	u_int8_t                                data[];
} nstat_msg_src_extended_quic_update;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_extended_quic_update)

typedef struct nstat_msg_src_extended_conn_update {
	NSTAT_SRC_UPDATE_FIELDS;
	nstat_connection_descriptor             conn;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	u_int8_t                                data[];
} nstat_msg_src_extended_conn_update;
DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_src_extended_conn_update)

/* While the only type of extended update is for domain information, we can fully define the structure */
typedef struct nstat_msg_src_tcp_update_domain_extension {
	nstat_msg_src_update_hdr                hdr;
	nstat_tcp_descriptor                    tcp;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	nstat_domain_info                       domain_info;
} nstat_msg_src_tcp_update_domain_extension;

typedef struct nstat_msg_src_udp_update_domain_extension {
	nstat_msg_src_update_hdr                hdr;
	nstat_udp_descriptor                    udp;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	nstat_domain_info                       domain_info;
} nstat_msg_src_udp_update_domain_extension;

typedef struct nstat_msg_src_quic_update_domain_extension {
	nstat_msg_src_update_hdr                hdr;
	nstat_quic_descriptor                   quic;
	nstat_msg_src_extended_item_hdr         extension_hdr;
	nstat_domain_info                       domain_info;
} nstat_msg_src_quic_update_domain_extension;

typedef struct nstat_msg_src_update_domain_extension_convenient {
	nstat_msg_src_tcp_update_domain_extension       tcp;
	nstat_msg_src_udp_update_domain_extension       udp;
	nstat_msg_src_quic_update_domain_extension      quic;
} nstat_msg_src_update_domain_extension_convenient;

#undef NSTAT_SRC_UPDATE_FIELDS

typedef struct nstat_msg_src_removed {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
} nstat_msg_src_removed;

typedef struct nstat_msg_sysinfo_counts {
	nstat_msg_hdr           hdr;
	nstat_src_ref_t         srcref __attribute__((aligned(sizeof(u_int64_t))));
	nstat_sysinfo_counts    counts;
}  nstat_msg_sysinfo_counts;

DEFINE_NTSTAT_DATA_ACCESSOR(struct nstat_msg_sysinfo_counts)

static inline
__attribute__((always_inline))
struct nstat_sysinfo_keyval * __header_indexable
nstat_sysinfo_get_keyvals(struct nstat_msg_sysinfo_counts *__header_indexable counts)
{
	return (struct nstat_sysinfo_keyval *)(void *)nstat_get_data(counts);
}

#pragma mark -- Statitiscs about Network Statistics --

struct nstat_stats {
	u_int32_t nstat_successmsgfailures;
	u_int32_t nstat_sendcountfailures;
	u_int32_t nstat_sysinfofailures;
	u_int32_t nstat_srcupatefailures;
	u_int32_t nstat_descriptionfailures;
	u_int32_t nstat_msgremovedfailures;
	u_int32_t nstat_srcaddedfailures;
	u_int32_t nstat_msgerrorfailures;
	u_int32_t nstat_copy_descriptor_failures;
	u_int32_t nstat_provider_counts_failures;
	u_int32_t nstat_control_send_description_failures;
	u_int32_t nstat_control_send_goodbye_failures;
	u_int32_t nstat_flush_accumulated_msgs_failures;
	u_int32_t nstat_accumulate_msg_failures;
	u_int32_t nstat_control_cleanup_source_failures;
	u_int32_t nstat_handle_msg_failures;
};

/*
 * Structure with information that gives insight into forward progress on an
 * interface, exported to user-land via sysctl(3).
 */
struct nstat_progress_indicators {
	u_int32_t       np_numflows;            /* Total number of flows */
	u_int32_t       np_conn_probe_fails;    /* Count of connection failures */
	u_int32_t       np_read_probe_fails;    /* Count of read probe failures */
	u_int32_t       np_write_probe_fails;   /* Count of write failures */
	u_int32_t       np_recentflows;         /* Total of "recent" flows */
	u_int32_t       np_recentflows_unacked; /* Total of "recent" flows with unacknowledged data */
	u_int64_t       np_recentflows_rxbytes; /* Total of "recent" flows received bytes */
	u_int64_t       np_recentflows_txbytes; /* Total of "recent" flows transmitted bytes */
	u_int64_t       np_recentflows_rxooo;   /* Total of "recent" flows received out of order bytes */
	u_int64_t       np_recentflows_rxdup;   /* Total of "recent" flows received duplicate bytes */
	u_int64_t       np_recentflows_retx;    /* Total of "recent" flows retransmitted bytes */
	u_int64_t       np_reserved1;           /* Expansion */
	u_int64_t       np_reserved2;           /* Expansion */
	u_int64_t       np_reserved3;           /* Expansion */
	u_int64_t       np_reserved4;           /* Expansion */
};

struct nstat_progress_req {
	u_int64_t       np_ifindex;                 /* Interface index for progress indicators */
	u_int64_t       np_recentflow_maxduration;  /* In mach_absolute_time, max duration for flow to be counted as "recent" */
	u_int64_t       np_filter_flags;            /* Optional additional filtering, values are interface properties per ntstat.h */
	u_int64_t       np_transport_protocol_mask; /* Transport protocol (currently supports TCP and QUIC) */
#define PR_PROTO_TCP         0x1
#define PR_PROTO_QUIC        0x2
};

#endif /* PRIVATE */

#ifdef XNU_KERNEL_PRIVATE
#include <sys/mcache.h>

#if (DEBUG || DEVELOPMENT)
extern int nstat_test_privacy_transparency;
#endif /* (DEBUG || DEVELOPMENT) */

#pragma mark -- System Information Internal Support --

typedef struct nstat_sysinfo_tcp_stats {
	/* When adding/removing here, also adjust NSTAT_SYSINFO_TCP_STATS_COUNT */
	u_int32_t       ipv4_avgrtt;    /* Average RTT for IPv4 */
	u_int32_t       ipv6_avgrtt;    /* Average RTT for IPv6 */
	u_int32_t       send_plr;       /* Average uplink packet loss rate */
	u_int32_t       recv_plr;       /* Average downlink packet loss rate */
	u_int32_t       send_tlrto_rate; /* Average rxt timeout after tail loss */
	u_int32_t       send_reorder_rate; /* Average packet reordering rate */
	u_int32_t       connection_attempts; /* TCP client connection attempts */
	u_int32_t       connection_accepts; /* TCP server connection accepts */
	u_int32_t       ecn_client_enabled; /* Global setting for ECN client side */
	u_int32_t       ecn_server_enabled; /* Global setting for ECN server side */
	u_int32_t       ecn_client_setup; /* Attempts to setup TCP client connection with ECN */
	u_int32_t       ecn_server_setup; /* Attempts to setup TCP server connection with ECN */
	u_int32_t       ecn_client_success; /* Number of successful negotiations of ECN for a client connection */
	u_int32_t       ecn_server_success; /* Number of successful negotiations of ECN for a server connection */
	u_int32_t       ecn_not_supported; /* Number of falbacks to Non-ECN, no support from peer */
	u_int32_t       ecn_lost_syn;   /* Number of SYNs lost with ECN bits */
	u_int32_t       ecn_lost_synack; /* Number of SYN-ACKs lost with ECN bits */
	u_int32_t       ecn_recv_ce;    /* Number of CEs received from network */
	u_int32_t       ecn_recv_ece;   /* Number of ECEs received from receiver */
	u_int32_t       ecn_sent_ece;   /* Number of ECEs sent in response to CE */
	u_int32_t       ecn_conn_recv_ce; /* Number of connections using ECN received CE at least once */
	u_int32_t       ecn_conn_recv_ece; /* Number of connections using ECN received ECE at least once */
	u_int32_t       ecn_conn_plnoce; /* Number of connections using ECN seen packet loss but never received CE */
	u_int32_t       ecn_conn_pl_ce; /* Number of connections using ECN seen packet loss and CE */
	u_int32_t       ecn_conn_nopl_ce; /* Number of connections using ECN with no packet loss but received CE */
	u_int32_t       ecn_fallback_synloss; /* Number of times we did fall back due to SYN-Loss */
	u_int32_t       ecn_fallback_reorder; /* Number of times we fallback because we detected the PAWS-issue */
	u_int32_t       ecn_fallback_ce; /* Number of times we fallback because we received too many CEs */
	u_int32_t       tfo_syn_data_rcv;       /* Number of SYN+data received with valid cookie */
	u_int32_t       tfo_cookie_req_rcv;/* Number of TFO cookie-requests received */
	u_int32_t       tfo_cookie_sent;        /* Number of TFO-cookies offered to the client */
	u_int32_t       tfo_cookie_invalid;/* Number of invalid TFO-cookies received */
	u_int32_t       tfo_cookie_req; /* Number of SYNs with cookie request received*/
	u_int32_t       tfo_cookie_rcv; /* Number of SYN/ACKs with Cookie received */
	u_int32_t       tfo_syn_data_sent;      /* Number of SYNs+data+cookie sent */
	u_int32_t       tfo_syn_data_acked;/* Number of times our SYN+data has been acknowledged */
	u_int32_t       tfo_syn_loss;   /* Number of times SYN+TFO has been lost and we fallback */
	u_int32_t       tfo_blackhole;  /* Number of times SYN+TFO has been lost and we fallback */
	u_int32_t       tfo_cookie_wrong;       /* TFO-cookie we sent was wrong */
	u_int32_t       tfo_no_cookie_rcv;      /* We asked for a cookie but didn't get one */
	u_int32_t       tfo_heuristics_disable; /* TFO got disabled due to heuristics */
	u_int32_t       tfo_sndblackhole;       /* TFO got blackholed in the sending direction */
	u_int32_t       mptcp_handover_attempt; /* Total number of MPTCP-attempts using handover mode */
	u_int32_t       mptcp_interactive_attempt;      /* Total number of MPTCP-attempts using interactive mode */
	u_int32_t       mptcp_aggregate_attempt;        /* Total number of MPTCP-attempts using aggregate mode */
	u_int32_t       mptcp_fp_handover_attempt; /* Same as previous three but only for first-party apps */
	u_int32_t       mptcp_fp_interactive_attempt;
	u_int32_t       mptcp_fp_aggregate_attempt;
	u_int32_t       mptcp_heuristic_fallback;       /* Total number of MPTCP-connections that fell back due to heuristics */
	u_int32_t       mptcp_fp_heuristic_fallback;    /* Same as previous but for first-party apps */
	u_int32_t       mptcp_handover_success_wifi;    /* Total number of successfull handover-mode connections that *started* on WiFi */
	u_int32_t       mptcp_handover_success_cell;    /* Total number of successfull handover-mode connections that *started* on Cell */
	u_int32_t       mptcp_interactive_success;              /* Total number of interactive-mode connections that negotiated MPTCP */
	u_int32_t       mptcp_aggregate_success;                /* Same as previous but for aggregate */
	u_int32_t       mptcp_fp_handover_success_wifi; /* Same as previous four, but for first-party apps */
	u_int32_t       mptcp_fp_handover_success_cell;
	u_int32_t       mptcp_fp_interactive_success;
	u_int32_t       mptcp_fp_aggregate_success;
	u_int32_t       mptcp_handover_cell_from_wifi;  /* Total number of connections that use cell in handover-mode (coming from WiFi) */
	u_int32_t       mptcp_handover_wifi_from_cell;  /* Total number of connections that use WiFi in handover-mode (coming from cell) */
	u_int32_t       mptcp_interactive_cell_from_wifi;       /* Total number of connections that use cell in interactive mode (coming from WiFi) */
	u_int32_t       mptcp_back_to_wifi;     /* Total number of connections that succeed to move traffic away from cell (when starting on cell) */
	u_int64_t       mptcp_handover_cell_bytes;              /* Total number of bytes sent on cell in handover-mode (on new subflows, ignoring initial one) */
	u_int64_t       mptcp_interactive_cell_bytes;   /* Same as previous but for interactive */
	u_int64_t       mptcp_aggregate_cell_bytes;
	u_int64_t       mptcp_handover_all_bytes;               /* Total number of bytes sent in handover */
	u_int64_t       mptcp_interactive_all_bytes;
	u_int64_t       mptcp_aggregate_all_bytes;
	u_int32_t       mptcp_wifi_proxy;               /* Total number of new subflows that fell back to regular TCP on cell */
	u_int32_t       mptcp_cell_proxy;               /* Total number of new subflows that fell back to regular TCP on WiFi */
	u_int32_t       mptcp_triggered_cell;           /* Total number of times an MPTCP-connection triggered cell bringup */
	u_int32_t       _padding;
	/* When adding/removing here, also adjust NSTAT_SYSINFO_TCP_STATS_COUNT */
} nstat_sysinfo_tcp_stats;
#define NSTAT_SYSINFO_TCP_STATS_COUNT   71

enum {
	NSTAT_IFNET_ECN_PROTO_IPV4 = 1
	, NSTAT_IFNET_ECN_PROTO_IPV6
};

enum {
	NSTAT_IFNET_ECN_TYPE_CELLULAR = 1
	, NSTAT_IFNET_ECN_TYPE_WIFI
	, NSTAT_IFNET_ECN_TYPE_ETHERNET
};

typedef struct nstat_sysinfo_ifnet_ecn_stats {
	u_int32_t               ifnet_proto;
	u_int32_t               ifnet_type;
	struct if_tcp_ecn_stat  ecn_stat;
} nstat_sysinfo_ifnet_ecn_stats;

/* Total number of Low Internet stats that will be reported */
#define NSTAT_LIM_STAT_KEYVAL_COUNT     12
typedef struct nstat_sysinfo_lim_stats {
	u_int8_t                ifnet_signature[NSTAT_SYSINFO_KEYVAL_STRING_MAXSIZE];
	u_int32_t               ifnet_siglen;
	u_int32_t               ifnet_type;
	struct if_lim_perf_stat lim_stat;
} nstat_sysinfo_lim_stats;

#define NSTAT_NET_API_STAT_KEYVAL_COUNT (NSTAT_SYSINFO_API_LAST - NSTAT_SYSINFO_API_FIRST + 1)
typedef struct nstat_sysinfo_net_api_stats {
	u_int32_t               report_interval;
	u_int32_t               _padding;
	struct net_api_stats    net_api_stats;
} nstat_sysinfo_net_api_stats;

typedef struct nstat_sysinfo_data {
	uint32_t                flags;
	uint32_t                unsent_data_cnt; /* Before sleeping */
	union {
		nstat_sysinfo_tcp_stats tcp_stats;
		nstat_sysinfo_ifnet_ecn_stats ifnet_ecn_stats;
		nstat_sysinfo_lim_stats lim_stats;
		nstat_sysinfo_net_api_stats net_api_stats;
	} u;
} nstat_sysinfo_data;

#pragma mark -- Route Statistics Gathering Functions --
struct rtentry;

enum{
	NSTAT_TX_FLAG_RETRANSMIT        = 1
};

enum{
	NSTAT_RX_FLAG_DUPLICATE         = 1,
	NSTAT_RX_FLAG_OUT_OF_ORDER      = 2
};

// indicates whether or not collection of statistics is enabled
extern int      nstat_collect;

void nstat_init(void);

// Route collection routines
void nstat_route_connect_attempt(struct rtentry *rte);
void nstat_route_connect_success(struct rtentry *rte);
void nstat_route_tx(struct rtentry *rte, u_int32_t packets, u_int32_t bytes, u_int32_t flags);
void nstat_route_rx(struct rtentry *rte, u_int32_t packets, u_int32_t bytes, u_int32_t flags);
void nstat_route_rtt(struct rtentry *rte, u_int32_t rtt, u_int32_t rtt_var);
void nstat_route_update(struct rtentry *rte, uint32_t connect_attempts, uint32_t connect_successes,
    uint32_t rx_packets, uint32_t rx_bytes, uint32_t rx_duplicatebytes, uint32_t rx_outoforderbytes,
    uint32_t tx_packets, uint32_t tx_bytes, uint32_t tx_retransmit,
    uint32_t rtt, uint32_t rtt_var);
struct nstat_counts* nstat_route_attach(struct rtentry  *rte);
void nstat_route_detach(struct rtentry *rte);

// watcher support
struct inpcb;
void nstat_tcp_new_pcb(struct inpcb *inp);
void nstat_udp_new_pcb(struct inpcb *inp);
void nstat_route_new_entry(struct rtentry *rt);
void nstat_pcb_detach(struct inpcb *inp);
void nstat_pcb_event(struct inpcb *inp, u_int64_t event);
void nstat_pcb_cache(struct inpcb *inp);
void nstat_pcb_invalidate_cache(struct inpcb *inp);


void nstat_ifnet_threshold_reached(unsigned int ifindex);

void nstat_sysinfo_send_data(struct nstat_sysinfo_data *);

int ntstat_tcp_progress_enable(struct sysctl_req *req);

#if SKYWALK

// Userland stats reporting

// Each side, NetworkStatistics and the kernel provider for userland,
// pass opaque references.
typedef void *userland_stats_provider_context;
typedef void *nstat_userland_context;

typedef struct nstat_progress_digest {
	u_int64_t       rxbytes;
	u_int64_t       txbytes;
	u_int32_t       rxduplicatebytes;
	u_int32_t       rxoutoforderbytes;
	u_int32_t       txretransmit;
	u_int32_t       ifindex;
	u_int32_t       state;
	u_int32_t       txunacked;
	u_int32_t       txwindow;
	union {
		struct tcp_conn_status connstatus;
		// On armv7k, tcp_conn_status is 1 byte instead of 4
		uint8_t                                 __pad_connstatus[4];
	};
} nstat_progress_digest;

// When things have been set up, Netstats can request a refresh of its data.
typedef bool (userland_stats_request_vals_fn)(userland_stats_provider_context *ctx,
    u_int32_t *ifflagsp,
    nstat_progress_digest *digestp,
    nstat_counts *countsp,
    void *metadatap);

// Netstats can also request "extension" items, specified by the allowed_extensions flag
// The return value is the amount of space currently required for the extension
typedef size_t (userland_stats_request_extension_fn)(userland_stats_provider_context *ctx,
    int requested_extension,        /* The extension to be returned */
    void *__sized_by(buf_size)buf,  /* If not NULL, the address for the extension to be returned in */
    size_t buf_size);               /* The size of the buffer space, typically matching the return from a previous call with null buffer pointer */

// Things get started with a call to netstats to say that there’s a new connection:
nstat_userland_context ntstat_userland_stats_open(userland_stats_provider_context *ctx,
    int provider_id,
    u_int64_t properties,
    userland_stats_request_vals_fn req_fn,
    userland_stats_request_extension_fn req_extension_fn);

void ntstat_userland_stats_close(nstat_userland_context nstat_ctx);


void ntstat_userland_stats_event(nstat_userland_context nstat_ctx, uint64_t event);

void nstats_userland_stats_defunct_for_process(int pid);

errno_t nstat_userland_mark_rnf_override(uuid_t fuuid, bool rnf_override);

typedef struct nstat_flow_data {
	nstat_counts        counts;
	union {
		nstat_udp_descriptor    udp_descriptor;
		nstat_tcp_descriptor    tcp_descriptor;
	} flow_descriptor;
} nstat_flow_data;

// Servicing a sysctl for information of TCP or UDP flows
int ntstat_userland_count(short proto);
int nstat_userland_get_snapshot(short proto, void *__sized_by(*snapshot_size) * snapshotp, size_t *snapshot_size, int *countp);
int nstat_userland_list_snapshot(short proto, struct sysctl_req *req, void *__sized_by(nuserland * sizeof(nstat_flow_data)) userlandsnapshot, int nuserland);
void nstat_userland_release_snapshot(void *snapshot, int nuserland);

#if NTSTAT_SUPPORTS_STANDALONE_SYSCTL
int ntstat_userland_list_n(short proto, struct sysctl_req *req);
#endif
#endif /* SKYWALK */

// Utilities for userland stats reporting

u_int32_t nstat_ifnet_to_flags(struct ifnet *ifp);

// Generic external provider reporting

// Each side passes opaque references.
typedef void *nstat_provider_context;   /* This is quoted to the external provider */
typedef void *nstat_context;            /* This is quoted by the external provider when calling nstat */

// After nstat_provider_stats_open() has been called (and potentially while the open is still executing), netstats can request a refresh of its data
// The various return pointer parameters may be null if the item is not required
// The return code is true for success
typedef bool (nstat_provider_request_vals_fn)(nstat_provider_context ctx,
    u_int32_t *ifflagsp,    /* Flags for being on cell/wifi etc, used for filtering */
    nstat_counts *countsp,  /* Counts to be filled in */
    void *metadatap);       /* A descriptor for the particular provider */

// Netstats can also request "extension" items, specified by the allowed_extensions flag
// The return value is the amount of space currently required for the extension
typedef size_t (nstat_provider_request_extensions_fn)(nstat_provider_context ctx,
    int requested_extension,    /* The extension to be returned */
    void *__sized_by (buf_size)buf,                  /* If not NULL, the address for the extension to be returned in */
    size_t buf_size);           /* The size of the buffer space, typically matching the return from a previous call with null buffer pointer */

// Things get started with a call to netstats to say that there’s a new item to become a netstats source
nstat_context nstat_provider_stats_open(nstat_provider_context ctx,
    int provider_id,
    u_int64_t properties,                   /* The bottom 32 bits can be used as per the interface / connection flags ifflagsp */
    nstat_provider_request_vals_fn req_fn,
    nstat_provider_request_extensions_fn req_extensions_fn);

// Note that when the source is closed, netstats will make one last call on the request functions to retrieve final values
void nstat_provider_stats_close(nstat_context nstat_ctx);

// Events that cause a significant change may be reported via a flags word
void nstat_provider_stats_event(nstat_context nstat_ctx, uint64_t event);

// locked_add_64 uses atomic operations on 32bit so the 64bit
// value can be properly read. The values are only ever incremented
// while under the socket lock, so on 64bit we don't actually need
// atomic operations to increment.
#if defined(__LP64__)
#define locked_add_64(__addr, __count) do { \
	*(__addr) += (__count); \
} while (0)
#else
#define locked_add_64(__addr, __count) do { \
	os_atomic_add((__addr), (__count), relaxed); \
} while (0)
#endif

#endif /* XNU_KERNEL_PRIVATE */

#endif /* __NTSTAT_H__ */
