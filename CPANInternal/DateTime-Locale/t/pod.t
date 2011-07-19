use strict;
use warnings;

use Test::More;

plan skip_all => 'This test is only run for the module author'
    unless -d '.hg' || $ENV{IS_MAINTAINER};

eval "use Test::Pod 1.14";
plan skip_all => "Test::Pod 1.14 required for testing POD" if $@;

all_pod_files_ok();
