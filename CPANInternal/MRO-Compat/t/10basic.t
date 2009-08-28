
use strict;
use warnings;

use Test::More tests => 11;

BEGIN {
    use_ok('MRO::Compat');
}

{
    package AAA; our @ISA = qw//;
    package BBB; our @ISA = qw/AAA/;
    package CCC; our @ISA = qw/AAA/;
    package DDD; our @ISA = qw/AAA/;
    package EEE; our @ISA = qw/BBB CCC DDD/;
    package FFF; our @ISA = qw/EEE DDD/;
    package GGG; our @ISA = qw/FFF/;
    package UNIVERSAL; our @ISA = qw/DDD/;
}

is_deeply(
  mro::get_linear_isa('GGG'),
  [ 'GGG', 'FFF', 'EEE', 'BBB', 'AAA', 'CCC', 'DDD' ],
  "get_linear_isa for GGG",
);

is_deeply(
  [sort @{mro::get_isarev('GGG')}],
  [],
  "get_isarev for GGG",
);

is_deeply(
  [sort @{mro::get_isarev('DDD')}],
  [ 'EEE', 'FFF', 'GGG', 'UNIVERSAL' ],
  "get_isarev for DDD",
);


is_deeply(
  [sort @{mro::get_isarev('AAA')}],
  [ 'BBB', 'CCC', 'DDD', 'EEE', 'FFF', 'GGG', 'UNIVERSAL' ],
  "get_isarev for AAA",
);

ok(mro::is_universal('UNIVERSAL'), "UNIVERSAL is_universal");
ok(mro::is_universal('DDD'), "DDD is_universal");
ok(mro::is_universal('AAA'), "AAA is_universal");
ok(!mro::is_universal('MRO::Compat'), "MRO::Compat !is_universal");
ok(!mro::is_universal('BBB'), "BBB !is_universal");
ok(!mro::is_universal('FFF'), "FFF !is_universal");
