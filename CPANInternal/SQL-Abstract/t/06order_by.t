#!/usr/bin/perl

use strict;
use warnings;
use Test::More;
use Test::Exception;

use SQL::Abstract;

use SQL::Abstract::Test import => ['is_same_sql_bind'];
my @cases = 
  (
   {
    given => \'colA DESC',
    expects => ' ORDER BY colA DESC',
    expects_quoted => ' ORDER BY colA DESC',
   },
   {
    given => 'colA',
    expects => ' ORDER BY colA',
    expects_quoted => ' ORDER BY `colA`',
   },
   {  # it may look odd, but this is the desired behaviour (mst)
    given => 'colA DESC',
    expects => ' ORDER BY colA DESC',
    expects_quoted => ' ORDER BY `colA DESC`',
   },
   {
    given => [qw/colA colB/],
    expects => ' ORDER BY colA, colB',
    expects_quoted => ' ORDER BY `colA`, `colB`',
   },
   {  # it may look odd, but this is the desired behaviour (mst)
    given => ['colA ASC', 'colB DESC'],
    expects => ' ORDER BY colA ASC, colB DESC',
    expects_quoted => ' ORDER BY `colA ASC`, `colB DESC`',
   },
   {
    given => {-asc => 'colA'},
    expects => ' ORDER BY colA ASC',
    expects_quoted => ' ORDER BY `colA` ASC',
   },
   {
    given => {-desc => 'colB'},
    expects => ' ORDER BY colB DESC',
    expects_quoted => ' ORDER BY `colB` DESC',
   },
   {
    given => [{-asc => 'colA'}, {-desc => 'colB'}],
    expects => ' ORDER BY colA ASC, colB DESC',
    expects_quoted => ' ORDER BY `colA` ASC, `colB` DESC',
   },
   {
    given => ['colA', {-desc => 'colB'}],
    expects => ' ORDER BY colA, colB DESC',
    expects_quoted => ' ORDER BY `colA`, `colB` DESC',
   },
   {
    given => undef,
    expects => '',
    expects_quoted => '',
   },

   {
    given => [{-desc => [ qw/colA colB/ ] }],
    expects => ' ORDER BY colA DESC, colB DESC',
    expects_quoted => ' ORDER BY `colA` DESC, `colB` DESC',
   },
   {
    given => [{-desc => [ qw/colA colB/ ] }, {-asc => 'colC'}],
    expects => ' ORDER BY colA DESC, colB DESC, colC ASC',
    expects_quoted => ' ORDER BY `colA` DESC, `colB` DESC, `colC` ASC',
   },
   {
    given => [{-desc => [ qw/colA colB/ ] }, {-asc => [ qw/colC colD/ ] }],
    expects => ' ORDER BY colA DESC, colB DESC, colC ASC, colD ASC',
    expects_quoted => ' ORDER BY `colA` DESC, `colB` DESC, `colC` ASC, `colD` ASC',
   },
   {
    given => [{-desc => [ qw/colA colB/ ] }, {-desc => 'colC' }],
    expects => ' ORDER BY colA DESC, colB DESC, colC DESC',
    expects_quoted => ' ORDER BY `colA` DESC, `colB` DESC, `colC` DESC',
   },
   {
    given => [{ -asc => 'colA' }, { -desc => [qw/colB/] }, { -asc => [qw/colC colD/] }],
    expects => ' ORDER BY colA ASC, colB DESC, colC ASC, colD ASC',
    expects_quoted => ' ORDER BY `colA` ASC, `colB` DESC, `colC` ASC, `colD` ASC',
   },
   {
    given => { -desc => \['colA LIKE ?', 'test'] },
    expects => ' ORDER BY colA LIKE ? DESC',
    expects_quoted => ' ORDER BY colA LIKE ? DESC',
    bind => ['test'],
   },
   {
    given => \['colA LIKE ? DESC', 'test'],
    expects => ' ORDER BY colA LIKE ? DESC',
    expects_quoted => ' ORDER BY colA LIKE ? DESC',
    bind => ['test'],
   },
   {
    given => [ { -asc => \['colA'] }, { -desc => \['colB LIKE ?', 'test'] }, { -asc => \['colC LIKE ?', 'tost'] }],
    expects => ' ORDER BY colA ASC, colB LIKE ? DESC, colC LIKE ? ASC',
    expects_quoted => ' ORDER BY colA ASC, colB LIKE ? DESC, colC LIKE ? ASC',
    bind => [qw/test tost/],
   },
  );


plan tests => (scalar(@cases) * 2) + 2;

my $sql  = SQL::Abstract->new;
my $sqlq = SQL::Abstract->new({quote_char => '`'});

for my $case( @cases) {
  my ($stat, @bind);

  ($stat, @bind) = $sql->_order_by($case->{given});
  is_same_sql_bind (
    $stat,
    \@bind,
    $case->{expects},
    $case->{bind} || [],
  );

  ($stat, @bind) = $sqlq->_order_by($case->{given});
  is_same_sql_bind (
    $stat,
    \@bind,
    $case->{expects_quoted},
    $case->{bind} || [],
  );
}

throws_ok (
  sub { $sql->_order_by({-desc => 'colA', -asc => 'colB' }) },
  qr/hash passed .+ must have exactly one key/,
  'Undeterministic order exception',
);

throws_ok (
  sub { $sql->_order_by({-desc => [ qw/colA colB/ ], -asc => [ qw/colC colD/ ] }) },
  qr/hash passed .+ must have exactly one key/,
  'Undeterministic order exception',
);
