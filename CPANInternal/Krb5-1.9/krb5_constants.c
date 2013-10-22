static double
constant(name, arg)
char *name;
int arg;
{
    errno = 0;
    switch (*name) {
    case 'A':
	if (strEQ(name, "ADDRTYPE_ADDRPORT"))
#ifdef ADDRTYPE_ADDRPORT
	    return ADDRTYPE_ADDRPORT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ADDRTYPE_CHAOS"))
#ifdef ADDRTYPE_CHAOS
	    return ADDRTYPE_CHAOS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ADDRTYPE_DDP"))
#ifdef ADDRTYPE_DDP
	    return ADDRTYPE_DDP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ADDRTYPE_INET"))
#ifdef ADDRTYPE_INET
	    return ADDRTYPE_INET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ADDRTYPE_IPPORT"))
#ifdef ADDRTYPE_IPPORT
	    return ADDRTYPE_IPPORT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ADDRTYPE_ISO"))
#ifdef ADDRTYPE_ISO
	    return ADDRTYPE_ISO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ADDRTYPE_XNS"))
#ifdef ADDRTYPE_XNS
	    return ADDRTYPE_XNS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AD_TYPE_EXTERNAL"))
#ifdef AD_TYPE_EXTERNAL
	    return AD_TYPE_EXTERNAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AD_TYPE_FIELD_TYPE_MASK"))
#ifdef AD_TYPE_FIELD_TYPE_MASK
	    return AD_TYPE_FIELD_TYPE_MASK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AD_TYPE_INTERNAL_MASK"))
#ifdef AD_TYPE_INTERNAL_MASK
	    return AD_TYPE_INTERNAL_MASK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AD_TYPE_REGISTERED"))
#ifdef AD_TYPE_REGISTERED
	    return AD_TYPE_REGISTERED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AD_TYPE_RESERVED"))
#ifdef AD_TYPE_RESERVED
	    return AD_TYPE_RESERVED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ANSI_STDIO"))
#ifdef ANSI_STDIO
	    return ANSI_STDIO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AP_OPTS_MUTUAL_REQUIRED"))
#ifdef AP_OPTS_MUTUAL_REQUIRED
	    return AP_OPTS_MUTUAL_REQUIRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AP_OPTS_RESERVED"))
#ifdef AP_OPTS_RESERVED
	    return AP_OPTS_RESERVED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AP_OPTS_USE_SESSION_KEY"))
#ifdef AP_OPTS_USE_SESSION_KEY
	    return AP_OPTS_USE_SESSION_KEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AP_OPTS_USE_SUBKEY"))
#ifdef AP_OPTS_USE_SUBKEY
	    return AP_OPTS_USE_SUBKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "AP_OPTS_WIRE_MASK"))
#ifdef AP_OPTS_WIRE_MASK
	    return AP_OPTS_WIRE_MASK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_BAD_FORMAT"))
#ifdef ASN1_BAD_FORMAT
	    return ASN1_BAD_FORMAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_BAD_ID"))
#ifdef ASN1_BAD_ID
	    return ASN1_BAD_ID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_BAD_LENGTH"))
#ifdef ASN1_BAD_LENGTH
	    return ASN1_BAD_LENGTH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_BAD_TIMEFORMAT"))
#ifdef ASN1_BAD_TIMEFORMAT
	    return ASN1_BAD_TIMEFORMAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_MISPLACED_FIELD"))
#ifdef ASN1_MISPLACED_FIELD
	    return ASN1_MISPLACED_FIELD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_MISSING_FIELD"))
#ifdef ASN1_MISSING_FIELD
	    return ASN1_MISSING_FIELD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_OVERFLOW"))
#ifdef ASN1_OVERFLOW
	    return ASN1_OVERFLOW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_OVERRUN"))
#ifdef ASN1_OVERRUN
	    return ASN1_OVERRUN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_PARSE_ERROR"))
#ifdef ASN1_PARSE_ERROR
	    return ASN1_PARSE_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ASN1_TYPE_MISMATCH"))
#ifdef ASN1_TYPE_MISMATCH
	    return ASN1_TYPE_MISMATCH;
#else
	    goto not_there;
#endif
	break;
    case 'B':
	break;
    case 'C':
	if (strEQ(name, "CKSUMTYPE_CRC32"))
#ifdef CKSUMTYPE_CRC32
	    return CKSUMTYPE_CRC32;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_DESCBC"))
#ifdef CKSUMTYPE_DESCBC
	    return CKSUMTYPE_DESCBC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_HMAC_SHA"))
#ifdef CKSUMTYPE_HMAC_SHA
	    return CKSUMTYPE_HMAC_SHA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_NIST_SHA"))
#ifdef CKSUMTYPE_NIST_SHA
	    return CKSUMTYPE_NIST_SHA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_RSA_MD4"))
#ifdef CKSUMTYPE_RSA_MD4
	    return CKSUMTYPE_RSA_MD4;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_RSA_MD4_DES"))
#ifdef CKSUMTYPE_RSA_MD4_DES
	    return CKSUMTYPE_RSA_MD4_DES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_RSA_MD5"))
#ifdef CKSUMTYPE_RSA_MD5
	    return CKSUMTYPE_RSA_MD5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "CKSUMTYPE_RSA_MD5_DES"))
#ifdef CKSUMTYPE_RSA_MD5_DES
	    return CKSUMTYPE_RSA_MD5_DES;
#else
	    goto not_there;
#endif
	break;
    case 'D':
	break;
    case 'E':
	if (strEQ(name, "ENCTYPE_DES3_CBC_RAW"))
#ifdef ENCTYPE_DES3_CBC_RAW
	    return ENCTYPE_DES3_CBC_RAW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_DES3_CBC_SHA"))
#ifdef ENCTYPE_DES3_CBC_SHA
	    return ENCTYPE_DES3_CBC_SHA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_DES_CBC_CRC"))
#ifdef ENCTYPE_DES_CBC_CRC
	    return ENCTYPE_DES_CBC_CRC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_DES_CBC_MD4"))
#ifdef ENCTYPE_DES_CBC_MD4
	    return ENCTYPE_DES_CBC_MD4;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_DES_CBC_MD5"))
#ifdef ENCTYPE_DES_CBC_MD5
	    return ENCTYPE_DES_CBC_MD5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_DES_CBC_RAW"))
#ifdef ENCTYPE_DES_CBC_RAW
	    return ENCTYPE_DES_CBC_RAW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_NULL"))
#ifdef ENCTYPE_NULL
	    return ENCTYPE_NULL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENCTYPE_UNKNOWN"))
#ifdef ENCTYPE_UNKNOWN
	    return ENCTYPE_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ENOMEM"))
#ifdef ENOMEM
	    return ENOMEM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ERROR_TABLE_BASE_asn1"))
#ifdef ERROR_TABLE_BASE_asn1
	    return ERROR_TABLE_BASE_asn1;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ERROR_TABLE_BASE_kdb5"))
#ifdef ERROR_TABLE_BASE_kdb5
	    return ERROR_TABLE_BASE_kdb5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ERROR_TABLE_BASE_krb5"))
#ifdef ERROR_TABLE_BASE_krb5
	    return ERROR_TABLE_BASE_krb5;
#else
	    goto not_there;
#endif
	if (strEQ(name, "ERROR_TABLE_BASE_kv5m"))
#ifdef ERROR_TABLE_BASE_kv5m
	    return ERROR_TABLE_BASE_kv5m;
#else
	    goto not_there;
#endif
	break;
    case 'F':
	if (strEQ(name, "FALSE"))
#ifdef FALSE
	    return FALSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "FAR"))
#ifdef FAR
	    return FAR;
#else
	    goto not_there;
#endif
	break;
    case 'G':
	break;
    case 'H':
	if (strEQ(name, "HAS_LABS"))
#ifdef HAS_LABS
	    return HAS_LABS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "HAS_VOID_TYPE"))
#ifdef HAS_VOID_TYPE
	    return HAS_VOID_TYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "HAVE_SRAND"))
#ifdef HAVE_SRAND
	    return HAVE_SRAND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "HAVE_STDARG_H"))
#ifdef HAVE_STDARG_H
	    return HAVE_STDARG_H;
#else
	    goto not_there;
#endif
	if (strEQ(name, "HAVE_SYS_TYPES_H"))
#ifdef HAVE_SYS_TYPES_H
	    return HAVE_SYS_TYPES_H;
#else
	    goto not_there;
#endif
	break;
    case 'I':
	if (strEQ(name, "INTERFACE"))
