#include "portable.h"

#include <ac/string.h>
#include <ac/ctype.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"
#define __COREFOUNDATION_CFFILESECURITY__
#include <CoreFoundation/CoreFoundation.h>
#include "applehelpers.h"

static Filter generic_filter = { LDAP_FILTER_PRESENT, { 0 }, NULL };
static struct berval generic_filterstr = BER_BVC("(objectclass=*)");

static  AttributeDescription *failedLoginsAD = NULL;
static  AttributeDescription *creationDateAD = NULL;
static  AttributeDescription *lastLoginAD = NULL;
static  AttributeDescription *disableReasonAD = NULL;

// This is the generic callback function for odusers_copy_entry, which just
// returns a copy of the found Entry*
static int odusers_lookup(Operation *op, SlapReply *rs) {
	if(rs->sr_type != REP_SEARCH) return 0;
	if(rs->sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to locate entry (%d)\n", __PRETTY_FUNCTION__, rs->sr_err, 0);
		return -1;
	}

	if(rs->sr_un.sru_search.r_entry) {
		*(Entry**)(op->o_callback->sc_private) = entry_dup(rs->sr_un.sru_search.r_entry);
		return 0;
	}

	return -1;
}

// Queries for the specified DN, returns the Entry pointer.  The caller needs
// to free this Entry.
// NULL is returned on error or if nothing is found.
Entry *odusers_copy_entry(Operation *op) {
	Entry *e = NULL;
	SlapReply rs = {REP_RESULT};
	slap_callback cb = {NULL, odusers_lookup, NULL, NULL};

	if(!op) {
		Debug(LDAP_DEBUG_TRACE, "%s: no operation to perform\n", __PRETTY_FUNCTION__, 0, 0);
		return NULL;
	}

	op->o_bd = select_backend(&op->o_req_ndn, 1);
	if(!op->o_bd) {
		Debug(LDAP_DEBUG_TRACE, "%s: could not find backend for: %s\n", __PRETTY_FUNCTION__, op->o_req_ndn.bv_val, 0);
		return NULL;
	}

	generic_filter.f_desc = slap_schema.si_ad_objectClass;
	op->o_do_not_cache = 1;
	slap_op_time(&op->o_time, &op->o_tincr);
	op->o_tag = LDAP_REQ_SEARCH;
	op->ors_scope = LDAP_SCOPE_BASE;
	op->ors_deref = LDAP_DEREF_NEVER;
	op->ors_tlimit = SLAP_NO_LIMIT;
	op->ors_slimit = 1;
	op->ors_filter = &generic_filter;
	op->ors_filterstr = generic_filterstr;
	op->ors_attrs = NULL;
	cb.sc_private = &e;
	op->o_callback = &cb;

	op->o_bd->be_search(op, &rs);
	if(rs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_TRACE, "%s: Unable to locate %s (%d)\n", __PRETTY_FUNCTION__, op->o_req_ndn.bv_val, rs.sr_err);
		return NULL;
	}

	return e;
}

int odusers_remove_authdata(char *slotid) {
	OperationBuffer opbuf;
	Operation *op;
	Connection conn = {0};
	Entry *e = NULL;
	SlapReply rs = {REP_RESULT};
	slap_callback cb = {NULL, odusers_lookup, NULL, NULL};
	struct berval dn;
	int ret = -1;

	dn.bv_val = NULL;
	dn.bv_len = asprintf(&dn.bv_val, "authGUID=%s,cn=users,cn=authdata", slotid);

	memset(&opbuf, 0, sizeof(opbuf));
	memset(&conn, 0, sizeof(conn));
	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	op = &opbuf.ob_op;

	op->o_dn = op->o_ndn = op->o_req_dn = op->o_req_ndn = dn;
	op->o_bd = frontendDB;
	
	slap_op_time(&op->o_time, &op->o_tincr);
	op->o_tag = LDAP_REQ_DELETE;
	op->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	op->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	op->o_bd->be_delete(op, &rs);
	if(rs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to delete %s (%d)\n", __PRETTY_FUNCTION__, dn.bv_val, rs.sr_err);
		goto out;
	}

	ret = 0;

out:
	if(dn.bv_val) free(dn.bv_val);
	return ret;
}

