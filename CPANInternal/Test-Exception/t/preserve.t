#! /usr/bin/perl -Tw

use strict;
use warnings;

use Test::More tests => 5;

BEGIN { use_ok( 'Test::Exception' ) };

sub div {
   my ($a, $b) = @_;
   return( $a / $b );
};

dies_ok { div(1, 0) } 'exception thrown okay in dies_ok';
like( $@, '/^Illegal division by zero/', 'exception preserved after dies_ok' );

throws_ok { div(1, 0) } '/^Illegal division by zero/', 'exception thrown okay in throws_ok';
like( $@, '/^Illegal division by zero/', 'exception preserved after thrown_ok' );
