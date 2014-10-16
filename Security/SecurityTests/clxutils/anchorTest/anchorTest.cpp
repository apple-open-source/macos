/*
 * anchorTest.cpp - test cert encode/decode using known good system
 *                  anchors
 */
#include <stdio.h>
#include <string.h>
#include <Security/cssm.h>
#include <Security/x509defs.h>
#include <Security/oidsattr.h>
#include <Security/oidscert.h>
#include <Security/certextensions.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettingsPriv.h>
#include <security_cdsa_utils/cuOidParser.h>
#include <security_cdsa_utils/cuPrintCert.h>
#include <utilLib/common.h>
#include <utilLib/cspwrap.h>
#include <security_cdsa_utils/cuFileIo.h>
#include <clAppUtils/clutils.h>
#include <clAppUtils/certVerify.h>
#include <clAppUtils/tpUtils.h>
#include <Security/SecAsn1Coder.h>
#include <Security/X509Templates.h>

#define ENC_TBS_BLOB	"encodedTbs.der"
#define DEC_TBS_BLOB	"decodedTbs.der"

static void usage(char **argv)
{
	printf("Usage: %s [options]\n", argv[0]);
	printf("Options:\n");
	printf("  w    -- writeBlobs\n");
	printf("  e    -- allow expired roots\n");
	printf("  t    -- use Trust Settings\n");
	printf("  q    -- quiet\n");
	printf("  v    -- verbose\n");
	exit(1);
}

/*
 * Certs for which we skip the "compare TBS blob" test, enumerated by 
 * DER-encoded issuer name.
 *
 * Get this formatted data from the extractCertFields program. 
 *
 * All of these have non-standard KeyUsage encoding (legal but it's
 * not the same as ours or everyone else's).
 */
/*
   Country         : HU
   Locality        : Budapest
   Org             : NetLock Halozatbiztonsagi Kft.
   OrgUnit         : Tanusitvanykiadok
   Common Name     : NetLock Expressz (Class C) Tanusitvanykiado 
 */
static const uint8 anchor_46_derIssuer_bytes[] = {
   0x30,  0x81,  0x9b,  0x31,  0x0b,  0x30,  0x09,  0x06,  
   0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x48,  0x55,  
   0x31,  0x11,  0x30,  0x0f,  0x06,  0x03,  0x55,  0x04,  
   0x07,  0x13,  0x08,  0x42,  0x75,  0x64,  0x61,  0x70,  
   0x65,  0x73,  0x74,  0x31,  0x27,  0x30,  0x25,  0x06,  
   0x03,  0x55,  0x04,  0x0a,  0x13,  0x1e,  0x4e,  0x65,  
   0x74,  0x4c,  0x6f,  0x63,  0x6b,  0x20,  0x48,  0x61,  
   0x6c,  0x6f,  0x7a,  0x61,  0x74,  0x62,  0x69,  0x7a,  
   0x74,  0x6f,  0x6e,  0x73,  0x61,  0x67,  0x69,  0x20,  
   0x4b,  0x66,  0x74,  0x2e,  0x31,  0x1a,  0x30,  0x18,  
   0x06,  0x03,  0x55,  0x04,  0x0b,  0x13,  0x11,  0x54,  
   0x61,  0x6e,  0x75,  0x73,  0x69,  0x74,  0x76,  0x61,  
   0x6e,  0x79,  0x6b,  0x69,  0x61,  0x64,  0x6f,  0x6b,  
   0x31,  0x34,  0x30,  0x32,  0x06,  0x03,  0x55,  0x04,  
   0x03,  0x13,  0x2b,  0x4e,  0x65,  0x74,  0x4c,  0x6f,  
   0x63,  0x6b,  0x20,  0x45,  0x78,  0x70,  0x72,  0x65,  
   0x73,  0x73,  0x7a,  0x20,  0x28,  0x43,  0x6c,  0x61,  
   0x73,  0x73,  0x20,  0x43,  0x29,  0x20,  0x54,  0x61,  
   0x6e,  0x75,  0x73,  0x69,  0x74,  0x76,  0x61,  0x6e,  
   0x79,  0x6b,  0x69,  0x61,  0x64,  0x6f
};
static const CSSM_DATA anchor_46_derIssuer = { 158, (uint8 *)anchor_46_derIssuer_bytes };

/*
   Country         : HU
   State           : Hungary
   Locality        : Budapest
   Org             : NetLock Halozatbiztonsagi Kft.
   OrgUnit         : Tanusitvanykiadok
   Common Name     : NetLock Kozjegyzoi (Class A) Tanusitvanykiado
*/
static const uint8 anchor_53_derIssuer_bytes[] = {
   0x30,  0x81,  0xaf,  0x31,  0x0b,  0x30,  0x09,  0x06,  
   0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x48,  0x55,  
   0x31,  0x10,  0x30,  0x0e,  0x06,  0x03,  0x55,  0x04,  
   0x08,  0x13,  0x07,  0x48,  0x75,  0x6e,  0x67,  0x61,  
   0x72,  0x79,  0x31,  0x11,  0x30,  0x0f,  0x06,  0x03,  
   0x55,  0x04,  0x07,  0x13,  0x08,  0x42,  0x75,  0x64,  
   0x61,  0x70,  0x65,  0x73,  0x74,  0x31,  0x27,  0x30,  
   0x25,  0x06,  0x03,  0x55,  0x04,  0x0a,  0x13,  0x1e,  
   0x4e,  0x65,  0x74,  0x4c,  0x6f,  0x63,  0x6b,  0x20,  
   0x48,  0x61,  0x6c,  0x6f,  0x7a,  0x61,  0x74,  0x62,  
   0x69,  0x7a,  0x74,  0x6f,  0x6e,  0x73,  0x61,  0x67,  
   0x69,  0x20,  0x4b,  0x66,  0x74,  0x2e,  0x31,  0x1a,  
   0x30,  0x18,  0x06,  0x03,  0x55,  0x04,  0x0b,  0x13,  
   0x11,  0x54,  0x61,  0x6e,  0x75,  0x73,  0x69,  0x74,  
   0x76,  0x61,  0x6e,  0x79,  0x6b,  0x69,  0x61,  0x64,  
   0x6f,  0x6b,  0x31,  0x36,  0x30,  0x34,  0x06,  0x03,  
   0x55,  0x04,  0x03,  0x13,  0x2d,  0x4e,  0x65,  0x74,  
   0x4c,  0x6f,  0x63,  0x6b,  0x20,  0x4b,  0x6f,  0x7a,  
   0x6a,  0x65,  0x67,  0x79,  0x7a,  0x6f,  0x69,  0x20,  
   0x28,  0x43,  0x6c,  0x61,  0x73,  0x73,  0x20,  0x41,  
   0x29,  0x20,  0x54,  0x61,  0x6e,  0x75,  0x73,  0x69,  
   0x74,  0x76,  0x61,  0x6e,  0x79,  0x6b,  0x69,  0x61,  
   0x64,  0x6f
};
static const CSSM_DATA anchor_53_derIssuer = { 178, (uint8 *)anchor_53_derIssuer_bytes };