int odusers_get_authguid(Entry *e, char *guidstr) {
	int ret = -1;
	// Figure out the authdata record's DN from the PWS auth authority
	AttributeDescription *aa = slap_schema.si_ad_authAuthority;
	Attribute *a = attr_find(e->e_attrs, aa);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate authAuthority attribute for: %s\n", __PRETTY_FUNCTION__, e->e_dn, 0);
		goto out;
	}

	int i;
	for(i = 0; i < a->a_numvals; i++) {
		if(memcmp(a->a_vals[i].bv_val, ";ApplePasswordServer;", 21) == 0) {
			strncpy(guidstr, a->a_vals[i].bv_val+23, 8);
			guidstr[8] = '-';
			strncpy(guidstr+9, a->a_vals[i].bv_val+31, 4);
			guidstr[13] = '-';
			strncpy(guidstr+14, a->a_vals[i].bv_val+35, 4);
			guidstr[18] = '-';
			strncpy(guidstr+19, a->a_vals[i].bv_val+39, 4);
			guidstr[23] = '-';
			strncpy(guidstr+24, a->a_vals[i].bv_val+43, 12);
			guidstr[36] = '\0';
				
			break;
		}
	}

	if(guidstr[0] == '\0') {
		Debug(LDAP_DEBUG_ANY, "%s: error parsing slotid", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	ret = 0;
out:

	return ret;
}

Entry *odusers_copy_authdata(struct berval *dn) {
	OperationBuffer opbuf;
	Connection conn;
	Operation *fakeop = NULL;
	Entry *usere = NULL;
	Entry *ret = NULL;
	char guidstr[37];
	struct berval authdn;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_ndn = *dn;
	fakeop->o_req_dn = fakeop->o_req_ndn = *dn;

	usere = odusers_copy_entry(fakeop);
	if(!usere) {
		Debug(LDAP_DEBUG_TRACE, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, 0);
		goto out;
	}

	if( odusers_get_authguid(usere, guidstr) != 0) {
		Debug(LDAP_DEBUG_TRACE, "%s: Could not locate authguid for record %s", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	entry_free(usere);
	usere = NULL;

	authdn.bv_len = asprintf(&authdn.bv_val, "authGUID=%s,cn=users,cn=authdata", guidstr);

	fakeop->o_dn = fakeop->o_ndn = fakeop->o_req_dn = fakeop->o_req_ndn = authdn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	ret = odusers_copy_entry(fakeop);
	free(authdn.bv_val);

out:
	if(usere) entry_free(usere);
	return ret;
}

Attribute *odusers_copy_globalpolicy(void) {
	OperationBuffer opbuf;
	Connection conn;
	Operation *fakeop = NULL;
	Entry *e = NULL;
	struct berval policydn;
	Attribute *ret = NULL;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	policydn.bv_val = "cn=access,cn=authdata";
	policydn.bv_len = strlen(policydn.bv_val);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_ndn = policydn;
	fakeop->o_req_dn = fakeop->o_req_ndn = policydn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	e = odusers_copy_entry(fakeop);
	if(!e) goto out;

	Attribute *a;
	for(a = e->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, "apple-user-passwordpolicy", a->a_desc->ad_cname.bv_len) == 0) {
			ret = attr_dup(a);
		}
	}
	

out:
	if(e) entry_free(e);
	return ret;
}

static CFDictionaryRef CopyPolicyToDict(const char *policyplist, int len) {
	CFDataRef xmlData = NULL;
	CFDictionaryRef ret = NULL;
	CFErrorRef err = NULL;

	xmlData = CFDataCreate(kCFAllocatorDefault, (const unsigned char*)policyplist, len);
	if(!xmlData) goto out;

	ret = (CFDictionaryRef)CFPropertyListCreateWithData(kCFAllocatorDefault, xmlData, kCFPropertyListMutableContainersAndLeaves, NULL, &err);
	if(!ret) goto out;

out:
	if(xmlData) CFRelease(xmlData);
	return ret;
}

static void MergePolicyIntValue(CFDictionaryRef global, CFDictionaryRef user, CFMutableDictionaryRef merged, CFStringRef policy) {
	unsigned int tmpbool = 0;
	CFNumberRef anumber = CFDictionaryGetValue(user, policy);
	if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpbool);
	if(!tmpbool) {
		anumber = CFDictionaryGetValue(global, policy);
		if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpbool);
		if(tmpbool) {
			CFDictionarySetValue(merged, policy, anumber);
		}
	}
	return;
}

