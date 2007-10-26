NEW_RC_ARCHS = $(filter-out %64,$(RC_ARCHS))
NEW_RC_CFLAGS = $(foreach A,$(NEW_RC_ARCHS),-arch $(A)) $(RC_NONARCH_CFLAGS)
_no_target_:
ifneq '$(NEW_RC_ARCHS)' ''
	RC_ARCHS='$(NEW_RC_ARCHS)' RC_CFLAGS='$(NEW_RC_CFLAGS)' RC_x86_64=no RC_ppc64=no make -f Makefile RC_ARCHS='$(NEW_RC_ARCHS)' RC_CFLAGS='$(NEW_RC_CFLAGS)' RC_x86_64=no RC_ppc64=no 
else
	@echo No 32-bit architectures to build
endif

.DEFAULT:
ifneq '$(NEW_RC_ARCHS)' ''
	RC_ARCHS='$(NEW_RC_ARCHS)' RC_CFLAGS='$(NEW_RC_CFLAGS)' RC_x86_64=no RC_ppc64=no make -f Makefile $@ RC_ARCHS='$(NEW_RC_ARCHS)' RC_CFLAGS='$(NEW_RC_CFLAGS)' RC_x86_64=no RC_ppc64=no 
else
	@echo No 32-bit architectures to build
endif
