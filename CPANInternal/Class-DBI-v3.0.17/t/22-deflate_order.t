$| = 1;
use strict;

use Test::More;

eval { require Time::Piece::MySQL };
plan skip_all => "Need Time::Piece::MySQL for this test" if $@;

eval { require 't/testlib/Log.pm' };
plan skip_all => "Need MySQL for this test" if $@;

plan tests => 2;

package main;

my $log = Log->insert( { message => 'initial message' } );
ok eval { $log->datetime_stamp }, "Have datetime";
diag $@ if $@;

$log->message( 'a revised message' );
$log->update;
ok eval { $log->datetime_stamp }, "Have datetime after update";
diag $@ if $@;

