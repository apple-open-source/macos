#!perl -w

use strict;

$|=1;
$^W=1;

my $calls = 0;
my %my_methods;


# =================================================
# Example code for sub classing the DBI.
#
# Note that the extra ::db and ::st classes must be set up
# as sub classes of the corresponding DBI classes.
#
# This whole mechanism is new and experimental - it may change!

package MyDBI;
@MyDBI::ISA = qw(DBI);

# the MyDBI::dr::connect method is NOT called!
# you can either override MyDBI::connect()
# or use MyDBI::db::connected()

package MyDBI::db;
@MyDBI::db::ISA = qw(DBI::db);

sub prepare {
    my($dbh, @args) = @_;
    ++$my_methods{prepare};
    ++$calls;
    my $sth = $dbh->SUPER::prepare(@args);
    return $sth;
}


package MyDBI::st;
@MyDBI::st::ISA = qw(DBI::st);

sub fetch {
    my($sth, @args) = @_;
    ++$my_methods{fetch};
    ++$calls;
    # this is just to trigger (re)STORE on exit to test that the STORE
    # doesn't clear any erro condition
    local $sth->{Taint} = 0;
    my $row = $sth->SUPER::fetch(@args);
    if ($row) {
	# modify fetched data as an example
	$row->[1] = lc($row->[1]);

	# also demonstrate calling set_err()
	return $sth->set_err(1,"Don't be so negative",undef,"fetch")
		if $row->[0] < 0;
	# ... and providing alternate results
	# (although typically would trap and hide and error from SUPER::fetch)
	return $sth->set_err(2,"Don't exagerate",undef, undef, [ 42,"zz",0 ])
		if $row->[0] > 42;
    }
    return $row;
}


# =================================================
package main;

use Test::More tests => 36;

BEGIN {
    use_ok( 'DBI' );
}

my $tmp;

#DBI->trace(2);
my $dbh = MyDBI->connect("dbi:Sponge:foo","","", {
	PrintError => 0,
	RaiseError => 1,
	CompatMode => 1, # just for clone test
});
isa_ok($dbh, 'MyDBI::db');
is($dbh->{CompatMode}, 1);
undef $dbh;

$dbh = DBI->connect("dbi:Sponge:foo","","", {
	PrintError => 0,
	RaiseError => 1,
	RootClass => "MyDBI",
	CompatMode => 1, # just for clone test
        dbi_foo => 1, # just to help debugging clone etc
});
isa_ok( $dbh, 'MyDBI::db');
is($dbh->{CompatMode}, 1);

#$dbh->trace(5);
my $sth = $dbh->prepare("foo",
    # data for DBD::Sponge to return via fetch
    { rows => [
	[ 40, "AAA", 9 ],
	[ 41, "BB",  8 ],
	[ -1, "C",   7 ],
	[ 49, "DD",  6 ]
	],
    }
);

is($calls, 1);
isa_ok($sth, 'MyDBI::st');

my $row = $sth->fetch;
is($calls, 2);
is($row->[1], "aaa");

$row = $sth->fetch;
is($calls, 3);
is($row->[1], "bb");

is($DBI::err, undef);
$row = eval { $sth->fetch };
my $eval_err = $@;
is(!defined $row, 1);
is(substr($eval_err,0,50), "DBD::Sponge::st fetch failed: Don't be so negative");

#$sth->trace(5);
#$sth->{PrintError} = 1;
$sth->{RaiseError} = 0;
$row = eval { $sth->fetch };
isa_ok($row, 'ARRAY');
is($row->[0], 42);
is($DBI::err, 2);
like($DBI::errstr, qr/Don't exagerate/);
is($@ =~ /Don't be so negative/, $@);


my $dbh2 = $dbh->clone;
isa_ok( $dbh2, 'MyDBI::db', "Clone A" );
is($dbh2 != $dbh, 1);
is($dbh2->{CompatMode}, 1);

my $dbh3 = $dbh->clone;
isa_ok( $dbh3, 'MyDBI::db', 'Clone B' );
is($dbh3 != $dbh, 1);
is($dbh3 != $dbh2, 1);
isa_ok( $dbh3, 'MyDBI::db');
is($dbh3->{CompatMode}, 1);

$tmp = $dbh->sponge_test_installed_method('foo','bar');
isa_ok( $tmp, "ARRAY", "installed method" );
is_deeply( $tmp, [qw( foo bar )] );
$tmp = eval { $dbh->sponge_test_installed_method() };
is(!$tmp, 1);
is($dbh->err, 42);
is($dbh->errstr, "not enough parameters");


$dbh = eval { DBI->connect("dbi:Sponge:foo","","", {
	RootClass => 'nonesuch1', PrintError => 0, RaiseError => 0, });
};
ok( !defined($dbh), "Failed connect #1" );
is(substr($@,0,25), "Can't locate nonesuch1.pm");

$dbh = eval { nonesuch2->connect("dbi:Sponge:foo","","", {
	PrintError => 0, RaiseError => 0, });
};
ok( !defined($dbh), "Failed connect #2" );
is(substr($@,0,36), q{Can't locate object method "connect"});

print "@{[ %my_methods ]}\n";
1;
