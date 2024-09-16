/*
 * Copyright (c) 2016-2023 Apple Inc. All rights reserved.
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

#include <skywalk/os_skywalk_private.h>
#include <skywalk/packet/pbufpool_var.h>
#include <sys/sdt.h>
#include <net/droptap.h>

static struct kern_pbufpool *pp_alloc(zalloc_flags_t);
static void pp_free(struct kern_pbufpool *);
static uint32_t pp_alloc_packet_common(struct kern_pbufpool *, uint16_t,
    uint64_t *__counted_by(num), uint32_t num, boolean_t, alloc_cb_func_t,
    const void *, uint32_t);
static void pp_free_packet_array(struct kern_pbufpool *,
    uint64_t *__counted_by(num)array, uint32_t num);
static int pp_metadata_ctor_no_buflet(struct skmem_obj_info *,
    struct skmem_obj_info *, void *, uint32_t);
static int pp_metadata_ctor_max_buflet(struct skmem_obj_info *,
    struct skmem_obj_info *, void *, uint32_t);
static void pp_metadata_dtor(void *, void *);
static int pp_metadata_construct(struct __kern_quantum *,
    struct __user_quantum *, obj_idx_t, struct kern_pbufpool *, uint32_t,
    uint16_t, bool, struct skmem_obj **);
static void pp_metadata_destruct(struct __kern_quantum *,
    struct kern_pbufpool *, bool);
static struct __kern_quantum *pp_metadata_init(struct __metadata_preamble *,
    struct kern_pbufpool *, uint16_t, uint32_t, struct skmem_obj **);
static struct __metadata_preamble *pp_metadata_fini(struct __kern_quantum *,
    struct kern_pbufpool *, struct mbuf **, struct __kern_packet **,
    struct skmem_obj **, struct skmem_obj **, struct skmem_obj **, struct skmem_obj **);
static void pp_purge_upp_locked(struct kern_pbufpool *pp, pid_t pid);
static void pp_buf_seg_ctor(struct sksegment *, IOSKMemoryBufferRef, void *);
static void pp_buf_seg_dtor(struct sksegment *, IOSKMemoryBufferRef, void *);
static void pp_destroy_upp_locked(struct kern_pbufpool *);
static void pp_destroy_upp_bft_locked(struct kern_pbufpool *);
static int pp_init_upp_bft_locked(struct kern_pbufpool *, boolean_t);
static void pp_free_buflet_common(const kern_pbufpool_t, kern_buflet_t);
static mach_vm_address_t pp_alloc_buffer_common(const kern_pbufpool_t pp,
    struct skmem_obj_info *oi, uint32_t skmflag, bool large);
static inline uint32_t
pp_alloc_buflet_common(struct kern_pbufpool *pp,
    uint64_t *__counted_by(num)array, uint32_t num, uint32_t skmflag,
    bool large);

#define KERN_PBUFPOOL_U_HASH_SIZE       64      /* hash table size */
#define KERN_BUF_MIN_STRIDING_SIZE      256 * 1024

/*
 * Since the inputs are small (indices to the metadata region), we can use
 * Knuth's multiplicative hash method which is fast and good enough.  Here
 * we multiply the input by the golden ratio of 2^32.  See "The Art of
 * Computer Programming", section 6.4.
 */
#define KERN_PBUFPOOL_U_HASH_INDEX(_i, _m)                      \
	(((_i) * 2654435761U) & (_m))
#define KERN_PBUFPOOL_U_HASH(_pp, _i)                           \
	(&(_pp)->pp_u_hash_table[KERN_PBUFPOOL_U_HASH_INDEX(_i, \
	KERN_PBUFPOOL_U_HASH_SIZE - 1)])
#define KERN_PBUFPOOL_U_BFT_HASH(_pp, _i)                           \
	(&(_pp)->pp_u_bft_hash_table[KERN_PBUFPOOL_U_HASH_INDEX(_i, \
	KERN_PBUFPOOL_U_HASH_SIZE - 1)])

static SKMEM_TYPE_DEFINE(pp_zone, struct kern_pbufpool);

#define SKMEM_TAG_PBUFPOOL_HASH  "com.apple.skywalk.pbufpool.hash"
static SKMEM_TAG_DEFINE(skmem_tag_pbufpool_hash, SKMEM_TAG_PBUFPOOL_HASH);

#define SKMEM_TAG_PBUFPOOL_BFT_HASH  "com.apple.skywalk.pbufpool.bft.hash"
static SKMEM_TAG_DEFINE(skmem_tag_pbufpool_bft_hash, SKMEM_TAG_PBUFPOOL_BFT_HASH);

struct kern_pbufpool_u_htbl {
	struct kern_pbufpool_u_bkt upp_hash[KERN_PBUFPOOL_U_HASH_SIZE];
};

#define PP_U_HTBL_SIZE  sizeof(struct kern_pbufpool_u_htbl)
static SKMEM_TYPE_DEFINE(pp_u_htbl_zone, struct kern_pbufpool_u_htbl);

static struct skmem_cache *pp_opt_cache;        /* cache for __packet_opt */
static struct skmem_cache *pp_flow_cache;       /* cache for __flow */
static struct skmem_cache *pp_compl_cache;      /* cache for __packet_compl */

static int __pp_inited = 0;

int
pp_init(void)
{
	_CASSERT(KPKT_SC_UNSPEC == MBUF_SC_UNSPEC);
	_CASSERT(KPKT_SC_BK_SYS == MBUF_SC_BK_SYS);
	_CASSERT(KPKT_SC_BK == MBUF_SC_BK);
	_CASSERT(KPKT_SC_BE == MBUF_SC_BE);
	_CASSERT(KPKT_SC_RD == MBUF_SC_RD);
	_CASSERT(KPKT_SC_OAM == MBUF_SC_OAM);
	_CASSERT(KPKT_SC_AV == MBUF_SC_AV);
	_CASSERT(KPKT_SC_RV == MBUF_SC_RV);
	_CASSERT(KPKT_SC_VI == MBUF_SC_VI);
	_CASSERT(KPKT_SC_SIG == MBUF_SC_SIG);
	_CASSERT(KPKT_SC_VO == MBUF_SC_VO);
	_CASSERT(KPKT_SC_CTL == MBUF_SC_CTL);

	_CASSERT(KPKT_SC_BK_SYS == PKT_SC_BK_SYS);
	_CASSERT(KPKT_SC_BK == PKT_SC_BK);
	_CASSERT(KPKT_SC_BE == PKT_SC_BE);
	_CASSERT(KPKT_SC_RD == PKT_SC_RD);
	_CASSERT(KPKT_SC_OAM == PKT_SC_OAM);
	_CASSERT(KPKT_SC_AV == PKT_SC_AV);
	_CASSERT(KPKT_SC_RV == PKT_SC_RV);
	_CASSERT(KPKT_SC_VI == PKT_SC_VI);
	_CASSERT(KPKT_SC_SIG == PKT_SC_SIG);
	_CASSERT(KPKT_SC_VO == PKT_SC_VO);
	_CASSERT(KPKT_SC_CTL == PKT_SC_CTL);
	_CASSERT(KPKT_SC_MAX_CLASSES == MBUF_SC_MAX_CLASSES);

	_CASSERT(KPKT_TC_UNSPEC == MBUF_TC_UNSPEC);
	_CASSERT(KPKT_TC_BE == MBUF_TC_BE);
	_CASSERT(KPKT_TC_BK == MBUF_TC_BK);
	_CASSERT(KPKT_TC_VI == MBUF_TC_VI);
	_CASSERT(KPKT_TC_VO == MBUF_TC_VO);
	_CASSERT(KPKT_TC_MAX == MBUF_TC_MAX);

	_CASSERT(KPKT_TC_BE == PKT_TC_BE);
	_CASSERT(KPKT_TC_BK == PKT_TC_BK);
	_CASSERT(KPKT_TC_VI == PKT_TC_VI);
	_CASSERT(KPKT_TC_VO == PKT_TC_VO);

	_CASSERT(PKT_SCVAL_BK_SYS == SCVAL_BK_SYS);
	_CASSERT(PKT_SCVAL_BK == SCVAL_BK);
	_CASSERT(PKT_SCVAL_BE == SCVAL_BE);
	_CASSERT(PKT_SCVAL_RD == SCVAL_RD);
	_CASSERT(PKT_SCVAL_OAM == SCVAL_OAM);
	_CASSERT(PKT_SCVAL_AV == SCVAL_AV);
	_CASSERT(PKT_SCVAL_RV == SCVAL_RV);
	_CASSERT(PKT_SCVAL_VI == SCVAL_VI);
	_CASSERT(PKT_SCVAL_VO == SCVAL_VO);
	_CASSERT(PKT_SCVAL_CTL == SCVAL_CTL);

	/*
	 * Assert that the value of common packet flags between mbuf and
	 * skywalk packets match, and that they are in PKT_F_COMMON_MASK.
	 */
	_CASSERT(PKT_F_BACKGROUND == PKTF_SO_BACKGROUND);
	_CASSERT(PKT_F_REALTIME == PKTF_SO_REALTIME);
	_CASSERT(PKT_F_REXMT == PKTF_TCP_REXMT);
	_CASSERT(PKT_F_LAST_PKT == PKTF_LAST_PKT);
	_CASSERT(PKT_F_FLOW_ID == PKTF_FLOW_ID);
	_CASSERT(PKT_F_FLOW_ADV == PKTF_FLOW_ADV);
	_CASSERT(PKT_F_TX_COMPL_TS_REQ == PKTF_TX_COMPL_TS_REQ);
	_CASSERT(PKT_F_TS_VALID == PKTF_TS_VALID);
	_CASSERT(PKT_F_NEW_FLOW == PKTF_NEW_FLOW);
	_CASSERT(PKT_F_START_SEQ == PKTF_START_SEQ);
	_CASSERT(PKT_F_KEEPALIVE == PKTF_KEEPALIVE);
	_CASSERT(PKT_F_WAKE_PKT == PKTF_WAKE_PKT);
	_CASSERT(PKT_F_COMMON_MASK == (PKT_F_BACKGROUND | PKT_F_REALTIME |
	    PKT_F_REXMT | PKT_F_LAST_PKT | PKT_F_FLOW_ID | PKT_F_FLOW_ADV |
	    PKT_F_TX_COMPL_TS_REQ | PKT_F_TS_VALID | PKT_F_NEW_FLOW |
	    PKT_F_START_SEQ | PKT_F_KEEPALIVE | PKT_F_WAKE_PKT));
	/*
	 * Assert packet flags shared with userland.
	 */
	_CASSERT(PKT_F_USER_MASK == (PKT_F_BACKGROUND | PKT_F_REALTIME |
	    PKT_F_REXMT | PKT_F_LAST_PKT | PKT_F_OPT_DATA | PKT_F_PROMISC |
	    PKT_F_TRUNCATED | PKT_F_WAKE_PKT | PKT_F_L4S));

	_CASSERT(offsetof(struct __kern_quantum, qum_len) ==
	    offsetof(struct __kern_packet, pkt_length));

	/*
	 * Due to the use of tagged pointer, we need the size of
	 * the metadata preamble structure to be multiples of 16.
	 * See SK_PTR_TAG() definition for details.
	 */
	_CASSERT(sizeof(struct __metadata_preamble) != 0 &&
	    (sizeof(struct __metadata_preamble) % 16) == 0);

	_CASSERT(NX_PBUF_FRAGS_MIN == 1 &&
	    NX_PBUF_FRAGS_MIN == NX_PBUF_FRAGS_DEFAULT);

	/*
	 * Batch alloc/free requires linking the objects together;
	 * make sure that the fields are at the same offset since
	 * we cast the object to struct skmem_obj.
	 */
	_CASSERT(offsetof(struct __metadata_preamble, _mdp_next) ==
	    offsetof(struct skmem_obj, mo_next));
	_CASSERT(offsetof(struct __buflet, __buflet_next) ==
	    offsetof(struct skmem_obj, mo_next));

	SK_LOCK_ASSERT_HELD();
	ASSERT(!__pp_inited);

	pp_opt_cache = skmem_cache_create("pkt.opt",
	    sizeof(struct __packet_opt), sizeof(uint64_t),
	    NULL, NULL, NULL, NULL, NULL, 0);
	pp_flow_cache = skmem_cache_create("pkt.flow",
	    sizeof(struct __flow), 16,  /* 16-bytes aligned */
	    NULL, NULL, NULL, NULL, NULL, 0);
	pp_compl_cache = skmem_cache_create("pkt.compl",
	    sizeof(struct __packet_compl), sizeof(uint64_t),
	    NULL, NULL, NULL, NULL, NULL, 0);

	return 0;
}

void
pp_fini(void)
{
	SK_LOCK_ASSERT_HELD();

	if (__pp_inited) {
		if (pp_compl_cache != NULL) {
			skmem_cache_destroy(pp_compl_cache);
			pp_compl_cache = NULL;
		}
		if (pp_flow_cache != NULL) {
			skmem_cache_destroy(pp_flow_cache);
			pp_flow_cache = NULL;
		}
		if (pp_opt_cache != NULL) {
			skmem_cache_destroy(pp_opt_cache);
			pp_opt_cache = NULL;
		}

		__pp_inited = 0;
	}
}

static struct kern_pbufpool *
pp_alloc(zalloc_flags_t how)
{
	struct kern_pbufpool *pp = zalloc_flags(pp_zone, how | Z_ZERO);

	if (pp) {
		lck_mtx_init(&pp->pp_lock, &skmem_lock_grp, &skmem_lock_attr);
	}
	return pp;
}

static void
pp_free(struct kern_pbufpool *pp)
{
	PP_LOCK_ASSERT_HELD(pp);

	pp_destroy(pp);
	PP_UNLOCK(pp);

	SK_DF(SK_VERB_MEM, "pp 0x%llx FREE", SK_KVA(pp));
	lck_mtx_destroy(&pp->pp_lock, &skmem_lock_grp);
	zfree(pp_zone, pp);
}

void
pp_retain_locked(struct kern_pbufpool *pp)
{
	PP_LOCK_ASSERT_HELD(pp);

	pp->pp_refcnt++;
	ASSERT(pp->pp_refcnt != 0);
}

void
pp_retain(struct kern_pbufpool *pp)
{
	PP_LOCK(pp);
	pp_retain_locked(pp);
	PP_UNLOCK(pp);
}

boolean_t
pp_release_locked(struct kern_pbufpool *pp)
{
	uint32_t oldref = pp->pp_refcnt;

	PP_LOCK_ASSERT_HELD(pp);

	ASSERT(pp->pp_refcnt != 0);
	if (--pp->pp_refcnt == 0) {
		pp_free(pp);
	}

	return oldref == 1;
}

boolean_t
pp_release(struct kern_pbufpool *pp)
{
	boolean_t lastref;

	PP_LOCK(pp);
	if (!(lastref = pp_release_locked(pp))) {
		PP_UNLOCK(pp);
	}

	return lastref;
}

