/* tls_st.c - Handle tls/ssl using SecureTransport */

#include "portable.h"

#ifdef HAVE_SECURE_TRANSPORT

#include "ldap_config.h"

#include <stdio.h>

#include <ac/stdlib.h>
#include <ac/errno.h>
#include <ac/socket.h>
#include <ac/string.h>
#include <ac/ctype.h>
#include <ac/time.h>
#include <ac/unistd.h>
#include <ac/param.h>
#include <ac/dirent.h>

#include "ldap-int.h"
#include "ldap-tls.h"

#include <Security/Security.h>
#include <CoreDaemon/CoreDaemon.h>
#include <syslog.h>
#include <sys/stat.h>

#define SYSTEM_KEYCHAIN_PATH  "/Library/Keychains/System.keychain"

#define CFRelease_and_null(obj)	do { CFRelease(obj); (obj) = NULL; } while (0)

typedef struct tlsst_ctx {
	int refcount;
	SSLProtocol protocol_min;
	int require_cert;
	int crl_check;
	SSLCipherSuite *ciphers;
	size_t ciphers_count;
	CFArrayRef identity_certs;
	CFArrayRef trusted_certs;
	CFDataRef dhparams;
#ifdef LDAP_R_COMPILE
	ldap_pvt_thread_mutex_t refmutex;
#endif
} tlsst_ctx;

typedef struct tlsst_session {
	SSLContextRef ssl;
	tlsst_ctx *ctx;
	char *last_error;

	CFMutableDataRef subject_data;
	int subject_result;
	CFMutableDataRef issuer_data;
	int issuer_result;

	Sockbuf_IO_Desc *sbiod;

	unsigned char sslv2_detect_bytes[3];

	unsigned int is_server : 1;
	unsigned int subject_cached : 1;
	unsigned int issuer_cached : 1;
	unsigned int want_read : 1;
	unsigned int want_write : 1;
	unsigned int cert_received : 1;
	unsigned int cert_trusted : 1;
	unsigned int sslv2_detect_length : 2;
} tlsst_session;

static int tlsst_session_strength(tls_session *_sess);

static struct {
	int ldap_protocol;
	SSLProtocol st_protocol;
} tlsst_protocol_map[] = {
	LDAP_OPT_X_TLS_PROTOCOL_SSL2,		kSSLProtocol2,
	LDAP_OPT_X_TLS_PROTOCOL_SSL3,		kSSLProtocol3,
	LDAP_OPT_X_TLS_PROTOCOL_TLS1_0,		kTLSProtocol1,
	LDAP_OPT_X_TLS_PROTOCOL_TLS1_1,		kTLSProtocol11,
	LDAP_OPT_X_TLS_PROTOCOL_TLS1_2,		kTLSProtocol12
};

static SSLProtocol
tlsst_protocol_map_ldap2st(int ldap_protocol, const char *setting_name)
{
	for (int i = 0; i < sizeof tlsst_protocol_map / sizeof tlsst_protocol_map[0]; i++)
		if (ldap_protocol == tlsst_protocol_map[i].ldap_protocol)
			return tlsst_protocol_map[i].st_protocol;

	syslog(LOG_ERR, "TLS: unknown %s setting %d", setting_name, ldap_protocol);
	return kSSLProtocolUnknown;
}

static struct {
	int ldap_authenticate;
	SSLAuthenticate st_authenticate;
} tlsst_auth_map[] = {
	LDAP_OPT_X_TLS_NEVER,				kNeverAuthenticate,
	LDAP_OPT_X_TLS_HARD,				kAlwaysAuthenticate,
	LDAP_OPT_X_TLS_DEMAND,				kAlwaysAuthenticate,
	LDAP_OPT_X_TLS_ALLOW,				kTryAuthenticate,
	LDAP_OPT_X_TLS_TRY,					kTryAuthenticate
};

static SSLAuthenticate
tlsst_auth_map_ldap2st(int ldap_authenticate)
{
	for (int i = 0; i < sizeof tlsst_auth_map / sizeof tlsst_auth_map[0]; i++)
		if (ldap_authenticate == tlsst_auth_map[i].ldap_authenticate)
			return tlsst_auth_map[i].st_authenticate;

	return kAlwaysAuthenticate;
}

static const char *
tlsst_protocol_name(SSLProtocol protocol)
{
	switch (protocol) {
	case kSSLProtocol2:
		return "SSLv2";
	case kSSLProtocol3:
	case kSSLProtocol3Only:		/* shouldn't happen but present only to make the no-default work (see below) */
		return "SSLv3";
	case kTLSProtocol1:
	case kTLSProtocol1Only:		/* shouldn't happen but present only to make the no-default work (see below) */
		return "TLSv1";
	case kTLSProtocol11:
		return "TLSv1.1";
	case kTLSProtocol12:
		return "TLSv1.2";
	case kDTLSProtocol1:		/* shouldn't happen but present only to make the no-default work (see below) */
		return "DTLSv1";
	case kSSLProtocolUnknown:
	case kSSLProtocolAll:		/* shouldn't happen but present only to make the no-default work (see below) */
		return "unknown";
	/* no default for this switch so the compiler will let us know when a new value is added to this enum so we can add a string for it */
	}
}

static void
tlsst_error_string(CFStringRef str, long num, char *buffer, size_t bufsize)
{
	if (CFStringGetCString(str, buffer, bufsize, kCFStringEncodingUTF8)) {
		/* add the num to the error string unless it's already in there */
		CFStringRef numeric = CFStringCreateWithFormat(NULL, NULL, CFSTR("%ld"), num);
		CFRange where = CFStringFind(str, numeric, 0);
		CFRelease_and_null(numeric);

		if (where.location == kCFNotFound) {
			char spnp[32];
			snprintf(spnp, sizeof spnp, " (%ld)", num);
			strlcat(buffer, spnp, bufsize);
		}
	} else
		snprintf(buffer, bufsize, "error %ld", num);
}

static char *
tlsst_oss2buf(OSStatus oss, char *buffer, size_t bufsize)
{
	if (oss >= errSecErrnoBase && oss <= errSecErrnoLimit)
		strlcpy(buffer, strerror(oss - errSecErrnoBase), bufsize);
	else {
		CFStringRef str = SecCopyErrorMessageString(oss, NULL);
		if (str != NULL) {
			tlsst_error_string(str, oss, buffer, bufsize);
			CFRelease_and_null(str);
		} else
			snprintf(buffer, bufsize, "error %ld", (long) oss);
	}

	return buffer;
}

static char *
tlsst_err2buf(CFErrorRef error, char *buffer, size_t bufsize)
{
	CFIndex code = error ? CFErrorGetCode(error) : 0;
	CFStringRef str = error ? CFErrorCopyDescription(error) : NULL;
	if (str != NULL) {
		tlsst_error_string(str, code, buffer, bufsize);
		CFRelease_and_null(str);
	} else
		snprintf(buffer, sizeof buffer, "error %ld", (long) code);

	return buffer;
}

static void
tlsst_report_oss(OSStatus oss, const char *format, ...)
{
	char msgbuf[512];
	va_list args;
	va_start(args, format);
	vsnprintf(msgbuf, sizeof msgbuf, format, args);
	va_end(args);

	char errbuf[512];
	tlsst_oss2buf(oss, errbuf, sizeof errbuf);

	syslog(LOG_ERR, "%s: %s", msgbuf, errbuf);
}

static void
tlsst_report_error(CFErrorRef error, const char *format, ...)
{
	char msgbuf[512];
	va_list args;
	va_start(args, format);
	vsnprintf(msgbuf, sizeof msgbuf, format, args);
	va_end(args);

	char errbuf[512];
	tlsst_err2buf(error, errbuf, sizeof errbuf);

	syslog(LOG_ERR, "%s: %s", msgbuf, errbuf);
}

