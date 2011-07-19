#!perl -T
use strict;
use warnings;

=head1 TEST PURPOSE

These tests that everything public is documented.

=cut

use Test::More;

BEGIN {
  eval "use Test::Pod::Coverage 1.08";
  plan skip_all => "Test::Pod::Coverage 1.08 required for testing POD coverage"
    if $@;
}

all_pod_coverage_ok({
  trustme => [
    # Sub::Exporter
    qw(default_exporter),

    # Sub::Exporter::Util
    qw(curry_class mixin_exporter),
  ],
});
