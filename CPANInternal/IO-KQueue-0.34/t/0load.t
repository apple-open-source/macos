#!/usr/bin/perl -w

use Test::More tests => 2;

use_ok('IO::KQueue');

ok(IO::KQueue->new(), "Check we can create a new kqueue");