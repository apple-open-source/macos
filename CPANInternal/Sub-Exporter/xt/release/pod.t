#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests that all POD is valid.

=cut

use Test::More;

BEGIN { 
  eval "use Test::Pod 1.00";
  plan skip_all => "Test::Pod 1.00 required for testing POD" if $@;
}

all_pod_files_ok;