/*
   Country         : HU
   Locality        : Budapest
   Org             : NetLock Halozatbiztonsagi Kft.
   OrgUnit         : Tanusitvanykiadok
   Common Name     : NetLock Uzleti (Class B) Tanusitvanykiado
*/
static const uint8 anchor_60_derIssuer_bytes[] = {
   0x30,  0x81,  0x99,  0x31,  0x0b,  0x30,  0x09,  0x06,  
   0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x48,  0x55,  
   0x31,  0x11,  0x30,  0x0f,  0x06,  0x03,  0x55,  0x04,  
   0x07,  0x13,  0x08,  0x42,  0x75,  0x64,  0x61,  0x70,  
   0x65,  0x73,  0x74,  0x31,  0x27,  0x30,  0x25,  0x06,  
   0x03,  0x55,  0x04,  0x0a,  0x13,  0x1e,  0x4e,  0x65,  
   0x74,  0x4c,  0x6f,  0x63,  0x6b,  0x20,  0x48,  0x61,  
   0x6c,  0x6f,  0x7a,  0x61,  0x74,  0x62,  0x69,  0x7a,  
   0x74,  0x6f,  0x6e,  0x73,  0x61,  0x67,  0x69,  0x20,  
   0x4b,  0x66,  0x74,  0x2e,  0x31,  0x1a,  0x30,  0x18,  
   0x06,  0x03,  0x55,  0x04,  0x0b,  0x13,  0x11,  0x54,  
   0x61,  0x6e,  0x75,  0x73,  0x69,  0x74,  0x76,  0x61,  
   0x6e,  0x79,  0x6b,  0x69,  0x61,  0x64,  0x6f,  0x6b,  
   0x31,  0x32,  0x30,  0x30,  0x06,  0x03,  0x55,  0x04,  
   0x03,  0x13,  0x29,  0x4e,  0x65,  0x74,  0x4c,  0x6f,  
   0x63,  0x6b,  0x20,  0x55,  0x7a,  0x6c,  0x65,  0x74,  
   0x69,  0x20,  0x28,  0x43,  0x6c,  0x61,  0x73,  0x73,  
   0x20,  0x42,  0x29,  0x20,  0x54,  0x61,  0x6e,  0x75,  
   0x73,  0x69,  0x74,  0x76,  0x61,  0x6e,  0x79,  0x6b,  
   0x69,  0x61,  0x64,  0x6f
};
static const CSSM_DATA anchor_60_derIssuer = { 156, (uint8 *)anchor_60_derIssuer_bytes };

/*
 Country         : TR
 Locality        : Ankara
 Org             : (c) 2005 TÜRKTRUST Bilgi İletişim ve Bilişim Güvenliği Hizmetleri A.Ş.
 Common Name     : TÜRKTRUST Elektronik Sertifika Hizmet Sağlayıcısı
 Serial Number   : 01
 Not Before      : 10:27:17 May 13, 2005
 Not After       : 10:27:17 Mar 22, 2015
*/
static const uint8 turk1_derIssuer_bytes[] = {
	0x30,  0x81,  0xb7,  0x31,  0x3f,  0x30,  0x3d,  0x06,  
	0x03,  0x55,  0x04,  0x03,  0x0c,  0x36,  0x54,  0xc3,  
	0x9c,  0x52,  0x4b,  0x54,  0x52,  0x55,  0x53,  0x54,  
	0x20,  0x45,  0x6c,  0x65,  0x6b,  0x74,  0x72,  0x6f,  
	0x6e,  0x69,  0x6b,  0x20,  0x53,  0x65,  0x72,  0x74,  
	0x69,  0x66,  0x69,  0x6b,  0x61,  0x20,  0x48,  0x69,  
	0x7a,  0x6d,  0x65,  0x74,  0x20,  0x53,  0x61,  0xc4,  
	0x9f,  0x6c,  0x61,  0x79,  0xc4,  0xb1,  0x63,  0xc4,  
	0xb1,  0x73,  0xc4,  0xb1,  0x31,  0x0b,  0x30,  0x09,  
	0x06,  0x03,  0x55,  0x04,  0x06,  0x0c,  0x02,  0x54,  
	0x52,  0x31,  0x0f,  0x30,  0x0d,  0x06,  0x03,  0x55,  
	0x04,  0x07,  0x0c,  0x06,  0x41,  0x4e,  0x4b,  0x41,  
	0x52,  0x41,  0x31,  0x56,  0x30,  0x54,  0x06,  0x03,  
	0x55,  0x04,  0x0a,  0x0c,  0x4d,  0x28,  0x63,  0x29,  
	0x20,  0x32,  0x30,  0x30,  0x35,  0x20,  0x54,  0xc3,  
	0x9c,  0x52,  0x4b,  0x54,  0x52,  0x55,  0x53,  0x54,  
	0x20,  0x42,  0x69,  0x6c,  0x67,  0x69,  0x20,  0xc4,  
	0xb0,  0x6c,  0x65,  0x74,  0x69,  0xc5,  0x9f,  0x69,  
	0x6d,  0x20,  0x76,  0x65,  0x20,  0x42,  0x69,  0x6c,  
	0x69,  0xc5,  0x9f,  0x69,  0x6d,  0x20,  0x47,  0xc3,  
	0xbc,  0x76,  0x65,  0x6e,  0x6c,  0x69,  0xc4,  0x9f,  
	0x69,  0x20,  0x48,  0x69,  0x7a,  0x6d,  0x65,  0x74,  
	0x6c,  0x65,  0x72,  0x69,  0x20,  0x41,  0x2e,  0xc5,  
	0x9e,  0x2e
};
static const CSSM_DATA turk1_derIssuer = { 186, (uint8 *)turk1_derIssuer_bytes };

