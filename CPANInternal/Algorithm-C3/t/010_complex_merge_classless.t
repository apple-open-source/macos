#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 12;

BEGIN {
    use_ok('Algorithm::C3');
}

=pod

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

my $foo = {
  k => [qw(j i)],
  j => [qw(f)],
  i => [qw(h f)],
  h => [qw(g)],
  g => [qw(d)],
  f => [qw(e)],
  e => [qw(d)],
  d => [qw(a b c)],
  c => [],
  b => [],
  a => [],
};

sub supers {
  return @{ $foo->{ $_[0] } };
}

is_deeply(
    [ Algorithm::C3::merge('a', \&supers) ],
    [ qw(a) ],
    '... got the right C3 merge order for a');

is_deeply(
    [ Algorithm::C3::merge('b', \&supers) ],
    [ qw(b) ],
    '... got the right C3 merge order for b');

is_deeply(
    [ Algorithm::C3::merge('c', \&supers) ],
    [ qw(c) ],
    '... got the right C3 merge order for c');

is_deeply(
    [ Algorithm::C3::merge('d', \&supers) ],
    [ qw(d a b c) ],
    '... got the right C3 merge order for d');

is_deeply(
    [ Algorithm::C3::merge('e', \&supers) ],
    [ qw(e d a b c) ],
    '... got the right C3 merge order for e');

is_deeply(
    [ Algorithm::C3::merge('f', \&supers) ],
    [ qw(f e d a b c) ],
    '... got the right C3 merge order for f');

is_deeply(
    [ Algorithm::C3::merge('g', \&supers) ],
    [ qw(g d a b c) ],
    '... got the right C3 merge order for g');

is_deeply(
    [ Algorithm::C3::merge('h', \&supers) ],
    [ qw(h g d a b c) ],
    '... got the right C3 merge order for h');

is_deeply(
    [ Algorithm::C3::merge('i', \&supers) ],
    [ qw(i h g f e d a b c) ],
    '... got the right C3 merge order for i');

is_deeply(
    [ Algorithm::C3::merge('j', \&supers) ],
    [ qw(j f e d a b c) ],
    '... got the right C3 merge order for j');

is_deeply(
    [ Algorithm::C3::merge('k', \&supers) ],
    [ qw(k j i h g f e d a b c) ],
    '... got the right C3 merge order for k');
