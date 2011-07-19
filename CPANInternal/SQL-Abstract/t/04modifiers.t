#!/usr/bin/perl

use strict;
use warnings;
use Test::More;
use Test::Exception;
use SQL::Abstract::Test import => ['is_same_sql_bind'];

use Data::Dumper;
use SQL::Abstract;
use Clone;

=begin
Test -and -or and -nest modifiers, assuming the following:

  * Modifiers are respected in both hashrefs and arrayrefs (with the obvious
    limitation of one modifier type per hahsref)
  * When in condition context i.e. where => { -or { a = 1 } }, each modifier
    affects only the immediate element following it.
  * When in column multi-condition context i.e. 
    where => { x => { '!=', [-and, [qw/1 2 3/]] } }, a modifier affects the
    OUTER ARRAYREF if and only if it is the first element of said ARRAYREF

=cut

# no warnings (the -or/-and => { } warning is silly, there is nothing wrong with such usage)
my $and_or_args = {
  and => { stmt => 'WHERE a = ? AND b = ?', bind => [qw/1 2/] },
  or => { stmt => 'WHERE a = ? OR b = ?', bind => [qw/1 2/] },
  or_and => { stmt => 'WHERE ( foo = ? OR bar = ? ) AND baz = ? ', bind => [qw/1 2 3/] },
  or_or => { stmt => 'WHERE foo = ? OR bar = ? OR baz = ?', bind => [qw/1 2 3/] },
  and_or => { stmt => 'WHERE ( foo = ? AND bar = ? ) OR baz = ?', bind => [qw/1 2 3/] },
};

