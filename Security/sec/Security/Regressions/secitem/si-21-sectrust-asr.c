/*
 *  si-21-sectrust-asr.c
 *  Security
 *
 *  Copyright (c) 2009-2010 Apple Inc.. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecKey.h>
#include <Security/SecInternal.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>

#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

static const UInt8 sITunesStoreRootCertificate[] =
{
	0x30, 0x82, 0x04, 0x65, 0x30, 0x82, 0x03, 0x4d, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x02, 0x09, 0x00,
	0xcb, 0x06, 0xa3, 0x3b, 0x30, 0xc3, 0x24, 0x03, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86,
	0xf7, 0x0d, 0x01, 0x01, 0x05, 0x05, 0x00, 0x30, 0x7e, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55,
	0x04, 0x0a, 0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31, 0x15,
	0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x0c, 0x69, 0x54, 0x75, 0x6e, 0x65, 0x73, 0x20,
	0x53, 0x74, 0x6f, 0x72, 0x65, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x11,
	0x69, 0x54, 0x75, 0x6e, 0x65, 0x73, 0x20, 0x53, 0x74, 0x6f, 0x72, 0x65, 0x20, 0x52, 0x6f, 0x6f,
	0x74, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13,
	0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x13, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f, 0x72,
	0x6e, 0x69, 0x61, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13, 0x09, 0x43, 0x75,
	0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f, 0x30, 0x1e, 0x17, 0x0d, 0x30, 0x37, 0x31, 0x30, 0x30,
	0x39, 0x31, 0x37, 0x35, 0x31, 0x33, 0x30, 0x5a, 0x17, 0x0d, 0x33, 0x32, 0x31, 0x30, 0x30, 0x32,
	0x31, 0x37, 0x35, 0x31, 0x33, 0x30, 0x5a, 0x30, 0x7e, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55,
	0x04, 0x0a, 0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49, 0x6e, 0x63, 0x2e, 0x31, 0x15,
	0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x0c, 0x69, 0x54, 0x75, 0x6e, 0x65, 0x73, 0x20,
	0x53, 0x74, 0x6f, 0x72, 0x65, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03, 0x55, 0x04, 0x03, 0x13, 0x11,
	0x69, 0x54, 0x75, 0x6e, 0x65, 0x73, 0x20, 0x53, 0x74, 0x6f, 0x72, 0x65, 0x20, 0x52, 0x6f, 0x6f,
	0x74, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x13,
	0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x13, 0x0a, 0x43, 0x61, 0x6c, 0x69, 0x66, 0x6f, 0x72,
	0x6e, 0x69, 0x61, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04, 0x07, 0x13, 0x09, 0x43, 0x75,
	0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f, 0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a,
	0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30,
	0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01, 0x00, 0xa9, 0x78, 0xc0, 0xaf, 0x1a, 0x96, 0x59, 0xad,
	0xf8, 0x3c, 0x16, 0xe1, 0xfc, 0xc4, 0x7a, 0xaf, 0xf0, 0x80, 0xed, 0x7f, 0x3a, 0xff, 0xf2, 0x2a,
	0xb6, 0xf3, 0x1a, 0xdd, 0xbd, 0x14, 0xb1, 0x5d, 0x9d, 0x66, 0xaf, 0xc7, 0xaf, 0x2b, 0x26, 0x78,
	0x9c, 0xb8, 0x0b, 0x41, 0x9c, 0xdc, 0x17, 0xf1, 0x40, 0x18, 0x09, 0xa1, 0x0a, 0xbc, 0x01, 0x9a,
	0x0c, 0xbe, 0x89, 0xdb, 0x9d, 0x34, 0xc7, 0x52, 0x8a, 0xf2, 0xbf, 0x35, 0x2b, 0x24, 0x04, 0xb0,
	0x0c, 0x9d, 0x41, 0x7d, 0x63, 0xe3, 0xad, 0xcf, 0x8b, 0x34, 0xbf, 0x5c, 0x42, 0x82, 0x9b, 0x78,
	0x7f, 0x00, 0x10, 0x88, 0xd9, 0xfd, 0xf8, 0xbf, 0x63, 0x2c, 0x91, 0x87, 0x03, 0xda, 0xbc, 0xc6,
	0x71, 0x2b, 0x9a, 0x21, 0x30, 0x95, 0xd6, 0x88, 0xe8, 0xbd, 0x0a, 0x74, 0xa4, 0xa6, 0x39, 0xd0,
	0x61, 0xd3, 0xb6, 0xe0, 0x2b, 0x1e, 0xe4, 0x78, 0x5c, 0x70, 0x32, 0x66, 0x97, 0x34, 0xa9, 0x79,
	0xfc, 0x96, 0xaf, 0x4b, 0x8a, 0xd5, 0x12, 0x07, 0x8c, 0x1c, 0xf6, 0x3e, 0x5f, 0xdc, 0x8f, 0x92,
	0x10, 0xe8, 0x7e, 0xa0, 0x14, 0x1e, 0x61, 0x28, 0xfa, 0xcc, 0xcf, 0x3c, 0xdb, 0x2b, 0xe3, 0xe9,
	0x44, 0x4a, 0x9d, 0x5f, 0x92, 0x3d, 0xa3, 0xfd, 0x1a, 0x63, 0xb4, 0xbb, 0xab, 0x67, 0x45, 0xc6,
	0x4d, 0x84, 0x4a, 0xaa, 0x33, 0xe4, 0xde, 0xd3, 0x04, 0x92, 0xbf, 0xf7, 0x00, 0x48, 0x76, 0xc6,
	0x4e, 0x17, 0xea, 0x70, 0xdb, 0x09, 0xbc, 0x22, 0x07, 0x7b, 0x97, 0x49, 0xe5, 0x29, 0xa7, 0x1a,
	0x04, 0xd2, 0x0d, 0x0e, 0x73, 0xf1, 0x49, 0x43, 0x34, 0x35, 0x61, 0xe5, 0x67, 0xdf, 0x3c, 0x58,
	0x42, 0x51, 0xfb, 0xc3, 0xa4, 0x15, 0x6d, 0x39, 0x6b, 0x2a, 0x22, 0xde, 0xdd, 0xe2, 0x36, 0x5b,
	0xd7, 0x37, 0x53, 0x96, 0x9d, 0x3a, 0x9f, 0x4b, 0x02, 0x03, 0x01, 0x00, 0x01, 0xa3, 0x81, 0xe5,
	0x30, 0x81, 0xe2, 0x30, 0x1d, 0x06, 0x03, 0x55, 0x1d, 0x0e, 0x04, 0x16, 0x04, 0x14, 0xb0, 0xda,
	0xe1, 0x7f, 0xa8, 0x8b, 0x4a, 0x6a, 0x81, 0x5d, 0x0c, 0xa1, 0x84, 0x56, 0x46, 0x1e, 0x6a, 0xef,
	0xe5, 0xcf, 0x30, 0x81, 0xb2, 0x06, 0x03, 0x55, 0x1d, 0x23, 0x04, 0x81, 0xaa, 0x30, 0x81, 0xa7,
	0x80, 0x14, 0xb0, 0xda, 0xe1, 0x7f, 0xa8, 0x8b, 0x4a, 0x6a, 0x81, 0x5d, 0x0c, 0xa1, 0x84, 0x56,
	0x46, 0x1e, 0x6a, 0xef, 0xe5, 0xcf, 0xa1, 0x81, 0x83, 0xa4, 0x81, 0x80, 0x30, 0x7e, 0x31, 0x13,
	0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x13, 0x0a, 0x41, 0x70, 0x70, 0x6c, 0x65, 0x20, 0x49,
	0x6e, 0x63, 0x2e, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x0b, 0x13, 0x0c, 0x69, 0x54,
	0x75, 0x6e, 0x65, 0x73, 0x20, 0x53, 0x74, 0x6f, 0x72, 0x65, 0x31, 0x1a, 0x30, 0x18, 0x06, 0x03,
	0x55, 0x04, 0x03, 0x13, 0x11, 0x69, 0x54, 0x75, 0x6e, 0x65, 0x73, 0x20, 0x53, 0x74, 0x6f, 0x72,
	0x65, 0x20, 0x52, 0x6f, 0x6f, 0x74, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13,
	0x02, 0x55, 0x53, 0x31, 0x13, 0x30, 0x11, 0x06, 0x03, 0x55, 0x04, 0x08, 0x13, 0x0a, 0x43, 0x61,
	0x6c, 0x69, 0x66, 0x6f, 0x72, 0x6e, 0x69, 0x61, 0x31, 0x12, 0x30, 0x10, 0x06, 0x03, 0x55, 0x04,
	0x07, 0x13, 0x09, 0x43, 0x75, 0x70, 0x65, 0x72, 0x74, 0x69, 0x6e, 0x6f, 0x82, 0x09, 0x00, 0xcb,
	0x06, 0xa3, 0x3b, 0x30, 0xc3, 0x24, 0x03, 0x30, 0x0c, 0x06, 0x03, 0x55, 0x1d, 0x13, 0x04, 0x05,
	0x30, 0x03, 0x01, 0x01, 0xff, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01,
	0x01, 0x05, 0x05, 0x00, 0x03, 0x82, 0x01, 0x01, 0x00, 0x7b, 0xcc, 0xfb, 0x34, 0x4e, 0xec, 0x27,
	0x05, 0xf9, 0x10, 0xc9, 0xdf, 0x8e, 0x22, 0x21, 0x94, 0x70, 0xe9, 0x74, 0x64, 0x11, 0xce, 0x07,
	0x91, 0xc2, 0x58, 0x0d, 0xff, 0x51, 0x6d, 0x97, 0x64, 0x32, 0x1a, 0x1c, 0xdf, 0x4a, 0x93, 0xdb,
	0x94, 0x62, 0x14, 0xcb, 0x00, 0x13, 0x37, 0x98, 0x0e, 0x3d, 0x96, 0x19, 0x5f, 0x44, 0xc9, 0x11,
	0xd2, 0xc9, 0x8c, 0xa3, 0x19, 0x2f, 0x88, 0x4f, 0x5f, 0x3c, 0x46, 0x56, 0xe2, 0xbd, 0x78, 0x4f,
	0xfe, 0x8e, 0x39, 0xb5, 0xed, 0x37, 0x3e, 0xfb, 0xf6, 0xae, 0x56, 0x2c, 0x49, 0x37, 0x4a, 0x94,
	0x05, 0x4b, 0x8f, 0x67, 0xdb, 0xe6, 0x24, 0xa6, 0x75, 0xae, 0xc8, 0xa2, 0x26, 0x87, 0x70, 0xb8,
	0x1d, 0xc2, 0xfc, 0x8d, 0xff, 0x41, 0x23, 0x8a, 0x01, 0x8a, 0xc3, 0x78, 0x5a, 0x61, 0x4a, 0xed,
	0x48, 0x96, 0xb5, 0x82, 0xa7, 0xaa, 0x2e, 0xb5, 0xed, 0xdd, 0xf4, 0xe6, 0xb5, 0xa1, 0x27, 0x3b,
	0xda, 0xf9, 0x18, 0x26, 0x7e, 0x8e, 0xec, 0xef, 0xe1, 0x00, 0x7d, 0x3d, 0xf7, 0x3d, 0x01, 0x68,
	0x14, 0x92, 0xfc, 0x9c, 0xbb, 0x0a, 0xa1, 0xc3, 0x60, 0x31, 0x16, 0x08, 0x9b, 0xef, 0x4d, 0xaf,
	0x46, 0xc7, 0xcc, 0x4e, 0x05, 0x34, 0xa8, 0x44, 0xb2, 0x85, 0x03, 0x67, 0x6c, 0x31, 0xae, 0xa3,
	0x18, 0xb5, 0x5f, 0x75, 0xae, 0xe0, 0x5a, 0xbf, 0x64, 0x32, 0x2b, 0x28, 0x99, 0x24, 0xcd, 0x01,
	0x34, 0xc2, 0xfc, 0xf1, 0x88, 0xba, 0x8c, 0x9b, 0x90, 0x85, 0x56, 0x6d, 0xaf, 0xd5, 0x2e, 0x88,
	0x12, 0x61, 0x7c, 0x76, 0x33, 0x6b, 0xc4, 0xf7, 0x31, 0x77, 0xe4, 0x02, 0xb7, 0x9e, 0x9c, 0x8c,
	0xbe, 0x04, 0x2e, 0x51, 0xa3, 0x04, 0x4c, 0xcd, 0xe2, 0x71, 0x5e, 0x36, 0xfb, 0xf1, 0x68, 0xf0,
	0xad, 0x37, 0x80, 0x98, 0x26, 0xc0, 0xef, 0x9b, 0x3c
};

static const unsigned char url_bag[] =
"<plist version=\"1.0\">"
"        <dict>"
"        <key>signature</key>"
"        <data>IIHLRC69w8K+iJQYKEh5U1wo/H2+U27lFzQlLrUWZIqBkd2rvUOcxBJlAG/5rCnq/mNwfhvrRZjpBzC9FzzH4a1mImPPGBYQtkD2pw/deJ67jPymyDlseH85grcDBgbRYaTR4+pbr4XTsMyQ1wEEF8OExKw9pNfHu1XyLg4iS3A=</data>"
"        <key>certs</key>"
"        <array>"
"            <data>MIIDOTCCAiGgAwIBAgIBATANBgkqhkiG9w0BAQQFADB+MRMwEQYDVQQKEwpBcHBsZSBJbmMuMRUwEwYDVQQLEwxpVHVuZXMgU3RvcmUxGjAYBgNVBAMTEWlUdW5lcyBTdG9yZSBSb290MQswCQYDVQQGEwJVUzETMBEGA1UECBMKQ2FsaWZvcm5pYTESMBAGA1UEBxMJQ3VwZXJ0aW5vMB4XDTA3MTAwOTIxNTkxNFoXDTA4MTEwNzIxNTkxNFowgYExEzARBgNVBAoTCkFwcGxlIEluYy4xFTATBgNVBAsTDGlUdW5lcyBTdG9yZTEdMBsGA1UEAxMUaVR1bmVzIFN0b3JlIFVSTCBCYWcxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRIwEAYDVQQHEwlDdXBlcnRpbm8wgZ8wDQYJKoZIhvcNAQEBBQADgY0AMIGJAoGBAOLMu/eV+eSLVEGtn536FkXAsi/vtpXdHpTNS9muEVlvlkubKXdPDd5jV5WnQpAKY4GZrBn8azP9UKBd85nhIb5nqHQHCmH5DpBK9GZPFpoIdXguJSre8pZwQaYEXQGtTt3nXvk9k8OHs5W/9xFLuD7fpkKSIl+0KLPFULdyEtlvAgMBAAGjQjBAMB0GA1UdDgQWBBTd4gDjfN3LFr3b5G8dvUTpC56JZTAfBgNVHSMEGDAWgBSw2uF/qItKaoFdDKGEVkYeau/lzzANBgkqhkiG9w0BAQQFAAOCAQEAIDpkK1CqTNyl7SEZWvUTRYPdZzn9Y4QjnbSQ6hFkF/PClJkXn3TzMW3ojnxNLphKZxOY53s6D/Hf1B5UX2bJDAnfQ/W8d10SPubGJ1FnUZK8KaKeOzAgks5ob9dnOUe4CZKhZ5FyggIJfgd38Q0s8WF474j5OA/5XRPczgjt+OiIfzEVX5Xqpm1TU7T4013eHze5umqAsd9fFxUXdTC+bl9xdj5VOmqUUfOivoiqiBK2/6XAaDIFF/PEnxVou+BpqkdsyTZz/HiQApve+7NONqS58ciq3Ov+wivpVJKxMyFgcXFWb/d2ZTc04i+fGf0OA4QmkSRcAZOxQkv0oggtTw==</data>"
"        </array>"
"        <key>bag</key>"
"        <data>PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiIHN0YW5kYWxvbmU9Im5vIj8+CjwhRE9DVFlQRSBwbGlzdCBQVUJMSUMgIi0vL0FwcGxlIENvbXB1dGVyLy9EVEQgUExJU1QgMS4wLy9FTiIgImh0dHA6Ly93d3cuYXBwbGUuY29tL0RURHMvUHJvcGVydHlMaXN0LTEuMC5kdGQiPiAKCiAgPHBsaXN0IHZlcnNpb249IjEuMCI+CiAgICA8ZGljdD4KICAgICAgCiAgICAgIAogICAgICAKICAgICAgCiAgICAgICAgPGtleT5zdG9yZUZyb250PC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evc3RvcmVGcm9udDwvc3RyaW5nPgogICAgPGtleT5uZXdVc2VyU3RvcmVGcm9udDwva2V5PjxzdHJpbmc+aHR0cDovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9maXJzdExhdW5jaDwvc3RyaW5nPgogICAgPGtleT5uZXdJUG9kVXNlclN0b3JlRnJvbnQ8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9uZXdJUG9kVXNlcj9uZXdJUG9kVXNlcj10cnVlPC9zdHJpbmc+CiAgICA8a2V5Pm5ld1Bob25lVXNlcjwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL3Bob25lTGFuZGluZ1BhZ2U8L3N0cmluZz4gICAgICAgICAgICAgICAgICAKICAgIDxrZXk+c2VhcmNoPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTZWFyY2gud29hL3dhL3NlYXJjaDwvc3RyaW5nPgogICAgPGtleT5hZHZhbmNlZFNlYXJjaDwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU2VhcmNoLndvYS93YS9hZHZhbmNlZFNlYXJjaDwvc3RyaW5nPgogICAgPGtleT5zZWFyY2hIaW50czwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU2VhcmNoSGludHMud29hL3dhL2hpbnRzPC9zdHJpbmc+CiAgICA8a2V5PnBhcmVudGFsQWR2aXNvcnk8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9wYXJlbnRhbEFkdmlzb3J5PC9zdHJpbmc+CiAgICA8a2V5PnNvbmdNZXRhRGF0YTwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL3NvbmdNZXRhRGF0YTwvc3RyaW5nPgogICAgPGtleT5icm93c2U8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9icm93c2U8L3N0cmluZz4KICAgIDxrZXk+YnJvd3NlU3RvcmU8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9icm93c2VTdG9yZTwvc3RyaW5nPgogICAgPGtleT5icm93c2VHZW5yZTwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL2Jyb3dzZUdlbnJlPC9zdHJpbmc+CiAgICA8a2V5PmJyb3dzZUFydGlzdDwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL2Jyb3dzZUFydGlzdDwvc3RyaW5nPgogICAgPGtleT5icm93c2VBbGJ1bTwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL2Jyb3dzZUFsYnVtPC9zdHJpbmc+CiAgICA8a2V5PnZpZXdBbGJ1bTwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL3ZpZXdBbGJ1bTwvc3RyaW5nPgogICAgPGtleT52aWV3QXJ0aXN0PC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evdmlld0FydGlzdDwvc3RyaW5nPgogICAgPGtleT52aWV3Q29tcG9zZXI8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS92aWV3Q29tcG9zZXI8L3N0cmluZz4KICAgIDxrZXk+dmlld0dlbnJlPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evdmlld0dlbnJlPC9zdHJpbmc+CiAgICA8a2V5PnZpZXdQb2RjYXN0PC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evdmlld1BvZGNhc3Q8L3N0cmluZz4KICAgIDxrZXk+dmlld1B1Ymxpc2hlZFBsYXlsaXN0PC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evdmlld1B1Ymxpc2hlZFBsYXlsaXN0PC9zdHJpbmc+CiAgICA8a2V5PnZpZXdWaWRlbzwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL3ZpZXdWaWRlbzwvc3RyaW5nPgogICAgPGtleT5wb2RjYXN0czwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL3ZpZXdQb2RjYXN0RGlyZWN0b3J5PC9zdHJpbmc+CiAgICA8a2V5PmV4dGVybmFsVVJMU2VhcmNoS2V5PC9rZXk+PHN0cmluZz5heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQ8L3N0cmluZz4KICAgIDxrZXk+ZXh0ZXJuYWxVUkxSZXBsYWNlS2V5PC9rZXk+PHN0cmluZz5waG9ib3MuYXBwbGUuY29tPC9zdHJpbmc+CiAgICA8a2V5PnNlbGVjdGVkSXRlbXNQYWdlPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evc2VsZWN0ZWRJdGVtc1BhZ2U8L3N0cmluZz4KCiAgICAKCiAgICAKCiAgICA8a2V5Pm1pbmktc3RvcmU8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9taW5pc3RvcmVWMjwvc3RyaW5nPgogICAgPGtleT5taW5pLXN0b3JlLWZpZWxkczwva2V5PjxzdHJpbmc+YSxraW5kLHA8L3N0cmluZz4KICAgIDxrZXk+bWluaS1zdG9yZS1tYXRjaDwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmVTZXJ2aWNlcy53b2Evd2EvbWluaXN0b3JlTWF0Y2hWMjwvc3RyaW5nPgogICAgPGtleT5taW5pLXN0b3JlLW1hdGNoLWZpZWxkczwva2V5PjxzdHJpbmc+YW4sZ24sa2luZCxwbjwvc3RyaW5nPgogICAgPGtleT5taW5pLXN0b3JlLXdlbGNvbWU8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS9taW5pc3RvcmVXZWxjb21lP3dpdGhDbGllbnRPcHRJbj0xPC9zdHJpbmc+CgogICAgPGtleT5jb3Zlci1hcnQ8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlU2VydmljZXMud29hL3dhL2NvdmVyQXJ0TWF0Y2g8L3N0cmluZz4KICAgIDxrZXk+Y292ZXItYXJ0LWZpZWxkczwva2V5PjxzdHJpbmc+YSxwPC9zdHJpbmc+CiAgICA8a2V5PmNvdmVyLWFydC1jZC1maWVsZHM8L2tleT48c3RyaW5nPmNkZGI8L3N0cmluZz4KICAgIDxrZXk+Y292ZXItYXJ0LW1hdGNoPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZVNlcnZpY2VzLndvYS93YS9jb3ZlckFydE1hdGNoPC9zdHJpbmc+CiAgICA8a2V5PmNvdmVyLWFydC1tYXRjaC1maWVsZHM8L2tleT48c3RyaW5nPmNkZGIsYW4scG48L3N0cmluZz4KICAgIDxrZXk+Y292ZXItYXJ0LXVzZXI8L2tleT48c3RyaW5nPmh0dHA6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpQZXJzb25hbGl6ZXIud29hL3dhL2NvdmVyQXJ0VXNlcjwvc3RyaW5nPgoKICAgIDxrZXk+bWF0Y2hVUkxzPC9rZXk+PGFycmF5PjxzdHJpbmc+aHR0cDovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy88L3N0cmluZz48L2FycmF5PgoKICAgIAogICAgPGtleT5saWJyYXJ5LWxpbms8L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlU2VydmljZXMud29hL3dhL2xpYnJhcnlMaW5rPC9zdHJpbmc+CiAgICAKICAgIDxrZXk+bGlicmFyeS1saW5rLWZpZWxkcy1saXN0PC9rZXk+CiAgICA8YXJyYXk+CiAgICAgIDxzdHJpbmc+YW4sY24sZ24sa2luZCxuLHBuPC9zdHJpbmc+CiAgICA8L2FycmF5PgogICAgCiAgICA8a2V5PmxpYnJhcnlMaW5rPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZVNlcnZpY2VzLndvYS93YS9saWJyYXJ5TGluazwvc3RyaW5nPgoKICAgIAogICAgPGtleT5hdmFpbGFibGUtcmluZ3RvbmVzPC9rZXk+PHN0cmluZz5odHRwOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aUGVyc29uYWxpemVyLndvYS93YS9hdmFpbGFibGVSaW5ndG9uZXM8L3N0cmluZz4KCiAgICAKICAgIDxrZXk+Y3JlYXRlLXJpbmd0b25lPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL2NyZWF0ZVJpbmd0b25lPC9zdHJpbmc+CiAgICA8a2V5PnJpbmd0b25lLWluZm88L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvaXNSaW5ndG9uZWFibGU8L3N0cmluZz4KICAgIDxrZXk+cmluZ3RvbmUtaW5mby1maWVsZHMtbGlzdDwva2V5PgogICAgPGFycmF5PgogICAgICAgIDxzdHJpbmc+aWQscyxkc2lkPC9zdHJpbmc+CiAgICA8L2FycmF5PgogICAgCgoKICAgIDxrZXk+bWF4Q29tcHV0ZXJzPC9rZXk+PHN0cmluZz41PC9zdHJpbmc+CiAgICA8a2V5Pm1heFB1Ymxpc2hlZFBsYXlsaXN0SXRlbXM8L2tleT48aW50ZWdlcj4xMDA8L2ludGVnZXI+CiAgICAKICAgIDxrZXk+dHJ1c3RlZERvbWFpbnM8L2tleT4KICAgIDxhcnJheT4KICAgICAgPHN0cmluZz4uYXBwbGUuY29tPC9zdHJpbmc+CiAgICAgIDxzdHJpbmc+LmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0PC9zdHJpbmc+CiAgICAgIDxzdHJpbmc+c3VwcG9ydC5tYWMuY29tPC9zdHJpbmc+CiAgICAgIDxzdHJpbmc+Lml0dW5lcy5jb208L3N0cmluZz4KICAgICAgPHN0cmluZz5pdHVuZXMuY29tPC9zdHJpbmc+CiAgICA8L2FycmF5PgoKICAgIDxrZXk+cGx1cy1pbmZvPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2EvaVR1bmVzUGx1c0xlYXJuTW9yZVBhZ2U8L3N0cmluZz4KCiAgICA8a2V5PmFwcGxldHYteW91dHViZS1hdXRoLXVybDwva2V5PjxzdHJpbmc+aHR0cHM6Ly93d3cuZ29vZ2xlLmNvbS88L3N0cmluZz4KICAgIDxrZXk+YXBwbGV0di15b3V0dWJlLXVybDwva2V5PjxzdHJpbmc+aHR0cDovL2dkYXRhLnlvdXR1YmUuY29tLzwvc3RyaW5nPgogICAgPGtleT5pdHVuZXMtcHJlc2VudHMtZGlyZWN0b3J5LXVybDwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmVTZXJ2aWNlcy53b2Evd3MvUlNTL2RpcmVjdG9yeTwvc3RyaW5nPgogICAgPGtleT5HaG9zdHJpZGVyPC9rZXk+PHN0cmluZz5ZRVM8L3N0cmluZz4KCiAgICA8a2V5PnAyLXRvcC10ZW48L2tleT48c3RyaW5nPmh0dHA6Ly9heC5waG9ib3MuYXBwbGUuY29tLmVkZ2VzdWl0ZS5uZXQvV2ViT2JqZWN0cy9NWlN0b3JlLndvYS93YS92aWV3VG9wVGVuc0xpc3Q8L3N0cmluZz4KICAgIDxrZXk+cDItc2VydmljZS10ZXJtcy11cmw8L2tleT48c3RyaW5nPmh0dHA6Ly93d3cuYXBwbGUuY29tL3N1cHBvcnQvaXR1bmVzL2xlZ2FsL3Rlcm1zLmh0bWw8L3N0cmluZz4KCiAgICA8a2V5Pm5vdy1wbGF5aW5nLXVybDwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL25vd1BsYXlpbmc8L3N0cmluZz4KICAgIDxrZXk+bm93LXBsYXlpbmctbmV0d29yay1kZXRlY3QtdXJsPC9rZXk+PHN0cmluZz5odHRwOi8vYXgucGhvYm9zLmFwcGxlLmNvbS5lZGdlc3VpdGUubmV0L1dlYk9iamVjdHMvTVpTdG9yZS53b2Evd2Evbm93UGxheWluZzwvc3RyaW5nPgogICAgPGtleT5hZGFtaWQtbG9va3VwLXVybDwva2V5PjxzdHJpbmc+aHR0cDovL2F4LnBob2Jvcy5hcHBsZS5jb20uZWRnZXN1aXRlLm5ldC9XZWJPYmplY3RzL01aU3RvcmUud29hL3dhL2FkYW1JZExvb2t1cDwvc3RyaW5nPgoKICAgIAoKICAgIAoKICAgICAgICA8a2V5PmF1dGhlbnRpY2F0ZUFjY291bnQ8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvYXV0aGVudGljYXRlPC9zdHJpbmc+CiAgICA8a2V5PmlQaG9uZUFjdGl2YXRpb248L2tleT48c3RyaW5nPmh0dHBzOi8vYWxiZXJ0LmFwcGxlLmNvbS9XZWJPYmplY3RzL0FMQWN0aXZhdGlvbi53b2Evd2EvaVBob25lUmVnaXN0cmF0aW9uPC9zdHJpbmc+CiAgICA8a2V5PmRldmljZS1hY3RpdmF0aW9uPC9rZXk+PHN0cmluZz5odHRwczovL2FsYmVydC5hcHBsZS5jb20vV2ViT2JqZWN0cy9BTEFjdGl2YXRpb24ud29hL3dhL2RldmljZUFjdGl2YXRpb248L3N0cmluZz4KICAgIDxrZXk+YXV0aG9yaXplTWFjaGluZTwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9hdXRob3JpemVNYWNoaW5lPC9zdHJpbmc+CiAgICA8a2V5PmJ1eVByb2R1Y3Q8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvYnV5UHJvZHVjdDwvc3RyaW5nPgogICAgPGtleT5idXlDYXJ0PC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL2J1eUNhcnQ8L3N0cmluZz4KICAgIDxrZXk+ZGVhdXRob3JpemVNYWNoaW5lPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL2RlYXV0aG9yaXplTWFjaGluZTwvc3RyaW5nPgogICAgPGtleT5tYWNoaW5lQXV0aG9yaXphdGlvbkluZm88L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmFzdEZpbmFuY2Uud29hL3dhL21hY2hpbmVBdXRob3JpemF0aW9uSW5mbzwvc3RyaW5nPgogICAgPGtleT5tb2RpZnlBY2NvdW50PC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL2FjY291bnRTdW1tYXJ5PC9zdHJpbmc+CiAgICA8a2V5PnBlbmRpbmdTb25nczwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9wZW5kaW5nU29uZ3M8L3N0cmluZz4KICAgIDxrZXk+c2lnbnVwPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL3NpZ251cFdpemFyZDwvc3RyaW5nPgogICAgPGtleT5zb25nRG93bmxvYWREb25lPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZhc3RGaW5hbmNlLndvYS93YS9zb25nRG93bmxvYWREb25lPC9zdHJpbmc+CiAgICA8a2V5PmZvcmdvdHRlblBhc3N3b3JkPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL2lGb3Jnb3Q8L3N0cmluZz4KICAgIDxrZXk+bXlJbmZvPC9rZXk+PHN0cmluZz5odHRwczovL215aW5mby5hcHBsZS5jb20vPC9zdHJpbmc+CiAgICA8a2V5Pm5vQU9MQWNjb3VudHM8L2tleT48ZmFsc2UvPgogICAgPGtleT51cGxvYWRQdWJsaXNoZWRQbGF5bGlzdDwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS91cGxvYWRQdWJsaXNoZWRQbGF5TGlzdDwvc3RyaW5nPgogICAgPGtleT5sb2dvdXQ8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvbG9nb3V0PC9zdHJpbmc+CiAgICA8a2V5PmFkZFRvQ2FydDwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9hZGRUb0NhcnQ8L3N0cmluZz4KICAgIDxrZXk+cmVtb3ZlRnJvbUNhcnQ8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvcmVtb3ZlRnJvbUNhcnQ8L3N0cmluZz4KICAgIDxrZXk+c2hvcHBpbmdDYXJ0PC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL3Nob3BwaW5nQ2FydDwvc3RyaW5nPgogICAgPGtleT5iY1VSTHM8L2tleT48YXJyYXk+PHN0cmluZz5odHRwOi8vLnBob2Jvcy5hcHBsZS5jb208L3N0cmluZz48c3RyaW5nPmh0dHA6Ly93d3cuYXRkbXQuY29tPC9zdHJpbmc+PC9hcnJheT4KICAgIDxrZXk+dXBncmFkZVBob25lPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL3VwZ3JhZGVQaG9uZTwvc3RyaW5nPgogICAgPGtleT51cGdyYWRlRHJtPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL3VwZ3JhZGVEcm08L3N0cmluZz4KICAgIDxrZXk+cmVwb3J0UG9kY2FzdDwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9yZXBvcnRQb2RjYXN0PC9zdHJpbmc+CiAgICA8a2V5PmdpZnRQbGF5bGlzdDwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9naWZ0U29uZ3NXaXphcmQ8L3N0cmluZz4KICAgIDxrZXk+Z2l2ZS1wbGF5bGlzdDwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9naWZ0U29uZ3NXaXphcmQ8L3N0cmluZz4KICAgIDxrZXk+Y2hlY2stZG93bmxvYWQtcXVldWU8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvY2hlY2tEb3dubG9hZFF1ZXVlPC9zdHJpbmc+CiAgICA8a2V5PnNldC1hdXRvLWRvd25sb2FkPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL3NldEF1dG9Eb3dubG9hZDwvc3RyaW5nPgogICAgPGtleT5uZXctaXBvZC11c2VyPC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL2lQb2RSZWdpc3RyYXRpb248L3N0cmluZz4KICAgIDxrZXk+bmV3LXR2LXVzZXI8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvaVRWUmVnaXN0cmF0aW9uPC9zdHJpbmc+CiAgICA8a2V5Pm1kNS1taXNtYXRjaDwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9tZDVNaXNtYXRjaDwvc3RyaW5nPgogICAgPGtleT5yZXBvcnQtZXJyb3I8L2tleT48c3RyaW5nPmh0dHBzOi8vcGhvYm9zLmFwcGxlLmNvbS9XZWJPYmplY3RzL01aRmluYW5jZS53b2Evd2EvcmVwb3J0RXJyb3JGcm9tQ2xpZW50PC9zdHJpbmc+CiAgICA8a2V5PnVwZGF0ZUFzc2V0PC9rZXk+PHN0cmluZz5odHRwczovL3Bob2Jvcy5hcHBsZS5jb20vV2ViT2JqZWN0cy9NWkZpbmFuY2Uud29hL3dhL3VwZGF0ZUFzc2V0PC9zdHJpbmc+CiAgICA8a2V5PmNyZWF0ZS10b2tlbjwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9jcmVhdGVUb2tlbjwvc3RyaW5nPgogICAgPGtleT5jcmVhdGUtc2Vzc2lvbjwva2V5PjxzdHJpbmc+aHR0cHM6Ly9waG9ib3MuYXBwbGUuY29tL1dlYk9iamVjdHMvTVpGaW5hbmNlLndvYS93YS9jcmVhdGVTZXNzaW9uPC9zdHJpbmc+CiAgICAKCiAgICAgIAogICAgICAKICAgIDwvZGljdD4KICA8L3BsaXN0PgoKCg==</data>"
"        </dict>"
"</plist>";

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
    SecTrustRef trust;
    SecCertificateRef leaf, root;
    SecPolicyRef policy;
    CFDataRef urlBagData;
	CFDictionaryRef urlBagDict;

    isnt(urlBagData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, url_bag, sizeof(url_bag), kCFAllocatorNull), NULL,
		"load url bag");
    isnt(urlBagDict = CFPropertyListCreateWithData(kCFAllocatorDefault, urlBagData, kCFPropertyListImmutable, NULL, NULL), NULL,
		"parse url bag");
	CFReleaseSafe(urlBagData);
    CFArrayRef certs_data = CFDictionaryGetValue(urlBagDict, CFSTR("certs"));
    CFDataRef cert_data = CFArrayGetValueAtIndex(certs_data, 0);
    isnt(leaf = SecCertificateCreateWithData(kCFAllocatorDefault, cert_data), NULL, "create leaf");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, sITunesStoreRootCertificate, sizeof(sITunesStoreRootCertificate)), NULL, "create root");

    CFArrayRef certs = CFArrayCreate(kCFAllocatorDefault, (const void **)&leaf, 1, NULL);
    CFDataRef signature = CFDictionaryGetValue(urlBagDict, CFSTR("signature"));
	CFDataRef bag = CFDictionaryGetValue(urlBagDict, CFSTR("bag"));
    unsigned char sha1_hash[CC_SHA1_DIGEST_LENGTH];
    CCDigest(kCCDigestSHA1, CFDataGetBytePtr(bag), CFDataGetLength(bag), sha1_hash);

    isnt(policy = SecPolicyCreateBasicX509(), NULL, "create policy instance");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust for leaf");

    SecTrustResultType trustResult;
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    TODO: {
        todo("Only works in !NO_SERVER setup");
        is_status(trustResult, kSecTrustResultOtherError,
            "trust is kSecTrustResultOtherError");
    }
	SecKeyRef pub_key_leaf;
	isnt(pub_key_leaf = SecTrustCopyPublicKey(trust), NULL, "get leaf pub key");
	ok_status(SecKeyRawVerify(pub_key_leaf, kSecPaddingPKCS1SHA1, sha1_hash, sizeof(sha1_hash), CFDataGetBytePtr(signature), CFDataGetLength(signature)),
		"verify signature on bag");

    CFReleaseSafe(pub_key_leaf);
	CFReleaseSafe(urlBagDict);
    CFReleaseSafe(certs);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(leaf);
    CFReleaseSafe(root);
}

int si_21_sectrust_asr(int argc, char *const *argv)
{
    plan_tests(10);


    tests();

    return 0;
}
