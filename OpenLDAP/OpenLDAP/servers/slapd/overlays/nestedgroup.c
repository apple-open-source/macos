
#include "portable.h"

#ifdef SLAPD_OVER_NESTEDGROUP

#include <stdio.h>

#include <ac/string.h>
#include <ac/socket.h>

#include "slap.h"
#include "overlayutils.h"

typedef struct adpair {
	struct adpair *ap_next;
	AttributeDescription *memberAttr;
	AttributeDescription *expandAttr;
	AttributeDescription *uuidAttr;
} adpair;

typedef struct nestedgroup_id_to_dn_t {
	int				found;
	struct berval	target_dn;
} nestedgroup_id_to_dn_t;

typedef struct nestedgroup_ismember_t {
	int				found;
} nestedgroup_ismember_t;

nestedgroup_is_member_cb (
	Operation	*op,
	SlapReply	*rs
)
{
	Debug(LDAP_DEBUG_ANY, "nestedgroup_is_member_cb sr_type[%d] sr_err[%d]\n", rs->sr_type, rs->sr_err, 0);
	nestedgroup_ismember_t *ismember = (nestedgroup_ismember_t *)op->o_callback->sc_private;
	
	if (rs->sr_err == LDAP_COMPARE_TRUE)
		ismember->found = 1;
	else
		ismember->found = 0;
	
	return 0;
}


static int __attribute__ ((noinline))
nestedgroup_is_member (
	Operation	*op,
	struct berval* groupDN,
	AttributeDescription *searchAttr,
	struct berval* searchID,
	int * result)
{
	Operation nop = *op;
	AttributeAssertion	ava = { NULL, BER_BVNULL };
	SlapReply 		sreply = {REP_RESULT};
	slap_callback cb = { NULL, nestedgroup_is_member_cb, NULL, NULL };
	BackendDB	*target_bd = NULL;
	nestedgroup_ismember_t ismember =  {0};
	int rc = 0;
	
	target_bd = select_backend(&op->o_req_ndn, 0);
	
	if (!target_bd || !target_bd->be_compare)
		return LDAP_NOT_SUPPORTED;
	
	sreply.sr_entry = NULL;
	sreply.sr_nentries = 0;
	
	nop.orc_ava = &ava;
	nop.orc_ava->aa_desc = searchAttr;
	nop.orc_ava->aa_value = *searchID;
	
	nop.o_tag = LDAP_REQ_COMPARE;
	nop.o_protocol = LDAP_VERSION3;
	nop.o_callback = &cb;
	nop.o_time = slap_get_time();
	nop.o_do_not_cache = 0;
	nop.o_dn = target_bd->be_rootdn;
	nop.o_ndn = target_bd->be_rootndn;
	nop.o_bd = target_bd;

	nop.o_req_dn = *groupDN;
	nop.o_req_ndn = *groupDN;
	cb.sc_private = &ismember;

 	rc = nop.o_bd->be_compare( &nop, &sreply );
	Debug(LDAP_DEBUG_ANY, "nestedgroup_is_member be_compare[%d] ismember[%d]\n", rc, ismember.found, 0);
	if (ismember.found)
		*result = 1;
	else
		*result = 0;
	return 0;
}

static int
nestedgroup_id_to_dn_cb (
	Operation	*op,
	SlapReply	*rs
)
{
	Debug(LDAP_DEBUG_ANY, "nestedgroup_id_to_dn_cb sr_type[%d]\n", rs->sr_type, 0, 0);
//dump_slap_entry(rs->sr_entry);
// && rs->sr_un.sru_search.r_entry->e_nname
	nestedgroup_id_to_dn_t *object_dn = (nestedgroup_id_to_dn_t *)op->o_callback->sc_private;
	
	if ( rs->sr_type == REP_SEARCH ) {
		if (rs->sr_un.sru_search.r_entry) {
			Debug( LDAP_DEBUG_ACL, "nestedgroup_id_to_dn_cb len[%d] [%s]\n", rs->sr_entry->e_nname.bv_len, rs->sr_entry->e_nname.bv_val, 0);	
			ber_dupbv_x( &object_dn->target_dn, &(rs->sr_entry->e_nname), op->o_tmpmemctx);
			object_dn->found = 1;
		}
	} else {
		assert( rs->sr_type == REP_RESULT );
	}

	return 0;
}