static const char *tlsst_tr2str(SecTrustResultType trustResult)
{
	switch (trustResult) {
	case kSecTrustResultInvalid:
		return "kSecTrustResultInvalid";
	case kSecTrustResultProceed:
		return "kSecTrustResultProceed";
/* deprecated
	case kSecTrustResultConfirm:
		return "kSecTrustResultConfirm"; */
	case kSecTrustResultDeny:
		return "kSecTrustResultDeny";
	case kSecTrustResultUnspecified:
		return "kSecTrustResultUnspecified";
	case kSecTrustResultRecoverableTrustFailure:
		return "kSecTrustResultRecoverableTrustFailure";
	case kSecTrustResultFatalTrustFailure:
		return "kSecTrustResultFatalTrustFailure";
	case kSecTrustResultOtherError:
		return "kSecTrustResultOtherError";
	default:
		return "unknown SecTrustResult";
	}
}

static SSLCipherSuite *
tlsst_ciphers_get(const char *cipherspec, size_t *ciphers_count_r, const char *setting_name)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_ciphers_get(%s, %s)\n", cipherspec ?: "(null)", setting_name, 0);

	if (cipherspec == NULL || *cipherspec == 0)
		cipherspec = "DEFAULT";

	*ciphers_count_r = 0;
	SSLCipherSuite *ciphers = XSCipherSpecParse(cipherspec, ciphers_count_r);
	if (*ciphers_count_r == 0)
		syslog(LOG_ERR, "TLS: could not parse cipher spec %s (check %s setting)\n", cipherspec, setting_name);

	return ciphers;
}

static void
tlsst_ciphers_list(const SSLCipherSuite *ciphers, size_t count)
{
	// log lines are truncated at 1024 characters so log ciphers one per line rather than all on one line
	for (size_t i = 0; i < count; i++) {
		const char *name = XSCipherToName(ciphers[i]);
		if (name == NULL)
			name = "unknown";
		Debug(LDAP_DEBUG_ARGS, "TLS: [%lu] %04x %s\n", i, ciphers[i], name);
	}
}

static int
tlsst_ciphers_set(SSLContextRef ssl, const SSLCipherSuite *want_set, size_t num_want, const char *setting_name)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_ciphers_set(%lu, %s)\n", num_want, setting_name, 0);

	/* these are the ciphers we want */
	Debug(LDAP_DEBUG_ARGS, "TLS: %lu cipher%s wanted:\n", num_want, num_want == 1 ? "" : "s", 0);
	tlsst_ciphers_list(want_set, num_want);

	/* these are the available ciphers */
	size_t max_avail = 0;
	OSStatus ret = SSLGetNumberSupportedCiphers(ssl, &max_avail);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLGetNumberSupportedCiphers() failed");
		return num_want == 0 ? 0 : -1;
	}

	SSLCipherSuite *avail_set = LDAP_CALLOC(max_avail, sizeof *avail_set);
	size_t num_avail = max_avail;
	ret = SSLGetSupportedCiphers(ssl, avail_set, &num_avail);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLGetSupportedCiphers() failed");
		LDAP_FREE(avail_set);
		return num_want == 0 ? 0 : -1;
	}
	Debug(LDAP_DEBUG_ARGS, "TLS: %lu cipher%s supported:\n", num_avail, num_avail == 1 ? "" : "s", 0);
	tlsst_ciphers_list(avail_set, num_avail);
	if (num_want == 0) {
		LDAP_FREE(avail_set);
		return 0;
	}

	/* Compute intersection of the two sets preserving the order of
	   want_set.  Perhaps this O(n^2) operation should be computed
	   using binary searches through avail_set, or done once and the
	   result reused (but beware hidden constraints of
	   SecureTransport's default enabled ciphers list).  */
	size_t max_intersect = MIN(num_want, num_avail);
	SSLCipherSuite *intersect_set = LDAP_CALLOC(max_intersect, sizeof *intersect_set);
	size_t num_intersect = 0;
	for (size_t w = 0; w < num_want; w++) {
		for (size_t a = 0; a < num_avail; a++) {
			if (want_set[w] == avail_set[a]) {
				intersect_set[num_intersect++] = want_set[w];
				break;
			}
		}
	}
	LDAP_FREE(avail_set);
	avail_set = NULL;

	if (num_intersect == 0) {
		syslog(LOG_ERR, "TLS: None of the desired SSL ciphers are supported (check %s setting)", setting_name);
		LDAP_FREE(intersect_set);
		return -1;
	}
	
	ret = SSLSetEnabledCiphers(ssl, intersect_set, num_intersect);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLSetEnabledCiphers(%lu) failed (check %s setting)", num_intersect, setting_name);
		LDAP_FREE(intersect_set);
		return -1;
	}
	LDAP_FREE(intersect_set);

	if (ldap_debug == 0)
		return 0;

	size_t max_enabled = 0;
	ret = SSLGetNumberEnabledCiphers(ssl, &max_enabled);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLGetNumberEnabledCiphers() failed");
		return 0;	/* non-fatal */
	}
	SSLCipherSuite *enabled_set = LDAP_CALLOC(max_enabled, sizeof *enabled_set);
	size_t num_enabled = max_enabled;
	ret = SSLGetEnabledCiphers(ssl, enabled_set, &num_enabled);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLGetEnabledCiphers() failed");
		LDAP_FREE(enabled_set);
		return 0;	/* non-fatal */
	}

	Debug(LDAP_DEBUG_ARGS, "TLS: %lu cipher%s enabled\n", num_enabled, num_enabled == 1 ? "" : "s", 0);
	tlsst_ciphers_list(enabled_set, num_enabled);
	LDAP_FREE(enabled_set);

	return 0;
}

static CFTypeRef
tlsst_item_get(const char *name, const char *setting_name, CFTypeRef class)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_item_get(%s, %s)\n", name, setting_name, 0);

	CFTypeRef result = NULL;

	SecKeychainRef keychain = NULL;
	OSStatus ret = SecKeychainOpen(SYSTEM_KEYCHAIN_PATH, &keychain);
	if (ret == 0) {
		CFArrayRef searchList = CFArrayCreate(NULL, (const void **) &keychain, 1, &kCFTypeArrayCallBacks);
		if (searchList != NULL) {
			CFMutableDictionaryRef query = CFDictionaryCreateMutable(NULL, 8, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			if (query != NULL) {
				CFDictionaryAddValue(query, kSecMatchSearchList, searchList);
				CFDictionaryAddValue(query, kSecClass, class);
				CFDictionaryAddValue(query, kSecMatchLimit, kSecMatchLimitOne);
				CFDictionaryAddValue(query, kSecReturnRef, kCFBooleanTrue);
				CFStringRef label = CFStringCreateWithCString(NULL, name, kCFStringEncodingUTF8);
				if (label != NULL) {
					CFDictionaryAddValue(query, kSecMatchSubjectWholeString, label);

					ret = SecItemCopyMatching(query, &result);
					if (ret) {
						tlsst_report_oss(ret, "TLS: SecItemCopyMatching(%s) failed (check %s setting)", name, setting_name);
						if (result != NULL)
							CFRelease_and_null(result);
					} else if (result == NULL)
						syslog(LOG_ERR, "TLS: SecItemCopyMatching(%s) returned no matches (check %s setting)", name, setting_name);

					CFRelease_and_null(label);
				} else
					syslog(LOG_ERR, "TLS: CFStringCreateWithCString(%s) failed (check %s setting)", name, setting_name);

				CFRelease_and_null(query);
			} else
				syslog(LOG_ERR, "TLS: CFDictionaryCreateMutable() failed");

			CFRelease_and_null(searchList);
		} else
			syslog(LOG_ERR, "TLS: CFArrayCreate() failed");

		CFRelease_and_null(keychain);
	} else
		tlsst_report_oss(ret, "TLS: SecKeychainOpen(%s) failed", SYSTEM_KEYCHAIN_PATH);

	return result;		/* caller must release */
}

