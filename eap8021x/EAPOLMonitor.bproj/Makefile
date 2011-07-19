EAPOLMonitor: eapol_monitor.c
	cc -Wall -DTEST_PLUGIN -o EAPOLMonitor eapol_monitor.c -g -I../EAP8021X.fproj -framework EAP8021X -framework CoreFoundation -framework SystemConfiguration -F/System/Library/PrivateFrameworks

clean:
	rm -rf *.dSYM/
	rm -f EAPOLMonitor *~