my @and_or_tests = (
  # basic tests
  {
    where => { -and => [a => 1, b => 2] },
    %{$and_or_args->{and}},
  },
  {
    where => [ -and => [a => 1, b => 2] ],
    %{$and_or_args->{and}},
  },
  {
    where => { -or => [a => 1, b => 2] },
    %{$and_or_args->{or}},
  },
  {
    where => [ -or => [a => 1, b => 2] ],
    %{$and_or_args->{or}},
  },

  {
    where => { -and => {a => 1, b => 2} },
    %{$and_or_args->{and}},
  },
  {
    where => [ -and => {a => 1, b => 2} ],
    %{$and_or_args->{and}},
  },
  {
    where => { -or => {a => 1, b => 2} },
    %{$and_or_args->{or}},
  },
  {
    where => [ -or => {a => 1, b => 2} ],
    %{$and_or_args->{or}},
  },

  # test modifiers within hashrefs 
  {
    where => { -or => [
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]},
    %{$and_or_args->{or_or}},
  },
  {
    where => { -and => [
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]},
    %{$and_or_args->{or_and}},
  },

  # test modifiers within arrayrefs 
  {
    where => [ -or => [
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]],
    %{$and_or_args->{or_or}},
  },
  {
    where => [ -and => [
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]],
    %{$and_or_args->{or_and}},
  },

  # test ambiguous modifiers within hashrefs (op extends to to immediate RHS only)
  {
    where => { -and => [ -or =>
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]},
    %{$and_or_args->{or_and}},
  },
  {
    where => { -or => [ -and =>
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]},
    %{$and_or_args->{and_or}},
  },

  # test ambiguous modifiers within arrayrefs (op extends to to immediate RHS only)
  {
    where => [ -and => [ -or =>
      [ foo => 1, bar => 2 ],
      baz => 3,
    ]],
    %{$and_or_args->{or_and}},
  },
  {
    where => [ -or => [ -and =>
      [ foo => 1, bar => 2 ],
      baz => 3
    ]],
    %{$and_or_args->{and_or}},
  },

  # test column multi-cond in arrayref (useless example)
  {
    where => { x => [ -and => (1 .. 3) ] },
    stmt => 'WHERE x = ? AND x = ? AND x = ?',
    bind => [1..3],
  },
  # test column multi-cond in arrayref (more useful)
  {
    where => { x => [ -and => {'!=' => 1}, {'!=' => 2}, {'!=' => 3} ] },
    stmt => 'WHERE x != ? AND x != ? AND x != ?',
    bind => [1..3],
  },
  # test column multi-cond in arrayref (even more useful)
  {
    where => { x => { '!=' => [ -and => (1 .. 3) ] } },
    stmt => 'WHERE x != ? AND x != ? AND x != ?',
    bind => [1..3],
  },

  # the -or should affect only the inner hashref, as we are not in an outer arrayref
  {
    where => { x => {
      -or => { '!=', 1, '>=', 2 }, -like => 'x%'
    }},
    stmt => 'WHERE x LIKE ? AND ( x != ? OR x >= ? )',
    bind => [qw/x% 1 2/],
  },

  # the -and should affect the OUTER arrayref, while the internal structures remain intact
  {
    where => { x => [ 
      -and => [ 1, 2 ], { -like => 'x%' } 
    ]},
    stmt => 'WHERE (x = ? OR x = ?) AND x LIKE ?',
    bind => [qw/1 2 x%/],
  },

  {
    where => { -and => [a => 1, b => 2], x => 9, -or => { c => 3, d => 4 } },
    stmt => 'WHERE a = ? AND b = ? AND ( c = ? OR d = ? ) AND x = ?',
    bind => [qw/1 2 3 4 9/],
  },

  {
    where => { -and => [a => 1, b => 2, k => [11, 12] ], x => 9, -or => { c => 3, d => 4, l => { '=' => [21, 22] } } },
    stmt => 'WHERE a = ? AND b = ? AND (k = ? OR k = ?) AND (c = ? OR d = ? OR (l = ? OR l = ?) ) AND x = ?',
    bind => [qw/1 2 11 12 3 4 21 22 9/],
  },

  {
    where => { -or => [a => 1, b => 2, k => [11, 12] ], x => 9, -and => { c => 3, d => 4, l => { '=' => [21, 22] } } },
    stmt => 'WHERE c = ? AND d = ? AND ( l = ? OR l = ?) AND (a = ? OR b = ? OR k = ? OR k = ?) AND x = ?',
    bind => [qw/3 4 21 22 1 2 11 12 9/],
  },

  {
    where => [ -or => [a => 1, b => 2], -or => { c => 3, d => 4}, e => 5, -and => [ f => 6, g => 7], [ h => 8, i => 9, -and => [ k => 10, l => 11] ], { m => 12, n => 13 }],
    stmt => 'WHERE a = ? OR b = ? OR c = ? OR d = ? OR e = ? OR ( f = ? AND g = ?) OR h = ? OR i = ? OR ( k = ? AND l = ? ) OR (m = ? AND n = ?)',
    bind => [1 .. 13],
  },

  {
    # explicit OR logic in arrays should leave everything intact
    args => { logic => 'or' },
    where => { -and => [a => 1, b => 2, k => [11, 12] ], x => 9, -or => { c => 3, d => 4, l => { '=' => [21, 22] } }  },
    stmt => 'WHERE a = ? AND b = ? AND (k = ? OR k = ?) AND ( c = ? OR d = ? OR l = ? OR l = ? ) AND x = ? ',
    bind => [qw/1 2 11 12 3 4 21 22 9/],
  },

  {
    # flip logic in arrays except where excplicitly requested otherwise
    args => { logic => 'and' },
    where => [ -or => [a => 1, b => 2], -or => { c => 3, d => 4}, e => 5, -and => [ f => 6, g => 7], [ h => 8, i => 9, -and => [ k => 10, l => 11] ], { m => 12, n => 13 }],
    stmt => 'WHERE (a = ? OR b = ?) AND (c = ? OR d = ?) AND e = ? AND f = ? AND g = ? AND h = ? AND i = ? AND k = ? AND l = ? AND m = ? AND n = ?',
    bind => [1 .. 13],
  },

  # 1st -and is in column mode, thus flips the entire array, whereas the 
  # 2nd one is just a condition modifier
  {
    where => [
      col => [ -and => {'<' => 123}, {'>' => 456 }, {'!=' => 789} ],
      -and => [
        col2 => [ -or => { -like => 'crap' }, { -like => 'crop' } ],
        col3 => [ -and => { -like => 'chap' }, { -like => 'chop' } ],
      ],
    ],
    stmt => 'WHERE
      (col < ? AND col > ? AND col != ?)
        OR
      (
        ( col2 LIKE ? OR col2 LIKE ? )
          AND
        ( col3 LIKE ? AND col3 LIKE ? )
      )
    ',
    bind => [qw/123 456 789 crap crop chap chop/],
  },

  ##########
  # some corner cases by ldami (some produce useless SQL, just for clarification on 1.5 direction)
  #

  {
    where => { foo => [
      -and => [ { -like => 'foo%'}, {'>' => 'moo'} ],
      { -like => '%bar', '<' => 'baz'},
      [ {-like => '%alpha'}, {-like => '%beta'} ],
      [ {'!=' => 'toto', '=' => 'koko'} ],
    ] },
    stmt => 'WHERE (foo LIKE ? OR foo > ?) AND (foo LIKE ? AND foo < ?) AND (foo LIKE ? OR foo LIKE ?) AND (foo != ? AND foo = ?)',
    bind => [qw/foo% moo %bar baz %alpha %beta toto koko/],
  },

  {
    where => [
      -and => [a => 1, b => 2],
      -or => [c => 3, d => 4],
      e => [-and => {-like => 'foo%'}, {-like => '%bar'} ],
    ],
    stmt => 'WHERE (a = ? AND b = ?) OR c = ? OR d = ? OR (e LIKE ? AND e LIKE ?)',
    bind => [qw/1 2 3 4 foo% %bar/],
  },

  # -or has nothing to flip
  {
    where => [-and => [{foo => 1}, {bar => 2}, -or => {baz => 3}] ],
    stmt => 'WHERE foo = ? AND bar = ? AND baz = ?',
    bind => [1 .. 3],
  },
  {
    where => [-and => [{foo => 1}, {bar => 2}, -or => {baz => 3, woz => 4} ] ],
    stmt => 'WHERE foo = ? AND bar = ? AND (baz = ? OR woz = ?)',
    bind => [1 .. 4],
  },

  # -and has only 1 following element, thus all still ORed
  {
    where => { col => [ -and => [{'<' => 123}, {'>' => 456 }, {'!=' => 789}] ] },
    stmt => 'WHERE col < ? OR col > ? OR col != ?',
    bind => [qw/123 456 789/],
  },

  # flipping array logic affects both column value and condition arrays
  {
    args => { logic => 'and' },
    where => [ col => [ {'<' => 123}, {'>' => 456 }, {'!=' => 789} ], col2 => 0 ],
    stmt => 'WHERE col < ? AND col > ? AND col != ? AND col2 = ?',
    bind => [qw/123 456 789 0/],
  },

  # flipping array logic with explicit -and works
  {
    args => { logic => 'and' },
    where => [ col => [ -and => {'<' => 123}, {'>' => 456 }, {'!=' => 789} ], col2 => 0 ],
    stmt => 'WHERE col < ? AND col > ? AND col != ? AND col2 = ?',
    bind => [qw/123 456 789 0/],
  },
  # flipping array logic with explicit -or flipping it back
  {
    args => { logic => 'and' },
    where => [ col => [ -or => {'<' => 123}, {'>' => 456 }, {'!=' => 789} ], col2 => 0 ],
    stmt => 'WHERE (col < ? OR col > ? OR col != ?) AND col2 = ?',
    bind => [qw/123 456 789 0/],
  },
);

