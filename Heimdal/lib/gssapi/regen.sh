perl ../../cf/make-proto.pl -a -x ../../packages/mac/framework/gss.json -E GSS_LIB -q -P comment -o gssapi/gssapi_protos.h {mech,krb5,spnego,ntlm}/*.c
perl ../../cf/make-proto.pl -a -x ../../packages/mac/framework/gss-apple.json -E GSS_LIB -q -P comment -o gssapi/gssapi_apple.h {mech,krb5,spnego,ntlm}/*.c
perl ../../cf/make-proto.pl -a -x ../../packages/mac/framework/gss.json -q -P comment -a -p gssapi/gssapi_private.h mech/gss_*.c