static CFMutableDictionaryRef CopyEffectivePolicy(CFDictionaryRef global, CFDictionaryRef user) {
	CFMutableDictionaryRef ret = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, user);

	MergePolicyIntValue(global, user, ret, CFSTR("requiresMixedCase"));
	MergePolicyIntValue(global, user, ret, CFSTR("requiresSymbol"));
	MergePolicyIntValue(global, user, ret, CFSTR("requiresAlpha"));
	MergePolicyIntValue(global, user, ret, CFSTR("requiresNumeric"));
	MergePolicyIntValue(global, user, ret, CFSTR("passwordCannotBeName"));
	MergePolicyIntValue(global, user, ret, CFSTR("maxMinutesUntilChangePasswo"));
	MergePolicyIntValue(global, user, ret, CFSTR("maxMinutesUntilDisabled"));
	MergePolicyIntValue(global, user, ret, CFSTR("maxMinutesOfNonUse"));
	MergePolicyIntValue(global, user, ret, CFSTR("maxFailedLoginAttempts"));
	MergePolicyIntValue(global, user, ret, CFSTR("minChars"));
	MergePolicyIntValue(global, user, ret, CFSTR("maxChars"));
	MergePolicyIntValue(global, user, ret, CFSTR("usingHistory"));
	
	// Merge expiration date if not set in the user policy and is set
	// in global policy
	unsigned int tmpint = 0;
	CFNumberRef anumber = CFDictionaryGetValue(user, CFSTR("usingExpirationDate"));
	if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpint);
	if(!tmpint) {
		anumber = CFDictionaryGetValue(global, CFSTR("usingExpirationDate"));
		if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpint);
		if(tmpint) {
			CFDateRef expdate = CFDictionaryGetValue(global, CFSTR("expirationDateGMT"));
			CFDictionarySetValue(ret, CFSTR("usingExpirationDate"), anumber);
			CFDictionarySetValue(ret, CFSTR("expirationDateGMT"), expdate);
		}
	}

	// Merge hard expiration date if not set in the user policy and is set
	// in global policy.
	anumber = CFDictionaryGetValue(user, CFSTR("usingHardExpirationDate"));
	if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpint);
	if(!tmpint) {
		anumber = CFDictionaryGetValue(global, CFSTR("usingHardExpirationDate"));
		if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpint);
		if(tmpint) {
			CFDateRef expdate = CFDictionaryGetValue(global, CFSTR("hardExpireDateGMT"));
			CFDictionarySetValue(ret, CFSTR("hardExpireDateGMT"), expdate);
			CFDictionarySetValue(ret, CFSTR("usingHardExpirationDate"), anumber);
		}
	}

	return ret;
}

