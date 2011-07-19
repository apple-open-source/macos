#!/usr/local/bin/perl

use Test::More;

eval 'use Test::Pod::Coverage 1.04' ;
plan skip_all =>
	'Test::Pod::Coverage 1.04 required for testing POD coverage' if $@ ;

all_pod_coverage_ok(
	{
		trustme =>	[
			'slurp',
			'O_APPEND',
			'O_BINARY',
			'O_CREAT',
			'O_EXCL',
			'O_RDONLY',
			'O_WRONLY',
			'SEEK_CUR',
			'SEEK_END',
			'SEEK_SET',
		],
	}
) ;