static int __attribute__ ((noinline))
nestedgroup_id_to_dn (
	Operation	*op,
	AttributeDescription *searchAttr,
	struct berval* searchID,
	struct berval* resultDN)
{
	Operation nop = *op;
	char			filter_str[128];
	AttributeAssertion	ava = { NULL, BER_BVNULL };
	Filter			filter = {LDAP_FILTER_EQUALITY};
	SlapReply 		sreply = {REP_RESULT};
	slap_callback cb = { NULL, nestedgroup_id_to_dn_cb, NULL, NULL };
	BackendDB	*target_bd = NULL;
	nestedgroup_id_to_dn_t dn_result =  {0};
	int rc = 0;
	
	target_bd = select_backend(&op->o_req_ndn, 0);
	
	if (!target_bd || !target_bd->be_search)
		return LDAP_NOT_SUPPORTED;
	
	sreply.sr_entry = NULL;
	sreply.sr_nentries = 0;
	nop.ors_filterstr.bv_len = snprintf(filter_str, sizeof(filter_str),
		"(%s=%s)",searchAttr->ad_cname.bv_val, searchID->bv_val);
	filter.f_ava = &ava;
	filter.f_av_desc = searchAttr;
	filter.f_av_value = *searchID;

	nop.o_tag = LDAP_REQ_SEARCH;
	nop.o_protocol = LDAP_VERSION3;
	nop.o_callback = &cb;
	nop.o_time = slap_get_time();
	nop.o_do_not_cache = 0;
	nop.o_dn = target_bd->be_rootdn;
	nop.o_ndn = target_bd->be_rootndn;
	nop.o_bd = target_bd;

	nop.o_req_dn = target_bd->be_suffix[0];
	nop.o_req_ndn = target_bd->be_nsuffix[0];
	nop.ors_scope = LDAP_SCOPE_SUBTREE;
	nop.ors_deref = LDAP_DEREF_NEVER;
	nop.ors_slimit = SLAP_NO_LIMIT;
	nop.ors_tlimit = SLAP_NO_LIMIT;
	nop.ors_filter = &filter;
	nop.ors_filterstr.bv_val = filter_str;
	nop.ors_filterstr.bv_len = strlen(filter_str);
	nop.ors_attrs = NULL;
	nop.ors_attrsonly = 0;

	cb.sc_private = &dn_result;

	rc = nop.o_bd->be_search( &nop, &sreply );
	Debug(LDAP_DEBUG_ANY, "nestedgroup_id_to_dn be_search[%d]\n", rc, 0, 0);
	if (dn_result.found)
		ber_dupbv(resultDN, &dn_result.target_dn );
	return 0;
}

static int __attribute__ ((noinline))
nestedgroup_getgroup(Operation *op, adpair *ap, int *isMember )
{
	int rc = LDAP_COMPARE_FALSE;
	Entry *e = NULL;	
	Attribute *members = NULL;
	BackendDB	*be = op->o_bd;
	int i;
	
	if (!op || !ap || !isMember) {
		goto cleanup;
	};
	
	*isMember = 0;
	op->o_bd = select_backend( &op->o_req_ndn, 0);
	
	if (op->o_bd) {
		rc = be_entry_get_rw( op, &op->o_req_ndn, NULL, ap->expandAttr, 0, &e );
		if ( e != NULL ) {
			dump_slap_entry(e);
			members = attr_find(e->e_attrs, ap->expandAttr );
			if (members) {
				dump_slap_attr(members);
				// is the target a direct relative? applies to groups (isgroupcheck?)
				rc = value_find_ex( ap->expandAttr,
					SLAP_MR_ATTRIBUTE_VALUE_NORMALIZED_MATCH |
					SLAP_MR_ASSERTED_VALUE_NORMALIZED_MATCH,
					members->a_nvals, &op->oq_compare.rs_ava->aa_value,
					op->o_tmpmemctx );
				if (rc != LDAP_SUCCESS) {
					for ( i = 0 ; members->a_nvals[i].bv_val; i++) {
						dump_berval( &(members->a_nvals[i]) );
						struct berval member_dn = BER_BVNULL;
						
						nestedgroup_id_to_dn (op, ap->uuidAttr, &(members->a_nvals[i]), &member_dn);
						if (!BER_BVISNULL(&member_dn)) {
							*isMember = 0;
							nestedgroup_is_member (op, &member_dn, op->oq_compare.rs_ava->aa_desc, &op->oq_compare.rs_ava->aa_value, isMember);
							if (*isMember) {
								rc = LDAP_COMPARE_TRUE;
								break;
							} else {
								rc = LDAP_COMPARE_FALSE;
							}
						}

					}
				} else {
					*isMember = 1;
					rc = LDAP_COMPARE_TRUE;
					Debug(LDAP_DEBUG_ANY, "=> nestedgroup_getgroup  FOUND member\n", rc, 0, 0);
				}
			}			
			be_entry_release_rw( op, e, 0 );
		}
	}
	op->o_bd = be;
	
	Debug(LDAP_DEBUG_ANY, "=> nestedgroup_getgroup result (%ld)\n", rc, 0, 0);
	

cleanup:
	return rc;
}