void
pp_close(struct kern_pbufpool *pp)
{
	PP_LOCK(pp);
	ASSERT(pp->pp_refcnt > 0);
	ASSERT(!(pp->pp_flags & PPF_CLOSED));
	pp->pp_flags |= PPF_CLOSED;
	if (!pp_release_locked(pp)) {
		PP_UNLOCK(pp);
	}
}

/*
 * -fbounds-safety: All callers of pp_regions_params_adjust use SKMEM_REGIONS
 * size for the srp_array. This is same as marking it __counted_by(SKMEM_REGIONS)
 */
void
pp_regions_params_adjust(struct skmem_region_params srp_array[SKMEM_REGIONS],
    nexus_meta_type_t md_type, nexus_meta_subtype_t md_subtype, uint32_t md_cnt,
    uint16_t max_frags, uint32_t buf_size, uint32_t large_buf_size,
    uint32_t buf_cnt, uint32_t buf_seg_size, uint32_t flags)
{
	struct skmem_region_params *srp, *kmd_srp, *buf_srp, *kbft_srp,
	    *lbuf_srp;
	uint32_t md_size = 0;
	bool kernel_only = ((flags & PP_REGION_CONFIG_KERNEL_ONLY) != 0);
	bool md_persistent = ((flags & PP_REGION_CONFIG_MD_PERSISTENT) != 0);
	bool buf_persistent = ((flags & PP_REGION_CONFIG_BUF_PERSISTENT) != 0);
	bool config_buflet = ((flags & PP_REGION_CONFIG_BUFLET) != 0);
	bool md_magazine_enable = ((flags &
	    PP_REGION_CONFIG_MD_MAGAZINE_ENABLE) != 0);

	ASSERT(max_frags != 0);

	switch (md_type) {
	case NEXUS_META_TYPE_QUANTUM:
		md_size = NX_METADATA_QUANTUM_SZ;
		break;
	case NEXUS_META_TYPE_PACKET:
		md_size = NX_METADATA_PACKET_SZ(max_frags);
		break;
	default:
		VERIFY(0);
		/* NOTREACHED */
		__builtin_unreachable();
	}

	switch (flags & PP_REGION_CONFIG_BUF_IODIR_BIDIR) {
	case PP_REGION_CONFIG_BUF_IODIR_IN:
		kmd_srp = &srp_array[SKMEM_REGION_RXKMD];
		buf_srp = &srp_array[SKMEM_REGION_RXBUF_DEF];
		lbuf_srp = &srp_array[SKMEM_REGION_RXBUF_LARGE];
		kbft_srp = &srp_array[SKMEM_REGION_RXKBFT];
		break;
	case PP_REGION_CONFIG_BUF_IODIR_OUT:
		kmd_srp = &srp_array[SKMEM_REGION_TXKMD];
		buf_srp = &srp_array[SKMEM_REGION_TXBUF_DEF];
		lbuf_srp = &srp_array[SKMEM_REGION_TXBUF_LARGE];
		kbft_srp = &srp_array[SKMEM_REGION_TXKBFT];
		break;
	case PP_REGION_CONFIG_BUF_IODIR_BIDIR:
	default:
		kmd_srp = &srp_array[SKMEM_REGION_KMD];
		buf_srp = &srp_array[SKMEM_REGION_BUF_DEF];
		lbuf_srp = &srp_array[SKMEM_REGION_BUF_LARGE];
		kbft_srp = &srp_array[SKMEM_REGION_KBFT];
		break;
	}

	/* add preamble size to metadata obj size */
	md_size += METADATA_PREAMBLE_SZ;
	ASSERT(md_size >= NX_METADATA_OBJ_MIN_SZ);

	/* configure kernel metadata region */
	kmd_srp->srp_md_type = md_type;
	kmd_srp->srp_md_subtype = md_subtype;
	kmd_srp->srp_r_obj_cnt = md_cnt;
	kmd_srp->srp_r_obj_size = md_size;
	kmd_srp->srp_max_frags = max_frags;
	ASSERT((kmd_srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT) == 0);
	if (md_persistent) {
		kmd_srp->srp_cflags |= SKMEM_REGION_CR_PERSISTENT;
	}
	ASSERT((kmd_srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES) != 0);
	if (md_magazine_enable) {
		kmd_srp->srp_cflags &= ~SKMEM_REGION_CR_NOMAGAZINES;
	}
	skmem_region_params_config(kmd_srp);

	/* configure user metadata region */
	srp = &srp_array[SKMEM_REGION_UMD];
	if (!kernel_only) {
		srp->srp_md_type = kmd_srp->srp_md_type;
		srp->srp_md_subtype = kmd_srp->srp_md_subtype;
		srp->srp_r_obj_cnt = kmd_srp->srp_c_obj_cnt;
		srp->srp_r_obj_size = kmd_srp->srp_c_obj_size;
		srp->srp_max_frags = kmd_srp->srp_max_frags;
		ASSERT((srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT) == 0);
		if (md_persistent) {
			srp->srp_cflags |= SKMEM_REGION_CR_PERSISTENT;
		}
		/*
		 * UMD is a mirrored region and object allocation operations
		 * are performed on the KMD objects.
		 */
		ASSERT((srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES) != 0);
		skmem_region_params_config(srp);
		ASSERT(srp->srp_c_obj_cnt == kmd_srp->srp_c_obj_cnt);
	} else {
		ASSERT(srp->srp_r_obj_cnt == 0);
		ASSERT(srp->srp_r_obj_size == 0);
	}

	/* configure buffer region */
	buf_srp->srp_r_obj_cnt = MAX(buf_cnt, kmd_srp->srp_c_obj_cnt);
	buf_srp->srp_r_obj_size = buf_size;
	buf_srp->srp_cflags &= ~SKMEM_REGION_CR_MONOLITHIC;
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT) == 0);
	if (buf_persistent) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_PERSISTENT;
	}
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES) != 0);
	if (buf_srp->srp_r_obj_size >= KERN_BUF_MIN_STRIDING_SIZE) {
		/*
		 * A buffer size larger than 256K indicates striding is in use, which
		 * means a buffer could be detached from a buflet. In this case, magzine
		 * layer should be enabled.
		 */
		buf_srp->srp_cflags &= ~SKMEM_REGION_CR_NOMAGAZINES;
	}
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_UREADONLY) == 0);
	if ((flags & PP_REGION_CONFIG_BUF_UREADONLY) != 0) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_UREADONLY;
	}
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_KREADONLY) == 0);
	if ((flags & PP_REGION_CONFIG_BUF_KREADONLY) != 0) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_KREADONLY;
	}
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_MONOLITHIC) == 0);
	if ((flags & PP_REGION_CONFIG_BUF_MONOLITHIC) != 0) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_MONOLITHIC;
	}
	ASSERT((srp->srp_cflags & SKMEM_REGION_CR_SEGPHYSCONTIG) == 0);
	if ((flags & PP_REGION_CONFIG_BUF_SEGPHYSCONTIG) != 0) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_SEGPHYSCONTIG;
	}
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_NOCACHE) == 0);
	if ((flags & PP_REGION_CONFIG_BUF_NOCACHE) != 0) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_NOCACHE;
	}
	ASSERT((buf_srp->srp_cflags & SKMEM_REGION_CR_THREADSAFE) == 0);
	if ((flags & PP_REGION_CONFIG_BUF_THREADSAFE) != 0) {
		buf_srp->srp_cflags |= SKMEM_REGION_CR_THREADSAFE;
	}
	if (buf_seg_size != 0) {
		buf_srp->srp_r_seg_size = buf_seg_size;
	}
	skmem_region_params_config(buf_srp);

	/* configure large buffer region */
	if (large_buf_size != 0) {
		lbuf_srp->srp_r_obj_cnt = buf_srp->srp_r_obj_cnt;
		lbuf_srp->srp_r_obj_size = large_buf_size;
		lbuf_srp->srp_r_seg_size = buf_srp->srp_r_seg_size;
		lbuf_srp->srp_cflags = buf_srp->srp_cflags;
		skmem_region_params_config(lbuf_srp);
	}

	/* configure kernel buflet region */
	if (config_buflet) {
		ASSERT(md_type == NEXUS_META_TYPE_PACKET);
		/*
		 * Ideally we want the number of buflets to be
		 * "kmd_srp->srp_c_obj_cnt * (kmd_srp->srp_max_frags - 1)",
		 * so that we have enough buflets when multi-buflet and
		 * shared buffer object is used.
		 * Currently multi-buflet is being used only by user pool
		 * which doesn't support shared buffer object, hence to reduce
		 * the number of objects we are restricting the number of
		 * buflets to the number of buffers.
		 */
		kbft_srp->srp_r_obj_cnt = buf_srp->srp_c_obj_cnt +
		    lbuf_srp->srp_c_obj_cnt;
		kbft_srp->srp_r_obj_size = MAX(sizeof(struct __kern_buflet_ext),
		    sizeof(struct __user_buflet));
		kbft_srp->srp_cflags = kmd_srp->srp_cflags;
		skmem_region_params_config(kbft_srp);
		ASSERT(kbft_srp->srp_c_obj_cnt >= buf_srp->srp_c_obj_cnt +
		    lbuf_srp->srp_c_obj_cnt);
	} else {
		ASSERT(kbft_srp->srp_r_obj_cnt == 0);
		ASSERT(kbft_srp->srp_r_obj_size == 0);
	}

	/* configure user buflet region */
	srp = &srp_array[SKMEM_REGION_UBFT];
	if (config_buflet && !kernel_only) {
		srp->srp_r_obj_cnt = kbft_srp->srp_c_obj_cnt;
		srp->srp_r_obj_size = kbft_srp->srp_c_obj_size;
		srp->srp_cflags = srp_array[SKMEM_REGION_UMD].srp_cflags;
		skmem_region_params_config(srp);
		ASSERT(srp->srp_c_obj_cnt == kbft_srp->srp_c_obj_cnt);
	} else {
		ASSERT(srp->srp_r_obj_cnt == 0);
		ASSERT(srp->srp_r_obj_size == 0);
	}

	/* make sure each metadata can be paired with a buffer */
	ASSERT(kmd_srp->srp_c_obj_cnt <= buf_srp->srp_c_obj_cnt);
}

SK_NO_INLINE_ATTRIBUTE
static int
pp_metadata_construct(struct __kern_quantum *kqum, struct __user_quantum *uqum,
    obj_idx_t midx, struct kern_pbufpool *pp, uint32_t skmflag, uint16_t bufcnt,
    bool raw, struct skmem_obj **blist)
{
	struct __kern_buflet *kbuf;
	mach_vm_address_t baddr = 0;
	uint16_t *pbufs_cnt, *pbufs_max;
	uint16_t i;

	ASSERT(bufcnt == 1 || PP_HAS_BUFFER_ON_DEMAND(pp));

	/* construct {user,kernel} metadata */
	switch (pp->pp_md_type) {
	case NEXUS_META_TYPE_PACKET: {
		struct __kern_packet *kpkt = SK_PTR_ADDR_KPKT(kqum);
		struct __user_packet *upkt = SK_PTR_ADDR_UPKT(uqum);
		struct __packet_opt *__single opt;
		struct __flow *__single flow;
		struct __packet_compl *__single compl;
		uint64_t pflags;

		if (raw) {
			opt = skmem_cache_alloc(pp_opt_cache, SKMEM_SLEEP);
			flow = skmem_cache_alloc(pp_flow_cache, SKMEM_SLEEP);
			compl = skmem_cache_alloc(pp_compl_cache, SKMEM_SLEEP);
			pflags = (PKT_F_OPT_ALLOC | PKT_F_FLOW_ALLOC |
			    PKT_F_TX_COMPL_ALLOC);
		} else {
			ASSERT((kpkt->pkt_pflags & PKT_F_OPT_ALLOC) &&
			    kpkt->pkt_com_opt != NULL);
			opt = kpkt->pkt_com_opt;
			ASSERT((kpkt->pkt_pflags & PKT_F_FLOW_ALLOC) &&
			    kpkt->pkt_flow != NULL);
			flow = kpkt->pkt_flow;
			ASSERT((kpkt->pkt_pflags & PKT_F_TX_COMPL_ALLOC) &&
			    kpkt->pkt_tx_compl != NULL);
			compl = kpkt->pkt_tx_compl;
			pflags = kpkt->pkt_pflags;
		}
		/* will be adjusted below as part of allocating buffer(s) */
		_CASSERT(sizeof(kpkt->pkt_bufs_cnt) == sizeof(uint16_t));
		_CASSERT(sizeof(kpkt->pkt_bufs_max) == sizeof(uint16_t));
		pbufs_cnt = __DECONST(uint16_t *, &kpkt->pkt_bufs_cnt);
		pbufs_max = __DECONST(uint16_t *, &kpkt->pkt_bufs_max);

		/* kernel (and user) packet */
		KPKT_CTOR(kpkt, pflags, opt, flow, compl, midx,
		    upkt, pp, 0, pp->pp_max_frags, 0);
		break;
	}
	default:
		ASSERT(pp->pp_md_type == NEXUS_META_TYPE_QUANTUM);
		VERIFY(bufcnt == 1);
		/* TODO: point these to quantum's once they're defined */
		pbufs_cnt = pbufs_max = NULL;
		/* kernel quantum */
		KQUM_CTOR(kqum, midx, uqum, pp, 0);
		break;
	}

	kbuf = kqum->qum_buf;
	for (i = 0; i < bufcnt; i++) {
		struct skmem_obj_info oib;

		if (!PP_HAS_BUFFER_ON_DEMAND(pp)) {
			ASSERT(i == 0);
			ASSERT(*blist == NULL);
			/*
			 * quantum has a native buflet, so we only need a
			 * buffer to be allocated and attached to the buflet.
			 */
			baddr = pp_alloc_buffer_common(pp, &oib, skmflag,
			    false);
			if (__improbable(baddr == 0)) {
				goto fail;
			}
			KBUF_CTOR(kbuf, baddr, SKMEM_OBJ_IDX_REG(&oib),
			    SKMEM_OBJ_BUFCTL(&oib), pp, false);
			baddr = 0;
		} else {
			/*
			 * we use pre-constructed buflets with attached buffers.
			 */
			struct __kern_buflet *pkbuf = kbuf;
			struct skmem_obj *blistn;

			ASSERT(pkbuf != NULL);
			kbuf = (kern_buflet_t)*blist;
			if (__improbable(kbuf == NULL)) {
				SK_DF(SK_VERB_MEM, "failed to get buflet,"
				    " pp 0x%llx", SK_KVA(pp));
				goto fail;
			}

#if CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT)
			/* Checking to ensure the object address is tagged */
			ASSERT((vm_offset_t)kbuf !=
			    vm_memtag_canonicalize_address((vm_offset_t)kbuf));
#endif /* CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */

			blistn = (*blist)->mo_next;
			(*blist)->mo_next = NULL;

			KBUF_EXT_INIT(kbuf, pp);
			KBUF_LINK(pkbuf, kbuf);
			*blist = blistn;
		}

		/* adjust buffer count accordingly */
		if (__probable(pbufs_cnt != NULL)) {
			*pbufs_cnt += 1;
			ASSERT(*pbufs_cnt <= *pbufs_max);
		}
	}

	ASSERT(!PP_KERNEL_ONLY(pp) || (kqum->qum_qflags & QUM_F_KERNEL_ONLY));
	ASSERT(METADATA_IDX(kqum) != OBJ_IDX_NONE);
	SK_DF(SK_VERB_MEM, "pp 0x%llx pkt 0x%llx bufcnt %d buf 0x%llx",
	    SK_KVA(pp), SK_KVA(kqum), bufcnt, SK_KVA(baddr));
	return 0;

fail:
	ASSERT(bufcnt != 0 && baddr == 0);
	pp_metadata_destruct(kqum, pp, raw);
	return ENOMEM;
}

