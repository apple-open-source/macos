#include "portable.h"
#ifdef SLAPD_OVER_ODUSERS
#include "overlayutils.h"

#include <arpa/inet.h>
#define ODUSERS_BACK_CONFIG 1

#include <ac/string.h>
#include <ac/ctype.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"
#define __COREFOUNDATION_CFFILESECURITY__
#include <CoreFoundation/CoreFoundation.h>
#include "applehelpers.h"

static slap_overinst odusers;
static ConfigDriver odusers_cf;

#define kDirservConfigName "cn=dirserv,cn=config"

static ConfigTable odcfg[] = {
	{ "odusers", "enabled", 1, 1, 0,
	  ARG_MAGIC, odusers_cf,
	  "( OLcfgOvAt:700.11 NAME 'olcODUsersEnabled' "
	  "DESC 'Enable OD Users overlay' "
	  "EQUALITY booleanMatch "
	  "SYNTAX OMsBoolean SINGLE-VALUE )", NULL, NULL },
	{ NULL, NULL, 0, 0, 0, ARG_IGNORED }
};

static ConfigOCs odocs[] = {
	{ "( OLcfgOvOc:700.11 "
	    "NAME 'olcODUsers' "
	    "DESC 'OD Users Overlay configuration' "
	    "SUP olcOverlayConfig "
	    "MAY (olcODUsersEnabled) )",
	    Cft_Overlay, odcfg, NULL, NULL },
	{ NULL, 0, NULL }
};

static int odusers_cf(ConfigArgs *c) {
	slap_overinst *on = (slap_overinst *)c->bi;
	return 1;
}

static int odusers_delete(Operation *op, SlapReply *rs) {
	OperationBuffer opbuf;
	Operation *fakeop;
	Entry *e = NULL;
	Entry *p = NULL;
	char guidstr[37];
	struct berval *dn = &op->o_req_ndn;

	if(op->o_req_ndn.bv_len < 14 || !(strnstr(op->o_req_ndn.bv_val, "cn=users,", op->o_req_ndn.bv_len)!=NULL || strnstr(op->o_req_ndn.bv_val, "cn=computers,", op->o_req_ndn.bv_len)!=NULL) || strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len)!=NULL) {
		goto out;
	}

	// Fake up a new Operation, but use the current Connection structure
	memset(&opbuf, 0, sizeof(opbuf));
	fakeop = (Operation*)&opbuf;
	fakeop->o_hdr = &opbuf.ob_hdr;
	fakeop->o_controls = opbuf.ob_controls;
	operation_fake_init(op->o_conn, (Operation*)&opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_ndn = op->o_ndn;

	dnParent(&op->o_req_ndn, &fakeop->o_req_ndn);
	if(fakeop->o_req_ndn.bv_len < 1) {
		goto out;
	}

	fakeop->o_req_dn = fakeop->o_req_ndn;
	p = odusers_copy_entry(fakeop);
	if(!p) {
		goto out;
	}

	// First check for delete access to children of the parent
	if(!access_allowed(fakeop, p, slap_schema.si_ad_children, NULL, ACL_WDEL, NULL)) {
		Debug(LDAP_DEBUG_ANY, "%s: access denied: %s attempted to delete child of %s", __PRETTY_FUNCTION__, fakeop->o_ndn.bv_val, p->e_dn);
		goto out;
	}

	// Find the entry we're trying to delete
	fakeop->o_req_dn = fakeop->o_req_ndn = *dn;
	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	// Check for delete access of the specific entry
	if(!access_allowed(fakeop, e, slap_schema.si_ad_entry, NULL, ACL_WDEL, NULL)) {
		Debug(LDAP_DEBUG_ANY, "%s: access denied: %s attempted to delete %s", __PRETTY_FUNCTION__, fakeop->o_ndn.bv_val, fakeop->o_req_ndn.bv_val);
		goto out;
	}

	if( odusers_get_authguid(e, guidstr) ) {
		goto out;
	}

	// Perform the removal
	if( odusers_remove_authdata(guidstr) != 0 ) {
		goto out;
	}

out:
	if(e) entry_free(e);
	if(p) entry_free(p);
	return SLAP_CB_CONTINUE;
}

