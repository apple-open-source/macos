#!perl -w

use strict;
use Test::More;

BEGIN {
    eval {
	require Test::Distribution;
    };
    if($@) {
	plan skip_all => 'Test::Distribution not installed';
    }
    else {
	import Test::Distribution;
    }
}