static int
pp_metadata_ctor_common(struct skmem_obj_info *oi0,
    struct skmem_obj_info *oim0, struct kern_pbufpool *pp, uint32_t skmflag,
    bool no_buflet)
{
	struct skmem_obj_info _oi, _oim;
	struct skmem_obj_info *oi, *oim;
	struct __kern_quantum *kqum;
	struct __user_quantum *uqum;
	uint16_t bufcnt = (no_buflet ? 0 : pp->pp_max_frags);
	struct skmem_obj *__single blist = NULL;
	int error;

#if (DEVELOPMENT || DEBUG)
	uint64_t mtbf = skmem_region_get_mtbf();
	/*
	 * MTBF is applicable only for non-blocking allocations here.
	 */
	if (__improbable(mtbf != 0 && (net_uptime_ms() % mtbf) == 0 &&
	    (skmflag & SKMEM_NOSLEEP))) {
		SK_ERR("pp \"%s\" MTBF failure", pp->pp_name);
		net_update_uptime();
		return ENOMEM;
	}
#endif /* (DEVELOPMENT || DEBUG) */

	/*
	 * Note that oi0 and oim0 may be stored inside the object itself;
	 * if so, copy them to local variables before constructing.  We
	 * don't use PPF_BATCH to test as the allocator may be allocating
	 * storage space differently depending on the number of objects.
	 */
	if (__probable((uintptr_t)oi0 >= (uintptr_t)SKMEM_OBJ_ADDR(oi0) &&
	    ((uintptr_t)oi0 + sizeof(*oi0)) <=
	    ((uintptr_t)SKMEM_OBJ_ADDR(oi0) + SKMEM_OBJ_SIZE(oi0)))) {
		oi = &_oi;
		*oi = *oi0;
		if (__probable(oim0 != NULL)) {
			oim = &_oim;
			*oim = *oim0;
		} else {
			oim = NULL;
		}
	} else {
		oi = oi0;
		oim = oim0;
	}

	kqum = SK_PTR_ADDR_KQUM((uintptr_t)SKMEM_OBJ_ADDR(oi) +
	    METADATA_PREAMBLE_SZ);

	if (__probable(!PP_KERNEL_ONLY(pp))) {
		ASSERT(oim != NULL && SKMEM_OBJ_ADDR(oim) != NULL);
		ASSERT(SKMEM_OBJ_SIZE(oi) == SKMEM_OBJ_SIZE(oim));
		uqum = SK_PTR_ADDR_UQUM((uintptr_t)SKMEM_OBJ_ADDR(oim) +
		    METADATA_PREAMBLE_SZ);
	} else {
		ASSERT(oim == NULL);
		uqum = NULL;
	}

	if (oim != NULL) {
		/* initialize user metadata redzone */
		struct __metadata_preamble *mdp = SKMEM_OBJ_ADDR(oim);
		mdp->mdp_redzone =
		    (SKMEM_OBJ_ROFF(oim) + METADATA_PREAMBLE_SZ) ^
		    __ch_umd_redzone_cookie;
	}

	/* allocate (constructed) buflet(s) with buffer(s) attached */
	if (PP_HAS_BUFFER_ON_DEMAND(pp) && bufcnt != 0) {
		(void) skmem_cache_batch_alloc(PP_KBFT_CACHE_DEF(pp), &blist,
		    PP_KBFT_CACHE_DEF(pp)->skm_objsize, bufcnt, skmflag);
	}

	error = pp_metadata_construct(kqum, uqum, SKMEM_OBJ_IDX_REG(oi), pp,
	    skmflag, bufcnt, TRUE, &blist);
	if (__improbable(blist != NULL)) {
		skmem_cache_batch_free(PP_KBFT_CACHE_DEF(pp), blist);
		blist = NULL;
	}
	return error;
}

static int
pp_metadata_ctor_no_buflet(struct skmem_obj_info *oi0,
    struct skmem_obj_info *oim0, void *arg, uint32_t skmflag)
{
	return pp_metadata_ctor_common(oi0, oim0, arg, skmflag, true);
}

static int
pp_metadata_ctor_max_buflet(struct skmem_obj_info *oi0,
    struct skmem_obj_info *oim0, void *arg, uint32_t skmflag)
{
	return pp_metadata_ctor_common(oi0, oim0, arg, skmflag, false);
}

__attribute__((always_inline))
static void
pp_metadata_destruct_common(struct __kern_quantum *kqum,
    struct kern_pbufpool *pp, bool raw, struct skmem_obj **blist_def,
    struct skmem_obj **blist_nocache_def, struct skmem_obj **blist_large,
    struct skmem_obj **blist_nocache_large)
{
	struct __kern_buflet *kbuf, *nbuf;
	struct skmem_obj *__single p_blist_def = NULL, *__single p_blist_large = NULL;
	struct skmem_obj *__single p_blist_nocache_def = NULL, *__single p_blist_nocache_large = NULL;
	struct skmem_obj **pp_blist_def = &p_blist_def;
	struct skmem_obj **pp_blist_large = &p_blist_large;
	struct skmem_obj **pp_blist_nocache_def = &p_blist_nocache_def;
	struct skmem_obj **pp_blist_nocache_large = &p_blist_nocache_large;
	uint16_t bufcnt, i = 0;
	bool first_buflet_empty;

	ASSERT(blist_def != NULL);
	ASSERT(blist_large != NULL);

	switch (pp->pp_md_type) {
	case NEXUS_META_TYPE_PACKET: {
		struct __kern_packet *kpkt = SK_PTR_ADDR_KPKT(kqum);

		ASSERT(kpkt->pkt_user != NULL || PP_KERNEL_ONLY(pp));
		ASSERT(kpkt->pkt_qum.qum_pp == pp);
		ASSERT(METADATA_TYPE(kpkt) == pp->pp_md_type);
		ASSERT(METADATA_SUBTYPE(kpkt) == pp->pp_md_subtype);
		ASSERT(METADATA_IDX(kpkt) != OBJ_IDX_NONE);
		ASSERT(kpkt->pkt_qum.qum_ksd == NULL);
		ASSERT(kpkt->pkt_bufs_cnt <= kpkt->pkt_bufs_max);
		ASSERT(kpkt->pkt_bufs_max == pp->pp_max_frags);
		_CASSERT(sizeof(kpkt->pkt_bufs_cnt) == sizeof(uint16_t));
		bufcnt = kpkt->pkt_bufs_cnt;
		kbuf = &kqum->qum_buf[0];
		/*
		 * special handling for empty first buflet.
		 */
		first_buflet_empty = (kbuf->buf_addr == 0);
		*__DECONST(uint16_t *, &kpkt->pkt_bufs_cnt) = 0;
		break;
	}
	default:
		ASSERT(pp->pp_md_type == NEXUS_META_TYPE_QUANTUM);
		ASSERT(kqum->qum_user != NULL || PP_KERNEL_ONLY(pp));
		ASSERT(kqum->qum_pp == pp);
		ASSERT(METADATA_TYPE(kqum) == pp->pp_md_type);
		ASSERT(METADATA_SUBTYPE(kqum) == pp->pp_md_subtype);
		ASSERT(METADATA_IDX(kqum) != OBJ_IDX_NONE);
		ASSERT(kqum->qum_ksd == NULL);
		kbuf = &kqum->qum_buf[0];
		/*
		 * XXX: Special handling for quantum as we don't currently
		 * define bufs_{cnt,max} there.  Given that we support at
		 * most only 1 buflet for now, check if buf_addr is non-NULL.
		 * See related code in pp_metadata_construct().
		 */
		first_buflet_empty = (kbuf->buf_addr == 0);
		bufcnt = first_buflet_empty ? 0 : 1;
		break;
	}

	/*
	 * -fbounds-safety: buf_nbft_addr is a mach_vm_address_t which is
	 * unsafe, so we forge it here.
	 */
	nbuf = __unsafe_forge_single(struct __kern_buflet *,
	    __DECONST(struct __kern_buflet *, kbuf->buf_nbft_addr));
	BUF_NBFT_ADDR(kbuf, 0);
	BUF_NBFT_IDX(kbuf, OBJ_IDX_NONE);
	if (!first_buflet_empty) {
		pp_free_buflet_common(pp, kbuf);
		++i;
	}

	while (nbuf != NULL) {
		ASSERT(nbuf->buf_ctl != NULL);
		if (BUFLET_HAS_LARGE_BUF(nbuf)) {
			/*
			 * bc_usecnt larger than 1 means the buffer has been cloned and is
			 * still being used by other bflts. In this case, when we free
			 * this bflt we need to explicitly ask for it to not be cached again
			 * into magzine layer to prevent immediate reuse of the buffer and
			 * data corruption.
			 */
			if (nbuf->buf_ctl->bc_usecnt > 1) {
				*pp_blist_nocache_large = (struct skmem_obj *)(void *)nbuf;
				pp_blist_nocache_large =
				    &((struct skmem_obj *)(void *)nbuf)->mo_next;
			} else {
				*pp_blist_large = (struct skmem_obj *)(void *)nbuf;
				pp_blist_large =
				    &((struct skmem_obj *)(void *)nbuf)->mo_next;
			}
		} else {
			if (nbuf->buf_ctl->bc_usecnt > 1) {
				*pp_blist_nocache_def = (struct skmem_obj *)(void *)nbuf;
				pp_blist_nocache_def =
				    &((struct skmem_obj *)(void *)nbuf)->mo_next;
			} else {
				*pp_blist_def = (struct skmem_obj *)(void *)nbuf;
				pp_blist_def =
				    &((struct skmem_obj *)(void *)nbuf)->mo_next;
			}
		}
		BUF_NBFT_IDX(nbuf, OBJ_IDX_NONE);
		nbuf = __unsafe_forge_single(struct __kern_buflet *,
		    __DECONST(struct __kern_buflet *, nbuf->buf_nbft_addr));
		++i;
	}

	ASSERT(i == bufcnt);

	if (p_blist_def != NULL) {
		*pp_blist_def = *blist_def;
		*blist_def = p_blist_def;
	}
	if (p_blist_large != NULL) {
		*pp_blist_large = *blist_large;
		*blist_large = p_blist_large;
	}
	if (p_blist_nocache_def != NULL) {
		*pp_blist_nocache_def = *blist_nocache_def;
		*blist_nocache_def = p_blist_nocache_def;
	}
	if (p_blist_nocache_large != NULL) {
		*pp_blist_nocache_large = *blist_nocache_large;
		*blist_nocache_large = p_blist_nocache_large;
	}

	/* if we're about to return this object to the slab, clean it up */
	if (raw) {
		switch (pp->pp_md_type) {
		case NEXUS_META_TYPE_PACKET: {
			struct __kern_packet *kpkt = SK_PTR_ADDR_KPKT(kqum);

			ASSERT(kpkt->pkt_com_opt != NULL ||
			    !(kpkt->pkt_pflags & PKT_F_OPT_ALLOC));
			if (kpkt->pkt_com_opt != NULL) {
				ASSERT(kpkt->pkt_pflags & PKT_F_OPT_ALLOC);
				skmem_cache_free(pp_opt_cache,
				    kpkt->pkt_com_opt);
				kpkt->pkt_com_opt = NULL;
			}
			ASSERT(kpkt->pkt_flow != NULL ||
			    !(kpkt->pkt_pflags & PKT_F_FLOW_ALLOC));
			if (kpkt->pkt_flow != NULL) {
				ASSERT(kpkt->pkt_pflags & PKT_F_FLOW_ALLOC);
				skmem_cache_free(pp_flow_cache, kpkt->pkt_flow);
				kpkt->pkt_flow = NULL;
			}
			ASSERT(kpkt->pkt_tx_compl != NULL ||
			    !(kpkt->pkt_pflags & PKT_F_TX_COMPL_ALLOC));
			if (kpkt->pkt_tx_compl != NULL) {
				ASSERT(kpkt->pkt_pflags & PKT_F_TX_COMPL_ALLOC);
				skmem_cache_free(pp_compl_cache,
				    kpkt->pkt_tx_compl);
				kpkt->pkt_tx_compl = NULL;
			}
			kpkt->pkt_pflags = 0;
			break;
		}
		default:
			ASSERT(METADATA_TYPE(kqum) == NEXUS_META_TYPE_QUANTUM);
			/* nothing to do for quantum (yet) */
			break;
		}
	}
}

__attribute__((always_inline))
static void
pp_free_kbft_list(struct kern_pbufpool *pp, struct skmem_obj *blist_def, struct skmem_obj *blist_nocache_def,
    struct skmem_obj *blist_large, struct skmem_obj *blist_nocache_large)
{
	if (blist_def != NULL) {
		skmem_cache_batch_free(PP_KBFT_CACHE_DEF(pp), blist_def);
	}
	if (blist_large != NULL) {
		skmem_cache_batch_free(PP_KBFT_CACHE_LARGE(pp), blist_large);
	}
	if (blist_nocache_def != NULL) {
		skmem_cache_batch_free_nocache(PP_KBFT_CACHE_DEF(pp), blist_nocache_def);
	}
	if (blist_nocache_large != NULL) {
		skmem_cache_batch_free_nocache(PP_KBFT_CACHE_LARGE(pp), blist_nocache_large);
	}
}

__attribute__((always_inline))
static void
pp_metadata_destruct(struct __kern_quantum *kqum, struct kern_pbufpool *pp,
    bool raw)
{
	struct skmem_obj *__single blist_def = NULL, *__single blist_large = NULL;
	struct skmem_obj *__single blist_nocache_def = NULL, *__single blist_nocache_large = NULL;

	pp_metadata_destruct_common(kqum, pp, raw, &blist_def, &blist_nocache_def,
	    &blist_large, &blist_nocache_large);
	pp_free_kbft_list(pp, blist_def, blist_nocache_def, blist_large, blist_nocache_large);
}

