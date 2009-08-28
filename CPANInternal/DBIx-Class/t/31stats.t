#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

BEGIN {
    eval "use DBD::SQLite";
    plan $@
        ? ( skip_all => 'needs DBD::SQLite for testing' )
        : ( tests => 12 );
}

use lib qw(t/lib);

use_ok('DBICTest');
my $schema = DBICTest->init_schema();

my $cbworks = 0;

$schema->storage->debugcb(sub { $cbworks = 1; });
$schema->storage->debug(0);
my $rs = $schema->resultset('CD')->search({});
$rs->count();
ok(!$cbworks, 'Callback not called with debug disabled');

$schema->storage->debug(1);

$rs->count();
ok($cbworks, 'Debug callback worked.');

my $prof = new DBIx::Test::Profiler();
$schema->storage->debugobj($prof);

# Test non-transaction calls.
$rs->count();
ok($prof->{'query_start'}, 'query_start called');
ok($prof->{'query_end'}, 'query_end called');
ok(!$prof->{'txn_begin'}, 'txn_begin not called');
ok(!$prof->{'txn_commit'}, 'txn_commit not called');

$prof->reset();

# Test transaction calls
$schema->txn_begin();
ok($prof->{'txn_begin'}, 'txn_begin called');

$rs = $schema->resultset('CD')->search({});
$rs->count();
ok($prof->{'query_start'}, 'query_start called');
ok($prof->{'query_end'}, 'query_end called');

$schema->txn_commit();
ok($prof->{'txn_commit'}, 'txn_commit called');

$prof->reset();

# Test a rollback
$schema->txn_begin();
$rs = $schema->resultset('CD')->search({});
$rs->count();
$schema->txn_rollback();
ok($prof->{'txn_rollback'}, 'txn_rollback called');

$schema->storage->debug(0);

package DBIx::Test::Profiler;
use strict;

sub new {
    my $self = bless({});
}

sub query_start {
    my $self = shift();
    $self->{'query_start'} = 1;
}

sub query_end {
    my $self = shift();
    $self->{'query_end'} = 1;
}

sub txn_begin {
    my $self = shift();
    $self->{'txn_begin'} = 1;
}

sub txn_rollback {
    my $self = shift();
    $self->{'txn_rollback'} = 1;
}

sub txn_commit {
    my $self = shift();
    $self->{'txn_commit'} = 1;
}

sub reset {
    my $self = shift();

    $self->{'query_start'} = 0;
    $self->{'query_end'} = 0;
    $self->{'txn_begin'} = 0;
    $self->{'txn_rollback'} = 0;
    $self->{'txn_end'} = 0;
}

1;
