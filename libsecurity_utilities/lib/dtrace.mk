$(DERIVED_SRC)/utilities_dtrace.h: $(SRCROOT)/lib/security_utilities.d
	/usr/sbin/dtrace -h -C -s $(SRCROOT)/lib/security_utilities.d -o $(DERIVED_SRC)/utilities_dtrace.h
