#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 22;
use Test::Exception;

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

my $sql_ab = SQL::Abstract::Limit->new( limit_dialect => 'LimitOffset' );

my ( $stmt, @bind );

# LimitOffset
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset) } 'select LimitOffset';
like( $stmt, qr~\Q$base_sql\E~, 'base SQL' );
like( $stmt, qr~^\QSELECT $base_sql ORDER BY pay, age LIMIT $limit OFFSET $offset\E$~, 'complete SQL' );

# LimitXY
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'LimitXY' ) } 'select LimitXY';
like( $stmt, qr~\Q$base_sql\E~, 'base SQL' );
like( $stmt, qr~^\QSELECT $base_sql ORDER BY pay, age LIMIT $offset, $limit\E$~, 'complete SQL' );

# RowsTo
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'RowsTo' ) } 'select LimitXY';
like( $stmt, qr~\Q$base_sql\E~, 'base SQL' );
like( $stmt, qr~^\QSELECT $base_sql ORDER BY pay, age ROWS $offset TO $last\E$~, 'complete SQL' );


### TODO - regexes to match full query ###

# Top
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'Top' ) } 'select Top';
like( $stmt, qr~\Q$base_sql\E~, 'base SQL' );

TODO: {
    local $TODO = 'need regex for complex query';
    like( $stmt, qr~^\Qcomplete SQL\E$~, 'complete SQL' );
}

# RowNum
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'RowNum' ) } 'select RowNum';
like( $stmt, qr~\Q$base_sql\E~, 'base SQL' );

TODO: {
    local $TODO = 'need regex for complex query';
    like( $stmt, qr~^\Qcomplete SQL\E$~, 'complete SQL' );
}

# GenericSubQ
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'GenericSubQ' ) } 'select GenericSubQ';
my $gen_q_base_sql = $base_sql;
$gen_q_base_sql =~ s/TheTable/TheTable X/;
like( $stmt, qr~\Q$gen_q_base_sql\E~, 'GenericSubQ SQL' );

TODO: {
    local $TODO = 'need regex for complex query';
    like( $stmt, qr~^\Qcomplete SQL\E$~, 'complete SQL' );
}

# FetchFirst
lives_ok { ( $stmt, @bind ) = $sql_ab->select( $table, $fields, $where, $order, $limit, $offset, 'FetchFirst' ) } 'select GenericSubQ';
like( $stmt, qr~\Q$base_sql\E~, 'base SQL' );

TODO: {
    local $TODO = 'need regex for complex query';
    like( $stmt, qr~^\Qcomplete SQL\E$~, 'complete SQL' );
}




#warn "\n\n" . $stmt;
#warn join( ', ', @bind ) . "\n\n";
#
#
warn " *** not yet testing subquery LIMIT emulations\n";
