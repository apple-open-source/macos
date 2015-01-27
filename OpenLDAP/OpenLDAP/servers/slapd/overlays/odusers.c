#include "portable.h"
#ifdef SLAPD_OVER_ODUSERS
#include "overlayutils.h"

#include <arpa/inet.h>
#define ODUSERS_BACK_CONFIG 1

#include <ac/string.h>
#include <ac/ctype.h>
#include <uuid/uuid.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"
#define __COREFOUNDATION_CFFILESECURITY__
#include <CoreFoundation/CoreFoundation.h>
#include "applehelpers.h"

extern AttributeDescription *policyAD;
extern AttributeDescription *passwordRequiredDateAD;

static AttributeDescription *uidAD = NULL;
static AttributeDescription *krbAD = NULL;
static AttributeDescription *draftkrbAD = NULL;
static AttributeDescription *draftkrbAliasesAD = NULL;
static AttributeDescription *aaAD = NULL;
static AttributeDescription *altsecAD = NULL;

static slap_overinst odusers;
static ConfigDriver odusers_cf;
static int odusers_add_authdata(Operation *op, SlapReply *rs, uuid_t newuuid);

#define kDirservConfigName "cn=dirserv,cn=config"
#define ODUSERS_EXTRA_KEY "odusers_key"

typedef struct OpExtraOD {
	OpExtra oe;
	uuid_t uuid;
} OpExtraOD;

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

#undef calloc
#undef malloc
#undef free


static int odusers_cf(ConfigArgs *c) {
	slap_overinst *on = (slap_overinst *)c->bi;
	return 1;
}

static bool odusers_isaccount(Operation *op) {
	bool ret = false;

	if(strnstr(op->o_req_ndn.bv_val, "cn=users", op->o_req_ndn.bv_len) != NULL) ret = 1;
	if(strnstr(op->o_req_ndn.bv_val, "cn=computers", op->o_req_ndn.bv_len) != NULL) ret = 1;
	
	return ret;
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

/* bridges an authdata attribute to a user container request */
static int odusers_search_bridge_authdata(Operation *op, SlapReply *rs, const char *reqattr) {
	OperationBuffer opbuf;
	Operation *fakeop;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	Entry *retentry = NULL;
	char guidstr[37];

	dnNormalize( 0, NULL, NULL, &op->o_req_dn, &op->o_req_ndn, NULL );
	e = odusers_copy_authdata(&op->o_req_ndn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __func__, op->o_req_ndn.bv_val, 0);
		goto out;
	}

	for(a = e->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, reqattr, a->a_desc->ad_cname.bv_len) == 0) {
			retentry = entry_alloc();
			if(!retentry) goto out;

			retentry->e_id = NOID;
			retentry->e_name = op->o_req_dn;
			retentry->e_nname = op->o_req_ndn;

			retentry->e_attrs = attr_dup(a);
			if(!retentry->e_attrs) {
				Debug(LDAP_DEBUG_ANY, "%s: could not duplicate entry: %s", __func__, retentry->e_name.bv_val, 0);
				goto out;
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
				Debug(LDAP_DEBUG_ANY, "%s: size limit exceeded on entry: %s", __func__, retentry->e_name.bv_val, 0);
			}
			rs->sr_entry = NULL;
			send_ldap_result(op, rs);
			if(e) entry_free(e);
			return rs->sr_err;
		}
	}

out:
	if(e) entry_free(e);
//	if (retentry) entry_free(retentry);
	return SLAP_CB_CONTINUE;
}

