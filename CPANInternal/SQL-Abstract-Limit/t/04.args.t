#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3;
use Test::Exception;

use SQL::Abstract::Limit;
use SQL::Abstract;

# 1, 2
{
    my $sql = SQL::Abstract::Limit->new( limit_dialect => 'LimitOffset' );
    
    #                       table       fields where  order   rows   offset
    my $stmt = $sql->select("MY_TABLE", "*",   undef, ["id"], undef, undef);
    
    my $expect = 'SELECT * FROM MY_TABLE ORDER BY id';
    
    like( $stmt, qr~\Q$expect\E~, 'no-LIMIT with ORDER BY' );
    
    
    
    
    #my $sql_ab = SQL::Abstract->new;
    
    my $stmt2 = SQL::Abstract->new->select("MY_TABLE", "*",   undef, ["id"]);
    
    like( $stmt2, qr~\Q$expect\E~, 'SQL::Abstract base stmt' );
}

# 3 bug in pre-0.1: order clause missing if no limit clause specified
{
    my $sql = SQL::Abstract::Limit->new( limit_dialect => 'LimitOffset' );
    
    my $stmt = $sql->where( { fee => 'fi' }, ["id"] );
    
    like( $stmt, qr/ORDER BY id/, 'got an order_by clause' );
    
    #warn $stmt;
    
}