/*
 Country         : TR
 Locality        : Ankara
 Org             : TÜRKTRUST Bilgi İletişim ve Bilişim Güvenliği Hizmetleri A.Ş. (c) Kasım 2005
 Common Name     : TÜRKTRUST Elektronik Sertifika Hizmet Sağlayıcısı 
 Serial Number   : 01
 Not Before      : 10:07:57 Nov 7, 2005
 Not After       : 10:07:57 Sep 16, 2015
*/
static const uint8 turk2_derIssuer_bytes[] = {
	0x30,  0x81,  0xbe,  0x31,  0x3f,  0x30,  0x3d,  0x06,  
	0x03,  0x55,  0x04,  0x03,  0x0c,  0x36,  0x54,  0xc3,  
	0x9c,  0x52,  0x4b,  0x54,  0x52,  0x55,  0x53,  0x54,  
	0x20,  0x45,  0x6c,  0x65,  0x6b,  0x74,  0x72,  0x6f,  
	0x6e,  0x69,  0x6b,  0x20,  0x53,  0x65,  0x72,  0x74,  
	0x69,  0x66,  0x69,  0x6b,  0x61,  0x20,  0x48,  0x69,  
	0x7a,  0x6d,  0x65,  0x74,  0x20,  0x53,  0x61,  0xc4,  
	0x9f,  0x6c,  0x61,  0x79,  0xc4,  0xb1,  0x63,  0xc4,  
	0xb1,  0x73,  0xc4,  0xb1,  0x31,  0x0b,  0x30,  0x09,  
	0x06,  0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x54,  
	0x52,  0x31,  0x0f,  0x30,  0x0d,  0x06,  0x03,  0x55,  
	0x04,  0x07,  0x0c,  0x06,  0x41,  0x6e,  0x6b,  0x61,  
	0x72,  0x61,  0x31,  0x5d,  0x30,  0x5b,  0x06,  0x03,  
	0x55,  0x04,  0x0a,  0x0c,  0x54,  0x54,  0xc3,  0x9c,  
	0x52,  0x4b,  0x54,  0x52,  0x55,  0x53,  0x54,  0x20,  
	0x42,  0x69,  0x6c,  0x67,  0x69,  0x20,  0xc4,  0xb0,  
	0x6c,  0x65,  0x74,  0x69,  0xc5,  0x9f,  0x69,  0x6d,  
	0x20,  0x76,  0x65,  0x20,  0x42,  0x69,  0x6c,  0x69,  
	0xc5,  0x9f,  0x69,  0x6d,  0x20,  0x47,  0xc3,  0xbc,  
	0x76,  0x65,  0x6e,  0x6c,  0x69,  0xc4,  0x9f,  0x69,  
	0x20,  0x48,  0x69,  0x7a,  0x6d,  0x65,  0x74,  0x6c,  
	0x65,  0x72,  0x69,  0x20,  0x41,  0x2e,  0xc5,  0x9e,  
	0x2e,  0x20,  0x28,  0x63,  0x29,  0x20,  0x4b,  0x61,  
	0x73,  0xc4,  0xb1,  0x6d,  0x20,  0x32,  0x30,  0x30,  
	0x35
};
static const CSSM_DATA turk2_derIssuer = { 193, (uint8 *)turk2_derIssuer_bytes };

/*
 Country         : TR
 Locality        : Ankara
 Org             : TÜRKTRUST Bilgi İletişim ve Bilişim Güvenliği Hizmetleri A.Ş. (c) Aralık 2007
 Common Name     : TÜRKTRUST Elektronik Sertifika Hizmet Sağlayıcısı 
 Serial Number   : 01
 Not Before      : 18:37:19 Dec 25, 2007
 Not After       : 18:37:19 Dec 22, 2017
*/
static const uint8 turk3_derIssuer_bytes[] = {
	0x30,  0x81,  0xbf,  0x31,  0x3f,  0x30,  0x3d,  0x06,  
	0x03,  0x55,  0x04,  0x03,  0x0c,  0x36,  0x54,  0xc3,  
	0x9c,  0x52,  0x4b,  0x54,  0x52,  0x55,  0x53,  0x54,  
	0x20,  0x45,  0x6c,  0x65,  0x6b,  0x74,  0x72,  0x6f,  
	0x6e,  0x69,  0x6b,  0x20,  0x53,  0x65,  0x72,  0x74,  
	0x69,  0x66,  0x69,  0x6b,  0x61,  0x20,  0x48,  0x69,  
	0x7a,  0x6d,  0x65,  0x74,  0x20,  0x53,  0x61,  0xc4,  
	0x9f,  0x6c,  0x61,  0x79,  0xc4,  0xb1,  0x63,  0xc4,  
	0xb1,  0x73,  0xc4,  0xb1,  0x31,  0x0b,  0x30,  0x09,  
	0x06,  0x03,  0x55,  0x04,  0x06,  0x13,  0x02,  0x54,  
	0x52,  0x31,  0x0f,  0x30,  0x0d,  0x06,  0x03,  0x55,  
	0x04,  0x07,  0x0c,  0x06,  0x41,  0x6e,  0x6b,  0x61,  
	0x72,  0x61,  0x31,  0x5e,  0x30,  0x5c,  0x06,  0x03,  
	0x55,  0x04,  0x0a,  0x0c,  0x55,  0x54,  0xc3,  0x9c,  
	0x52,  0x4b,  0x54,  0x52,  0x55,  0x53,  0x54,  0x20,  
	0x42,  0x69,  0x6c,  0x67,  0x69,  0x20,  0xc4,  0xb0,  
	0x6c,  0x65,  0x74,  0x69,  0xc5,  0x9f,  0x69,  0x6d,  
	0x20,  0x76,  0x65,  0x20,  0x42,  0x69,  0x6c,  0x69,  
	0xc5,  0x9f,  0x69,  0x6d,  0x20,  0x47,  0xc3,  0xbc,  
	0x76,  0x65,  0x6e,  0x6c,  0x69,  0xc4,  0x9f,  0x69,  
	0x20,  0x48,  0x69,  0x7a,  0x6d,  0x65,  0x74,  0x6c,  
	0x65,  0x72,  0x69,  0x20,  0x41,  0x2e,  0xc5,  0x9e,  
	0x2e,  0x20,  0x28,  0x63,  0x29,  0x20,  0x41,  0x72,  
	0x61,  0x6c,  0xc4,  0xb1,  0x6b,  0x20,  0x32,  0x30,  
	0x30,  0x37
};
static const CSSM_DATA turk3_derIssuer = { 194, (uint8 *)turk3_derIssuer_bytes };

