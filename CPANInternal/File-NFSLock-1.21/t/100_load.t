# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.t'

######################### We start with some black magic to print on failure.
use strict;
use warnings;

use Test::More tests => 1;

use_ok 'File::NFSLock';