static int odusers_search_userpolicy(Operation *op, SlapReply *rs, int iseffective) {
	OperationBuffer opbuf;
	Operation *fakeop;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a;
	char guidstr[37];
	Entry *retentry = NULL;
	
	e = odusers_copy_authdata(&op->o_req_ndn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, op->o_req_ndn.bv_val, 0);
		goto out;
	}

	for(a = e->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, "apple-user-passwordpolicy", a->a_desc->ad_cname.bv_len) == 0) {
			retentry = entry_alloc();
			if(!retentry) goto out;

			retentry->e_id = NOID;
			retentry->e_name = op->o_req_dn;
			retentry->e_nname = op->o_req_ndn;

			if(iseffective) {
				Attribute *effective = NULL;
				const char *text = NULL;
				CFDictionaryRef effectivedict = odusers_copy_effectiveuserpoldict(&op->o_req_ndn);

				struct berval *bv = odusers_copy_dict2bv(effectivedict);
				CFRelease(effectivedict);
				if(!bv) {
					Debug(LDAP_DEBUG_ANY, "%s: Unable to convert effective policy to berval", __PRETTY_FUNCTION__, 0, 0);
					goto out;
				}

				AttributeDescription *effectivedesc = NULL;
				if(slap_str2ad("apple-user-passwordpolicy-effective", &effectivedesc, &text) != 0) {
					Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for effective policy", __PRETTY_FUNCTION__, 0, 0);
					goto out;
				}

				effective = attr_alloc(effectivedesc);
				effective->a_vals = ch_malloc(2 * sizeof(struct berval));
				effective->a_vals[0] = *bv;
				free(bv);
				effective->a_vals[1].bv_len = 0;
				effective->a_vals[1].bv_val = NULL;
				effective->a_nvals = effective->a_vals;

				retentry->e_attrs = effective;
			} else {
				retentry->e_attrs = attr_dup(a);
				if(!retentry->e_attrs) {
					Debug(LDAP_DEBUG_ANY, "%s: could not duplicate entry: %s", __PRETTY_FUNCTION__, retentry->e_name.bv_val, 0);
					goto out;
				}
			}
			
			op->ors_slimit = -1;
			rs->sr_entry = retentry;
			rs->sr_nentries = 0;
			rs->sr_flags = 0;
			rs->sr_ctrls = NULL;
			rs->sr_operational_attrs = NULL;
			rs->sr_attrs = op->ors_attrs;
			rs->sr_err = LDAP_SUCCESS;
			rs->sr_err = send_search_entry(op, rs);
			if(rs->sr_err == LDAP_SIZELIMIT_EXCEEDED) {
				Debug(LDAP_DEBUG_ANY, "%s: size limit exceeded on entry: %s", __PRETTY_FUNCTION__, retentry->e_name.bv_val, 0);
			}
			rs->sr_entry = NULL;
			send_ldap_result(op, rs);
			if(e) entry_free(e);
			if(retentry) entry_free(retentry);
			return rs->sr_err;
		}
	}

out:
	if(e) entry_free(e);
	if(retentry) entry_free(retentry);
	return SLAP_CB_CONTINUE;
}

