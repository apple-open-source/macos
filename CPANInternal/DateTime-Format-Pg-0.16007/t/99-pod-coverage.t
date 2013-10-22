#!perl
use Test::More;
BEGIN {
    eval "use Test::Pod";
    eval "use Test::Pod::Coverage";
    if ($@) {
        plan(skip_all => "Test::Pod::Coverage required for testing POD");
        eval "sub all_pod_coverage_ok {}";
    }
}

all_pod_coverage_ok();
