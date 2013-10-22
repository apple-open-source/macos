#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 17;
use Test::Exception;

use lib qw(t/lib);

# dynamically load SQL::Abstract::Test;
eval "use SQL::Abstract::Limit::Test; 1" or die $@;

=for notes

    use SQL::Abstract::Limit;

    my $syntax = 'LimitOffset';

    # others include: Top RowNum LimitXY Fetch RowsTo

    my $sql = SQL::Abstract::Limit->new( limit => $syntax );

    my($stmt, @bind) = $sql->select($table, \@fields, \%where, \@order, $limit, $offset);

=cut

use SQL::Abstract::Limit;

my @syntaxes = qw( LimitOffset LimitXY RowsTo Top RowNum GenericSubQ FetchFirst shgfh );

my @not_syntaxes = qw( Rank );

lives_ok { SQL::Abstract::Limit->new( limit => $_ ) for @syntaxes } 'survives constructor';

# query

my $table  = 'TheTable';
my $fields = [ qw( requestor worker colC colH ) ];
my $where  = { requestor => 'inna',
               worker    => ['nwiger', 'rcwe', 'sfz'],
               status    => { '!=', 'completed' },
               };
my $order = [ qw( pay age ) ];
my $limit = 10;     # 10 per page
my $offset = 70;    # page 7
my $last = $offset + $limit;


my $base_sql = 'requestor, worker, colC, colH FROM TheTable WHERE ( requestor = ? AND status != ? AND ( ( worker = ? ) OR ( worker = ? ) OR ( worker = ? ) ) )';

my @expected_bind = qw/inna completed nwiger rcwe sfz/; 

my $sql_ab = SQL::Abstract::Limit->new( limit_dialect => 'LimitOffset' );

my ( $stmt, @bind );

# LimitOffset
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset) } 'select LimitOffset';

is_same_sql_bind(
  $stmt, \@bind, 
  "SELECT $base_sql ORDER BY pay, age LIMIT $limit OFFSET $offset", \@expected_bind,
  'LimitOffset SQL',
);

# LimitXY
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'LimitXY' ) } 'select LimitXY';
is_same_sql_bind(
  $stmt, \@bind, 
  "SELECT $base_sql ORDER BY pay, age LIMIT $offset, $limit", \@expected_bind,
  'LimitXY SQL',
);

# RowsTo
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'RowsTo' ) } 'select RowsTo';
is_same_sql_bind(
  $stmt, \@bind, 
  "SELECT $base_sql ORDER BY pay, age ROWS $offset TO $last", \@expected_bind,
  'RowsTo SQL',
);


# Top
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'Top' ) } 'select Top';

is_same_sql_bind(
  $stmt, \@bind,
  "SELECT * FROM ("
 .  "SELECT TOP $limit * FROM ("
 .     "SELECT TOP $last $base_sql ORDER BY pay ASC, age ASC"
 .  ") AS foo ORDER BY pay DESC, age DESC"
 .") AS bar ORDER BY pay ASC, age ASC", \@expected_bind,
  'Top SQL',
);



# RowNum
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'RowNum' ) } 'select RowNum';

is_same_sql_bind(
  $stmt, \@bind,
  "SELECT * FROM ("
 .  "SELECT A.*, ROWNUM r FROM ("
 .     "SELECT $base_sql ORDER BY pay, age"
 .  ") A WHERE ROWNUM < @{[$last + 1]}"
 .") B WHERE r >= @{[$offset + 1]}", \@expected_bind,
  'RowNum SQL',
);



# GenericSubQ
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'GenericSubQ' ) } 'select GenericSubQ';
(my $gen_q_base_sql = $base_sql) =~ s/TheTable/TheTable X/;

is_same_sql_bind(
  $stmt, \@bind,
  "SELECT $gen_q_base_sql AND"
 .  "(SELECT COUNT(*) FROM TheTable WHERE requestor > X.requestor)"
 .  "  BETWEEN $offset AND $last ORDER BY requestor DESC", \@expected_bind,
  'GenericSubQ SQL',
);


# FetchFirst
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'FetchFirst' ) } 'select FetchFirst';

is_same_sql_bind(
  $stmt, \@bind,
  "SELECT * FROM ("
 .  "SELECT * FROM ("
 .    "SELECT $base_sql ORDER BY pay ASC, age ASC FETCH FIRST $last ROWS ONLY"
 .    ") foo ORDER BY pay DESC, age DESC FETCH FIRST $limit ROWS ONLY"
 .  ") bar ORDER BY pay ASC, age ASC", \@expected_bind,
  'FetchFirst SQL',
);

# Skip
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'Skip' ) } 'select Skip';

is_same_sql_bind(
  $stmt, \@bind,
  "select skip $offset limit $limit $base_sql ORDER BY pay, age", \@expected_bind,
  'Skip SQL',
);