#ifdef INTERFACE
	    return INTERFACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "INTERFACE_C"))
#ifdef INTERFACE_C
	    return INTERFACE_C;
#else
	    goto not_there;
#endif
	break;
    case 'J':
	break;
    case 'K':
	if (strEQ(name, "KDC_OPT_ALLOW_POSTDATE"))
#ifdef KDC_OPT_ALLOW_POSTDATE
	    return KDC_OPT_ALLOW_POSTDATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_ENC_TKT_IN_SKEY"))
#ifdef KDC_OPT_ENC_TKT_IN_SKEY
	    return KDC_OPT_ENC_TKT_IN_SKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_FORWARDABLE"))
#ifdef KDC_OPT_FORWARDABLE
	    return KDC_OPT_FORWARDABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_FORWARDED"))
#ifdef KDC_OPT_FORWARDED
	    return KDC_OPT_FORWARDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_POSTDATED"))
#ifdef KDC_OPT_POSTDATED
	    return KDC_OPT_POSTDATED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_PROXIABLE"))
#ifdef KDC_OPT_PROXIABLE
	    return KDC_OPT_PROXIABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_PROXY"))
#ifdef KDC_OPT_PROXY
	    return KDC_OPT_PROXY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_RENEW"))
#ifdef KDC_OPT_RENEW
	    return KDC_OPT_RENEW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_RENEWABLE"))
#ifdef KDC_OPT_RENEWABLE
	    return KDC_OPT_RENEWABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_RENEWABLE_OK"))
#ifdef KDC_OPT_RENEWABLE_OK
	    return KDC_OPT_RENEWABLE_OK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_OPT_VALIDATE"))
#ifdef KDC_OPT_VALIDATE
	    return KDC_OPT_VALIDATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KDC_TKT_COMMON_MASK"))
#ifdef KDC_TKT_COMMON_MASK
	    return KDC_TKT_COMMON_MASK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5DES_BAD_KEYPAR"))
#ifdef KRB5DES_BAD_KEYPAR
	    return KRB5DES_BAD_KEYPAR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5DES_WEAK_KEY"))
#ifdef KRB5DES_WEAK_KEY
	    return KRB5DES_WEAK_KEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_BADOPTION"))
#ifdef KRB5KDC_ERR_BADOPTION
	    return KRB5KDC_ERR_BADOPTION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_BAD_PVNO"))
#ifdef KRB5KDC_ERR_BAD_PVNO
	    return KRB5KDC_ERR_BAD_PVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_CANNOT_POSTDATE"))
#ifdef KRB5KDC_ERR_CANNOT_POSTDATE
	    return KRB5KDC_ERR_CANNOT_POSTDATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_CLIENT_NOTYET"))
#ifdef KRB5KDC_ERR_CLIENT_NOTYET
	    return KRB5KDC_ERR_CLIENT_NOTYET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_CLIENT_REVOKED"))
#ifdef KRB5KDC_ERR_CLIENT_REVOKED
	    return KRB5KDC_ERR_CLIENT_REVOKED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_C_OLD_MAST_KVNO"))
#ifdef KRB5KDC_ERR_C_OLD_MAST_KVNO
	    return KRB5KDC_ERR_C_OLD_MAST_KVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN"))
#ifdef KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN
	    return KRB5KDC_ERR_C_PRINCIPAL_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_ETYPE_NOSUPP"))
#ifdef KRB5KDC_ERR_ETYPE_NOSUPP
	    return KRB5KDC_ERR_ETYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_KEY_EXP"))
#ifdef KRB5KDC_ERR_KEY_EXP
	    return KRB5KDC_ERR_KEY_EXP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_NAME_EXP"))
#ifdef KRB5KDC_ERR_NAME_EXP
	    return KRB5KDC_ERR_NAME_EXP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_NEVER_VALID"))
#ifdef KRB5KDC_ERR_NEVER_VALID
	    return KRB5KDC_ERR_NEVER_VALID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_NONE"))
#ifdef KRB5KDC_ERR_NONE
	    return KRB5KDC_ERR_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_NULL_KEY"))
#ifdef KRB5KDC_ERR_NULL_KEY
	    return KRB5KDC_ERR_NULL_KEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_PADATA_TYPE_NOSUPP"))
#ifdef KRB5KDC_ERR_PADATA_TYPE_NOSUPP
	    return KRB5KDC_ERR_PADATA_TYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_POLICY"))
#ifdef KRB5KDC_ERR_POLICY
	    return KRB5KDC_ERR_POLICY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_PREAUTH_FAILED"))
#ifdef KRB5KDC_ERR_PREAUTH_FAILED
	    return KRB5KDC_ERR_PREAUTH_FAILED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_PREAUTH_REQUIRED"))
#ifdef KRB5KDC_ERR_PREAUTH_REQUIRED
	    return KRB5KDC_ERR_PREAUTH_REQUIRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE"))
#ifdef KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE
	    return KRB5KDC_ERR_PRINCIPAL_NOT_UNIQUE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_SERVER_NOMATCH"))
#ifdef KRB5KDC_ERR_SERVER_NOMATCH
	    return KRB5KDC_ERR_SERVER_NOMATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_SERVICE_EXP"))
#ifdef KRB5KDC_ERR_SERVICE_EXP
	    return KRB5KDC_ERR_SERVICE_EXP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_SERVICE_NOTYET"))
#ifdef KRB5KDC_ERR_SERVICE_NOTYET
	    return KRB5KDC_ERR_SERVICE_NOTYET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_SERVICE_REVOKED"))
#ifdef KRB5KDC_ERR_SERVICE_REVOKED
	    return KRB5KDC_ERR_SERVICE_REVOKED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_SUMTYPE_NOSUPP"))
#ifdef KRB5KDC_ERR_SUMTYPE_NOSUPP
	    return KRB5KDC_ERR_SUMTYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_S_OLD_MAST_KVNO"))
#ifdef KRB5KDC_ERR_S_OLD_MAST_KVNO
	    return KRB5KDC_ERR_S_OLD_MAST_KVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN"))
#ifdef KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN
	    return KRB5KDC_ERR_S_PRINCIPAL_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_TGT_REVOKED"))
#ifdef KRB5KDC_ERR_TGT_REVOKED
	    return KRB5KDC_ERR_TGT_REVOKED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KDC_ERR_TRTYPE_NOSUPP"))
#ifdef KRB5KDC_ERR_TRTYPE_NOSUPP
	    return KRB5KDC_ERR_TRTYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADADDR"))
#ifdef KRB5KRB_AP_ERR_BADADDR
	    return KRB5KRB_AP_ERR_BADADDR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADDIRECTION"))
#ifdef KRB5KRB_AP_ERR_BADDIRECTION
	    return KRB5KRB_AP_ERR_BADDIRECTION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADKEYVER"))
#ifdef KRB5KRB_AP_ERR_BADKEYVER
	    return KRB5KRB_AP_ERR_BADKEYVER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADMATCH"))
#ifdef KRB5KRB_AP_ERR_BADMATCH
	    return KRB5KRB_AP_ERR_BADMATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADORDER"))
#ifdef KRB5KRB_AP_ERR_BADORDER
	    return KRB5KRB_AP_ERR_BADORDER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADSEQ"))
#ifdef KRB5KRB_AP_ERR_BADSEQ
	    return KRB5KRB_AP_ERR_BADSEQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BADVERSION"))
#ifdef KRB5KRB_AP_ERR_BADVERSION
	    return KRB5KRB_AP_ERR_BADVERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_BAD_INTEGRITY"))
#ifdef KRB5KRB_AP_ERR_BAD_INTEGRITY
	    return KRB5KRB_AP_ERR_BAD_INTEGRITY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_ILL_CR_TKT"))
#ifdef KRB5KRB_AP_ERR_ILL_CR_TKT
	    return KRB5KRB_AP_ERR_ILL_CR_TKT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_INAPP_CKSUM"))
#ifdef KRB5KRB_AP_ERR_INAPP_CKSUM
	    return KRB5KRB_AP_ERR_INAPP_CKSUM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_METHOD"))
#ifdef KRB5KRB_AP_ERR_METHOD
	    return KRB5KRB_AP_ERR_METHOD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_MODIFIED"))
#ifdef KRB5KRB_AP_ERR_MODIFIED
	    return KRB5KRB_AP_ERR_MODIFIED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_MSG_TYPE"))