/*
Cert File Name: globalSignRoot.cer
   Country         : BE
   Org             : GlobalSign nv-sa
   OrgUnit         : Root CA
   Common Name     : GlobalSign Root CA
*/
static const uint8 globalSignRoot_derIssuer_bytes[] = {
   0x30,  0x57,  0x31,  0x0b,  0x30,  0x09,  0x06,  0x03,  
   0x55,  0x04,  0x06,  0x13,  0x02,  0x42,  0x45,  0x31,  
   0x19,  0x30,  0x17,  0x06,  0x03,  0x55,  0x04,  0x0a,  
   0x13,  0x10,  0x47,  0x6c,  0x6f,  0x62,  0x61,  0x6c,  
   0x53,  0x69,  0x67,  0x6e,  0x20,  0x6e,  0x76,  0x2d,  
   0x73,  0x61,  0x31,  0x10,  0x30,  0x0e,  0x06,  0x03,  
   0x55,  0x04,  0x0b,  0x13,  0x07,  0x52,  0x6f,  0x6f,  
   0x74,  0x20,  0x43,  0x41,  0x31,  0x1b,  0x30,  0x19,  
   0x06,  0x03,  0x55,  0x04,  0x03,  0x13,  0x12,  0x47,  
   0x6c,  0x6f,  0x62,  0x61,  0x6c,  0x53,  0x69,  0x67,  
   0x6e,  0x20,  0x52,  0x6f,  0x6f,  0x74,  0x20,  0x43,  
   0x41
};
static const CSSM_DATA globalSignRoot_derIssuer = { 89, (uint8 *)globalSignRoot_derIssuer_bytes };

/***********************
Cert File Name: swisssign.der
Subject Name       :
   Country         : CH
   Org             : SwissSign
   Common Name     : SwissSign CA (RSA IK May 6 1999 18:00:58)
   Email addrs     : ca@SwissSign.com
   
 This one has a bogus AuthorityKeyId, with a value of {0x30, 0} inside the octet string. 
 ***********************/
static const uint8 swisssign_derIssuer_bytes[] = {
   0x30,  0x76,  0x31,  0x0b,  0x30,  0x09,  0x06,  0x03,  
   0x55,  0x04,  0x06,  0x13,  0x02,  0x43,  0x48,  0x31,  
   0x12,  0x30,  0x10,  0x06,  0x03,  0x55,  0x04,  0x0a,  
   0x13,  0x09,  0x53,  0x77,  0x69,  0x73,  0x73,  0x53,  
   0x69,  0x67,  0x6e,  0x31,  0x32,  0x30,  0x30,  0x06,  
   0x03,  0x55,  0x04,  0x03,  0x13,  0x29,  0x53,  0x77,  
   0x69,  0x73,  0x73,  0x53,  0x69,  0x67,  0x6e,  0x20,  
   0x43,  0x41,  0x20,  0x28,  0x52,  0x53,  0x41,  0x20,  
   0x49,  0x4b,  0x20,  0x4d,  0x61,  0x79,  0x20,  0x36,  
   0x20,  0x31,  0x39,  0x39,  0x39,  0x20,  0x31,  0x38,  
   0x3a,  0x30,  0x30,  0x3a,  0x35,  0x38,  0x29,  0x31,  
   0x1f,  0x30,  0x1d,  0x06,  0x09,  0x2a,  0x86,  0x48,  
   0x86,  0xf7,  0x0d,  0x01,  0x09,  0x01,  0x16,  0x10,  
   0x63,  0x61,  0x40,  0x53,  0x77,  0x69,  0x73,  0x73,  
   0x53,  0x69,  0x67,  0x6e,  0x2e,  0x63,  0x6f,  0x6d
   
};
static const CSSM_DATA swisssign_derIssuer = { 120, (uint8 *)swisssign_derIssuer_bytes };

/*
 * Simple class to hold arrays of fields.
 */
class FieldArray {
public:
	/*
	 * Create from existing field array obtained from
	 * CSSM_CL_CertGetAllFields(). We'll do the CSSM_CL_FreeFields()
	 * in our destructor.
	 */
	FieldArray(
		CSSM_FIELD 		*fields, 
		uint32 			numFields,
		CSSM_CL_HANDLE 	clHand);
	
	/*
	 * Create empty array of specified size. We don't own the fields 
	 * themselves.
	 */
	FieldArray(
		uint32 			size);
	
	~FieldArray();
	
	/*
	 * Append a field - no realloc!
	 */
	void appendField(CSSM_FIELD &field);
	
	/* get specified field */
	CSSM_FIELD &fieldAt(uint32 index);
	
	/* get nth occurence of field matching specified OID */
	int  fieldForOid(
		const CSSM_OID	&oid,
		unsigned		n,			// n == 0 --> first one
		CSSM_FIELD		*&found);	// RETURNED
		
	CSSM_FIELD	*mFields;
	uint32		mNumFields;		// sizeof of *fields
	uint32		mMallocdSize;	// if NULL, read-only
	CSSM_CL_HANDLE	mClHand;
};

FieldArray::FieldArray(
	CSSM_FIELD *fields, 
	uint32 numFields,
	CSSM_CL_HANDLE clHand)
{
	mFields = fields;
	mNumFields = numFields;
	mMallocdSize = 0;
	mClHand = clHand;
}

FieldArray::FieldArray(
	uint32 size)
{
	unsigned len = sizeof(CSSM_FIELD) * size;
	mFields = (CSSM_FIELD_PTR)malloc(len);
	memset(mFields, 0, len);
	mNumFields = 0;
	mMallocdSize = size;
	mClHand = 0;
}

FieldArray::~FieldArray()
{
	if(mMallocdSize != 0) {
		/* 
		 * Just free the array of fields we mallocd, not the fields 
		 * themselves
		 */
		free(mFields);
	}
	else {
		/* The CL mallocd these fields, tell it to free the whole thing */
		CSSM_RETURN crtn = CSSM_CL_FreeFields(mClHand, 
			mNumFields, &mFields);
		if(crtn) {
			printError("CSSM_CL_FreeFields", crtn);
		}
	}
	mFields = NULL;
	mNumFields = 0;
	mMallocdSize = 0;
}

void FieldArray::appendField(
	CSSM_FIELD &field)
{
	if(mMallocdSize == 0) {
		printf("***Attempt to append to a read-only FieldArray\n");
		exit(1);
	}
	if(mNumFields >= mMallocdSize) {
		printf("***Attempt to append past present size of FieldArray\n");
		exit(1);
	}
	mFields[mNumFields] = field;
	mNumFields++;
}

