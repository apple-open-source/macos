use warnings;
use strict;

use Test::More;
plan skip_all => "Test::Pod::Coverage not available"
	unless eval "use Test::Pod::Coverage; 1";
Test::Pod::Coverage::all_pod_coverage_ok();

1;
