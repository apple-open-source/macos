#!perl

use Test::More tests => 3;
use strict;
use warnings;
use Sub::Identify ();

sub MODIFY_CODE_ATTRIBUTES {
    my ($class, $subref, @attributed) = @_;
    local $TODO = 1;
    is(Sub::Identify::sub_fullname($subref), 'main::foo', 'half compiled');
    return ();
}

sub foo : MyAttribute {}

BEGIN {
    is(Sub::Identify::sub_fullname(\&foo), 'main::foo', 'full compiled');
}

is(Sub::Identify::sub_fullname(\&foo), 'main::foo', 'runtime');
