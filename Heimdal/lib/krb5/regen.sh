#!/bin/sh
files=$(ls -1 *.c | grep -v config_reg.c)
perl ../../cf/make-proto.pl -q -P keep -o krb5-protos.h -E KRB5_LIB ${files}
perl ../../cf/make-proto.pl -q -P keep -p krb5-private.h ${files}
