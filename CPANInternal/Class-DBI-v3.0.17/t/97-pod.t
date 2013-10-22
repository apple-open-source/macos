use Test::More;
use strict;
eval "use Test::Pod 1.00";
plan skip_all => "Test::Pod 1.00 required for testing POD" if $@;
eval "use Test::Pod::Coverage 1.00";
all_pod_files_ok();
