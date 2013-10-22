#include "portable.h"

#include <ac/string.h>
#include <ac/ctype.h>
#include "slap.h"
#include "ldif.h"
#include "config.h"
#define __COREFOUNDATION_CFFILESECURITY__
#include <CoreFoundation/CoreFoundation.h>
#include <HeimODAdmin/HeimODAdmin.h>
#include <Heimdal/krb5.h>
#include <CommonAuth/CommonAuth.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include "applehelpers.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <ifaddrs.h>
#include <arpa/inet.h>

/* For PWS compatible DES encrypted plaintext password */
#include <PasswordServer/AuthDBFileDefs.h>
#include <PasswordServer/AuthFile.h>

static Filter generic_filter = { LDAP_FILTER_PRESENT, { 0 }, NULL };
static struct berval generic_filterstr = BER_BVC("(objectclass=*)");

static  AttributeDescription *failedLoginsAD = NULL;
static  AttributeDescription *creationDateAD = NULL;
static  AttributeDescription *passModDateAD = NULL;
static  AttributeDescription *lastLoginAD = NULL;
static 	AttributeDescription *disableReasonAD = NULL;
static 	AttributeDescription *realnameAD = NULL;
static 	AttributeDescription *pwslocAD = NULL;
// shared with odusers
AttributeDescription *passwordRequiredDateAD = NULL;
AttributeDescription *policyAD = NULL;

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
	Entry *e = NULL;
	Entry *ret = NULL;
	char guidstr[37];
	struct berval authdn;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = *dn;
	dnNormalize(0, NULL, NULL, dn, &fakeop->o_ndn, NULL);
	fakeop->o_req_dn = *dn;
	dnNormalize(0, NULL, NULL, dn, &fakeop->o_req_ndn, NULL);

	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_TRACE, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, fakeop->o_req_ndn.bv_val, 0);
		goto out;
	}

	if( odusers_get_authguid(e, guidstr) != 0) {
		Debug(LDAP_DEBUG_TRACE, "%s: Could not locate authguid for record %s", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	entry_free(e);
	e = NULL;

	authdn.bv_len = asprintf(&authdn.bv_val, "authGUID=%s,cn=users,cn=authdata", guidstr);

	if(!BER_BVISNULL(&fakeop->o_ndn)) ch_free(fakeop->o_ndn.bv_val);
	if(!BER_BVISNULL(&fakeop->o_req_ndn)) ch_free(fakeop->o_req_ndn.bv_val);
	fakeop->o_dn = fakeop->o_req_dn = authdn;
	dnNormalize(0, NULL, NULL, &fakeop->o_dn, &fakeop->o_ndn, NULL);
	dnNormalize(0, NULL, NULL, &fakeop->o_req_dn, &fakeop->o_req_ndn, NULL);
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	ret = odusers_copy_entry(fakeop);
	free(authdn.bv_val);

out:
	if(fakeop && !BER_BVISNULL(&fakeop->o_ndn)) ch_free(fakeop->o_ndn.bv_val);
	if(fakeop && !BER_BVISNULL(&fakeop->o_req_ndn)) ch_free(fakeop->o_req_ndn.bv_val);
	if(e) entry_free(e);
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

static Attribute *odusers_copy_passwordRequiredDate(void) {
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
		if(strncmp(a->a_desc->ad_cname.bv_val, "passwordRequiredDate", a->a_desc->ad_cname.bv_len) == 0) {
			ret = attr_dup(a);
		}
	}
	

out:
	if(e) entry_free(e);
	return ret;
}

CFMutableDictionaryRef CopyPolicyToDict(const char *policyplist, int len) {
	CFDataRef xmlData = NULL;
	CFMutableDictionaryRef ret = NULL;
	CFErrorRef err = NULL;

	xmlData = CFDataCreate(kCFAllocatorDefault, (const unsigned char*)policyplist, len);
	if(!xmlData) goto out;

	ret = (CFMutableDictionaryRef)CFPropertyListCreateWithData(kCFAllocatorDefault, xmlData, kCFPropertyListMutableContainersAndLeaves, NULL, &err);
	if(!ret) goto out;

out:
	if(xmlData) CFRelease(xmlData);
	return ret;
}

static void MergePolicyIntValue(CFDictionaryRef global, CFDictionaryRef user, CFMutableDictionaryRef merged, CFStringRef policy, int defaultvalue) {
	unsigned int tmpbool = 0;
	CFNumberRef anumber = CFDictionaryGetValue(user, policy);
	if(anumber) {
		CFNumberGetValue(anumber, kCFNumberIntType, &tmpbool);
		if(tmpbool == defaultvalue) {
			anumber = CFDictionaryGetValue(global, policy);
			if(anumber) CFNumberGetValue(anumber, kCFNumberIntType, &tmpbool);
			if(tmpbool) {
				CFDictionarySetValue(merged, policy, anumber);
			}
		}
	} else {
		anumber = CFDictionaryGetValue(global, policy);
		if(anumber) {
			CFNumberGetValue(anumber, kCFNumberIntType, &tmpbool);
			CFDictionarySetValue(merged, policy, anumber);
		}
	}
	return;
}

static CFMutableDictionaryRef CopyEffectivePolicy(CFDictionaryRef global, CFDictionaryRef user) {
	CFMutableDictionaryRef ret = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, user);

	MergePolicyIntValue(global, user, ret, CFSTR("isDisabled"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("isAdminUser"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("newPasswordRequired"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("canModifyPasswordforSelf"), 1);
	MergePolicyIntValue(global, user, ret, CFSTR("usingExpirationDate"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("usingHardExpirationDate"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("notGuessablePattern"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("isSessionKeyAgent"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("isComputerAccount"), 0);

	MergePolicyIntValue(global, user, ret, CFSTR("requiresMixedCase"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("requiresSymbol"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("requiresAlpha"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("requiresNumeric"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("passwordCannotBeName"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("maxMinutesUntilChangePassword"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("maxMinutesUntilDisabled"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("maxMinutesOfNonUse"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("maxFailedLoginAttempts"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("minChars"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("maxChars"), 0);
	MergePolicyIntValue(global, user, ret, CFSTR("usingHistory"), 0);
	
	// Merge expiration date if not set in the user policy and is set
	// in global policy
	unsigned int tmpint = 0;
	unsigned int tmpshort = 0;
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

static int GetDisabledStatus(CFMutableDictionaryRef policy, CFDateRef ctime, CFDateRef lastLogin, CFDateRef passModDate, uint16_t *failedattempts, int disableReason) {
	int ret = 0;
	bool setToDisabled = false;
	short tmpshort = 0;
	long tmplong = 0;
	CFAbsoluteTime tmptime = 0;

	// Admins are exempt and should never be disabled due to policy
	CFNumberRef isadmin = CFDictionaryGetValue(policy, CFSTR("isAdminUser"));
	if(isadmin) CFNumberGetValue(isadmin, kCFNumberShortType, &tmpshort);
	if(tmpshort != 0) {
		int zero = 0;
		CFNumberRef cfzero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);
		CFDictionarySetValue(policy, CFSTR("newPasswordRequired"), cfzero);
		CFRelease(cfzero);
		return 0;
	}

	// Computers are exempt from policy too, since we have no way to cope
	// with computer accounts being disabled.
	CFNumberRef iscomputer = CFDictionaryGetValue(policy, CFSTR("isComputer"));
	if(iscomputer) CFNumberGetValue(iscomputer, kCFNumberShortType, &tmpshort);
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
		ret = kDisabledByAdmin;
		if(validAfter) {
			if(CFDateCompare(validAfter, now, NULL) == kCFCompareLessThan) {
				if(disableReason == kDisabledInactive) {
					int zero = 0;
					CFNumberRef cfzero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);
					CFDictionarySetValue(policy, CFSTR("isDisabled"), cfzero);
					CFRelease(cfzero);
					ret = 0;
				}
			}
		}
	}

	if(ret) goto out;

	// Check to see if the user's policy has a max failed login set
	CFNumberRef maxFailedLogins = CFDictionaryGetValue(policy, CFSTR("maxFailedLoginAttempts"));
	if(maxFailedLogins) CFNumberGetValue(maxFailedLogins, kCFNumberShortType, &tmpshort);
	if(tmpshort > 0) {
		if(*failedattempts >= tmpshort) {
			*failedattempts = 0;
			ret = kDisabledTooManyFailedLogins;
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due to failed logins", __PRETTY_FUNCTION__, 0, 0);
			goto out;
		}
	}

	tmpshort = 0;
	CFNumberRef usingHardExpire = CFDictionaryGetValue(policy, CFSTR("usingHardExpirationDate"));
	if(usingHardExpire) CFNumberGetValue(usingHardExpire, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		CFTypeRef hardExpire = CFDictionaryGetValue(policy, CFSTR("hardExpireDateGMT"));
		if(hardExpire) {
			if(CFGetTypeID(hardExpire) == CFDateGetTypeID()) {
				if(CFDateCompare(hardExpire, now, NULL) != kCFCompareGreaterThan) {
					ret = kDisabledExpired;
					Debug(LDAP_DEBUG_TRACE, "%s: disabling due to hard expire", __PRETTY_FUNCTION__, 0, 0);
					goto out;
				}
			} else if(CFGetTypeID(hardExpire) == CFNumberGetTypeID()) {
				CFNumberGetValue(hardExpire, kCFNumberLongType, &tmplong);
				CFDateRef tmpcfdate = CFDateCreate(kCFAllocatorDefault, tmplong - kCFAbsoluteTimeIntervalSince1970);
				if(CFDateCompare(tmpcfdate, now, NULL) != kCFCompareGreaterThan) {
					if(tmpcfdate) CFRelease(tmpcfdate);
					ret = kDisabledExpired;
					Debug(LDAP_DEBUG_TRACE, "%s: disabling due to hard expire", __PRETTY_FUNCTION__, 0, 0);
					goto out;
				}
				if(tmpcfdate) CFRelease(tmpcfdate);
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
			goto out;
		}
	}
	
	if(lastLogin) {
		tmplong = 0;
		CFNumberRef maxMinutesOfNonUse = CFDictionaryGetValue(policy, CFSTR("maxMinutesOfNonUse"));
		if(maxMinutesOfNonUse) CFNumberGetValue(maxMinutesOfNonUse, kCFNumberLongType, &tmplong);
		if(tmplong > 0) {
			tmptime = CFDateGetAbsoluteTime(lastLogin);
			if( ((CFAbsoluteTimeGetCurrent() - tmptime)/60) > tmplong ) {
				Debug(LDAP_DEBUG_TRACE, "%s: disabling due to max minutes of non use", __PRETTY_FUNCTION__, 0, 0);
				ret = kDisabledInactive;
				goto out;
			}
		}
	}

	CFDateRef validAfter = CFDictionaryGetValue(policy, CFSTR("validAfter"));
	if(validAfter) {
		if(CFDateCompare(validAfter, now, NULL) == kCFCompareGreaterThan) {
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due to validafter", __PRETTY_FUNCTION__, 0, 0);
			ret = kDisabledInactive;
			goto out;
		}
	}

	// The change password policies must be the last ones evaluated.
	// They only get set if the user would otherwise be allowed.
	tmpshort = tmplong = 0;
	CFNumberRef maxMinutesUntilChangePassword = CFDictionaryGetValue(policy, CFSTR("maxMinutesUntilChangePassword"));
	if(maxMinutesUntilChangePassword) CFNumberGetValue(maxMinutesUntilChangePassword, kCFNumberLongType, &tmplong);
	if(tmplong > 0) {
		tmptime = CFDateGetAbsoluteTime(passModDate);
		if( ((CFAbsoluteTimeGetCurrent() - tmptime)/60) > tmplong ) {
			Debug(LDAP_DEBUG_TRACE, "%s: disabling due password expiration", __func__, 0, 0);
			if(!ret) ret = kDisabledNewPasswordRequired;
		}
	}

	tmpshort = 0;
	CFNumberRef newPasswordRequired = CFDictionaryGetValue(policy, CFSTR("newPasswordRequired"));
	if(newPasswordRequired) CFNumberGetValue(newPasswordRequired, kCFNumberShortType, &tmpshort);
	if(tmpshort > 0) {
		if(!ret) ret = kDisabledNewPasswordRequired;
	}

out:
	if(ret != 0) {
		int one = 1;
		CFNumberRef cfone = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one);
		CFDictionarySetValue(policy, CFSTR("isDisabled"), cfone);
		CFRelease(cfone);
	}

	CFNumberRef cfdisableReason = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &ret);
	CFDictionarySetValue(policy, CFSTR("effectiveDisableReason"), cfdisableReason);
	if(cfdisableReason) CFRelease(cfdisableReason);

	if(now) CFRelease(now);
	return ret;
}

CFDictionaryRef odusers_copydefaultuserpolicy(void) {
	CFMutableDictionaryRef ret = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	
	int one = 1;
	int zero = 0;
	CFNumberRef cfone = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one);
	CFNumberRef cfzero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);

	CFDictionarySetValue(ret, CFSTR("isDisabled"), cfzero);
	CFDictionarySetValue(ret, CFSTR("isAdminUser"), cfzero);
	CFDictionarySetValue(ret, CFSTR("newPasswordRequired"), cfzero);
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
	CFDictionarySetValue(ret, CFSTR("notGuessablePattern"), cfzero);
	CFDictionarySetValue(ret, CFSTR("isSessionKeyAgent"), cfzero);
	CFDictionarySetValue(ret, CFSTR("isComputerAccount"), cfzero);
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

	struct berval *ret = ber_mem2bv((LDAP_CONST char *)CFDataGetBytePtr(xmlData), CFDataGetLength(xmlData), 1, NULL);
	
	CFRelease(xmlData);
	return ret;
}

CFDictionaryRef odusers_copy_effectiveuserpoldict(struct berval *dn) {
	Entry *e = NULL;
	Attribute *effective = NULL;
	Attribute *global_attr = odusers_copy_globalpolicy();
	Attribute *passwordRequiredDateAttr = odusers_copy_passwordRequiredDate();
	CFDictionaryRef globaldict = NULL;
	CFMutableDictionaryRef effectivedict = NULL;
	CFDictionaryRef userdict = NULL;
	Attribute *policyAttr = NULL;
	Attribute *failedLogins = NULL;
	Attribute *creationDate = NULL;
	Attribute *passModDate = NULL;
	Attribute *modDate = NULL;
	Attribute *lastLogin = NULL;
	Attribute *disableReason = NULL;
	int disableReasonInt = 0;
	const char *text = NULL;
	CFDateRef lastLoginCF = NULL;
	CFDateRef creationDateCF = NULL;
	CFDateRef passModDateCF = NULL;
	CFDateRef passwordRequiredDateCF = NULL;
	uint16_t loginattempts = 0;
	int one = 1;
	CFNumberRef cfone = NULL;

	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __PRETTY_FUNCTION__, dn->bv_val, 0);
		goto out;
	}

	if(!policyAD && slap_str2ad("apple-user-passwordpolicy", &policyAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of apple-user-passwordpolicy attribute", __PRETTY_FUNCTION__, 0, 0);
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
	if(!passModDateAD && slap_str2ad("passwordModDate", &passModDateAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of passwordModDate attribute", __PRETTY_FUNCTION__, 0, 0);
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

	if(!global_attr || global_attr->a_numvals == 0) {
		globaldict = odusers_copy_defaultglobalpolicy();
	} else {
		globaldict = CopyPolicyToDict(global_attr->a_vals[0].bv_val, global_attr->a_vals[0].bv_len);
		if(!globaldict) {
			Debug(LDAP_DEBUG_ANY, "%s: Unable to convert retrieved global policy to CFDictionary", __PRETTY_FUNCTION__, 0, 0);
			goto out;
		}
	}

	policyAttr = attrs_find(e->e_attrs, policyAD);
	if(policyAttr) {
		CFDictionaryRef tmpdict = CopyPolicyToDict(policyAttr->a_vals[0].bv_val, policyAttr->a_vals[0].bv_len);
		CFDictionaryRef defaultdict = odusers_copydefaultuserpolicy();
		userdict = CopyEffectivePolicy(defaultdict, tmpdict);
		CFRelease(defaultdict);
		CFRelease(tmpdict);
	} else {
		userdict = odusers_copydefaultuserpolicy();
	}

	if(!userdict) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to convert retrieved user policy to CFDictionary", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

		
	effectivedict = CopyEffectivePolicy(globaldict, userdict);
	if(!effectivedict) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to compute effective policy from global and user dictionaries", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	lastLogin = attrs_find(e->e_attrs, lastLoginAD);
	struct tm tmptm = {0};
	time_t tmptime = 0;
	if(lastLogin && lastLogin->a_numvals && lastLogin->a_nvals[0].bv_len) {
		strptime(lastLogin->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
		tmptime = timegm(&tmptm);
		lastLoginCF = CFDateCreate(kCFAllocatorDefault, tmptime - kCFAbsoluteTimeIntervalSince1970);
	}

	creationDate = attrs_find(e->e_attrs, creationDateAD);
	tmptime = 0;
	if(creationDate && creationDate->a_numvals && creationDate->a_nvals[0].bv_len) {
		strptime(creationDate->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
		tmptime = timegm(&tmptm);
	}
	creationDateCF = CFDateCreate(kCFAllocatorDefault, tmptime - kCFAbsoluteTimeIntervalSince1970);

	passModDate = attrs_find(e->e_attrs, passModDateAD);
	tmptime = 0;
	if(passModDate && passModDate->a_numvals && passModDate->a_nvals[0].bv_len) {
		strptime(passModDate->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
		tmptime = timegm(&tmptm);
		CFNumberRef cftmptime = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongType, &tmptime);
		CFDictionarySetValue(effectivedict, CFSTR("passwordLastSetTime"), cftmptime);
		CFRelease(cftmptime);
	}
	passModDateCF = CFDateCreate(kCFAllocatorDefault, tmptime - kCFAbsoluteTimeIntervalSince1970);
	if(!passModDateCF) {
		passModDateCF = CFDateCreate(kCFAllocatorDefault, 0);
	}

	if(passwordRequiredDateAttr && passwordRequiredDateAttr->a_numvals && passwordRequiredDateAttr->a_nvals[0].bv_len) {
		strptime(passwordRequiredDateAttr->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
		tmptime = timegm(&tmptm);
	}
	passwordRequiredDateCF = CFDateCreate(kCFAllocatorDefault, tmptime - kCFAbsoluteTimeIntervalSince1970);

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


	// newPasswordRequired is a bit more complicated.
	// In PWS, when setting the global newPasswordRequired policy, PWS would
	// iterate over all user records setting the policy on the individual record,
	// then update the individual record as the password is changed.
	// That gets pretty inefficient, especially with ldap.
	// So instead we store the passwordRequiredDate when global newPasswordRequired
	// is set.  If the policy is set on the user's record, that always wins over global.
	// If not set on the user's record, but is set in the global policy, compare
	// the global passwordRequiredDate to the user's passwordModDate to see if they
	// changed it more recently than the policy was set.
	short tmpshort = 0;
	int zero = 0;
	CFNumberRef cfzero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);
	CFDictionarySetValue(effectivedict, CFSTR("newPasswordRequired"), cfzero);
	CFRelease(cfzero);
	CFNumberRef newPasswordRequired = CFDictionaryGetValue(userdict, CFSTR("newPasswordRequired"));
	if(newPasswordRequired) CFNumberGetValue(newPasswordRequired, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		CFDictionarySetValue(effectivedict, CFSTR("newPasswordRequired"), newPasswordRequired);
	} else {
		tmpshort = 0;
		newPasswordRequired = CFDictionaryGetValue(globaldict, CFSTR("newPasswordRequired"));
		if(newPasswordRequired) CFNumberGetValue(newPasswordRequired, kCFNumberShortType, &tmpshort);
		if(tmpshort) {
			if(passwordRequiredDateCF) {
				if((CFDateCompare(passwordRequiredDateCF, passModDateCF, NULL) == kCFCompareGreaterThan) || (CFDateCompare(passwordRequiredDateCF, passModDateCF, NULL) == kCFCompareEqualTo)) {
					CFDictionarySetValue(effectivedict, CFSTR("newPasswordRequired"), newPasswordRequired);
				}
			} else {
				Debug(LDAP_DEBUG_ANY, "%s: inconsistency detected, global newPasswordRequired policy set, but no passwordRequiredDate found in cn=access,cn=authdata\n", __func__, 0, 0);
			}
		}
	}

	long tmplong = 0;
	CFNumberRef minutesUntilFailedLoginReset = CFDictionaryGetValue(effectivedict, CFSTR("minutesUntilFailedLoginReset"));
	if(minutesUntilFailedLoginReset) CFNumberGetValue(minutesUntilFailedLoginReset, kCFNumberLongType, &tmplong);
	if(tmplong > 0) {
		modDate = attrs_find(e->e_attrs, slap_schema.si_ad_modifyTimestamp);
		tmptime = 0;
		if(modDate && modDate->a_numvals && modDate->a_nvals[0].bv_len) {
			strptime(modDate->a_nvals[0].bv_val, "%Y%m%d%H%M%SZ", &tmptm);
			tmptime = timegm(&tmptm);
		}

		if( time(NULL) >= (tmptime + (tmplong*60)) ) {
			loginattempts = 0;
			if(odusers_reset_failedlogin(dn) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: error resetting failed login count for %s\n", __func__, dn->bv_val, 0);
			}
		}
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

	GetDisabledStatus(effectivedict, creationDateCF, lastLoginCF, passModDateCF, &loginattempts, disableReasonInt);
	if(lastLoginCF) CFRelease(lastLoginCF);
	if(creationDateCF) CFRelease(creationDateCF);
	if(passModDateCF) CFRelease(passModDateCF);
	if(passwordRequiredDateCF) CFRelease(passwordRequiredDateCF);
	if(userdict) CFRelease(userdict);
	if(globaldict) CFRelease(globaldict);
	if (global_attr) attr_free(global_attr);
	if (passwordRequiredDateAttr) attr_free(passwordRequiredDateAttr);
	if (e) entry_free(e);
	return effectivedict;

out:
	if (global_attr) attr_free(global_attr);
	if (e) entry_free(e);
	if (globaldict) CFRelease(globaldict);
	return NULL;
}

int odusers_isdisabled(CFDictionaryRef policy) {
	short tmpshort = 0;
	CFNumberRef isdisabled = CFDictionaryGetValue(policy, CFSTR("isDisabled"));
	if(isdisabled) CFNumberGetValue(isdisabled, kCFNumberShortType, &tmpshort);
	if(tmpshort) {
		CFNumberRef disableReason = CFDictionaryGetValue(policy, CFSTR("effectiveDisableReason"));
		if(disableReason) {
			int tmpint;
			CFNumberGetValue(disableReason, kCFNumberIntType, &tmpint);
			if(tmpint) return tmpint;
		}
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

int odusers_reset_failedlogin(struct berval *dn) {
	int ret = -1;
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	SlapReply rs = {REP_RESULT};

	Modifications *mod = NULL;

	Entry *e = NULL;
	Attribute *failedLogins = NULL;
	const char *text = NULL;
	short optype = LDAP_MOD_ADD;

	if(!failedLoginsAD && slap_str2ad("loginFailedAttempts", &failedLoginsAD, &text) != 0) {
		Debug(LDAP_DEBUG_TRACE, "%s: Unable to retrieve description of loginFailedAttempts attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	mod = (Modifications *) ch_malloc(sizeof(Modifications));

	mod->sml_op = optype;
	mod->sml_flags = 0;
	mod->sml_type = failedLoginsAD->ad_cname;
	mod->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	mod->sml_values[0].bv_val = strdup("0");
	mod->sml_values[0].bv_len = 1;
	mod->sml_values[1].bv_val = NULL;
	mod->sml_values[1].bv_len = 0;
	mod->sml_numvals = 1;
	mod->sml_nvalues = NULL;

	mod->sml_desc = failedLoginsAD;
	mod->sml_next = NULL;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = *dn;
	dnNormalize(0, NULL, NULL, dn, &fakeop->o_ndn, NULL);
	fakeop->o_req_dn = e->e_name;
	fakeop->o_req_ndn = e->e_nname;
	fakeop->orm_modlist = mod;
	fakeop->o_tag = LDAP_REQ_MODIFY;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->o_bd = frontendDB;

	fakeop->o_bd->be_modify(fakeop, &rs);
	if(rs.sr_err != LDAP_SUCCESS) {
		//Debug(LDAP_DEBUG_ANY, "Unable to modify failedlogins for user %s: %d %s\n", fakeop->o_req_ndn.bv_val, rs.sr_err, rs.sr_text);
		goto out;
	}

	ret = 0;

out:
	if(fakeop && !BER_BVISNULL(&fakeop->o_ndn)) ch_free(fakeop->o_ndn.bv_val);
	if(e) entry_free(e);
	if(mod) slap_mods_free(mod, 1);
	return ret;
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

	mod = (Modifications *) ch_malloc(sizeof(Modifications));

	mod->sml_op = optype;
	mod->sml_flags = 0;
	mod->sml_type = failedLoginsAD->ad_cname;
	mod->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	mod->sml_values[0].bv_val = attemptsstr;
	mod->sml_values[0].bv_len = strlen(attemptsstr);
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
		mod->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
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
		mod->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
		mod->sml_values[0].bv_val = ch_calloc(256, 1);
		mod->sml_values[0].bv_len = strftime(mod->sml_values[0].bv_val, 256, "%Y%m%d%H%M%SZ", &tmptm);
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

char *odusers_copy_saslrealm(void) {
	OperationBuffer opbuf = {0};
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	char *ret = NULL;
	const char *text = NULL;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);

	if(root_dse_info(&conn, &e, &text) != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: root_dse_info failed\n", __func__, 0, 0);
		goto out;
	}
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry found\n", __func__, 0, 0);
		goto out;
	}

	a = attr_find(e->e_attrs, slap_schema.si_ad_saslRealm);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate sasl realm\n", __func__, 0, 0);
		goto out;
	}

	if(a->a_numvals == 0) {
		Debug(LDAP_DEBUG_ANY, "%s: No values returned\n", __func__, 0, 0);
		goto out;
	}

	ret = ch_calloc(a->a_nvals[0].bv_len +1, 1);
	if(!ret) goto out;
	memcpy(ret, a->a_nvals[0].bv_val, a->a_nvals[0].bv_len);

out:
	if(e) entry_free(e);
	return ret;
}

char *odusers_copy_suffix(void) {
	OperationBuffer opbuf = {0};
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	char *ret = NULL;
	const char *text = NULL;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);

	if(root_dse_info(&conn, &e, &text) != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: root_dse_info failed\n", __func__, 0, 0);
		goto out;
	}
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry found\n", __func__, 0, 0);
		goto out;
	}

	a = attr_find(e->e_attrs, slap_schema.si_ad_namingContexts);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate naming contexts\n", __func__, 0, 0);
		goto out;
	}

	if(a->a_numvals == 0) {
		Debug(LDAP_DEBUG_ANY, "%s: No values returned\n", __func__, 0, 0);
		goto out;
	}

	ret = ch_calloc(a->a_nvals[0].bv_len +1, 1);
	if(!ret) goto out;
	memcpy(ret, a->a_nvals[0].bv_val, a->a_nvals[0].bv_len);

out:
	if(e) entry_free(e);
	return ret;
}

/* Returns a newly allocated copy of the kerberos realm.
 * Caller is responsible for free()'ing the returned result.
 */
char *odusers_copy_krbrealm(Operation *op) {
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	const char *text = NULL;
	static char *savedrealm = NULL;
	char *ret = NULL;
	char *suffix = NULL;
	
	if(savedrealm) {
		return strdup(savedrealm);
	}

	suffix = strnstr(op->o_req_ndn.bv_val, "dc=", op->o_req_ndn.bv_len);
	if(!suffix) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate suffix of request %s", __func__, op->o_req_ndn.bv_val, 0);
		return NULL;
	}

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_req_dn.bv_len = asprintf(&fakeop->o_req_dn.bv_val, "cn=kerberoskdc,cn=config,%s", suffix);
	fakeop->o_req_dn.bv_len = strlen(fakeop->o_req_dn.bv_val);
	fakeop->o_dn = fakeop->o_ndn = fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->ors_attrs = NULL;

	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with KerberosKDC %s\n", __func__, fakeop->o_req_dn.bv_val, 0);
		goto out;
	}
	
	if(!realnameAD && slap_str2ad("apple-config-realname", &realnameAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for apple-config-realname", __func__, 0, 0);
		goto out;
	}

	a = attr_find(e->e_attrs, realnameAD);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate apple-config-realname attribute", __func__, 0, 0);
		goto out;
	}

	ret = calloc(1, a->a_vals[0].bv_len + 1);
	if(!ret) goto out;
	memcpy(ret, a->a_vals[0].bv_val, a->a_vals[0].bv_len);
	savedrealm = strdup(ret);

out:
	if(e) entry_free(e);
	if(fakeop && fakeop->o_req_dn.bv_val) free(fakeop->o_req_dn.bv_val);
	return ret;
}

char *odusers_copy_recname(Operation *op) {
	Attribute *attriter = NULL;
	char *recname = NULL;
	char *start = NULL;
	char *end = NULL;
	bool isuser = false;

	start = strnstr(op->o_req_dn.bv_val, "=", op->o_req_dn.bv_len);
	if(!start) return NULL;
	start++;
	end = strnstr(op->o_req_dn.bv_val, ",", op->o_req_dn.bv_len);
	if(!end) return NULL;
	recname = calloc(1, (end - start) + 1);
	if(!recname) return NULL;
	memcpy(recname, start, (end - start));

	return recname;
}

/* Returns a newly allocated copy of the primary master's IP address.
 * Caller is responsible for free()'ing the returned result.
 */
char *odusers_copy_primarymasterip(Operation *op) {
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	Entry *e = NULL;
	Attribute *a = NULL;
	const char *text = NULL;
	static char *savedprimarymasterip = NULL;
	char *ret = NULL;
	char *suffix = NULL;
	
	if(savedprimarymasterip) {
		return strdup(savedprimarymasterip);
	}

	suffix = strnstr(op->o_req_ndn.bv_val, "dc=", op->o_req_ndn.bv_len);
	if(!suffix) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate suffix of request %s", __func__, op->o_req_ndn.bv_val, 0);
		return NULL;
	}

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_req_dn.bv_len = asprintf(&fakeop->o_req_dn.bv_val, "cn=passwordserver,cn=config,%s", suffix);
	fakeop->o_req_dn.bv_len = strlen(fakeop->o_req_dn.bv_val);
	fakeop->o_dn = fakeop->o_ndn = fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->ors_attrs = NULL;

	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: No entry associated with passwordserver %s\n", __func__, fakeop->o_req_dn.bv_val, 0);
		goto out;
	}
	
	if(!pwslocAD && slap_str2ad("apple-password-server-location", &pwslocAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: slap_str2ad failed for apple-password-server-location", __func__, 0, 0);
		goto out;
	}

	a = attr_find(e->e_attrs, pwslocAD);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate apple-password-server-location attribute", __func__, 0, 0);
		goto out;
	}

	ret = calloc(1, a->a_vals[0].bv_len + 1);
	if(!ret) goto out;
	memcpy(ret, a->a_vals[0].bv_val, a->a_vals[0].bv_len);
	savedprimarymasterip = strdup(ret);

out:
	if(e) entry_free(e);
	if(fakeop && fakeop->o_req_dn.bv_val) free(fakeop->o_req_dn.bv_val);
	return ret;
}

static void ConvertHexToBinary( const char *inHexStr, unsigned char *outData, unsigned long *outLen )
{
	unsigned char *tptr = outData;
	unsigned char val;
   
	while ( *inHexStr && *(inHexStr+1) )
	{
		if ( *inHexStr >= 'a' )
			val = (*inHexStr - 'a' + 0x0A) << 4;
		else
			val = (*inHexStr - '0') << 4;

		inHexStr++;

		if ( *inHexStr >= 'a' )
			val += (*inHexStr - 'a' + 0x0A);
		else
			val += (*inHexStr - '0');

		inHexStr++;
		*tptr++ = val;
	}

	*outLen = (tptr - outData);
}

static bool ConvertBinaryToHex( const unsigned char *inData, long len, char *outHexStr )
{
	bool result = true;
	char *tptr = outHexStr;
	char base16table[16] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };

	if ( inData == nil || outHexStr == nil )
		return false;

	for ( int idx = 0; idx < len; idx++ )
	{
		*tptr++ = base16table[(inData[idx] >> 4) & 0x0F];
		*tptr++ = base16table[(inData[idx] & 0x0F)];
	}
	*tptr = '\0';

	return result;
}

int odusers_clear_authattr(char *authguid, char *attribute) {
	int ret = -1;
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	char *authdatadn = NULL;
	AttributeDescription *genericAD = NULL;
	Modifications *mhead = NULL;
	Modifications *m = NULL;
	SlapReply rs = {0};

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->o_req_dn.bv_len = asprintf(&authdatadn, "authGUID=%s,cn=users,cn=authdata", authguid);
	fakeop->o_req_dn.bv_val = authdatadn;
	fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_dn = fakeop->o_ndn = fakeop->o_req_ndn = fakeop->o_req_dn;
	fakeop->o_tag = LDAP_REQ_MODIFY;
	/* Internal ops, never replicate these */
	fakeop->orm_no_opattrs = 1;
	fakeop->o_dont_replicate = 1;

	const char *text = NULL;
	if(slap_str2ad(attribute, &genericAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for %s\n", __func__, attribute, 0);
		goto out;
	}
	m = ch_calloc(sizeof(Modifications), 1);
	m->sml_op = LDAP_MOD_DELETE;
	m->sml_flags = SLAP_MOD_INTERNAL;
	m->sml_type = genericAD->ad_cname;
	m->sml_desc = genericAD;
	m->sml_numvals = 0;
	m->sml_values = NULL;
	m->sml_nvalues = NULL;
	m->sml_next = mhead;
	mhead = m;

	fakeop->orm_modlist = mhead;
	fakeop->o_bd = select_backend(&fakeop->o_req_ndn, 1);
	if(!fakeop->o_bd) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate backend for %s\n", __func__, authdatadn, 0);
		goto out;
	}
	
	fakeop->o_callback = NULL;
	fakeop->o_bd->be_modify(fakeop, &rs);
	slap_mods_free(mhead, 1);

	// Ignore the return.  It will fail if any of the above attributes
	// don't exist, which is entirely likely.  We just want them all
	// gone if they exist.

	ret = 0;
out:
	// if (genericAD) ad_destroy(genericAD);
    if(authdatadn) free(authdatadn);
	return ret;
}

int odusers_clear_authhashes(char *authguid) {
	/* Each hash deletion needs to be its own modify operation,
	 * otherwise, if the removal of one fails (such as it doesn't
	 * exist), all subsequent mods in the same operation will
	 * not be processed.
	 */
	odusers_clear_authattr(authguid, "cmusaslsecretCRAM-MD5");
	odusers_clear_authattr(authguid, "cmusaslsecretDIGEST-MD5");
	odusers_clear_authattr(authguid, "cmusaslsecretDIGEST-UMD5");
	odusers_clear_authattr(authguid, "cmusaslsecretPPS");
	odusers_clear_authattr(authguid, "cmusaslsecretSMBNT");
	odusers_clear_authattr(authguid, "password");
	odusers_clear_authattr(authguid, "draft-krbKeySet");
	return 0;
}

int odusers_set_password(struct berval *dn, char *password, int isChangingOwnPassword) {
	int ret = -1;
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	Entry *usere = NULL;
	Entry *authe = NULL;
	Attribute *polattr = NULL;
	char authguid[37] = {0};
	char *recname = NULL;
	char *kerbrealm = NULL;
	char *saslrealm = NULL;
	char *authdatadn = NULL;
	char *suffix = NULL;
	CFErrorRef error = NULL;
	CFArrayRef keyset = NULL;
	CFStringRef principal = NULL;
	CFStringRef cfpassword = NULL;
	CFTypeRef encVals[] = {CFSTR("aes256-cts-hmac-sha1-96"), CFSTR("aes128-cts-hmac-sha1-96"), CFSTR("des3-cbc-sha1") };
	CFArrayRef enctypes = NULL;
	SlapReply rs = {0};
	CFArrayRef enabledmechArray = NULL;
	CFIndex i;
	Modifications *mhead = NULL;
	Modifications *m = NULL;
	const char *text = NULL;
	unsigned long tmplong;
	char needPlaintext = 0;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_req_dn = *dn;
	dnNormalize(0, NULL, NULL, dn, &fakeop->o_ndn, NULL);
	fakeop->o_req_ndn = fakeop->o_ndn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	usere = odusers_copy_entry(fakeop);
	if(!usere) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate user record for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	if( odusers_get_authguid(usere, authguid) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate authguid for user %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	recname = odusers_copy_recname(fakeop);
	if(!recname) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not identify record name for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	char slotid[37];
	int sloti, slotn;
	slotid[0] = '0';
	slotid[1] = 'x';
	for(sloti = 0, slotn = 2; slotn < 34 && sloti < 36; sloti++) {
		if( authguid[sloti] != '-' ) {
			slotid[slotn] = authguid[sloti];
			slotn++;
		}
	}
	slotid[slotn] = '\0';
	
	saslrealm = odusers_copy_saslrealm();
	if(!saslrealm) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not find sasl realm\n", __func__, 0, 0);
		goto out;
	}

	suffix = odusers_copy_suffix();
	if(!suffix) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not find default naming context\n", __func__, 0, 0);
		goto out;
	}

	enabledmechArray = odusers_copy_enabledmechs(suffix);
	if(!enabledmechArray) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not find enabled mech list\n", __func__, 0, 0);
		goto out;
	}

	for(i = 0; i < CFArrayGetCount(enabledmechArray); i++) {
		CFStringRef mech = CFArrayGetValueAtIndex(enabledmechArray, i);
		if(!mech) continue;
		if(CFStringCompare(mech, CFSTR("GSSAPI"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			kerbrealm = odusers_copy_krbrealm(fakeop);
			if(!kerbrealm) {
				Debug(LDAP_DEBUG_ANY, "%s: Could not find kerberos realm\n", __func__, dn->bv_val, 0);
				goto out;
			}

			principal = CFStringCreateWithFormat(NULL, NULL, CFSTR("%s@%s"), recname, kerbrealm);
			cfpassword = CFStringCreateWithCString(NULL, password, kCFStringEncodingUTF8);
			enctypes = CFArrayCreate(NULL, (const void**)&encVals, sizeof(encVals)/sizeof(*encVals), &kCFTypeArrayCallBacks);
			if (!enctypes)
			{  
				Debug(LDAP_DEBUG_ANY, "%s: Unable to create CFArray of enctypes\n", __func__, 0, 0);
				goto out;
			}
		
			keyset = HeimODModifyKeys(NULL, principal, enctypes, cfpassword, 0, &error);
			if (!keyset)
			{  
				Debug(LDAP_DEBUG_ANY, "%s: HeimODModifyKeys() returned a NULL keyset: %ld\n", __func__, error ? CFErrorGetCode(error) : 0, 0);   
				goto out;
			}
			if (CFArrayGetCount(keyset) == 0)
			{  
				Debug(LDAP_DEBUG_ANY, "%s: HeimODModifyKeys() returned an empty keyset\n", __func__,0, 0);
				goto out;
			}
		

			AttributeDescription *krbkeysAD = NULL;
			int n;
			
			if(slap_str2ad("draft-krbKeySet", &krbkeysAD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for draft-krbKeySet\n", __func__, 0, 0);
				goto out;
			}

			m = ch_calloc(sizeof(Modifications), 1);
			m->sml_op = LDAP_MOD_REPLACE;
			m->sml_flags = 0;
			m->sml_type = krbkeysAD->ad_cname;
			m->sml_desc = krbkeysAD;
			m->sml_numvals = CFArrayGetCount(keyset);
			m->sml_values = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_nvalues = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			for(n = 0; n < CFArrayGetCount(keyset); n++) {
				CFDataRef key = CFArrayGetValueAtIndex(keyset, n);
				m->sml_values[n].bv_len = CFDataGetLength(key);
				m->sml_values[n].bv_val = ch_calloc(CFDataGetLength(key), 1);
				memcpy(m->sml_values[n].bv_val, (void*)CFDataGetBytePtr(key), CFDataGetLength(key));
		
				m->sml_nvalues[n].bv_len = CFDataGetLength(key);
				m->sml_nvalues[n].bv_val = ch_calloc(CFDataGetLength(key), 1);
				memcpy(m->sml_nvalues[n].bv_val, (void*)CFDataGetBytePtr(key), CFDataGetLength(key));
			}
			m->sml_next = mhead;
			mhead = m;
		}

		if(CFStringCompare(mech, CFSTR("CRAM-MD5"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			AttributeDescription *crammd5AD = NULL;
			heim_CRAM_MD5_STATE crammd5state;
			heim_cram_md5_export(password, &crammd5state);
			if(slap_str2ad("cmusaslsecretCRAM-MD5", &crammd5AD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for cmusaslsecretCRAM-MD5\n", __func__, 0, 0);
				goto out;
			}
			m = ch_calloc(sizeof(Modifications), 1);
			m->sml_op = LDAP_MOD_REPLACE;
			m->sml_flags = 0;
			m->sml_type = crammd5AD->ad_cname;
			m->sml_desc = crammd5AD;
			m->sml_numvals = 1;
			m->sml_values = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_nvalues = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_values[0].bv_len = sizeof(crammd5state);
			m->sml_values[0].bv_val = ch_calloc(sizeof(crammd5state), 1);
			memcpy(m->sml_values[0].bv_val, &crammd5state, sizeof(crammd5state));
			m->sml_nvalues[0].bv_len = sizeof(crammd5state);
			m->sml_nvalues[0].bv_val = ch_calloc(sizeof(crammd5state), 1);
			memcpy(m->sml_nvalues[0].bv_val, &crammd5state, sizeof(crammd5state));
			m->sml_next = mhead;
			mhead = m;
		}
	
		if((CFStringCompare(mech, CFSTR("MS-CHAPv2"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) || (CFStringCompare(mech, CFSTR("NTLM"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) || (CFStringCompare(mech, CFSTR("SMB-NT"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) || (CFStringCompare(mech, CFSTR("SMB-NTLMv2"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
			AttributeDescription *ntkeyAD = NULL;
			struct ntlm_buf ntlmstate;
			int rc;
			rc = heim_ntlm_nt_key(password, &ntlmstate);
			if(rc != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: heim_ntlm_nt_key returned %d\n", __func__, rc, 0);
				goto out;
			}
			if(slap_str2ad("cmusaslsecretSMBNT", &ntkeyAD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for cmusaslsecretSMBNT\n", __func__, 0, 0);
				goto out;
			}
			m = ch_calloc(sizeof(Modifications), 1);
			m->sml_op = LDAP_MOD_REPLACE;
			m->sml_flags = 0;
			m->sml_type = ntkeyAD->ad_cname;
			m->sml_desc = ntkeyAD;
			m->sml_numvals = 1;
			m->sml_values = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_nvalues = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_values[0].bv_len = ntlmstate.length * 2;
			m->sml_values[0].bv_val = ch_calloc(m->sml_values[0].bv_len + 1, 1);
			ConvertBinaryToHex(ntlmstate.data, ntlmstate.length, m->sml_values[0].bv_val);
			heim_ntlm_free_buf(&ntlmstate);
			m->sml_nvalues[0].bv_len = m->sml_values[0].bv_len;
			m->sml_nvalues[0].bv_val = ch_calloc(m->sml_nvalues[0].bv_len, 1);
			memcpy(m->sml_nvalues[0].bv_val, m->sml_values[0].bv_val, m->sml_nvalues[0].bv_len);
			m->sml_next = mhead;
			mhead = m;
		}
		if(CFStringCompare(mech, CFSTR("DIGEST-MD5"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			AttributeDescription *digestmd5AD = NULL;
			/* Convert authguid into slotid for use with digest-md5 hash */
			char *digest_userhash = heim_digest_userhash(slotid, saslrealm, password);
			if(slap_str2ad("cmusaslsecretDIGEST-MD5", &digestmd5AD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for cmusaslsecretDIGEST-MD5\n", __func__, 0, 0);
				goto out;
			}
			m = ch_calloc(sizeof(Modifications), 1);
			m->sml_op = LDAP_MOD_REPLACE;
			m->sml_flags = 0;
			m->sml_type = digestmd5AD->ad_cname;
			m->sml_desc = digestmd5AD;
			m->sml_numvals = 1;
			m->sml_values = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_nvalues = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_values[0].bv_len = tmplong = 16;
			m->sml_values[0].bv_val = ch_calloc(m->sml_values[0].bv_len+1, 1);
			ConvertHexToBinary(digest_userhash, (unsigned char*)m->sml_values[0].bv_val, &tmplong);
			free(digest_userhash);
			m->sml_nvalues[0].bv_len = m->sml_values[0].bv_len;
			m->sml_nvalues[0].bv_val = ch_calloc(m->sml_nvalues[0].bv_len, 1);
			memcpy(m->sml_nvalues[0].bv_val, m->sml_values[0].bv_val, m->sml_nvalues[0].bv_len);
			m->sml_next = mhead;
			mhead = m;
		}

		if((CFStringCompare(mech, CFSTR("DIGEST-MD5"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) || (CFStringCompare(mech, CFSTR("WEBDAV-DIGEST"), kCFCompareCaseInsensitive) == kCFCompareEqualTo)) {
			AttributeDescription *digestumd5AD = NULL;
			char *udigest_userhash = heim_digest_userhash(recname, saslrealm, password);
			if(slap_str2ad("cmusaslsecretDIGEST-UMD5", &digestumd5AD, &text) != 0) {
				Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for cmusaslsecretDIGEST-UMD5\n", __func__, 0, 0);
				goto out;
			}
			needPlaintext = 1;
			m = ch_calloc(sizeof(Modifications), 1);
			m->sml_op = LDAP_MOD_REPLACE;
			m->sml_flags = 0;
			m->sml_type = digestumd5AD->ad_cname;
			m->sml_desc = digestumd5AD;
			m->sml_numvals = 1;
			m->sml_values = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_nvalues = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
			m->sml_values[0].bv_len = tmplong = 16;
			m->sml_values[0].bv_val = ch_calloc(m->sml_values[0].bv_len+1, 1);
			ConvertHexToBinary(udigest_userhash, (unsigned char*)m->sml_values[0].bv_val, &tmplong);
			free(udigest_userhash);
			m->sml_nvalues[0].bv_len = m->sml_values[0].bv_len;
			m->sml_nvalues[0].bv_val = ch_calloc(m->sml_nvalues[0].bv_len, 1);
			memcpy(m->sml_nvalues[0].bv_val, m->sml_values[0].bv_val, m->sml_nvalues[0].bv_len);
			m->sml_next = mhead;
			mhead = m;
		}

		if(CFStringCompare(mech, CFSTR("APOP"), kCFCompareCaseInsensitive) == kCFCompareEqualTo) {
			needPlaintext = 1;
		}
	}

	if(needPlaintext) {
		AttributeDescription *passwordAD = NULL;
		int encodeLen = strlen(password);
		encodeLen += (kFixedDESChunk - (encodeLen % kFixedDESChunk));

		if(slap_str2ad("password", &passwordAD, &text) != 0) {
			Debug(LDAP_DEBUG_ANY, "%s: Could not find attribute description for password\n", __func__, 0, 0);
			goto out;
		}
		m = ch_calloc(sizeof(Modifications), 1);
		m->sml_op = LDAP_MOD_REPLACE;
		m->sml_flags = 0;
		m->sml_type = passwordAD->ad_cname;
		m->sml_desc = passwordAD;
		m->sml_numvals = 1;
		m->sml_values = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
		m->sml_nvalues = ch_calloc(sizeof(struct berval), m->sml_numvals+1);
		m->sml_values[0].bv_len = encodeLen;
		m->sml_values[0].bv_val = ch_calloc(m->sml_values[0].bv_len+1, 1);
		strlcpy(m->sml_values[0].bv_val, password, encodeLen);
		pwsf_DESEncode(m->sml_values[0].bv_val, encodeLen);
		
		m->sml_nvalues[0].bv_len = m->sml_values[0].bv_len;
		m->sml_nvalues[0].bv_val = ch_calloc(m->sml_nvalues[0].bv_len, 1);
		memcpy(m->sml_nvalues[0].bv_val, m->sml_values[0].bv_val, m->sml_nvalues[0].bv_len);
		m->sml_next = mhead;
		mhead = m;
	}

	if(!policyAD && slap_str2ad("apple-user-passwordpolicy", &policyAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of apple-user-passwordpolicy  attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	if(!passModDateAD && slap_str2ad("passwordModDate", &passModDateAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of passwordModDate attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}

	authe = odusers_copy_authdata(dn);
	if(!authe) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate authdata for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	polattr = attr_find(authe->e_attrs, policyAD);
	if(polattr) {
		// The attribute not existing is a normal case and requires no action.
		CFMutableDictionaryRef poldict = CopyPolicyToDict(polattr->a_vals[0].bv_val, polattr->a_vals[0].bv_len);
		if(!poldict) {
			Debug(LDAP_DEBUG_ANY, "%s: Could not convert policy to CFDictionary for %s\n", __func__, dn->bv_val, 0);
			goto out;
		}

		if(isChangingOwnPassword) {
			short tmpshort = 0;
			CFNumberRef newPasswordRequired = CFDictionaryGetValue(poldict, CFSTR("newPasswordRequired"));
			if(newPasswordRequired) CFNumberGetValue(newPasswordRequired, kCFNumberShortType, &tmpshort);
			if(tmpshort > 0) {
				struct berval *berdict = NULL;
				tmpshort = 0;
				newPasswordRequired = CFNumberCreate(kCFAllocatorDefault, kCFNumberShortType, &tmpshort);
				CFDictionarySetValue(poldict, CFSTR("newPasswordRequired"), newPasswordRequired);
				CFRelease(newPasswordRequired);
				berdict = odusers_copy_dict2bv(poldict);
				m = ch_calloc(sizeof(Modifications), 1);
				m->sml_op = LDAP_MOD_REPLACE;
				m->sml_flags = 0;
				m->sml_type = policyAD->ad_cname;
				m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
				m->sml_values[0].bv_val = berdict->bv_val;
				m->sml_values[0].bv_len = berdict->bv_len;
				// Free just the ber struct, the values will be freed later
				ch_free(berdict);
				m->sml_values[1].bv_val = NULL;
				m->sml_values[1].bv_len = 0;
				m->sml_nvalues = NULL;
				m->sml_numvals = 1;
				m->sml_desc = policyAD;
				m->sml_next = mhead;
				mhead = m;
			}
		}
		CFRelease(poldict);
	}

	time_t tmptime;
	struct tm tmptm;
	tmptime = time(NULL);
	gmtime_r(&tmptime, &tmptm);
	m = ch_calloc(sizeof(Modifications), 1);
	m->sml_op = LDAP_MOD_REPLACE;
	m->sml_flags = 0;
	m->sml_type = passModDateAD->ad_cname;
	m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	m->sml_values[0].bv_val = ch_calloc(256, 1);
	m->sml_values[0].bv_len = strftime(m->sml_values[0].bv_val, 256, "%Y%m%d%H%M%SZ", &tmptm);
	m->sml_values[1].bv_val = NULL;
	m->sml_values[1].bv_len = 0;
	m->sml_nvalues = NULL;
	m->sml_numvals = 1;
	m->sml_desc = passModDateAD;
	m->sml_next = mhead;
	mhead = m;

	if(!failedLoginsAD && slap_str2ad("loginFailedAttempts", &failedLoginsAD, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: Unable to retrieve description of loginFailedAttempts attribute", __PRETTY_FUNCTION__, 0, 0);
		goto out;
	}
	m = (Modifications *) ch_malloc(sizeof(Modifications));

	m->sml_op = LDAP_MOD_REPLACE;
	m->sml_flags = 0;
	m->sml_type = failedLoginsAD->ad_cname;
	m->sml_values = (struct berval*) ch_malloc(2 * sizeof(struct berval));
	m->sml_values[0].bv_val = strdup("0");
	m->sml_values[0].bv_len = 1;
	m->sml_values[1].bv_val = NULL;
	m->sml_values[1].bv_len = 0;
	m->sml_numvals = 1;
	m->sml_nvalues = NULL;

	m->sml_desc = failedLoginsAD;
	m->sml_next = mhead;
	mhead = m;

	odusers_clear_authhashes(authguid);

	OperationBuffer opbuf2 = {0};
	Operation *fakeop2 = NULL;
	Connection conn2 = {0};

	connection_fake_init2(&conn2, &opbuf2, ldap_pvt_thread_pool_context(), 0);
	fakeop2 = &opbuf2.ob_op;
	fakeop2->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop2->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	fakeop2->o_req_dn.bv_len = asprintf(&authdatadn, "authGUID=%s,cn=users,cn=authdata", authguid);
	fakeop2->o_req_dn.bv_val = authdatadn;
	fakeop2->o_req_ndn = fakeop2->o_req_dn;
	fakeop2->o_tag = LDAP_REQ_MODIFY;
	fakeop2->orm_modlist = mhead;
	fakeop2->o_bd = select_backend(&fakeop2->o_req_ndn, 1);
	if(!fakeop2->o_bd) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate backend for %s\n", __func__, authdatadn, 0);
		goto out;
	}
	
	fakeop2->o_callback = NULL;
	fakeop2->o_bd->be_modify(fakeop2, &rs);
	slap_mods_free(mhead, 1);
	if(rs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "%s: Error modifying authdata with new password\n", __func__, 0, 0);
		goto out;
	}

	ret = 0;
out:
	if(usere) entry_free(usere);
	if(authe) entry_free(authe);
	if(suffix) free(suffix);
	if(enabledmechArray) CFRelease(enabledmechArray);
	if(keyset) CFRelease(keyset);
	if(enctypes) CFRelease(enctypes);
	if(principal) CFRelease(principal);
	if(cfpassword) CFRelease(cfpassword);
	if(kerbrealm) free(kerbrealm);
	if(recname) free(recname);
	if(authdatadn) free(authdatadn);
	if(saslrealm) free(saslrealm);
	if ( !BER_BVISNULL( &fakeop->o_ndn ) ) ber_memfree( fakeop->o_ndn.bv_val );
	return ret;
}

int odusers_verify_passwordquality(const char *password, const char *username, CFDictionaryRef effectivepolicy, SlapReply *rs) {
	int requiresAlpha = 0;
	int requiresNumeric = 0;
	int requiresSymbol = 0;
	int requiresMixedCase = 0;
	unsigned int minChars = 0;
	unsigned int maxChars = 0;
	int passwordCannotBeName = 0;
	CFNumberRef tmpnum;
	int len = 0;

	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("requiresAlpha"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &requiresAlpha);
	}
	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("requiresNumeric"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &requiresNumeric);
	}
	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("minChars"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &minChars);
	}
	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("requiresSymbol"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &requiresSymbol);
	}
	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("requiresMixedCase"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &requiresMixedCase);
	}
	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("maxChars"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &maxChars);
	}
	tmpnum = CFDictionaryGetValue(effectivepolicy, CFSTR("passwordCannotBeName"));
	if(tmpnum) {
		CFNumberGetValue(tmpnum, kCFNumberIntType, &passwordCannotBeName);
	}

	len = strlen(password);
	if(len == 0) {
		rs->sr_text = "too short";
		rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
		return rs->sr_err;
	}

	if(len < minChars) {
		rs->sr_text = "too short";
		rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
		return rs->sr_err;
	}

	if(maxChars > 0 && len > maxChars) {
		rs->sr_text = "too long";
		rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
		return rs->sr_err;
	}

	if(requiresAlpha) {
		bool hasAlpha = false;
		int index;
		for(index = 0; index < len; index++) {
			if(isalpha(password[index])) {
				hasAlpha = true;
				break;
			}
		}

		if(!hasAlpha) {
			rs->sr_text = "Needs alpha";
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			return rs->sr_err;
		}
	}

	if(requiresNumeric) {
		bool hasNumeric = false;
		int index;
		for(index = 0; index < len; index++) {
			if(isdigit(password[index])) {
				hasNumeric = true;
				break;
			}
		}

		if(!hasNumeric) {
			rs->sr_text = "Needs number";
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			return rs->sr_err;
		}
	}

	if(requiresMixedCase) {
		bool hasUpper = false;
		bool hasLower = false;
		int index;
		for(index = 0; index < len; index++) {
			if(password[index] >= 'A' && password[index] <= 'Z') {
				hasUpper = true;
			} else if(password[index] >= 'a' && password[index] <= 'z') {
				hasLower = true;
			}
			if(hasUpper && hasLower) {
				break;
			}
		}

		if(!(hasUpper && hasLower)) {
			rs->sr_text = "Needs mixed case";
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			return rs->sr_err;
		}
	}

	if(requiresSymbol) {
		bool hasSymbol = false;
		int index;
		for(index = 0; index < len; index++) {
			if(password[index] >= 'A' && password[index] <= 'Z') {
				continue;
			}
			if(password[index] >= 'a' && password[index] <= 'z') {
				continue;
			}
			if(password[index] >= '0' && password[index] <= '9') {
				continue;
			}
			hasSymbol = true;
			break;
		}
		
		if(!hasSymbol) {
			rs->sr_text = "Needs symbol";
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			return rs->sr_err;
		}
	}

	if(passwordCannotBeName) {
		uint16_t unamelen = strlen(username);
		uint16_t smallerlen = ((len < unamelen) ? len : unamelen);

		if(strncasecmp(password, username, smallerlen) == 0) {
			rs->sr_text = "Cannot be username";
			rs->sr_err = LDAP_UNWILLING_TO_PERFORM;
			return rs->sr_err;
		}
	}

	return 0;
}

CFArrayRef odusers_copy_enabledmechs(const char *suffix) {
	OperationBuffer opbuf;
	Connection conn;
	Operation *fakeop = NULL;
	Entry *e = NULL;
	struct berval dn;
	CFMutableArrayRef ret = NULL;
	char *searchdn = NULL;
	
	dn.bv_len = asprintf(&searchdn, "cn=dirserv,cn=config,%s", suffix);
	dn.bv_val = searchdn;
	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_req_dn = dn;
	dnNormalize(0, NULL, NULL, &dn, &fakeop->o_req_ndn, NULL);
	fakeop->o_ndn = fakeop->o_req_ndn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate %s\n", __func__, searchdn, 0);
		goto out;
	}

	Attribute *a;
	AttributeDescription *ad = NULL;
	const char *text = NULL;
	if(slap_str2ad("apple-enabled-auth-mech", &ad, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: could not get attribute description for apple-enabled-auth-mech\n", __func__, 0, 0);
		goto out;
	}
	a = attr_find(e->e_attrs, ad);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate any apple-enabled-auth-mech attributes\n", __func__, 0, 0);
		goto out;
	}

	int i;
	ret = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	if(!ret) {
		Debug(LDAP_DEBUG_ANY, "%s: could not create mutable array\n", __func__, 0, 0);
		goto out;
	}
	for(i = 0; i < a->a_numvals; i++) {
		CFStringRef mech = CFStringCreateWithCString(NULL, a->a_vals[i].bv_val, kCFStringEncodingUTF8);
		if(!mech) {
			Debug(LDAP_DEBUG_ANY, "%s: could not process mech %s\n", __func__, a->a_vals[i].bv_val, 0);
			continue;
		}
		CFArrayAppendValue(ret, mech);
		CFRelease(mech);
	}
out:
	if(e) entry_free(e);
	if(searchdn) free(searchdn);
	if ( !BER_BVISNULL( &fakeop->o_req_ndn ) ) ber_memfree( fakeop->o_req_ndn.bv_val );

	return ret;
}

int odusers_krb_auth(Operation *op, char *password) {
	char *name = odusers_copy_recname(op);
	char *realm = odusers_copy_krbrealm(op);
	krb5_error_code problem;
	krb5_context krbctx = NULL;
	krb5_principal princ = NULL;
	krb5_creds creds = {0};
	int ret = -1;

	if(!name) {
		Debug(LDAP_DEBUG_ANY, "%s: could not retrieve record name\n", __func__, 0, 0);
		goto out;
	}
	if(!realm) {
		Debug(LDAP_DEBUG_ANY, "%s: could not retrieve krb realm while authing %s\n", __func__, name, 0);
		goto out;
	}

	problem = krb5_init_context(&krbctx);
	if(problem) {
		Debug(LDAP_DEBUG_ANY, "%s: Error initting krb ctx for %s: %d\n", __func__, name, problem);
		goto out;
	}

	problem = krb5_build_principal(krbctx, &princ, (int)strlen(realm), realm, name, NULL);
	if(problem) {
		Debug(LDAP_DEBUG_ANY, "%s: Error building principal for %s: %d", __func__, name, problem);
		goto out;
	}

	problem = krb5_get_init_creds_password(krbctx, &creds, princ, password, NULL, 0, 0, NULL, NULL);
	if(problem) {
		Debug(LDAP_DEBUG_ANY, "%s: Error obtaining credentials for %s: %d", __func__, name, problem);
		goto out;
	}

	ret = 0;
out:
	if (name)
		free(name);
	if (realm)
		free(realm);		
	if (krbctx) {
		if (princ)
			krb5_free_principal(krbctx, princ);
		krb5_free_cred_contents(krbctx, &creds);
		krb5_free_context(krbctx);
	}
	return ret;
}

char *odusers_copy_owner(struct berval *dn) {
	OperationBuffer opbuf;
	Connection conn;
	Operation *fakeop = NULL;
	Entry *e = NULL;
	char *ret = NULL;
	Attribute *a = NULL;
	
	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_ndn = *dn;
	fakeop->o_req_dn = fakeop->o_req_ndn = *dn;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");

	e = odusers_copy_entry(fakeop);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	a = attr_find(e->e_attrs, slap_schema.si_ad_creatorsName);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: could not locate creatorsName attribute for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	if(a->a_numvals < 1) {
		Debug(LDAP_DEBUG_ANY, "%s: no values associated with creatorsName for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	ret = calloc(1,a->a_nvals[0].bv_len + 1);
	if(!ret) goto out;
	memcpy(ret, a->a_nvals[0].bv_val, a->a_nvals[0].bv_len);
out:
	if(e) entry_free(e);
	return ret;
}

int odusers_store_history(struct berval *dn, const char *password, CFDictionaryRef policy) {
	Entry *e = NULL;
	int ret = -1;
	Attribute *a = NULL;
	AttributeDescription *ad = NULL;
	const char *text = NULL;
	heim_CRAM_MD5_STATE crammd5state;
	char *history = NULL;
	int i;
	short historyCount = 0;
	char newhistory[sizeof(heim_CRAM_MD5_STATE)*16];
    Modifications *mod = NULL;
	short optype = LDAP_MOD_ADD;
	OperationBuffer opbuf = {0};
	Operation *fakeop = NULL;
	Connection conn = {0};
	SlapReply rs = {REP_RESULT};

	if(!policy) return -1;

	CFNumberRef usingHistory = CFDictionaryGetValue(policy, CFSTR("usingHistory"));
	if(!usingHistory) return -1;

	CFNumberGetValue(usingHistory, kCFNumberShortType, &historyCount);
	if(!historyCount) return -1;
	if(historyCount > 16) historyCount = 16;

	heim_cram_md5_export(password, &crammd5state);

	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate authdata for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	if(slap_str2ad("historyData", &ad, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: could not get attribute description for historyData\n", __func__, 0, 0);
		goto out;
	}
	a = attr_find(e->e_attrs, ad);
	if(a) {
		if(a->a_numvals < 1) {
			Debug(LDAP_DEBUG_ANY, "%s: history attribute lacks values for %s\n", __func__, dn->bv_val, 0);
			ret = 0;
			goto out;
		}

		if(a->a_nvals[0].bv_len > (16*sizeof(crammd5state))) {
			Debug(LDAP_DEBUG_ANY, "%s: history data is larger than expected for %s\n", __func__, dn->bv_val, 0);
			goto out;
		}

		// If the password already exists in the history, return success
		history = a->a_nvals[0].bv_val;
		for(i = 0; i < historyCount; i++) {
			if(memcmp(&crammd5state, history, sizeof(crammd5state)) == 0) {
				ret = 0;
				goto out;
			}
			history += sizeof(crammd5state);
		}
	
		history = a->a_nvals[0].bv_val;
		memcpy(newhistory, a->a_nvals[0].bv_val, a->a_nvals[0].bv_len);
		for(i = 15; i >= 1; i--) {
			memcpy(newhistory + i*sizeof(crammd5state), newhistory + (i-1)*sizeof(crammd5state), sizeof(crammd5state));
		}
		optype = LDAP_MOD_REPLACE;
	}

	memcpy(newhistory, &crammd5state, sizeof(crammd5state));
	if((historyCount-1) < 15) {
		bzero(newhistory + historyCount*sizeof(crammd5state), sizeof(newhistory) - historyCount*sizeof(crammd5state));
	}

    mod = (Modifications *)ch_malloc(sizeof(Modifications));
    mod->sml_op = optype;
	mod->sml_flags = 0;
	mod->sml_type = ad->ad_cname;
	mod->sml_values = (struct berval*)ch_malloc(2 * sizeof(struct berval));
	mod->sml_values[0].bv_val = newhistory;
	mod->sml_values[0].bv_len = sizeof(newhistory);
	mod->sml_values[1].bv_len = 0;
	mod->sml_values[1].bv_val = NULL;
	mod->sml_numvals = 1;
	mod->sml_nvalues = NULL;
	mod->sml_desc = ad;
	mod->sml_next = NULL;

	connection_fake_init2(&conn, &opbuf, ldap_pvt_thread_pool_context(), 0);
	fakeop = &opbuf.ob_op;
	fakeop->o_dn = fakeop->o_ndn = *dn;
	fakeop->o_req_dn = e->e_name;
	fakeop->o_req_ndn = e->e_nname;
	fakeop->orm_modlist = mod;
	fakeop->o_tag = LDAP_REQ_MODIFY;
	fakeop->o_conn->c_listener->sl_url.bv_val = "ldapi://%2Fvar%2Frun%2Fldapi";
	fakeop->o_conn->c_listener->sl_url.bv_len = strlen("ldapi://%2Fvar%2Frun%2Fldapi");
	fakeop->o_bd = frontendDB;
	fakeop->o_bd->be_modify(fakeop, &rs);
	if(rs.sr_err != LDAP_SUCCESS) {
		Debug(LDAP_DEBUG_ANY, "Unable to modify history for %s: %d %s\n", dn->bv_val, rs.sr_err, rs.sr_text);
		goto out;
	}

	ret = 0;
out:
	if(e) entry_free(e);
	return ret;
}

int odusers_check_history(struct berval *dn, const char *password, CFDictionaryRef policy) {
	Entry *e = NULL;
	int ret = -1;
	Attribute *a = NULL;
	AttributeDescription *ad = NULL;
	const char *text = NULL;
	heim_CRAM_MD5_STATE crammd5state;
	char *history = NULL;
	int i;
	short historyCount = 0;

	if(!policy) return -1;

	CFNumberRef usingHistory = CFDictionaryGetValue(policy, CFSTR("usingHistory"));
	if(!usingHistory) return 0;

	CFNumberGetValue(usingHistory, kCFNumberShortType, &historyCount);
	if(!historyCount) return 0;
	if(historyCount > 16) historyCount = 16;

	e = odusers_copy_authdata(dn);
	if(!e) {
		Debug(LDAP_DEBUG_ANY, "%s: Could not locate authdata for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	if(slap_str2ad("historyData", &ad, &text) != 0) {
		Debug(LDAP_DEBUG_ANY, "%s: could not get attribute description for historyData\n", __func__, 0, 0);
		goto out;
	}
	a = attr_find(e->e_attrs, ad);
	if(!a) {
		Debug(LDAP_DEBUG_ANY, "%s: no history data found for %s\n", __func__, dn->bv_val, 0);
		ret = 0;
		goto out;
	}

	if(a->a_numvals < 1) {
		Debug(LDAP_DEBUG_ANY, "%s: history attribute lacks values for %s\n", __func__, dn->bv_val, 0);
		ret = 0;
		goto out;
	}

	if(a->a_nvals[0].bv_len > (16*sizeof(crammd5state))) {
		Debug(LDAP_DEBUG_ANY, "%s: history data is larger than expected for %s\n", __func__, dn->bv_val, 0);
		goto out;
	}

	heim_cram_md5_export(password, &crammd5state);
	
	history = a->a_nvals[0].bv_val;
	for(i = 0; i < historyCount; i++) {
		if(memcmp(&crammd5state, history, sizeof(crammd5state)) == 0) {
			ret = 1;
			goto out;
		}
		history += sizeof(crammd5state);
	}

	ret = 0;
out:
	if(e) entry_free(e);
	return ret;
}

char *CopyPrimaryIPv4Address(void)
{
	char *ret = NULL;
	char *primaryInterfaceNameCStr = NULL;

	SCDynamicStoreRef session = SCDynamicStoreCreate(NULL, CFSTR("org.openldap.slapd"), NULL, NULL);
	if (session == NULL) {
	    return NULL;
	}
	
	CFStringRef primaryInterfaceKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, (CFStringRef)kSCDynamicStoreDomainState, (CFStringRef)kSCEntNetIPv4); 
	if (primaryInterfaceKey != NULL) {
	    CFDictionaryRef primaryInterfaceDict = SCDynamicStoreCopyValue(session, primaryInterfaceKey);
	    if (primaryInterfaceDict) {
		if (CFGetTypeID(primaryInterfaceDict) == CFDictionaryGetTypeID()) {
		    CFStringRef primaryInterfaceName = CFDictionaryGetValue(primaryInterfaceDict, kSCDynamicStorePropNetPrimaryInterface);
		    if (primaryInterfaceName) {
			CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(primaryInterfaceName), kCFStringEncodingUTF8) + 1;
			primaryInterfaceNameCStr = malloc(size);
			if (primaryInterfaceNameCStr) {
			    if (!CFStringGetCString(primaryInterfaceName, primaryInterfaceNameCStr, size, kCFStringEncodingUTF8)) {
				free(primaryInterfaceNameCStr);
				primaryInterfaceNameCStr = NULL;
			    }
			}
		    }
		}
		CFRelease(primaryInterfaceDict);
	    }
	    CFRelease(primaryInterfaceKey);
	}
	CFRelease(session);
	session = NULL;

	if (!primaryInterfaceNameCStr) {
	    return NULL;
	}
	
	struct ifaddrs *ifap = NULL;
	if (getifaddrs(&ifap) != 0) {
	    return NULL;
	}
	
	struct ifaddrs *ifa;
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next)
	{
	    if (ifa->ifa_name == NULL) continue;
	    if (ifa->ifa_addr == NULL) continue;
	    if (strncmp(ifa->ifa_name, "lo", 2) == 0) continue;
	    if (ifa->ifa_addr->sa_family == AF_INET) {
		if (strcmp(ifa->ifa_name, primaryInterfaceNameCStr) == 0) {
		    char ipv4AddressCStr[INET_ADDRSTRLEN] = { 0 };
		    if (inet_ntop(AF_INET, &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr.s_addr, ipv4AddressCStr, sizeof(ipv4AddressCStr))) {
			ret = strdup(ipv4AddressCStr);
			break;
		    }
		}
	    }
	}
	freeifaddrs(ifap);
	
	return ret;
}
