CC = /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc-4.2
SYSROOT = /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.0.Internal.sdk
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks

certattrs: EAPCertificateUtil.c
	cc -Wall -DTEST_EAPSecCertificateCopyAttributesDictionary -g -o certattrs EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c -framework Security -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

certattrsp: EAPCertificateUtil.c
	cc -Wall -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk -arch armv6 -DTEST_EAPSecCertificateCopyAttributesDictionary -g -o certattrsp EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c -framework SystemConfiguration -framework Security -framework CoreFoundation -F/System/Library/PrivateFrameworks

identity: EAPCertificateUtil.c
	cc -Wall -DTEST_EAPSecIdentity -g -o identity EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c -framework Security -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

identity_trust_chain: EAPCertificateUtil.c
	cc -Wall -DTEST_EAPSecIdentityHandleCreateSecIdentityTrustChain -g -o identity_trust_chain EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c -framework Security -framework CoreFoundation -F/System/Library/PrivateFrameworks

identity_trust_chainp: EAPCertificateUtil.c
	cc -arch armv6 -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk -Wall -DTEST_EAPSecIdentityHandleCreateSecIdentityTrustChain -g -o identity_trust_chainp EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c -framework Security -framework SystemConfiguration -framework CoreFoundation -F/System/Library/PrivateFrameworks

identityp: EAPCertificateUtil.c
	cc -Wall -arch armv6 -I.. -DTEST_EAPSecIdentity -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk -g -o identityp EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c -framework Security -framework CoreFoundation -F/System/Library/PrivateFrameworks

mschap: mschap.c
	cc -Wall -g -DTEST_MSCHAP -o mschap mschap.c printdata.c DESSupport.c -lcrypto

keychain: EAPKeychainUtil.c
	cc -Wall -DTEST_EAPKEYCHAINUTIL -g -o keychain EAPKeychainUtil.c myCFUtil.c EAPSecurity.c -framework Security -framework CoreFoundation

item: EAPKeychainUtil.c
	cc -Wall -DTEST_EAPKEYCHAINUTIL -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk -arch armv6 -g -o item EAPKeychainUtil.c EAPSecurity.c myCFUtil.c -framework Security -framework CoreFoundation

trustx: EAPTLSUtil.c
	cc -Wall -I. -DTEST_TRUST_EXCEPTIONS -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk -arch armv6 -g -o trustx EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c printdata.c myCFUtil.c -framework Security -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

eapsectrust: EAPTLSUtil.c
	cc -Wall -I. -DTEST_SEC_TRUST -isysroot /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS2.0.Internal.sdk -arch armv6 -g -o eapsectrust EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c printdata.c myCFUtil.c -framework Security -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

test_server_names: EAPTLSUtil.c
	cc -Wall -I. -DTEST_SERVER_NAMES -g -o test_server_names EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c printdata.c myCFUtil.c -framework Security -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

simtlv: eapsim_plugin.c
	cc -g -Wall -I. -DTEST_TLVLIST_PARSE -o simtlv eapsim_plugin.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

rand_dups: eapsim_plugin.c
	cc -g -Wall -I. -DTEST_RAND_DUPS -o rand_dups fips186prf.c fr_sha1.c myCFUtil.c eapsim_plugin.c printdata.c -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

sim_crypto: eapsim_plugin.c
	cc -g -Wall -I. -DTEST_SIM_CRYPTO -o sim_crypto eapsim_plugin.c SIMAccess.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

sim_set_version: eapsim_plugin.c
	cc -g -Wall -I. -DTEST_SET_VERSION_LIST -o sim_set_version eapsim_plugin.c myCFUtil.c fips186prf.c fr_sha1.c printdata.c -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

fips186prf: fips186prf.c
	cc -o fips186prf -g -Wall -I. -DTEST_FIPS186PRF fips186prf.c fr_sha1.c

fips186prf_p: fips186prf.c
	$(CC) -isysroot $(SYSROOT) -arch armv6 -o fips186prf_p -g -Wall -I. -DTEST_FIPS186PRF fips186prf.c fr_sha1.c

sim_crypto_p: eapsim_plugin.c
	$(CC) -arch armv6 -isysroot $(SYSROOT) -g -Wall -I. -DTEST_SIM_CRYPTO -o sim_crypto_p eapsim_plugin.c SIMAccess.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

simtlv_p: eapsim_plugin.c
	$(CC) -arch armv6 -isysroot $(SYSROOT) -g -Wall -I. -DTEST_TLVLIST_PARSE -o simtlv_p eapsim_plugin.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

siminfo: eapsim_plugin.c
	$(CC) -arch armv6 -isysroot $(SYSROOT) -I. -g -Wall -DTEST_SIM_INFO -o siminfo eapsim_plugin.c SIMAccess.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c -framework CoreFoundation -framework SystemConfiguration -framework CoreTelephony $(PF_INC)
	codesign -s - -f siminfo

SIMAccess: SIMAccess.c
	$(CC) -DTEST_SIMACCESS -arch armv6 -isysroot $(SYSROOT) -I. -g -Wall -o SIMAccess SIMAccess.c printdata.c -framework CoreFoundation -framework CoreTelephony $(PF_INC)
	codesign -s - -f SIMAccess

verify_server: EAPTLSUtil.c
	cc -g -DTEST_EAPTLSVerifyServerCertificateChain -o verify_server EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c myCFUtil.c printdata.c -framework CoreFoundation -framework Security -F/System/Library/PrivateFrameworks

eapol_socket: eapol_socket.c
	cc -g -I../EAP8021X.fproj -DTEST_EAPOL_SOCKET -o eapol_socket eapol_socket.c

clean:
	rm -rf *.dSYM/
	rm -f *~
	rm -f simtlv simtlv_p rand_dups sim_crypto sim_crypto_p sim_set_version fips186prf fips186prf_p siminfo SIMAccess verify_server keychain eapol_socket certattrs certattrsp
