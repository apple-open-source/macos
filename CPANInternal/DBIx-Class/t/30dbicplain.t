#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use lib qw(t/lib);
use DBICTest::Plain;

plan tests => 1;

cmp_ok(DBICTest::Plain->resultset('Test')->count, '>', 0, 'count is valid');
