/* dynid.c  */
/* $OpenLDAP: pkg/ldap/servers/slapd/overlays/dynid.c,v 1.1.2.4 2006/06/20 17:28:43 sjones Exp $ */
/* This work is part of OpenLDAP Software <http://www.openldap.org/>.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted only as authorized by the OpenLDAP
 * Public License.
 *
 * A copy of this license is available in the file LICENSE in the
 * top-level directory of the distribution or, alternatively, at
 * <http://www.OpenLDAP.org/license.html>.
 */

#include "portable.h"

#ifdef SLAPD_OVER_DYNID
#include "overlayutils.h"

#include <stdio.h>
#include <uuid/uuid.h>

#include <ac/string.h>
#include <ac/ctype.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"

#define DYNID_BACK_CONFIG 1
static slap_overinst dynid;

static AttributeDescription	*dynid_memberships = NULL;
static AttributeDescription	*dynid_uuid = NULL;

typedef struct dynid_objmap {
	AttributeDescription *owneruuidAttr;
	AttributeDescription *uuidAttr;
	AttributeDescription *idAttr;
	unsigned long max;
	unsigned long min;
	unsigned long lastid;
	int generateuuid;
	struct berval	override_dn;
	BerValue 		target_dn;
	struct dynid_objmap *next;
} dynid_objmap;

typedef struct dynid_data {
	struct dynid_objmap *map;
} dynid_data;

typedef struct dynid_unique_counter_s {
	struct berval *ndn;
	int count;
} dynid_unique_counter;

typedef struct dynid_ismember_t {
	int				found;
} dynid_ismember_t;

static int dynid_count_attr_cb(
	Operation *op,
	SlapReply *rs
)
{
	dynid_unique_counter *uc;

	/* because you never know */
	if(!op || !rs) return(0);

	/* Only search entries are interesting */
	if(rs->sr_type != REP_SEARCH) return(0);

	uc = op->o_callback->sc_private;

	/* Ignore the current entry */
	if ( dn_match( uc->ndn, &rs->sr_entry->e_nname )) return(0);

	Debug(LDAP_DEBUG_TRACE, "==> count_attr_cb <%s>\n",
		rs->sr_entry ? rs->sr_entry->e_name.bv_val : "UNKNOWN_DN", 0, 0);

	uc->count++;

	return(0);
}

static int dynid_search(
	Operation *op,
	Operation *nop,
	char *key,
	struct berval *searchbase
)
{
	slap_overinst *on = (slap_overinst *) op->o_bd->bd_info;
//	unique_data *ud = on->on_bi.bi_private;
	SlapReply nrs = { REP_RESULT };
	slap_callback cb = { NULL, NULL, NULL, NULL }; /* XXX */
	dynid_unique_counter uq = { NULL, 0 };
	int rc;

	nop->ors_filter = str2filter_x(nop, key);
	ber_str2bv(key, 0, 0, &nop->ors_filterstr);

	cb.sc_response	= (slap_response*)dynid_count_attr_cb;
	cb.sc_private	= &uq;
	nop->o_callback	= &cb;
	nop->o_tag	= LDAP_REQ_SEARCH;
	nop->ors_scope	= LDAP_SCOPE_SUBTREE;
	nop->ors_deref	= LDAP_DEREF_NEVER;
	nop->ors_limit	= NULL;
	nop->ors_slimit	= SLAP_NO_LIMIT;
	nop->ors_tlimit	= SLAP_NO_LIMIT;
	nop->ors_attrs	= slap_anlist_no_attrs;
	nop->ors_attrsonly = 1;

	uq.ndn = &op->o_req_ndn;

	nop->o_req_dn	= *searchbase;
	nop->o_req_ndn	= *searchbase;
	nop->o_ndn = op->o_bd->be_rootndn;

	nop->o_bd = on->on_info->oi_origdb;
	rc = nop->o_bd->be_search(nop, &nrs);

	if (rc == LDAP_NO_SUCH_OBJECT) {
		Debug(LDAP_DEBUG_TRACE, "=> dynid_search LDAP_NO_SUCH_OBJECT\n", 0, 0, 0);
		return (0);
	}
	if(rc != LDAP_SUCCESS) {
		op->o_bd->bd_info = (BackendInfo *) on->on_info;
		Debug(LDAP_DEBUG_TRACE, "=> dynid_search failed (%ld)\n", rc, 0, 0);
		return(-1);
	}

	Debug(LDAP_DEBUG_TRACE, "=> dynid_search found %d records\n", uq.count, 0, 0);

	if(uq.count) {
		op->o_bd->bd_info = (BackendInfo *) on->on_info;
		Debug(LDAP_DEBUG_TRACE, "=> dynid_search not unique - count(%d)\n", uq.count, 0, 0);
		return(-1);
	}

	return(0);
}

