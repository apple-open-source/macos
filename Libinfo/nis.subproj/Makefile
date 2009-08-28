Project = nis
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

HFILES = ypclnt.h ypinternal.h yp_prot.h

CFILES = getdomainname.c getnetgrent.c innetgr.c setdomainname.c\
         xdr_domainname.c xdr_keydat.c xdr_mapname.c xdr_peername.c\
         xdr_valdat.c xdr_ypbind_binding.c xdr_ypbind_resp.c\
         xdr_ypbind_resptype.c xdr_ypbind_setdom.c xdr_ypmaplist.c\
         xdr_ypreq_key.c xdr_ypreq_nokey.c xdr_ypresp_all.c\
         xdr_ypresp_key_val.c xdr_ypresp_maplist.c xdr_ypresp_master.c\
         xdr_ypresp_order.c xdr_ypresp_val.c xdr_ypstat.c\
         yperr_string.c ypmatch_cache.c yppasswdd_xdr.c ypprot_err.c\
         yp_all.c yp_bind.c yp_first.c yp_get_default_domain.c\
         yp_maplist.c yp_master.c yp_order.c

MANPAGES = yp_all.3 yp_bind.3 yp_first.3\
            yp_get_default_domain.3 yp_master.3 yp_match.3 yp_next.3\
            yp_order.3 yp_unbind.3 ypclnt.3 yperr_string.3 ypprot_err.3\
            yp.8

Install_Headers = ypclnt.h yp_prot.h
Install_Headers_Directory =  /usr/include/rpcsvc

Extra_CC_Flags = -Wall -fno-common -I. \
	-I../gen.subproj -I../lookup.subproj \
	-D__DARWIN_NON_CANCELABLE=1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make