# modN and mod_N were a bad design decision - they go away in SQLA2, warn now
my @numbered_mods = (
  {
    backcompat => {
      -and => [a => 10, b => 11],
      -and2 => [ c => 20, d => 21 ],
      -nest => [ x => 1 ],
      -nest2 => [ y => 2 ],
      -or => { m => 7, n => 8 },
      -or2 => { m => 17, n => 18 },
    },
    correct => { -and => [
      -and => [a => 10, b => 11],
      -and => [ c => 20, d => 21 ],
      -nest => [ x => 1 ],
      -nest => [ y => 2 ],
      -or => { m => 7, n => 8 },
      -or => { m => 17, n => 18 },
    ] },
  },
  {
    backcompat => {
      -and2 => [a => 10, b => 11],
      -and_3 => [ c => 20, d => 21 ],
      -nest2 => [ x => 1 ],
      -nest_3 => [ y => 2 ],
      -or2 => { m => 7, n => 8 },
      -or_3 => { m => 17, n => 18 },
    },
    correct => [ -and => [
      -and => [a => 10, b => 11],
      -and => [ c => 20, d => 21 ],
      -nest => [ x => 1 ],
      -nest => [ y => 2 ],
      -or => { m => 7, n => 8 },
      -or => { m => 17, n => 18 },
    ] ],
  },
);

