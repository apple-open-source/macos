#include "overlayutils.h"

void dump_berval( struct berval *bv )
{
	if (bv)
	{
		Debug( LDAP_DEBUG_BER, "berval len[%d] [%s]\n", bv->bv_len, bv->bv_val, 0);			
	}
}

void dump_berval_array(BerVarray bva)
{
	int i = 0;
	
	if (bva)
	{
		for ( i = 0 ; bva[i].bv_val; i++)
			Debug( LDAP_DEBUG_BER, "dump_berval_array[%d] len[%d] [%s]\n", i, bva[i].bv_len, bva[i].bv_val);			
	}
}

void dump_slap_attr_desc(AttributeDescription *desc)
{
	if (desc)
	{
		// dump ad_type
		Debug( LDAP_DEBUG_BER, "#####AD_CNAME#####\n", 0, 0, 0);
		dump_berval(&desc->ad_cname);
		// dump ad_tags
		// dump ad_flags
		Debug( LDAP_DEBUG_BER, "ad_flags [%x]\n", desc->ad_flags, 0, 0);
		
	}
}

void dump_slap_attr(Attribute *attr)
{
	Attribute *current_attr = NULL;
	int i = 0;
	
	if (attr)
	{
		for ( i = 1, current_attr = attr; current_attr; i++, current_attr = current_attr->a_next )
		{
			Debug( LDAP_DEBUG_BER, "#####A_DESC#####\n", 0, 0, 0);
			dump_slap_attr_desc(current_attr->a_desc);
			Debug( LDAP_DEBUG_BER, "#####A_VALS#####\n", 0, 0, 0);
			dump_berval_array(current_attr->a_vals);
			Debug( LDAP_DEBUG_BER, "#####A_NVALS#####\n", 0, 0, 0);
			dump_berval_array(current_attr->a_nvals);
		}
		Debug( LDAP_DEBUG_BER, "dump_slap_attr count(%d)\n", i, 0, 0);

	}
}

void dump_slap_entry(Entry *ent)
{
	if (ent)
	{
		dump_slap_attr(ent->e_attrs);
	}
	
}

void dump_req_bind_s(req_bind_s *req)
{	
#if 0
typedef struct req_bind_s {
	int rb_method;
	struct berval rb_cred;
	struct berval rb_edn;
	slap_ssf_t rb_ssf;
	struct berval rb_tmp_mech;	/* FIXME: temporary */
} req_bind_s;
#endif

	if (req)
	{
		Debug( LDAP_DEBUG_BER, "#####RB_CRED#####\n", 0, 0, 0);
		dump_berval( &req->rb_cred );
		Debug( LDAP_DEBUG_BER, "#####RB_EDN#####\n", 0, 0, 0);
		dump_berval( &req->rb_edn );
		Debug( LDAP_DEBUG_BER, "#####RB_TMP_MECH#####\n", 0, 0, 0);
		dump_berval( &req->rb_tmp_mech );
	}
}

void dump_req_add_s(req_add_s *req)
{	
	if (req)
	{
		dump_slap_entry(req->rs_e);
		// dump_slap_mod_list(req->rs_modlist);
	}
}

