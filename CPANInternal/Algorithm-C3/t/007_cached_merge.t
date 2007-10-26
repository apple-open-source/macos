#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 12;

BEGIN {
    use_ok('Algorithm::C3');
}

=pod

Just like 006_complex_merge, but with the caching turned on.

This example is taken from: http://rt.cpan.org/Public/Bug/Display.html?id=20879

               ---     ---     ---
Level 5     8 | A | 9 | B | A | C |    (More General)
               ---     ---     ---       V
                  \     |     /          |
                   \    |    /           |
                    \   |   /            |
                     \  |  /             |
                       ---               |
Level 4             7 | D |              |
                       ---               |
                      /   \              |
                     /     \             |
                  ---       ---          |
Level 3        4 | G |   6 | E |         |
                  ---       ---          |
                   |         |           |
                   |         |           |
                  ---       ---          |
Level 2        3 | H |   5 | F |         |
                  ---       ---          |
                      \   /  |           |
                       \ /   |           |
                        \    |           |
                       / \   |           |
                      /   \  |           |
                  ---       ---          |
Level 1        1 | J |   2 | I |         |
                  ---       ---          |
                    \       /            |
                     \     /             |
                       ---               v
Level 0             0 | K |            (More Specialized)
                       ---


0123456789A
KJIHGFEDABC

=cut

{
    package Test::A;
    sub x { 1 }

    package Test::B;
    sub x { 1 }

    package Test::C;
    sub x { 1 }

    package Test::D;
    use base qw/Test::A Test::B Test::C/;

    package Test::E;
    use base qw/Test::D/;

    package Test::F;
    use base qw/Test::E/;

    package Test::G;
    use base qw/Test::D/;

    package Test::H;
    use base qw/Test::G/;

    package Test::I;
    use base qw/Test::H Test::F/;

    package Test::J;
    use base qw/Test::F/;

    package Test::K;
    use base qw/Test::J Test::I/;
}

sub supers {
    no strict 'refs';
    @{$_[0] . '::ISA'};
}

my %cache;

is_deeply(
    [ Algorithm::C3::merge('Test::A', \&supers, \%cache) ],
    [ qw(Test::A) ],
    '... got the right C3 merge order for Test::A');

is_deeply(
    [ Algorithm::C3::merge('Test::B', \&supers, \%cache) ],
    [ qw(Test::B) ],
    '... got the right C3 merge order for Test::B');

is_deeply(
    [ Algorithm::C3::merge('Test::C', \&supers, \%cache) ],
    [ qw(Test::C) ],
    '... got the right C3 merge order for Test::C');

is_deeply(
    [ Algorithm::C3::merge('Test::D', \&supers, \%cache) ],
    [ qw(Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::D');

is_deeply(
    [ Algorithm::C3::merge('Test::E', \&supers, \%cache) ],
    [ qw(Test::E Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::E');

is_deeply(
    [ Algorithm::C3::merge('Test::F', \&supers, \%cache) ],
    [ qw(Test::F Test::E Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::F');

is_deeply(
    [ Algorithm::C3::merge('Test::G', \&supers, \%cache) ],
    [ qw(Test::G Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::G');

is_deeply(
    [ Algorithm::C3::merge('Test::H', \&supers, \%cache) ],
    [ qw(Test::H Test::G Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::H');

is_deeply(
    [ Algorithm::C3::merge('Test::I', \&supers, \%cache) ],
    [ qw(Test::I Test::H Test::G Test::F Test::E Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::I');

is_deeply(
    [ Algorithm::C3::merge('Test::J', \&supers, \%cache) ],
    [ qw(Test::J Test::F Test::E Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::J');

is_deeply(
    [ Algorithm::C3::merge('Test::K', \&supers, \%cache) ],
    [ qw(Test::K Test::J Test::I Test::H Test::G Test::F Test::E Test::D Test::A Test::B Test::C) ],
    '... got the right C3 merge order for Test::K');