static __attribute__ ((noinline))  int dynid_addownerguid(Operation *op, SlapReply *rs)
{
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	dynid_data *ad = on->on_bi.bi_private;
	dynid_objmap *ddmap = ad->map;
	int rc = -1;
	Entry *e = NULL;
	
	int cache = op->o_do_not_cache;
	struct berval	op_dn = op->o_dn,
			op_ndn = op->o_ndn;
	BackendDB	*op_bd = op->o_bd;
	BackendDB	*target_bd = NULL;
	struct berval *target_ndn = NULL;
	Attribute *owneruuidAttr = NULL;
	
	if (!ddmap->owneruuidAttr || !ddmap->uuidAttr || BER_BVISNULL(&op->o_ndn)) {
		goto cleanup;
	};
	
	target_bd = select_backend( &op->o_req_ndn, 0, 0 );
	target_ndn = &op_ndn; 
	
	if (target_bd) {
		//op->o_do_not_cache = 1;
		op->o_dn = op->o_bd->be_rootdn;
		op->o_ndn = op->o_bd->be_rootndn;
		op->o_bd = target_bd;
		
		if (op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
			Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid CURRENT Authenticated PROXY User\n", 0, 0, 0);
			rc = attr_merge_one( op->oq_add.rs_e, ddmap->owneruuidAttr, &op->o_conn->c_authz.c_sai_krb5_pac_id, NULL );
		} else {
			rc = be_entry_get_rw( op, target_ndn, NULL, ddmap->uuidAttr, 0, &e );
			if ( e != NULL ) {
				dump_slap_entry(e);
	
				owneruuidAttr = attr_find( op->oq_add.rs_e->e_attrs, ddmap->owneruuidAttr );
				if (owneruuidAttr) {
					Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid SUGGESTED Owner\n", 0, 0, 0);
					dump_slap_attr(owneruuidAttr);			
					rc = attr_delete(&(op->oq_add.rs_e->e_attrs), ddmap->owneruuidAttr );
					owneruuidAttr = NULL;
				}			
				owneruuidAttr = attr_find( e->e_attrs, ddmap->uuidAttr  );
				if (owneruuidAttr) {
					Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid CURRENT Authenticated User\n", 0, 0, 0);
					dump_slap_attr(owneruuidAttr);			
	
					rc = attr_merge_one( op->oq_add.rs_e, ddmap->owneruuidAttr, owneruuidAttr->a_vals, NULL );
				}			
				be_entry_release_rw( op, e, 0 );
			}
		}
		op->o_do_not_cache = cache;
		op->o_dn = op_dn;
		op->o_ndn = op_ndn;
		op->o_bd = op_bd;
	}
	if(rc != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid be_entry_get_rw failed (%ld)\n", rc, 0, 0);
		return(rc);
	}
	
cleanup:
	return rc;
}

static __attribute__ ((noinline))  int dynid_addownerguidextended(Operation *op, SlapReply *rs)
{
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	dynid_data *ad = on->on_bi.bi_private;
	dynid_objmap *ddmap = ad->map;
	int rc = -1;
	Entry *e = NULL;
	
	int cache = op->o_do_not_cache;
	struct berval	op_dn = op->o_dn,
			op_ndn = op->o_ndn;
	BackendDB	*op_bd = op->o_bd;
	BackendDB	*target_bd = NULL;
	struct berval *target_ndn = NULL;
	Attribute *owneruuidAttr = NULL;
	
	if (!ddmap->owneruuidAttr || !ddmap->uuidAttr || BER_BVISNULL(&op->o_ndn)) {
		goto cleanup;
	};
	
	target_bd = select_backend( &op->o_req_ndn, 0, 0 );
	target_ndn = &op->o_ndn; 
	
	if (target_bd) {
		rc = be_entry_get_rw( op, target_ndn, NULL, ddmap->uuidAttr, 0, &e );
		if ( e != NULL ) {
			dump_slap_entry(e);

			owneruuidAttr = attr_find( op->oq_add.rs_e->e_attrs, ddmap->owneruuidAttr );
			if (owneruuidAttr) {
				Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid SUGGESTED Owner\n", 0, 0, 0);
				dump_slap_attr(owneruuidAttr);			
				rc = attr_delete(&(op->oq_add.rs_e->e_attrs), ddmap->owneruuidAttr );
				owneruuidAttr = NULL;
			}			
			owneruuidAttr = attr_find( e->e_attrs, ddmap->uuidAttr  );
			if (owneruuidAttr) {
				Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid CURRENT Authenticated User\n", 0, 0, 0);
				dump_slap_attr(owneruuidAttr);			

				rc = attr_merge_one( op->oq_add.rs_e, ddmap->owneruuidAttr, owneruuidAttr->a_vals, NULL );
			}			
			be_entry_release_rw( op, e, 0 );
		}

	}
	if(rc != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_TRACE, "=> dynid_addownerguid be_entry_get_rw failed (%ld)\n", rc, 0, 0);
		return(rc);
	}
	
