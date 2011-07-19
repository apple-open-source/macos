#!/usr/bin/perl

use strict;
use warnings;

use Test::More;

eval "use Test::NoTabs 0.8";
plan skip_all => "Test::NoTabs 0.8 required for testing tabs" if $@;

for my $file ( qw( mop.c mop.h ), glob "xs/*xs" ) {
    notabs_ok( $file, "$file is tab free" );
}

# Module::Install has tabs, so we can't check 'inc' or ideally '.'
all_perl_files_ok('lib', 't', 'xt');
