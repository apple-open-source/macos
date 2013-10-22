# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test::More tests => 21;
use Lingua::EN::Inflect::Number (qw( PL_N to_PL to_S number));

ok(*PL_N{CODE}, "Imported something from L::EN::Inflect");
ok(*to_PL{CODE}, "Imported something from Number");

is(number("goat"), "s", "one goat");
is(number("goats"), "p", "two goats");
is(number("sheep"), "ambig", "who knows how many sheep?");

test_all(@$_) for (
     [ qw(  goat goats )],
     [ qw( brewery breweries )],
     [ qw( beer beers )],
     [ qw( sheep sheep )],
 );

sub test_all {
    my ($s, $p) = @_;
    is( to_S($p), $s, "$p to singular is $s");
    is( to_S($s), $s, "$s is already singular");
    is( to_PL($s), $p, "Force $s to plural");
    is( to_PL($p), $p, "Force $p to plural");
}

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