cleanup:
	return rc;
}

static __attribute__ ((noinline))  int dynid_generate_id(Operation *op)
{
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	dynid_data *ad = on->on_bi.bi_private;
	dynid_objmap *ddmap = ad->map;
	Attribute *idNumber = NULL;
	const char *text = NULL;
	int rc = 0;
	int isUnique = 1;
	Operation nop = *op;
	struct berval bv = {0};
	struct berval searchbase = {0};
	char tmp[150] = {0};
	
	if ( ddmap->lastid < ddmap->max)
	{
		for (ddmap->lastid++; (ddmap->lastid > ddmap->min) && (ddmap->lastid < ddmap->max) && !isUnique; ddmap->lastid++)
		{
			snprintf(tmp, sizeof(tmp),"(%s=%ld)",ddmap->idAttr->ad_cname.bv_val, ddmap->lastid);
		}
	} else {
		Debug( LDAP_DEBUG_TRACE, "#####dynid_generate_id OUT OF IDs max[%d]#####\n", ddmap->lastid, 0, 0);
	}
	
	if (isUnique)
	{
		snprintf(tmp, sizeof(tmp), "%ld", ddmap->lastid);
		Debug( LDAP_DEBUG_TRACE, "#####[NEW ID][%d][%s]#####\n", ddmap->lastid, tmp, 0);
		ber_str2bv( tmp, 0, 0, &bv );
		dump_berval(&bv);					
		rc = attr_delete(&(op->oq_add.rs_e->e_attrs), ddmap->idAttr );
		rc = attr_merge_one( op->oq_add.rs_e, ddmap->idAttr, &bv, NULL );
		
		idNumber = attr_find( op->oq_add.rs_e->e_attrs, ddmap->idAttr );
		if (idNumber)
		{
			dump_slap_attr(idNumber);
		} else {
			Debug( LDAP_DEBUG_TRACE, "#####[idNumber - NOT FOUND]#####\n", 0, 0, 0);
		}
	}
	
	op->o_tag = LDAP_REQ_ADD;
	return rc;
}
static __attribute__ ((noinline))  int dynid_generate_uuid(Operation *op)
{
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	dynid_data *ad = on->on_bi.bi_private;
	dynid_objmap *ddmap = ad->map;
	struct berval bv;
	Attribute  *uuidAttr = NULL;
	const char		*text;
	int rc = -1;
	char uuidStr[512] = {0};
	uuid_t uu;
	
	if (ddmap->uuidAttr)
	{
		uuidAttr = attr_find( op->oq_add.rs_e->e_attrs, ddmap->uuidAttr );
		if (uuidAttr)
		{
			dump_slap_attr(uuidAttr);			
			rc = attr_delete(&(op->oq_add.rs_e->e_attrs), ddmap->uuidAttr );
		}
		
		uuid_generate(uu);
		uuid_unparse(uu,uuidStr);
		ber_str2bv( uuidStr, 0, 0, &bv );
		rc = attr_merge_one( op->oq_add.rs_e, ddmap->uuidAttr, &bv, NULL );
	}

	return rc;
}

