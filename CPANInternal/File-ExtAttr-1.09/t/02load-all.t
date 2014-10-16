#!perl -w

use strict;
use Test::More tests => 2;

BEGIN {
	use_ok( 'File::ExtAttr', ':all' );
        use_ok( 'File::ExtAttr::Tie' );
}