#ifdef KRB5KRB_AP_ERR_MSG_TYPE
	    return KRB5KRB_AP_ERR_MSG_TYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_MUT_FAIL"))
#ifdef KRB5KRB_AP_ERR_MUT_FAIL
	    return KRB5KRB_AP_ERR_MUT_FAIL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_NOKEY"))
#ifdef KRB5KRB_AP_ERR_NOKEY
	    return KRB5KRB_AP_ERR_NOKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_NOT_US"))
#ifdef KRB5KRB_AP_ERR_NOT_US
	    return KRB5KRB_AP_ERR_NOT_US;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_REPEAT"))
#ifdef KRB5KRB_AP_ERR_REPEAT
	    return KRB5KRB_AP_ERR_REPEAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_SKEW"))
#ifdef KRB5KRB_AP_ERR_SKEW
	    return KRB5KRB_AP_ERR_SKEW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_TKT_EXPIRED"))
#ifdef KRB5KRB_AP_ERR_TKT_EXPIRED
	    return KRB5KRB_AP_ERR_TKT_EXPIRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_TKT_INVALID"))
#ifdef KRB5KRB_AP_ERR_TKT_INVALID
	    return KRB5KRB_AP_ERR_TKT_INVALID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_TKT_NYV"))
#ifdef KRB5KRB_AP_ERR_TKT_NYV
	    return KRB5KRB_AP_ERR_TKT_NYV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_ERR_V4_REPLY"))
#ifdef KRB5KRB_AP_ERR_V4_REPLY
	    return KRB5KRB_AP_ERR_V4_REPLY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_AP_WRONG_PRINC"))
#ifdef KRB5KRB_AP_WRONG_PRINC
	    return KRB5KRB_AP_WRONG_PRINC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_ERR_FIELD_TOOLONG"))
#ifdef KRB5KRB_ERR_FIELD_TOOLONG
	    return KRB5KRB_ERR_FIELD_TOOLONG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5KRB_ERR_GENERIC"))
#ifdef KRB5KRB_ERR_GENERIC
	    return KRB5KRB_ERR_GENERIC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_100"))
#ifdef KRB5PLACEHOLD_100
	    return KRB5PLACEHOLD_100;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_101"))
#ifdef KRB5PLACEHOLD_101
	    return KRB5PLACEHOLD_101;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_102"))
#ifdef KRB5PLACEHOLD_102
	    return KRB5PLACEHOLD_102;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_103"))
#ifdef KRB5PLACEHOLD_103
	    return KRB5PLACEHOLD_103;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_104"))
#ifdef KRB5PLACEHOLD_104
	    return KRB5PLACEHOLD_104;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_105"))
#ifdef KRB5PLACEHOLD_105
	    return KRB5PLACEHOLD_105;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_106"))
#ifdef KRB5PLACEHOLD_106
	    return KRB5PLACEHOLD_106;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_107"))
#ifdef KRB5PLACEHOLD_107
	    return KRB5PLACEHOLD_107;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_108"))
#ifdef KRB5PLACEHOLD_108
	    return KRB5PLACEHOLD_108;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_109"))
#ifdef KRB5PLACEHOLD_109
	    return KRB5PLACEHOLD_109;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_110"))
#ifdef KRB5PLACEHOLD_110
	    return KRB5PLACEHOLD_110;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_111"))
#ifdef KRB5PLACEHOLD_111
	    return KRB5PLACEHOLD_111;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_112"))
#ifdef KRB5PLACEHOLD_112
	    return KRB5PLACEHOLD_112;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_113"))
#ifdef KRB5PLACEHOLD_113
	    return KRB5PLACEHOLD_113;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_114"))
#ifdef KRB5PLACEHOLD_114
	    return KRB5PLACEHOLD_114;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_115"))
#ifdef KRB5PLACEHOLD_115
	    return KRB5PLACEHOLD_115;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_116"))
#ifdef KRB5PLACEHOLD_116
	    return KRB5PLACEHOLD_116;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_117"))
#ifdef KRB5PLACEHOLD_117
	    return KRB5PLACEHOLD_117;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_118"))
#ifdef KRB5PLACEHOLD_118
	    return KRB5PLACEHOLD_118;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_119"))
#ifdef KRB5PLACEHOLD_119
	    return KRB5PLACEHOLD_119;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_120"))
#ifdef KRB5PLACEHOLD_120
	    return KRB5PLACEHOLD_120;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_121"))
#ifdef KRB5PLACEHOLD_121
	    return KRB5PLACEHOLD_121;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_122"))
#ifdef KRB5PLACEHOLD_122
	    return KRB5PLACEHOLD_122;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_123"))
#ifdef KRB5PLACEHOLD_123
	    return KRB5PLACEHOLD_123;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_124"))
#ifdef KRB5PLACEHOLD_124
	    return KRB5PLACEHOLD_124;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_125"))
#ifdef KRB5PLACEHOLD_125
	    return KRB5PLACEHOLD_125;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_126"))
#ifdef KRB5PLACEHOLD_126
	    return KRB5PLACEHOLD_126;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_127"))
#ifdef KRB5PLACEHOLD_127
	    return KRB5PLACEHOLD_127;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_27"))
#ifdef KRB5PLACEHOLD_27
	    return KRB5PLACEHOLD_27;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_28"))
#ifdef KRB5PLACEHOLD_28
	    return KRB5PLACEHOLD_28;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_29"))
#ifdef KRB5PLACEHOLD_29
	    return KRB5PLACEHOLD_29;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_30"))
#ifdef KRB5PLACEHOLD_30
	    return KRB5PLACEHOLD_30;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_51"))
#ifdef KRB5PLACEHOLD_51
	    return KRB5PLACEHOLD_51;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_52"))
#ifdef KRB5PLACEHOLD_52
	    return KRB5PLACEHOLD_52;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_53"))
#ifdef KRB5PLACEHOLD_53
	    return KRB5PLACEHOLD_53;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_54"))
#ifdef KRB5PLACEHOLD_54
	    return KRB5PLACEHOLD_54;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_55"))
#ifdef KRB5PLACEHOLD_55
	    return KRB5PLACEHOLD_55;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_56"))
#ifdef KRB5PLACEHOLD_56
	    return KRB5PLACEHOLD_56;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_57"))
#ifdef KRB5PLACEHOLD_57
	    return KRB5PLACEHOLD_57;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_58"))
#ifdef KRB5PLACEHOLD_58
	    return KRB5PLACEHOLD_58;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_59"))
#ifdef KRB5PLACEHOLD_59
	    return KRB5PLACEHOLD_59;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_62"))
#ifdef KRB5PLACEHOLD_62
	    return KRB5PLACEHOLD_62;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_63"))
#ifdef KRB5PLACEHOLD_63
	    return KRB5PLACEHOLD_63;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_64"))
#ifdef KRB5PLACEHOLD_64
	    return KRB5PLACEHOLD_64;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_65"))
#ifdef KRB5PLACEHOLD_65
	    return KRB5PLACEHOLD_65;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_66"))
#ifdef KRB5PLACEHOLD_66
	    return KRB5PLACEHOLD_66;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_67"))
#ifdef KRB5PLACEHOLD_67
	    return KRB5PLACEHOLD_67;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_68"))
#ifdef KRB5PLACEHOLD_68
	    return KRB5PLACEHOLD_68;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_69"))
#ifdef KRB5PLACEHOLD_69
	    return KRB5PLACEHOLD_69;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_70"))
#ifdef KRB5PLACEHOLD_70
	    return KRB5PLACEHOLD_70;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_71"))
#ifdef KRB5PLACEHOLD_71
	    return KRB5PLACEHOLD_71;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_72"))
#ifdef KRB5PLACEHOLD_72
	    return KRB5PLACEHOLD_72;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_73"))
#ifdef KRB5PLACEHOLD_73
	    return KRB5PLACEHOLD_73;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_74"))
#ifdef KRB5PLACEHOLD_74
	    return KRB5PLACEHOLD_74;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_75"))
#ifdef KRB5PLACEHOLD_75
	    return KRB5PLACEHOLD_75;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_76"))
#ifdef KRB5PLACEHOLD_76
	    return KRB5PLACEHOLD_76;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_77"))
#ifdef KRB5PLACEHOLD_77
	    return KRB5PLACEHOLD_77;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_78"))
#ifdef KRB5PLACEHOLD_78
	    return KRB5PLACEHOLD_78;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_79"))
#ifdef KRB5PLACEHOLD_79
	    return KRB5PLACEHOLD_79;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_80"))