static int odusers_search_accountpolicy_proxy(Operation *op, SlapReply *rs, char *attr_str) {
	Entry *retentry = NULL;
	Attribute *attr = NULL;
	Filter *filter = NULL;
	AttributeAssertion *ava = NULL;
	char *tmpstr = NULL;
	struct berval dn;

	CFDictionaryRef userpolicyinfodict = NULL;
	__block CFDictionaryRef policyData = NULL;
	CFStringRef globalaccountpolicyGUID = NULL;
	CFErrorRef cferr = NULL;
	int64_t	policyresult = 0;	
	int64_t willExpireIn = 0;
	bool override = false;

	bool isaccount = odusers_isaccount(op);
	
	dn = op->o_req_ndn;

	if(!BER_BVISEMPTY(&dn)) {
		if (op->ors_filter) {
			ava = op->ors_filter->f_ava;
			filter = op->ors_filter;							
		}
		
		if (isaccount) {
			userpolicyinfodict = odusers_copy_accountpolicyinfo(&dn);
			override = odusers_accountpolicy_override(&dn);
		} else if (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) {
			userpolicyinfodict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			/* kAPAttributeGlobalPoliciesGUID */
			globalaccountpolicyGUID = odusers_copy_globalaccountpolicyGUID();
			if (globalaccountpolicyGUID) {
				CFDictionarySetValue(userpolicyinfodict, kAPAttributeGlobalPoliciesGUID, globalaccountpolicyGUID);
			}
		}
		
		if (userpolicyinfodict) {
			if (override) { /* bypass AccountPolicy evaluation and return 'pass by default' */
				if ( strcmp(attr_str, "apple-authenticationAllowed") == 0 || strcmp(attr_str, "apple-passwordChangeAllowed") == 0 ) {
					policyresult = 1;
				} else if (	strcmp(attr_str, "apple-willPasswordExpire") == 0 || strcmp(attr_str, "apple-willAuthenticationsExpire") == 0 ) {
					policyresult = 0;
				} else if (strcmp(attr_str, "apple-secondsUntilPasswordExpires") == 0 || strcmp(attr_str, "apple-secondsUntilAuthenticationsExpire") == 0) {
					policyresult = -1;
				}			
			} else if (strcmp(attr_str, "apple-authenticationAllowed") == 0) {
				policyresult = APAuthenticationAllowed(userpolicyinfodict, true, &cferr,        ^(CFArrayRef keys){ return odusers_accountpolicy_retrievedata(&policyData, keys, &dn); }, NULL) ? (int64_t) 1 : (int64_t) CFErrorGetCode(cferr);
			} else if (strcmp(attr_str, "apple-passwordChangeAllowed") == 0) {
					Filter *flist = NULL;
					if (filter ) {
						if ( filter->f_choice == LDAP_FILTER_AND ) { /* "(&(uid=user1)(userPassword=abcd))" */
							flist = filter->f_and;
						} else if ( filter->f_choice == LDAP_FILTER_EQUALITY ) {  /* "(userPassword=abcd)" */
							flist = filter;
						}
						if (flist && flist->f_choice == LDAP_FILTER_EQUALITY)  { /* (attr=value) */
							CFStringRef passcfstr = NULL;
							CFStringRef uidcfstr = NULL;
							for ( ; flist ; flist = flist->f_next) {
								if (flist->f_av_desc == slap_schema.si_ad_userPassword && flist->f_av_value.bv_val) {
									passcfstr =  CFStringCreateWithBytes(kCFAllocatorDefault, flist->f_av_value.bv_val, flist->f_av_value.bv_len, kCFStringEncodingUTF8, false);
									CFDictionarySetValue(userpolicyinfodict, kAPAttributePassword, passcfstr);
								} else if (flist->f_av_desc == slap_schema.si_ad_uid  && flist->f_av_value.bv_val) {
									uidcfstr =  CFStringCreateWithBytes(kCFAllocatorDefault, flist->f_av_value.bv_val, flist->f_av_value.bv_len, kCFStringEncodingUTF8, false);
									CFDictionarySetValue(userpolicyinfodict, kAPAttributeRecordName, uidcfstr);
								}
							}
							if (passcfstr) {
								policyresult = (int64_t) APPasswordChangeAllowed(userpolicyinfodict, &cferr,	^(CFArrayRef keys){ return odusers_accountpolicy_retrievedata(&policyData, keys, &dn); }, NULL);
							} else {
								Debug(LDAP_DEBUG_ANY, "%s: APPasswordChangeAllowed - parameter error\n", __func__, 0, 0);							
							}
							if (passcfstr) CFRelease(passcfstr);
							if (uidcfstr) CFRelease(uidcfstr);
						}
					}		
			} else if (strcmp(attr_str, "apple-willPasswordExpire") == 0) {
					if (ava && ava->aa_value.bv_val) {
						willExpireIn = strtoll(ava->aa_value.bv_val, NULL, 10);
						policyresult = (int64_t) APWillPasswordExpire(willExpireIn, userpolicyinfodict, ^(CFArrayRef keys){ return odusers_accountpolicy_retrievedata(&policyData, keys, &dn); });
						Debug(LDAP_DEBUG_ANY, "%s: APWillPasswordExpire %lld \n", __func__, willExpireIn, 0);
					}
			} else if (strcmp(attr_str, "apple-willAuthenticationsExpire") == 0) {
					if (ava && ava->aa_value.bv_val) {
						willExpireIn = strtoll(ava->aa_value.bv_val, NULL, 10);
						policyresult = (int64_t) APWillAuthenticationsExpire(willExpireIn, userpolicyinfodict, ^(CFArrayRef keys){ return odusers_accountpolicy_retrievedata(&policyData, keys, &dn); });
						Debug(LDAP_DEBUG_ANY, "%s: APWillAuthenticationsExpire %lld \n", __func__, willExpireIn, 0);
					}
			} else if (strcmp(attr_str, "apple-secondsUntilPasswordExpires") == 0) {
					policyresult = APSecondsUntilPasswordExpiration(userpolicyinfodict, ^(CFArrayRef keys){ return odusers_accountpolicy_retrievedata(&policyData, keys, &dn); });
			} else if (strcmp(attr_str, "apple-secondsUntilAuthenticationsExpire") == 0) {
					policyresult = APSecondsUntilAuthenticationExpiration(userpolicyinfodict, ^(CFArrayRef keys){ return odusers_accountpolicy_retrievedata(&policyData, keys, &dn); });
			}

			AttributeDescription *attrDesc = NULL;
			const char *text = NULL;
			if(slap_str2ad(attr_str, &attrDesc, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for %s", __PRETTY_FUNCTION__, attr_str, 0);
				goto out;
			}
			
			asprintf(&tmpstr, "%lld", policyresult);			
			attr = attr_alloc(attrDesc);
			attr->a_flags |= SLAP_ATTR_DONT_FREE_DATA;
			attr->a_vals = ch_malloc(2 * sizeof(struct berval));
			attr->a_vals[0].bv_val = tmpstr;
			attr->a_vals[0].bv_len = strlen(tmpstr);
			attr->a_vals[1].bv_len = 0;
			attr->a_vals[1].bv_val = NULL;
			attr->a_nvals = attr->a_vals;			
			
		} else {
			goto out;
		}
	} else {
		goto out;
	}

	retentry = entry_alloc();
	if(!retentry) goto out;

	retentry->e_id = NOID;
	ber_dupbv(&retentry->e_name, &op->o_req_dn);
	ber_dupbv(&retentry->e_nname, &op->o_req_ndn);

	if (attr) {
		retentry->e_attrs = attr;
	} else {
		retentry->e_attrs = NULL;
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
		Debug(LDAP_DEBUG_ANY, "%s: size limit exceeded on entry: %s", __func__, retentry->e_name.bv_val, 0);
	}
	rs->sr_entry = NULL;
	send_ldap_result(op, rs);
	if(retentry) entry_free(retentry);
	if (userpolicyinfodict) CFRelease(userpolicyinfodict);
	if (policyData) CFRelease(policyData);
	if (globalaccountpolicyGUID) CFRelease(globalaccountpolicyGUID);
	if (cferr) CFRelease(cferr);
	free(tmpstr);
	return rs->sr_err;
	
	
out:
	if(retentry) entry_free(retentry);
	if (userpolicyinfodict) CFRelease(userpolicyinfodict);
	if (policyData) CFRelease(policyData);
	if (globalaccountpolicyGUID) CFRelease(globalaccountpolicyGUID);
	if (cferr) CFRelease(cferr);
	return SLAP_CB_CONTINUE;
}

static int odusers_search_bridge_accountpolicy(Operation *op, SlapReply *rs) {
	OperationBuffer opbuf;
	Operation *fakeop;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	Entry *retentry = NULL;
	char guidstr[37];
	Attribute *pwsPolicyAttr = NULL;
	
	dnNormalize( 0, NULL, NULL, &op->o_req_dn, &op->o_req_ndn, NULL );
	e = odusers_copy_authdata(&op->o_req_ndn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __func__, op->o_req_ndn.bv_val, 0);
		goto out;
	}

	/* translate  PasswordServer 'apple-user-passwordpolicy' queries from new 'apple-accountpolicy' format */
	CFDictionaryRef accountpolicy = odusers_copy_accountpolicy_fromentry(e);
	if (accountpolicy) {
		CFDictionaryRef pwsPolicy = APLegacyPoliciesWithPolicySet(accountpolicy);
		if (pwsPolicy) {
			AttributeDescription *pwsPolicydesc = NULL;
			const char *text = NULL;
			if(slap_str2ad("apple-user-passwordpolicy", &pwsPolicydesc, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for global policy", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}

			struct berval *bv = odusers_copy_dict2bv(pwsPolicy);
			CFRelease(pwsPolicy);

			pwsPolicyAttr = attr_alloc(pwsPolicydesc);
			pwsPolicyAttr->a_vals = ch_malloc(2 * sizeof(struct berval));
			pwsPolicyAttr->a_vals[0] = *bv;
			pwsPolicyAttr->a_vals[1].bv_len = 0;
			pwsPolicyAttr->a_vals[1].bv_val = NULL;
			pwsPolicyAttr->a_nvals = pwsPolicyAttr->a_vals;			
		}
		CFRelease(accountpolicy);
	}

	retentry = entry_alloc();
	if(!retentry) goto out;

	retentry->e_id = NOID;
	retentry->e_name = op->o_req_dn;
	retentry->e_nname = op->o_req_ndn;

	if (pwsPolicyAttr) {
		retentry->e_attrs = pwsPolicyAttr;
	} else {
		retentry->e_attrs = NULL;
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
		Debug(LDAP_DEBUG_ANY, "%s: size limit exceeded on entry: %s", __func__, retentry->e_name.bv_val, 0);
	}
	rs->sr_entry = NULL;
	send_ldap_result(op, rs);
	if(e) entry_free(e);
	return rs->sr_err;
	
	
out:
	if(e) entry_free(e);
	return SLAP_CB_CONTINUE;

}

static int odusers_search_effective_userpolicy(Operation *op, SlapReply *rs) {
	Attribute *effective = NULL;
	const char *text = NULL;
	CFDictionaryRef effectivedict = odusers_copy_effectiveuserpoldict(&op->o_req_ndn);
	if(!effectivedict) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve effective policy for %s\n", __func__, op->o_req_ndn.bv_val, 0);
		goto out;
	}

	Entry *retentry = entry_alloc();
	if(!retentry) goto out;

	retentry->e_id = NOID;
	retentry->e_name = op->o_req_dn;
	retentry->e_nname = op->o_req_ndn;

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
	ch_free(bv);
	effective->a_vals[1].bv_len = 0;
	effective->a_vals[1].bv_val = NULL;
	effective->a_nvals = effective->a_vals;

	retentry->e_attrs = effective;

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
//	if (retentry) entry_free(retentry);
	return SLAP_CB_CONTINUE;
}
static int odusers_search_global_accountpolicy(Operation *op, SlapReply *rs) {
	Attribute *attr = odusers_copy_attr("cn=access,cn=authdata", "apple-accountpolicy");

	Entry *retentry = entry_alloc();
	if(!retentry) goto out;

	retentry->e_id = NOID;
	retentry->e_name = op->o_req_dn;
	retentry->e_nname = op->o_req_ndn;
	retentry->e_attrs = attr;

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
	if(attr) attr_free(attr);
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
	if(global) attr_free(global);
	return SLAP_CB_CONTINUE;
}

static int odusers_search_pwsprefs(Operation *op, SlapReply *rs) {
	OperationBuffer opbuf;
	Operation *fakeop;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a;
	char *saslrealm = NULL;
	int ret = 0;

	CFMutableDictionaryRef prefs = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	int zeroint = 0;
	CFNumberRef zero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zeroint);
	CFDictionaryAddValue(prefs, CFSTR("BadTrialDelay"), zero);
	CFRelease(zero);

	CFDictionaryAddValue(prefs, CFSTR("ExternalCommand"), CFSTR("Disabled"));

	CFMutableArrayRef listeners = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFArrayAppendValue(listeners, CFSTR("Ethernet"));
	CFArrayAppendValue(listeners, CFSTR("Local"));
	CFArrayAppendValue(listeners, CFSTR("UNIX Domain Socket"));
	CFDictionaryAddValue(prefs, CFSTR("ListenerInterfaces"), listeners);
	CFRelease(listeners);

	CFMutableArrayRef ports = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	int oneohsixint = 106;
	CFNumberRef oneohsix = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &oneohsixint);
	CFArrayAppendValue(ports, oneohsix);
	CFRelease(oneohsix);
	int threesixfivenineint = 3659;
	CFNumberRef threesixfivenine = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &threesixfivenineint);
	CFArrayAppendValue(ports, threesixfivenine);
	CFRelease(threesixfivenine);
	CFDictionaryAddValue(prefs, CFSTR("ListenerPorts"), ports);
	CFRelease(ports);

	int threeint = 3;
	CFNumberRef three = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &threeint);
	CFDictionaryAddValue(prefs, CFSTR("Preference File Version"), three);
	CFRelease(three);

	char *suffix = op->o_req_ndn.bv_val + strlen("cn=passwordserver,cn=config,");
	CFArrayRef mechs = odusers_copy_enabledmechs(suffix);
	if(mechs) {
		int i;
		CFMutableDictionaryRef pluginstates = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		for(i = 0; i < CFArrayGetCount(mechs); i++) {
			CFStringRef mech = CFArrayGetValueAtIndex(mechs, i);
			CFDictionaryAddValue(pluginstates, mech, CFSTR("ON"));
		}
		CFDictionaryAddValue(prefs, CFSTR("SASLPluginStates"), pluginstates);
		CFRelease(mechs);
		CFRelease(pluginstates);
	}

	CFDictionaryAddValue(prefs, CFSTR("SyncSASLPlugInList"), kCFBooleanTrue);

	saslrealm = odusers_copy_saslrealm();
	if(!saslrealm) {
		Debug(LDAP_DEBUG_ANY, "%s: unable to find sasl realm\n", __func__, 0, 0);
		goto out;
	}

	CFStringRef cfsaslrealm = CFStringCreateWithCString(kCFAllocatorDefault, saslrealm, kCFStringEncodingUTF8);
	CFDictionaryAddValue(prefs, CFSTR("SASLRealm"), cfsaslrealm);
	CFRelease(cfsaslrealm);
	ch_free(saslrealm);

	e = entry_alloc();
	e->e_id = NOID;
	e->e_name = op->o_req_dn;
	e->e_nname = op->o_req_ndn;
	AttributeDescription *desc = NULL;
	const char *text;
	if(slap_str2ad("apple-xmlplist", &desc, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for apple-xmlplist\n", __func__, 0, 0);
		goto out;
	}
	struct berval *bv = odusers_copy_dict2bv(prefs);
	if(!bv) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to convert prefs to berval\n", __func__, 0, 0);
		goto out;
	}
	e->e_attrs = attr_alloc(desc);
	e->e_attrs->a_vals = ch_malloc(2 * sizeof(struct berval));
	e->e_attrs->a_vals[0] = *bv;
	e->e_attrs->a_vals[1].bv_len = 0;
	e->e_attrs->a_vals[1].bv_val = 0;
	e->e_attrs->a_nvals = e->e_attrs->a_vals;
	e->e_attrs->a_numvals = 1;
	ch_free(bv);
	
	op->ors_slimit = -1;
	rs->sr_entry = e;
	rs->sr_nentries = 1;
	rs->sr_flags = 0;
	rs->sr_ctrls = 0;
	rs->sr_operational_attrs = NULL;
	rs->sr_attrs = op->ors_attrs;
	rs->sr_err = LDAP_SUCCESS;
	rs->sr_err = send_search_entry(op, rs);
	rs->sr_entry = NULL;
	send_ldap_result(op, rs);
	ret = rs->sr_err;
	