static void
pp_metadata_dtor(void *addr, void *arg)
{
	pp_metadata_destruct(SK_PTR_ADDR_KQUM((uintptr_t)addr +
	    METADATA_PREAMBLE_SZ), arg, TRUE);
}

static void
pp_buf_seg_ctor(struct sksegment *sg, IOSKMemoryBufferRef md, void *arg)
{
	struct kern_pbufpool *__single pp = arg;

	if (pp->pp_pbuf_seg_ctor != NULL) {
		pp->pp_pbuf_seg_ctor(pp, sg, md);
	}
}

static void
pp_buf_seg_dtor(struct sksegment *sg, IOSKMemoryBufferRef md, void *arg)
{
	struct kern_pbufpool *__single pp = arg;

	if (pp->pp_pbuf_seg_dtor != NULL) {
		pp->pp_pbuf_seg_dtor(pp, sg, md);
	}
}

static int
pp_buflet_metadata_ctor_common(struct skmem_obj_info *oi0,
    struct skmem_obj_info *oim0, void *arg, uint32_t skmflag, bool large)
{
#pragma unused (skmflag)
	struct kern_pbufpool *pp = (struct kern_pbufpool *)arg;
	struct __kern_buflet *kbft;
	struct __user_buflet *ubft;
	struct skmem_obj_info oib;
	mach_vm_address_t baddr;
	obj_idx_t oi_idx_reg;

	baddr = pp_alloc_buffer_common(pp, &oib, skmflag, large);
	if (__improbable(baddr == 0)) {
		return ENOMEM;
	}
	/*
	 * Note that oi0 and oim0 may be stored inside the object itself;
	 * so copy what is required to local variables before constructing.
	 */
	oi_idx_reg = SKMEM_OBJ_IDX_REG(oi0);
	kbft = SKMEM_OBJ_ADDR(oi0);

	if (__probable(!PP_KERNEL_ONLY(pp))) {
		ASSERT(oim0 != NULL && SKMEM_OBJ_ADDR(oim0) != NULL);
		ASSERT(SKMEM_OBJ_SIZE(oi0) == SKMEM_OBJ_SIZE(oim0));
		ASSERT(oi_idx_reg == SKMEM_OBJ_IDX_REG(oim0));
		ASSERT(SKMEM_OBJ_IDX_SEG(oi0) == SKMEM_OBJ_IDX_SEG(oim0));
		ubft = SKMEM_OBJ_ADDR(oim0);
	} else {
		ASSERT(oim0 == NULL);
		ubft = NULL;
	}
	KBUF_EXT_CTOR(kbft, ubft, baddr, SKMEM_OBJ_IDX_REG(&oib),
	    SKMEM_OBJ_BUFCTL(&oib), oi_idx_reg, pp, large);
	return 0;
}

static int
pp_buflet_default_buffer_metadata_ctor(struct skmem_obj_info *oi0,
    struct skmem_obj_info *oim0, void *arg, uint32_t skmflag)
{
	return pp_buflet_metadata_ctor_common(oi0, oim0, arg, skmflag, false);
}

static int
pp_buflet_large_buffer_metadata_ctor(struct skmem_obj_info *oi0,
    struct skmem_obj_info *oim0, void *arg, uint32_t skmflag)
{
	return pp_buflet_metadata_ctor_common(oi0, oim0, arg, skmflag, true);
}

static void
pp_buflet_metadata_dtor(void *addr, void *arg)
{
	struct __kern_buflet *__single kbft = addr;
	void *objaddr = kbft->buf_objaddr;
	struct kern_pbufpool *__single pp = arg;
	uint32_t usecnt = 0;
	bool large = BUFLET_HAS_LARGE_BUF(kbft);

	ASSERT(kbft->buf_flag & BUFLET_FLAG_EXTERNAL);
	/*
	 * don't assert for (buf_nbft_addr == 0) here as constructed
	 * buflet may have this field as non-zero. This is because
	 * buf_nbft_addr (__buflet_next) is used by skmem batch alloc
	 * for chaining the buflets.
	 * To ensure that the frred buflet was not part of a chain we
	 * assert for (buf_nbft_idx == OBJ_IDX_NONE).
	 */
	ASSERT(kbft->buf_nbft_idx == OBJ_IDX_NONE);
	ASSERT(((struct __kern_buflet_ext *)kbft)->kbe_buf_upp_link.sle_next ==
	    NULL);
	ASSERT(kbft->buf_addr != 0);
	ASSERT(kbft->buf_idx != OBJ_IDX_NONE);
	ASSERT(kbft->buf_ctl != NULL);

	KBUF_DTOR(kbft, usecnt);
	SK_DF(SK_VERB_MEM, "pp 0x%llx buf 0x%llx usecnt %u", SK_KVA(pp),
	    SK_KVA(objaddr), usecnt);
	if (__probable(usecnt == 0)) {
		skmem_cache_free(large ? PP_BUF_CACHE_LARGE(pp) :
		    PP_BUF_CACHE_DEF(pp), objaddr);
	}
}

/*
 * -fbounds-safety: all callers of pp_create use srp_array with a known size:
 * SKMEM_REGIONS. This is same as marking it __counted_by(SKMEM_REGIONS)
 */
struct kern_pbufpool *
pp_create(const char *name, struct skmem_region_params srp_array[SKMEM_REGIONS],
    pbuf_seg_ctor_fn_t buf_seg_ctor, pbuf_seg_dtor_fn_t buf_seg_dtor,
    const void *ctx, pbuf_ctx_retain_fn_t ctx_retain,
    pbuf_ctx_release_fn_t ctx_release, uint32_t ppcreatef)
{
	struct kern_pbufpool *pp = NULL;
	uint32_t md_size, def_buf_obj_size;
	uint32_t def_buf_size, large_buf_size;
	nexus_meta_type_t md_type;
	nexus_meta_subtype_t md_subtype;
	uint32_t md_cflags;
	uint16_t max_frags;
	uint32_t buf_def_cflags;
	char cname[64];
	const char *__null_terminated cache_name = NULL;
	struct skmem_region_params *kmd_srp;
	struct skmem_region_params *buf_srp;
	struct skmem_region_params *kbft_srp;
	struct skmem_region_params *umd_srp = NULL;
	struct skmem_region_params *ubft_srp = NULL;
	struct skmem_region_params *lbuf_srp = NULL;

	/* buf_seg_{ctor,dtor} pair must be either NULL or non-NULL */
	ASSERT(!(!(buf_seg_ctor == NULL && buf_seg_dtor == NULL) &&
	    ((buf_seg_ctor == NULL) ^ (buf_seg_dtor == NULL))));

	/* ctx{,_retain,_release} must be either ALL NULL or ALL non-NULL */
	ASSERT((ctx == NULL && ctx_retain == NULL && ctx_release == NULL) ||
	    (ctx != NULL && ctx_retain != NULL && ctx_release != NULL));

	if (srp_array[SKMEM_REGION_KMD].srp_c_obj_cnt != 0) {
		kmd_srp = &srp_array[SKMEM_REGION_KMD];
		buf_srp = &srp_array[SKMEM_REGION_BUF_DEF];
		lbuf_srp = &srp_array[SKMEM_REGION_BUF_LARGE];
		kbft_srp = &srp_array[SKMEM_REGION_KBFT];
	} else if (srp_array[SKMEM_REGION_RXKMD].srp_c_obj_cnt != 0) {
		kmd_srp = &srp_array[SKMEM_REGION_RXKMD];
		buf_srp = &srp_array[SKMEM_REGION_RXBUF_DEF];
		lbuf_srp = &srp_array[SKMEM_REGION_RXBUF_LARGE];
		kbft_srp = &srp_array[SKMEM_REGION_RXKBFT];
	} else {
		VERIFY(srp_array[SKMEM_REGION_TXKMD].srp_c_obj_cnt != 0);
		kmd_srp = &srp_array[SKMEM_REGION_TXKMD];
		buf_srp = &srp_array[SKMEM_REGION_TXBUF_DEF];
		lbuf_srp = &srp_array[SKMEM_REGION_TXBUF_LARGE];
		kbft_srp = &srp_array[SKMEM_REGION_TXKBFT];
	}

	VERIFY(kmd_srp->srp_c_obj_size != 0);
	VERIFY(buf_srp->srp_c_obj_cnt != 0);
	VERIFY(buf_srp->srp_c_obj_size != 0);

	if (ppcreatef & PPCREATEF_ONDEMAND_BUF) {
		VERIFY(kbft_srp->srp_c_obj_cnt != 0);
		VERIFY(kbft_srp->srp_c_obj_size != 0);
	} else {
		kbft_srp = NULL;
	}

	if ((ppcreatef & PPCREATEF_KERNEL_ONLY) == 0) {
		umd_srp = &srp_array[SKMEM_REGION_UMD];
		ASSERT(umd_srp->srp_c_obj_size == kmd_srp->srp_c_obj_size);
		ASSERT(umd_srp->srp_c_obj_cnt == kmd_srp->srp_c_obj_cnt);
		ASSERT(umd_srp->srp_c_seg_size == kmd_srp->srp_c_seg_size);
		ASSERT(umd_srp->srp_seg_cnt == kmd_srp->srp_seg_cnt);
		ASSERT(umd_srp->srp_md_type == kmd_srp->srp_md_type);
		ASSERT(umd_srp->srp_md_subtype == kmd_srp->srp_md_subtype);
		ASSERT(umd_srp->srp_max_frags == kmd_srp->srp_max_frags);
		ASSERT((umd_srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT) ==
		    (kmd_srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT));
		if (kbft_srp != NULL) {
			ubft_srp = &srp_array[SKMEM_REGION_UBFT];
			ASSERT(ubft_srp->srp_c_obj_size ==
			    kbft_srp->srp_c_obj_size);
			ASSERT(ubft_srp->srp_c_obj_cnt ==
			    kbft_srp->srp_c_obj_cnt);
			ASSERT(ubft_srp->srp_c_seg_size ==
			    kbft_srp->srp_c_seg_size);
			ASSERT(ubft_srp->srp_seg_cnt == kbft_srp->srp_seg_cnt);
		}
	}

	md_size = kmd_srp->srp_r_obj_size;
	md_type = kmd_srp->srp_md_type;
	md_subtype = kmd_srp->srp_md_subtype;
	max_frags = kmd_srp->srp_max_frags;
	def_buf_obj_size = buf_srp->srp_c_obj_size;
	def_buf_size = def_buf_obj_size;
	large_buf_size = lbuf_srp->srp_c_obj_size;

#if (DEBUG || DEVELOPMENT)
	ASSERT(def_buf_obj_size != 0);
	ASSERT(md_type > NEXUS_META_TYPE_INVALID &&
	    md_type <= NEXUS_META_TYPE_MAX);
	if (md_type == NEXUS_META_TYPE_QUANTUM) {
		ASSERT(max_frags == 1);
		ASSERT(md_size >=
		    (METADATA_PREAMBLE_SZ + NX_METADATA_QUANTUM_SZ));
	} else {
		ASSERT(max_frags >= 1);
		ASSERT(md_type == NEXUS_META_TYPE_PACKET);
		ASSERT(md_size >= (METADATA_PREAMBLE_SZ +
		    NX_METADATA_PACKET_SZ(max_frags)));
	}
	ASSERT(md_subtype > NEXUS_META_SUBTYPE_INVALID &&
	    md_subtype <= NEXUS_META_SUBTYPE_MAX);
#endif /* DEBUG || DEVELOPMENT */

	pp = pp_alloc(Z_WAITOK);

	(void) snprintf((char *)pp->pp_name, sizeof(pp->pp_name),
	    "skywalk.pp.%s", name);

	pp->pp_ctx = __DECONST(void *, ctx);
	pp->pp_ctx_retain = ctx_retain;
	pp->pp_ctx_release = ctx_release;
	if (pp->pp_ctx != NULL) {
		pp->pp_ctx_retain(pp->pp_ctx);
	}

	pp->pp_pbuf_seg_ctor = buf_seg_ctor;
	pp->pp_pbuf_seg_dtor = buf_seg_dtor;
	PP_BUF_SIZE_DEF(pp) = def_buf_size;
	PP_BUF_OBJ_SIZE_DEF(pp) = def_buf_obj_size;
	PP_BUF_SIZE_LARGE(pp) = large_buf_size;
	PP_BUF_OBJ_SIZE_LARGE(pp) = lbuf_srp->srp_c_obj_size;
	pp->pp_md_type = md_type;
	pp->pp_md_subtype = md_subtype;
	pp->pp_max_frags = max_frags;
	if (ppcreatef & PPCREATEF_EXTERNAL) {
		pp->pp_flags |= PPF_EXTERNAL;
	}
	if (ppcreatef & PPCREATEF_TRUNCATED_BUF) {
		pp->pp_flags |= PPF_TRUNCATED_BUF;
	}
	if (ppcreatef & PPCREATEF_KERNEL_ONLY) {
		pp->pp_flags |= PPF_KERNEL;
	}
	if (ppcreatef & PPCREATEF_ONDEMAND_BUF) {
		pp->pp_flags |= PPF_BUFFER_ON_DEMAND;
	}
	if (ppcreatef & PPCREATEF_DYNAMIC) {
		pp->pp_flags |= PPF_DYNAMIC;
	}
	if (lbuf_srp->srp_c_obj_cnt > 0) {
		ASSERT(lbuf_srp->srp_c_obj_size != 0);
		pp->pp_flags |= PPF_LARGE_BUF;
	}

	pp_retain(pp);

	md_cflags = ((kmd_srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES) ?
	    SKMEM_CR_NOMAGAZINES : 0);
	md_cflags |= SKMEM_CR_BATCH;
	pp->pp_flags |= PPF_BATCH;

	if (pp->pp_flags & PPF_DYNAMIC) {
		md_cflags |= SKMEM_CR_DYNAMIC;
	}

	if (umd_srp != NULL && (pp->pp_umd_region =
	    skmem_region_create(name, umd_srp, NULL, NULL, NULL)) == NULL) {
		SK_ERR("\"%s\" (0x%llx) failed to create %s region",
		    pp->pp_name, SK_KVA(pp), umd_srp->srp_name);
		goto failed;
	}

	if ((pp->pp_kmd_region = skmem_region_create(name, kmd_srp, NULL, NULL,
	    NULL)) == NULL) {
		SK_ERR("\"%s\" (0x%llx) failed to create %s region",
		    pp->pp_name, SK_KVA(pp), kmd_srp->srp_name);
		goto failed;
	}

	if (PP_HAS_BUFFER_ON_DEMAND(pp)) {
		VERIFY((kbft_srp != NULL) && (kbft_srp->srp_c_obj_cnt > 0));
		if (!PP_KERNEL_ONLY(pp)) {
			VERIFY((ubft_srp != NULL) &&
			    (ubft_srp->srp_c_obj_cnt > 0));
		}
	}
	/*
	 * Metadata regions {KMD,KBFT,UBFT} magazines layer and persistency
	 * attribute must match.
	 */
	if (PP_HAS_BUFFER_ON_DEMAND(pp)) {
		ASSERT((kmd_srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES) ==
		    (kbft_srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES));
		ASSERT((kmd_srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT) ==
		    (kbft_srp->srp_cflags & SKMEM_REGION_CR_PERSISTENT));
	}

	if (PP_HAS_BUFFER_ON_DEMAND(pp) && !PP_KERNEL_ONLY(pp)) {
		if ((pp->pp_ubft_region = skmem_region_create(name, ubft_srp,
		    NULL, NULL, NULL)) == NULL) {
			SK_ERR("\"%s\" (0x%llx) failed to create %s region",
			    pp->pp_name, SK_KVA(pp), ubft_srp->srp_name);
			goto failed;
		}
	}

	if (PP_HAS_BUFFER_ON_DEMAND(pp)) {
		if ((pp->pp_kbft_region = skmem_region_create(name,
		    kbft_srp, NULL, NULL, NULL)) == NULL) {
			SK_ERR("\"%s\" (0x%llx) failed to create %s region",
			    pp->pp_name, SK_KVA(pp), kbft_srp->srp_name);
			goto failed;
		}
	}

	if (!PP_KERNEL_ONLY(pp)) {
		skmem_region_mirror(pp->pp_kmd_region, pp->pp_umd_region);
	}
	if (!PP_KERNEL_ONLY(pp) && pp->pp_ubft_region != NULL) {
		ASSERT(pp->pp_kbft_region != NULL);
		skmem_region_mirror(pp->pp_kbft_region, pp->pp_ubft_region);
	}

	/*
	 * Create the metadata cache; magazines layer is determined by caller.
	 */
	cache_name = tsnprintf(cname, sizeof(cname), "kmd.%s", name);
	if (PP_HAS_BUFFER_ON_DEMAND(pp)) {
		pp->pp_kmd_cache = skmem_cache_create(cache_name, md_size, 0,
		    pp_metadata_ctor_no_buflet, pp_metadata_dtor, NULL, pp,
		    pp->pp_kmd_region, md_cflags);
	} else {
		pp->pp_kmd_cache = skmem_cache_create(cache_name, md_size, 0,
		    pp_metadata_ctor_max_buflet, pp_metadata_dtor, NULL, pp,
		    pp->pp_kmd_region, md_cflags);
	}

	if (pp->pp_kmd_cache == NULL) {
		SK_ERR("\"%s\" (0x%llx) failed to create \"%s\" cache",
		    pp->pp_name, SK_KVA(pp), cname);
		goto failed;
	}

	/*
	 * Create the buflet metadata cache
	 */
	if (pp->pp_kbft_region != NULL) {
		cache_name = tsnprintf(cname, sizeof(cname), "kbft_def.%s", name);
		PP_KBFT_CACHE_DEF(pp) = skmem_cache_create(cache_name,
		    kbft_srp->srp_c_obj_size, 0,
		    pp_buflet_default_buffer_metadata_ctor,
		    pp_buflet_metadata_dtor, NULL, pp, pp->pp_kbft_region,
		    md_cflags);

		if (PP_KBFT_CACHE_DEF(pp) == NULL) {
			SK_ERR("\"%s\" (0x%llx) failed to create \"%s\" cache",
			    pp->pp_name, SK_KVA(pp), cname);
			goto failed;
		}

		if (PP_HAS_LARGE_BUF(pp)) {
			/* Aggressive memory reclaim flag set to kbft_large for now */
			md_cflags |= SKMEM_CR_RECLAIM;
			cache_name = tsnprintf(cname, sizeof(cname),
			    "kbft_large.%s", name);
			PP_KBFT_CACHE_LARGE(pp) = skmem_cache_create(cache_name,
			    kbft_srp->srp_c_obj_size, 0,
			    pp_buflet_large_buffer_metadata_ctor,
			    pp_buflet_metadata_dtor,
			    NULL, pp, pp->pp_kbft_region, md_cflags);

			if (PP_KBFT_CACHE_LARGE(pp) == NULL) {
				SK_ERR("\"%s\" (0x%llx) failed to "
				    "create \"%s\" cache", pp->pp_name,
				    SK_KVA(pp), cname);
				goto failed;
			}
		}
	}

	if ((PP_BUF_REGION_DEF(pp) = skmem_region_create(name,
	    buf_srp, pp_buf_seg_ctor, pp_buf_seg_dtor, pp)) == NULL) {
		SK_ERR("\"%s\" (0x%llx) failed to create %s region",
		    pp->pp_name, SK_KVA(pp), buf_srp->srp_name);
		goto failed;
	}

	if (PP_HAS_LARGE_BUF(pp)) {
		PP_BUF_REGION_LARGE(pp) = skmem_region_create(name, lbuf_srp,
		    pp_buf_seg_ctor, pp_buf_seg_dtor, pp);
		if (PP_BUF_REGION_LARGE(pp) == NULL) {
			SK_ERR("\"%s\" (0x%llx) failed to create %s region",
			    pp->pp_name, SK_KVA(pp), lbuf_srp->srp_name);
			goto failed;
		}
	}

	/*
	 * Create the buffer object cache without the magazines layer.
	 * We rely on caching the constructed metadata object instead.
	 */
	cache_name = tsnprintf(cname, sizeof(cname), "buf_def.%s", name);
	buf_def_cflags = buf_srp->srp_cflags & SKMEM_REGION_CR_NOMAGAZINES ? SKMEM_CR_NOMAGAZINES : 0;
	if ((PP_BUF_CACHE_DEF(pp) = skmem_cache_create(cache_name,
	    def_buf_obj_size,
	    0, NULL, NULL, NULL, pp, PP_BUF_REGION_DEF(pp),
	    buf_def_cflags)) == NULL) {
		SK_ERR("\"%s\" (0x%llx) failed to create \"%s\" cache",
		    pp->pp_name, SK_KVA(pp), cname);
		goto failed;
	}

	if (PP_BUF_REGION_LARGE(pp) != NULL) {
		cache_name = tsnprintf(cname, sizeof(cname), "buf_large.%s", name);
		if ((PP_BUF_CACHE_LARGE(pp) = skmem_cache_create(cache_name,
		    lbuf_srp->srp_c_obj_size, 0, NULL, NULL, NULL, pp,
		    PP_BUF_REGION_LARGE(pp), SKMEM_CR_NOMAGAZINES)) == NULL) {
			SK_ERR("\"%s\" (0x%llx) failed to create \"%s\" cache",
			    pp->pp_name, SK_KVA(pp), cname);
			goto failed;
		}
	}

	return pp;

failed:
	if (pp != NULL) {
		if (pp->pp_ctx != NULL) {
			pp->pp_ctx_release(pp->pp_ctx);
			pp->pp_ctx = NULL;
		}
		pp_close(pp);
	}

	return NULL;
}

