#!/bin/sh

mkdir -p ${HEIMDAL_TMPDIR}/Heimdal

for a in \
	${HEIMDAL_TMPDIR}/asn1_err.h \
	${HEIMDAL_TMPDIR}/krb5_err.h \
	${HEIMDAL_TMPDIR}/krb_err.h \
	${HEIMDAL_TMPDIR}/k524_err.h \
	${HEIMDAL_TMPDIR}/heim_err.h \
	${HEIMDAL_TMPDIR}/wind_err.h \
	${HEIMDAL_TMPDIR}/gkrb5_err.h \
	${HEIMDAL_TMPDIR}/hx509_err.h \
	${HEIMDAL_TMPDIR}/hdb_err.h \
	${HEIMDAL_TMPDIR}/hc_err.h \
	;
do
     perl ${SRCROOT}/packages/mac/Heimdal-ify.pl Heimdal "$a" ${HEIMDAL_TMPDIR}/Heimdal || exit 1
done
