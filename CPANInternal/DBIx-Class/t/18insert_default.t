use strict;
use warnings;

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $tests = 3;
plan tests => $tests;

my $schema = DBICTest->init_schema();
my $rs = $schema->resultset ('Artist');
my $last_obj = $rs->search ({}, { order_by => { -desc => 'artistid' }, rows => 1})->single;
my $last_id = $last_obj ? $last_obj->artistid : 0;

my $obj;
eval { $obj = $rs->create ({}) };
my $err = $@;

ok ($obj, 'Insert defaults ( $rs->create ({}) )' );
SKIP: {
  skip "Default insert failed: $err", $tests-1 if $err;

  # this should be picked up without calling the DB again
  is ($obj->artistid, $last_id + 1, 'Autoinc PK works');

  # for this we need to refresh
  $obj->discard_changes;
  is ($obj->rank, 13, 'Default value works');
}

