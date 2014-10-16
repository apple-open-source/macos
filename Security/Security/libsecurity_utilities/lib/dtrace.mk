utilities_dtrace.h: $(PROJECT_DIR)/lib/security_utilities.d
	if [ ! -f "$(DERIVED_SRC)/utilities_dtrace.h" ]; then /usr/sbin/dtrace -h -C -s $(PROJECT_DIR)/lib/security_utilities.d -o $(DERIVED_SRC)/utilities_dtrace.h ; fi