static int GetDisabledStatus(CFMutableDictionaryRef policy, CFDateRef ctime, CFDateRef lastLogin, uint16_t *failedattempts, int disableReason) {
	int ret = 0;
	bool setToDisabled = false;
	short tmpshort = 0;
	long tmplong = 0;
	CFAbsoluteTime tmptime = 0;

	// Admins are exempt and should never be disabled due to policy
	CFNumberRef isadmin = CFDictionaryGetValue(policy, CFSTR("isAdminUser"));
	if(isadmin) CFNumberGetValue(isadmin, kCFNumberShortType, &tmpshort);
	if(tmpshort != 0) return 0;

	CFDateRef now = CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
	
	// If the user is disabled, and the validAfter policy is in use, and
	// it is after the validAfter date, and the disable reason was
	// kDisabledInactive (the closest we can come to determining whether
	// their disabled status was caused by validAfter), then we reenable.
	CFNumberRef isdisabled = CFDictionaryGetValue(policy, CFSTR("isDisabled"));
	if(isdisabled) CFNumberGetValue(isdisabled, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		CFDateRef validAfter = CFDictionaryGetValue(policy, CFSTR("validAfter"));
		if(validAfter) {
			if(CFDateCompare(validAfter, now, NULL) == kCFCompareLessThan) {
				if(disableReason == kDisabledInactive) {
					int zero = 0;
					CFNumberRef cfzero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);
					CFDictionarySetValue(policy, CFSTR("isDisabled"), cfzero);
					CFRelease(cfzero);
				}
			}
		}
	}

	// Check to see if the user's policy has a max failed login set
	CFNumberRef maxFailedLogins = CFDictionaryGetValue(policy, CFSTR("maxFailedLoginAttempts"));
	if(maxFailedLogins) CFNumberGetValue(maxFailedLogins, kCFNumberShortType, &tmpshort);
	if(tmpshort > 0) {
		if(*failedattempts >= tmpshort) {
			*failedattempts = 0;
			ret = kDisabledTooManyFailedLogins;
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due to failed logins", __PRETTY_FUNCTION__, 0, 0);
		}
	}

	tmpshort = 0;
	CFNumberRef usingHardExpire = CFDictionaryGetValue(policy, CFSTR("usingHardExpirationDate"));
	if(usingHardExpire) CFNumberGetValue(usingHardExpire, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		CFDateRef hardExpire = CFDictionaryGetValue(policy, CFSTR("hardExpireDateGMT"));
		if(hardExpire) {
			if(CFDateCompare(hardExpire, now, NULL) != kCFCompareGreaterThan) {
				ret = kDisabledExpired;
				Debug(LDAP_DEBUG_TRACE, "%s: disabling due to hard expire", __PRETTY_FUNCTION__, 0, 0);
			}
		}
	}

	tmpshort = tmplong = 0;
	CFNumberRef maxMinutesUntilDisabled = CFDictionaryGetValue(policy, CFSTR("maxMinutesUntilDisabled"));
	if(maxMinutesUntilDisabled) CFNumberGetValue(maxMinutesUntilDisabled, kCFNumberLongType, &tmplong);
	if(tmplong > 0) {
		tmptime = CFDateGetAbsoluteTime(ctime);
		if( ((CFAbsoluteTimeGetCurrent() - tmptime)/60) > tmplong ) {
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due to max minutes expired", __PRETTY_FUNCTION__, 0, 0);
			ret = kDisabledExpired;
		}
	}
	
	tmplong = 0;
	CFNumberRef maxMinutesOfNonUse = CFDictionaryGetValue(policy, CFSTR("maxMinutesOfNonUse"));
	if(maxMinutesOfNonUse) CFNumberGetValue(maxMinutesOfNonUse, kCFNumberLongType, &tmplong);
	if(tmplong > 0) {
		tmptime = CFDateGetAbsoluteTime(lastLogin);
		if( ((CFAbsoluteTimeGetCurrent() - tmptime)/60) > tmplong ) {
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due to max minutes of non use", __PRETTY_FUNCTION__, 0, 0);
			ret = kDisabledInactive;
		}
	}

	CFDateRef validAfter = CFDictionaryGetValue(policy, CFSTR("validAfter"));
	if(validAfter) {
		if(CFDateCompare(validAfter, now, NULL) == kCFCompareGreaterThan) {
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due to validafter", __PRETTY_FUNCTION__, 0, 0);
			ret = kDisabledInactive;
		}
	}

	if(ret != 0) {
		int one = 1;
		CFNumberRef cfone = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one);
		CFDictionarySetValue(policy, CFSTR("isDisabled"), cfone);
		CFRelease(cfone);
	}

	if(now) CFRelease(now);
	return ret;
}