void
pp_destroy(struct kern_pbufpool *pp)
{
	PP_LOCK_ASSERT_HELD(pp);

	/* may be called for built-in pp with outstanding reference */
	ASSERT(!(pp->pp_flags & PPF_EXTERNAL) || pp->pp_refcnt == 0);

	pp_destroy_upp_locked(pp);

	pp_destroy_upp_bft_locked(pp);

	if (pp->pp_kmd_cache != NULL) {
		skmem_cache_destroy(pp->pp_kmd_cache);
		pp->pp_kmd_cache = NULL;
	}

	if (pp->pp_umd_region != NULL) {
		skmem_region_release(pp->pp_umd_region);
		pp->pp_umd_region = NULL;
	}

	if (pp->pp_kmd_region != NULL) {
		skmem_region_release(pp->pp_kmd_region);
		pp->pp_kmd_region = NULL;
	}

	if (PP_KBFT_CACHE_DEF(pp) != NULL) {
		skmem_cache_destroy(PP_KBFT_CACHE_DEF(pp));
		PP_KBFT_CACHE_DEF(pp) = NULL;
	}

	if (PP_KBFT_CACHE_LARGE(pp) != NULL) {
		skmem_cache_destroy(PP_KBFT_CACHE_LARGE(pp));
		PP_KBFT_CACHE_LARGE(pp) = NULL;
	}

	if (pp->pp_ubft_region != NULL) {
		skmem_region_release(pp->pp_ubft_region);
		pp->pp_ubft_region = NULL;
	}

	if (pp->pp_kbft_region != NULL) {
		skmem_region_release(pp->pp_kbft_region);
		pp->pp_kbft_region = NULL;
	}

	/*
	 * The order is important here, since pp_metadata_dtor()
	 * called by freeing on the pp_kmd_cache will in turn
	 * free the attached buffer.  Therefore destroy the
	 * buffer cache last.
	 */
	if (PP_BUF_CACHE_DEF(pp) != NULL) {
		skmem_cache_destroy(PP_BUF_CACHE_DEF(pp));
		PP_BUF_CACHE_DEF(pp) = NULL;
	}
	if (PP_BUF_REGION_DEF(pp) != NULL) {
		skmem_region_release(PP_BUF_REGION_DEF(pp));
		PP_BUF_REGION_DEF(pp) = NULL;
	}
	if (PP_BUF_CACHE_LARGE(pp) != NULL) {
		skmem_cache_destroy(PP_BUF_CACHE_LARGE(pp));
		PP_BUF_CACHE_LARGE(pp) = NULL;
	}
	if (PP_BUF_REGION_LARGE(pp) != NULL) {
		skmem_region_release(PP_BUF_REGION_LARGE(pp));
		PP_BUF_REGION_LARGE(pp) = NULL;
	}

	if (pp->pp_ctx != NULL) {
		pp->pp_ctx_release(pp->pp_ctx);
		pp->pp_ctx = NULL;
	}
}

static int
pp_init_upp_locked(struct kern_pbufpool *pp, boolean_t can_block)
{
	int i, err = 0;

	if (pp->pp_u_hash_table != NULL) {
		goto done;
	}

	/* allocated-address hash table */
	/*
	 * -fbounds-safety: We switched to sk_alloc (aka kalloc) from zalloc, so
	 * if we see any performance hit, we can check if this caused it.
	 */
	if (can_block) {
		pp->pp_u_hash_table = sk_alloc_type_array(
			struct kern_pbufpool_u_bkt, KERN_PBUFPOOL_U_HASH_SIZE,
			Z_WAITOK, skmem_tag_pbufpool_hash);
		pp->pp_u_hash_table_size = KERN_PBUFPOOL_U_HASH_SIZE;
	} else {
		pp->pp_u_hash_table = sk_alloc_type_array(
			struct kern_pbufpool_u_bkt, KERN_PBUFPOOL_U_HASH_SIZE,
			Z_NOWAIT, skmem_tag_pbufpool_hash);
		pp->pp_u_hash_table_size = KERN_PBUFPOOL_U_HASH_SIZE;
	}
	if (pp->pp_u_hash_table == NULL) {
		SK_ERR("failed to zalloc packet buffer pool upp hash table");
		err = ENOMEM;
		goto done;
	}

	for (i = 0; i < KERN_PBUFPOOL_U_HASH_SIZE; i++) {
		SLIST_INIT(&pp->pp_u_hash_table[i].upp_head);
	}
done:
	return err;
}

static void
pp_destroy_upp_locked(struct kern_pbufpool *pp)
{
	PP_LOCK_ASSERT_HELD(pp);
	if (pp->pp_u_hash_table != NULL) {
		/* purge anything that's left */
		pp_purge_upp_locked(pp, -1);

#if (DEBUG || DEVELOPMENT)
		for (int i = 0; i < KERN_PBUFPOOL_U_HASH_SIZE; i++) {
			ASSERT(SLIST_EMPTY(&pp->pp_u_hash_table[i].upp_head));
		}
#endif /* DEBUG || DEVELOPMENT */

		kfree_type_counted_by(struct kern_pbufpool_u_bkt,
		    pp->pp_u_hash_table_size,
		    pp->pp_u_hash_table);
	}
	ASSERT(pp->pp_u_bufinuse == 0);
}

int
pp_init_upp(struct kern_pbufpool *pp, boolean_t can_block)
{
	int err = 0;

	PP_LOCK(pp);
	err = pp_init_upp_locked(pp, can_block);
	if (err) {
		SK_ERR("packet UPP init failed (%d)", err);
		goto done;
	}
	err = pp_init_upp_bft_locked(pp, can_block);
	if (err) {
		SK_ERR("buflet UPP init failed (%d)", err);
		pp_destroy_upp_locked(pp);
		goto done;
	}
	pp_retain_locked(pp);
done:
	PP_UNLOCK(pp);
	return err;
}

__attribute__((always_inline))
static void
pp_insert_upp_bft_locked(struct kern_pbufpool *pp,
    struct __kern_buflet *kbft, pid_t pid)
{
	struct kern_pbufpool_u_bft_bkt *bkt;
	struct __kern_buflet_ext *kbe = (struct __kern_buflet_ext *)kbft;

	ASSERT(kbft->buf_flag & BUFLET_FLAG_EXTERNAL);
	ASSERT(kbe->kbe_buf_pid == (pid_t)-1);
	kbe->kbe_buf_pid = pid;
	bkt = KERN_PBUFPOOL_U_BFT_HASH(pp, kbft->buf_bft_idx_reg);
	SLIST_INSERT_HEAD(&bkt->upp_head, kbe, kbe_buf_upp_link);
	pp->pp_u_bftinuse++;
}

__attribute__((always_inline))
static void
pp_insert_upp_bft_chain_locked(struct kern_pbufpool *pp,
    struct __kern_buflet *kbft, pid_t pid)
{
	while (kbft != NULL) {
		pp_insert_upp_bft_locked(pp, kbft, pid);
		kbft = __unsafe_forge_single(struct __kern_buflet *,
		    __DECONST(kern_buflet_t, kbft->buf_nbft_addr));
	}
}

/* Also inserts the attached chain of buflets */
void static inline
pp_insert_upp_common(struct kern_pbufpool *pp, struct __kern_quantum *kqum,
    pid_t pid)
{
	struct kern_pbufpool_u_bkt *bkt;
	struct __kern_buflet *kbft;

	ASSERT(kqum->qum_pid == (pid_t)-1);
	kqum->qum_pid = pid;

	bkt = KERN_PBUFPOOL_U_HASH(pp, METADATA_IDX(kqum));
	SLIST_INSERT_HEAD(&bkt->upp_head, kqum, qum_upp_link);
	pp->pp_u_bufinuse++;

	kbft = __unsafe_forge_single(struct __kern_buflet *, (kern_buflet_t)kqum->qum_buf[0].buf_nbft_addr);
	if (kbft != NULL) {
		ASSERT(((kern_buflet_t)kbft)->buf_flag & BUFLET_FLAG_EXTERNAL);
		ASSERT(kqum->qum_qflags & QUM_F_INTERNALIZED);
		pp_insert_upp_bft_chain_locked(pp, kbft, pid);
	}
}