dynid_is_member_cb (
	Operation	*op,
	SlapReply	*rs
)
{
	Debug(LDAP_DEBUG_TRACE, "dynid_is_member_cb sr_type[%d] sr_err[%d]\n", rs->sr_type, rs->sr_err, 0);
	dynid_ismember_t *ismember = (dynid_ismember_t *)op->o_callback->sc_private;
	
	if (rs->sr_err == LDAP_COMPARE_TRUE)
		ismember->found = 1;
	else
		ismember->found = 0;
	
	return 0;
}


static int __attribute__ ((noinline))
dynid_is_member (
	Operation	*op,
	struct berval* groupDN,
	AttributeDescription *searchAttr,
	struct berval* searchID,
	int * result)
{
	Operation nop = *op;
	AttributeAssertion	ava = { NULL, BER_BVNULL };
	SlapReply 		sreply = {REP_RESULT};
	slap_callback cb = { NULL, dynid_is_member_cb, NULL, NULL };
	BackendDB	*target_bd = NULL;
	dynid_ismember_t ismember =  {0};
	int rc = 0;
	
	target_bd = select_backend(&op->o_req_ndn, 0 , 0);
	
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
	Debug(LDAP_DEBUG_TRACE, "dynid_is_member be_compare[%d] ismember[%d]\n", rc, ismember.found, 0);
	if (ismember.found)
		*result = 1;
	else
		*result = 0;
	return 0;
}

static int  __attribute__ ((noinline)) dynid_is_match(Operation *op)
{
	Attribute *oc = NULL, *idNumber = NULL;
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	dynid_data *ad = on->on_bi.bi_private;
	dynid_objmap *ddmap = ad->map;
	const char *text = NULL;
	Operation nop = *op;
	Entry *theEntry = NULL;
	int rc = LDAP_COMPARE_FALSE, entry_rc = -1;
	struct berval	op_dn = op->o_dn,
			op_ndn = op->o_ndn;
	BackendDB	*op_bd = op->o_bd;
	BackendDB	*target_bd = NULL;
	struct berval *target_ndn = NULL;
	
	
	if (!BER_BVISNULL(&op->o_req_ndn) && !BER_BVISNULL(&ddmap->target_dn) && dnIsSuffix(&op->o_req_ndn, &ddmap->target_dn)) {
		if (!BER_BVISNULL(&ddmap->override_dn) && !op->o_conn->c_authz.c_sai_krb5_auth_data_provisioned) {
			rc = LDAP_COMPARE_FALSE;
			
			target_bd = select_backend( &op->o_req_ndn, 0, 0 );
			target_ndn = &op_ndn;
			
			if (target_bd && !BER_BVISNULL(&op_ndn)) {
					op->o_dn = op->o_bd->be_rootdn;
					op->o_ndn = op->o_bd->be_rootndn;
					op->o_bd = target_bd;
		
					entry_rc = be_entry_get_rw( op, &op_ndn, NULL , dynid_uuid, 0, &theEntry ); // user uuid
					Debug( LDAP_DEBUG_TRACE, "#####dynid_is_match be_entry_get_rw [%d] [%s]#####\n", entry_rc, op_ndn.bv_val, 0);
					
					if (entry_rc == LDAP_SUCCESS && theEntry) {
						Attribute *uuidAttr = NULL;
						int isMember = 0;
						
						uuidAttr = attr_find( theEntry->e_attrs, dynid_uuid);
						if (uuidAttr) {
							dynid_is_member ( op, &ddmap->override_dn, dynid_memberships, uuidAttr->a_nvals , &isMember);
							if (isMember) { // override
								rc = LDAP_COMPARE_FALSE;
							} else {
								rc = LDAP_COMPARE_TRUE;						
							}
						}
					}
					
					if (theEntry)
						be_entry_release_r(op, theEntry);
			
		
					op->o_dn = op_dn;
					op->o_ndn = op_ndn;
					op->o_bd = op_bd;
			}
		}	else {						
			rc = LDAP_COMPARE_TRUE;
		}
	} else {
		rc = LDAP_COMPARE_FALSE;
	}
	
	return rc;
}