static Boolean
tlsst_identity_validate(SecIdentityRef identity, const char *setting_name)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_identity_validate(%s)\n", setting_name, 0, 0);

	/* use the private key to sign some test data */

	SecKeyRef key = NULL;
	OSStatus ret = SecIdentityCopyPrivateKey(identity, &key);
	if (ret || key == NULL) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecIdentityCopyPrivateKey() failed: %s (check %s setting)\n", tlsst_oss2buf(ret, errbuf, sizeof errbuf), setting_name, 0);
		if (key)
			CFRelease_and_null(key);
		return FALSE;
	}

	CFErrorRef error = NULL;
	SecTransformRef xform = SecSignTransformCreate(key, &error);
	CFRelease_and_null(key);
	if (error || xform == NULL) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecSignTransformCreate() failed: %s (check %s setting)\n", tlsst_err2buf(error, errbuf, sizeof errbuf), setting_name, 0);
		if (error)
			CFRelease_and_null(error);
		if (xform)
			CFRelease_and_null(xform);
		return FALSE;
	}

	CFDataRef data = CFDataCreate(NULL, (const UInt8 *) "John Hancock", 12);
	if (data == NULL) {
		Debug(LDAP_DEBUG_ANY, "TLS: CFDataCreate() failed\n", 0, 0, 0);
		CFRelease_and_null(xform);
		return FALSE;
	}

	Boolean ok = SecTransformSetAttribute(xform, kSecTransformInputAttributeName, data, &error);
	if (error || !ok) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecTransformSetAttribute(sign) failed: %s (check %s setting)\n", tlsst_err2buf(error, errbuf, sizeof errbuf), setting_name, 0);
		if (error)
			CFRelease_and_null(error);
		CFRelease_and_null(data);
		CFRelease_and_null(xform);
		return FALSE;
	}

	CFTypeRef result = SecTransformExecute(xform, &error);
	CFRelease_and_null(xform);
	if (error || result == NULL || CFGetTypeID(result) != CFDataGetTypeID()) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecTransformExecute(sign) failed (%lu, %lu): %s\n", result ? CFGetTypeID(result) : 0, CFDataGetTypeID(), tlsst_err2buf(error, errbuf, sizeof errbuf));
		if (error)
			CFRelease_and_null(error);
		if (result)
			CFRelease_and_null(result);
		CFRelease_and_null(data);
		return FALSE;
	}

	/* verify the signature using the public key */

	CFDataRef signature = (CFDataRef) result;
	result = NULL;

	SecCertificateRef cert = NULL;
	ret = SecIdentityCopyCertificate(identity, &cert);
	if (ret || cert == NULL) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecIdentityCopyCertificate() failed: %s (check %s setting)\n", tlsst_oss2buf(ret, errbuf, sizeof errbuf), setting_name, 0);
		if (cert)
			CFRelease_and_null(cert);
		CFRelease_and_null(signature);
		CFRelease_and_null(data);
		return FALSE;
	}

	ret = SecCertificateCopyPublicKey(cert, &key);
	CFRelease_and_null(cert);
	if (ret || key == NULL) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecCertificateCopyPublicKey() failed: %s (check %s setting)\n", tlsst_oss2buf(ret, errbuf, sizeof errbuf), setting_name, 0);
		if (key)
			CFRelease_and_null(key);
		CFRelease_and_null(signature);
		CFRelease_and_null(data);
		return FALSE;
	}

	xform = SecVerifyTransformCreate(key, signature, &error);
	CFRelease_and_null(key);
	CFRelease_and_null(signature);
	if (error || xform == NULL) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecVerifyTransformCreate() failed: %s (check %s setting)\n", tlsst_err2buf(error, errbuf, sizeof errbuf), setting_name, 0);
		if (error)
			CFRelease_and_null(error);
		if (xform)
			CFRelease_and_null(xform);
		CFRelease_and_null(data);
		return FALSE;
	}

	ok = SecTransformSetAttribute(xform, kSecTransformInputAttributeName, data, &error);
	CFRelease_and_null(data);
	if (error || !ok) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecTransformSetAttribute(verify) failed: %s (check %s setting)\n", tlsst_err2buf(error, errbuf, sizeof errbuf), setting_name, 0);
		if (error)
			CFRelease_and_null(error);
		CFRelease_and_null(xform);
		return FALSE;
	}

	result = SecTransformExecute(xform, &error);
	CFRelease_and_null(xform);
	if (error || result == NULL || CFGetTypeID(result) != CFBooleanGetTypeID()) {
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: SecTransformExecute(verify) failed (%lu, %lu): %s\n", result ? CFGetTypeID(result) : 0, CFBooleanGetTypeID(), tlsst_err2buf(error, errbuf, sizeof errbuf));
		if (error)
			CFRelease_and_null(error);
		if (result)
			CFRelease_and_null(result);
		return FALSE;
	}

	ok = CFBooleanGetValue((CFBooleanRef) result);
	CFRelease_and_null(result);
	return ok;
}

static CFArrayRef
tlsst_identity_certs_get(const char *identity, const char *setting_name)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_identity_certs_get(%s, %s)\n", identity ?: "(null)", setting_name, 0);

	if (identity == NULL || *identity == '\0')
		return NULL;

	CFArrayRef certs = NULL;

	SecIdentityRef identRef = SecIdentityCopyPreferred(CFSTR("OPENDIRECTORY_SSL_IDENTITY"), NULL, NULL);
	if (identRef != NULL)
		syslog(LOG_INFO, "TLS: OPENDIRECTORY_SSL_IDENTITY identity preference overrode configured %s \"%s\"", setting_name, identity);
	else {
		/* The identity name may be preceded by "APPLE:".  If so, strip that part. */
		const char *sep = strchr(identity, ':');
		if (sep)
			identity = sep + 1;
		CFTypeRef ref = tlsst_item_get(identity, setting_name, kSecClassIdentity);
		if (ref != NULL) {
			if (CFGetTypeID(ref) == SecIdentityGetTypeID()) {
				identRef = (SecIdentityRef) ref;
				CFRetain(identRef);
			} else
				syslog(LOG_ERR, "TLS: Keychain item for \"%s\" is not an identity (check %s setting)", identity, setting_name);

			CFRelease_and_null(ref);
		}
	}

	if (identRef != NULL) {
		/* the private key must be usable */
		if (tlsst_identity_validate(identRef, setting_name)) {
			certs = CFArrayCreate(NULL, (const void **) &identRef, 1, &kCFTypeArrayCallBacks);
			if (certs == NULL)
				syslog(LOG_ERR, "TLS: CFArrayCreate() failed");
		} else
			syslog(LOG_ERR, "TLS: Can't get or use private key for %s \"%s\"; is it application-restricted?", setting_name, identity);

		CFRelease_and_null(identRef);
	}

	return certs;		/* caller must release */
}

static CFArrayRef
tlsst_trusted_certs_get(const char *trusted_certs, const char *setting_name)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_trusted_certs_get(%s, %s)\n", trusted_certs ?: "(null)", setting_name, 0);

	if (trusted_certs == NULL || *trusted_certs == 0)
		return NULL;

	CFMutableArrayRef certs = NULL;
	Boolean ok = TRUE;

	char *trusted_names = strdup(trusted_certs);
	const char *separators = "|";
	char *mark = NULL;
	for (char *tok = strtok_r(trusted_names, separators, &mark); tok != NULL; tok = strtok_r(NULL, separators, &mark)) {
		CFTypeRef ref = tlsst_item_get(tok, setting_name, kSecClassCertificate);
		if (ref != NULL) {
			if (CFGetTypeID(ref) == SecCertificateGetTypeID()) {
				if (certs == NULL)
					certs = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				if (certs != NULL)
					CFArrayAppendValue(certs, ref);
				else {
					syslog(LOG_ERR, "TLS: CFArrayCreateMutable() failed");
					ok = FALSE;
				}
			} else {
				syslog(LOG_ERR, "TLS: Keychain item for \"%s\" is not a certificate (check %s setting)", tok, setting_name);
				ok = FALSE;
			}

			CFRelease_and_null(ref);
		} else
			ok = FALSE;

		if (!ok)
			break;
	}
	free(trusted_names);

	if (!ok && certs)
		CFRelease_and_null(certs);

	return certs;		/* caller must release */
}

static void
tlsst_clear_error(tlsst_session *sess)
{
	if (sess->last_error != NULL) {
		free(sess->last_error);
		sess->last_error = NULL;
	}
}

static void
tlsst_save_error(tlsst_session *sess, OSStatus oss, const char *func_name, const char *comment)
{
	tlsst_clear_error(sess);

	char errbuf[512];
	tlsst_oss2buf(oss, errbuf, sizeof errbuf);

	asprintf(&sess->last_error, "%s failed: %s%s", func_name, errbuf, comment ?: "");
}