#ifdef KRB5PLACEHOLD_80
	    return KRB5PLACEHOLD_80;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_81"))
#ifdef KRB5PLACEHOLD_81
	    return KRB5PLACEHOLD_81;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_82"))
#ifdef KRB5PLACEHOLD_82
	    return KRB5PLACEHOLD_82;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_83"))
#ifdef KRB5PLACEHOLD_83
	    return KRB5PLACEHOLD_83;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_84"))
#ifdef KRB5PLACEHOLD_84
	    return KRB5PLACEHOLD_84;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_85"))
#ifdef KRB5PLACEHOLD_85
	    return KRB5PLACEHOLD_85;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_86"))
#ifdef KRB5PLACEHOLD_86
	    return KRB5PLACEHOLD_86;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_87"))
#ifdef KRB5PLACEHOLD_87
	    return KRB5PLACEHOLD_87;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_88"))
#ifdef KRB5PLACEHOLD_88
	    return KRB5PLACEHOLD_88;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_89"))
#ifdef KRB5PLACEHOLD_89
	    return KRB5PLACEHOLD_89;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_90"))
#ifdef KRB5PLACEHOLD_90
	    return KRB5PLACEHOLD_90;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_91"))
#ifdef KRB5PLACEHOLD_91
	    return KRB5PLACEHOLD_91;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_92"))
#ifdef KRB5PLACEHOLD_92
	    return KRB5PLACEHOLD_92;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_93"))
#ifdef KRB5PLACEHOLD_93
	    return KRB5PLACEHOLD_93;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_94"))
#ifdef KRB5PLACEHOLD_94
	    return KRB5PLACEHOLD_94;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_95"))
#ifdef KRB5PLACEHOLD_95
	    return KRB5PLACEHOLD_95;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_96"))
#ifdef KRB5PLACEHOLD_96
	    return KRB5PLACEHOLD_96;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_97"))
#ifdef KRB5PLACEHOLD_97
	    return KRB5PLACEHOLD_97;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_98"))
#ifdef KRB5PLACEHOLD_98
	    return KRB5PLACEHOLD_98;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5PLACEHOLD_99"))
#ifdef KRB5PLACEHOLD_99
	    return KRB5PLACEHOLD_99;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_ALTAUTH_ATT_CHALLENGE_RESPONSE"))
#ifdef KRB5_ALTAUTH_ATT_CHALLENGE_RESPONSE
	    return KRB5_ALTAUTH_ATT_CHALLENGE_RESPONSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AP_REP"))
#ifdef KRB5_AP_REP
	    return KRB5_AP_REP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AP_REQ"))
#ifdef KRB5_AP_REQ
	    return KRB5_AP_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AS_REP"))
#ifdef KRB5_AS_REP
	    return KRB5_AS_REP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AS_REQ"))
#ifdef KRB5_AS_REQ
	    return KRB5_AS_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTHDATA_OSF_DCE"))
#ifdef KRB5_AUTHDATA_OSF_DCE
	    return KRB5_AUTHDATA_OSF_DCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTHDATA_SESAME"))
#ifdef KRB5_AUTHDATA_SESAME
	    return KRB5_AUTHDATA_SESAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_DO_SEQUENCE"))
#ifdef KRB5_AUTH_CONTEXT_DO_SEQUENCE
	    return KRB5_AUTH_CONTEXT_DO_SEQUENCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_DO_TIME"))
#ifdef KRB5_AUTH_CONTEXT_DO_TIME
	    return KRB5_AUTH_CONTEXT_DO_TIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR"))
#ifdef KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR
	    return KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR"))
#ifdef KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR
	    return KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_GENERATE_REMOTE_ADDR"))
#ifdef KRB5_AUTH_CONTEXT_GENERATE_REMOTE_ADDR
	    return KRB5_AUTH_CONTEXT_GENERATE_REMOTE_ADDR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR"))
#ifdef KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR
	    return KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_RET_SEQUENCE"))
#ifdef KRB5_AUTH_CONTEXT_RET_SEQUENCE
	    return KRB5_AUTH_CONTEXT_RET_SEQUENCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_AUTH_CONTEXT_RET_TIME"))
#ifdef KRB5_AUTH_CONTEXT_RET_TIME
	    return KRB5_AUTH_CONTEXT_RET_TIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_BADMSGTYPE"))
#ifdef KRB5_BADMSGTYPE
	    return KRB5_BADMSGTYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_BAD_ENCTYPE"))
#ifdef KRB5_BAD_ENCTYPE
	    return KRB5_BAD_ENCTYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_BAD_KEYSIZE"))
#ifdef KRB5_BAD_KEYSIZE
	    return KRB5_BAD_KEYSIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_BAD_MSIZE"))
#ifdef KRB5_BAD_MSIZE
	    return KRB5_BAD_MSIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CCACHE_BADVNO"))
#ifdef KRB5_CCACHE_BADVNO
	    return KRB5_CCACHE_BADVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_BADNAME"))
#ifdef KRB5_CC_BADNAME
	    return KRB5_CC_BADNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_END"))
#ifdef KRB5_CC_END
	    return KRB5_CC_END;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_FORMAT"))
#ifdef KRB5_CC_FORMAT
	    return KRB5_CC_FORMAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_IO"))
#ifdef KRB5_CC_IO
	    return KRB5_CC_IO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_NOMEM"))
#ifdef KRB5_CC_NOMEM
	    return KRB5_CC_NOMEM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_NOTFOUND"))
#ifdef KRB5_CC_NOTFOUND
	    return KRB5_CC_NOTFOUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_TYPE_EXISTS"))
#ifdef KRB5_CC_TYPE_EXISTS
	    return KRB5_CC_TYPE_EXISTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_UNKNOWN_TYPE"))
#ifdef KRB5_CC_UNKNOWN_TYPE
	    return KRB5_CC_UNKNOWN_TYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CC_WRITE"))
#ifdef KRB5_CC_WRITE
	    return KRB5_CC_WRITE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CONFIG_BADFORMAT"))
#ifdef KRB5_CONFIG_BADFORMAT
	    return KRB5_CONFIG_BADFORMAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CONFIG_CANTOPEN"))
#ifdef KRB5_CONFIG_CANTOPEN
	    return KRB5_CONFIG_CANTOPEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CONFIG_NODEFREALM"))
#ifdef KRB5_CONFIG_NODEFREALM
	    return KRB5_CONFIG_NODEFREALM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CONFIG_NOTENUFSPACE"))
#ifdef KRB5_CONFIG_NOTENUFSPACE
	    return KRB5_CONFIG_NOTENUFSPACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CRED"))
#ifdef KRB5_CRED
	    return KRB5_CRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CRYPTO_INTERNAL"))
#ifdef KRB5_CRYPTO_INTERNAL
	    return KRB5_CRYPTO_INTERNAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_CYBERSAFE_SECUREID"))
#ifdef KRB5_CYBERSAFE_SECUREID
	    return KRB5_CYBERSAFE_SECUREID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_DECLSPEC"))
#ifdef KRB5_DECLSPEC
	    return KRB5_DECLSPEC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_DLLIMP"))
#ifdef KRB5_DLLIMP
	    return KRB5_DLLIMP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_DOMAIN_X500_COMPRESS"))
#ifdef KRB5_DOMAIN_X500_COMPRESS
	    return KRB5_DOMAIN_X500_COMPRESS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_ERROR"))
#ifdef KRB5_ERROR
	    return KRB5_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_ERR_BAD_HOSTNAME"))
#ifdef KRB5_ERR_BAD_HOSTNAME
	    return KRB5_ERR_BAD_HOSTNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_ERR_HOST_REALM_UNKNOWN"))
#ifdef KRB5_ERR_HOST_REALM_UNKNOWN
	    return KRB5_ERR_HOST_REALM_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_ERR_RCSID"))
#ifdef KRB5_ERR_RCSID
	    return KRB5_ERR_RCSID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_FCC_INTERNAL"))
#ifdef KRB5_FCC_INTERNAL
	    return KRB5_FCC_INTERNAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_FCC_NOFILE"))
#ifdef KRB5_FCC_NOFILE
	    return KRB5_FCC_NOFILE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_FCC_PERM"))
#ifdef KRB5_FCC_PERM
	    return KRB5_FCC_PERM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_FWD_BAD_PRINCIPAL"))
#ifdef KRB5_FWD_BAD_PRINCIPAL
	    return KRB5_FWD_BAD_PRINCIPAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_GC_CACHED"))
