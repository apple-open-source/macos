#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

unshift(@INC, './t/lib');

plan tests => 7;

my $warnings;
eval {
    local $SIG{__WARN__} = sub { $warnings .= shift };
    package DBICNSTestOther;
    use base qw/DBIx::Class::Schema/;
    __PACKAGE__->load_namespaces(
        result_namespace => [ '+DBICNSTest::Rslt', '+DBICNSTest::OtherRslt' ],
        resultset_namespace => '+DBICNSTest::RSet',
    );
};
ok(!$@) or diag $@;
like($warnings, qr/load_namespaces found ResultSet class C with no corresponding Result class/);

my $source_a = DBICNSTestOther->source('A');
isa_ok($source_a, 'DBIx::Class::ResultSource::Table');
my $rset_a   = DBICNSTestOther->resultset('A');
isa_ok($rset_a, 'DBICNSTest::RSet::A');

my $source_b = DBICNSTestOther->source('B');
isa_ok($source_b, 'DBIx::Class::ResultSource::Table');
my $rset_b   = DBICNSTestOther->resultset('B');
isa_ok($rset_b, 'DBIx::Class::ResultSet');

my $source_d = DBICNSTestOther->source('D');
isa_ok($source_d, 'DBIx::Class::ResultSource::Table');