static int
tlsst_socket_flags(tlsst_session *sess)
{	
	Debug(LDAP_DEBUG_TRACE, "tlsst_socket_flags sess(%p)\n", sess, 0, 0);
	
	int flags = fcntl( sess->sbiod->sbiod_sb->sb_fd, F_GETFL);
	
	if (flags != -1) {
		Debug(LDAP_DEBUG_TRACE, "tlsst_socket_flags ->(%ld)\n", flags, 0, 0);	
	} else {
		Debug(LDAP_DEBUG_TRACE, "tlsst_socket_flags error(%ld)\n", sock_errno(), 0, 0);		
	}
	
	return flags;
}

/* read encrypted data */
static OSStatus
tlsst_socket_read(SSLConnectionRef conn, void *data, size_t *size)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_socket_read(%p, %lu)\n", data, *size, 0);

	tlsst_session *sess = (tlsst_session *) conn;
	OSStatus result = 0;
	size_t requested_size =  *size;
	
	ber_slen_t r = LBER_SBIOD_READ_NEXT(sess->sbiod, data, *size);
	if (r > 0) {
		if (r < requested_size)  {  /* retrieve remaining encrypted data */
			result = errSSLWouldBlock;
			Debug(LDAP_DEBUG_TRACE, "tlsst_socket_read -  received (%ld) bytes of (%ld) requested bytes -  (%ld) encrypted bytes remaining\n",  r, requested_size, requested_size - r);
		}

		*size = r;

		for (int i = 0; sess->sslv2_detect_length < sizeof sess->sslv2_detect_bytes && i < r; i++)
			sess->sslv2_detect_bytes[sess->sslv2_detect_length++] = *((unsigned char *) data + i);
	} else if (r == 0) {
		*size = 0;
		result = errSSLClosedGraceful;
	} else {
		*size = 0;
		int err = sock_errno();
		if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
			result = errSSLWouldBlock;
		else
			result = errSSLClosedAbort;
	}

	if (result == errSSLWouldBlock)
		sess->want_read = TRUE;

	return result;
}

/* write encrypted data */
static OSStatus
tlsst_socket_write(SSLConnectionRef conn, const void *data, size_t *size)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_socket_write(%p, %lu)\n", data, *size, 0);

	tlsst_session *sess = (tlsst_session *) conn;
	OSStatus result = 0;
	size_t requested_size =  *size;

	ber_slen_t w = LBER_SBIOD_WRITE_NEXT(sess->sbiod, (void *) data, *size);
	if (w > 0) {
		if (w < requested_size)  {  /* write remaining encrypted data */
			result = errSSLWouldBlock;
			Debug(LDAP_DEBUG_TRACE, "tlsst_socket_write -  written (%ld) bytes of (%ld) requested bytes -  (%ld) encrypted bytes remaining\n",  w, requested_size, requested_size - w);
		}

		*size = w;
	} else if (w == 0) {
		*size = 0;
		result = errSSLWouldBlock;
	} else {
		*size = 0;
		int err = sock_errno();
		if (err == EINTR || err == EAGAIN || err == EWOULDBLOCK)
			result = errSSLWouldBlock;
		else
			result = errSSLClosedAbort;
	}

	if (result == errSSLWouldBlock)
		sess->want_write = TRUE;

	return result;
}

static int
tlsst_session_handshake(tlsst_session *sess)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_handshake()\n", 0, 0, 0);

	OSStatus ret;
	const char *comment = NULL;

	for (;;) {
		sess->want_read = FALSE;
		sess->want_write = FALSE;
		ret = SSLHandshake(sess->ssl);
		if (ret == 0)
			break;
		if (ret == errSSLWouldBlock) {
			int flags = tlsst_socket_flags(sess);
			
			if (flags != -1) {
				if (flags & O_NONBLOCK) /* non blocking i/o - break and allow caller to return when data is available  */
					break;
			}
			/* blocking i/o <or> error retrieving socket options */
			/* retry SSLHandshake */
			continue;
		}
		char errbuf[512];
		Debug(LDAP_DEBUG_ANY, "TLS: during handshake: %s\n", tlsst_oss2buf(ret, errbuf, sizeof errbuf), 0, 0);
		if (ret == errSSLPeerAuthCompleted) {
			sess->cert_received = TRUE;

			SecTrustRef trust = NULL;
			ret = SSLCopyPeerTrust(sess->ssl, &trust);
			if (ret == 0 && trust) {
				if (sess->ctx->trusted_certs) {
					ret = SecTrustSetAnchorCertificates(trust, sess->ctx->trusted_certs);
					if (ret)
						tlsst_report_oss(ret, "TLS: during handshake: SecTrustSetAnchorCertificates(%lu) failed (check %s setting)", CFArrayGetCount(sess->ctx->trusted_certs),
										 sess->is_server ? "olcTLSTrustedCerts" : "TLS_TRUSTED_CERTS");
				}
				if (ret == 0 && sess->ctx->crl_check != LDAP_OPT_X_TLS_CRL_NONE) {
					ret = SecTrustSetOptions(trust, kSecTrustOptionRequireRevPerCert | kSecTrustOptionFetchIssuerFromNet);
					if (ret)
						tlsst_report_oss(ret, "TLS: during handshake: SecTrustSetOptions() failed (check %s setting)", sess->is_server ? "TLSCRLCheck" : "TLS_CRLCHECK");
				}
				if (ret == 0) {
					SecTrustResultType trustResult = kSecTrustResultInvalid;
					ret = SecTrustEvaluate(trust, &trustResult);
					if (ret == 0) {
						/*									LDAP_OPT_X_TLS_...
						 *	kSecTrustResult...		NEVER	ALLOW	TRY		DEMAND	HARD
						 *	Invalid					ok		ok		fail	fail	fail
						 *	Proceed					ok		ok		ok		ok		ok
						 *	Confirm (deprecated)	ok		ok		ok		fail	fail
						 *	Deny					ok		ok		ok		fail	fail
						 *	Unspecified				ok		ok		ok		ok		ok
						 *	RecoverableTrustFailure	ok		ok		ok		fail	fail
						 *	FatalTrustFailure		ok		ok		fail	fail	fail
						 *	OtherError				ok		ok		fail	fail	fail
						 */
						if (trustResult == kSecTrustResultProceed ||
							trustResult == kSecTrustResultUnspecified) {
							Debug(LDAP_DEBUG_ANY, "TLS: during handshake: Peer certificate is trusted\n", 0, 0, 0);
							sess->cert_trusted = TRUE;
							/* call SSLHandshake() again */
						} else if (sess->ctx->require_cert == LDAP_OPT_X_TLS_NEVER ||
								   sess->ctx->require_cert == LDAP_OPT_X_TLS_ALLOW ||
								   (sess->ctx->require_cert == LDAP_OPT_X_TLS_TRY &&
									(/* trustResult == kSecTrustResultConfirm || */
									 trustResult == kSecTrustResultDeny ||
									 trustResult == kSecTrustResultRecoverableTrustFailure))) {
							Debug(LDAP_DEBUG_ANY, "TLS: during handshake: Allowing untrusted peer certificate\n", 0, 0, 0);
							/* call SSLHandshake() again */
						} else {
							Debug(LDAP_DEBUG_ANY, "TLS: during handshake: Peer certificate is not trusted: %s\n", tlsst_tr2str(trustResult), 0, 0);
							ret = errSSLPeerBadCert;
						}
					} else {
						char errbuf[512];
						Debug(LDAP_DEBUG_ANY, "TLS: during handshake: SecTrustEvaluate() failed: %s\n", tlsst_oss2buf(ret, errbuf, sizeof errbuf), 0, 0);
					}
				}

				CFRelease(trust);
			} else {
				char errbuf[512];
				Debug(LDAP_DEBUG_ANY, "TLS: during handshake: SSLCopyPeerTrust() failed: %s\n", tlsst_oss2buf(ret, errbuf, sizeof errbuf), 0, 0);
			}
			if (ret)
				break;
		} else {
			if (ret == errSSLProtocol && sess->is_server && sess->sslv2_detect_length >= 3 && (sess->sslv2_detect_bytes[0] & 0x80) == 0x80 && sess->sslv2_detect_bytes[2] == 0x01)
				comment = "; possible attempt to connect via obsolete SSLv2 protocol";
			break;
		}
	}
	if (ret) {
		tlsst_save_error(sess, ret, "SSLHandshake()", comment);
		return -1;
	}

	if (DebugTest(LDAP_DEBUG_ANY)) {
		SSLProtocol protocol = kSSLProtocolUnknown;
		if (SSLGetNegotiatedProtocolVersion(sess->ssl, &protocol) != 0)
			protocol = kSSLProtocolUnknown;
		const char *pname = tlsst_protocol_name(protocol);

		SSLCipherSuite cipher = SSL_NO_SUCH_CIPHERSUITE;
		if (SSLGetNegotiatedCipher(sess->ssl, &cipher) != 0)
			cipher = SSL_NO_SUCH_CIPHERSUITE;
		const char *cname = NULL;
		if (cipher != SSL_NO_SUCH_CIPHERSUITE)
			cname = XSCipherToName(cipher);
		if (cname == NULL)
			cname = "unknown";

		Debug(LDAP_DEBUG_ANY, "TLS: %s session established using %d-bit %s cipher\n", pname, tlsst_session_strength((tls_session *) sess), cname);
	}

	tlsst_clear_error(sess);
	return 0;
}

