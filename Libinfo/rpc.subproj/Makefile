Project = rpc
ProductType = staticlib
Install_Dir = /scratch
BuildDebug = YES
BuildProfile = YES

HFILES = auth.h auth_unix.h clnt.h pmap_clnt.h pmap_prot.h pmap_rmt.h\
         rpc.h rpc_msg.h svc.h svc_auth.h types.h xdr.h pmap_wakeup.h

CFILES = auth_none.c auth_unix.c authunix_prot.c bindresvport.c\
         clnt_generic.c clnt_perror.c clnt_raw.c clnt_simple.c\
         clnt_tcp.c clnt_udp.c get_myaddress.c pmap_clnt.c\
         pmap_getmaps.c pmap_getport.c pmap_prot.c pmap_prot2.c\
         pmap_rmt.c rpc_callmsg.c rpc_commondata.c rpc_dtablesize.c\
         rpc_prot.c svc.c svc_auth.c svc_auth_unix.c svc_raw.c\
         svc_run.c svc_simple.c svc_tcp.c getrpcent.c svc_udp.c xdr.c\
         xdr_array.c xdr_float.c xdr_mem.c xdr_rec.c xdr_reference.c\
         xdr_sizeof.c xdr_stdio.c getrpcport.c pmap_wakeup.c

MANPAGES = bindresvport.3 getrpcent.3 getrpcport.3 rpc.3 xdr.3

Install_Headers_Directory = /usr/include/rpc
Install_Headers = auth.h auth_unix.h clnt.h pmap_clnt.h pmap_prot.h\
                 pmap_rmt.h rpc.h rpc_msg.h svc.h svc_auth.h types.h\
                 xdr.h

Extra_CC_Flags = -Wall -fno-common \
	-I../gen.subproj -I../lookup.subproj \
	-D__DARWIN_NON_CANCELABLE=1

include $(MAKEFILEPATH)/CoreOS/ReleaseControl/BSDCommon.make

after_install:
	@for LINK in getrpcbyname.3 getrpcbynumber.3 \
		endrpcent.3 setrpcent.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/getrpcent.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done 
	@for LINK in auth_destroy.3 authnone_create.3 authunix_create.3 \
		authunix_create_default.3 callrpc.3 clnt_broadcast.3 \
		clnt_call.3 clnt_control.3 clnt_create.3 clnt_destroy.3	\
		clnt_freeres.3 clnt_geterr.3 clnt_pcreateerror.3 \
		clnt_perrno.3 clnt_perror.3 clnt_spcreateerror.3 \
		clnt_sperrno.3 clnt_sperror.3 clntraw_create.3 \
		clnttcp_create.3 clntudp_bufcreate.3 clntudp_create.3 \
		get_myaddress.3 pmap_getmaps.3 pmap_getport.3 pmap_rmtcall.3 \
		pmap_set.3 pmap_unset.3 regsterrpc.3 rpc_createerr.3 \
		svc_destroy.3 svc_fds.3 svc_fdset.3 svc_getargs.3 \
		svc_getcaller.3 svc_getreg.3 svc_getregset.3 svc_register.3 \
		svc_run.3 svc_sendreply.3 svc_unregister.3 svcerr_auth.3 \
		svcerr_decode.3 svcerr_noproc.3 svcerr_noprog.3 \
		svcerr_progvers.3 svcerr_systemerr.3 svcerr_weakauth.3 \
		svcfd_create.3 svcraw_create.3 svctcp_create.3 \
		svcudp_bufcreate.3 xdr_accepted_reply.3 xdr_authunix_parms.3 \
		xdr_callhdr.3 xdr_callmsg.3 xdr_opaque_auth.3 xdr_pmap.3 \
		xdr_pmaplist.3 xdr_rejected_reply.3 xdr_replymsg.3 \
		xprt_register.3 xprt_unregister.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/rpc.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
	@for LINK in xdr_array.3 xdr_bool.3 xdr_bytes.3 xdr_char.3 \
		xdr_destroy.3 xdr_double.3 xdr_enum.3 xdr_float.3 xdr_free.3 \
		xdr_getpos.3 xdr_inline.3 xdr_int.3 xdr_long.3 \
		xdrmem_create.3 xdr_opaque.3 xdr_pointer.3 xdrrec_create.3 \
		xdrrec_endofrecord.3 xdrrec_eof.3 xdrrec_skiprecord.3 \
		xdr_reference.3 xdr_setpos.3 xdr_short.3 xdrstdio_create.3 \
		xdr_string.3 xdr_u_char.3 xdr_u_long.3 xdr_u_short.3 \
		xdr_union.3 xdr_vector.3 xdr_void.3 xdr_wrapstring.3 ; do \
		$(LN) -f "$(DSTROOT)/usr/share/man/man3/xdr.3" \
			"$(DSTROOT)/usr/share/man/man3/$${LINK}" ; \
	done
