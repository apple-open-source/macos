#!/usr/bin/perl

use strict;
use warnings;  

use Test::More tests => 8;
use lib qw(t/lib);
use DBICTest;


my $schema = DBICTest->init_schema();
my $storage = $schema->storage;

my $test_func = sub {
    is $_[0], $storage;
    is $_[1], $storage->dbh;
    is $_[2], "foo";
    is $_[3], "bar";
};

$storage->dbh_do(
    $test_func,
    "foo", "bar"
);

my $storage_class = ref $storage;
{
    no strict 'refs';
    *{$storage_class .'::__test_method'} = $test_func;
}
$storage->dbh_do("__test_method", "foo", "bar");

    