void
pp_insert_upp_locked(struct kern_pbufpool *pp, struct __kern_quantum *kqum,
    pid_t pid)
{
	pp_insert_upp_common(pp, kqum, pid);
}

void
pp_insert_upp(struct kern_pbufpool *pp, struct __kern_quantum *kqum, pid_t pid)
{
	PP_LOCK(pp);
	pp_insert_upp_common(pp, kqum, pid);
	PP_UNLOCK(pp);
}

void
pp_insert_upp_batch(struct kern_pbufpool *pp, pid_t pid,
    uint64_t *__counted_by(num)array, uint32_t num)
{
	uint32_t i = 0;

	ASSERT(array != NULL && num > 0);
	PP_LOCK(pp);
	while (i < num) {
		struct __kern_quantum *kqum = SK_PTR_ADDR_KQUM(array[i]);

		ASSERT(kqum != NULL);
		pp_insert_upp_common(pp, kqum, pid);
		++i;
	}
	PP_UNLOCK(pp);
}

__attribute__((always_inline))
static struct __kern_buflet *
pp_remove_upp_bft_locked(struct kern_pbufpool *pp, obj_idx_t bft_idx)
{
	struct __kern_buflet_ext *kbft, *tbft;
	struct kern_pbufpool_u_bft_bkt *bkt;

	bkt = KERN_PBUFPOOL_U_BFT_HASH(pp, bft_idx);
	SLIST_FOREACH_SAFE(kbft, &bkt->upp_head, kbe_buf_upp_link, tbft) {
		if (((kern_buflet_t)kbft)->buf_bft_idx_reg == bft_idx) {
			SLIST_REMOVE(&bkt->upp_head, kbft, __kern_buflet_ext,
			    kbe_buf_upp_link);
			kbft->kbe_buf_pid = (pid_t)-1;
			kbft->kbe_buf_upp_link.sle_next = NULL;
			ASSERT(pp->pp_u_bftinuse != 0);
			pp->pp_u_bftinuse--;
			break;
		}
	}
	return (kern_buflet_t)kbft;
}

struct __kern_buflet *
pp_remove_upp_bft(struct kern_pbufpool *pp, obj_idx_t md_idx, int *err)
{
	struct __kern_buflet *kbft = pp_remove_upp_bft_locked(pp, md_idx);

	*err = __improbable(kbft != NULL) ? 0 : EINVAL;
	return kbft;
}

__attribute__((always_inline))
static int
pp_remove_upp_bft_chain_locked(struct kern_pbufpool *pp,
    struct __kern_quantum *kqum)
{
	uint32_t max_frags = pp->pp_max_frags;
	struct __kern_buflet *kbft;
	uint16_t nbfts, upkt_nbfts;
	obj_idx_t bft_idx;

	ASSERT(!(kqum->qum_qflags & QUM_F_INTERNALIZED));
	bft_idx = kqum->qum_user->qum_buf[0].buf_nbft_idx;
	kbft = &kqum->qum_buf[0];
	if (bft_idx == OBJ_IDX_NONE) {
		return 0;
	}

	ASSERT(METADATA_TYPE(kqum) == NEXUS_META_TYPE_PACKET);
	struct __kern_packet *kpkt = __DECONST(struct __kern_packet *, kqum);
	struct __user_packet *upkt = __DECONST(struct __user_packet *,
	    kpkt->pkt_qum.qum_user);

	upkt_nbfts = upkt->pkt_bufs_cnt;
	if (__improbable(upkt_nbfts > max_frags)) {
		SK_ERR("bad bcnt in upkt (%d > %d)", upkt_nbfts, max_frags);
		BUF_NBFT_IDX(kbft, OBJ_IDX_NONE);
		BUF_NBFT_ADDR(kbft, 0);
		return ERANGE;
	}

	nbfts = (kbft->buf_addr != 0) ? 1 : 0;

	do {
		struct __kern_buflet *pbft = kbft;
		struct __kern_buflet_ext *kbe;

		kbft = pp_remove_upp_bft_locked(pp, bft_idx);
		if (__improbable(kbft == NULL)) {
			BUF_NBFT_IDX(pbft, OBJ_IDX_NONE);
			BUF_NBFT_ADDR(pbft, 0);
			SK_ERR("unallocated next buflet (%d), %p", bft_idx,
			    SK_KVA(pbft));
			return ERANGE;
		}
		ASSERT(kbft->buf_flag & BUFLET_FLAG_EXTERNAL);
		BUF_NBFT_IDX(pbft, bft_idx);
		BUF_NBFT_ADDR(pbft, kbft);
		kbe = __container_of(kbft, struct __kern_buflet_ext, kbe_overlay);
		bft_idx = kbe->kbe_buf_user->buf_nbft_idx;
		++nbfts;
	} while ((bft_idx != OBJ_IDX_NONE) && (nbfts < upkt_nbfts));

	ASSERT(kbft != NULL);
	BUF_NBFT_IDX(kbft, OBJ_IDX_NONE);
	BUF_NBFT_ADDR(kbft, 0);
	*__DECONST(uint16_t *, &kpkt->pkt_bufs_cnt) = nbfts;

	if (__improbable((bft_idx != OBJ_IDX_NONE) || (nbfts != upkt_nbfts))) {
		SK_ERR("bad buflet in upkt (%d, %d)", nbfts, upkt_nbfts);
		return ERANGE;
	}
	return 0;
}

struct __kern_quantum *
pp_remove_upp_locked(struct kern_pbufpool *pp, obj_idx_t md_idx, int *err)
{
	struct __kern_quantum *kqum, *tqum;
	struct kern_pbufpool_u_bkt *bkt;

	bkt = KERN_PBUFPOOL_U_HASH(pp, md_idx);
	SLIST_FOREACH_SAFE(kqum, &bkt->upp_head, qum_upp_link, tqum) {
		if (METADATA_IDX(kqum) == md_idx) {
			SLIST_REMOVE(&bkt->upp_head, kqum, __kern_quantum,
			    qum_upp_link);
			kqum->qum_pid = (pid_t)-1;
			ASSERT(pp->pp_u_bufinuse != 0);
			pp->pp_u_bufinuse--;
			break;
		}
	}
	if (__probable(kqum != NULL)) {
		*err = pp_remove_upp_bft_chain_locked(pp, kqum);
	} else {
		*err = ERANGE;
	}
	return kqum;
}

struct __kern_quantum *
pp_remove_upp(struct kern_pbufpool *pp, obj_idx_t md_idx, int *err)
{
	struct __kern_quantum *kqum;

	PP_LOCK(pp);
	kqum = pp_remove_upp_locked(pp, md_idx, err);
	PP_UNLOCK(pp);
	return kqum;
}

struct __kern_quantum *
pp_find_upp(struct kern_pbufpool *pp, obj_idx_t md_idx)
{
	struct __kern_quantum *kqum, *tqum;
	struct kern_pbufpool_u_bkt *bkt;

	PP_LOCK(pp);
	bkt = KERN_PBUFPOOL_U_HASH(pp, md_idx);
	SLIST_FOREACH_SAFE(kqum, &bkt->upp_head, qum_upp_link, tqum) {
		if (METADATA_IDX(kqum) == md_idx) {
			break;
		}
	}
	PP_UNLOCK(pp);

	return kqum;
}

__attribute__((always_inline))
static void
pp_purge_upp_locked(struct kern_pbufpool *pp, pid_t pid)
{
	struct __kern_quantum *kqum, *tqum;
	struct kern_pbufpool_u_bkt *bkt;
	int i;

	PP_LOCK_ASSERT_HELD(pp);

	/*
	 * TODO: Build a list of packets and batch-free them.
	 */
	for (i = 0; i < KERN_PBUFPOOL_U_HASH_SIZE; i++) {
		bkt = &pp->pp_u_hash_table[i];
		SLIST_FOREACH_SAFE(kqum, &bkt->upp_head, qum_upp_link, tqum) {
			ASSERT(kqum->qum_pid != (pid_t)-1);
			if (pid != (pid_t)-1 && kqum->qum_pid != pid) {
				continue;
			}
			SLIST_REMOVE(&bkt->upp_head, kqum, __kern_quantum,
			    qum_upp_link);
			pp_remove_upp_bft_chain_locked(pp, kqum);
			kqum->qum_pid = (pid_t)-1;
			kqum->qum_qflags &= ~QUM_F_FINALIZED;
			kqum->qum_ksd = NULL;
			pp_free_packet(__DECONST(struct kern_pbufpool *,
			    kqum->qum_pp), (uint64_t)kqum);
			ASSERT(pp->pp_u_bufinuse != 0);
			pp->pp_u_bufinuse--;
		}
	}
}

__attribute__((always_inline))
static void
pp_purge_upp_bft_locked(struct kern_pbufpool *pp, pid_t pid)
{
	struct __kern_buflet_ext *kbft, *tbft;
	struct kern_pbufpool_u_bft_bkt *bkt;
	int i;

	PP_LOCK_ASSERT_HELD(pp);

	for (i = 0; i < KERN_PBUFPOOL_U_HASH_SIZE; i++) {
		bkt = &pp->pp_u_bft_hash_table[i];
		SLIST_FOREACH_SAFE(kbft, &bkt->upp_head, kbe_buf_upp_link,
		    tbft) {
			ASSERT(kbft->kbe_buf_pid != (pid_t)-1);
			if (pid != (pid_t)-1 && kbft->kbe_buf_pid != pid) {
				continue;
			}
			SLIST_REMOVE(&bkt->upp_head, kbft, __kern_buflet_ext,
			    kbe_buf_upp_link);
			kbft->kbe_buf_pid = (pid_t)-1;
			kbft->kbe_buf_upp_link.sle_next = NULL;
			pp_free_buflet(pp, (kern_buflet_t)kbft);
			ASSERT(pp->pp_u_bftinuse != 0);
			pp->pp_u_bftinuse--;
		}
	}
}

void
pp_purge_upp(struct kern_pbufpool *pp, pid_t pid)
{
	PP_LOCK(pp);
	pp_purge_upp_locked(pp, pid);
	pp_purge_upp_bft_locked(pp, pid);
	PP_UNLOCK(pp);
}

static int
pp_init_upp_bft_locked(struct kern_pbufpool *pp, boolean_t can_block)
{
	int i, err = 0;

	PP_LOCK_ASSERT_HELD(pp);
	if (pp->pp_u_bft_hash_table != NULL) {
		return 0;
	}

	/* allocated-address hash table */
	/*
	 * -fbounds-safety: We switched to sk_alloc (aka kalloc) from zalloc, so
	 * if we see any performance hit, we can check if this caused it.
	 */
	if (can_block) {
		pp->pp_u_bft_hash_table = sk_alloc_type_array(
			struct kern_pbufpool_u_bft_bkt, KERN_PBUFPOOL_U_HASH_SIZE,
			Z_WAITOK, skmem_tag_pbufpool_bft_hash);
		pp->pp_u_bft_hash_table_size = KERN_PBUFPOOL_U_HASH_SIZE;
	} else {
		pp->pp_u_bft_hash_table = sk_alloc_type_array(
			struct kern_pbufpool_u_bft_bkt, KERN_PBUFPOOL_U_HASH_SIZE,
			Z_NOWAIT, skmem_tag_pbufpool_bft_hash);
		pp->pp_u_bft_hash_table_size = KERN_PBUFPOOL_U_HASH_SIZE;
	}
	if (pp->pp_u_bft_hash_table == NULL) {
		SK_ERR("failed to zalloc packet buffer pool upp buflet hash table");
		err = ENOMEM;
		goto fail;
	}

	for (i = 0; i < KERN_PBUFPOOL_U_HASH_SIZE; i++) {
		SLIST_INIT(&pp->pp_u_bft_hash_table[i].upp_head);
	}

fail:
	return err;
}

static void
pp_destroy_upp_bft_locked(struct kern_pbufpool *pp)
{
	PP_LOCK_ASSERT_HELD(pp);
	if (pp->pp_u_bft_hash_table != NULL) {
		/* purge anything that's left */
		pp_purge_upp_bft_locked(pp, -1);

#if (DEBUG || DEVELOPMENT)
		for (int i = 0; i < KERN_PBUFPOOL_U_HASH_SIZE; i++) {
			ASSERT(SLIST_EMPTY(&pp->pp_u_bft_hash_table[i].upp_head));
		}
#endif /* DEBUG || DEVELOPMENT */

		kfree_type_counted_by(struct kern_pbufpool_u_bft_bkt,
		    pp->pp_u_bft_hash_table_size,
		    pp->pp_u_bft_hash_table);
	}
	ASSERT(pp->pp_u_bftinuse == 0);
}

void
pp_insert_upp_bft(struct kern_pbufpool *pp,
    struct __kern_buflet *kbft, pid_t pid)
{
	PP_LOCK(pp);
	pp_insert_upp_bft_locked(pp, kbft, pid);
	PP_UNLOCK(pp);
}

boolean_t
pp_isempty_upp(struct kern_pbufpool *pp)
{
	boolean_t isempty;

	PP_LOCK(pp);
	isempty = (pp->pp_u_bufinuse == 0);
	PP_UNLOCK(pp);

	return isempty;
}

