#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

use SQL::Abstract::Test import => [qw/is_same_sql_bind is_same_bind/];

use SQL::Abstract;

my @data = (
    {
        user => 'nwiger',
        name => 'Nathan Wiger',
        phone => '123-456-7890',
        addr => 'Yeah, right',
        city => 'Milwalkee',
        state => 'Minnesota',
    },

    {
        user => 'jimbo',
        name => 'Jimbo Bobson',
        phone => '321-456-0987',
        addr => 'Yo Momma',
        city => 'Yo City',
        state => 'Minnesota',
    },

    {
        user => 'mr.hat',
        name => 'Mr. Garrison',
        phone => '123-456-7890',
        addr => undef,
        city => 'South Park',
        state => 'CO',
    },

    {
        user => 'kennyg',
        name => undef,
        phone => '1-800-Sucky-Sucky',
        addr => 'Mr. Garrison',
        city => undef,
        state => 'CO',
    },

    {
        user => 'barbara_streisand',
        name => 'MechaStreisand!',
        phone => 0,
        addr => -9230992340,
        city => 42,
        state => 'CO',
    },
);


plan tests => (@data * 5  +  2);

# test insert() and values() for reentrancy
my($insert_hash, $insert_array, $numfields);
my $a_sql = SQL::Abstract->new;
my $h_sql = SQL::Abstract->new;

for my $record (@data) {

  my $values = [ map { $record->{$_} } sort keys %$record ];

  my ($h_stmt, @h_bind) = $h_sql->insert('h_table', $record);
  my ($a_stmt, @a_bind) = $a_sql->insert('a_table', $values );

  # init from first run, should not change afterwards
  $insert_hash ||= $h_stmt;
  $insert_array ||= $a_stmt;
  $numfields ||= @$values;

  is ( $a_stmt, $insert_array, 'Array-based insert statement unchanged' );
  is ( $h_stmt, $insert_hash, 'Hash-based insert statement unchanged' );

  is_deeply ( \@a_bind, \@h_bind, 'Bind values match after both insert() calls' );
  is_deeply ( [$h_sql->values ($record)] , \@h_bind, 'values() output matches bind values after insert()' );

  is ( scalar @h_bind, $numfields, 'Number of fields unchanged' );
}

# test values() with literal sql
#
# NOTE:
# The example is deliberately complicated by the addition of a literal ? in xfunc
# This is an intentional test making sure literal ? remains untouched.
# It is rather impractical in the field, as the user will have to insert
# a bindvalue for the literal position(s) in the correct offset of \@bind
{
  my $sql = SQL::Abstract->new;

  my $data = { 
    event => 'rapture',
    stuff => 'fluff',
    time => \ 'now()',
    xfunc => \ 'xfunc(?)',
    yfunc => ['yfunc(?)', 'ystuff' ],
    zfunc => \['zfunc(?)', 'zstuff' ],
    zzlast => 'zzstuff',
  };

  my ($stmt, @bind) = $sql->insert ('table', $data);

  is_same_sql_bind (
    $stmt,
    \@bind,
    'INSERT INTO table ( event, stuff, time, xfunc, yfunc, zfunc, zzlast) VALUES ( ?, ?, now(), xfunc (?), yfunc(?), zfunc(?), ? )',
    [qw/rapture fluff ystuff zstuff zzstuff/],  # event < stuff
  );

  is_same_bind (
    [$sql->values ($data)],
    [@bind],
    'values() output matches that of initial bind'
  ) || diag "Corresponding SQL statement: $stmt";
}
