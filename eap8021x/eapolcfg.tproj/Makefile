eapolcfg: eapolcfg.c
	cc -Wall -g -o eapolcfg eapolcfg.c -framework EAP8021X -F/System/Library/PrivateFrameworks -framework CoreFoundation -framework SystemConfiguration -framework Security -I../EAP8021X.fproj

clean:
	rm -f eapolcfg
	rm -rf *.dSYM/