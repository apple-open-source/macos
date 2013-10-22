perl ../../cf/make-proto.pl -a -x ../../packages/mac/framework/gss.json -E GSS_LIB -q -P comment -o gssapi/gssapi_protos.h {mech,krb5,spnego,ntlm,cf}/*.c
perl ../../cf/make-proto.pl -a -x ../../packages/mac/framework/gss-apple.json -E GSS_LIB -q -P comment -o gssapi/gssapi_apple.h {mech,krb5,spnego,ntlm,cf}/*.c
perl ../../cf/make-proto.pl -a -x ../../packages/mac/framework/gss.json -q -P comment -a -p gssapi/gssapi_private.h mech/gss_*.c mech/cred.c
perl ../../cf/make-proto.pl -q -P comment -a -p krb5/gsskrb5-private.h krb5/*.c


perl ./gen-oid.pl -b base -h ./oid.txt > ./gssapi/gssapi_oid.h
perl ./gen-oid.pl -b base ./oid.txt > ./mech/gss_oid.c