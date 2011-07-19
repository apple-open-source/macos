#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use SQL::Abstract::Test import => ['is_same_sql_bind'];

use SQL::Abstract;

my $sqlmaker = SQL::Abstract->new(special_ops => [

  # special op for MySql MATCH (field) AGAINST(word1, word2, ...)
  {regex => qr/^match$/i, 
   handler => sub {
     my ($self, $field, $op, $arg) = @_;
     $arg = [$arg] if not ref $arg;
     my $label         = $self->_quote($field);
     my ($placeholder) = $self->_convert('?');
     my $placeholders  = join ", ", (($placeholder) x @$arg);
     my $sql           = $self->_sqlcase('match') . " ($label) "
                       . $self->_sqlcase('against') . " ($placeholders) ";
     my @bind = $self->_bindtype($field, @$arg);
     return ($sql, @bind);
     }
   },

  # special op for Basis+ NATIVE
  {regex => qr/^native$/i, 
   handler => sub {
     my ($self, $field, $op, $arg) = @_;
     $arg =~ s/'/''/g;
     my $sql = "NATIVE (' $field $arg ')";
     return ($sql);
     }
   },

]);

my @tests = (

  #1 
  { where => {foo => {-match => 'foo'},
              bar => {-match => [qw/foo bar/]}},
    stmt  => " WHERE ( MATCH (bar) AGAINST (?, ?) AND MATCH (foo) AGAINST (?) )",
    bind  => [qw/foo bar foo/],
  },

  #2
  { where => {foo => {-native => "PH IS 'bar'"}},
    stmt  => " WHERE ( NATIVE (' foo PH IS ''bar'' ') )",
    bind  => [],
  },

);


plan tests => scalar(@tests);

for (@tests) {

  my($stmt, @bind) = $sqlmaker->where($_->{where}, $_->{order});
  is_same_sql_bind($stmt, \@bind, $_->{stmt}, $_->{bind});
}