static SecCertificateRef
tlsst_copy_peer_cert(tlsst_session *sess)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_copy_peer_cert()\n", 0, 0, 0);

	SecCertificateRef result = NULL;

	if (sess->cert_received && sess->cert_trusted) {
		SecTrustRef trust = NULL;
		(void) SSLCopyPeerTrust(sess->ssl, &trust);
		if (trust != NULL) {
			if (SecTrustGetCertificateCount(trust) > 0) {
				result = SecTrustGetCertificateAtIndex(trust, 0);
				CFRetain(result);
			}

			CFRelease_and_null(trust);
		}
	}

	return result;	// caller must release
}

static CFDataRef
tlsst_builtin_dhparams(void)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_builtin_dhparams()\n", 0, 0, 0);

	// output of command "openssl dhparam 1024"
	static const char dhparams_pem[] =
		// -----BEGIN DH PARAMETERS-----
		"MIGHAoGBAOi2AYTgWEzB4TI07BKz4Z6H3oFKvCz77YAaPizFwUW5Jy7JDV6vXO8n"
		"RrjSCuZ8V4TyfewkDW/iju5Rkgsy44UO9fGDLWjNG8fom92fuXBdNcbO8zAvG97B"
		"mojNok6fpxvsFoUWWLmrlVPr/gtWANZAmSDr78ovtstQcdf5+6a7AgEC";
		// -----END DH PARAMETERS-----

	CFDataRef result = NULL;

	CFDataRef pemData = CFDataCreateWithBytesNoCopy(NULL, (const UInt8 *) dhparams_pem, strlen(dhparams_pem), kCFAllocatorNull);
	if (pemData != NULL) {
		SecTransformRef decoder = SecDecodeTransformCreate(kSecBase64Encoding, NULL);
		if (decoder != NULL) {
			SecTransformSetAttribute(decoder, kSecTransformInputAttributeName, pemData, NULL);
			result = SecTransformExecute(decoder, NULL);

			CFRelease(decoder);
		}

		CFRelease_and_null(pemData);
	}

	return result;		// caller must release
}

static int
tlsst_init(void)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_init()\n", 0, 0, 0);

	/* initialize PRNG */
	/* except we can't: <rdar://problem/10587132> */
	struct stat st;
	if (stat("/dev/random", &st) != 0) {
		syslog(LOG_ERR, "TLS: /dev/random not available.  SSL will not work.  Don't chroot.");
		return -1;
	}

	/* load error strings */
	char buf[256];
	(void) tlsst_oss2buf(errSSLWouldBlock, buf, sizeof buf);

	SecKeychainSetUserInteractionAllowed(false);
	SecKeychainSetPreferenceDomain(kSecPreferencesDomainSystem);

	return 0;
}

static void
tlsst_destroy(void)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_destroy()\n", 0, 0, 0);
}

static tls_ctx *
tlsst_ctx_new(struct ldapoptions *lo)
{
	tlsst_ctx *ctx = LDAP_CALLOC(1, sizeof *ctx);
	Debug(LDAP_DEBUG_TRACE, "tlsst_ctx_new() = %p\n", ctx, 0, 0);
	if (ctx) {
		ctx->refcount = 1;
#ifdef LDAP_R_COMPILE
		ldap_pvt_thread_mutex_init(&ctx->refmutex);
#endif
	}
	return (tls_ctx *) ctx;
}

static void
tlsst_ctx_ref(tls_ctx *_ctx)
{
	tlsst_ctx *ctx = (tlsst_ctx *) _ctx;
	Debug(LDAP_DEBUG_TRACE, "tlsst_ctx_ref(%p)\n", ctx, 0, 0);
	LDAP_MUTEX_LOCK(&ctx->refmutex);
	++ctx->refcount;
	LDAP_MUTEX_UNLOCK(&ctx->refmutex);
}

static void
tlsst_ctx_free(tls_ctx *_ctx)
{
	tlsst_ctx *ctx = (tlsst_ctx *) _ctx;
	Debug(LDAP_DEBUG_TRACE, "tlsst_ctx_free(%p)\n", ctx, 0, 0);
	if (ctx) {
		LDAP_MUTEX_LOCK(&ctx->refmutex);
		int refcount = --ctx->refcount;
		LDAP_MUTEX_UNLOCK(&ctx->refmutex);
		if (refcount == 0) {
			if (ctx->ciphers != NULL) {
				free(ctx->ciphers);
				ctx->ciphers = NULL;
			}
			if (ctx->identity_certs != NULL)
				CFRelease_and_null(ctx->identity_certs);
			if (ctx->trusted_certs != NULL)
				CFRelease_and_null(ctx->trusted_certs);
			if (ctx->dhparams != NULL)
				CFRelease_and_null(ctx->dhparams);

			LDAP_FREE(ctx);
			ctx = NULL;
		}
	}
}

static int
tlsst_ctx_init(struct ldapoptions *lo, struct ldaptls *lt, int is_server)
{
	tlsst_ctx *ctx = lo->ldo_tls_ctx;
	Debug(LDAP_DEBUG_TRACE, "tlsst_ctx_init(%p)\n", ctx, 0, 0);
	int result = -1;

	if (lo->ldo_tls_protocol_min) {
		ctx->protocol_min = tlsst_protocol_map_ldap2st(lo->ldo_tls_protocol_min, is_server ? "TLSProtocolMin" : "TLS_PROTOCOL_MIN");
		if (ctx->protocol_min == kSSLProtocolUnknown) {
			return -1;
		} else if (ctx->protocol_min == kSSLProtocol2) {
			syslog(LOG_ERR, "TLS: SSLv2 is no longer supported (check %s setting)", is_server ? "TLSProtocolMin" : "TLS_PROTOCOL_MIN");
			return -1;
		} else if (ctx->protocol_min == kSSLProtocol3) {
			syslog(LOG_ERR, "TLS: SSLv3 is no longer supported (check %s setting)", is_server ? "TLSProtocolMin" : "TLS_PROTOCOL_MIN");
			return -1;
		}
	} else {
		ctx->protocol_min = kTLSProtocol1;
	}
	ctx->require_cert = lo->ldo_tls_require_cert;
	ctx->crl_check = lo->ldo_tls_crlcheck;
	ctx->ciphers = tlsst_ciphers_get(lo->ldo_tls_ciphersuite, &ctx->ciphers_count, is_server ? "TLSCipherSuite" : "TLS_CIPHER_SUITE");
	if (ctx->ciphers_count == 0)
		return -1;
	if (lo->ldo_tls_identity) {
		ctx->identity_certs = tlsst_identity_certs_get(lo->ldo_tls_identity, is_server ? "olcTLSIdentity" : "TLS_IDENTITY");
		if (ctx->identity_certs == NULL)
			return -1;
	}
	if (lo->ldo_tls_trusted_certs) {
		ctx->trusted_certs = tlsst_trusted_certs_get(lo->ldo_tls_trusted_certs, is_server ? "olcTLSTrustedCerts" : "TLS_TRUSTED_CERTS");
		if (ctx->trusted_certs == NULL)
			return -1;
	}
	if (lo->ldo_tls_dhfile) {
		CFStringRef path = CFStringCreateWithCString(NULL, lo->ldo_tls_dhfile, kCFStringEncodingUTF8);
		if (path != NULL) {
			CFArrayRef dhparams = XSDHParamCreateFromFile(path);
			if (dhparams != NULL) {
				// use only the strongest one <rdar://problem/10595552>
				CFDataRef maxdh = NULL;
				CFIndex maxdhsize = 0;
				for (CFIndex i = 0; i < CFArrayGetCount(dhparams); ++i) {
					CFDataRef dh = CFArrayGetValueAtIndex(dhparams, i);
					CFIndex size = XSDHParamGetSize(dh);
					if (maxdhsize < size) {
						maxdh = dh;
						maxdhsize = size;
					}
				}
				
				if (maxdh != NULL) {
					CFRetain(maxdh);
					ctx->dhparams = maxdh;

					result = 0;
				} else
					syslog(LOG_ERR, "TLS: DH file %s contains no DH params (check TLSDHParamFile setting)", lo->ldo_tls_dhfile);

				CFRelease_and_null(dhparams);
			} else
				syslog(LOG_ERR, "TLS: Unable to parse DH file %s (check TLSDHParamFile setting)", lo->ldo_tls_dhfile);

			CFRelease_and_null(path);
		} else
			syslog(LOG_ERR, "TLS: CFStringCreateWithCString(%s) failed (check TLSDHParamFile setting)", lo->ldo_tls_dhfile);
	} else {
		ctx->dhparams = tlsst_builtin_dhparams();
		result = 0;
	}

	return result;
}