CSSM_FIELD &FieldArray::fieldAt(
	uint32 index)
{
	if(index >= mNumFields) {
		printf("***Attempt to access past present size of FieldArray\n");
		exit(1);
	}
	return mFields[index];
}

/* get nth occurence of field matching specified OID */
/* returns nonzero on error */
int FieldArray::fieldForOid(
	const CSSM_OID	&oid,
	unsigned		n,			// n == 0 --> first one
	CSSM_FIELD		*&found)	// RETURNED
{
	unsigned foundDex = 0;
	for(unsigned dex=0; dex<mNumFields; dex++) {
		CSSM_FIELD &field = mFields[dex];
		if(appCompareCssmData(&field.FieldOid, &oid)) {
			if(foundDex == n) {
				found = &field;
				return 0;
			}
			foundDex++;
		}
	}
	printf("FieldArray::fieldForOid field not found\n");
	return 1;
}

/*
 * How many items in a NULL-terminated array of pointers?
 */
static unsigned nssArraySize(
	const void **array)
{
    unsigned count = 0;
    if (array) {
		while (*array++) {
			count++;
		}
    }
    return count;
}

static void doPrintCert(
	const CSSM_DATA &cert)
{
	printCert(cert.Data, cert.Length, CSSM_TRUE);
}

/*
 * The extensions whose presence causes us to skip the "compare
 * encoded and original TBS" test.
 */
#define USE_SKIPPED_EXTENS		1
#if		USE_SKIPPED_EXTENS
static const CSSM_OID *skippedExtens[] = {	// %%% FIXME: this is a workaround for <rdar://8265523>; shouldn't need to skip!
	&CSSMOID_PolicyMappings,
	&CSSMOID_PolicyConstraints
};
#define NUM_SKIPPED_EXTENS	\
	(sizeof(skippedExtens) / sizeof(skippedExtens[0]))
#endif	/* USE_SKIPPED_EXTENS */

static const CSSM_DATA *skippedCerts[] = {
	&anchor_46_derIssuer,
	&anchor_53_derIssuer,
	&anchor_60_derIssuer,
	&turk1_derIssuer,
	&turk2_derIssuer,
	&turk3_derIssuer,
	&globalSignRoot_derIssuer,
	&swisssign_derIssuer
};
#define NUM_SKIPPED_CERTS	(sizeof(skippedCerts) / sizeof(skippedCerts[0]))

static bool skipThisCert(
	const NSS_TBSCertificate &tbs)
{
	/* search by extension - currently unused */
	unsigned dex;
	#if USE_SKIPPED_EXTENS
	unsigned numExtens = nssArraySize((const void **)tbs.extensions);
	/* skip this section if that's empty - compiler warning causes failure */
	for(dex=0; dex<numExtens; dex++) {
		NSS_CertExtension *exten = tbs.extensions[dex];
		CSSM_OID *oid = &exten->extnId;
		for(unsigned skipDex=0; skipDex<NUM_SKIPPED_EXTENS; skipDex++) {
			if(appCompareCssmData(skippedExtens[skipDex], oid)) {
				return true;
			}
		}
	}
	#endif	/* USE_SKIPPED_EXTENS */
	
	/* search by specific issuer */
	for(dex=0; dex<NUM_SKIPPED_CERTS; dex++) {
		if(appCompareCssmData(skippedCerts[dex], &tbs.derIssuer)) {
			return true;
		}
	}
	return false;
}

/*
 * Given a field OID, figure out what WE think this field is.
 */
 
/* the field types we grok */
typedef enum {
	FT_Unknown,		// probably means we're out of sync with the CL
	FT_Normal,		// standard component of TBS
	FT_ReadOnly,	// Read only, don't use to create template
	FT_NotTBS,		// part of top-level cert, don't use to create TBS
	FT_ExtenParsed,	// extension the CL SHOULD HAVE parsed
	FT_ExtenUnknown	// extension the CL should NOT have parsed		
} FieldType;

/* map OID --> FieldType */
typedef struct {
	const CSSM_OID	*oid;
	FieldType		type;
} FieldOidType;

/* 
 * The CL-specific mapping table. 
 * This has to change whenever the CL is modified to add or delete
 * an extension or field!
 * For newbies, a tip: this basically has to stay in sync with the 
 * fieldFuncTable array in Security/AppleX509CL/CertFields.cpp. 
 */
FieldOidType knownFields[] = {
	{ 	&CSSMOID_X509V1Version, FT_Normal },
	{ 	&CSSMOID_X509V1SerialNumber, FT_Normal },
	{ 	&CSSMOID_X509V1IssuerNameCStruct, FT_Normal },
	{ 	&CSSMOID_X509V1SubjectNameCStruct, FT_Normal },
	{	&CSSMOID_X509V1SignatureAlgorithmTBS, FT_Normal },
	{	&CSSMOID_X509V1SignatureAlgorithm, FT_NotTBS },
	{	&CSSMOID_X509V1ValidityNotBefore, FT_Normal },
	{	&CSSMOID_X509V1ValidityNotAfter, FT_Normal },
	{	&CSSMOID_X509V1CertificateIssuerUniqueId, FT_Normal },
	{	&CSSMOID_X509V1CertificateSubjectUniqueId, FT_Normal },
	/* only one of these two can be set - use the SubjectPublicKeyInfo
	 * version */
	{	&CSSMOID_X509V1SubjectPublicKeyCStruct, FT_Normal },
	{	&CSSMOID_CSSMKeyStruct, FT_ReadOnly },
	{	&CSSMOID_X509V1Signature, FT_NotTBS },
	{   &CSSMOID_X509V1IssuerName, FT_ReadOnly },		// DER encoded
	{   &CSSMOID_X509V1SubjectName, FT_ReadOnly },		// DER encoded
	{   &CSSMOID_X509V1IssuerNameStd, FT_ReadOnly },	// DER encoded
	{   &CSSMOID_X509V1SubjectNameStd,FT_ReadOnly },	// DER encoded
		
	/* Extensions */
	{	&CSSMOID_KeyUsage, FT_ExtenParsed },
	{   &CSSMOID_BasicConstraints, FT_ExtenParsed },
	{	&CSSMOID_ExtendedKeyUsage, FT_ExtenParsed } ,
	{	&CSSMOID_SubjectKeyIdentifier, FT_ExtenParsed } ,
	{	&CSSMOID_AuthorityKeyIdentifier, FT_ExtenParsed } ,
	{	&CSSMOID_SubjectAltName, FT_ExtenParsed } ,
	{	&CSSMOID_IssuerAltName, FT_ExtenParsed } ,
	{	&CSSMOID_CertificatePolicies, FT_ExtenParsed } ,
	{	&CSSMOID_NetscapeCertType, FT_ExtenParsed } ,
	{	&CSSMOID_CrlDistributionPoints, FT_ExtenParsed },
	{   &CSSMOID_AuthorityInfoAccess, FT_ExtenParsed },
	{   &CSSMOID_SubjectInfoAccess, FT_ExtenParsed },
	{   &CSSMOID_X509V3CertificateExtensionCStruct, FT_ExtenUnknown },
	{   &CSSMOID_QC_Statements, FT_ExtenParsed },
	{	&CSSMOID_NameConstraints, FT_ExtenParsed },
	{	&CSSMOID_PolicyMappings, FT_ExtenParsed },
	{	&CSSMOID_PolicyConstraints, FT_ExtenParsed },
//	{	&CSSMOID_InhibitAnyPolicy, FT_ExtenParsed } //%%% FIXME: CSSMOID_InhibitAnyPolicy not exported!?
};
#define NUM_KNOWN_FIELDS (sizeof(knownFields) / sizeof(knownFields[0]))

