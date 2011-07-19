CC = /Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/gcc-4.2
SYSROOT = /Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS3.0.Internal.sdk
PF_INC = -F$(SYSROOT)/System/Library/PrivateFrameworks

wireless: wireless.c
	cc -arch i386 -I../EAP8021X.fproj -DTEST_WIRELESS -g -o wireless wireless.c -F/System/Library/PrivateFrameworks -framework Apple80211 -framework CoreFoundation -framework EAP8021X

wireless_p: wireless.c
	$(CC) -arch armv6 -isysroot $(SYSROOT) -I../EAP8021X.fproj -DTEST_WIRELESS -g -o wireless_p wireless.c $(PF_INC) -framework CoreFoundation -framework EAP8021X -lio80211

Dialogue: Dialogue.c
	cc -Wall -DTEST_DIALOGUE -g -o Dialogue Dialogue.c mylog.c -I../EAP8021X.fproj ../EAP8021X.fproj/myCFUtil.c -framework CoreFoundation -framework SystemConfiguration -framework EAP8021X -F/System/Library/PrivateFrameworks -framework Security

InterestNotification: InterestNotification.c
	cc -DTEST_INTERESTNOTIFICATION -Wall -g -o InterestNotification InterestNotification.c -framework CoreFoundation -framework IOKit

InterestNotification_p: InterestNotification.c
	$(CC) -arch armv6 -isysroot $(SYSROOT) -I../EAP8021X.fproj -DTEST_INTERESTNOTIFICATION -Wall -g -o InterestNotification_p InterestNotification.c -framework CoreFoundation -framework IOKit

clean:
	rm -f Dialogue wireless_p wireless InterestNotification InterestNotification_p
	rm -rf *.dSYM/