CFDictionaryRef odusers_copy_defaultglobalpolicy(void) {
	CFMutableDictionaryRef ret = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	int one = 1;
	int zero = 0;
	CFNumberRef cfone = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one);
	CFNumberRef cfzero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);
	CFDictionarySetValue(ret, CFSTR("usingHistory"), cfzero);
	CFDictionarySetValue(ret, CFSTR("canModifyPasswordforSelf"), cfone);
	CFDictionarySetValue(ret, CFSTR("usingExpirationDate"), cfzero);
	CFDictionarySetValue(ret, CFSTR("usingHardExpirationDate"), cfzero);
	CFDictionarySetValue(ret, CFSTR("requiresAlpha"), cfzero);
	CFDictionarySetValue(ret, CFSTR("requiresNumeric"), cfzero);
	CFDictionarySetValue(ret, CFSTR("maxMinutesUntilChangePassword"), cfzero);
	CFDictionarySetValue(ret, CFSTR("maxMinutesUntilDisabled"), cfzero);
	CFDictionarySetValue(ret, CFSTR("maxMinutesOfNonUse"), cfzero);
	CFDictionarySetValue(ret, CFSTR("maxFailedLoginAttempts"), cfzero);
	CFDictionarySetValue(ret, CFSTR("minChars"), cfzero);
	CFDictionarySetValue(ret, CFSTR("maxChars"), cfzero);
	CFDictionarySetValue(ret, CFSTR("passwordCannotBeName"), cfzero);
	CFDictionarySetValue(ret, CFSTR("requiresMixedCase"), cfzero);
	CFDictionarySetValue(ret, CFSTR("requiresSymbol"), cfzero);
	CFDictionarySetValue(ret, CFSTR("newPasswordRequired"), cfzero);
	CFDictionarySetValue(ret, CFSTR("minutesUntilFailedLoginReset"), cfzero);
	CFDictionarySetValue(ret, CFSTR("notGuessablePattern"), cfzero);

	return ret;
}

struct berval *odusers_copy_dict2bv(CFDictionaryRef dict) {
	CFDataRef xmlData = CFPropertyListCreateData(kCFAllocatorDefault, (CFPropertyListRef)dict, kCFPropertyListXMLFormat_v1_0, 0, NULL);
	if(!xmlData) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not convert CFDictionary to CFData", __PRETTY_FUNCTION__, 0, 0);
		return NULL;
	}

	struct berval *ret = ber_mem2bv(CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData), 1, NULL);
	
	CFRelease(xmlData);
	return ret;
}