static int odusers_search_globalpolicy(Operation *op, SlapReply *rs) {
	Attribute *global = odusers_copy_globalpolicy();
	if(!global) {
		CFDictionaryRef globaldict = odusers_copy_defaultglobalpolicy();
		struct berval *bv = odusers_copy_dict2bv(globaldict);
		CFRelease(globaldict);

		AttributeDescription *globaldesc = NULL;
		const char *text = NULL;
		if(slap_str2ad("apple-user-passwordpolicy", &globaldesc, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for global policy", __PRETTY_FUNCTION__, 0, 0);
			goto out;
		}

		global = attr_alloc(globaldesc);
		global->a_vals = ch_malloc(2 * sizeof(struct berval));
		global->a_vals[0] = *bv;
		global->a_vals[1].bv_len = 0;
		global->a_vals[1].bv_val = NULL;
		global->a_nvals = global->a_vals;
	}

	Entry *retentry = entry_alloc();
	if(!retentry) goto out;

	retentry->e_id = NOID;
	retentry->e_name = op->o_req_dn;
	retentry->e_nname = op->o_req_ndn;
	retentry->e_attrs = global;

	op->ors_slimit = -1;
	rs->sr_entry = retentry;
	rs->sr_nentries = 0;
	rs->sr_flags = 0;
	rs->sr_ctrls = NULL;
	rs->sr_operational_attrs = NULL;
	rs->sr_attrs = op->ors_attrs;
	rs->sr_err = LDAP_SUCCESS;
	rs->sr_err = send_search_entry(op, rs);
	if(rs->sr_err == LDAP_SIZELIMIT_EXCEEDED) {
		Debug(LDAP_DEBUG_ANY, "%s: size limit exceeded on entry: %s", __PRETTY_FUNCTION__, retentry->e_name.bv_val, 0);
	}
	rs->sr_entry = NULL;
	send_ldap_result(op, rs);
	return rs->sr_err;

out:
	if(retentry) entry_free(retentry);
	if(global) attr_free(global);
	return SLAP_CB_CONTINUE;
}

static void enabledmech_callback(const void *key, const void *value, void *context) {
	CFMutableArrayRef enabledmechs = (CFMutableArrayRef)context;

	if(CFStringCompare(value, CFSTR("ON"), 0) == kCFCompareEqualTo) {
		CFArrayAppendValue(enabledmechs, key);
	}
}

static void disabledmech_callback(const void *key, const void *value, void *context) {
	CFMutableArrayRef disabledmechs = (CFMutableArrayRef)context;

	if(CFStringCompare(value, CFSTR("OFF"), 0) == kCFCompareEqualTo) {
		CFArrayAppendValue(disabledmechs, key);
	}
}

static int odusers_search_mechs(Operation *op, SlapReply *rs, int enabled) {
	int ret = -1;
	const char *suffix = NULL;
	CFDictionaryRef prefsdict = NULL;
	CFDictionaryRef sasldict = NULL;
	CFMutableArrayRef enabledMechs = NULL;
	Attribute *a = NULL;
	AttributeDescription *enabledMechAD = NULL;
	Entry *e = NULL;
	const char *text = NULL;
	int i;

	suffix = op->o_req_ndn.bv_val + strlen(kDirservConfigName);
	Debug(LDAP_DEBUG_ANY, "%s: suffix %s", __PRETTY_FUNCTION__, suffix, 0);
	prefsdict = odusers_copy_pwsprefs(suffix);
	if(!prefsdict) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve PWS prefs dictionary", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	sasldict = CFDictionaryGetValue(prefsdict, CFSTR("SASLPluginStates"));
	if(!sasldict) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to locate SASLPluginStates dictionary in PWS prefs", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	enabledMechs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if(!enabledMechs) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to allocate mutable array", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	if(enabled) {
		CFDictionaryApplyFunction(sasldict, enabledmech_callback, enabledMechs);

		if(slap_str2ad("apple-enabled-auth-mech", &enabledMechAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for apple-enabled-auth-mech", __PRETTY_FUNCTION__, 0, 0);
			goto out;
		}
	} else {
		CFDictionaryApplyFunction(sasldict, disabledmech_callback, enabledMechs);

		if(slap_str2ad("apple-disabled-auth-mech", &enabledMechAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for apple-disabled-auth-mech", __PRETTY_FUNCTION__, 0, 0);
			goto out;
		}

	}

	a = attr_alloc(enabledMechAD);
	a->a_vals = ch_malloc((CFArrayGetCount(enabledMechs)+1) * sizeof(struct berval));
	for(i = 0; i < CFArrayGetCount(enabledMechs); i++) {
		CFStringRef tmpstr = CFArrayGetValueAtIndex(enabledMechs, i);
		if(!tmpstr) continue;

		a->a_vals[i].bv_len = CFStringGetLength(tmpstr);
		a->a_vals[i].bv_val = calloc(1, a->a_vals[i].bv_len + 1);
		CFStringGetCString(tmpstr, a->a_vals[i].bv_val, a->a_vals[i].bv_len+1, kCFStringEncodingUTF8);
	}
	a->a_vals[i].bv_val = NULL;
	a->a_vals[i].bv_len = 0;
	a->a_nvals = a->a_vals;
	a->a_next = NULL;
	a->a_numvals = CFArrayGetCount(enabledMechs);
	a->a_flags = 0;
	
	e = entry_alloc();
	if(!e) goto out;

	e->e_id = NOID;
	e->e_name = op->o_req_dn;
	e->e_nname = op->o_req_ndn;
	e->e_attrs = a;

	op->ors_slimit = -1;
	rs->sr_entry = e;
	rs->sr_nentries = 0;
	rs->sr_flags = 0;
	rs->sr_ctrls = NULL;
	rs->sr_operational_attrs = NULL;
	rs->sr_attrs = op->ors_attrs;
	rs->sr_err = LDAP_SUCCESS;
	rs->sr_err = send_search_entry(op, rs);
	
	rs->sr_entry = NULL;
	send_ldap_result(op, rs);
	ret = rs->sr_err;

out:
	if(e) entry_free(e);
	if(prefsdict) CFRelease(prefsdict);
	if(enabledMechs) CFRelease(enabledMechs);
	return ret;
}

static bool odusers_isaccount(Operation *op) {
	bool ret = false;

	if(strnstr(op->o_req_ndn.bv_val, "cn=users", op->o_req_ndn.bv_len) != NULL) ret = 1;
	if(strnstr(op->o_req_ndn.bv_val, "cn=computers", op->o_req_ndn.bv_len) != NULL) ret = 1;
	
	return ret;
}

static int odusers_search(Operation *op, SlapReply *rs) {
	bool isaccount;

	if(!op || op->o_req_ndn.bv_len == 0) return SLAP_CB_CONTINUE;
	if(!op->ors_attrs) return SLAP_CB_CONTINUE;
	if(strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len) != NULL) return SLAP_CB_CONTINUE;

	isaccount = odusers_isaccount(op);
	
	if(isaccount && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-user-passwordpolicy", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_userpolicy(op, rs, 0);
	} else if(isaccount && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-user-passwordpolicy-effective", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_userpolicy(op, rs, 1);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-user-passwordpolicy", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_globalpolicy(op, rs);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-enabled-auth-mech", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_mechs(op, rs, 1);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-disabled-auth-mech", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_mechs(op, rs, 0);
	}

	return SLAP_CB_CONTINUE;
}

static int odusers_insert_vendorName(SlapReply *rs) {
	Attribute *a = NULL;
	// Make sure attribute doesn't exist
	for(a = rs->sr_un.sru_search.r_entry->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, "vendorName", a->a_desc->ad_cname.bv_len) == 0) {
			goto out;
		}
	}

	AttributeDescription *namedesc = NULL;
	const char *text = NULL;
	if(slap_str2ad("vendorName", &namedesc, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for vendorName", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	a = attr_alloc(namedesc);
	a->a_vals = ch_malloc(2 * sizeof(struct berval));
	a->a_vals[0].bv_val = strdup("Apple") ;
	a->a_vals[0].bv_len = strlen(a->a_vals[0].bv_val);
	a->a_vals[1].bv_len = 0;
	a->a_vals[1].bv_val = NULL;
	a->a_nvals = a->a_vals;

	a->a_next = rs->sr_un.sru_search.r_entry->e_attrs;
	rs->sr_un.sru_search.r_entry->e_attrs = a;
out:
	return 0;
}

static int odusers_insert_vendorVersion(SlapReply *rs) {
	Attribute *a = NULL;
	// Make sure attribute doesn't exist
	for(a = rs->sr_un.sru_search.r_entry->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, "vendorVersion", a->a_desc->ad_cname.bv_len) == 0) {
			goto out;
		}
	}

	AttributeDescription *namedesc = NULL;
	const char *text = NULL;
	if(slap_str2ad("vendorVersion", &namedesc, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for vendorVersion", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	a = attr_alloc(namedesc);
	a->a_vals = ch_malloc(2 * sizeof(struct berval));
	a->a_vals[0].bv_val = strdup(PROJVERSION);
	a->a_vals[0].bv_len = strlen(a->a_vals[0].bv_val);
	a->a_vals[1].bv_len = 0;
	a->a_vals[1].bv_val = NULL;
	a->a_nvals = a->a_vals;

	a->a_next = rs->sr_un.sru_search.r_entry->e_attrs;
	rs->sr_un.sru_search.r_entry->e_attrs = a;
out:
	return 0;
}

static int odusers_insert_operatingSystemVersion(SlapReply *rs) {
	const char* attrName = "operatingSystemVersion";
	Attribute *a = NULL;
	// Make sure attribute doesn't exist
	for(a = rs->sr_un.sru_search.r_entry->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, attrName, a->a_desc->ad_cname.bv_len) == 0) {
			goto out;
		}
	}

	AttributeDescription *namedesc = NULL;
	const char *text = NULL;
	if(slap_str2ad(attrName, &namedesc, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for %s", __PRETTY_FUNCTION__, attrName, 0);
		goto out;
	}

	a = attr_alloc(namedesc);
	a->a_vals = ch_malloc(2 * sizeof(struct berval));
	a->a_vals[0].bv_val = strdup(TGT_OS_VERSION);
	a->a_vals[0].bv_len = strlen(a->a_vals[0].bv_val);
	a->a_vals[1].bv_len = 0;
	a->a_vals[1].bv_val = NULL;
	a->a_nvals = a->a_vals;

	a->a_next = rs->sr_un.sru_search.r_entry->e_attrs;
	rs->sr_un.sru_search.r_entry->e_attrs = a;
out:
	return 0;
}

static int odusers_response(Operation *op, SlapReply *rs) {
	static char *version = NULL;
	AttributeName *an = NULL;
	if ((rs->sr_type != REP_SEARCH) || (op->oq_search.rs_attrs == NULL) ) {
		return SLAP_CB_CONTINUE;
	}

	if(!op->ors_attrs) return SLAP_CB_CONTINUE;

	// Only interested in the rootDSE
	if(op->o_req_ndn.bv_len != 0) return SLAP_CB_CONTINUE;

	int i;
	for(i = 0; op->ors_attrs[i].an_name.bv_len > 0; i++) {
		if(op->ors_attrs[i].an_name.bv_val == NULL) break;
		if(strncmp(op->ors_attrs[i].an_name.bv_val, "vendorName", op->ors_attrs[i].an_name.bv_len) == 0) {
			odusers_insert_vendorName(rs);
		} else if(strncmp(op->ors_attrs[i].an_name.bv_val, "vendorVersion", op->ors_attrs[i].an_name.bv_len) == 0) {
			odusers_insert_vendorVersion(rs);
		} else if(strncmp(op->ors_attrs[i].an_name.bv_val, "operatingSystemVersion", op->ors_attrs[i].an_name.bv_len) == 0) {
			odusers_insert_operatingSystemVersion(rs);
		} else if(strncmp(op->ors_attrs[i].an_name.bv_val, "+", op->ors_attrs[i].an_name.bv_len) == 0) {
			odusers_insert_vendorName(rs);
			odusers_insert_vendorVersion(rs);
			odusers_insert_operatingSystemVersion(rs);
		}
	}
	
out:
	return SLAP_CB_CONTINUE;
}

static int odusers_modify_policy(Operation *op, SlapReply *rs) {
	OperationBuffer opbuf = {0};
	Operation *fakeop;
	Connection conn = {0};
	Entry *e;

	e = odusers_copy_authdata(&op->o_req_ndn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, op->o_req_ndn.bv_val, 0);
		goto out;
	}
	
	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = op->o_dn;
	fakeop->o_ndn = op->o_ndn;
	fakeop->o_req_dn = e->e_name;
	fakeop->o_req_ndn = e->e_nname;
	fakeop->o_tag = op->o_tag;
	fakeop->orm_modlist = op->orm_modlist;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	// Use the frontend DB for the modification so we go through the syncrepl	
	// overlay and our change gets replicated.
	fakeop->o_bd = frontendDB;

	slap_op_time(&op->o_time, &op->o_tincr);

	fakeop->o_bd->be_modify(fakeop, rs);
	if(rs->sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to modify user policy: %s (%d)\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, rs->sr_err);
		goto out;
	}

	send_ldap_result(op, rs);
	return rs->sr_err;

