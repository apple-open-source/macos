
lookupDSLocalKDC: lookupDSLocalKDC.c
	cc -gdwarf-2 -c -F/System/Library/PrivateFrameworks -DLOOKUP_IN_DS -DSTANDALONE lookupDSLocalKDC.c
	cc -o lookupDSLocalKDC -F/System/Library/PrivateFrameworks -framework CoreFoundation -framework OpenDirectory lookupDSLocalKDC.o
	dsymutil -o lookupDSLocalKDC.dSYM lookupDSLocalKDC