static tls_session *
tlsst_session_new(tls_ctx *_ctx, int is_server)
{
	tlsst_ctx *ctx = (tlsst_ctx *) _ctx;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_new(%p)\n", ctx, 0, 0);

	struct stat st;
	if (stat("/dev/random", &st) != 0) {
		syslog(LOG_ERR, "TLS: /dev/random not available.  SSL will not work.  Don't chroot.");
		return NULL;
	}

	SSLContextRef ssl = SSLCreateContext(NULL, is_server ? kSSLServerSide : kSSLClientSide, kSSLStreamType);
	if (ssl == NULL) {
		syslog(LOG_ERR, "TLS: SSLCreateContext() failed");
		return NULL;
	}

	OSStatus ret;
	if (is_server) {
		ret = SSLSetClientSideAuthenticate(ssl, tlsst_auth_map_ldap2st(ctx->require_cert));
		if (ret) {
			tlsst_report_oss(ret, "TLS: SSLSetClientSideAuthentication(%d) failed", (int) tlsst_auth_map_ldap2st(ctx->require_cert));
			CFRelease_and_null(ssl);
			return NULL;
		}

		if (ctx->dhparams != NULL) {
			ret = SSLSetDiffieHellmanParams(ssl, CFDataGetBytePtr(ctx->dhparams), CFDataGetLength(ctx->dhparams));
			if (ret) {
				tlsst_report_oss(ret, "TLS: SSLSetDiffieHellmanParams() failed");
				CFRelease_and_null(ssl);
				return NULL;
			}
		}
	}

	if (ctx->ciphers != NULL) {
		if (tlsst_ciphers_set(ssl, ctx->ciphers, ctx->ciphers_count, is_server ? "TLSCipherSuite" : "TLS_CIPHER_SUITE") < 0) {
			CFRelease_and_null(ssl);
			return NULL;
		}
	}

	if (ctx->identity_certs != NULL) {
		ret = SSLSetCertificate(ssl, ctx->identity_certs);
		if (ret) {
			tlsst_report_oss(ret, "TLS: SSLSetCertificate() failed (check %s setting)", is_server ? "olcTLSIdentity" : "TLS_IDENTITY");
			CFRelease_and_null(ssl);
			return NULL;
		}
	}

	if (ctx->protocol_min != kSSLProtocolUnknown) {
		ret = SSLSetProtocolVersionMin(ssl, ctx->protocol_min);
		if (ret) {
			tlsst_report_oss(ret, "TLS: SSLSetProtocolVersionMin(%d) failed (check %s setting)", (int) ctx->protocol_min, is_server ? "TLSProtocolMin" : "TLS_PROTOCOL_MIN");
			CFRelease_and_null(ssl);
			return NULL;
		}
	}

	ret = SSLSetIOFuncs(ssl, tlsst_socket_read, tlsst_socket_write);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLSetIOFuncs() failed");
		CFRelease_and_null(ssl);
		return NULL;
	}

	ret = SSLSetSessionOption(ssl, is_server ? kSSLSessionOptionBreakOnClientAuth : kSSLSessionOptionBreakOnServerAuth, TRUE);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLSetSessionOption(BreakOnAuth) failed");
		CFRelease_and_null(ssl);
		return NULL;
	}

	tlsst_session *sess = LDAP_CALLOC(1, sizeof *sess);

	ret = SSLSetConnection(ssl, (SSLConnectionRef) sess);
	if (ret) {
		tlsst_report_oss(ret, "TLS: SSLSetSessionOption(BreakOnAuth) failed");
		CFRelease_and_null(ssl);
		LDAP_FREE(sess);
		return NULL;
	}

	sess->ctx = ctx;
	tlsst_ctx_ref((tls_ctx *) ctx);
	sess->ssl = ssl;
	sess->is_server = is_server;

	Debug(LDAP_DEBUG_TRACE, "tlsst_session_new(%p) = %p\n", ctx, sess, 0);
	return (tls_session *) sess;
}

static void
tlsst_session_free(tlsst_session *sess)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_free(%p)\n", sess, 0, 0);
	CFRelease_and_null(sess->ssl);
	tlsst_ctx_free((tls_ctx *) sess->ctx);
	sess->ctx = NULL;
	tlsst_clear_error(sess);
	if (sess->subject_data != NULL)
		CFRelease_and_null(sess->subject_data);
	if (sess->issuer_data != NULL)
		CFRelease_and_null(sess->issuer_data);
	LDAP_FREE(sess);
}

static int
tlsst_session_connect(LDAP *ld, tls_session *_sess)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_connect(%p)\n", sess, 0, 0);

	return tlsst_session_handshake(sess);
}

static int
tlsst_session_accept(tls_session *_sess)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_accept(%p)\n", sess, 0, 0);

	return tlsst_session_handshake(sess);
}

static int
tlsst_session_upflags(Sockbuf *sb, tls_session *_sess, int rc)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_upflags(%p)\n", sess, 0, 0);

	sb->sb_trans_needs_read = sess->want_read;
	sb->sb_trans_needs_write = sess->want_write;

	return sess->want_read || sess->want_write;
}

static char *
tlsst_session_errmsg(tls_session *_sess, int rc, char *buf, size_t len)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_errmsg(%p)\n", sess, 0, 0);

	if (sess->last_error) {
		strlcpy(buf, sess->last_error, len);
		return buf;
	}

	return NULL;
}
	
static int
tlsst_session_my_dn(tls_session *_sess, struct berval *der_dn)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_my_dn(%p)\n", sess, 0, 0);

	// make sure the pointer we return in der_dn->bv_val remains valid for as long as the session does
	if (!sess->subject_cached) {
		sess->subject_cached = TRUE;
		sess->subject_result = LDAP_INVALID_CREDENTIALS;

		// SecureTransport has no way to retrieve the server certificate <rdar://problem/10930619> so fudge it by using identity_certs
		if (sess->ctx->identity_certs != NULL && CFArrayGetCount(sess->ctx->identity_certs) > 0) {
			SecIdentityRef identity = (SecIdentityRef) CFArrayGetValueAtIndex(sess->ctx->identity_certs, 0);
			SecCertificateRef cert = NULL;
			(void) SecIdentityCopyCertificate(identity, &cert);
			if (cert != NULL) {
				CFDataRef subject = SecCertificateCopyNormalizedSubjectContent(cert, NULL);
				if (subject != NULL) {
					// mutable because der_dn->bv_val is not const
					sess->subject_data = CFDataCreateMutableCopy(NULL, 0, subject);
					sess->subject_result = LDAP_SUCCESS;

					CFRelease_and_null(subject);
				}

				CFRelease_and_null(cert);
			}
		}
	}

	if (sess->subject_data != NULL) {
		der_dn->bv_len = CFDataGetLength(sess->subject_data);
		der_dn->bv_val = (char *) CFDataGetMutableBytePtr(sess->subject_data);
	}
	return sess->subject_result;
}

