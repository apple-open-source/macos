#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test;
use Test::More tests => 27;
use Test::NoWarnings;

my $dbh = connect_ok(
    RaiseError => 1,
    PrintError => 0,
    AutoCommit => 0,
);

ok($dbh->do("CREATE TABLE Blah ( id INTEGER, val VARCHAR )"));
ok($dbh->commit);

my $blob = "";

my $b = "";
for my $j (0..255) {
    $b .= chr($j);
}
for my $i (0..127) {
    $blob .= $b;
}

ok($blob);
dumpblob($blob);

my $sth = $dbh->prepare("INSERT INTO Blah VALUES (?, ?)");

ok($sth);

for (1..5) {
    ok($sth->execute($_, $blob));
}

$sth->finish;

undef $sth;

my $sel = $dbh->prepare("SELECT * FROM Blah WHERE id = ?");

ok($sel);

for (1..5) {
    $sel->execute($_);
    my $row = $sel->fetch;
    ok($row->[0] == $_);
    dumpblob($row->[1]);
    ok($row->[1] eq $blob);
    ok(!$sel->fetch);
}

$dbh->rollback;

sub dumpblob {
    my $blob = shift;
    print("# showblob length: ", length($blob), "\n");
    
    if ($ENV{SHOW_BLOBS}) { open(OUT, ">>$ENV{SHOW_BLOBS}") }
    my $i = 0;
    while (1) {
	if (defined($blob)  &&  length($blob) > ($i*32)) {
	    $b = substr($blob, $i*32);
	} else {
	    $b = "";
            last;
	}
        if ($ENV{SHOW_BLOBS}) { printf OUT "%08lx %s\n", $i*32, unpack("H64", $b) }
        else { printf("# %08lx %s\n", $i*32, unpack("H64", $b)) }
        $i++;
        last if $i == 8;
    }
    if ($ENV{SHOW_BLOBS}) { close(OUT) }
}

