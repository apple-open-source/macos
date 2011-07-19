#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use Test::More tests => 21;
use t::lib::Test;

# 1-4. Connect & create tables
my $dbh = connect_ok(dbfile => 'foo');
ok $dbh->do('CREATE TABLE meta1 (f1 INTEGER PRIMARY KEY, f2 CHAR(1))'), 'Create table meta1';
ok $dbh->do('CREATE TABLE meta2 (f1 VARCHAR(2), f2 CHAR(1), PRIMARY KEY (f1))'), 'Create table meta2';
ok $dbh->do('CREATE TABLE meta3 (f2 CHAR(1), f1 VARCHAR(2) PRIMARY KEY)'), 'Create table meta3';

$dbh->trace(0);
$DBI::neat_maxlen = 4000;

# 5-10. Get & check primary_key_info
for my $table (qw(meta1 meta2 meta3)) {
    ok my $sth = $dbh->primary_key_info(undef, undef, $table), "Get primary_key_info for $table";
    my $pki = $sth->fetchall_arrayref([3,4]);
    #use Data::Dumper; print Dumper($pki);
    is_deeply $pki, [['f1', 1]], "Correct primary_key_info returned for $table";
}

# 11-14. Multi column primary key
ok $dbh->do('CREATE TABLE meta4 (f1 VARCHAR(2), f2 CHAR(1), PRIMARY KEY (f1,f2))'), 'Create table meta4';
ok my $sth = $dbh->primary_key_info(undef, undef, 'meta4'), 'Get primary_key_info for meta4';
my $pki = $sth->fetchall_arrayref({COLUMN_NAME => 1, KEY_SEQ => 1});
#use Data::Dumper; print Dumper($pki);
is @$pki, 2, 'Primary key contains 2 columns';
is_deeply $pki, [{COLUMN_NAME => 'f1', KEY_SEQ => 1},{COLUMN_NAME => 'f2', KEY_SEQ => 2}],
    'Correct primary_key_info returned for meta4';

# 15,16. Test primary_key
ok my @pk = $dbh->primary_key(undef, undef, 'meta4'), 'Get primary_key for meta4';
is_deeply \@pk, [qw(f1 f2)], 'Correct primary_key returned for meta4';

# 17-21. I'm not sure what this is testing
$dbh->do("INSERT INTO meta4 VALUES ('xyz', 'b')");
$sth = $dbh->prepare('SELECT * FROM meta4');
$sth->execute;
$sth->fetch;
my $types = $sth->{TYPE};
my $names = $sth->{NAME};
# diag "Types: @$types\nNames: @$names";
is scalar @$types, scalar @$names, '$sth->{TYPE} array is same length as $sth->{NAME} array';
# FIXME: This is wrong! $sth->{TYPE} should return an array of integers see: rt #46873
TODO: {
    local $TODO = '$sth->{TYPE} should return an array of integers.';
    isnt $types->[0], 'VARCHAR(2)', '$sth->{TYPE}[0] doesn\'t return a string';
    isnt $types->[1], 'CHAR(1)', '$sth->{TYPE}[1] doesn\'t return a string';
    like $types->[0], qr/^-?\d+$/, '$sth->{TYPE}[0] returns an integer';
    like $types->[1], qr/^-?\d+$/, '$sth->{TYPE}[1] returns an integer';
}