__attribute__((always_inline))
static inline struct __kern_quantum *
pp_metadata_init(struct __metadata_preamble *mdp, struct kern_pbufpool *pp,
    uint16_t bufcnt, uint32_t skmflag, struct skmem_obj **blist)
{
	struct __kern_quantum *kqum;
	struct __user_quantum *uqum;

	kqum = SK_PTR_ADDR_KQUM((uintptr_t)mdp + METADATA_PREAMBLE_SZ);
	ASSERT(kqum->qum_pp == pp);
	if (__probable(!PP_KERNEL_ONLY(pp))) {
		ASSERT(!(kqum->qum_qflags & QUM_F_KERNEL_ONLY));
		uqum =  __DECONST(struct __user_quantum *, kqum->qum_user);
		ASSERT(uqum != NULL);
	} else {
		ASSERT(kqum->qum_qflags & QUM_F_KERNEL_ONLY);
		ASSERT(kqum->qum_user == NULL);
		uqum = NULL;
	}

	if (PP_HAS_BUFFER_ON_DEMAND(pp) && bufcnt != 0 &&
	    pp_metadata_construct(kqum, uqum, METADATA_IDX(kqum), pp,
	    skmflag, bufcnt, FALSE, blist) != 0) {
		return NULL;
	}

	/* (re)construct {user,kernel} metadata */
	switch (pp->pp_md_type) {
	case NEXUS_META_TYPE_PACKET: {
		struct __kern_packet *kpkt = SK_PTR_ADDR_KPKT(kqum);
		struct __kern_buflet *kbuf = &kpkt->pkt_qum_buf;
		uint16_t i;

		/* sanitize flags */
		kpkt->pkt_pflags &= PKT_F_INIT_MASK;

		ASSERT((kpkt->pkt_pflags & PKT_F_OPT_ALLOC) &&
		    kpkt->pkt_com_opt != NULL);
		ASSERT((kpkt->pkt_pflags & PKT_F_FLOW_ALLOC) &&
		    kpkt->pkt_flow != NULL);
		ASSERT((kpkt->pkt_pflags & PKT_F_TX_COMPL_ALLOC) &&
		    kpkt->pkt_tx_compl != NULL);

		/*
		 * XXX: For now we always set PKT_F_FLOW_DATA;
		 * this is a no-op but done for consistency
		 * with the other PKT_F_*_DATA flags.
		 */
		kpkt->pkt_pflags |= PKT_F_FLOW_DATA;

		/* initialize kernel packet */
		KPKT_INIT(kpkt, QUM_F_INTERNALIZED);

		ASSERT(bufcnt || PP_HAS_BUFFER_ON_DEMAND(pp));
		if (PP_HAS_BUFFER_ON_DEMAND(pp)) {
			ASSERT(kbuf->buf_ctl == NULL);
			ASSERT(kbuf->buf_addr == 0);
			/*
			 * -fbounds-safety: buf_nbft_addr is a mach_vm_address_t
			 * which is unsafe, so we just forge it here.
			 */
			kbuf = __unsafe_forge_single(struct __kern_buflet *,
			    __DECONST(struct __kern_buflet *, kbuf->buf_nbft_addr));
		}
		/* initialize kernel buflet */
		for (i = 0; i < bufcnt; i++) {
			ASSERT(kbuf != NULL);
			KBUF_INIT(kbuf);
			kbuf = __unsafe_forge_single(struct __kern_buflet *,
			    __DECONST(struct __kern_buflet *, kbuf->buf_nbft_addr));
		}
		ASSERT((kbuf == NULL) || (bufcnt == 0));
		break;
	}
	default:
		ASSERT(pp->pp_md_type == NEXUS_META_TYPE_QUANTUM);
		/* kernel quantum */
		KQUM_INIT(kqum, QUM_F_INTERNALIZED);
		KBUF_INIT(&kqum->qum_buf[0]);
		break;
	}

	return kqum;
}

/*
 * When PPF_BUFFER_ON_DEMAND flag is set on packet pool creation, we create
 * packet descriptor cache with no buffer attached and a buflet cache with
 * cpu layer caching enabled. While operating in this mode, we can call
 * pp_alloc_packet_common() either with `bufcnt = 0` or `bufcnt = n`,
 * where n <= pp->pp_max_frags. If `bufcnt == 0` then we allocate packet
 * descriptor with no attached buffer from the metadata cache.
 * If `bufcnt != 0`, then this routine allocates packet descriptor and buflets
 * from their respective caches and constructs the packet on behalf of the
 * caller.
 */
__attribute__((always_inline))
static inline uint32_t
pp_alloc_packet_common(struct kern_pbufpool *pp, uint16_t bufcnt,
    uint64_t *__counted_by(num)array, uint32_t num, boolean_t tagged,
    alloc_cb_func_t cb, const void *ctx, uint32_t skmflag)
{
	struct __metadata_preamble *mdp;
	struct __kern_quantum *kqum = NULL;
	uint32_t allocp, need = num;
	struct skmem_obj *__single plist, *__single blist = NULL;
	uint64_t *array_cp;  /* -fbounds-safety */

	ASSERT(bufcnt <= pp->pp_max_frags);
	ASSERT(array != NULL && num > 0);
	ASSERT(PP_BATCH_CAPABLE(pp));

	/* allocate (constructed) packet(s) with buffer(s) attached */
	allocp = skmem_cache_batch_alloc(pp->pp_kmd_cache, &plist,
	    pp->pp_kmd_cache->skm_objsize, num, skmflag);

	/* allocate (constructed) buflet(s) with buffer(s) attached */
	if (PP_HAS_BUFFER_ON_DEMAND(pp) && bufcnt != 0 && allocp != 0) {
		(void) skmem_cache_batch_alloc(PP_KBFT_CACHE_DEF(pp), &blist,
		    PP_KBFT_CACHE_DEF(pp)->skm_objsize, (allocp * bufcnt), skmflag);
	}

	array_cp = array;
	while (plist != NULL) {
		struct skmem_obj *plistn;

		plistn = plist->mo_next;
		plist->mo_next = NULL;

		mdp = (struct __metadata_preamble *)(void *)plist;
		kqum = pp_metadata_init(mdp, pp, bufcnt, skmflag, &blist);
		if (kqum == NULL) {
			if (blist != NULL) {
				skmem_cache_batch_free(PP_KBFT_CACHE_DEF(pp),
				    blist);
				blist = NULL;
			}
			plist->mo_next = plistn;
			skmem_cache_batch_free(pp->pp_kmd_cache, plist);
			plist = NULL;
			break;
		}

#if CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT)
		/* Checking to ensure the object address is tagged */
		ASSERT((vm_offset_t)kqum !=
		    vm_memtag_canonicalize_address((vm_offset_t)kqum));
#endif /* CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */

		if (tagged) {
			*array_cp = SK_PTR_ENCODE(kqum, METADATA_TYPE(kqum),
			    METADATA_SUBTYPE(kqum));
		} else {
			*array_cp = (uint64_t)kqum;
		}

		if (cb != NULL) {
			(cb)(*array_cp, (num - need), ctx);
		}

		++array_cp;
		plist = plistn;

		ASSERT(need > 0);
		--need;
	}
	ASSERT(blist == NULL);
	ASSERT((num - need) == allocp || kqum == NULL);

	return num - need;
}

uint64_t
pp_alloc_packet(struct kern_pbufpool *pp, uint16_t bufcnt, uint32_t skmflag)
{
	uint64_t kpkt = 0;

	(void) pp_alloc_packet_common(pp, bufcnt, &kpkt, 1, FALSE,
	    NULL, NULL, skmflag);

	return kpkt;
}

int
pp_alloc_packet_batch(struct kern_pbufpool *pp, uint16_t bufcnt,
    uint64_t *__counted_by(*size)array, uint32_t *size, boolean_t tagged,
    alloc_cb_func_t cb, const void *ctx, uint32_t skmflag)
{
	uint32_t i, n;
	int err;

	ASSERT(array != NULL && size > 0);

	n = *size;
	/*
	 * -fbounds-safety: Originally there was this line here: *size = 0; but
	 * we removed this because array is now __counted_by(*size), so *size =
	 * 0 leads to brk 0x5519. Also, *size is set to i anyway.
	 */

	i = pp_alloc_packet_common(pp, bufcnt, array, n, tagged,
	    cb, ctx, skmflag);
	/*
	 * -fbounds-safety: Since array is __counted_by(*size), we need to be
	 * extra careful when *size is updated, like below. Here, we know i will
	 * be less than or equal to the original *size value, so updating *size
	 * is okay.
	 */
	*size = i;

	if (__probable(i == n)) {
		err = 0;
	} else if (i != 0) {
		err = EAGAIN;
	} else {
		err = ENOMEM;
	}

	return err;
}

int
pp_alloc_pktq(struct kern_pbufpool *pp, uint16_t bufcnt,
    struct pktq *pktq, uint32_t num, alloc_cb_func_t cb, const void *ctx,
    uint32_t skmflag)
{
	struct __metadata_preamble *mdp;
	struct __kern_packet *kpkt = NULL;
	uint32_t allocp, need = num;
	struct skmem_obj *__single plist, *__single blist = NULL;
	int err;

	ASSERT(pktq != NULL && num > 0);
	ASSERT(pp->pp_md_type == NEXUS_META_TYPE_PACKET);
	ASSERT(bufcnt <= pp->pp_max_frags);
	ASSERT(PP_BATCH_CAPABLE(pp));

	/* allocate (constructed) packet(s) with buffer(s) attached */
	allocp = skmem_cache_batch_alloc(pp->pp_kmd_cache, &plist,
	    pp->pp_kmd_cache->skm_objsize, num, skmflag);

	/* allocate (constructed) buflet(s) with buffer(s) attached */
	if (PP_HAS_BUFFER_ON_DEMAND(pp) && bufcnt != 0 && allocp != 0) {
		(void) skmem_cache_batch_alloc(PP_KBFT_CACHE_DEF(pp), &blist,
		    PP_KBFT_CACHE_DEF(pp)->skm_objsize, (allocp * bufcnt), skmflag);
	}

	while (plist != NULL) {
		struct skmem_obj *plistn;

		plistn = plist->mo_next;
		plist->mo_next = NULL;

		mdp = (struct __metadata_preamble *)(void *)plist;
		kpkt = (struct __kern_packet *)pp_metadata_init(mdp, pp,
		    bufcnt, skmflag, &blist);
		if (kpkt == NULL) {
			if (blist != NULL) {
				skmem_cache_batch_free(PP_KBFT_CACHE_DEF(pp),
				    blist);
				blist = NULL;
			}
			plist->mo_next = plistn;
			skmem_cache_batch_free(pp->pp_kmd_cache, plist);
			plist = NULL;
			break;
		}

#if CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT)
		/* Checking to ensure the object address is tagged */
		ASSERT((vm_offset_t)kpkt !=
		    vm_memtag_canonicalize_address((vm_offset_t)kpkt));
#endif /* CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */

		KPKTQ_ENQUEUE(pktq, kpkt);

		if (cb != NULL) {
			(cb)((uint64_t)kpkt, (num - need), ctx);
		}

		plist = plistn;

		ASSERT(need > 0);
		--need;
	}
	ASSERT(blist == NULL);
	ASSERT((num - need) == allocp || kpkt == NULL);

	if (__probable(need == 0)) {
		err = 0;
	} else if (need == num) {
		err = ENOMEM;
	} else {
		err = EAGAIN;
	}

	return err;
}

uint64_t
pp_alloc_packet_by_size(struct kern_pbufpool *pp, uint32_t size,
    uint32_t skmflag)
{
	uint32_t bufcnt = pp->pp_max_frags;
	uint64_t kpkt = 0;

	if (PP_HAS_BUFFER_ON_DEMAND(pp)) {
		bufcnt =
		    SK_ROUNDUP(size, PP_BUF_SIZE_DEF(pp)) / PP_BUF_SIZE_DEF(pp);
		ASSERT(bufcnt <= UINT16_MAX);
	}

	(void) pp_alloc_packet_common(pp, (uint16_t)bufcnt, &kpkt, 1, TRUE,
	    NULL, NULL, skmflag);

	return kpkt;
}

__attribute__((always_inline))
static inline struct __metadata_preamble *
pp_metadata_fini(struct __kern_quantum *kqum, struct kern_pbufpool *pp,
    struct mbuf **mp, struct __kern_packet **kpp, struct skmem_obj **blist_def,
    struct skmem_obj **blist_nocache_def, struct skmem_obj **blist_large,
    struct skmem_obj **blist_nocahce_large)
{
	struct __metadata_preamble *mdp = METADATA_PREAMBLE(kqum);

	ASSERT(SK_PTR_TAG(kqum) == 0);

	switch (pp->pp_md_type) {
	case NEXUS_META_TYPE_PACKET: {
		struct __kern_packet *kpkt = SK_PTR_KPKT(kqum);

		if ((kpkt->pkt_pflags & PKT_F_TX_COMPL_TS_REQ) != 0) {
			__packet_perform_tx_completion_callbacks(
				SK_PKT2PH(kpkt), NULL);
		}
		if ((kpkt->pkt_pflags & PKT_F_MBUF_DATA) != 0) {
			ASSERT((kpkt->pkt_pflags & PKT_F_PKT_DATA) == 0);
			ASSERT(kpkt->pkt_mbuf != NULL);
			ASSERT(kpkt->pkt_mbuf->m_nextpkt == NULL);
			if (mp != NULL) {
				ASSERT(*mp == NULL);
				*mp = kpkt->pkt_mbuf;
			} else {
				m_freem(kpkt->pkt_mbuf);
			}
			KPKT_CLEAR_MBUF_DATA(kpkt);
		} else if ((kpkt->pkt_pflags & PKT_F_PKT_DATA) != 0) {
			ASSERT(kpkt->pkt_pkt != NULL);
			ASSERT(kpkt->pkt_pkt->pkt_nextpkt == NULL);
			if (kpp != NULL) {
				ASSERT(*kpp == NULL);
				*kpp = kpkt->pkt_pkt;
			} else {
				/* can only recurse once */
				ASSERT((kpkt->pkt_pkt->pkt_pflags &
				    PKT_F_PKT_DATA) == 0);
				pp_free_packet_single(kpkt->pkt_pkt);
			}
			KPKT_CLEAR_PKT_DATA(kpkt);
		}
		kpkt->pkt_pflags &= ~PKT_F_TRUNCATED;
		ASSERT(kpkt->pkt_nextpkt == NULL);
		ASSERT(kpkt->pkt_qum.qum_ksd == NULL);
		ASSERT((kpkt->pkt_pflags & PKT_F_MBUF_MASK) == 0);
		ASSERT((kpkt->pkt_pflags & PKT_F_PKT_MASK) == 0);
		break;
	}
	default:
		break;
	}

	if (__improbable(PP_HAS_BUFFER_ON_DEMAND(pp))) {
		pp_metadata_destruct_common(kqum, pp, FALSE, blist_def, blist_nocache_def,
		    blist_large, blist_nocahce_large);
	}
	return mdp;
}

void
pp_free_packet_chain(struct __kern_packet *pkt_chain, int *npkt)
{
	struct __metadata_preamble *mdp;
	struct skmem_obj *__single obj_mdp;
	struct skmem_obj *__single top = NULL;
	struct skmem_obj *__single blist_def = NULL, *__single blist_nocache_def = NULL;
	struct skmem_obj *__single blist_large = NULL, *__single blist_nocache_large = NULL;
	struct skmem_obj **list = &top;
	struct mbuf *__single mtop = NULL;
	struct mbuf **mp = &mtop;
	struct __kern_packet *__single kptop = NULL;
	struct __kern_packet **__single kpp = &kptop, *pkt, *next;
	struct kern_pbufpool *pp;
	int c = 0;

	pp = __DECONST(struct kern_pbufpool *, pkt_chain->pkt_qum.qum_pp);
	ASSERT(pp != NULL);
	ASSERT(PP_BATCH_CAPABLE(pp));

	for (pkt = pkt_chain; pkt != NULL; pkt = next) {
		next = pkt->pkt_nextpkt;
		pkt->pkt_nextpkt = NULL;

		ASSERT(SK_PTR_ADDR_KQUM(pkt)->qum_pp == pp);
		mdp = pp_metadata_fini(SK_PTR_ADDR_KQUM(pkt), pp,
		    mp, kpp, &blist_def, &blist_nocache_def, &blist_large, &blist_nocache_large);

		obj_mdp = __unsafe_forge_single(struct skmem_obj *, mdp);
		*list = obj_mdp;
		list = &(*list)->mo_next;
		c++;

		if (*mp != NULL) {
			mp = &(*mp)->m_nextpkt;
			ASSERT(*mp == NULL);
		}
		if (*kpp != NULL) {
			kpp = &(*kpp)->pkt_nextpkt;
			ASSERT(*kpp == NULL);
		}
	}

	ASSERT(top != NULL);
	skmem_cache_batch_free(pp->pp_kmd_cache, top);
	pp_free_kbft_list(pp, blist_def, blist_nocache_def, blist_large, blist_nocache_large);
	if (mtop != NULL) {
		DTRACE_SKYWALK(free__attached__mbuf);
		if (__probable(mtop->m_nextpkt != NULL)) {
			m_freem_list(mtop);
		} else {
			m_freem(mtop);
		}
	}
	if (kptop != NULL) {
		int cnt = 0;
		pp_free_packet_chain(kptop, &cnt);
		DTRACE_SKYWALK1(free__attached__pkt, int, cnt);
	}
	if (npkt != NULL) {
		*npkt = c;
	}
}

