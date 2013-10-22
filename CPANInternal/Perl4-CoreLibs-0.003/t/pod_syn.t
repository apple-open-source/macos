use warnings;
use strict;

use Test::More;
plan skip_all => "Test::Pod not available" unless eval "use Test::Pod 1.00; 1";
Test::Pod::all_pod_files_ok();

1;