#ifdef KRB5_GC_CACHED
	    return KRB5_GC_CACHED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_GC_USER_USER"))
#ifdef KRB5_GC_USER_USER
	    return KRB5_GC_USER_USER;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_GET_IN_TKT_LOOP"))
#ifdef KRB5_GET_IN_TKT_LOOP
	    return KRB5_GET_IN_TKT_LOOP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_INT16_MAX"))
#ifdef KRB5_INT16_MAX
	    return KRB5_INT16_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_INT16_MIN"))
#ifdef KRB5_INT16_MIN
	    return KRB5_INT16_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_INT32_MAX"))
#ifdef KRB5_INT32_MAX
	    return KRB5_INT32_MAX;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_INT32_MIN"))
#ifdef KRB5_INT32_MIN
	    return KRB5_INT32_MIN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_INVALID_FLAGS"))
#ifdef KRB5_INVALID_FLAGS
	    return KRB5_INVALID_FLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_IN_TKT_REALM_MISMATCH"))
#ifdef KRB5_IN_TKT_REALM_MISMATCH
	    return KRB5_IN_TKT_REALM_MISMATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_BADLOCKMODE"))
#ifdef KRB5_KDB_BADLOCKMODE
	    return KRB5_KDB_BADLOCKMODE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_BADMASTERKEY"))
#ifdef KRB5_KDB_BADMASTERKEY
	    return KRB5_KDB_BADMASTERKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_BADSTORED_MKEY"))
#ifdef KRB5_KDB_BADSTORED_MKEY
	    return KRB5_KDB_BADSTORED_MKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_BAD_ENCTYPE"))
#ifdef KRB5_KDB_BAD_ENCTYPE
	    return KRB5_KDB_BAD_ENCTYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_BAD_SALTTYPE"))
#ifdef KRB5_KDB_BAD_SALTTYPE
	    return KRB5_KDB_BAD_SALTTYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_BAD_VERSION"))
#ifdef KRB5_KDB_BAD_VERSION
	    return KRB5_KDB_BAD_VERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_CANTLOCK_DB"))
#ifdef KRB5_KDB_CANTLOCK_DB
	    return KRB5_KDB_CANTLOCK_DB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_CANTREAD_STORED"))
#ifdef KRB5_KDB_CANTREAD_STORED
	    return KRB5_KDB_CANTREAD_STORED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_DBINITED"))
#ifdef KRB5_KDB_DBINITED
	    return KRB5_KDB_DBINITED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_DBNOTINITED"))
#ifdef KRB5_KDB_DBNOTINITED
	    return KRB5_KDB_DBNOTINITED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_DB_CHANGED"))
#ifdef KRB5_KDB_DB_CHANGED
	    return KRB5_KDB_DB_CHANGED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_DB_CORRUPT"))
#ifdef KRB5_KDB_DB_CORRUPT
	    return KRB5_KDB_DB_CORRUPT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_DB_INUSE"))
#ifdef KRB5_KDB_DB_INUSE
	    return KRB5_KDB_DB_INUSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_ILLDIRECTION"))
#ifdef KRB5_KDB_ILLDIRECTION
	    return KRB5_KDB_ILLDIRECTION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_ILL_WILDCARD"))
#ifdef KRB5_KDB_ILL_WILDCARD
	    return KRB5_KDB_ILL_WILDCARD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_INUSE"))
#ifdef KRB5_KDB_INUSE
	    return KRB5_KDB_INUSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_INVALIDKEYSIZE"))
#ifdef KRB5_KDB_INVALIDKEYSIZE
	    return KRB5_KDB_INVALIDKEYSIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_NOENTRY"))
#ifdef KRB5_KDB_NOENTRY
	    return KRB5_KDB_NOENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_NOMASTERKEY"))
#ifdef KRB5_KDB_NOMASTERKEY
	    return KRB5_KDB_NOMASTERKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_NOTLOCKED"))
#ifdef KRB5_KDB_NOTLOCKED
	    return KRB5_KDB_NOTLOCKED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_RCSID"))
#ifdef KRB5_KDB_RCSID
	    return KRB5_KDB_RCSID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_RECURSIVELOCK"))
#ifdef KRB5_KDB_RECURSIVELOCK
	    return KRB5_KDB_RECURSIVELOCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_TRUNCATED_RECORD"))
#ifdef KRB5_KDB_TRUNCATED_RECORD
	    return KRB5_KDB_TRUNCATED_RECORD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_UK_RERROR"))
#ifdef KRB5_KDB_UK_RERROR
	    return KRB5_KDB_UK_RERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_UK_SERROR"))
#ifdef KRB5_KDB_UK_SERROR
	    return KRB5_KDB_UK_SERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDB_UNAUTH"))
#ifdef KRB5_KDB_UNAUTH
	    return KRB5_KDB_UNAUTH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDCREP_MODIFIED"))
#ifdef KRB5_KDCREP_MODIFIED
	    return KRB5_KDCREP_MODIFIED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDCREP_SKEW"))
#ifdef KRB5_KDCREP_SKEW
	    return KRB5_KDCREP_SKEW;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KDC_UNREACH"))
#ifdef KRB5_KDC_UNREACH
	    return KRB5_KDC_UNREACH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KEYTAB_BADVNO"))
#ifdef KRB5_KEYTAB_BADVNO
	    return KRB5_KEYTAB_BADVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_BADNAME"))
#ifdef KRB5_KT_BADNAME
	    return KRB5_KT_BADNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_END"))
#ifdef KRB5_KT_END
	    return KRB5_KT_END;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_IOERR"))
#ifdef KRB5_KT_IOERR
	    return KRB5_KT_IOERR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_KVNONOTFOUND"))
#ifdef KRB5_KT_KVNONOTFOUND
	    return KRB5_KT_KVNONOTFOUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_NAME_TOOLONG"))
#ifdef KRB5_KT_NAME_TOOLONG
	    return KRB5_KT_NAME_TOOLONG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_NOTFOUND"))
#ifdef KRB5_KT_NOTFOUND
	    return KRB5_KT_NOTFOUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_NOWRITE"))
#ifdef KRB5_KT_NOWRITE
	    return KRB5_KT_NOWRITE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_TYPE_EXISTS"))
#ifdef KRB5_KT_TYPE_EXISTS
	    return KRB5_KT_TYPE_EXISTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_KT_UNKNOWN_TYPE"))
#ifdef KRB5_KT_UNKNOWN_TYPE
	    return KRB5_KT_UNKNOWN_TYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LIBOS_BADLOCKFLAG"))
#ifdef KRB5_LIBOS_BADLOCKFLAG
	    return KRB5_LIBOS_BADLOCKFLAG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LIBOS_BADPWDMATCH"))
#ifdef KRB5_LIBOS_BADPWDMATCH
	    return KRB5_LIBOS_BADPWDMATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LIBOS_CANTREADPWD"))
#ifdef KRB5_LIBOS_CANTREADPWD
	    return KRB5_LIBOS_CANTREADPWD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LIBOS_PWDINTR"))
#ifdef KRB5_LIBOS_PWDINTR
	    return KRB5_LIBOS_PWDINTR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LNAME_BADFORMAT"))
#ifdef KRB5_LNAME_BADFORMAT
	    return KRB5_LNAME_BADFORMAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LNAME_CANTOPEN"))
#ifdef KRB5_LNAME_CANTOPEN
	    return KRB5_LNAME_CANTOPEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LNAME_NOTRANS"))
#ifdef KRB5_LNAME_NOTRANS
	    return KRB5_LNAME_NOTRANS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ALL_LAST_INITIAL"))
#ifdef KRB5_LRQ_ALL_LAST_INITIAL
	    return KRB5_LRQ_ALL_LAST_INITIAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ALL_LAST_RENEWAL"))
#ifdef KRB5_LRQ_ALL_LAST_RENEWAL
	    return KRB5_LRQ_ALL_LAST_RENEWAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ALL_LAST_REQ"))
#ifdef KRB5_LRQ_ALL_LAST_REQ
	    return KRB5_LRQ_ALL_LAST_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ALL_LAST_TGT"))
#ifdef KRB5_LRQ_ALL_LAST_TGT
	    return KRB5_LRQ_ALL_LAST_TGT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ALL_LAST_TGT_ISSUED"))
#ifdef KRB5_LRQ_ALL_LAST_TGT_ISSUED
	    return KRB5_LRQ_ALL_LAST_TGT_ISSUED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_NONE"))
