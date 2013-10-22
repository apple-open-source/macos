# Default platform uses the native SDK.
# To build for Mac OS X using internal SDK, use 'make PLATFORM=macosx <target>'
# To build for iOS, use 'make PLATFORM=iphoneos <target>'

ifeq ($(PLATFORM),iphoneos)
# iOS internal SDK
CORETELEPHONY=-framework CoreTelephony
ARCHS=armv7
endif

ifeq ($(PLATFORM),macosx)
# Mac OS X internal SDK
ARCHS=i386 x86_64
CORETELEPHONY=
endif

ifeq ($(PLATFORM),)
# Mac OS X native SDK
ARCHS=i386 x86_64
CORETELEPHONY=
CC = cc
SYSROOT = /
PF_INC = -F/System/Library/PrivateFrameworks
else
# Mac OS X or iOS internal SDK
SDK=$(PLATFORM).internal
SYSROOT=$(shell xcodebuild -version -sdk $(SDK) Path)
CC = xcrun -sdk $(SDK) cc
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks
endif

ARCH_FLAGS=$(foreach a,$(ARCHS),-arch $(a))

SCPRIV += -DUSE_SYSTEMCONFIGURATION_PRIVATE_HEADERS=1

certattrs: EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c EAPUtil.c EAPClientModule.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -DTEST_EAPSecCertificateCopyAttributesDictionary $(PF_INC) -framework Security -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^

identity: EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c EAPUtil.c EAPClientModule.c EAPLog.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c
	$(CC) $(ARCH_FLAGS) $(SCPRIV) -isysroot $(SYSROOT) -Wall -I.. -DTEST_EAPSecIdentity $(PF_INC) -framework Security -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^
	xcrun -sdk $(SDK) codesign -s - -f --entitlements ../eapolclient.tproj/entitlements.plist $@


identity_trust_chain: EAPCertificateUtil.c myCFUtil.c EAPSecurity.c EAPTLSUtil.c printdata.c EAPUtil.c EAPClientModule.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -DTEST_EAPSecIdentityHandleCreateSecIdentityTrustChain $(PF_INC) -framework Security -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^

mschap: mschap.c printdata.c DESSupport.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -DTEST_MSCHAP -framework CoreFoundation -framework SystemConfiguration -lcrypto -g -o $@ $^

keychain: EAPKeychainUtil.c myCFUtil.c EAPSecurity.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -DTEST_EAPKEYCHAINUTIL $(PF_INC) -framework Security -framework CoreFoundation -g -o $@ $^

item: EAPKeychainUtil.c EAPSecurity.c myCFUtil.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -DTEST_EAPKEYCHAINUTIL $(PF_INC) -framework Security -framework CoreFoundation -g -o $@ $^

trustx: EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c printdata.c myCFUtil.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DTEST_TRUST_EXCEPTIONS $(PF_INC) -framework Security -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^

eapsectrust: EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c printdata.c myCFUtil.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DTEST_SEC_TRUST $(PF_INC) -framework Security -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^

test_server_names: EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c printdata.c myCFUtil.c EAPUtil.c EAPClientModule.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DTEST_SERVER_NAMES $(PF_INC) -framework Security -framework CoreFoundation -framework SystemConfiguration -g -o $@ $^

simtlv: EAPSIMAKAUtil.c EAPUtil.c EAPClientModule.c printdata.c EAPLog.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c EAPSecurity.c myCFUtil.c fips186prf.c fr_sha1.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -Wcast-align -I. -DEAPSIMAKA_PACKET_DUMP -DTEST_TLVLIST_PARSE $(PF_INC) -framework CoreFoundation -framework SystemConfiguration $(CORETELEPHONY) -framework Security -g -o $@ $^


rand_dups: eapsim_plugin.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c SIMAccess.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c EAPUtil.c EAPClientModule.c EAPSecurity.c EAPLog.c EAPSIMAKAUtil.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DTEST_RAND_DUPS $(PF_INC) -framework CoreFoundation -framework SystemConfiguration $(CORETELEPHONY) -framework Security -g -o $@ $^

