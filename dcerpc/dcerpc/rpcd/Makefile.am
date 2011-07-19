## Process this file with automake to produce Makefile.in
include $(top_srcdir)/include.mk
C_FLAGS=-Drpcd_c_database_name_prefix1=\"$(localstatedir)/run/\" -I$(top_srcdir)/ncklib -I$(top_srcdir)/ncklib/include/$(target_os)
AM_CPPFLAGS+=$(C_FLAGS) -DDCETHREAD_ENFORCE_API

RPCD_IDLFLAGS=$(IDL_INCLUDES) -I. -keep c_source -v -no_mepv -cepv @DCETHREADINCLUDES@

dcom_sources=objex_sstub.c

sbin_PROGRAMS=dcerpcd

dcerpcd_SOURCES=dsm.c dsm_unix.c rpcd.c rpcddb.c rpcdep.c rpcdepdb.c rpcdsliv.c rpcdutil.c ep_sstub.c $(rpcd_dcom_sources)

noinst_HEADERS=dsm_p.h rpcddb.h rpcdepdb.h rpcdepdbp.h rpcdp.h rpcdutil.h dsm.idl

dcerpcd_LDADD=../dcelib/libdcerpc.la ../uuid/libuuid.la ../libdcethread/libdcethread.la
dcerpcd_LDFLAGS=@LTLDFLAGS@ $(NAF_NP_LDFLAGS)

BUILT_SOURCES=ep_sstub.c dsm.h
CLEANFILES=objex.h object_sstub.c ep_sstub.c dsm.h

if ENABLE_DCOM
objex_sstub.c: $(IDL_INCLUDE_DIR)/objex.idl
	$(IDL) $(RPCD_IDLFLAGS) -client none $<
endif

ep_sstub.c: $(IDL_INCLUDE_DIR)/ep.idl
	$(IDL) $(RPCD_IDLFLAGS) -client none $<

dsm.h:	dsm.idl
	$(IDL) $(RPCD_IDLFLAGS) -client none -server none $<

install-data-local:
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/rpc
	$(mkinstalldirs) $(DESTDIR)$(localstatedir)/run

regdir = $(sysconfdir)
reg_DATA = dcerpcd.reg