my @nest_tests = (
 {
   where => {a => 1, -nest => [b => 2, c => 3]},
   stmt  => 'WHERE ( ( (b = ? OR c = ?) AND a = ? ) )',
   bind  => [qw/2 3 1/],
 },
 {
   where => {a => 1, -nest => {b => 2, c => 3}},
   stmt  => 'WHERE ( ( (b = ? AND c = ?) AND a = ? ) )',
   bind  => [qw/2 3 1/],
 },
 {
   where => {a => 1, -or => {-nest => {b => 2, c => 3}}},
   stmt  => 'WHERE ( ( (b = ? AND c = ?) AND a = ? ) )',
   bind  => [qw/2 3 1/],
 },
 {
   where => {a => 1, -or => {-nest => [b => 2, c => 3]}},
   stmt  => 'WHERE ( ( (b = ? OR c = ?) AND a = ? ) )',
   bind  => [qw/2 3 1/],
 },
 {
   where => {a => 1, -nest => {-or => {b => 2, c => 3}}},
   stmt  => 'WHERE ( ( (b = ? OR c = ?) AND a = ? ) )',
   bind  => [qw/2 3 1/],
 },
 {
   where => [a => 1, -nest => {b => 2, c => 3}, -nest => [d => 4, e => 5]],
   stmt  => 'WHERE ( ( a = ? OR ( b = ? AND c = ? ) OR ( d = ? OR e = ? ) ) )',
   bind  => [qw/1 2 3 4 5/],
 },
);

plan tests => @and_or_tests*4 + @numbered_mods*4 + @nest_tests*2;

for my $case (@and_or_tests) {
  TODO: {
    local $TODO = $case->{todo} if $case->{todo};

    local $Data::Dumper::Terse = 1;

    my @w;
    local $SIG{__WARN__} = sub { push @w, @_ };

    my $sql = SQL::Abstract->new ($case->{args} || {});
    my $where_copy = Clone::clone ($case->{where});

    lives_ok (sub { 
      my ($stmt, @bind) = $sql->where($case->{where});
      is_same_sql_bind(
        $stmt,
        \@bind,
        $case->{stmt},
        $case->{bind},
      )
        || diag "Search term:\n" . Dumper $case->{where};
    });
    is (@w, 0, 'No warnings within and-or tests')
      || diag join "\n", 'Emitted warnings:', @w;

    is_deeply ($case->{where}, $where_copy, 'Where conditions unchanged');
  }
}

for my $case (@nest_tests) {
  TODO: {
    local $TODO = $case->{todo} if $case->{todo};

    local $SQL::Abstract::Test::parenthesis_significant = 1;
    local $Data::Dumper::Terse = 1;

    my $sql = SQL::Abstract->new ($case->{args} || {});
    lives_ok (sub {
      my ($stmt, @bind) = $sql->where($case->{where});
      is_same_sql_bind(
        $stmt,
        \@bind,
        $case->{stmt},
        $case->{bind},
      )
        || diag "Search term:\n" . Dumper $case->{where};
    });
  }
}



my $w_str = "\QUse of [and|or|nest]_N modifiers is deprecated and will be removed in SQLA v2.0\E";
for my $case (@numbered_mods) {
  TODO: {
    local $TODO = $case->{todo} if $case->{todo};

    local $Data::Dumper::Terse = 1;

    my @w;
    local $SIG{__WARN__} = sub { push @w, @_ };
    my $sql = SQL::Abstract->new ($case->{args} || {});
    lives_ok (sub {
      my ($old_s, @old_b) = $sql->where($case->{backcompat});
      my ($new_s, @new_b) = $sql->where($case->{correct});
      is_same_sql_bind(
        $old_s, \@old_b,
        $new_s, \@new_b,
        'Backcompat and the correct(tm) syntax result in identical statements',
      ) || diag "Search terms:\n" . Dumper {
          backcompat => $case->{backcompat},
          correct => $case->{correct},
        };
    });

    ok (@w, 'Warnings were emitted about a mod_N construct');

    my @non_match;
    for (@w) {
      push @non_match, $_ if ($_ !~ /$w_str/);
    }

    is (@non_match, 0, 'All warnings match the deprecation message')
      || diag join "\n", 'Rogue warnings:', @non_match;
  }
}