static int
nestedgroup_response( Operation *op, SlapReply *rs )
{
	slap_overinst *on = (slap_overinst *) op->o_bd->bd_info;
	adpair *ap = on->on_bi.bi_private;

	/* If we've been configured and the current response is
	 * what we're looking for...
	 */
	if ( ap && op->o_tag == LDAP_REQ_COMPARE &&
		(rs->sr_err == LDAP_NO_SUCH_ATTRIBUTE || rs->sr_err == LDAP_COMPARE_FALSE )) {

		for (;ap;ap=ap->ap_next) {
			if ( op->oq_compare.rs_ava->aa_desc == ap->memberAttr ) {
				int isMember = 0;
				nestedgroup_getgroup(op, ap, &isMember);
				if (isMember)
					rs->sr_err = LDAP_COMPARE_TRUE;
				else 
					rs->sr_err = LDAP_COMPARE_FALSE;
			}
		}
	}
	/* Default is to just fall through to the normal processing */
	return SLAP_CB_CONTINUE;
}

static int nestedgroup_config(
    BackendDB	*be,
    const char	*fname,
    int		lineno,
    int		argc,
    char	**argv
)
{
	slap_overinst *on = (slap_overinst *) be->bd_info;
	adpair ap = { NULL, NULL, NULL }, *a2;



	if ( strcasecmp( argv[0], "nestedgroup-expandattribute" ) == 0 ) {
		const char *text;
		if ( argc != 3 ) {
			Debug( LDAP_DEBUG_ANY, "%s: line %d: "
				"attribute description missing in "
				"\"nestedgroup-expandattribute <member-attribute> <expanded-attribute>\" line.\n",
				fname, lineno, 0 );
	    	return( 1 );
		}
		if ( slap_str2ad( argv[1], &ap.memberAttr, &text ) ) {
			Debug( LDAP_DEBUG_ANY, "%s: line %d: "
				"attribute description unknown \"nestedgroup-expandattribute\" line: %s.\n",
				fname, lineno, text );
			return( 1 );
		}
		if ( slap_str2ad( argv[2], &ap.expandAttr, &text ) ) {
			Debug( LDAP_DEBUG_ANY, "%s: line %d: "
				"attribute description unknown \"nestedgroup-expandattribute\" line: %s.\n",
				fname, lineno, text );
			return( 1 );
		}

		if ( slap_str2ad( "apple-generateduid", &ap.uuidAttr, &text ) ) {
			Debug( LDAP_DEBUG_ANY,"unable to lookup apple-generateduid [%s]\n", text, 0, 0);
			return( 1 );
		}

		a2 = ch_malloc( sizeof(adpair) );
		a2->ap_next = on->on_bi.bi_private;
		a2->memberAttr = ap.memberAttr;
		a2->expandAttr = ap.expandAttr;
		a2->uuidAttr = ap.uuidAttr;
		on->on_bi.bi_private = a2;
	} else {
		return SLAP_CONF_UNKNOWN;
	}
	return 0;
}

static int
nestedgroup_close(
	BackendDB *be
)
{
	slap_overinst *on = (slap_overinst *) be->bd_info;
	adpair *ap, *a2;

	for ( ap = on->on_bi.bi_private; ap; ap = a2 ) {
		a2 = ap->ap_next;
		ch_free( ap );
	}
	return 0;
}

static slap_overinst nestedgroup;

/* This overlay is set up for dynamic loading via moduleload. For static
 * configuration, you'll need to arrange for the slap_overinst to be
 * initialized and registered by some other function inside slapd.
 */

enum {
	NG_EXPANDATTRIBUTE = 1,
	NG_LAST
};

static ConfigDriver	ng_cfgen;

static ConfigTable ngcfg[] = {
	{ "nestedgroup-expandattribute", "targetAttribute> <nestedAttribute",
		2, 3, 0, ARG_MAGIC|NG_EXPANDATTRIBUTE, ng_cfgen,
				"( OLcfgOvAt:701.1 NAME 'olcExpandAttribute' "
			"DESC 'Nested Group configuration: <target attributeDescription>, <nested attributeDescription>' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString )",
			NULL, NULL },
	{ NULL, NULL, 0, 0, 0, ARG_IGNORED }
};