static int
tlsst_session_peer_dn(tls_session *_sess, struct berval *der_dn)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_peer_dn(%p)\n", sess, 0, 0);

	// make sure the pointer we return in der_dn->bv_val remains valid for as long as the session does
	if (!sess->issuer_cached) {
		sess->issuer_cached = TRUE;
		sess->issuer_result = LDAP_INVALID_CREDENTIALS;

		SecCertificateRef cert = tlsst_copy_peer_cert(sess);
		if (cert != NULL) {
			CFDataRef issuer = SecCertificateCopyNormalizedIssuerContent(cert, NULL);
			if (issuer != NULL) {
				// mutable because der_dn->bv_val is not const
				sess->issuer_data = CFDataCreateMutableCopy(NULL, 0, issuer);
				sess->issuer_result = LDAP_SUCCESS;

				CFRelease_and_null(issuer);
			}

			CFRelease_and_null(cert);
		}
	}

	if (sess->issuer_data != NULL) {
		der_dn->bv_len = CFDataGetLength(sess->issuer_data);
		der_dn->bv_val = (char *) CFDataGetMutableBytePtr(sess->issuer_data);
	}
	return sess->issuer_result;
}

/* what kind of hostname were we given? */
#define	IS_DNS	0
#define	IS_IP4	1
#define	IS_IP6	2

// copied from the other tls*_session_chkhost() implementations so don't blame me for the yuck
static int
tlsst_session_chkhost(LDAP *ld, tls_session *_sess, const char *name_in)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	int ret = LDAP_LOCAL_ERROR;
	const char *name;
	char *ptr;
	int ntype = IS_DNS, nlen;
#ifdef LDAP_PF_INET6
	struct in6_addr addr;
#else
	struct in_addr addr;
#endif

	Debug(LDAP_DEBUG_TRACE, "tlsst_session_chkhost(%p)\n", sess, 0, 0);

	if( ldap_int_hostname &&
		( !name_in || !strcasecmp( name_in, "localhost" ) ) )
	{
		name = ldap_int_hostname;
	} else {
		name = name_in;
	}
	nlen = strlen(name);

	SecCertificateRef cert = tlsst_copy_peer_cert(sess);
	if (cert == NULL) {
		Debug( LDAP_DEBUG_ANY,
			"TLS: unable to get peer certificate.\n",
			0, 0, 0 );
		/* If this was a fatal condition, things would have
		 * aborted long before now.
		 */
		return LDAP_SUCCESS;
	}

	memset(&addr, 0, sizeof addr);
#ifdef LDAP_PF_INET6
	if (name[0] == '[' && strchr(name, ']')) {
		char *n2 = ldap_strdup(name+1);
		*strchr(n2, ']') = 0;
		if (inet_pton(AF_INET6, n2, &addr) > 0)
			ntype = IS_IP6;
		LDAP_FREE(n2);
	} else 
#endif
	if ((ptr = strrchr(name, '.')) && isdigit((unsigned char)ptr[1])) {
		if (inet_aton(name, (struct in_addr *)&addr)) ntype = IS_IP4;
	}
	
	int dlen = 0;
	char *domain = NULL;
	if (ntype == IS_DNS) {
		domain = strchr(name, '.');
		if (domain)
			dlen = nlen - (domain-name);
	}

	CFDictionaryRef certContents = SecCertificateCopyValues(cert, NULL, NULL);
	if (certContents != NULL) {
		CFDictionaryRef altNamesValues = (CFDictionaryRef) CFDictionaryGetValue(certContents, kSecOIDSubjectAltName);
		if (altNamesValues != NULL && CFGetTypeID(altNamesValues) == CFDictionaryGetTypeID()) {
			CFArrayRef altNames = (CFArrayRef) CFDictionaryGetValue(altNamesValues, kSecPropertyKeyValue);
			if (altNames != NULL && CFGetTypeID(altNames) == CFArrayGetTypeID()) {
				CFIndex i;
				for (i = 0; i < CFArrayGetCount(altNames); ++i) {
					CFDictionaryRef altNameValues = (CFDictionaryRef) CFArrayGetValueAtIndex(altNames, i);
					if (CFGetTypeID(altNameValues) == CFDictionaryGetTypeID()) {
						CFStringRef altNameLabel = CFDictionaryGetValue(altNameValues, kSecPropertyKeyLabel);
						CFStringRef altNameValue = CFDictionaryGetValue(altNameValues, kSecPropertyKeyValue);
						if (altNameLabel != NULL && CFGetTypeID(altNameLabel) == CFStringGetTypeID() &&
							altNameValue != NULL && CFGetTypeID(altNameValue) == CFStringGetTypeID()) {
							CFIndex altNameLen = CFStringGetLength(altNameValue);
							CFIndex altNameSize = CFStringGetMaximumSizeForEncoding(altNameLen, kCFStringEncodingUTF8) + 1;
							char *altNameBuf = alloca(altNameSize);
							if (!CFStringGetCString(altNameValue, altNameBuf, altNameSize, kCFStringEncodingUTF8))
								continue;

							if (CFEqual(altNameLabel, CFSTR("DNS Name"))) {
								if (ntype != IS_DNS)
									continue;

								/* ignore empty */
								if (altNameLen == 0)
									continue;

								/* Is this an exact match? */
								if (nlen == altNameLen && !strncasecmp(name, altNameBuf, nlen))
									break;

								/* Is this a wildcard match? */
								if (domain && dlen == altNameLen - 1 && altNameBuf[0] == '*' && altNameBuf[1] == '.' && !strncasecmp(domain, &altNameBuf[1], dlen))
									break;
							} else if (CFEqual(altNameLabel, CFSTR("IP Address"))) {
								if (ntype == IS_DNS)
									continue;

#ifdef LDAP_PF_INET6
								if (ntype == IS_IP6) {
									struct in6_addr altAddr6;
									memset(&altAddr6, 0, sizeof altAddr6);
									if (inet_pton(AF_INET6, altNameBuf, &altAddr6) > 0 && !memcmp(&addr, &altAddr6, sizeof altAddr6))
										break;
								} else
#endif
								if (ntype == IS_IP4) {
									struct in_addr altAddr4;
									memset(&altAddr4, 0, sizeof altAddr4);
									if (inet_aton(altNameBuf, &altAddr4) && !memcmp(&addr, &altAddr4, sizeof altAddr4))
										break;
								}
							}
						}
					}
				}
				if (i < CFArrayGetCount(altNames))
					ret = LDAP_SUCCESS;
			}
		}

		if (ret != LDAP_SUCCESS) {
			CFStringRef commonName = NULL;

			/* find the last CN */
			CFDictionaryRef commonNamesValues = (CFDictionaryRef) CFDictionaryGetValue(certContents, kSecOIDCommonName);
			if (commonNamesValues != NULL && CFGetTypeID(commonNamesValues) == CFDictionaryGetTypeID()) {
				CFArrayRef commonNames = (CFArrayRef) CFDictionaryGetValue(commonNamesValues, kSecPropertyKeyValue);
				if (commonNames != NULL && CFGetTypeID(commonNames) == CFArrayGetTypeID() && CFArrayGetCount(commonNames) > 0)
					commonName = CFArrayGetValueAtIndex(commonNames, CFArrayGetCount(commonNames) - 1);
			}

			CFIndex commonNameLen = 0;
			CFIndex commonNameSize = 0;
			char *commonNameBuf = NULL;
			if (commonName != NULL) {
				commonNameLen = CFStringGetLength(commonName);
				commonNameSize = CFStringGetMaximumSizeForEncoding(commonNameLen, kCFStringEncodingUTF8) + 1;
				commonNameBuf = LDAP_MALLOC(commonNameSize);

				if (!CFStringGetCString(commonName, commonNameBuf, commonNameSize, kCFStringEncodingUTF8))
					commonNameLen = 0;
			}

			if (commonNameLen == 0) {
				Debug(LDAP_DEBUG_ANY, "TLS: unable to get common name from peer certificate\n", 0, 0, 0);
				ret = LDAP_CONNECT_ERROR;
				if (ld->ld_error)
					LDAP_FREE(ld->ld_error);
				ld->ld_error = LDAP_STRDUP("TLS: unable to get CN from peer certificate");
			} else if (commonNameLen == nlen && strncasecmp(name, commonNameBuf, nlen) == 0)
				ret = LDAP_SUCCESS;
			else if (domain && dlen == commonNameLen - 1 && commonNameBuf[0] == '*' && commonNameBuf[1] == '.' && !strncasecmp(domain, &commonNameBuf[1], dlen))
				ret = LDAP_SUCCESS;

			if( ret == LDAP_LOCAL_ERROR ) {
				Debug( LDAP_DEBUG_ANY, "TLS: hostname (%s) does not match "
					"common name in certificate (%.*s).\n", 
					name, (int) commonNameLen, commonNameBuf );
				ret = LDAP_CONNECT_ERROR;
				if ( ld->ld_error ) {
					LDAP_FREE( ld->ld_error );
				}
				ld->ld_error = LDAP_STRDUP(
					_("TLS: hostname does not match CN in peer certificate"));
			}

			if (commonNameBuf != NULL)
				LDAP_FREE(commonNameBuf);
		}

		CFRelease_and_null(certContents);
	}

	CFRelease_and_null(cert);
	return ret;
}

