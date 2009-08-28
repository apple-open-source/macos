
use strict;
use warnings;

use Test::More tests => 8;

BEGIN {
    use_ok('MRO::Compat');
}

{
    package AAA; our @ISA = qw//; use mro 'dfs';
    package BBB; our @ISA = qw/AAA/; use mro 'dfs';
    package CCC; our @ISA = qw/AAA/; use mro 'dfs';
    package DDD; our @ISA = qw/AAA/; use mro 'dfs';
    package EEE; our @ISA = qw/BBB CCC DDD/; use mro 'dfs';
    package FFF; our @ISA = qw/EEE DDD/; use mro 'dfs';
    package GGG; our @ISA = qw/FFF/; use mro 'dfs';

    package AAA3; our @ISA = qw//;
    sub testsub { return $_[0] . '_first_in_dfs' }
    package BBB3; our @ISA = qw/AAA3/;
    package CCC3; our @ISA = qw/AAA3/;
    sub testsub { return $_[0] . '_first_in_c3' }
    package DDD3; our @ISA = qw/AAA3/;
    package EEE3; our @ISA = qw/BBB3 CCC3 DDD3/;
    package FFF3; our @ISA = qw/EEE3 DDD3/; use mro 'c3';
    package GGG3; our @ISA = qw/FFF3/; use mro 'c3';
}

is_deeply(
  mro::get_linear_isa('GGG'),
  [ 'GGG', 'FFF', 'EEE', 'BBB', 'AAA', 'CCC', 'DDD' ],
  "get_linear_isa for GGG",
);

is_deeply(
  mro::get_linear_isa('GGG3'),
  [ 'GGG3', 'FFF3', 'EEE3', 'BBB3', 'CCC3', 'DDD3', 'AAA3' ],
  "get_linear_isa for GGG3",
);

SKIP: {
    skip "Does not work like this on 5.9.5+", 1 if $] > 5.009_004;
    is(FFF3->testsub(), 'FFF3_first_in_dfs', 'dfs resolution pre-init');
}

Class::C3::initialize();

is(FFF3->testsub(), 'FFF3_first_in_c3', 'c3 resolution post-init');

mro::set_mro('FFF3', 'dfs');
is_deeply(
  mro::get_linear_isa('FFF3'),
  [ 'FFF3', 'EEE3', 'BBB3', 'AAA3', 'CCC3', 'DDD3' ],
  "get_linear_isa for FFF3 (dfs)",
);

is(FFF3->testsub(), 'FFF3_first_in_dfs', 'dfs resolution post- setmro dfs');

is_deeply(
  mro::get_linear_isa('GGG3'),
  [ 'GGG3', 'FFF3', 'EEE3', 'BBB3', 'CCC3', 'DDD3', 'AAA3' ],
  "get_linear_isa for GGG3 (still c3)",
);