out:
	if(prefs) CFRelease(prefs);
	return 0;
}

static int odusers_search(Operation *op, SlapReply *rs) {
	bool isaccount;

	if(!op || op->o_req_ndn.bv_len == 0) return SLAP_CB_CONTINUE;
	if(!op->ors_attrs) return SLAP_CB_CONTINUE;
	if(strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len) != NULL) return SLAP_CB_CONTINUE;

	isaccount = odusers_isaccount(op);
	
	if(isaccount && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-user-passwordpolicy", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_bridge_accountpolicy(op, rs);
	} else if(strncmp(op->ors_attrs[0].an_name.bv_val, "apple-authenticationAllowed", op->ors_attrs[0].an_name.bv_len) == 0 ||
			   strncmp(op->ors_attrs[0].an_name.bv_val, "apple-passwordChangeAllowed", op->ors_attrs[0].an_name.bv_len) == 0 ||
			   strncmp(op->ors_attrs[0].an_name.bv_val, "apple-willPasswordExpire", op->ors_attrs[0].an_name.bv_len) == 0 ||
			   strncmp(op->ors_attrs[0].an_name.bv_val, "apple-willAuthenticationsExpire", op->ors_attrs[0].an_name.bv_len) == 0 ||
			   strncmp(op->ors_attrs[0].an_name.bv_val, "apple-secondsUntilPasswordExpires", op->ors_attrs[0].an_name.bv_len) == 0 ||
			   strncmp(op->ors_attrs[0].an_name.bv_val, "apple-secondsUntilAuthenticationsExpire", op->ors_attrs[0].an_name.bv_len) == 0 ) {
		return odusers_search_accountpolicy_proxy(op, rs, op->ors_attrs[0].an_name.bv_val);
	} else if(isaccount && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-user-passwordpolicy-effective", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_effective_userpolicy(op, rs);
	} else if(isaccount && strncmp(op->ors_attrs[0].an_name.bv_val, "draft-krbPrincipalAliases", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_bridge_authdata(op, rs, "draft-krbPrincipalAliases");
	} else if(isaccount && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-accountpolicy", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_bridge_authdata(op, rs, "apple-accountpolicy");	
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-accountpolicy", op->ors_attrs[0].an_name.bv_len) == 0) {
		odusers_search_global_accountpolicy(op, rs);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-user-passwordpolicy", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_globalpolicy(op, rs);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, "cn=passwordserver,cn=config", strlen(kDirservConfigName)) == 0) && strncmp(op->ors_attrs[0].an_name.bv_val, "apple-xmlplist", op->ors_attrs[0].an_name.bv_len) == 0) {
		return odusers_search_pwsprefs(op, rs);
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
	a->a_vals[0].bv_val = ch_strdup("Apple") ;
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
	a->a_vals[0].bv_val = ch_strdup(PROJVERSION);
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
	a->a_vals[0].bv_val = ch_strdup(TGT_OS_VERSION);
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

	/* If this is a result to an ADD that is a user/computer account */
	if(op->o_tag == LDAP_REQ_ADD && (strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len) == NULL) && odusers_isaccount(op)) {
		Debug(LDAP_DEBUG_ANY, "%s: processing response to add of %s\n", __func__, op->o_req_dn.bv_val, 0);

		Attribute *a = NULL;
		OpExtraOD *oe = NULL;
		OpExtra *oex = NULL;
		a = attr_find( op->ora_e->e_attrs, slap_schema.si_ad_entryUUID );
		if(!a) {
			Debug(LDAP_DEBUG_ANY, "%s: Could not find entryUUID\n", __func__, 0, 0);
		} else {
			Debug(LDAP_DEBUG_ANY, "%s: entryUUID %s\n", __func__, a->a_vals[0].bv_val, 0);
		}

		LDAP_SLIST_FOREACH(oex, &op->o_extra, oe_next) {
			if(oex->oe_key == ODUSERS_EXTRA_KEY) {
				char uuidstr[37];
				oe = (OpExtraOD*)oex;
				
				uuidstr[sizeof(uuidstr)-1] = '\0';
				uuid_unparse_lower(oe->uuid, uuidstr);
				Debug(LDAP_DEBUG_ANY, "%s: Found uuid: %s\n", __func__, uuidstr, 0);
				if (rs->sr_err == LDAP_SUCCESS && rs->sr_type == REP_RESULT) { // add authdata on successful creation
					odusers_add_authdata(op, rs, oe->uuid);
				} 
				LDAP_SLIST_REMOVE(&op->o_extra, &oe->oe, OpExtra, oe_next);
				free(oe);
				break;
			}
		}

		return SLAP_CB_CONTINUE;
	}

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

static int odusers_modify_bridge_authdata(Operation *op, SlapReply *rs) {
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
		Debug(LDAP_DEBUG_ANY, "%s: Unable to modify authdata attribute: %s (%d)\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, rs->sr_err);
		goto out;
	}

	send_ldap_result(op, rs);
	return rs->sr_err;

out:
	return SLAP_CB_CONTINUE;
}

static int odusers_modify_userpolicy(Operation *op, SlapReply *rs) {
	OperationBuffer opbuf = {0};
	Operation *fakeop;
	Connection conn = {0};
	Entry *e;
	Modifications *m = op->orm_modlist;
	CFDictionaryRef userdict = NULL;
	CFDictionaryRef accountPolicyDict = NULL;
	
	e = odusers_copy_authdata(&op->o_req_ndn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __func__, op->o_req_ndn.bv_val, 0);
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

	if(m && m->sml_numvals) {
		userdict = CopyPolicyToDict(m->sml_values[0].bv_val, m->sml_values[0].bv_len);
		if(!userdict) {
			Debug(LDAP_DEBUG_ANY, "%s: Could not convert user policy to dict", __func__, 0, 0);
		} else {
			CFNumberRef isAdminUser = CFDictionaryGetValue(userdict, CFSTR("isAdminUser"));
			unsigned int tmpint = 0;
			if(isAdminUser) {
				CFNumberGetValue(isAdminUser, kCFNumberIntType, &tmpint);
				if(tmpint) {
					odusers_joingroup("admin", &op->o_req_dn, 0);
				} else {
					odusers_joingroup("admin", &op->o_req_dn, 1);
				}
			}

			CFNumberRef isSessionKeyAgent = CFDictionaryGetValue(userdict, CFSTR("isSessionKeyAgent"));
			tmpint = 0;
			if(isSessionKeyAgent) {
				CFNumberGetValue(isSessionKeyAgent, kCFNumberIntType, &tmpint);
				if(tmpint) {
					odusers_joingroup("com.apple.access_sessionkey", &op->o_req_dn, 0);
				} else {
					odusers_joingroup("com.apple.access_sessionkey", &op->o_req_dn, 1);
				}
			}

			CFNumberRef isDisabled = CFDictionaryGetValue(userdict, CFSTR("isDisabled"));
			tmpint = 0;
			if(isDisabled) {
				CFNumberGetValue(isDisabled, kCFNumberIntType, &tmpint);
				if(tmpint) {
					odusers_joingroup("com.apple.access_disabled", &op->o_req_dn, 0);
				} else {
					odusers_joingroup("com.apple.access_disabled", &op->o_req_dn, 1);
				}
			}
			
			accountPolicyDict = APPolicySetWithLegacyPolicies(userdict);
			if (accountPolicyDict) {
				Debug(LDAP_DEBUG_ANY, "%s: APPolicySetWithLegacyPolicies", __func__, 0, 0);	
				odusers_accountpolicy_set(&op->o_req_ndn, e, accountPolicyDict);	
				CFRelease(accountPolicyDict);
			}			
			CFRelease(userdict);
		}
	}

	// Use the frontend DB for the modification so we go through the syncrepl	
	// overlay and our change gets replicated.
	fakeop->o_bd = frontendDB;

	slap_op_time(&op->o_time, &op->o_tincr);

	fakeop->o_bd->be_modify(fakeop, rs);
	if(rs->sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to modify authdata attribute: %s (%d)\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, rs->sr_err);
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
	Modifications *m = op->orm_modlist;
	CFDictionaryRef globaldict = NULL;

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

	if(m && m->sml_numvals) {
		globaldict = CopyPolicyToDict(m->sml_values[0].bv_val, m->sml_values[0].bv_len);
		if(!globaldict) {
			Debug(LDAP_DEBUG_ANY, "%s: Could not convert global policy to dict", __func__, 0, 0);
		} else {
			/* translate apple-user-passwordpolicy to apple-accountpolicy */
			if (strncmp(m->sml_desc->ad_cname.bv_val, "apple-user-passwordpolicy", m->sml_desc->ad_cname.bv_len) == 0) {
				CFDictionaryRef accountPolicyDict = NULL;
				Attribute *passwordRequiredDateAttr = odusers_copy_passwordRequiredDate();
				
				if(passwordRequiredDateAttr && passwordRequiredDateAttr->a_numvals && passwordRequiredDateAttr->a_nvals[0].bv_len) {
					time_t tmptime = 0;
					struct tm tmptm = {0};
					strptime(passwordRequiredDateAttr->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
					tmptime = timegm(&tmptm);
					CFNumberRef cftmptime = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &tmptime);
					CFDictionarySetValue(globaldict, CFSTR("passwordRequiredDate"), cftmptime);
					CFRelease(cftmptime);
				}
				
				accountPolicyDict = APPolicySetWithLegacyPolicies(globaldict);
				if (accountPolicyDict) {
					Debug(LDAP_DEBUG_ANY, "%s: APPolicySetWithLegacyPolicies - global", __func__, 0, 0);	
					AttributeDescription *appleAccountPolicyAD = NULL;
					const char *text = NULL;
					struct berval *policy_bv = NULL;
					
					if(slap_str2ad("apple-accountpolicy", &appleAccountPolicyAD, &text) != 0) {
						Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of apple-accountpolicy attribute", __PRETTY_FUNCTION__, 0, 0);
						goto out;
					}

					m = (Modifications *) ch_malloc(sizeof(Modifications));	
	
					policy_bv = odusers_copy_dict2bv(accountPolicyDict);
	
					m->sml_op = LDAP_MOD_REPLACE;
					m->sml_flags = 0;
					m->sml_type = appleAccountPolicyAD->ad_cname;
					m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
					m->sml_values[0].bv_val = policy_bv->bv_val;
					m->sml_values[0].bv_len = policy_bv->bv_len;
					m->sml_values[1].bv_val = NULL;
					m->sml_values[1].bv_len = 0;
					m->sml_nvalues = NULL;
					m->sml_numvals = 1;
					m->sml_desc = appleAccountPolicyAD;
					m->sml_next = fakeop->orm_modlist;
					fakeop->orm_modlist = m;
					
					CFRelease(accountPolicyDict);
				}							
			}			
			CFRelease(globaldict);
		}
	}

	// Use the frontend DB for the modification so we go through the syncrepl	
	// overlay and our change gets replicated.
	fakeop->o_bd = frontendDB;

	slap_op_time(&op->o_time, &op->o_tincr);

	fakeop->o_bd->be_modify(fakeop, rs);
	if(rs->sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to modify user policy: %s (%d)\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, rs->sr_err);
		goto out;
	}
	
	APInvalidateCacheForPolicySet(NULL);
	
	send_ldap_result(op, rs);
	return rs->sr_err;

out:
	return SLAP_CB_CONTINUE;
}

