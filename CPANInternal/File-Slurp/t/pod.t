#!/usr/local/bin/perl

use Test::More;

eval 'use Test::Pod 1.14' ;
plan skip_all =>
	'Test::Pod 1.14 required for testing PODe' if $@ ;

all_pod_files_ok(
# 	{
# 		trustme =>	[ qr/slurp/ ]
# 	}
) ;
