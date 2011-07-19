#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

unshift(@INC, './t/lib');

plan tests => 4;

my $warnings;
eval {
    local $SIG{__WARN__} = sub { $warnings .= shift };
    package DBICTest::Schema;
    use base qw/DBIx::Class::Schema/;
    __PACKAGE__->load_classes;
};
ok(!$@, 'Loaded all loadable classes') or diag $@;
like($warnings, qr/Failed to load DBICTest::Schema::NoSuchClass. Can't find source_name method. Is DBICTest::Schema::NoSuchClass really a full DBIC result class?/, 'Warned about broken result class');

my $source_a = DBICTest::Schema->source('Artist');
isa_ok($source_a, 'DBIx::Class::ResultSource::Table');
my $rset_a   = DBICTest::Schema->resultset('Artist');
isa_ok($rset_a, 'DBIx::Class::ResultSet');