out:
	return SLAP_CB_CONTINUE;
}

static int odusers_modify_globalpolicy(Operation *op, SlapReply *rs) {
	OperationBuffer opbuf = {0};
	Operation *fakeop;
	Connection conn = {0};
	Entry *e;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = op->o_dn;
	fakeop->o_ndn = op->o_ndn;
	fakeop->o_req_dn.bv_val = "cn=access,cn=authdata";
	fakeop->o_req_dn.bv_len = strlen(fakeop->o_req_dn.bv_val);
	fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_tag = op->o_tag;
	fakeop->orm_modlist = op->orm_modlist;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	// Use the frontend DB for the modification so we go through the syncrepl	
	// overlay and our change gets replicated.
	fakeop->o_bd = frontendDB;

	slap_op_time(&op->o_time, &op->o_tincr);

	fakeop->o_bd->be_modify(fakeop, rs);
	if(rs->sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to modify user policy: %s (%d)\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, rs->sr_err);
		goto out;
	}

	send_ldap_result(op, rs);
	return rs->sr_err;

out:
	return SLAP_CB_CONTINUE;
}

static int odusers_enforce_admin(Operation *op) {
	CFDictionaryRef policy = NULL;
	int ret = -1;

	policy = odusers_copy_effectiveuserpoldict(&op->o_conn->c_dn);
	if(!policy) return SLAP_CB_CONTINUE;
	if(odusers_isdisabled(policy)) {
		Debug(LDAP_DEBUG_ANY, "%s: disabled user tried to set policy", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	if(!odusers_isadmin(policy)) {
		Debug(LDAP_DEBUG_ANY, "%s: non-admin user tried to set policy", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	ret = 0;
out:
	if(policy) CFRelease(policy);
	return ret;
}

static int odusers_modify(Operation *op, SlapReply *rs) {
	bool isaccount;
	Modifications *m;

	if(!op || op->o_req_ndn.bv_len == 0) return SLAP_CB_CONTINUE;
	if(strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len) != NULL) return SLAP_CB_CONTINUE;

	m = op->orm_modlist;
	if(!m) return SLAP_CB_CONTINUE;

	isaccount = odusers_isaccount(op);

	// setpolicy will only work on a replacement basis, if we support
	// non-setpolicy modifys this needs to become policy specific.
	if((m->sml_op & LDAP_MOD_OP) != LDAP_MOD_REPLACE) return SLAP_CB_CONTINUE;

	if(isaccount && strncmp(m->sml_desc->ad_cname.bv_val, "apple-user-passwordpolicy", m->sml_desc->ad_cname.bv_len) == 0) {
		if(odusers_enforce_admin(op) != 0) {
			send_ldap_error(op, rs, LDAP_INSUFFICIENT_ACCESS, "policy modification not permitted");
			return rs->sr_err;
		}

		return odusers_modify_policy(op, rs);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) && strncmp(m->sml_desc->ad_cname.bv_val, "apple-user-passwordpolicy", m->sml_desc->ad_cname.bv_len) == 0) {
		if(odusers_enforce_admin(op) != 0) {
			send_ldap_error(op, rs, LDAP_INSUFFICIENT_ACCESS, "global policy modification not permitted");
			return rs->sr_err;
		}

		return odusers_modify_globalpolicy(op, rs);
	}

	return SLAP_CB_CONTINUE;
}

int odusers_initialize() {
	int rc = 0;

	memset(&odusers, 0, sizeof(slap_overinst));

	odusers.on_bi.bi_type = "odusers";
	odusers.on_bi.bi_cf_ocs = odocs;
	odusers.on_bi.bi_op_delete = odusers_delete;
	odusers.on_bi.bi_op_search = odusers_search;
	odusers.on_bi.bi_op_modify = odusers_modify;
	odusers.on_response = odusers_response;

	rc = config_register_schema(odcfg, odocs);
	if(rc) return rc;

	return overlay_register(&odusers);
}

#if SLAPD_OVER_ODUSERS == SLAPD_MOD_DYNAMIC
int
init_module( int argc, char *argv[] )
{
    return odusers_initialize();
}
#endif

#endif /* SLAPD_OVER_ODUSERS */
