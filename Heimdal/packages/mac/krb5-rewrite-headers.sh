#!/bin/sh

mkdir -p ${HEIMDAL_TMPDIR}/Heimdal

for a in \
	${SRCROOT}/lib/com_err/com_err.h \
	${SRCROOT}/lib/com_err/com_right.h \
	${SRCROOT}/base/heimbase.h \
	${SRCROOT}/lib/krb5/krb5.h \
	${SRCROOT}/lib/krb5/config_plugin.h \
	${SRCROOT}/lib/hdb/hdb.h \
	${SRCROOT}/lib/hdb/hdb-protos.h \
	${SRCROOT}/lib/hx509/hx509.h \
	${SRCROOT}/lib/hx509/hx509-protos.h \
	${SRCROOT}/kdc/windc_plugin.h \
     ;
do
     perl ${SRCROOT}/packages/mac/Heimdal-ify.pl Heimdal "$a" ${HEIMDAL_TMPDIR}/Heimdal || exit 1
done