static int
tlsst_session_strength(tls_session *_sess)
{
	tlsst_session *sess = (tlsst_session *) _sess;
	Debug(LDAP_DEBUG_TRACE, "tlsst_session_strength(%p)\n", sess, 0, 0);

	int result = 0;

	SSLCipherSuite cipher = SSL_NO_SUCH_CIPHERSUITE;
	if (SSLGetNegotiatedCipher(sess->ssl, &cipher) != 0)
		cipher = SSL_NO_SUCH_CIPHERSUITE;
	if (cipher != SSL_NO_SUCH_CIPHERSUITE) {
		CFDictionaryRef properties = XSCipherCopyCipherProperties(cipher);
		if (properties != NULL) {
			CFNumberRef bits = CFDictionaryGetValue(properties, kXSCipherPropertyStrengthBits);
			if (bits != NULL) {
				if (!CFNumberGetValue(bits, kCFNumberIntType, &result))
					result = 0;
			}

			CFRelease_and_null(properties);
		}
	}

	return result;
}

static void
tlsst_thr_init(void)
{
	Debug(LDAP_DEBUG_TRACE, "tlsst_thr_init()\n", 0, 0, 0);
}

static int
tlsst_sb_setup(Sockbuf_IO_Desc *sbiod, void *arg)
{
	tlsst_session *sess = (tlsst_session *) arg;
	Debug(LDAP_DEBUG_TRACE, "tlsst_sb_setup(%p)\n", sess, 0, 0);
	assert(sess->sbiod == NULL);
	sess->sbiod = sbiod;
	sbiod->sbiod_pvt = sess;
	return 0;
}

static int
tlsst_sb_remove(Sockbuf_IO_Desc *sbiod)
{
	tlsst_session *sess = (tlsst_session *) sbiod->sbiod_pvt;
	Debug(LDAP_DEBUG_TRACE, "tlsst_sb_remove(%p)\n", sess, 0, 0);
	sbiod->sbiod_pvt = NULL;
	sess->sbiod = NULL;
	tlsst_session_free(sess);
	return 0;
}

static int
tlsst_sb_ctrl(Sockbuf_IO_Desc *sbiod, int opt, void *arg)
{
	tlsst_session *sess = (tlsst_session *) sbiod->sbiod_pvt;
	Debug(LDAP_DEBUG_TRACE, "tlsst_sb_ctrl(%p, %d)\n", sess, opt, 0);

	switch (opt) {
	case LBER_SB_OPT_GET_SSL:
		*(tlsst_session **) arg = sess;
		return 1;
	case LBER_SB_OPT_DATA_READY:
		{
			size_t bytes = 0;
			(void) SSLGetBufferedReadSize(sess->ssl, &bytes);
			if (bytes > 0)
				return 1;
			// else pass opt to next sbiod
		}
		break;
	}

	return LBER_SBIOD_CTRL_NEXT(sbiod, opt, arg);
}

/* read cleartext data */
static ber_slen_t
tlsst_sb_read(Sockbuf_IO_Desc *sbiod, void *buf, ber_len_t len)
{
	tlsst_session *sess = (tlsst_session *) sbiod->sbiod_pvt;
	Debug(LDAP_DEBUG_TRACE, "tlsst_sb_read(%p, %lu)\n", sess, len, 0);

	if (len <= 0)
		return 0;

	sess->want_read = FALSE;
	sess->want_write = FALSE;

	size_t processed = 0;
	OSStatus ret = SSLRead(sess->ssl, buf, len, &processed);
	if (tlsst_session_upflags(sbiod->sbiod_sb, (tls_session *) sess, ret))
		sock_errset(EWOULDBLOCK);
	if (processed > 0)
		return processed;
	if (ret == errSSLWouldBlock) {
		sock_errset(EWOULDBLOCK);
		return -1;
	}
	if (ret)
		tlsst_save_error(sess, ret, "SSLRead()", NULL);
	else
		tlsst_clear_error(sess);
	if (ret == errSSLClosedGraceful)
		return 0;
	return -1;
}

/* write cleartext data */
static ber_slen_t
tlsst_sb_write(Sockbuf_IO_Desc *sbiod, void *buf, ber_len_t len)
{
	tlsst_session *sess = (tlsst_session *) sbiod->sbiod_pvt;
	Debug(LDAP_DEBUG_TRACE, "tlsst_sb_write(%p, %lu)\n", sess, len, 0);

	if (len <= 0)
		return 0;

	sess->want_read = FALSE;
	sess->want_write = FALSE;

	size_t processed = 0;
	OSStatus ret = SSLWrite(sess->ssl, buf, len, &processed);
	if (tlsst_session_upflags(sbiod->sbiod_sb, (tls_session *) sess, ret))
		sock_errset(EWOULDBLOCK);
	if (processed > 0)
		return processed;
	if (ret == errSSLWouldBlock) {
		sock_errset(EWOULDBLOCK);
		return -1;
	}
	if (ret)
		tlsst_save_error(sess, ret, "SSLWrite()", NULL);
	else
		tlsst_clear_error(sess);
	return -1;
}

static int
tlsst_sb_close(Sockbuf_IO_Desc *sbiod)
{
	tlsst_session *sess = (tlsst_session *) sbiod->sbiod_pvt;
	Debug(LDAP_DEBUG_TRACE, "tlsst_sb_close(%p)\n", sess, 0, 0);

	OSStatus ret = SSLClose(sess->ssl);
	if (ret)
		tlsst_save_error(sess, ret, "SSLClose()", NULL);
	else
		tlsst_clear_error(sess);
	return ret == 0 ? 0 : -1;
}

static Sockbuf_IO tlsst_sbio = {
	tlsst_sb_setup,		/* sbi_setup */
	tlsst_sb_remove,	/* sbi_remove */
	tlsst_sb_ctrl,		/* sbi_ctrl */
	tlsst_sb_read,		/* sbi_read */
	tlsst_sb_write,		/* sbi_write */
	tlsst_sb_close		/* sbi_close */
};

tls_impl ldap_int_tls_impl = {
	"SecureTransport",

	tlsst_init,
	tlsst_destroy,

	tlsst_ctx_new,
	tlsst_ctx_ref,
	tlsst_ctx_free,
	tlsst_ctx_init,

	tlsst_session_new,
	tlsst_session_connect,
	tlsst_session_accept,
	tlsst_session_upflags,
	tlsst_session_errmsg,
	tlsst_session_my_dn,
	tlsst_session_peer_dn,
	tlsst_session_chkhost,
	tlsst_session_strength,

	&tlsst_sbio,

#ifdef LDAP_R_COMPILE
	tlsst_thr_init,
#else
	NULL,
#endif

	0
};

#endif /* HAVE_SECURE_TRANSPORT */