static int dynid_add(Operation *op, SlapReply *rs) {
	slap_overinst *on = (slap_overinst *)op->o_bd->bd_info;
	dynid_data *ad = on->on_bi.bi_private;
	dynid_objmap *ddmap = ad->map;
	
	if ( rs->sr_err != LDAP_SUCCESS ) return SLAP_CB_CONTINUE;

	if ( !op->o_bd ) return SLAP_CB_CONTINUE;

	switch(op->o_tag) {
		case LDAP_REQ_ADD:
			//Debug( LDAP_DEBUG_TRACE, "dynid: LDAP_REQ_ADD[%s]\n", op->o_req_dn.bv_val, 0, 0);
			if (dynid_is_match(op) == LDAP_COMPARE_TRUE)
			{
				if (ddmap->idAttr)
				{
					dynid_generate_id(op);
				}
				if (ddmap->uuidAttr)
				{
					dynid_generate_uuid(op);
				}
				if (ddmap->owneruuidAttr)
				{
					dynid_addownerguid(op, rs);
				}				
			}
			break;
		default:
			return SLAP_CB_CONTINUE;
	}
	return SLAP_CB_CONTINUE;
}

static int
dynid_db_init(
	BackendDB *be
)
{
	slap_overinst *on = (slap_overinst *)be->bd_info;
	dynid_data *dd = ch_calloc(1, sizeof(dynid_data));

	on->on_bi.bi_private = dd;
	return 0;
}

static int
dynid_db_close(
	BackendDB *be
)
{
	slap_overinst *on = (slap_overinst *)be->bd_info;
	dynid_data *dd = on->on_bi.bi_private;
}

static int
dynid_db_destroy(
	BackendDB *be
)
{
	slap_overinst *on = (slap_overinst *)be->bd_info;
	dynid_data *dd = on->on_bi.bi_private;

	if (dd)
		free( dd );
}

#ifdef DYNID_BACK_CONFIG

enum {
	DI_RANGE = 1,
	DI_GUID,
	DI_OWNERGUID,
	DI_OVERRIDE,
	DI_LAST
};

static ConfigDriver	di_cfgen;

static ConfigTable dicfg[] = {
	{ "dynid-range", "dn> <attribute> <minimum> <maximum",
		4, 5, 0, ARG_MAGIC|DI_RANGE, di_cfgen,
		"( OLcfgOvAt:700.1 NAME 'olcDIRange' "
			"DESC 'Dynamic ID Range: <dn>, <attributeDescription>, <minimum>, <maximum>' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString )",
			NULL, NULL },
	{ "dynid-generateduid", "dn> <ad",
		2, 3, 0, ARG_MAGIC|DI_GUID, di_cfgen,
				"( OLcfgOvAt:700.2 NAME 'olcDIGUIDGen' "
			"DESC 'Dynamic ID GUID Generation: <dn>, <attributeDescription>' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString )",
			NULL, NULL },
	{ "dynid-owneruuid", "dn> <ad",
		2, 3, 0, ARG_MAGIC|DI_OWNERGUID, di_cfgen,
				"( OLcfgOvAt:700.3 NAME 'olcDIOwnerGUIDGen' "
			"DESC 'Dynamic ID Owner GUID Generation: <dn>, <attributeDescription>' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString )",
			NULL, NULL },
	{ "dynid-override", "dn> <override dn>",
		2, 3, 0, ARG_MAGIC|DI_OVERRIDE, di_cfgen,
				"( OLcfgOvAt:700.4 NAME 'olcDIOverride' "
			"DESC 'Dynamic ID Override: <dn>, <override dn>' "
			"EQUALITY caseIgnoreMatch "
			"SYNTAX OMsDirectoryString )",
			NULL, NULL },
	{ NULL, NULL, 0, 0, 0, ARG_IGNORED }
};

static ConfigOCs diocs[] = {
	{ "( OLcfgOvOc:700.1 "
		"NAME 'olcDynamicID' "
		"DESC 'Dynamic id configuration' "
		"SUP olcOverlayConfig "
		"MAY (olcDIRange   $ olcDIGUIDGen $ olcDIOwnerGUIDGen $ olcDIOverride) )",
		Cft_Overlay, dicfg, NULL, NULL },
	{ NULL, 0, NULL }
};

