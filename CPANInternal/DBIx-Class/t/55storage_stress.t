use strict;
use warnings;
use Test::More;

# XXX obviously, the guts of this test haven't been written yet --blblack

use lib qw(t/lib);

plan skip_all => 'Set $ENV{DBICTEST_STORAGE_STRESS} to run this test'
    . ' (it is very resource intensive!)'
        unless $ENV{DBICTEST_STORAGE_STRESS};

my $NKIDS = 20;
my $CYCLES = 5;
my @KILL_RATES = qw/0 0.001 0.01 0.1 0.2 0.5 0.75 1.0/;

# Stress the storage with these parameters...
sub stress_storage {
    my ($connect_info, $num_kids, $cycles, $kill_rate) = @_;

    foreach my $cycle (1..$cycles) {
        my $schema = DBICTest::Schema->connection(@$connect_info, { AutoCommit => 1 });
        foreach my $kidno (1..$num_kids) {
            ok(1);
        }
    }
}

# Get a set of connection information -
#  whatever the user has supplied for the vendor-specific tests
sub get_connect_infos {
    my @connect_infos;
    foreach my $db_prefix (qw/PG MYSQL DB2 MSSQL ORA/) {
        my @conn_info = @ENV{
            map { "DBICTEST_${db_prefix}_${_}" } qw/DSN USER PASS/
        };
        push(@connect_infos, \@conn_info) if $conn_info[0];
    }
    \@connect_infos;
}

my $connect_infos = get_connect_infos();

plan skip_all => 'This test needs some non-sqlite connect info!'
    unless @$connect_infos;

plan tests => (1 * @$connect_infos * $NKIDS * $CYCLES * @KILL_RATES) + 1;

use_ok('DBICTest::Schema');

foreach my $connect_info (@$connect_infos) {
    foreach my $kill_rate (@KILL_RATES) {
        stress_storage($connect_info, $NKIDS, $CYCLES, $kill_rate);
    }
}
