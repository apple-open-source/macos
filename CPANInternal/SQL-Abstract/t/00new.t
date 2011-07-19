#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use SQL::Abstract::Test import => ['is_same_sql_bind'];

#LDNOTE: renamed all "bind" into "where" because that's what they are

my @handle_tests = (
      #1
      {
              args => {logic => 'OR'},
#              stmt => 'SELECT * FROM test WHERE ( a = ? OR b = ? )'
# LDNOTE: modified the line above (changing the test suite!!!) because
# the test was not consistent with the doc: hashrefs should not be
# influenced by the current logic, they always mean 'AND'. So 
# { a => 4, b => 0} should ALWAYS mean ( a = ? AND b = ? ).
#
# acked by RIBASUSHI
              stmt => 'SELECT * FROM test WHERE ( a = ? AND b = ? )'
      },
      #2
      {
              args => {},
              stmt => 'SELECT * FROM test WHERE ( a = ? AND b = ? )'
      },
      #3
      {
              args => {case => "upper"},
              stmt => 'SELECT * FROM test WHERE ( a = ? AND b = ? )'
      },
      #4
      {
              args => {case => "upper", cmp => "="},
              stmt => 'SELECT * FROM test WHERE ( a = ? AND b = ? )'
      },
      #5
      {
              args => {cmp => "=", logic => 'or'},
# LDNOTE idem
#              stmt => 'SELECT * FROM test WHERE ( a = ? OR b = ? )'
# acked by RIBASUSHI
              stmt => 'SELECT * FROM test WHERE ( a = ? AND b = ? )'
      },
      #6
      {
              args => {cmp => "like"},
              stmt => 'SELECT * FROM test WHERE ( a LIKE ? AND b LIKE ? )'
      },
      #7
      {
              args => {logic => "or", cmp => "like"},
# LDNOTE idem
#              stmt => 'SELECT * FROM test WHERE ( a LIKE ? OR b LIKE ? )'
# acked by RIBASUSHI
              stmt => 'SELECT * FROM test WHERE ( a LIKE ? AND b LIKE ? )'
      },
      #8
      {
              args => {case => "lower"},
              stmt => 'select * from test where ( a = ? and b = ? )'
      },
      #9
      {
              args => {case => "lower", cmp => "="},
              stmt => 'select * from test where ( a = ? and b = ? )'
      },
      #10
      {
              args => {case => "lower", cmp => "like"},
              stmt => 'select * from test where ( a like ? and b like ? )'
      },
      #11
      {
              args => {case => "lower", convert => "lower", cmp => "like"},
              stmt => 'select * from test where ( lower(a) like lower(?) and lower(b) like lower(?) )'
      },
      #12
      {
              args => {convert => "Round"},
              stmt => 'SELECT * FROM test WHERE ( ROUND(a) = ROUND(?) AND ROUND(b) = ROUND(?) )',
      },
      #13
      {
              args => {convert => "lower"},
              stmt => 'SELECT * FROM test WHERE ( ( LOWER(ticket) = LOWER(?) ) OR ( LOWER(hostname) = LOWER(?) ) OR ( LOWER(taco) = LOWER(?) ) OR ( LOWER(salami) = LOWER(?) ) )',
              where => [ { ticket => 11 }, { hostname => 11 }, { taco => 'salad' }, { salami => 'punch' } ],
      },
      #14
      {
              args => {convert => "upper"},
              stmt => 'SELECT * FROM test WHERE ( ( UPPER(hostname) IN ( UPPER(?), UPPER(?), UPPER(?), UPPER(?) ) AND ( ( UPPER(ticket) = UPPER(?) ) OR ( UPPER(ticket) = UPPER(?) ) OR ( UPPER(ticket) = UPPER(?) ) ) ) OR ( UPPER(tack) BETWEEN UPPER(?) AND UPPER(?) ) OR ( ( ( UPPER(a) = UPPER(?) ) OR ( UPPER(a) = UPPER(?) ) OR ( UPPER(a) = UPPER(?) ) ) AND ( ( UPPER(e) != UPPER(?) ) OR ( UPPER(e) != UPPER(?) ) ) AND UPPER(q) NOT IN ( UPPER(?), UPPER(?), UPPER(?), UPPER(?), UPPER(?), UPPER(?), UPPER(?) ) ) )',
              where => [ { ticket => [11, 12, 13], 
                           hostname => { in => ['ntf', 'avd', 'bvd', '123'] } },
                        { tack => { between => [qw/tick tock/] } },
                        { a => [qw/b c d/], 
                          e => { '!=', [qw(f g)] }, 
                          q => { 'not in', [14..20] } } ],
      },
);


plan tests => (1 + scalar(@handle_tests));

use_ok('SQL::Abstract');

for (@handle_tests) {
  local $" = ', ';
  #print "creating a handle with args ($_->{args}): ";
  my $sql  = SQL::Abstract->new($_->{args});
  my $where = $_->{where} || { a => 4, b => 0};
  my($stmt, @bind) = $sql->select('test', '*', $where);

  # LDNOTE: this original test suite from NWIGER did no comparisons
  # on @bind values, just checking if @bind is nonempty.
  # So here we just fake a [1] bind value for the comparison.
  is_same_sql_bind($stmt, [@bind ? 1 : 0], $_->{stmt}, [1]);
}