static FieldType typeForOid(
	const CSSM_OID &oid)
{
	for(unsigned dex=0; dex<NUM_KNOWN_FIELDS; dex++) {
		FieldOidType &ft = knownFields[dex];
		if(appCompareCssmData(&oid, ft.oid)) {
			return ft.type;
		}
	}
	/* not found */
	return FT_Unknown;
}

static const char *fieldTypeStr(
	FieldType type)
{
	switch(type) {
		case FT_Unknown:		return "FT_Unknown";
		case FT_Normal: 		return "FT_Normal";
		case FT_ReadOnly: 		return "FT_ReadOnly";
		case FT_NotTBS: 		return "FT_NotTBS";
		case FT_ExtenParsed: 	return "FT_ExtenParsed";
		case FT_ExtenUnknown: 	return "FT_ExtenUnknown";
		default:
			printf("***BRRZAP!\n");
			exit(1);
	}
}

static const uint8 emptyAuthKeyId[2] = {0x30, 0};

/*
 * Extensions come in two flavors - parsed and not. However the 
 * CL will give us an unparsed version for extensions it normally 
 * understands but failed to decode. Detect that, and basic
 * extension formatting screwups, here.
 */
static int vfyExtens(
	FieldArray &extenFields,
	const CSSM_DATA &cert,		// for error display only 
	CSSM_BOOL quiet)	
{
	for(unsigned dex=0; dex<extenFields.mNumFields; dex++) {
		CSSM_FIELD &extenField = extenFields.fieldAt(dex);
		FieldType type = typeForOid(extenField.FieldOid);
		FieldType expectType;
		
		/* first verify well-formed extension field */
		CSSM_DATA &fieldValue = extenField.FieldValue;
		CSSM_X509_EXTENSION *exten = (CSSM_X509_EXTENSION *)fieldValue.Data;
		if((exten == NULL) || 
				(fieldValue.Length != sizeof(CSSM_X509_EXTENSION))) {
			doPrintCert(cert);
			printf("***Malformed CSSM_X509_EXTENSION\n");
			if(testError(quiet)) {
				return 1;
			}
			/* well let's limp along */
			continue;
		}

		/* currently (since Radar 3593624), these are both always valid */
		if((exten->BERvalue.Data == NULL) || 
		   (exten->value.parsedValue == NULL)) {  /* actually, one of three variants */
			printf("***Malformed CSSM_X509_EXTENSION (1)\n");
			return 1;
		}
		
		switch(exten->format) {
			case CSSM_X509_DATAFORMAT_ENCODED:
				if(type != FT_ExtenUnknown) {
					doPrintCert(cert);
					printf("***Entension format ENCODED, expected PARSED\n");
					if(testError(quiet)) {
						return 1;
					}
				}
				
				/* 
				 * Now make sure that the underlying extension ID isn't
				 * one that the CL was SUPPOSED to parse 
				 */
				// %%% FIXME: need to investigate why these are not fully parsed: <rdar://8265523>
				if(appCompareCssmData(&exten->extnId, &CSSMOID_PolicyConstraints)) {
					printf("...skipping policyConstraints extension per <rdar://8265523> (fix me!)\n");
					break;
				}
				if(appCompareCssmData(&exten->extnId, &CSSMOID_PolicyMappings)) {
					printf("...skipping policyMappings extension per <rdar://8265523> (fix me!)\n");
					break;
				}
				
				expectType = typeForOid(exten->extnId);
				if(expectType != FT_Unknown) {
					/* 
					 * Swisscom root has an authorityKeyId extension with an illegal value, 
					 * data inside the octet string is <30 00>, no context-specific wrapper
					 * or tag. 
					 * Instead of a hopeless complaint about that cert, let's just tolerate it 
					 * like this...
					 */
					if(appCompareCssmData(&exten->extnId, &CSSMOID_AuthorityKeyIdentifier) &&
					   (exten->BERvalue.Length == 2) &&
					   !memcmp(emptyAuthKeyId, exten->BERvalue.Data, 2)) {
						printf("...skipping bogus swisssign AuthorityKeyId\n");
						break;
					}
					doPrintCert(cert);
					printf("***underlying exten type %s, expect Unknown\n",
						fieldTypeStr(expectType));
					if(testError(quiet)) {
						return 1;
					}
				}
				break;
				
			case CSSM_X509_DATAFORMAT_PARSED:
				if(type != FT_ExtenParsed) {
					doPrintCert(cert);
					printf("***Entension format PARSED, expected ENCODED\n");
					if(testError(quiet)) {
						return 1;
					}
				}
				if(exten->value.parsedValue == NULL) {
					doPrintCert(cert);
					printf("***Parsed extension with NULL parsedValue\n");
					if(testError(quiet)) {
						return 1;
					}
				}
				break;
				
			default:
				doPrintCert(cert);
				printf("***Unknown Entension format %u\n",
					exten->format);
				if(testError(quiet)) {
					return 1;
				}
				break;
				
		}	/* switch(exten.format) */
	}
	return 0;
}

/*
 * Here's the hard part.
 * Given a raw cert and its components in two FieldArrays, crate a TBS
 * cert from scratch from those fields and ensure that the result 
 * is the same as the raw TBS field in the original cert. 
 */
