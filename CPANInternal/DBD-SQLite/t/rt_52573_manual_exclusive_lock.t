#!/usr/bin/perl -w

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 92 * 4 + 2;
use Test::NoWarnings;

my $dbh = connect_ok(
	AutoCommit => 1,
	RaiseError => 1,
	PrintError => 0,
);

$dbh->do('create table foo (id)');

my @funcs = (
    sub { shift->rollback },
    sub { shift->commit },
    sub { shift->do('rollback') },
    sub { shift->do('commit') },
);

foreach my $func (@funcs) {
	# scenario 1: AutoCommit => 1 and no begin_work

	eval { $dbh->{AutoCommit} = 1 }; # initialize
	ok $dbh->{AutoCommit}, "AutoCommit is on";
	ok !$dbh->{BegunWork},  "BegunWork is off";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'a statement works';
	diag $@ if $@;
	# eval { $func->($dbh) };
	# ok !$@, "commit/rollback ignored";
	# diag $@ if $@;
	ok $dbh->{AutoCommit}, "AutoCommit is still on";
	ok !$dbh->{BegunWork},  "BegunWork is still off";

	# scenario 2: AutoCommit => 1 and begin_work and implicit BEGIN

	eval { $dbh->begin_work };
	ok !$@, "begin_work works";
	ok !$dbh->{AutoCommit}, "AutoCommit is turned off";
	ok $dbh->{BegunWork},    "BegunWork is turned on";
	eval { $dbh->begin_work };
	like $@ => qr/Already in a transaction/, "but second begin_work should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, "other statement should work";
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok $dbh->{AutoCommit}, "AutoCommit is turned on";
	ok !$dbh->{BegunWork},  "BegunWork is turned off";

	# scenario 3: AutoCommit => 1 and begin_work and explicit and immediate BEGIN

	eval { $dbh->begin_work };
	ok !$@, "begin_work works";
	ok !$dbh->{AutoCommit}, "AutoCommit is turned off";
	ok $dbh->{BegunWork},    "BegunWork is turned on";
	eval { $dbh->do('BEGIN EXCLUSIVE TRANSACTION') };
	ok !$@, "first BEGIN should be passed through";
	diag $@ if $@;
	eval { $dbh->do('BEGIN TRANSACTION') };
	like $@ => qr/cannot start a transaction/, "second BEGIN should fail";
	eval { $dbh->begin_work };
	like $@ => qr/Already in a transaction/, "and second begin_work also should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok $dbh->{AutoCommit}, "AutoCommit is turned on now";
	ok !$dbh->{BegunWork},  "BegunWork is turned off";

	# scenario 4: AutoCommit => 1 and begin_work and explicit but not immediate BEGIN
	eval { $dbh->begin_work };
	ok !$@, "begin_work works";
	ok !$dbh->{AutoCommit}, "AutoCommit is turned off";
	ok $dbh->{BegunWork},    "BegunWork is turned on";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'statement should work';
	diag $@ if $@;
	eval { $dbh->do('BEGIN TRANSACTION') };
	like $@ => qr/cannot start a transaction/, "BEGIN after other statements should fail";
	eval { $dbh->begin_work };
	like $@ => qr/Already in a transaction/, "and second begin_work also should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok $dbh->{AutoCommit}, "AutoCommit is turned on now";
	ok !$dbh->{BegunWork},  "BegunWork is turned off";

	# scenario 5: AutoCommit => 1 and explicit BEGIN and no begin_work
	ok $dbh->{AutoCommit}, "AutoCommit is on";
	ok !$dbh->{BegunWork},    "BegunWork is off";
	eval { $dbh->do('BEGIN TRANSACTION'); };
	ok !$@, 'BEGIN should work';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is turned off";
	ok $dbh->{BegunWork},    "BegunWork is turned on";
	eval { $dbh->do('BEGIN TRANSACTION') };
	like $@ => qr/cannot start a transaction/, "second BEGIN should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok $dbh->{AutoCommit}, "AutoCommit is turned on now";
	ok !$dbh->{BegunWork},  "BegunWork is turned off";

	# scenario 6: AutoCommit => 1 and explicit BEGIN and begin_work
	ok $dbh->{AutoCommit}, "AutoCommit is on";
	ok !$dbh->{BegunWork},    "BegunWork is off";
	eval { $dbh->do('BEGIN TRANSACTION'); };
	ok !$@, 'BEGIN should work';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is turned off";
	ok $dbh->{BegunWork},    "BegunWork is turned on";
	eval { $dbh->do('BEGIN TRANSACTION') };
	like $@ => qr/cannot start a transaction/, "second BEGIN should fail";
	eval { $dbh->begin_work };
	like $@ => qr/Already in a transaction/, "and second begin_work also should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok $dbh->{AutoCommit}, "AutoCommit is turned on now";
	ok !$dbh->{BegunWork},  "BegunWork is turned off";

	# scenario 7: AutoCommit => 0 and explicit BEGIN
	eval { $dbh->{AutoCommit} = 1 }; # to initialize
	ok $dbh->{AutoCommit}, "AutoCommit is on";
	ok !$dbh->{BegunWork},  "BegunWork is off";
	eval { $dbh->{AutoCommit} = 0 };
	ok !$@, "AutoCommit is turned off";
	ok !$dbh->{BegunWork},    "BegunWork is still off";
	eval { $dbh->do('BEGIN TRANSACTION'); };
	ok !$@, 'BEGIN should work';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is turned off";
	ok !$dbh->{BegunWork},   "BegunWork is still off";
	eval { $dbh->do('BEGIN TRANSACTION') };
	like $@ => qr/cannot start a transaction/, "second BEGIN should fail";
	eval { $dbh->begin_work };
	like $@ => qr/Already in a transaction/, "and begin_work also should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is still off";
	ok !$dbh->{BegunWork},   "BegunWork is still off";

	# scenario 8: AutoCommit => 0 and begin_work
	eval { $dbh->{AutoCommit} = 1 }; # to initialize
	ok $dbh->{AutoCommit}, "AutoCommit is on";
	ok !$dbh->{BegunWork},  "BegunWork is off";
	eval { $dbh->{AutoCommit} = 0 };
	ok !$@, "AutoCommit is turned off";
	ok !$dbh->{BegunWork}, "BegunWork is still off";
	eval { $dbh->begin_work; };
	like $@ => qr/Already in a transaction/, "begin_work should fail";
	ok !$dbh->{AutoCommit}, "AutoCommit is still off";
	ok !$dbh->{BegunWork},   "BegunWork is still off";
	eval { $dbh->do('BEGIN TRANSACTION') };
	ok !$@, "BEGIN should work";
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is still off";
	ok !$dbh->{BegunWork},   "BegunWork is still off";
	eval { $dbh->begin_work };
	like $@ => qr/Already in a transaction/, "and second begin_work also should fail";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is still off";
	ok !$dbh->{BegunWork},   "BegunWork is still off";

	# scenario 9: AutoCommit => 0 and implicit BEGIN
	eval { $dbh->{AutoCommit} = 1 }; # to initialize
	ok $dbh->{AutoCommit}, "AutoCommit is on";
	ok !$dbh->{BegunWork},  "BegunWork is off";
	eval { $dbh->{AutoCommit} = 0 };
	ok !$@, "AutoCommit is turned off";
	ok !$dbh->{BegunWork}, "BegunWork is still off";
	eval { $dbh->do('insert into foo (id) values (1)'); };
	ok !$@, 'other statement should work';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is still off";
	ok !$dbh->{BegunWork},   "BegunWork is still off";
	eval { $func->($dbh) };
	ok !$@, 'rolled back/committed';
	diag $@ if $@;
	ok !$dbh->{AutoCommit}, "AutoCommit is still off";
	ok !$dbh->{BegunWork},  "BegunWork is still off";
}
eval { $dbh->{AutoCommit} = 1 }; # to end transaction
$dbh->disconnect;
