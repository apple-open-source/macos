use strict;

use Test::More;
eval "use Test::Pod::Coverage 1.06";
plan skip_all => "Test::Pod::Coverage 1.06 required for testing POD coverage"
	if $@;

# Doesn't this show you why the pod-coverage Kwalitee metric is bull?

my $covered = [ map { qr/\A$_\z/ } qw(
  compare
  constant
  create
  create_b64
  create_bin
  create_from_name
  create_from_name_b64
  create_from_name_bin
  create_from_name_hex
  create_from_name_str
  create_hex
  create_str
  from_b64string
  from_hexstring
  from_string
  new
  to_b64string
  to_hexstring
  to_string  
)];

all_pod_coverage_ok({also_private => $covered });
