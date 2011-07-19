$| = 1;
use strict;

use Test::More;

eval "use DBIx::Class::CDBICompat;";
if ($@) {
    plan (skip_all => "Class::Trigger and DBIx::ContextualFetch required: $@");
    next;
}

plan skip_all => 'Set $ENV{DBICTEST_MYSQL_DSN}, _USER and _PASS to run this test'
  unless ($ENV{DBICTEST_MYSQL_DSN} && $ENV{DBICTEST_MYSQL_USER});

eval { require Time::Piece::MySQL };
plan skip_all => "Need Time::Piece::MySQL for this test" if $@;

plan tests => 3;

use lib 't/cdbi/testlib';
use_ok ('Log');

package main;

my $log = Log->insert( { message => 'initial message' } );
ok eval { $log->datetime_stamp }, "Have datetime";
diag $@ if $@;

$log->message( 'a revised message' );
$log->update;
ok eval { $log->datetime_stamp }, "Have datetime after update";
diag $@ if $@;

