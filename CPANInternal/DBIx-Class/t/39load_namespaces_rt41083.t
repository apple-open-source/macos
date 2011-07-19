#!/usr/bin/perl

use strict;
use warnings;

use lib 't/lib';
use DBICTest; # do not remove even though it is not used 
use Test::More tests => 8;

sub _chk_warning {
  defined $_[0]?
    $_[0] !~ qr/We found ResultSet class '([^']+)' for '([^']+)', but it seems that you had already set '([^']+)' to use '([^']+)' instead/ :
    1
}

sub _chk_extra_sources_warning {
  my $p = qr/already has a source, use register_extra_source for additional sources/;
  defined $_[0]? $_[0] !~ /$p/ : 1;
}

sub _verify_sources {
  my @monikers = @_;
  is_deeply (
    [ sort DBICNSTest::RtBug41083->sources ],
    \@monikers,
    'List of resultsource registrations',
  );
}

{
  my $warnings;
  eval {
    local $SIG{__WARN__} = sub { $warnings .= shift };
    package DBICNSTest::RtBug41083;
    use base 'DBIx::Class::Schema';
    __PACKAGE__->load_namespaces(
      result_namespace => 'Schema_A',
      resultset_namespace => 'ResultSet_A',
      default_resultset_class => 'ResultSet'
    );
  };

  ok(!$@) or diag $@;
  ok(_chk_warning($warnings), 'expected no resultset complaint');
  ok(_chk_extra_sources_warning($warnings), 'expected no extra sources complaint') or diag($warnings);

  _verify_sources (qw/A A::Sub/);
}

{
  my $warnings;
  eval {
    local $SIG{__WARN__} = sub { $warnings .= shift };
    package DBICNSTest::RtBug41083;
    use base 'DBIx::Class::Schema';
    __PACKAGE__->load_namespaces(
      result_namespace => 'Schema',
      resultset_namespace => 'ResultSet',
      default_resultset_class => 'ResultSet'
    );
  };
  ok(!$@) or diag $@;
  ok(_chk_warning($warnings), 'expected no resultset complaint') or diag $warnings;
  ok(_chk_extra_sources_warning($warnings), 'expected no extra sources complaint') or diag($warnings);

  _verify_sources (qw/A A::Sub Foo Foo::Sub/);
}