static ConfigOCs ngocs[] = {
	{ "( OLcfgOvOc:701.1 "
		"NAME 'olcNestedGroup' "
		"DESC 'Nested Group configuration' "
		"SUP olcOverlayConfig "
		"MAY (olcExpandAttribute) )",
		Cft_Overlay, ngcfg, NULL, NULL },
	{ NULL, 0, NULL }
};

static int
ng_cfgen( ConfigArgs *c )
{
	slap_overinst	*on = (slap_overinst *)c->bi;
	adpair *ap = (adpair *)on->on_bi.bi_private;

	int		rc = 0, i;

	if ( c->op == SLAP_CONFIG_EMIT ) {
		switch( c->type ) {
		case NG_EXPANDATTRIBUTE:
			if ( ap && ap->memberAttr && ap->expandAttr) {
				struct berval bv;
				bv.bv_len = sprintf( c->cr_msg, "%s %s",
					ap->memberAttr->ad_cname.bv_val, ap->expandAttr->ad_cname.bv_val);
				bv.bv_val = c->cr_msg;
				value_add_one( &c->rvalue_vals, &bv );
				Debug(LDAP_DEBUG_CONFIG, "[SLAP_CONFIG_EMIT] nestedgroup-expandattribute target Attribute (%s) expanded Attribute (%s)\n", ap->memberAttr->ad_cname.bv_val, ap->expandAttr->ad_cname.bv_val, 0);
			} else {
				rc = 1;
			}
			break;

		default:
			rc = 1;
			break;
		}

		return rc;

	}

	switch( c->type ) {
		case NG_EXPANDATTRIBUTE: {
			AttributeDescription *targetAD = NULL, *expandedAD = NULL, *uuidAttr = NULL;
			adpair *aplist = (adpair *)on->on_bi.bi_private;
			const char		*text;
			
			rc = slap_str2ad( c->argv[ 1 ], &targetAD, &text );
			if ( rc != LDAP_SUCCESS ) {
				snprintf( c->cr_msg, sizeof( c->cr_msg ),
					"\"nestedgroup-expandattribute [<target attrbute>] [<expanded attribute>]\": "
					"unable to find AttributeDescription \"%s\"",
					c->argv[ 1 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->cr_msg, 0 );
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> nestedgroup-expandattribute  target AttributeDescription (%s)\n", targetAD->ad_cname.bv_val, 0, 0);
	
			rc = slap_str2ad( c->argv[ 2 ], &expandedAD, &text );
			if ( rc != LDAP_SUCCESS ) {
				snprintf( c->cr_msg, sizeof( c->cr_msg ),
					"\"nestedgroup-expandattribute [<target attrbute>] [<expanded attribute>]\": "
					"unable to find AttributeDescription \"%s\"",
					c->argv[ 2 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->cr_msg, 0 );
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> nestedgroup-expandattribute  expanded AttributeDescription (%s)\n", expandedAD->ad_cname.bv_val, 0, 0);

			rc = slap_str2ad( "apple-generateduid", &uuidAttr, &text );
			if ( rc != LDAP_SUCCESS ) {
				Debug( LDAP_DEBUG_ANY,"nestedgroup: unable to lookup apple-generateduid [%s]\n", text, 0, 0);
				return( -1 );
			}

			aplist->memberAttr = targetAD;
			aplist->expandAttr = expandedAD;
			aplist->uuidAttr = uuidAttr;
		} break;

		default:
			rc = 1;
			break;
	}

	return rc;
}


int nestedgroup_initialize() {
	int rc = -1;

	adpair	*ad;

	ad = ch_calloc(1, sizeof(adpair));
	nestedgroup.on_bi.bi_private = ad;
	
	nestedgroup.on_bi.bi_type = "nestedgroup";
	nestedgroup.on_bi.bi_db_close = nestedgroup_close;
	nestedgroup.on_response = nestedgroup_response;
	nestedgroup.on_bi.bi_db_config = config_generic_wrapper;
	nestedgroup.on_bi.bi_cf_ocs = ngocs;

	rc = config_register_schema( ngcfg, ngocs );
	if ( rc ) {
		return rc;
	}

	return overlay_register( &nestedgroup );
}

#if SLAPD_OVER_NESTEDGROUP == SLAPD_MOD_DYNAMIC
int
init_module( int argc, char *argv[] )
{
	return nestedgroup_initialize();
}
#endif

#endif /* defined(SLAPD_OVER_NESTEDGROUP) */