static int buildTbs(
	CSSM_CL_HANDLE	clHand,
	const CSSM_DATA	&rawCert,
	FieldArray		&allFields,		// on entry, standard fields
	FieldArray		&extenFields,	// extensions only 
	CSSM_BOOL		quiet,
	CSSM_BOOL		verbose,
	CSSM_BOOL		writeBlobs)
{
	/*
	 * First do raw BER-decode in two ways - one to get the 
	 * extensions as they actuallly appear in the cert, and one
	 * to get the raw undecoded TBS.
	 */
	SecAsn1CoderRef coder;
	OSStatus ortn = SecAsn1CoderCreate(&coder);
	if(ortn) {
		cssmPerror("SecAsn1CoderCreate", ortn);
		return testError(quiet);
	}
	
	NSS_SignedCertOrCRL signedCert;	// with TBS as ASN_ANY
	memset(&signedCert, 0, sizeof(signedCert));
	if(SecAsn1DecodeData(coder, &rawCert, kSecAsn1SignedCertOrCRLTemplate,
			&signedCert)) {
		doPrintCert(rawCert);
		printf("***Error decoding cert to kSecAsn1SignedCertOrCRL\n");
		return testError(quiet);
	}
	
	NSS_Certificate fullCert;		// fully decoded
	memset(&fullCert, 0, sizeof(fullCert));
	if(SecAsn1DecodeData(coder, &rawCert, kSecAsn1SignedCertTemplate,
			&fullCert)) {
		doPrintCert(rawCert);
		printf("***Error decoding cert to kSecAsn1Certificate\n");
		return testError(quiet);
	}
	 
	NSS_TBSCertificate &tbs = fullCert.tbs;
	unsigned numExtens = nssArraySize((const void **)tbs.extensions);
	if(numExtens != extenFields.mNumFields) {
		/* The CL told us the wrong number of extensions */
		doPrintCert(rawCert);
		printf("***NSS says %u extens, CL says %u\n", numExtens,
			(unsigned)extenFields.mNumFields);
		return testError(quiet);
	}
	
	if(skipThisCert(tbs)) {
		if(verbose) {
			printf("   ...skipping TBS blob check\n");
		}
		SecAsn1CoderRelease(coder);
		return 0;
	}	
	
	/*
	 * The CL returns extension fields in an order which differs from
	 * the order of the extensions in the actual cert (because it
	 * does a table-based lookup, field by field, when doing a 
	 * CSSM_CL_CertGetAllFields()). We have to add the extensions
	 * from extenFields to allFields in the order they appear in 
	 * OUR decoded fullCert.
	 */
	unsigned numUnknowns = 0;
	for(unsigned dex=0; dex<numExtens; dex++) {
		NSS_CertExtension *exten = tbs.extensions[dex];
		CSSM_OID &oid = exten->extnId;
		FieldType type = typeForOid(oid);
		CSSM_FIELD *found = NULL;
		int rtn;
		switch(type) {
			case FT_ExtenParsed:
				/* 
				 * look for this exact extension
				 * NOTE we're assuming that only one copy of 
				 * each specific parsed extension exists. The 
				 * 509 spec does't specifically require this but
				 * I've never seen a case of multiple extensions
				 * of the same type in one cert. 
				 */
				rtn = extenFields.fieldForOid(oid, 0, found);
				break;
			case FT_Unknown:
				/* search for nth unparsed exten field */
				rtn = extenFields.fieldForOid(
					CSSMOID_X509V3CertificateExtensionCStruct,
					numUnknowns++,
					found);
				break;
			default:
				/* caller was already supposed to check this */
				doPrintCert(rawCert);
				printf("***HEY! buildTBS was given a bogus extension!\n");
				return 1;
		}
		if(rtn) {
			doPrintCert(rawCert);
			printf("***buildTBS could not find extension in CL's fields\n");
			return testError(quiet);
		}
		
		allFields.appendField(*found);
	}	/* processing extensions */
	
	/* 
	 * OK, the field array in allFields is ready to go down to 
	 * the CL.
	 */
	CSSM_RETURN crtn;
	CSSM_DATA clTbs = {0, NULL};
	crtn = CSSM_CL_CertCreateTemplate(clHand,
		allFields.mNumFields,
		allFields.mFields,
		&clTbs);
	if(crtn) {
		doPrintCert(rawCert);
		printError("CSSM_CL_CertCreateTemplate", crtn);
		return testError(quiet);
	}
	
	/*
	 * The moment of truth. Is that template identical to the 
	 * raw undecoded TBS blob we got by decoding a NSS_SignedCertOrCRL?
	 */
	int ourRtn = 0;
	if(!appCompareCssmData(&clTbs, &signedCert.tbsBlob)) {
		doPrintCert(rawCert);
		printf("***Encoded TBS does not match decoded TBS.\n");
		if(writeBlobs) {
			writeFile(ENC_TBS_BLOB, clTbs.Data, clTbs.Length);
			writeFile(DEC_TBS_BLOB, signedCert.tbsBlob.Data, 
				signedCert.tbsBlob.Length);
			printf("...wrote TBS blobs to %s and %s\n",
				ENC_TBS_BLOB, DEC_TBS_BLOB);
		}
		ourRtn = testError(quiet);
	}
	CSSM_FREE(clTbs.Data);
	SecAsn1CoderRelease(coder);
	return ourRtn;
}