static int
di_cfgen( ConfigArgs *c )
{
	slap_overinst	*on = (slap_overinst *)c->bi;
	dynid_data *dd = (dynid_data *)on->on_bi.bi_private;
	dynid_objmap *ddmap = NULL;
	const char	*text = NULL;

	int		rc = 0, i;

	if (dynid_memberships == NULL) {		
		rc = slap_str2ad( "apple-group-memberguid", &dynid_memberships, &text );
		if ( rc != LDAP_SUCCESS ) {
			Debug(LDAP_DEBUG_CONFIG, "=> [%d]dynid_initialize apple-group-memberguid error\n", rc, 0, 0);
			return rc;
		}
	}

	if (dynid_uuid == NULL) {		
		rc = slap_str2ad( "apple-generateduid", &dynid_uuid, &text );
		if ( rc != LDAP_SUCCESS ) {
			Debug(LDAP_DEBUG_CONFIG, "=> [%d]dynid_initialize apple-generateduid error\n", rc, 0, 0);
			return rc;
		}
	}

	if ( c->op == SLAP_CONFIG_EMIT ) {
		switch( c->type ) {
		case DI_RANGE:
			if ( dd && dd->map && !BER_BVISNULL(&dd->map->target_dn) && dd->map->idAttr) {
				struct berval bv;
				bv.bv_len = sprintf( c->msg, "%s %s %d %d",
					dd->map->target_dn.bv_val, dd->map->idAttr->ad_cname.bv_val,  dd->map->min, dd->map->max );
				bv.bv_val = c->msg;
				value_add_one( &c->rvalue_vals, &bv );
				Debug(LDAP_DEBUG_CONFIG, "[SLAP_CONFIG_EMIT] dynid-range dn (%s) AttributeDescription (%s)\n", dd->map->target_dn.bv_val, dd->map->idAttr->ad_cname.bv_val, 0);
				Debug(LDAP_DEBUG_CONFIG, "[SLAP_CONFIG_EMIT] dynid-range minimum (%d) maximum (%d)\n", dd->map->min, dd->map->max, 0);
		} else { 
				rc = 1;
		}
			break;

		case DI_GUID:
			if ( dd && dd->map && !BER_BVISNULL(&dd->map->target_dn) && dd->map->uuidAttr) {
				struct berval bv;
				bv.bv_len = sprintf( c->msg, "%s %s",
					dd->map->target_dn.bv_val, dd->map->uuidAttr->ad_cname.bv_val);
				bv.bv_val = c->msg;
				value_add_one( &c->rvalue_vals, &bv );
				Debug(LDAP_DEBUG_CONFIG, "[SLAP_CONFIG_EMIT] dynid-generateuuid dn (%s) AttributeDescription (%s)\n", dd->map->target_dn.bv_val, dd->map->uuidAttr->ad_cname.bv_val, 0);
			} else {
				rc = 1;
			}
			break;

		case DI_OWNERGUID:
			if ( dd && dd->map && !BER_BVISNULL(&dd->map->target_dn) && dd->map->owneruuidAttr) {
				struct berval bv;
				bv.bv_len = sprintf( c->msg, "%s %s",
					dd->map->target_dn.bv_val, dd->map->owneruuidAttr->ad_cname.bv_val);
				bv.bv_val = c->msg;
				value_add_one( &c->rvalue_vals, &bv );
				Debug(LDAP_DEBUG_CONFIG, "[SLAP_CONFIG_EMIT] dynid-owneruuid dn (%s) AttributeDescription (%s)\n", dd->map->target_dn.bv_val, dd->map->owneruuidAttr->ad_cname.bv_val, 0);
			} else {
				rc = 1;
			}
			break;

		case DI_OVERRIDE:
			if ( dd && dd->map && !BER_BVISNULL(&dd->map->target_dn) && !BER_BVISNULL(&dd->map->override_dn)) {
				struct berval bv;
				bv.bv_len = sprintf( c->msg, "%s %s", dd->map->target_dn.bv_val, dd->map->override_dn.bv_val);
				bv.bv_val = c->msg;
				value_add_one( &c->rvalue_vals, &bv );
				Debug(LDAP_DEBUG_CONFIG, "[SLAP_CONFIG_EMIT] dynid-override dn (%s)\n",  dd->map->target_dn.bv_val, dd->map->override_dn.bv_val, 0);
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
		case DI_RANGE: {
			struct berval target = {0};
			AttributeDescription	*ad = NULL,
						*member_ad = NULL;
			int minimum, maximum;
			char *next;
			const char		*text;
			
			ber_str2bv( c->argv[ 1 ], 0, 1, &target );
			if (BER_BVISNULL(&target)) {
				Debug(LDAP_DEBUG_CONFIG, "=> dynid-range - target not defined\n", 0, 0, 0);			
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-range dn(%s) [%s]\n", c->argv[ 1 ], target.bv_val, 0);			

			rc = slap_str2ad( c->argv[ 2 ], &ad, &text );
			if ( rc != LDAP_SUCCESS ) {
				snprintf( c->msg, sizeof( c->msg ),
					"\"dynid-range [<dn>] [<id attribute>] [minimum] [maximum]\": "
					"unable to find AttributeDescription \"%s\"",
					c->argv[ 2 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->msg, 0 );
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-range AttributeDescription (%s)\n", ad->ad_cname.bv_val, 0, 0);
			
			minimum = strtol(c->argv[ 3 ], &next, 10);
			if ( next == NULL || next[0] != '\0' ) {
				snprintf( c->msg, sizeof( c->msg ),
					"\"dynid-range [<dn>] [<id attribute>] [minimum] [maximum]\": "
					"unable to parse minimum id \"%s\"\n",
					c->argv[ 3 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->msg, 0 );
				return 1;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-range minimum (%d)\n", minimum, 0, 0);
	
			maximum = strtol(c->argv[4], &next, 10);
			if ( next == NULL || next[0] != '\0' ) {
				snprintf( c->msg, sizeof( c->msg ),
					"\"dynid-range [<dn>] [<id attribute>] [minimum] [maximum]\": "
					"unable to parse maximum id \"%s\"\n",
					c->argv[ 4 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->msg, 0 );
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-range maximum (%d)\n", maximum, 0, 0);

			for ( ddmap = dd->map; ddmap != NULL; ddmap = ddmap->next )
			{
				if ( dn_match(&ddmap->target_dn, &target) ) {
					snprintf( c->msg, sizeof( c->msg ),
						"\"dynid-range \": "
							"dn \"%s\" already mapped (reuse).\n",
						ddmap->target_dn.bv_val );
					Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
						c->log, c->msg, 0 );
						break;
				}
			}

			if (!ddmap)
			{
				ddmap = (dynid_objmap *)ch_calloc( 1, sizeof( dynid_objmap ) );
				ber_dupbv (&ddmap->target_dn, &target);
				ddmap->next = dd->map;
				dd->map = ddmap;
			}

			ddmap->idAttr = ad;
			ddmap->min = minimum;
			ddmap->max = maximum;
			ddmap->lastid = minimum;	
		
			} break;
	
		case DI_GUID: {
			struct berval target = {0};
			AttributeDescription	*uuidAd = NULL;
			const char		*text;
	
			ber_str2bv( c->argv[ 1 ], 0, 1, &target );
			if (BER_BVISNULL(&target)) {
				Debug(LDAP_DEBUG_CONFIG, "=> dynid-generateuuid - target not defined\n", 0, 0, 0);			
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-generateuuid dn(%s) [%s]\n", c->argv[ 1 ], target.bv_val, 0);			
	
			rc = slap_str2ad( c->argv[ 2 ], &uuidAd, &text );
			if ( rc != LDAP_SUCCESS ) {
				snprintf( c->msg, sizeof( c->msg ),
					"\"dynid-generateuuid [<oc>] [<id attribute>]\": "
					"unable to find AttributeDescription \"%s\"",
					c->argv[ 2 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->msg, 0 );
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-generateuuid AttributeDescription (%s)\n", uuidAd->ad_cname.bv_val, 0, 0);
	
			for ( ddmap = dd->map; ddmap != NULL; ddmap = ddmap->next )
			{
				if ( dn_match(&ddmap->target_dn, &target) ) {
					snprintf( c->msg, sizeof( c->msg ),
						"\"dynid-generateuuid \": "
							"dn \"%s\" already mapped (reuse).\n",
						ddmap->target_dn.bv_val );
					Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
						c->log, c->msg, 0 );
						break;
				}
			}

			if (!ddmap)
			{
				ddmap = (dynid_objmap *)ch_calloc( 1, sizeof( dynid_objmap ) );
				ber_dupbv (&ddmap->target_dn, &target);
				ddmap->next = dd->map;
				dd->map = ddmap;
			}
			
			ddmap->uuidAttr = uuidAd;
	
	
		} break;
	
		case DI_OWNERGUID: {
			struct berval target = {0};
			AttributeDescription	*owneruuidAd = NULL;
			const char		*text;
	
			ber_str2bv( c->argv[ 1 ], 0, 1, &target );
			if (BER_BVISNULL(&target)) {
				Debug(LDAP_DEBUG_CONFIG, "=> dynid-owneruuid - target not defined\n", 0, 0, 0);			
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-owneruuid dn(%s) [%s]\n", c->argv[ 1 ], target.bv_val, 0);			
	
			rc = slap_str2ad( c->argv[ 2 ], &owneruuidAd, &text );
			if ( rc != LDAP_SUCCESS ) {
				snprintf( c->msg, sizeof( c->msg ),
					"\"dynid-owneruuid [<oc>] [<id attribute>]\": "
					"unable to find AttributeDescription \"%s\"",
					c->argv[ 2 ] );
				Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
					c->log, c->msg, 0 );
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-owneruuid AttributeDescription (%s)\n", owneruuidAd->ad_cname.bv_val, 0, 0);
	
			for ( ddmap = dd->map; ddmap != NULL; ddmap = ddmap->next )
			{
				if ( dn_match(&ddmap->target_dn, &target) ) {
					snprintf( c->msg, sizeof( c->msg ),
						"\"dynid-owneruuid \": "
							"dn \"%s\" already mapped (reuse).\n",
						ddmap->target_dn.bv_val );
					Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
						c->log, c->msg, 0 );
						break;
				}
			}

			if (!ddmap)
			{
				ddmap = (dynid_objmap *)ch_calloc( 1, sizeof( dynid_objmap ) );
				ber_dupbv (&ddmap->target_dn, &target);
				ddmap->next = dd->map;
				dd->map = ddmap;
			}

			ddmap->owneruuidAttr = owneruuidAd;
	
	
		} break;

		case DI_OVERRIDE: {
			struct berval target = {0};
			struct berval override = {0};
			AttributeDescription	*owneruuidAd = NULL;
			const char		*text;

			ber_str2bv( c->argv[ 1 ], 0, 1, &target );
			if (BER_BVISNULL(&target)) {
				Debug(LDAP_DEBUG_CONFIG, "=> dynid-override - target not defined\n", 0, 0, 0);			
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-override target dn(%s) [%s]\n", c->argv[ 1 ], target.bv_val, 0);			

			ber_str2bv( c->argv[ 2 ], 0, 1, &override );
			if (BER_BVISNULL(&override)) {
				Debug(LDAP_DEBUG_CONFIG, "=> dynid-override - override not defined\n", 0, 0, 0);			
				return ARG_BAD_CONF;
			}
			Debug(LDAP_DEBUG_CONFIG, "=> dynid-override override dn(%s) [%s]\n", c->argv[ 2 ], override.bv_val, 0);			

			for ( ddmap = dd->map; ddmap != NULL; ddmap = ddmap->next )
			{
				if ( dn_match(&ddmap->target_dn, &target) ) {
					snprintf( c->msg, sizeof( c->msg ),
						"\"dynid-override \": "
							"dn \"%s\" already mapped (reuse).\n",
						ddmap->target_dn.bv_val );
					Debug( LDAP_DEBUG_CONFIG, "%s: %s.\n",
						c->log, c->msg, 0 );
						break;
				}
			}

			if (!ddmap)
			{
				ddmap = (dynid_objmap *)ch_calloc( 1, sizeof( dynid_objmap ) );
				ber_dupbv (&ddmap->target_dn, &target);
				ddmap->next = dd->map;
				dd->map = ddmap;
			}

			ber_dupbv (&ddmap->override_dn, &override);		
		} break;

		default:
			rc = 1;
			break;
	}

	return rc;
}
#endif
int dynid_initialize() {
	int rc = 0;

	memset( &dynid, 0, sizeof( slap_overinst ) );
	
	dynid.on_bi.bi_type = "dynid";
	dynid.on_bi.bi_db_init = dynid_db_init;
	dynid.on_bi.bi_op_add = dynid_add;
	dynid.on_bi.bi_db_close = dynid_db_close;
	dynid.on_bi.bi_db_destroy = dynid_db_destroy;
		
	dynid.on_bi.bi_db_config = config_generic_wrapper;
	dynid.on_bi.bi_cf_ocs = diocs;

	rc = config_register_schema( dicfg, diocs );
	if ( rc ) {
		return rc;
	}

	return overlay_register(&dynid);
}

#if SLAPD_OVER_dynid == SLAPD_MOD_DYNAMIC && defined(PIC)
int
init_module( int argc, char *argv[] )
{
	return dynid_initialize();
}
#endif

#endif /* SLAPD_OVER_DYNID */