#ifdef KRB5_LRQ_NONE
	    return KRB5_LRQ_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ONE_LAST_INITIAL"))
#ifdef KRB5_LRQ_ONE_LAST_INITIAL
	    return KRB5_LRQ_ONE_LAST_INITIAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ONE_LAST_RENEWAL"))
#ifdef KRB5_LRQ_ONE_LAST_RENEWAL
	    return KRB5_LRQ_ONE_LAST_RENEWAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ONE_LAST_REQ"))
#ifdef KRB5_LRQ_ONE_LAST_REQ
	    return KRB5_LRQ_ONE_LAST_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ONE_LAST_TGT"))
#ifdef KRB5_LRQ_ONE_LAST_TGT
	    return KRB5_LRQ_ONE_LAST_TGT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_LRQ_ONE_LAST_TGT_ISSUED"))
#ifdef KRB5_LRQ_ONE_LAST_TGT_ISSUED
	    return KRB5_LRQ_ONE_LAST_TGT_ISSUED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_MUTUAL_FAILED"))
#ifdef KRB5_MUTUAL_FAILED
	    return KRB5_MUTUAL_FAILED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NOCREDS_SUPPLIED"))
#ifdef KRB5_NOCREDS_SUPPLIED
	    return KRB5_NOCREDS_SUPPLIED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NO_2ND_TKT"))
#ifdef KRB5_NO_2ND_TKT
	    return KRB5_NO_2ND_TKT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NO_LOCALNAME"))
#ifdef KRB5_NO_LOCALNAME
	    return KRB5_NO_LOCALNAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NO_TKT_IN_RLM"))
#ifdef KRB5_NO_TKT_IN_RLM
	    return KRB5_NO_TKT_IN_RLM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NO_TKT_SUPPLIED"))
#ifdef KRB5_NO_TKT_SUPPLIED
	    return KRB5_NO_TKT_SUPPLIED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NT_PRINCIPAL"))
#ifdef KRB5_NT_PRINCIPAL
	    return KRB5_NT_PRINCIPAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NT_SRV_HST"))
#ifdef KRB5_NT_SRV_HST
	    return KRB5_NT_SRV_HST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NT_SRV_INST"))
#ifdef KRB5_NT_SRV_INST
	    return KRB5_NT_SRV_INST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NT_SRV_XHST"))
#ifdef KRB5_NT_SRV_XHST
	    return KRB5_NT_SRV_XHST;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NT_UID"))
#ifdef KRB5_NT_UID
	    return KRB5_NT_UID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_NT_UNKNOWN"))
#ifdef KRB5_NT_UNKNOWN
	    return KRB5_NT_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_AFS3_SALT"))
#ifdef KRB5_PADATA_AFS3_SALT
	    return KRB5_PADATA_AFS3_SALT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_AP_REQ"))
#ifdef KRB5_PADATA_AP_REQ
	    return KRB5_PADATA_AP_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_DASS"))
#ifdef KRB5_PADATA_DASS
	    return KRB5_PADATA_DASS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_ENC_ENCKEY"))
#ifdef KRB5_PADATA_ENC_ENCKEY
	    return KRB5_PADATA_ENC_ENCKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_ENC_SANDIA_SECURID"))
#ifdef KRB5_PADATA_ENC_SANDIA_SECURID
	    return KRB5_PADATA_ENC_SANDIA_SECURID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_ENC_TIMESTAMP"))
#ifdef KRB5_PADATA_ENC_TIMESTAMP
	    return KRB5_PADATA_ENC_TIMESTAMP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_ENC_UNIX_TIME"))
#ifdef KRB5_PADATA_ENC_UNIX_TIME
	    return KRB5_PADATA_ENC_UNIX_TIME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_ETYPE_INFO"))
#ifdef KRB5_PADATA_ETYPE_INFO
	    return KRB5_PADATA_ETYPE_INFO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_NONE"))
#ifdef KRB5_PADATA_NONE
	    return KRB5_PADATA_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_OSF_DCE"))
#ifdef KRB5_PADATA_OSF_DCE
	    return KRB5_PADATA_OSF_DCE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_PW_SALT"))
#ifdef KRB5_PADATA_PW_SALT
	    return KRB5_PADATA_PW_SALT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_SAM_CHALLENGE"))
#ifdef KRB5_PADATA_SAM_CHALLENGE
	    return KRB5_PADATA_SAM_CHALLENGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_SAM_RESPONSE"))
#ifdef KRB5_PADATA_SAM_RESPONSE
	    return KRB5_PADATA_SAM_RESPONSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_SESAME"))
#ifdef KRB5_PADATA_SESAME
	    return KRB5_PADATA_SESAME;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PADATA_TGS_REQ"))
#ifdef KRB5_PADATA_TGS_REQ
	    return KRB5_PADATA_TGS_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PARSE_ILLCHAR"))
#ifdef KRB5_PARSE_ILLCHAR
	    return KRB5_PARSE_ILLCHAR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PARSE_MALFORMED"))
#ifdef KRB5_PARSE_MALFORMED
	    return KRB5_PARSE_MALFORMED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PREAUTH_BAD_TYPE"))
#ifdef KRB5_PREAUTH_BAD_TYPE
	    return KRB5_PREAUTH_BAD_TYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PREAUTH_FAILED"))
#ifdef KRB5_PREAUTH_FAILED
	    return KRB5_PREAUTH_FAILED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PREAUTH_NO_KEY"))
#ifdef KRB5_PREAUTH_NO_KEY
	    return KRB5_PREAUTH_NO_KEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PRINC_NOMATCH"))
#ifdef KRB5_PRINC_NOMATCH
	    return KRB5_PRINC_NOMATCH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PRIV"))
#ifdef KRB5_PRIV
	    return KRB5_PRIV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PROG_ATYPE_NOSUPP"))
#ifdef KRB5_PROG_ATYPE_NOSUPP
	    return KRB5_PROG_ATYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PROG_ETYPE_NOSUPP"))
#ifdef KRB5_PROG_ETYPE_NOSUPP
	    return KRB5_PROG_ETYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PROG_KEYTYPE_NOSUPP"))
#ifdef KRB5_PROG_KEYTYPE_NOSUPP
	    return KRB5_PROG_KEYTYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PROG_SUMTYPE_NOSUPP"))
#ifdef KRB5_PROG_SUMTYPE_NOSUPP
	    return KRB5_PROG_SUMTYPE_NOSUPP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PROVIDE_PROTOTYPES"))
#ifdef KRB5_PROVIDE_PROTOTYPES
	    return KRB5_PROVIDE_PROTOTYPES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_PVNO"))
#ifdef KRB5_PVNO
	    return KRB5_PVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RCACHE_BADVNO"))
#ifdef KRB5_RCACHE_BADVNO
	    return KRB5_RCACHE_BADVNO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO"))
#ifdef KRB5_RC_IO
	    return KRB5_RC_IO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO_EOF"))
#ifdef KRB5_RC_IO_EOF
	    return KRB5_RC_IO_EOF;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO_IO"))
#ifdef KRB5_RC_IO_IO
	    return KRB5_RC_IO_IO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO_MALLOC"))
#ifdef KRB5_RC_IO_MALLOC
	    return KRB5_RC_IO_MALLOC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO_PERM"))
#ifdef KRB5_RC_IO_PERM
	    return KRB5_RC_IO_PERM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO_SPACE"))
#ifdef KRB5_RC_IO_SPACE
	    return KRB5_RC_IO_SPACE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_IO_UNKNOWN"))
#ifdef KRB5_RC_IO_UNKNOWN
	    return KRB5_RC_IO_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_MALLOC"))
#ifdef KRB5_RC_MALLOC
	    return KRB5_RC_MALLOC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_NOIO"))
#ifdef KRB5_RC_NOIO
	    return KRB5_RC_NOIO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_PARSE"))
#ifdef KRB5_RC_PARSE
	    return KRB5_RC_PARSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_REPLAY"))
#ifdef KRB5_RC_REPLAY
	    return KRB5_RC_REPLAY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_REQUIRED"))
#ifdef KRB5_RC_REQUIRED
	    return KRB5_RC_REQUIRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_TYPE_EXISTS"))
#ifdef KRB5_RC_TYPE_EXISTS
	    return KRB5_RC_TYPE_EXISTS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_TYPE_NOTFOUND"))
#ifdef KRB5_RC_TYPE_NOTFOUND
	    return KRB5_RC_TYPE_NOTFOUND;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RC_UNKNOWN"))