static int odusers_enforce_admin(Operation *op) {
	CFDictionaryRef policy = NULL;
	int ret = -1;

	if((op->o_conn->c_listener->sl_url.bv_len == strlen("ldapi://%2Fvar%2Frun%2Fldapi")) && (strncmp(op->o_conn->c_listener->sl_url.bv_val, "ldapi://%2Fvar%2Frun%2Fldapi", op->o_conn->c_listener->sl_url.bv_len) == 0)) {
		ret = 0;
		goto out;
	}

	policy = odusers_copy_effectiveuserpoldict(&op->o_conn->c_dn);
	if(!policy) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve effective policy for %s\n", __func__, op->o_conn->c_dn.bv_val, 0);
		return SLAP_CB_CONTINUE;
	}
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

static int odusers_rename(Operation *op, SlapReply *rs) {
	bool isaccount;

	if(!op || op->o_req_ndn.bv_len == 0) return SLAP_CB_CONTINUE;

	if(strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len) != NULL) return SLAP_CB_CONTINUE;

	isaccount = odusers_isaccount(op);
	if(!isaccount) {
		return SLAP_CB_CONTINUE;
	}

	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	Entry *e = NULL;
	const char *text = NULL;
	SlapReply rs2 = {REP_RESULT};
	Modifications *m = NULL;
	char *newname = NULL;
	char *newnameminusdollar = NULL;
	char *oldname = NULL;
	char *princname = NULL;
	char *oldprincname = NULL;
	char *realmname = NULL;

	OperationBuffer useropbuf = {0};
	Operation *userfakeop = NULL;
	Connection userconn = {0};
	Entry *usere = NULL;
	SlapReply userrs = {REP_RESULT};
	Modifications *userm = NULL;
	Attribute *aas = NULL;
	Attribute *altsec = NULL;
	Attribute *aliases = NULL;
	int i;
	Entry *authe = NULL;
	AccessControlState acl_state = ACL_STATE_INIT;

	realmname = odusers_copy_krbrealm(op);
	if(!realmname) {
		Debug(LDAP_DEBUG_ANY, "%s: Can't locate kerberos realm", __func__, 0, 0);
		goto out;
	}

	newname = strchr(op->orr_newrdn.bv_val, '=');
	if(!newname) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not determine new name from %s\n", __func__, op->orr_newrdn.bv_val, 0);
		goto out;
	}
	newname++;

	e = odusers_copy_authdata(&op->o_req_ndn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, op->o_req_ndn.bv_val, 0);
		goto out;
	}

	oldname = odusers_copy_recname(op);
	if(!oldname) {
		Debug(LDAP_DEBUG_ANY, "%s: could not find recname of %s\n", __PRETTY_FUNCTION__, op->o_req_ndn.bv_val, 0);
		goto out;
	}

	asprintf(&princname, "%s@%s", newname, realmname);
	asprintf(&oldprincname, "%s@%s", oldname, realmname);

	if(!uidAD && slap_str2ad("uid", &uidAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of uid attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	if(!krbAD && slap_str2ad("KerberosPrincName", &krbAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of KerberosPrincName attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	if(!draftkrbAD && slap_str2ad("draft-krbPrincipalName", &draftkrbAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of draft-PrincipalName attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	if(!draftkrbAliasesAD && slap_str2ad("draft-krbPrincipalAliases", &draftkrbAliasesAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of draft-krbPrincipalAliases attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	if(!aaAD && slap_str2ad("authAuthority", &aaAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of authAuthority attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	if(!altsecAD && slap_str2ad("altSecurityIdentities", &altsecAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of altSecurityIdentities attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = op->o_dn;
	fakeop->o_ndn = op->o_ndn;
	fakeop->o_req_dn = e->e_name;
	fakeop->o_req_ndn = e->e_nname;
	fakeop->o_tag = LDAP_REQ_MODIFY;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->o_bd = frontendDB;
	fakeop->orm_modlist = m = (Modifications *) ch_malloc(sizeof(Modifications));
	m->sml_op = LDAP_MOD_REPLACE;
	m->sml_flags = 0;
	m->sml_type = uidAD->ad_cname;
	m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	m->sml_values[0].bv_val = ch_strdup(newname);
	m->sml_values[0].bv_len = strlen(newname);
	m->sml_values[1].bv_val = NULL;
	m->sml_values[1].bv_len = 0;
	m->sml_numvals = 1;
	m->sml_nvalues = NULL;
	m->sml_desc = uidAD;

	m->sml_next = (Modifications *) ch_malloc(sizeof(Modifications));
	m = m->sml_next;

	m->sml_op = LDAP_MOD_REPLACE;
	m->sml_flags = 0;
	m->sml_type = krbAD->ad_cname;
	m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	m->sml_values[0].bv_val = ch_strdup(newname);
	m->sml_values[0].bv_len = strlen(newname);
	m->sml_values[1].bv_val = NULL;
	m->sml_values[1].bv_len = 0;
	m->sml_numvals = 1;
	m->sml_nvalues = NULL;
	m->sml_desc = krbAD;
	m->sml_next = (Modifications *) ch_malloc(sizeof(Modifications));
	m = m->sml_next;

	m->sml_op = LDAP_MOD_REPLACE;
	m->sml_flags = 0;
	m->sml_type = draftkrbAD->ad_cname;
	m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	m->sml_values[0].bv_val = ch_strdup(princname);
	m->sml_values[0].bv_len = strlen(princname);
	m->sml_values[1].bv_val = NULL;
	m->sml_values[1].bv_len = 0;
	m->sml_numvals = 1;
	m->sml_nvalues = NULL;
	m->sml_desc = draftkrbAD;
	m->sml_next = NULL;


	// Update user record now
	connection_fake_init2(&userconn, &useropbuf, ldap_pvt_thread_pool_context(), 0);
	userfakeop = &useropbuf.ob_op;
	userfakeop->o_dn = op->o_dn;
	userfakeop->o_ndn = op->o_ndn;
	userfakeop->o_req_dn = op->o_dn;
	userfakeop->o_req_ndn = op->o_ndn;
	authe = odusers_copy_entry(userfakeop);
	userfakeop->o_req_dn = op->o_req_dn;
	userfakeop->o_req_ndn = op->o_req_ndn;
	usere = odusers_copy_entry(userfakeop);
	if(!usere) {
		Debug(LDAP_DEBUG_ANY, "%s: No user entry associated with %s\n", __PRETTY_FUNCTION__, userfakeop->o_req_ndn.bv_val, 0);
		goto out;
	}
	userfakeop->o_bd = frontendDB;
	userfakeop->o_tag = LDAP_REQ_MODIFY;
	userfakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	userfakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	userfakeop->orm_modlist = userm = (Modifications *) ch_malloc(sizeof(Modifications));
	aas = attrs_find(usere->e_attrs, aaAD);
	if(!aas) {
		Debug(LDAP_DEBUG_ANY, "%s: User has no authauthorities: %s\n", __func__, userfakeop->o_req_dn.bv_val, 0);
		ch_free(userm);
		userm = NULL;
		goto out;
	}
	userm->sml_op = LDAP_MOD_REPLACE;
	userm->sml_flags = 0;
	userm->sml_type = aaAD->ad_cname;
	userm->sml_values = (struct berval*) ch_malloc((aas->a_numvals+1) * sizeof(struct berval));
	for(i = 0; i < aas->a_numvals; i++) {
		if(strnstr(aas->a_vals[i].bv_val, ";Kerberosv5;", aas->a_vals[i].bv_len) != NULL) {
			userm->sml_values[i].bv_len = asprintf(&userm->sml_values[i].bv_val, ";Kerberosv5;;%s@%s;%s;", newname, realmname, realmname);
		} else {
			ber_dupbv(&userm->sml_values[i], &aas->a_vals[i]);
		}
	}
	userm->sml_values[i].bv_val = NULL;
	userm->sml_values[i].bv_len = 0;
	userm->sml_numvals = i;
	userm->sml_nvalues = NULL;
	userm->sml_desc = aaAD;
	userm->sml_next = NULL;

	altsec = attrs_find(usere->e_attrs, altsecAD);
	if(altsec) {
		userm->sml_next = (Modifications *) ch_malloc(sizeof(Modifications));
		userm = userm->sml_next;
		userm->sml_op = LDAP_MOD_REPLACE;
		userm->sml_flags = 0;
		userm->sml_type = altsecAD->ad_cname;
		userm->sml_values = (struct berval*) ch_malloc((altsec->a_numvals+1) * sizeof(struct berval));
		for(i = 0; i < altsec->a_numvals; i++) {
			if(strnstr(altsec->a_vals[i].bv_val, oldprincname, altsec->a_vals[i].bv_len) != NULL) {
				userm->sml_values[i].bv_len = asprintf(&userm->sml_values[i].bv_val, "Kerberos:%s", princname);
			} else {
				ber_dupbv(&userm->sml_values[i], &altsec->a_vals[i]);
			}
		}
		userm->sml_values[i].bv_val = NULL;
		userm->sml_values[i].bv_len = 0;
		userm->sml_numvals = i;
		userm->sml_nvalues = NULL;
		userm->sml_desc = altsecAD;
		userm->sml_next = NULL;
	}

	aliases = attrs_find(e->e_attrs, draftkrbAliasesAD);
	if(aliases) {
		m->sml_next = (Modifications *) ch_malloc(sizeof(Modifications));
		m = m->sml_next;
		m->sml_op = LDAP_MOD_DELETE;
		m->sml_flags = 0;
		m->sml_type = draftkrbAliasesAD->ad_cname;
		m->sml_values = (struct berval*) ch_malloc((aliases->a_numvals+1) * sizeof(struct berval));
		for(i = 0; i < aliases->a_numvals; i++) {
			ber_dupbv(&m->sml_values[i], &aliases->a_vals[i]);
		}
		m->sml_values[i].bv_val = NULL;
		m->sml_values[i].bv_len = 0;
		m->sml_numvals = i;
		m->sml_nvalues = NULL;
		m->sml_desc = draftkrbAliasesAD;
		m->sml_next = NULL;

		newnameminusdollar = ch_strdup(newname);
		newnameminusdollar[strlen(newnameminusdollar)-1] = '\0';

		m->sml_next = (Modifications *) ch_malloc(sizeof(Modifications));
		m = m->sml_next;
		m->sml_op = LDAP_MOD_ADD;
		m->sml_flags = 0;
		m->sml_type = draftkrbAliasesAD->ad_cname;
		m->sml_values = (struct berval*) ch_malloc((aliases->a_numvals+1) * sizeof(struct berval));
		for(i = 0; i < aliases->a_numvals; i++) {
			if(strnstr(aliases->a_vals[i].bv_val, "/", aliases->a_vals[i].bv_len) != NULL) {
				char *tmpstr = ch_calloc(1, aliases->a_vals[i].bv_len+1);
				memcpy(tmpstr, aliases->a_vals[i].bv_val, aliases->a_vals[i].bv_len);
				char *freeme = strsep(&tmpstr, "/");
				m->sml_values[i].bv_len = asprintf(&m->sml_values[i].bv_val, "%s/%s@%s", freeme, newnameminusdollar, realmname);
				ch_free(freeme);
			} else {
				ber_dupbv(&m->sml_values[i], &aliases->a_vals[i]);
			}
		}
		ch_free(newnameminusdollar);
		m->sml_values[i].bv_val = NULL;
		m->sml_values[i].bv_len = 0;
		m->sml_numvals = i;
		m->sml_nvalues = NULL;
		m->sml_desc = draftkrbAliasesAD;
		m->sml_next = NULL;
	}

	bool isldapi = false;
	if((op->o_conn->c_listener->sl_url.bv_len == strlen("ldapi://%2Fvar%2Frun%2Fldapi")) && (strncmp(op->o_conn->c_listener->sl_url.bv_val, "ldapi://%2Fvar%2Frun%2Fldapi", op->o_conn->c_listener->sl_url.bv_len) == 0)) {
		isldapi = true;
	}

	if(!isldapi) {
		if(!authe) {
			Debug(LDAP_DEBUG_ANY, "%s: rename of %s attempted by unauthed user", __func__, userfakeop->o_req_ndn.bv_val, 0);
			goto out;
		}

		BackendDB *tmpbd = op->o_bd;
		op->o_bd = select_backend(&op->o_req_ndn, 1);
		userrs.sr_err = access_allowed(op, authe, slap_schema.si_ad_entry, &op->o_req_ndn, ACL_WRITE, &acl_state);
		op->o_bd = tmpbd;
		if(!userrs.sr_err) {
			Debug(LDAP_DEBUG_ANY, "%s: access denied renaming %s: %d\n", __func__, userfakeop->o_req_ndn.bv_val, userrs.sr_err);
			goto out;
		}
	}

	// Actually do the updates
	userfakeop->o_bd->be_modify(userfakeop, &userrs);
	if(userrs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "Unable to rename userdata for user %s: %d %s\n", userfakeop->o_req_ndn.bv_val, userrs.sr_err, userrs.sr_text);
		goto out;
	}

	fakeop->o_bd->be_modify(fakeop, &rs2);
	if(rs2.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "Unable to rename authdata for user %s: %d %s\n", fakeop->o_req_ndn.bv_val, rs2.sr_err, rs2.sr_text);
		goto out;
	}

out:
	if(e) entry_free(e);
	if(usere) entry_free(usere);
//	if(authe) entry_free(authe);
	if(m) slap_mods_free(fakeop->orm_modlist, 1);
	if(userm) slap_mods_free(userfakeop->orm_modlist, 1);
	ch_free(realmname);
	free(princname);
	free(oldprincname);
	ch_free(oldname);

	return SLAP_CB_CONTINUE;
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
			Debug(LDAP_DEBUG_ANY, "%s: no admin privs while attempting policy modification for %s\n", __func__, op->o_req_ndn.bv_val, 0);
			send_ldap_error(op, rs, LDAP_INSUFFICIENT_ACCESS, "policy modification not permitted");
			return rs->sr_err;
		}

		return odusers_modify_userpolicy(op, rs);
	} else if(isaccount && (strncmp(m->sml_desc->ad_cname.bv_val, "draft-krbPrincipalAliases", m->sml_desc->ad_cname.bv_len) == 0 ||
							strncmp(m->sml_desc->ad_cname.bv_val, "apple-accountpolicy", m->sml_desc->ad_cname.bv_len) == 0)) {
		if((odusers_enforce_admin(op) != 0) && (ber_bvcmp(&op->o_ndn, &op->o_req_ndn) != 0)) {
			send_ldap_error(op, rs, LDAP_INSUFFICIENT_ACCESS, "No access");
			rs->sr_err = LDAP_INSUFFICIENT_ACCESS;
			return rs->sr_err;
		}

		return odusers_modify_bridge_authdata(op, rs);
	} else if(!isaccount && (strncmp(op->o_req_ndn.bv_val, kDirservConfigName, strlen(kDirservConfigName)) == 0) 
						 && (strncmp(m->sml_desc->ad_cname.bv_val, "apple-user-passwordpolicy", m->sml_desc->ad_cname.bv_len) == 0 ||
						 	 strncmp(m->sml_desc->ad_cname.bv_val, "apple-accountpolicy", m->sml_desc->ad_cname.bv_len) == 0)) {
		if(odusers_enforce_admin(op) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: no admin privs while attempting global policy modification\n", __func__, 0, 0);
			send_ldap_error(op, rs, LDAP_INSUFFICIENT_ACCESS, "global policy modification not permitted");
			return rs->sr_err;
		}

		return odusers_modify_globalpolicy(op, rs);
	}

	return SLAP_CB_CONTINUE;
}

/* Returns a newly allocated copy of the pws pubkey attribute.
 * Caller is responsible for free()'ing the returned result.
 */
static char *odusers_copy_pwspubkey(Operation *op) {
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	AttributeDescription *pubkeyAD = NULL;
	const char *text = NULL;
	static char *savedkey = NULL;
	char *ret = NULL;
	
	if(savedkey) {
		return ch_strdup(savedkey);
	}

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = op->o_dn;
	fakeop->o_ndn = op->o_ndn;
	fakeop->o_req_dn.bv_len = strlen("cn=authdata");
	fakeop->o_req_dn.bv_val = "cn=authdata";
	fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_TRACE, "%s: No entry associated with cn=authdata\n", __func__, 0, 0);
		goto out;
	}
	
	if(slap_str2ad("PWSPublicKey", &pubkeyAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for authGUID", __func__, 0, 0);
		goto out;
	}

	a = attr_find(e->e_attrs, pubkeyAD);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate PWSPublicKey attribute", __func__, 0, 0);
		goto out;
	}

	ret = ch_calloc(1, a->a_vals[0].bv_len + 1);
	if(!ret) goto out;
	memcpy(ret, a->a_vals[0].bv_val, a->a_vals[0].bv_len);
	savedkey = ch_strdup(ret);

out:
	if(e) entry_free(e);
	return ret;
}

/* takes a uuid and converts it to PWS slot representation of
 * 0xXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
 * appending a NUL byte.
 * Caller is responsible for allocation of string.
 */
static void odusers_uuid_to_pwsslot(uuid_t uuid, char *string) {
	char uuidstr[37];
	int i, n;

	uuidstr[sizeof(uuidstr)-1] = '\0';
	uuid_unparse_lower(uuid, uuidstr);
	string[0] = '0';
	string[1] = 'x';
	for(i = 0, n = 2; n < 34 && i < 36; i++) {
		if( uuidstr[i] != '-' ) {
			string[n] = uuidstr[i];
			n++;
		}
	}
	string[n] = '\0';
	return;
}

static int odusers_add_authdata(Operation *op, SlapReply *rs, uuid_t newuuid) {
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	Attribute *attriter = NULL;
	Attribute *entryUUID = NULL;
	AttributeDescription *authGUIDAD = NULL;
	AttributeDescription *objectClassAD = NULL;
	const char *text = NULL;
	uuid_string_t uuidstr;
	SlapReply rs2 = {REP_RESULT};
	char *authauthority = NULL;
	char slotid[37];
	char *recname = NULL;
	char *realm = NULL;
	bool iscomputer = false;

	uuid_unparse_lower(newuuid, uuidstr);

	recname = odusers_copy_recname(op);
	if(!recname) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not determine record name", __func__, 0, 0);
		goto out;
	}

	if(strnstr(op->o_req_dn.bv_val, "cn=computer", op->o_req_dn.bv_len) != NULL) {
		iscomputer = true;
	}

	realm = odusers_copy_krbrealm(op);
	
	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = op->o_dn;
	fakeop->o_ndn = op->o_ndn;
	fakeop->o_req_dn.bv_len = asprintf(&fakeop->o_req_dn.bv_val, "authGUID=%s,cn=users,cn=authdata", uuidstr);
	fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_tag = LDAP_REQ_ADD;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	
	if(slap_str2ad("authGUID", &authGUIDAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for authGUID", __func__, 0, 0);
		goto out;
	}
	if(slap_str2ad("objectClass", &objectClassAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for objectClass", __func__, 0, 0);
		goto out;
	}

	a = attr_alloc(authGUIDAD);
	if(!a) goto out;
	a->a_vals = ch_calloc(2, sizeof(struct berval));
	if(!a->a_vals) goto out;
	a->a_vals[0].bv_len = strlen(uuidstr);
	a->a_vals[0].bv_val = ch_calloc(1, a->a_vals[0].bv_len+1);
	strncpy(a->a_vals[0].bv_val, uuidstr, a->a_vals[0].bv_len);
	a->a_nvals = a->a_vals;
	a->a_numvals = 1;

	a->a_next = attr_alloc(objectClassAD);
	if(!a->a_next) goto out;
	attriter = a->a_next;
	attriter->a_vals = ch_calloc(2, sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	attriter->a_vals[0].bv_len = strlen("pwsAuthdata");
	attriter->a_vals[0].bv_val = ch_strdup("pwsAuthdata");
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 1;

	if(recname) {
		AttributeDescription *uidAD = NULL;
		if(slap_str2ad("uid", &uidAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for uid", __func__, 0, 0);
			goto out;
		}
		attriter->a_next = attr_alloc(uidAD);
		if(!attriter->a_next) goto out;
		attriter = attriter->a_next;
		attriter->a_vals = ch_calloc(2, sizeof(struct berval));
		if(!attriter->a_vals) goto out;
		attriter->a_vals[0].bv_len = strlen(recname);
		attriter->a_vals[0].bv_val = ch_strdup(recname);
		attriter->a_nvals = attriter->a_vals;
		attriter->a_numvals = 1;

		AttributeDescription *princnameAD = NULL;
		if(slap_str2ad("KerberosPrincName", &princnameAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for KerberosPrincName", __func__, 0, 0);
			goto out;
		}
		attriter->a_next = attr_alloc(princnameAD);
		if(!attriter->a_next) goto out;
		attriter = attriter->a_next;
		attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
		if(!attriter->a_vals) goto out;
		attriter->a_vals[0].bv_len = strlen(recname);
		attriter->a_vals[0].bv_val = ch_strdup(recname);
		attriter->a_vals[1].bv_len = 0;
		attriter->a_vals[1].bv_val = NULL;
		attriter->a_nvals = attriter->a_vals;
		attriter->a_numvals = 1;
		attriter->a_flags = 0;
	}

	AttributeDescription *creationDateAD = NULL;
	struct tm tmtime;
	time_t now;
	if(slap_str2ad("creationDate", &creationDateAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for creationDate", __func__, 0, 0);
		goto out;
	}
	now = time(NULL);
	gmtime_r(&now, &tmtime);
	attriter->a_next = attr_alloc(creationDateAD);
	if(!attriter->a_next) goto out;
	attriter = attriter->a_next;
	attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	attriter->a_vals[0].bv_val = ch_calloc(1, 256);
	attriter->a_vals[0].bv_len = strftime(attriter->a_vals[0].bv_val, 256, "%Y%m%d%H%M%SZ", &tmtime);
	attriter->a_vals[1].bv_len = 0;
	attriter->a_vals[1].bv_val = NULL;
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 1;
	attriter->a_flags = 0;

	AttributeDescription *disablereasonAD = NULL;
	if(slap_str2ad("disableReason", &disablereasonAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for disableReason", __func__, 0, 0);
		goto out;
	}
	attriter->a_next = attr_alloc(disablereasonAD);
	if(!attriter->a_next) goto out;
	attriter = attriter->a_next;
	attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	attriter->a_vals[0].bv_len = strlen("none");
	attriter->a_vals[0].bv_val = ch_strdup("none");
	attriter->a_vals[1].bv_len = 0;
	attriter->a_vals[1].bv_val = NULL;
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 1;
	attriter->a_flags = 0;

	AttributeDescription *adminGroupsAD = NULL;
	if(slap_str2ad("adminGroups", &adminGroupsAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for adminGroups", __func__, 0, 0);
		goto out;
	}
	attriter->a_next = attr_alloc(adminGroupsAD);
	if(!attriter->a_next) goto out;
	attriter = attriter->a_next;
	attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	attriter->a_vals[0].bv_len = strlen("unrestricted");
	attriter->a_vals[0].bv_val = ch_strdup("unrestricted");
	attriter->a_vals[1].bv_len = 0;
	attriter->a_vals[1].bv_val = NULL;
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 1;
	attriter->a_flags = 0;

	AttributeDescription *failedLoginAD = NULL;
	if(slap_str2ad("loginFailedAttempts", &failedLoginAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for loginFailedAttempts", __func__, 0, 0);
		goto out;
	}
	attriter->a_next = attr_alloc(failedLoginAD);
	if(!attriter->a_next) goto out;
	attriter = attriter->a_next;
	attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	attriter->a_vals[0].bv_len = strlen("0");
	attriter->a_vals[0].bv_val = ch_strdup("0");
	attriter->a_vals[1].bv_len = 0;
	attriter->a_vals[1].bv_val = NULL;
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 1;
	attriter->a_flags = 0;

	entryUUID = attr_find( op->ora_e->e_attrs, slap_schema.si_ad_entryUUID );
	if(!entryUUID) {
		Debug(LDAP_DEBUG_ANY, "%s: couldn't find entryUUID attribute in copy of %s", __func__, op->o_req_ndn.bv_val, 0);
		goto out;
	}
	
	AttributeDescription *userLinkAD = NULL;
	if(slap_str2ad("userLinkage", &userLinkAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for userLinkage", __func__, 0, 0);
		goto out;
	}
	attriter->a_next = attr_alloc(userLinkAD);
	if(!attriter->a_next) goto out;
	attriter = attriter->a_next;
	attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	attriter->a_vals[0].bv_len = entryUUID->a_vals[0].bv_len;
	attriter->a_vals[0].bv_val = ch_malloc(entryUUID->a_vals[0].bv_len+1);
	memcpy(attriter->a_vals[0].bv_val, entryUUID->a_vals[0].bv_val, entryUUID->a_vals[0].bv_len);
	attriter->a_vals[0].bv_val[entryUUID->a_vals[0].bv_len] = '\0';
	attriter->a_vals[1].bv_len = 0;
	attriter->a_vals[1].bv_val = NULL;
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 1;
	attriter->a_flags = 0;

	if(realm) {
		AttributeDescription *draftNameAD = NULL;
		if(slap_str2ad("draft-krbPrincipalName", &draftNameAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for draft-krbPrincipalName", __func__, 0, 0);
			goto out;
		}
		attriter->a_next = attr_alloc(draftNameAD);
		if(!attriter->a_next) goto out;
		attriter = attriter->a_next;
		attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
		if(!attriter->a_vals) goto out;
		char *principalName = NULL;
		asprintf(&principalName, "%s@%s", recname, realm);
		ber_str2bv(principalName, strlen(principalName), 1, &attriter->a_vals[0]);
		free(principalName);
		attriter->a_vals[1].bv_len = 0;
		attriter->a_vals[1].bv_val = NULL;
		attriter->a_nvals = attriter->a_vals;
		attriter->a_numvals = 1;
		attriter->a_flags = 0;

		AttributeDescription *draftPolicyAD = NULL;
		if(slap_str2ad("draft-krbTicketPolicy", &draftPolicyAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for draft-krbTicketPolicy", __func__, 0, 0);
			goto out;
		}
		attriter->a_next = attr_alloc(draftPolicyAD);
		if(!attriter->a_next) goto out;
		attriter = attriter->a_next;
		attriter->a_vals = ch_malloc(2 * sizeof(struct berval));
		if(!attriter->a_vals) goto out;
		if(iscomputer) {
			attriter->a_vals[0].bv_len = strlen("358");
			attriter->a_vals[0].bv_val = ch_strdup("358");
		} else {
			attriter->a_vals[0].bv_len = strlen("326");
			attriter->a_vals[0].bv_val = ch_strdup("326");
		}
		attriter->a_vals[1].bv_len = 0;
		attriter->a_vals[1].bv_val = NULL;
		attriter->a_nvals = attriter->a_vals;
		attriter->a_numvals = 1;
		attriter->a_flags = 0;
	}
	attriter->a_next = NULL;

	e = entry_alloc();
	e->e_id = NOID;
	ber_dupbv(&e->e_name, &fakeop->o_req_dn);
	ber_dupbv(&e->e_nname, &fakeop->o_req_ndn);
	e->e_attrs = a;
	
	fakeop->ora_e = e;
	fakeop->o_bd = select_backend(&fakeop->o_req_ndn, 1);
	if(!fakeop->o_bd) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate backend for %s", __func__, fakeop->o_req_ndn.bv_val, 0);
		goto out;
	}

	fakeop->o_bd->be_add(fakeop, &rs2);

out:
//	if (e) entry_free(e);
	if(fakeop && fakeop->o_req_dn.bv_val) free(fakeop->o_req_dn.bv_val);
	ch_free(realm);
	ch_free(recname);

	return 0;
}

/* Add auth authorities and altsecurityidentities to a new user record request*/
static int odusers_add_aa(Operation *op, SlapReply *rs, uuid_t newuuid) {
	Attribute *attriter = NULL;
	Attribute *newAA = NULL;
	AttributeDescription *authAuthorityAD = NULL;
	const char *text = NULL;
	uuid_string_t uuidstr;
	char *pubkey = NULL;
	char *authauthority = NULL;
	char slotid[37];
	char *recname = NULL;
	char *realm = NULL;
	char *primary_master_ip = NULL;
	bool iscomputer = false;

	recname = odusers_copy_recname(op);
	if(!recname) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not determine record name", __func__, 0, 0);
		goto out;
	}

	if(strnstr(op->o_req_dn.bv_val, "cn=computer", op->o_req_dn.bv_len) != NULL) {
		iscomputer = true;
	}

	realm = odusers_copy_krbrealm(op);

	pubkey = odusers_copy_pwspubkey(op);
	if(!pubkey) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate PWS public key", __func__, 0, 0);
		goto out;
	}

	primary_master_ip = odusers_copy_primarymasterip(op);
	if(!primary_master_ip) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate Primary Master's IP address; trying System Configuration", __func__, 0, 0);
		primary_master_ip = CopyPrimaryIPv4Address();
		if (!primary_master_ip) {
			Debug(LDAP_DEBUG_ANY, "%s: could not locate IP address in System Configuration; using 127.0.0.1 in the auth authority", __func__, 0, 0);
			primary_master_ip = ch_strdup("127.0.0.1");
		}
	}

	odusers_uuid_to_pwsslot(newuuid, slotid);

	authAuthorityAD = slap_schema.si_ad_authAuthority;
	newAA = attr_alloc(authAuthorityAD);
	if(!newAA) goto out;
	attriter = newAA;
	attriter->a_vals = ch_malloc(3 *sizeof(struct berval));
	if(!attriter->a_vals) goto out;
	if(pubkey[strlen(pubkey)-1] == '\n') {
		pubkey[strlen(pubkey)-1] = '\0'; // strip of trailing newline
	}
	char *pws_authAuthority = NULL;
	char *krb5_authAuthority = NULL;
	asprintf(&pws_authAuthority, ";ApplePasswordServer;%s,%s:%s", slotid, pubkey, primary_master_ip);
	ber_str2bv(pws_authAuthority, strlen(pws_authAuthority), 1, &attriter->a_vals[0]);
	free(pws_authAuthority);
	asprintf(&krb5_authAuthority, ";Kerberosv5;;%s@%s;%s;", recname, realm, realm);
	ber_str2bv(krb5_authAuthority, strlen(krb5_authAuthority), 1, &attriter->a_vals[1]);
	free(krb5_authAuthority);
	
	attriter->a_vals[2].bv_len = 0;
	attriter->a_vals[2].bv_val = NULL;
	attriter->a_nvals = attriter->a_vals;
	attriter->a_numvals = 2;
	attriter->a_flags = 0;

	if(!iscomputer) {
		AttributeDescription *altSecAD = NULL;
		if(slap_str2ad("altSecurityIdentities", &altSecAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for altSecurityIdentities", __func__, 0, 0);
			goto out;
		}
		attriter->a_next = attr_alloc(altSecAD);
		attriter = attriter->a_next;
		if(!attriter) goto out;
		attriter->a_vals = ch_malloc(2 *sizeof(struct berval));
		if(!attriter->a_vals) goto out;
		char * altSecurityIdentities = NULL;
		asprintf(&altSecurityIdentities, "Kerberos:%s@%s", recname, realm);
		ber_str2bv(altSecurityIdentities, strlen(altSecurityIdentities), 1, &attriter->a_vals[0]);
		free(altSecurityIdentities);
		attriter->a_vals[1].bv_len = 0;
		attriter->a_vals[1].bv_val = NULL;
		attriter->a_nvals = attriter->a_vals;
		attriter->a_numvals = 1;
		attriter->a_flags = 0;
	}
	attriter->a_next = NULL;
	
	for(attriter = op->ora_e->e_attrs; attriter->a_next; attriter = attriter->a_next);
	attriter->a_next = newAA;

out:
	ch_free(realm);
	ch_free(recname);
	ch_free(pubkey);
	ch_free(primary_master_ip);

	return 0;
}

static int odusers_add(Operation *op, SlapReply *rs) {
	bool isaccount;
	Modifications *m;

	if(!op || op->o_req_ndn.bv_len == 0) return SLAP_CB_CONTINUE;
	if(strnstr(op->o_req_ndn.bv_val, "cn=authdata", op->o_req_ndn.bv_len) != NULL) return SLAP_CB_CONTINUE;

	m = op->orm_modlist;
	if(!m) return SLAP_CB_CONTINUE;

	isaccount = odusers_isaccount(op);

	if(isaccount) {
		OpExtraOD *oe = NULL;

		oe = calloc(1, sizeof(OpExtraOD));
		if(!oe) return SLAP_CB_CONTINUE;
		oe->oe.oe_key = ODUSERS_EXTRA_KEY;
		uuid_generate_time(oe->uuid);
		LDAP_SLIST_INSERT_HEAD(&op->o_extra, &oe->oe, oe_next);

		odusers_add_aa(op, rs, oe->uuid);
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
	odusers.on_bi.bi_op_add = odusers_add;
	odusers.on_bi.bi_op_modrdn = odusers_rename;
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
