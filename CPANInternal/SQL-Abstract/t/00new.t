#!/usr/bin/perl -I. -w

use strict;
use vars qw($TESTING);
$TESTING = 1;
use Test;

# use a BEGIN block so we print our plan before SQL::Abstract is loaded
BEGIN { plan tests => 14 }

use SQL::Abstract;

my @handle_tests = (
      #1
      {
              args => {logic => 'OR'},
              stmt => 'SELECT * FROM test WHERE ( a = ? OR b = ? )'
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
              stmt => 'SELECT * FROM test WHERE ( a = ? OR b = ? )'
      },
      #6
      {
              args => {cmp => "like"},
              stmt => 'SELECT * FROM test WHERE ( a LIKE ? AND b LIKE ? )'
      },
      #7
      {
              args => {logic => "or", cmp => "like"},
              stmt => 'SELECT * FROM test WHERE ( a LIKE ? OR b LIKE ? )'
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
              bind => [ { ticket => 11 }, { hostname => 11 }, { taco => 'salad' }, { salami => 'punch' } ],
      },
      #14
      {
              args => {convert => "upper"},
              stmt => 'SELECT * FROM test WHERE ( ( UPPER(hostname) IN ( UPPER(?), UPPER(?), UPPER(?), UPPER(?) ) AND ( ( UPPER(ticket) = UPPER(?) ) OR ( UPPER(ticket) = UPPER(?) ) OR ( UPPER(ticket) = UPPER(?) ) ) ) OR ( UPPER(tack) BETWEEN UPPER(?) AND UPPER(?) ) OR ( ( ( UPPER(a) = UPPER(?) ) OR ( UPPER(a) = UPPER(?) ) OR ( UPPER(a) = UPPER(?) ) ) AND ( ( UPPER(e) != UPPER(?) ) OR ( UPPER(e) != UPPER(?) ) ) AND UPPER(q) NOT IN ( UPPER(?), UPPER(?), UPPER(?), UPPER(?), UPPER(?), UPPER(?), UPPER(?) ) ) )',
              bind => [ { ticket => [11, 12, 13], hostname => { in => ['ntf', 'avd', 'bvd', '123'] } },
                        { tack => { between => [qw/tick tock/] } },
                        { a => [qw/b c d/], e => { '!=', [qw(f g)] }, q => { 'not in', [14..20] } } ],
      },
);

for (@handle_tests) {
      local $" = ', ';
      #print "creating a handle with args ($_->{args}): ";
      my $sql  = SQL::Abstract->new($_->{args});
      my $bind = $_->{bind} || { a => 4, b => 0};
      my($stmt, @bind) = $sql->select('test', '*', $bind);
      ok($stmt eq $_->{stmt} && @bind) or 
              warn "got\n",
                    "[$stmt], [@bind]\n",
                    "instead of\n",
                    "[$_->{stmt}] [4, 0]\n\n";
}


