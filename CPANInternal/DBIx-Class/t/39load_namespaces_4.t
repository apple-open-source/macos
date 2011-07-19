#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use lib qw(t/lib);
use DBICTest; # do not remove even though it is not used

plan tests => 6;

my $warnings;
eval {
    local $SIG{__WARN__} = sub { $warnings .= shift };
    package DBICNSTest;
    use base qw/DBIx::Class::Schema/;
    __PACKAGE__->load_namespaces( default_resultset_class => 'RSBase' );
};
ok(!$@) or diag $@;
like($warnings, qr/load_namespaces found ResultSet class C with no corresponding Result class/);

my $source_a = DBICNSTest->source('A');
isa_ok($source_a, 'DBIx::Class::ResultSource::Table');
my $rset_a   = DBICNSTest->resultset('A');
isa_ok($rset_a, 'DBICNSTest::ResultSet::A');

my $source_b = DBICNSTest->source('B');
isa_ok($source_b, 'DBIx::Class::ResultSource::Table');
my $rset_b   = DBICNSTest->resultset('B');
isa_ok($rset_b, 'DBICNSTest::RSBase');