void
pp_free_pktq(struct pktq *pktq)
{
	if (__improbable(KPKTQ_EMPTY(pktq))) {
		return;
	}
	struct __kern_packet *pkt = KPKTQ_FIRST(pktq);
	pp_free_packet_chain(pkt, NULL);
	KPKTQ_DISPOSE(pktq);
}

void
pp_drop_pktq(struct pktq *pktq, struct ifnet *ifp, uint16_t flags,
    drop_reason_t reason, const char *funcname, uint16_t linenum)
{
	drop_func_t dropfunc;
	struct __kern_packet *kpkt;

	if (KPKTQ_EMPTY(pktq)) {
		return;
	}
	if (__probable(droptap_total_tap_count == 0)) {
		goto nodroptap;
	}

	if (flags & DROPTAP_FLAG_DIR_OUT) {
		dropfunc = droptap_output_packet;
	} else if (flags & DROPTAP_FLAG_DIR_IN) {
		dropfunc = droptap_input_packet;
	} else {
		goto nodroptap;
	}

	KPKTQ_FOREACH(kpkt, pktq) {
		dropfunc(SK_PKT2PH(kpkt), reason, funcname, linenum, flags, ifp,
		    kpkt->pkt_qum.qum_pid, NULL, -1, NULL, 0, 0);
	}

nodroptap:
	pp_free_pktq(pktq);
}

__attribute__((always_inline))
static inline void
pp_free_packet_array(struct kern_pbufpool *pp, uint64_t *__counted_by(num)array, uint32_t num)
{
	struct __metadata_preamble *mdp;
	struct skmem_obj *__single obj_mdp = NULL;
	struct skmem_obj *__single top = NULL;
	struct skmem_obj *__single blist_def = NULL, *__single blist_nocache_def = NULL;
	struct skmem_obj *__single blist_large = NULL, *__single blist_nocache_large = NULL;
	struct skmem_obj **list = &top;
	struct mbuf *__single mtop = NULL;
	struct mbuf **mp = &mtop;
	struct __kern_packet *__single kptop = NULL;
	struct __kern_packet **kpp = &kptop;
	uint32_t i;

	ASSERT(pp != NULL);
	ASSERT(array != NULL && num > 0);
	ASSERT(PP_BATCH_CAPABLE(pp));

	for (i = 0; i < num; i++) {
		ASSERT(SK_PTR_ADDR_KQUM(array[i])->qum_pp == pp);
		mdp = pp_metadata_fini(SK_PTR_ADDR_KQUM(array[i]), pp,
		    mp, kpp, &blist_def, &blist_nocache_def, &blist_large, &blist_nocache_large);

		obj_mdp = __unsafe_forge_single(struct skmem_obj *, mdp);
		*list = obj_mdp;
		list = &(*list)->mo_next;
		array[i] = 0;

		if (*mp != NULL) {
			mp = &(*mp)->m_nextpkt;
			ASSERT(*mp == NULL);
		}
		if (*kpp != NULL) {
			kpp = &(*kpp)->pkt_nextpkt;
			ASSERT(*kpp == NULL);
		}
	}

	ASSERT(top != NULL);
	skmem_cache_batch_free(pp->pp_kmd_cache, top);
	pp_free_kbft_list(pp, blist_def, blist_nocache_def, blist_large, blist_nocache_large);
	if (mtop != NULL) {
		DTRACE_SKYWALK(free__attached__mbuf);
		if (__probable(mtop->m_nextpkt != NULL)) {
			m_freem_list(mtop);
		} else {
			m_freem(mtop);
		}
	}
	if (kptop != NULL) {
		int cnt = 0;
		pp_free_packet_chain(kptop, &cnt);
		DTRACE_SKYWALK1(free__attached__pkt, int, cnt);
	}
}

void
pp_free_packet(struct kern_pbufpool *pp, uint64_t kqum)
{
	pp_free_packet_array(pp, &kqum, 1);
}

void
pp_free_packet_batch(const kern_pbufpool_t pp, uint64_t *__counted_by(size)array, uint32_t size)
{
	pp_free_packet_array(pp, array, size);
}

void
pp_free_packet_single(struct __kern_packet *pkt)
{
	ASSERT(pkt->pkt_nextpkt == NULL);
	pp_free_packet(__DECONST(struct kern_pbufpool *,
	    pkt->pkt_qum.qum_pp), SK_PTR_ADDR(pkt));
}

void
pp_drop_packet_single(struct __kern_packet *pkt, struct ifnet *ifp, uint16_t flags,
    drop_reason_t reason, const char *funcname, uint16_t linenum)
{
	drop_func_t dropfunc;

	if (pkt->pkt_length == 0) {
		return;
	}
	if (__probable(droptap_total_tap_count == 0)) {
		goto nodroptap;
	}

	if (flags & DROPTAP_FLAG_DIR_OUT) {
		dropfunc = droptap_output_packet;
	} else if (flags & DROPTAP_FLAG_DIR_IN) {
		dropfunc = droptap_input_packet;
	} else {
		goto nodroptap;
	}

	dropfunc(SK_PKT2PH(pkt), reason, funcname, linenum, flags, ifp,
	    pkt->pkt_qum.qum_pid, NULL, -1, NULL, 0, 0);

nodroptap:
	pp_free_packet_single(pkt);
}

static mach_vm_address_t
pp_alloc_buffer_common(const kern_pbufpool_t pp, struct skmem_obj_info *oi,
    uint32_t skmflag, bool large)
{
	/*
	 * XXX -fbounds-safety: We can't change this mach_vm_address_t to some
	 * other (safe) pointer type, because IOSkywalkFamily depends on this
	 * being mach_vm_address_t
	 */
	mach_vm_address_t baddr;
	struct skmem_cache *skm = large ? PP_BUF_CACHE_LARGE(pp):
	    PP_BUF_CACHE_DEF(pp);

	ASSERT(skm != NULL);
	/* allocate a cached buffer */
	baddr = (mach_vm_address_t)skmem_cache_alloc(skm, skmflag);

#if (DEVELOPMENT || DEBUG)
	uint64_t mtbf = skmem_region_get_mtbf();
	/*
	 * MTBF is applicable only for non-blocking allocations here.
	 */
	if (__improbable(mtbf != 0 && (net_uptime_ms() % mtbf) == 0 &&
	    (skmflag & SKMEM_NOSLEEP))) {
		SK_ERR("pp \"%s\" MTBF failure", pp->pp_name);
		net_update_uptime();
		if (baddr != 0) {
			skmem_cache_free(skm,
			    __unsafe_forge_single(struct skmem_obj *, baddr));
			baddr = 0;
		}
	}
#endif /* (DEVELOPMENT || DEBUG) */

	if (__improbable(baddr == 0)) {
		SK_DF(SK_VERB_MEM, "failed to alloc buffer, pp 0x%llx",
		    SK_KVA(pp));
		return 0;
	}
	skmem_cache_get_obj_info(skm,
	    __unsafe_forge_single(struct skmem_obj *, baddr), oi, NULL);
	ASSERT(SKMEM_OBJ_BUFCTL(oi) != NULL);
	ASSERT((mach_vm_address_t)SKMEM_OBJ_ADDR(oi) == baddr);
	return baddr;
}

errno_t
pp_alloc_buffer(const kern_pbufpool_t pp, mach_vm_address_t *baddr,
    kern_segment_t *seg, kern_obj_idx_seg_t *idx, uint32_t skmflag)
{
	struct skmem_obj_info oib;

	VERIFY(pp != NULL && baddr != NULL);
	VERIFY((seg != NULL) == (idx != NULL));

	if (__improbable(!PP_HAS_BUFFER_ON_DEMAND(pp))) {
		return ENOTSUP;
	}

	*baddr = pp_alloc_buffer_common(pp, &oib, skmflag, false);
	if (__improbable(*baddr == 0)) {
		return ENOMEM;
	}

	if (seg != NULL) {
		ASSERT(SKMEM_OBJ_SEG(&oib) != NULL);
		*seg = SKMEM_OBJ_SEG(&oib);
		*idx = SKMEM_OBJ_IDX_SEG(&oib);
	}
	return 0;
}

void
pp_free_buffer(const kern_pbufpool_t pp, mach_vm_address_t addr)
{
	ASSERT(pp != NULL && addr != 0);
	skmem_cache_free(PP_BUF_CACHE_DEF(pp), __unsafe_forge_single(
		    struct skmem_obj *, addr));
}

__attribute__((always_inline))
static inline uint32_t
pp_alloc_buflet_common(struct kern_pbufpool *pp,
    uint64_t *__counted_by(num)array, uint32_t num, uint32_t skmflag,
    bool large)
{
	struct __kern_buflet *kbft = NULL;
	uint32_t allocd, need = num;
	struct skmem_obj *__single list;
	uint64_t *array_cp;  /* -fbounds-safety */

	ASSERT(array != NULL && num > 0);
	ASSERT(PP_BATCH_CAPABLE(pp));
	ASSERT(PP_KBFT_CACHE_DEF(pp) != NULL);
	ASSERT(PP_BUF_SIZE_LARGE(pp) != 0 || !large);

	if (large) {
		allocd = skmem_cache_batch_alloc(PP_KBFT_CACHE_LARGE(pp), &list,
		    PP_KBFT_CACHE_LARGE(pp)->skm_objsize, num, skmflag);
	} else {
		allocd = skmem_cache_batch_alloc(PP_KBFT_CACHE_DEF(pp), &list,
		    PP_KBFT_CACHE_DEF(pp)->skm_objsize, num, skmflag);
	}

	array_cp = array;
	while (list != NULL) {
		struct skmem_obj *listn;

		listn = list->mo_next;
		list->mo_next = NULL;
		kbft = (kern_buflet_t)(void *)list;

#if CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT)
		/* Checking to ensure the object address is tagged */
		ASSERT((vm_offset_t)kbft !=
		    vm_memtag_canonicalize_address((vm_offset_t)kbft));
#endif /* CONFIG_KERNEL_TAGGING && !defined(KASAN_LIGHT) */

		KBUF_EXT_INIT(kbft, pp);
		*array_cp = (uint64_t)kbft;
		++array_cp;
		list = listn;
		ASSERT(need > 0);
		--need;
	}
	ASSERT((num - need) == allocd || kbft == NULL);
	return num - need;
}

errno_t
pp_alloc_buflet(struct kern_pbufpool *pp, kern_buflet_t *kbft, uint32_t skmflag,
    bool large)
{
	uint64_t bft;

	if (__improbable(!pp_alloc_buflet_common(pp, &bft, 1, skmflag, large))) {
		return ENOMEM;
	}
	*kbft = __unsafe_forge_single(kern_buflet_t, bft);
	return 0;
}

errno_t
pp_alloc_buflet_batch(struct kern_pbufpool *pp,
    uint64_t *__counted_by(*size)array, uint32_t *size, uint32_t skmflag,
    bool large)
{
	uint32_t i, n;
	int err;

	ASSERT(array != NULL && size > 0);

	n = *size;
	i = pp_alloc_buflet_common(pp, array, n, skmflag, large);
	*size = i;

	if (__probable(i == n)) {
		err = 0;
	} else if (i != 0) {
		err = EAGAIN;
	} else {
		err = ENOMEM;
	}

	return err;
}

__attribute__((always_inline))
static void
pp_free_buflet_common(const kern_pbufpool_t pp, kern_buflet_t kbft)
{
	ASSERT(kbft->buf_nbft_idx == OBJ_IDX_NONE);
	ASSERT(kbft->buf_nbft_addr == 0);

	if (kbft->buf_flag & BUFLET_FLAG_EXTERNAL) {
		ASSERT(kbft->buf_addr != 0);
		ASSERT(kbft->buf_idx != OBJ_IDX_NONE);
		ASSERT(kbft->buf_bft_idx_reg != OBJ_IDX_NONE);
		ASSERT(kbft->buf_ctl != NULL);
		ASSERT(((struct __kern_buflet_ext *)kbft)->
		    kbe_buf_upp_link.sle_next == NULL);
		if (kbft->buf_ctl->bc_usecnt > 1) {
			skmem_cache_free_nocache(BUFLET_HAS_LARGE_BUF(kbft) ?
			    PP_KBFT_CACHE_LARGE(pp) : PP_KBFT_CACHE_DEF(pp),
			    (void *)kbft);
		} else {
			skmem_cache_free(BUFLET_HAS_LARGE_BUF(kbft) ?
			    PP_KBFT_CACHE_LARGE(pp) : PP_KBFT_CACHE_DEF(pp),
			    (void *)kbft);
		}
	} else if (__probable(kbft->buf_addr != 0)) {
		void *objaddr = kbft->buf_objaddr;
		uint32_t usecnt = 0;

		ASSERT(kbft->buf_idx != OBJ_IDX_NONE);
		ASSERT(kbft->buf_ctl != NULL);
		KBUF_DTOR(kbft, usecnt);
		SK_DF(SK_VERB_MEM, "pp 0x%llx buf 0x%llx usecnt %u",
		    SK_KVA(pp), SK_KVA(objaddr), usecnt);
		if (__probable(usecnt == 0)) {
			skmem_cache_free(BUFLET_HAS_LARGE_BUF(kbft) ?
			    PP_BUF_CACHE_LARGE(pp) : PP_BUF_CACHE_DEF(pp),
			    objaddr);
		}
	}
}

void
pp_free_buflet(const kern_pbufpool_t pp, kern_buflet_t kbft)
{
	ASSERT(kbft->buf_flag & BUFLET_FLAG_EXTERNAL);
	ASSERT(pp != NULL && kbft != NULL);
	pp_free_buflet_common(pp, kbft);
}

void
pp_reap_caches(boolean_t purge)
{
	skmem_cache_reap_now(pp_opt_cache, purge);
	skmem_cache_reap_now(pp_flow_cache, purge);
	skmem_cache_reap_now(pp_compl_cache, purge);
}
