#!/bin/sh

mkdir -p ${HEIMDAL_TMPDIR}/GSS

for a in \
	${SRCROOT}/lib/gssapi/gssapi/gssapi.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_krb5.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_netlogon.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_ntlm.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_scram.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_spnego.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_spi.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_oid.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_plugin.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_private.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_protos.h \
	${SRCROOT}/lib/gssapi/gssapi/gssapi_apple.h \
     ;
do
     perl ${SRCROOT}/packages/mac/Heimdal-ify.pl GSS "$a" ${HEIMDAL_TMPDIR}/GSS || exit 1
done

for a in \
	${HEIMDAL_TMPDIR}/gssapi_rewrite.h \
	;
do
     perl ${SRCROOT}/packages/mac/Heimdal-ify.pl GSS "$a" ${HEIMDAL_TMPDIR}/GSS || exit 1
done