sim_crypto: fips186prf.c fr_sha1.c myCFUtil.c printdata.c EAPUtil.c EAPClientModule.c EAPSecurity.c EAPSIMAKAUtil.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c EAPLog.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DEAPSIMAKA_PACKET_DUMP -DTEST_SIM_CRYPTO $(PF_INC) -framework CoreFoundation -framework SystemConfiguration -framework Security $(CORETELEPHONY) -g -o $@ $^

sim_set_version: eapsim_plugin.c myCFUtil.c fips186prf.c fr_sha1.c printdata.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c EAPUtil.c EAPClientModule.c EAPSecurity.c SIMAccess.c EAPSIMAKAUtil.c EAPLog.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DTEST_SET_VERSION_LIST $(PF_INC) -framework CoreFoundation -framework SystemConfiguration -framework Security $(CORETELEPHONY) -g -o $@ $^

fips186prf: fips186prf.c fr_sha1.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -Wall -I. -DTEST_FIPS186PRF -g -o $@ $^

siminfo: eapsim_plugin.c SIMAccess.c fips186prf.c fr_sha1.c myCFUtil.c printdata.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c EAPUtil.c EAPClientModule.c EAPSecurity.c EAPSIMAKAUtil.c EAPLog.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) $(SCPRIV) -Wall -I. -DTEST_SIM_INFO $(PF_INC) -framework CoreFoundation -framework SystemConfiguration -framework Security -framework CoreTelephony -g -o $@ $^
	xcrun -sdk $(SDK) codesign -s - -f --entitlements ../eapolclient.tproj/entitlements.plist $@

SIMAccess: SIMAccess.c printdata.c myCFUtil.c EAPSIMAKAUtil.c EAPUtil.c EAPClientModule.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c EAPSecurity.c fips186prf.c fr_sha1.c EAPLog.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) $(SCPRIV) -Wall -I. -DTARGET_OS_EMBEDDED -DTEST_SIMACCESS $(PF_INC) -framework CoreFoundation $(CORETELEPHONY) -framework SystemConfiguration -framework Security -g -o $@ $^
	xcrun -sdk $(SDK) codesign -s - -f --entitlements ../eapolclient.tproj/entitlements.plist $@

verify_server: EAPTLSUtil.c EAPCertificateUtil.c EAPSecurity.c myCFUtil.c printdata.c EAPUtil.c EAPClientModule.c EAPSIMAKAPersistentState.c EAPKeychainUtil.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -DTEST_EAPTLSVerifyServerCertificateChain $(PF_INC) -framework CoreFoundation -framework Security -g -o $@ $^

eapol_socket: eapol_socket.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -I../EAP8021X.fproj -DTEST_EAPOL_SOCKET -g -o $@ $^

simaka_persist: EAPSIMAKAPersistentState.c EAPKeychainUtil.c myCFUtil.c EAPSecurity.c printdata.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -DTEST_EAPSIMAKA_PERSISTENT_STATE $(PF_INC) -framework CoreFoundation -framework Security -framework SystemConfiguration -g -o $@ $^
	export CODESIGN_ALLOCATE=$(shell xcrun -sdk $(SDK) -find codesign_allocate)
	xcrun -sdk $(SDK) codesign -s - -f --entitlements ../eapolclient.tproj/entitlements.plist $@

test_eapaka: eapaka_plugin.c EAPSIMAKAUtil.c SIMAccess.c EAPUtil.c EAPClientModule.c EAPSIMAKAPersistentState.c fips186prf.c printdata.c myCFUtil.c EAPKeychainUtil.c EAPSecurity.c fr_sha1.c EAPLog.c
	$(CC) $(ARCH_FLAGS) -isysroot $(SYSROOT) -DEAPSIMAKA_PACKET_DUMP -DTEST_EAPAKA_PLUGIN $(PF_INC) -framework CoreFoundation -framework Security -framework SystemConfiguration -g -o $@ $^

clean:
	rm -rf *.dSYM/
	rm -f *~
	rm -f certattrs identity identity_trust_chain mschap keychain item trustx eapsectrust test_server_names simtlv rand_dups sim_crypto sim_set_version fips186prf siminfo SIMAccess verify_server eapol_socket simaka_persist test_eapaka
