#!/bin/sh
files=$(ls -1 der*.c extra.c fuzzer.c template.c) 
perl ../../cf/make-proto.pl -q -P comment -o der-protos.h -E KRB5_LIB ${files}
perl ../../cf/make-proto.pl -q -P comment -p der-private.h ${files}
