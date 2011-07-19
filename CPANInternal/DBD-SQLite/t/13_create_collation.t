#!/usr/bin/perl

use strict;
BEGIN {
	$|  = 1;
	$^W = 1;
}

use t::lib::Test     qw/connect_ok dies @CALL_FUNCS/;
use Test::More;
BEGIN {
        my $COLLATION_TESTS = 10;
        my $WRITE_ONCE_TESTS = 4;

	if ( $] >= 5.008005 ) {
		plan( tests => $COLLATION_TESTS * @CALL_FUNCS + 
                               $WRITE_ONCE_TESTS + 1);
	} else {
		plan( skip_all => 'Unicode is not supported before 5.8.5' );
	}
}
use Test::NoWarnings;
use Encode qw/decode/;
use DBD::SQLite;

BEGIN {
	# Sadly perl for windows (and probably sqlite, too) may hang
	# if the system locale doesn't support european languages.
	# en-us should be a safe default. if it doesn't work, use 'C'.
	if ( $^O eq 'MSWin32') {
		use POSIX 'locale_h';
		setlocale(LC_COLLATE, 'en-us');
	}
}

# ad hoc collation functions
sub no_accents ($$) {
	my ( $a, $b ) = map lc, @_;
	tr[àâáäåãçğèêéëìîíïñòôóöõøùûúüı]
	  [aaaaaacdeeeeiiiinoooooouuuuy] for $a, $b;
	$a cmp $b;
}

sub by_length ($$) {
	length($_[0]) <=> length($_[1])
}

sub by_num ($$) {
	$_[0] <=> $_[1];
}
sub by_num_desc ($$) {
	$_[1] <=> $_[0];
}


# collation 'no_accents' will be automatically loaded on demand
$DBD::SQLite::COLLATION{no_accents} = \&no_accents;


$" = ", "; # to embed arrays into message strings

my $sql = "SELECT txt from collate_test ORDER BY txt";



# test interaction with the global COLLATION hash ("WriteOnce")

dies (sub {$DBD::SQLite::COLLATION{perl} = sub {}},
      qr/already registered/,
      "can't override builtin perl collation");

dies (sub {delete $DBD::SQLite::COLLATION{perl}},
      qr/deletion .* is forbidden/,
      "can't delete builtin perl collation");

# once a collation is registered, we can't override it ... unless by
# digging into the tied object
$DBD::SQLite::COLLATION{foo} = \&by_num;
dies (sub {$DBD::SQLite::COLLATION{foo} = \&by_num_desc},
      qr/already registered/,
      "can't override registered collation");
my $tied = tied %DBD::SQLite::COLLATION;
delete $tied->{foo};
$DBD::SQLite::COLLATION{foo} = \&by_num_desc; # override, no longer dies
is($DBD::SQLite::COLLATION{foo}, \&by_num_desc, "overridden collation");



# now really test the collation functions

foreach my $call_func (@CALL_FUNCS) {

  for my $use_unicode (0, 1) {

    # connect
    my $dbh = connect_ok( RaiseError => 1, sqlite_unicode => $use_unicode );

    # populate test data
    my @words = qw{
	berger Bergèòe bergèòe Bergere
	HOT hôôe 
	héôéòoclite héôaïòe hêôre héòaut
	HAT hâôer 
	féôu fêôe fèöe ferme
     };
    if ($use_unicode) {
      utf8::upgrade($_) foreach @words;
    }

    $dbh->do( 'CREATE TEMP TABLE collate_test ( txt )' );
    $dbh->do( "INSERT INTO collate_test VALUES ( '$_' )" ) foreach @words;

    # test builtin collation "perl"
    my @sorted    = sort @words;
    my $db_sorted = $dbh->selectcol_arrayref("$sql COLLATE perl");
    is_deeply(\@sorted, $db_sorted, "collate perl (@sorted // @$db_sorted)");

  SCOPE: {
      use locale;
      @sorted = sort @words;
    }

    # test builtin collation "perllocale"
    $db_sorted = $dbh->selectcol_arrayref("$sql COLLATE perllocale");
    is_deeply(\@sorted, $db_sorted, 
              "collate perllocale (@sorted // @$db_sorted)");

    # test additional collation "no_accents"
    @sorted    = sort no_accents @words;
    $db_sorted = $dbh->selectcol_arrayref("$sql COLLATE no_accents");
    is_deeply(\@sorted, $db_sorted, 
              "collate no_accents (@sorted // @$db_sorted)");


    # manual addition of a collation for this dbh
    $dbh->$call_func(by_length => \&by_length, "create_collation");
    @sorted    = sort by_length @words;
    $db_sorted = $dbh->selectcol_arrayref("$sql COLLATE by_length");
    is_deeply(\@sorted, $db_sorted, 
              "collate by_length (@sorted // @$db_sorted)");
  }
}




