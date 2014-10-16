$(DERIVED_SRC)/securityd_dtrace.h: $(SRCROOT)/src/securityd.d
	/usr/sbin/dtrace -h -C -s $(SRCROOT)/src/securityd.d -o $(DERIVED_SRC)/securityd_dtrace.h
