NCK_INCLUDES=-I$(top_srcdir)/ncklib -I$(top_srcdir)/ncklib/include/$(target_os)
NCK_DEFINES=-DNCK

# It would seem that putting the defines in CFLAGS is a hack.
AM_CPPFLAGS+=$(NCK_INCLUDES)
AM_CFLAGS+=$(NCK_DEFINES)

NCK_IDL_FLAGS=$(IDL_INCLUDES) -keep c_source -no_cpp -v -no_mepv @DCETHREADINCLUDES@ $(TARGET_OS)
