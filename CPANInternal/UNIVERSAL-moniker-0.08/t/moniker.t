#!/usr/bin/perl
use Test::More tests => 4;

require_ok 'UNIVERSAL::moniker';
is Bpm::User->moniker, "user";
is +(bless {}, "Bpm::Customer")->moniker, "customer";

SKIP: {
	skip "You don't seem to have Lingua::EN::Inflect installed", 1
		unless eval {require Lingua::EN::Inflect};
	is Bpm::Octopus->plural_moniker, "octopuses";
}
