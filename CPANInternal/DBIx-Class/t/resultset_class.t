#!/usr/bin/perl

use strict;
use warnings;
use Test::More;
use Class::Inspector ();

unshift(@INC, './t/lib');
use lib 't/lib';
plan tests => 5;

use DBICTest;

is(DBICTest::Schema->source('Artist')->resultset_class, 'DBICTest::BaseResultSet', 'default resultset class');
ok(!Class::Inspector->loaded('DBICNSTest::ResultSet::A'), 'custom resultset class not loaded');
DBICTest::Schema->source('Artist')->resultset_class('DBICNSTest::ResultSet::A');
ok(Class::Inspector->loaded('DBICNSTest::ResultSet::A'), 'custom resultset class loaded automatically');
is(DBICTest::Schema->source('Artist')->resultset_class, 'DBICNSTest::ResultSet::A', 'custom resultset class set');

my $schema = DBICTest->init_schema;
my $resultset = $schema->resultset('Artist')->search;
isa_ok($resultset, 'DBICNSTest::ResultSet::A', 'resultset is custom class');
