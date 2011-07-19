#!/usr/bin/perl

use strict;
use warnings;
use Test::More;
use Test::Exception;
use SQL::Abstract::Test import => ['is_same_sql_bind'];

use Data::Dumper;
use SQL::Abstract;

my @in_between_tests = (
  {
    where => { x => { -between => [1, 2] } },
    stmt => 'WHERE (x BETWEEN ? AND ?)',
    bind => [qw/1 2/],
    test => '-between with two placeholders',
  },
  {
    where => { x => { -between => [\"1", 2] } },
    stmt => 'WHERE (x BETWEEN 1 AND ?)',
    bind => [qw/2/],
    test => '-between with one literal sql arg and one placeholder',
  },
  {
    where => { x => { -between => [1, \"2"] } },
    stmt => 'WHERE (x BETWEEN ? AND 2)',
    bind => [qw/1/],
    test => '-between with one placeholder and one literal sql arg',
  },
  {
    where => { x => { -between => [\'current_date - 1', \'current_date - 0'] } },
    stmt => 'WHERE (x BETWEEN current_date - 1 AND current_date - 0)',
    bind => [],
    test => '-between with two literal sql arguments',
  },
  {
    where => { x => { -between => [ \['current_date - ?', 1], \['current_date - ?', 0] ] } },
    stmt => 'WHERE (x BETWEEN current_date - ? AND current_date - ?)',
    bind => [1, 0],
    test => '-between with two literal sql arguments with bind',
  },
  {
    where => { x => { -between => \['? AND ?', 1, 2] } },
    stmt => 'WHERE (x BETWEEN ? AND ?)',
    bind => [1,2],
    test => '-between with literal sql with placeholders (\["? AND ?", scalar, scalar])',
  },
  {
    where => { x => { -between => \["'something' AND ?", 2] } },
    stmt => "WHERE (x BETWEEN 'something' AND ?)",
    bind => [2],
    test => '-between with literal sql with one literal arg and one placeholder (\["\'something\' AND ?", scalar])',
  },
  {
    where => { x => { -between => \["? AND 'something'", 1] } },
    stmt => "WHERE (x BETWEEN ? AND 'something')",
    bind => [1],
    test => '-between with literal sql with one placeholder and one literal arg (\["? AND \'something\'", scalar])',
  },
  {
    where => { x => { -between => \"'this' AND 'that'" } },
    stmt => "WHERE (x BETWEEN 'this' AND 'that')",
    bind => [],
    test => '-between with literal sql with a literal (\"\'this\' AND \'that\'")',
  },
  {
    where => {
      start0 => { -between => [ 1, 2 ] },
      start1 => { -between => \["? AND ?", 1, 2] },
      start2 => { -between => \"lower(x) AND upper(y)" },
      start3 => { -between => [
        \"lower(x)",
        \["upper(?)", 'stuff' ],
      ] },
    },
    stmt => "WHERE (
          ( start0 BETWEEN ? AND ?                )
      AND ( start1 BETWEEN ? AND ?                )
      AND ( start2 BETWEEN lower(x) AND upper(y)  )
      AND ( start3 BETWEEN lower(x) AND upper(?)  )
    )",
    bind => [1, 2, 1, 2, 'stuff'],
    test => '-between POD test',
  },

  {
    parenthesis_significant => 1,
    where => { x => { -in => [ 1 .. 3] } },
    stmt => "WHERE ( x IN (?, ?, ?) )",
    bind => [ 1 .. 3],
    test => '-in with an array of scalars',
  },
  {
    parenthesis_significant => 1,
    where => { x => { -in => [] } },
    stmt => "WHERE ( 0=1 )",
    bind => [],
    test => '-in with an empty array',
  },
  {
    parenthesis_significant => 1,
    where => { x => { -in => \'( 1,2,lower(y) )' } },
    stmt => "WHERE ( x IN ( 1,2,lower(y) ) )",
    bind => [],
    test => '-in with a literal scalarref',
  },
  {
    parenthesis_significant => 1,
    where => { x => { -in => \['( ( ?,?,lower(y) ) )', 1, 2] } },
    stmt => "WHERE ( x IN ( ?,?,lower(y) ) )",  # note that outer parens are opened even though literal was requested (RIBASUSHI)
    bind => [1, 2],
    test => '-in with a literal arrayrefref',
  },
  {
    parenthesis_significant => 1,
    where => {
      customer => { -in => \[
        'SELECT cust_id FROM cust WHERE balance > ?',
        2000,
      ]},
      status => { -in => \'SELECT status_codes FROM states' },
    },
    stmt => "
      WHERE ((
            customer IN ( SELECT cust_id FROM cust WHERE balance > ? )
        AND status IN ( SELECT status_codes FROM states )
      ))
    ",
    bind => [2000],
    test => '-in POD test',
  },
);

plan tests => @in_between_tests*4;

for my $case (@in_between_tests) {
  TODO: {
    local $TODO = $case->{todo} if $case->{todo};
    local $SQL::Abstract::Test::parenthesis_significant = $case->{parenthesis_significant};

    local $Data::Dumper::Terse = 1;

    lives_ok (sub {

      my @w;
      local $SIG{__WARN__} = sub { push @w, @_ };
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
      is (@w, 0, $case->{test} || 'No warnings within in-between tests')
        || diag join "\n", 'Emitted warnings:', @w;
    }, "$case->{test} doesn't die");
  }
}