CFDictionaryRef odusers_copy_effectiveuserpoldict(struct berval *dn) {
	Entry *e = NULL;
	Attribute *a;
	Attribute *global_attr = NULL;
	
	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	for(a = e->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, "apple-user-passwordpolicy", a->a_desc->ad_cname.bv_len) == 0) {
			Attribute *effective = NULL;
			global_attr = odusers_copy_globalpolicy();
			CFDictionaryRef globaldict = NULL;
			CFMutableDictionaryRef effectivedict = NULL;
			CFDictionaryRef userdict = NULL;
			Attribute *failedLogins = NULL;
			Attribute *creationDate = NULL;
			Attribute *lastLogin = NULL;
			Attribute *disableReason = NULL;
			int disableReasonInt = 0;
			const char *text = NULL;
			CFDateRef lastLoginCF = NULL;
			CFDateRef creationDateCF = NULL;
			uint16_t loginattempts = 0;
			int one = 1;
			CFNumberRef cfone = NULL;

			if(!global_attr || global_attr->a_numvals == 0) {
				globaldict = odusers_copy_defaultglobalpolicy();
			} else {
				globaldict = CopyPolicyToDict(global_attr->a_vals[0].bv_val, global_attr->a_vals[0].bv_len);
				if(!globaldict) {
					Debug(LDAP_DEBUG_ANY, "%s: Unable to convert retrieved global policy to CFDictionary", __PRETTY_FUNCTION__, 0, 0);
					goto out;
				}
			}

			userdict = CopyPolicyToDict(a->a_vals[0].bv_val, a->a_vals[0].bv_len);
			if(!userdict) {
				Debug(LDAP_DEBUG_ANY, "%s: Unable to convert retrieved user policy to CFDictionary", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}
		
			effectivedict = CopyEffectivePolicy(globaldict, userdict);
			CFRelease(globaldict);
			globaldict = NULL;
			CFRelease(userdict);
			userdict = NULL;
			if(!effectivedict) {
				Debug(LDAP_DEBUG_ANY, "%s: Unable to compute effective policy from global and user dictionaries", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}

			if(!lastLoginAD && slap_str2ad("lastLoginTime", &lastLoginAD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of lastLoginTime attribute", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}
			if(!creationDateAD && slap_str2ad("creationDate", &creationDateAD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of creationDate attribute", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}
			if(!failedLoginsAD && slap_str2ad("loginFailedAttempts", &failedLoginsAD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of loginFailedAttempts attribute", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}
			if(!disableReasonAD && slap_str2ad("disableReason", &disableReasonAD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of disableReason attribute", __PRETTY_FUNCTION__, 0, 0);
				goto out;
			}

			lastLogin = attrs_find(e->e_attrs, lastLoginAD);
			struct tm tmptm = {0};
			time_t tmptime = 0;
			if(lastLogin && lastLogin->a_numvals && lastLogin->a_nvals[0].bv_len) {
				strptime(lastLogin->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
				tmptime = timegm(&tmptm);
			}
			lastLoginCF = CFDateCreate(kCFAllocatorDefault, tmptime - kCFAbsoluteTimeIntervalSince1970);

			creationDate = attrs_find(e->e_attrs, creationDateAD);
			tmptime = 0;
			if(creationDate && creationDate->a_numvals && creationDate->a_nvals[0].bv_len) {
				strptime(creationDate->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
				tmptime = timegm(&tmptm);
			}
			creationDateCF = CFDateCreate(kCFAllocatorDefault, tmptime - kCFAbsoluteTimeIntervalSince1970);

			failedLogins = attrs_find(e->e_attrs, failedLoginsAD);
			if(failedLogins && failedLogins->a_numvals && failedLogins->a_nvals[0].bv_len) {
				long long tmpll;
				errno = 0;
				tmpll = strtoll(failedLogins->a_nvals[0].bv_val, NULL, 10);
				if( ((tmpll == LLONG_MAX) || (tmpll == LLONG_MIN)) && (errno == ERANGE) ) {
					tmpll = 0;
				}
				if( (tmpll > USHRT_MAX) || (tmpll < 0) ) {
					tmpll = 0;
				}
				loginattempts = tmpll;
			}

			disableReason = attrs_find(e->e_attrs, disableReasonAD);
			if(disableReason && disableReason->a_numvals && disableReason->a_nvals[0].bv_len) {
				long long tmpll;
				errno = 0;
				tmpll = strtoll(disableReason->a_nvals[0].bv_val, NULL, 10);
				if( ((tmpll == LLONG_MAX) || (tmpll == LLONG_MIN)) && (errno == ERANGE) ) {
					tmpll = 0;
				}
				if( (tmpll > USHRT_MAX) || (tmpll < 0) ) {
					tmpll = 0;
				}
				disableReasonInt = tmpll;
			}

			GetDisabledStatus(effectivedict, creationDateCF, lastLoginCF, &loginattempts, disableReasonInt);
			if(lastLoginCF) CFRelease(lastLoginCF);
			if(creationDateCF) CFRelease(creationDateCF);
			if (e) entry_free(e);
			return effectivedict;
		}
	}
out:
	if (global_attr) attr_free(global_attr);
	if (e) entry_free(e);
	return NULL;
}

bool odusers_isdisabled(CFDictionaryRef policy) {
	short tmpshort = 0;
	CFNumberRef isdisabled = CFDictionaryGetValue(policy, CFSTR("isDisabled"));
	if(isdisabled) CFNumberGetValue(isdisabled, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		return true;
	}
	return false;
}

bool odusers_isadmin(CFDictionaryRef policy) {
	short tmpshort = 0;
	CFNumberRef isdisabled = CFDictionaryGetValue(policy, CFSTR("isAdminUser"));
	if(isdisabled) CFNumberGetValue(isdisabled, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		return true;
	}
	return false;
}

int odusers_increment_failedlogin(struct berval *dn) {
	int ret = -1;
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	SlapReply rs = {REP_RESULT};

	Modifications *mod = NULL;
	char *attemptsstr = NULL;

	Entry *e = NULL;
	Attribute *failedLogins = NULL;
	const char *text = NULL;
	uint16_t loginattempts = 0;
	short optype = LDAP_MOD_ADD;

	if(!failedLoginsAD && slap_str2ad("loginFailedAttempts", &failedLoginsAD, &text) != 0) {
		Debug(LDAP_DEBUG_TRACE, "%s: Unable to retrieve description of loginFailedAttempts attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_TRACE, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	failedLogins = attrs_find(e->e_attrs, failedLoginsAD);
	if(failedLogins && failedLogins->a_numvals && failedLogins->a_nvals[0].bv_len) {
		long long tmpll;
		errno = 0;
		tmpll = strtoll(failedLogins->a_nvals[0].bv_val, NULL, 10);
		if( ((tmpll == LLONG_MAX) || (tmpll == LLONG_MIN)) && (errno == ERANGE) ) {
			tmpll = 0;
		}
		if( (tmpll > USHRT_MAX) || (tmpll < 0) ) {
			tmpll = 0;
		}
		loginattempts = tmpll;
		optype = LDAP_MOD_REPLACE;
	}

	loginattempts++;
	asprintf(&attemptsstr, "%hu", loginattempts);

	mod = (Modifications *) ch_calloc(sizeof(Modifications), 2);

	mod->sml_op = optype;
	mod->sml_flags = 0;
	mod->sml_type = failedLoginsAD->ad_cname;
	
	mod->sml_values = (struct berval*) ch_calloc(sizeof(struct berval), 2);
	ber_str2bv(attemptsstr, strlen(attemptsstr), 1, &mod->sml_values[0]);
	if (attemptsstr) free(attemptsstr);
	attemptsstr = NULL;
	
	mod->sml_values[1].bv_val = NULL;
	mod->sml_values[1].bv_len = 0;
	mod->sml_numvals = 1;
	mod->sml_nvalues = NULL;

	mod->sml_desc = failedLoginsAD;
	mod->sml_next = NULL;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = *dn;
	fakeop->o_ndn = *dn;
	fakeop->o_req_dn = e->e_name;
	fakeop->o_req_ndn = e->e_name;
	fakeop->orm_modlist = mod;
	fakeop->o_tag = LDAP_REQ_MODIFY;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->o_bd = frontendDB;

	fakeop->o_bd->be_modify(fakeop, &rs);
	if(rs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "Unable to modify failedlogins for user %s: %d %s\n", fakeop->o_req_ndn.bv_val, rs.sr_err, rs.sr_text);
		goto out;
	}

	ret = 0;

out:
	if(e) entry_free(e);
	if(mod) slap_mods_free(mod, 1);
	return ret;
}

int odusers_successful_auth(struct berval *dn, CFDictionaryRef policy) {
	int ret = -1;
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	SlapReply rs = {REP_RESULT};

	Modifications *mod = NULL;
	Modifications *modhead = NULL;
	char *attemptsstr = NULL;

	Entry *e = NULL;
	Attribute *failedLogins = NULL;
	const char *text = NULL;
	short tmpshort = 0;
	CFNumberRef maxMins = NULL;

	if(!failedLoginsAD && slap_str2ad("loginFailedAttempts", &failedLoginsAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of loginFailedAttempts attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	failedLogins = attrs_find(e->e_attrs, failedLoginsAD);
	if(failedLogins && failedLogins->a_numvals && failedLogins->a_nvals[0].bv_len) {
		mod = (Modifications *) ch_malloc(sizeof(Modifications));

		mod->sml_op = LDAP_MOD_REPLACE;
		mod->sml_flags = 0;
		mod->sml_type = failedLoginsAD->ad_cname;
		mod->sml_values = (struct berval*) ch_calloc(sizeof(struct berval), 2);
		mod->sml_values[0].bv_val = ch_strdup("0");
		mod->sml_values[0].bv_len = 1;
		mod->sml_values[1].bv_val = NULL;
		mod->sml_values[1].bv_len = 0;
		mod->sml_nvalues = NULL;
		mod->sml_numvals = 1;

		mod->sml_desc = failedLoginsAD;
		mod->sml_next = modhead;
		modhead = mod;
	}

	maxMins = CFDictionaryGetValue(policy, CFSTR("maxMinutesOfNonUse"));
	if(maxMins) CFNumberGetValue(maxMins, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		time_t tmptime;
		struct tm tmptm;

		if(!lastLoginAD && slap_str2ad("lastLoginTime", &lastLoginAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of lastLoginTime attribute", __PRETTY_FUNCTION__, 0, 0);
			goto out;
		}

		tmptime = time(NULL);
		gmtime_r(&tmptime, &tmptm);
		
		mod = (Modifications *) ch_malloc(sizeof(Modifications));

		mod->sml_op = LDAP_MOD_REPLACE;
		mod->sml_flags = 0;
		mod->sml_type = lastLoginAD->ad_cname;
		mod->sml_values = (struct berval*) ch_calloc(sizeof(struct berval), 2);
		
		char time_str[256] = {0};
		int time_str_len = 0;
		 
		time_str_len = strftime(time_str, sizeof(time_str), "%Y%m%d%H%M%SZ", &tmptm);
		
		ber_str2bv(time_str, time_str_len, 1, &mod->sml_values[0]);

		mod->sml_values[1].bv_val = NULL;
		mod->sml_values[1].bv_len = 0;
		mod->sml_nvalues = NULL;
		mod->sml_numvals = 1;

		mod->sml_desc = lastLoginAD;
		mod->sml_next = modhead;
		modhead = mod;
	}

	if(!modhead) {
		ret = 0;
		goto out;
	}

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = *dn;
	fakeop->o_ndn = *dn;
	fakeop->o_req_dn = e->e_name;
	fakeop->o_req_ndn = e->e_name;
	fakeop->orm_modlist = modhead;
	fakeop->o_tag = LDAP_REQ_MODIFY;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->o_bd = frontendDB;

	fakeop->o_bd->be_modify(fakeop, &rs);
	if(rs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "Unable to modify record for successful login for user %s: %d %s\n", fakeop->o_req_ndn.bv_val, rs.sr_err, rs.sr_text);
		goto out;
	}

	ret = 0;

out:
	if(e) entry_free(e);
	if(modhead) slap_mods_free(modhead, 1);
	return ret;
}

CFDictionaryRef odusers_copy_pwsprefs(const char *suffix) {
	OperationBuffer opbuf;
	Connection conn;
	Operation *fakeop = NULL;
	Entry *e = NULL;
	struct berval dn;
	CFDictionaryRef ret = NULL;
	char *searchdn = NULL;

	asprintf(&searchdn, "cn=passwordserver,cn=config%s", suffix);
	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	dn.bv_val = searchdn;
	dn.bv_len = strlen(dn.bv_val);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_ndn = dn;
	fakeop->o_req_dn = fakeop->o_req_ndn = dn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	e = odusers_copy_entry(fakeop);
	if(!e) goto out;

	Attribute *a;
	for(a = e->e_attrs; a; a = a->a_next) {
		if(strncmp(a->a_desc->ad_cname.bv_val, "apple-xmlplist", a->a_desc->ad_cname.bv_len) == 0) {
			ret = CopyPolicyToDict(a->a_vals[0].bv_val, a->a_vals[0].bv_len);
			break;
		}
	}

out:
	if(e) entry_free(e);
	if(searchdn) free(searchdn);
	return ret;
}