/* verify root with itself using TP */
static int verifyRoot(
	CSSM_TP_HANDLE  tpHand,
	CSSM_CL_HANDLE	clHand,
	CSSM_CSP_HANDLE cspHand,
	const CSSM_DATA	&cert,
	CSSM_BOOL		allowExpired,
	CSSM_BOOL		useTrustSettings,
	CSSM_BOOL		quiet)
{
	BlobList blobs;
	blobs.addBlob(cert, CSSM_TRUE);
	int i;
	
	const char *certStatus;
	if(useTrustSettings) {
		/*
		 * CSSM_CERT_STATUS_IS_IN_INPUT_CERTS 
		 * CSSM_CERT_STATUS_IS_ROOT
		 * CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_SYSTEM
		 * CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST
		 */
		certStatus = "0:0x314";  
	}
	else {
		/*
		 * CSSM_CERT_STATUS_IS_IN_INPUT_CERTS (new since radar 3855635 was fixed)
		 * CSSM_CERT_STATUS_IS_IN_ANCHORS
		 * CSSM_CERT_STATUS_IS_ROOT
		 */
		certStatus = "0:0x1C";  
	}

	/* try one with allowExpiredRoot false, then true on error and if so 
	 * enabled to make sure we know what's going wrong */
	CSSM_BOOL expireEnable = CSSM_FALSE;
	for(int dex=0; dex<2; dex++) {
		i = certVerifySimple(tpHand, clHand, cspHand,
			blobs,		// certs
			blobs,		// and roots
			CSSM_FALSE, // useSystemAnchors
			CSSM_TRUE,  // leaf is CA
			expireEnable,
			CVP_Basic,
			NULL,		// SSL host
			CSSM_FALSE, // SSL client
			NULL,		// sender email
			0,			// key use
			NULL,		// expected error str
			0, NULL,	// per-cert errors
			1, &certStatus,	// per-cert status
			useTrustSettings,
			quiet, 
			CSSM_FALSE);	// verbose
		if(i == 0) {
			/* success */
			if(dex == 1) {
				printf("...warning: expired root detected. Be aware.\n");
			}
			return 0;
		}
		if(!allowExpired) {
			/* no second chance */
			return i;
		}
		expireEnable = CSSM_TRUE;
		if(useTrustSettings) {
			/* now expect EXPIRED, IS_ROOT, IS_IN_INPUT_CERTS, TRUST_SETTINGS_FOUND_SYSTEM,
			 * CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST */
			certStatus = "0:0x315";	
		}
		else {
			/* now expect EXPIRED, IS_ROOT, IS_IN_ANCHORS, IS_IN_INPUT_CERTS */
			certStatus = "0:0x1d";	
		}
	}
	return i;
}


static int doTest(
	CSSM_CL_HANDLE	clHand,
	CSSM_TP_HANDLE  tpHand,
	CSSM_CSP_HANDLE cspHand,
	const CSSM_DATA	&cert,
	CSSM_BOOL		allowExpired,
	CSSM_BOOL		quiet,
	CSSM_BOOL		verbose,
	CSSM_BOOL		writeBlobs,
	CSSM_BOOL		useTrustSettings)
{
	/* first see if this anchor self-verifies. */
	if(verifyRoot(tpHand, clHand, cspHand, cert, allowExpired, 
			useTrustSettings, quiet)) {
		doPrintCert(cert);
		printf("***This anchor does not self-verify!\n");
		return testError(quiet);
	}
	
	/* have the CL parse it to the best of its ability */
	CSSM_FIELD_PTR certFields;
	uint32 numFields;
	CSSM_RETURN crtn = CSSM_CL_CertGetAllFields(clHand, &cert, &numFields, 
		&certFields);
	if(crtn) {
		printError("CSSM_CL_CertGetAllFields", crtn);
		doPrintCert(cert);
		printf("***The CL can not parse this anchor!\n");
		return testError(quiet);
	}

	/* save, this object does the free fields when it goes out of scope */
	FieldArray parsed(certFields, numFields, clHand);
	
	/* 
	 * We're going to build a TBSCert from these received fields.
	 * Extensions need to be processed specially because they 
	 * come back from the CL ordered differently than they appear
	 * in the cert. 
	 *
	 * First make two buckets for making copies of incoming fields.
	 */
	FieldArray forCreate(numFields);	// for creating template
	FieldArray extenFields(numFields);
	
	for(unsigned dex=0; dex<numFields; dex++) {
		CSSM_FIELD &parsedField = parsed.fieldAt(dex);
		FieldType type = typeForOid(parsedField.FieldOid);
		switch(type) {
			case FT_Normal:
				forCreate.appendField(parsedField);
				break;
			case FT_ReadOnly:
			case FT_NotTBS:
				/* ignore */
				break;
			case FT_ExtenParsed:
			case FT_ExtenUnknown:
				/* extensions, save and process later */
				extenFields.appendField(parsedField);
				break;
			default:
				doPrintCert(cert);
				printf("***This anchor contains an unknown field!\n");
				if(testError(quiet)) {
					return 1;
				}
				/* well let's limp along */
				forCreate.appendField(parsedField);
				break;
		}
	}
	
	/* basic extension verification */
	if(vfyExtens(extenFields, cert, quiet)) {
		return 1;
	}
	return buildTbs(clHand, cert, forCreate, extenFields, quiet, 
		verbose, writeBlobs);
}

int main(int argc, char **argv)
{
	CSSM_BOOL quiet = CSSM_FALSE;
	CSSM_BOOL verbose = CSSM_FALSE;
	CSSM_BOOL writeBlobs = CSSM_FALSE;
	CSSM_BOOL allowExpired = CSSM_FALSE;
	CSSM_BOOL useTrustSettings = CSSM_FALSE;

	for(int arg=1; arg<argc; arg++) {
		switch(argv[arg][0]) {
			case 'q':
				quiet = CSSM_TRUE;
				break;
			case 'v':
				verbose = CSSM_TRUE;
				break;
			case 'w':
				writeBlobs = CSSM_TRUE;
				break;
			case 't':
				useTrustSettings = CSSM_TRUE;
				break;
			case 'e':
				allowExpired = CSSM_TRUE;
				break;
			default:
				usage(argv);
		}
	}
	
	printf("Starting anchorTest; args: ");
	for(int i=1; i<argc; i++) {
		printf("%s ", argv[i]);
	}
	printf("\n");

	/* get system anchors only; convert to CSSM */
	CFArrayRef cfAnchors;
	OSStatus ortn;
	CSSM_DATA *anchors;
	unsigned numAnchors;
	ortn = getSystemAnchors(&cfAnchors, &anchors, &numAnchors);
	if(ortn) {
		exit(1);
	}
	if(numAnchors < 50) {
		printf("***Hey! I can only find %u anchors; there should be way more than that.\n",
			numAnchors);
		exit(1);
	}

	CSSM_CL_HANDLE clHand = clStartup();
	CSSM_TP_HANDLE tpHand = tpStartup();
	CSSM_CSP_HANDLE cspHand = cspStartup();
	if((clHand == 0) || (tpHand == 0) || (cspHand == 0)) {
		return 0;
	}

	int rtn = 0;
	for(unsigned dex=0; dex<numAnchors; dex++) {
		if(!quiet) {
			printf("...anchor %u\n", dex);
		}
		rtn = doTest(clHand, tpHand, cspHand, anchors[dex], allowExpired,
			quiet, verbose, writeBlobs, useTrustSettings);
		if(rtn) {
			break;
		}
	}
	if(rtn == 0) {
		if(!quiet) {
			printf("...%s success.\n", argv[0]);
		}
	}
	return rtn;
}
