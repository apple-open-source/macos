#!/usr/bin/perl

use strict;
use warnings;

use vars qw( @files );

BEGIN {
    eval "require File::Find::Rule";
    if ($@)
    {
        print "1..1\nok 1 # skip File::Find::Rule not installed\n";
        exit;
    }
    @files = File::Find::Rule->file()->name( '*.pm', '*.pod' )->in( 'blib/lib' );
}

use Test::More tests => scalar @files;

eval "use Test::Pod 0.95";
SKIP: {
    skip "Test::Pod 0.95 not installed.", scalar @files if $@;
    pod_file_ok( $_ ) for @files;
}