#ifdef KRB5_RC_UNKNOWN
	    return KRB5_RC_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_REALM_BRANCH_CHAR"))
#ifdef KRB5_REALM_BRANCH_CHAR
	    return KRB5_REALM_BRANCH_CHAR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_REALM_CANT_RESOLVE"))
#ifdef KRB5_REALM_CANT_RESOLVE
	    return KRB5_REALM_CANT_RESOLVE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_REALM_UNKNOWN"))
#ifdef KRB5_REALM_UNKNOWN
	    return KRB5_REALM_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RECVAUTH_BADAUTHVERS"))
#ifdef KRB5_RECVAUTH_BADAUTHVERS
	    return KRB5_RECVAUTH_BADAUTHVERS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_RECVAUTH_SKIP_VERSION"))
#ifdef KRB5_RECVAUTH_SKIP_VERSION
	    return KRB5_RECVAUTH_SKIP_VERSION;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SAFE"))
#ifdef KRB5_SAFE
	    return KRB5_SAFE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SAM_MUST_PK_ENCRYPT_SAD"))
#ifdef KRB5_SAM_MUST_PK_ENCRYPT_SAD
	    return KRB5_SAM_MUST_PK_ENCRYPT_SAD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SAM_SEND_ENCRYPTED_SAD"))
#ifdef KRB5_SAM_SEND_ENCRYPTED_SAD
	    return KRB5_SAM_SEND_ENCRYPTED_SAD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SAM_UNSUPPORTED"))
#ifdef KRB5_SAM_UNSUPPORTED
	    return KRB5_SAM_UNSUPPORTED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SAM_USE_SAD_AS_KEY"))
#ifdef KRB5_SAM_USE_SAD_AS_KEY
	    return KRB5_SAM_USE_SAD_AS_KEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SENDAUTH_BADAPPLVERS"))
#ifdef KRB5_SENDAUTH_BADAPPLVERS
	    return KRB5_SENDAUTH_BADAPPLVERS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SENDAUTH_BADAUTHVERS"))
#ifdef KRB5_SENDAUTH_BADAUTHVERS
	    return KRB5_SENDAUTH_BADAUTHVERS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SENDAUTH_BADRESPONSE"))
#ifdef KRB5_SENDAUTH_BADRESPONSE
	    return KRB5_SENDAUTH_BADRESPONSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SENDAUTH_REJECTED"))
#ifdef KRB5_SENDAUTH_REJECTED
	    return KRB5_SENDAUTH_REJECTED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SERVICE_UNKNOWN"))
#ifdef KRB5_SERVICE_UNKNOWN
	    return KRB5_SERVICE_UNKNOWN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SNAME_UNSUPP_NAMETYPE"))
#ifdef KRB5_SNAME_UNSUPP_NAMETYPE
	    return KRB5_SNAME_UNSUPP_NAMETYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_SYSTYPES__"))
#ifdef KRB5_SYSTYPES__
	    return KRB5_SYSTYPES__;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_2ND_TKT"))
#ifdef KRB5_TC_MATCH_2ND_TKT
	    return KRB5_TC_MATCH_2ND_TKT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_AUTHDATA"))
#ifdef KRB5_TC_MATCH_AUTHDATA
	    return KRB5_TC_MATCH_AUTHDATA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_FLAGS"))
#ifdef KRB5_TC_MATCH_FLAGS
	    return KRB5_TC_MATCH_FLAGS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_FLAGS_EXACT"))
#ifdef KRB5_TC_MATCH_FLAGS_EXACT
	    return KRB5_TC_MATCH_FLAGS_EXACT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_IS_SKEY"))
#ifdef KRB5_TC_MATCH_IS_SKEY
	    return KRB5_TC_MATCH_IS_SKEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_KTYPE"))
#ifdef KRB5_TC_MATCH_KTYPE
	    return KRB5_TC_MATCH_KTYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_SRV_NAMEONLY"))
#ifdef KRB5_TC_MATCH_SRV_NAMEONLY
	    return KRB5_TC_MATCH_SRV_NAMEONLY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_TIMES"))
#ifdef KRB5_TC_MATCH_TIMES
	    return KRB5_TC_MATCH_TIMES;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_MATCH_TIMES_EXACT"))
#ifdef KRB5_TC_MATCH_TIMES_EXACT
	    return KRB5_TC_MATCH_TIMES_EXACT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TC_OPENCLOSE"))
#ifdef KRB5_TC_OPENCLOSE
	    return KRB5_TC_OPENCLOSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TGS_NAME_SIZE"))
#ifdef KRB5_TGS_NAME_SIZE
	    return KRB5_TGS_NAME_SIZE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TGS_REP"))
#ifdef KRB5_TGS_REP
	    return KRB5_TGS_REP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TGS_REQ"))
#ifdef KRB5_TGS_REQ
	    return KRB5_TGS_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TKT_NOT_FORWARDABLE"))
#ifdef KRB5_TKT_NOT_FORWARDABLE
	    return KRB5_TKT_NOT_FORWARDABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TRANS_BADFORMAT"))
#ifdef KRB5_TRANS_BADFORMAT
	    return KRB5_TRANS_BADFORMAT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_TRANS_CANTOPEN"))
#ifdef KRB5_TRANS_CANTOPEN
	    return KRB5_TRANS_CANTOPEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KRB5_WRONG_ETYPE"))
#ifdef KRB5_WRONG_ETYPE
	    return KRB5_WRONG_ETYPE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ADDRESS"))
#ifdef KV5M_ADDRESS
	    return KV5M_ADDRESS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ALT_METHOD"))
#ifdef KV5M_ALT_METHOD
	    return KV5M_ALT_METHOD;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_AP_REP"))
#ifdef KV5M_AP_REP
	    return KV5M_AP_REP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_AP_REP_ENC_PART"))
#ifdef KV5M_AP_REP_ENC_PART
	    return KV5M_AP_REP_ENC_PART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_AP_REQ"))
#ifdef KV5M_AP_REQ
	    return KV5M_AP_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_AUTHDATA"))
#ifdef KV5M_AUTHDATA
	    return KV5M_AUTHDATA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_AUTHENTICATOR"))
#ifdef KV5M_AUTHENTICATOR
	    return KV5M_AUTHENTICATOR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_AUTH_CONTEXT"))
#ifdef KV5M_AUTH_CONTEXT
	    return KV5M_AUTH_CONTEXT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CCACHE"))
#ifdef KV5M_CCACHE
	    return KV5M_CCACHE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CHECKSUM"))
#ifdef KV5M_CHECKSUM
	    return KV5M_CHECKSUM;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CHECKSUM_ENTRY"))
#ifdef KV5M_CHECKSUM_ENTRY
	    return KV5M_CHECKSUM_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CONTEXT"))
#ifdef KV5M_CONTEXT
	    return KV5M_CONTEXT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CRED"))
#ifdef KV5M_CRED
	    return KV5M_CRED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CREDS"))
#ifdef KV5M_CREDS
	    return KV5M_CREDS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CRED_ENC_PART"))
#ifdef KV5M_CRED_ENC_PART
	    return KV5M_CRED_ENC_PART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CRED_INFO"))
#ifdef KV5M_CRED_INFO
	    return KV5M_CRED_INFO;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CRYPTOSYSTEM_ENTRY"))
#ifdef KV5M_CRYPTOSYSTEM_ENTRY
	    return KV5M_CRYPTOSYSTEM_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_CS_TABLE_ENTRY"))
#ifdef KV5M_CS_TABLE_ENTRY
	    return KV5M_CS_TABLE_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_DATA"))
#ifdef KV5M_DATA
	    return KV5M_DATA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_DB_CONTEXT"))
#ifdef KV5M_DB_CONTEXT
	    return KV5M_DB_CONTEXT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ENCRYPT_BLOCK"))
#ifdef KV5M_ENCRYPT_BLOCK
	    return KV5M_ENCRYPT_BLOCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ENC_DATA"))
#ifdef KV5M_ENC_DATA
	    return KV5M_ENC_DATA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ENC_KDC_REP_PART"))
#ifdef KV5M_ENC_KDC_REP_PART
	    return KV5M_ENC_KDC_REP_PART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ENC_SAM_RESPONSE_ENC"))
#ifdef KV5M_ENC_SAM_RESPONSE_ENC
	    return KV5M_ENC_SAM_RESPONSE_ENC;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ENC_TKT_PART"))
#ifdef KV5M_ENC_TKT_PART
	    return KV5M_ENC_TKT_PART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ERROR"))
#ifdef KV5M_ERROR
	    return KV5M_ERROR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_ETYPE_INFO_ENTRY"))
#ifdef KV5M_ETYPE_INFO_ENTRY
	    return KV5M_ETYPE_INFO_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_GSS_OID"))
#ifdef KV5M_GSS_OID
	    return KV5M_GSS_OID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_GSS_QUEUE"))
#ifdef KV5M_GSS_QUEUE
	    return KV5M_GSS_QUEUE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_KDC_REP"))
#ifdef KV5M_KDC_REP
	    return KV5M_KDC_REP;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_KDC_REQ"))
#ifdef KV5M_KDC_REQ
	    return KV5M_KDC_REQ;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_KEYBLOCK"))
#ifdef KV5M_KEYBLOCK
	    return KV5M_KEYBLOCK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_KEYTAB"))
#ifdef KV5M_KEYTAB
	    return KV5M_KEYTAB;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_KEYTAB_ENTRY"))
#ifdef KV5M_KEYTAB_ENTRY
	    return KV5M_KEYTAB_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_LAST_REQ_ENTRY"))
#ifdef KV5M_LAST_REQ_ENTRY
	    return KV5M_LAST_REQ_ENTRY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_NONE"))
#ifdef KV5M_NONE
	    return KV5M_NONE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_OS_CONTEXT"))
#ifdef KV5M_OS_CONTEXT
	    return KV5M_OS_CONTEXT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PASSWD_PHRASE_ELEMENT"))
#ifdef KV5M_PASSWD_PHRASE_ELEMENT
	    return KV5M_PASSWD_PHRASE_ELEMENT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PA_DATA"))
#ifdef KV5M_PA_DATA
	    return KV5M_PA_DATA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PREAUTH_OPS"))
#ifdef KV5M_PREAUTH_OPS
	    return KV5M_PREAUTH_OPS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PREDICTED_SAM_RESPONSE"))
#ifdef KV5M_PREDICTED_SAM_RESPONSE
	    return KV5M_PREDICTED_SAM_RESPONSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PRINCIPAL"))
#ifdef KV5M_PRINCIPAL
	    return KV5M_PRINCIPAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PRIV"))
#ifdef KV5M_PRIV
	    return KV5M_PRIV;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PRIV_ENC_PART"))
#ifdef KV5M_PRIV_ENC_PART
	    return KV5M_PRIV_ENC_PART;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_PWD_DATA"))
#ifdef KV5M_PWD_DATA
	    return KV5M_PWD_DATA;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_RCACHE"))
#ifdef KV5M_RCACHE
	    return KV5M_RCACHE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_RESPONSE"))
#ifdef KV5M_RESPONSE
	    return KV5M_RESPONSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_SAFE"))
#ifdef KV5M_SAFE
	    return KV5M_SAFE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_SAM_CHALLENGE"))
#ifdef KV5M_SAM_CHALLENGE
	    return KV5M_SAM_CHALLENGE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_SAM_KEY"))
#ifdef KV5M_SAM_KEY
	    return KV5M_SAM_KEY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_SAM_RESPONSE"))
#ifdef KV5M_SAM_RESPONSE
	    return KV5M_SAM_RESPONSE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_TICKET"))
#ifdef KV5M_TICKET
	    return KV5M_TICKET;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_TKT_AUTHENT"))
#ifdef KV5M_TKT_AUTHENT
	    return KV5M_TKT_AUTHENT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "KV5M_TRANSITED"))
#ifdef KV5M_TRANSITED
	    return KV5M_TRANSITED;
#else
	    goto not_there;
#endif
	break;
    case 'L':
	if (strEQ(name, "LR_TYPE_INTERPRETATION_MASK"))
#ifdef LR_TYPE_INTERPRETATION_MASK
	    return LR_TYPE_INTERPRETATION_MASK;
#else
	    goto not_there;
#endif
	if (strEQ(name, "LR_TYPE_THIS_SERVER_ONLY"))
#ifdef LR_TYPE_THIS_SERVER_ONLY
	    return LR_TYPE_THIS_SERVER_ONLY;
#else
	    goto not_there;
#endif
	break;
    case 'M':
	if (strEQ(name, "MAX_KEYTAB_NAME_LEN"))
#ifdef MAX_KEYTAB_NAME_LEN
	    return MAX_KEYTAB_NAME_LEN;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MSEC_DIRBIT"))
#ifdef MSEC_DIRBIT
	    return MSEC_DIRBIT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "MSEC_VAL_MASK"))
#ifdef MSEC_VAL_MASK
	    return MSEC_VAL_MASK;
#else
	    goto not_there;
#endif
	break;
    case 'N':
	if (strEQ(name, "NEAR"))
#ifdef NEAR
	    return NEAR;
#else
	    goto not_there;
#endif
	if (strEQ(name, "NO_PASSWORD"))
#ifdef NO_PASSWORD
	    return NO_PASSWORD;
#else
	    goto not_there;
#endif
	break;
    case 'O':
	break;
    case 'P':
	break;
    case 'Q':
	break;
    case 'R':
	break;
    case 'S':
	if (strEQ(name, "SIZEOF_INT"))
#ifdef SIZEOF_INT
	    return SIZEOF_INT;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SIZEOF_LONG"))
#ifdef SIZEOF_LONG
	    return SIZEOF_LONG;
#else
	    goto not_there;
#endif
	if (strEQ(name, "SIZEOF_SHORT"))
#ifdef SIZEOF_SHORT
	    return SIZEOF_SHORT;
#else
	    goto not_there;
#endif
	break;
    case 'T':
	if (strEQ(name, "TKT_FLG_FORWARDABLE"))
#ifdef TKT_FLG_FORWARDABLE
	    return TKT_FLG_FORWARDABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_FORWARDED"))
#ifdef TKT_FLG_FORWARDED
	    return TKT_FLG_FORWARDED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_HW_AUTH"))
#ifdef TKT_FLG_HW_AUTH
	    return TKT_FLG_HW_AUTH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_INITIAL"))
#ifdef TKT_FLG_INITIAL
	    return TKT_FLG_INITIAL;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_INVALID"))
#ifdef TKT_FLG_INVALID
	    return TKT_FLG_INVALID;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_MAY_POSTDATE"))
#ifdef TKT_FLG_MAY_POSTDATE
	    return TKT_FLG_MAY_POSTDATE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_POSTDATED"))
#ifdef TKT_FLG_POSTDATED
	    return TKT_FLG_POSTDATED;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_PRE_AUTH"))
#ifdef TKT_FLG_PRE_AUTH
	    return TKT_FLG_PRE_AUTH;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_PROXIABLE"))
#ifdef TKT_FLG_PROXIABLE
	    return TKT_FLG_PROXIABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_PROXY"))
#ifdef TKT_FLG_PROXY
	    return TKT_FLG_PROXY;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TKT_FLG_RENEWABLE"))
#ifdef TKT_FLG_RENEWABLE
	    return TKT_FLG_RENEWABLE;
#else
	    goto not_there;
#endif
	if (strEQ(name, "TRUE"))
#ifdef TRUE
	    return TRUE;
#else
	    goto not_there;
#endif
	break;
    case 'U':
	break;
    case 'V':
	if (strEQ(name, "VALID_INT_BITS"))
#ifdef VALID_INT_BITS
	    return VALID_INT_BITS;
#else
	    goto not_there;
#endif
	if (strEQ(name, "VALID_UINT_BITS"))
#ifdef VALID_UINT_BITS
	    return VALID_UINT_BITS;
#else
	    goto not_there;
#endif
	break;
    case 'W':
	break;
    case 'X':
	break;
    case 'Y':
	break;
    case 'Z':
	break;
    case 'a':
	break;
    case 'b':
	break;
    case 'c':
	break;
    case 'd':
	break;
    case 'e':
	break;
    case 'f':
	break;
    case 'g':
	break;
    case 'h':
	break;
    case 'i':
	break;
    case 'j':
	break;
    case 'k':
	break;
    case 'l':
	break;
    case 'm':
	break;
    case 'n':
	break;
    case 'o':
	break;
    case 'p':
	break;
    case 'q':
	break;
    case 'r':
	break;
    case 's':
	break;
    case 't':
	break;
    case 'u':
	break;
    case 'v':
	break;
    case 'w':
	break;
    case 'x':
	break;
    case 'y':
	break;
    case 'z':
	break;
    case '_':
	break;
    }
    errno = EINVAL;
    return 0;

not_there:
    errno = ENOENT;
    return 0;
}
