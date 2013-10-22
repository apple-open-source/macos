#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 3;
use Test::Exception;


{
    package MyLimit;
    use base 'SQL::Abstract::Limit';
    sub emulate_limit { 'yah' }
}

my ( $sql, $stmt );

lives_ok { $sql = MyLimit->new; $stmt = $sql->select( 'table',
                                                      [ 'col1', 'col2' ],
                                                      { this => 'that' },
                                                      [ 'col3 ASC' ],
                                                      10,
                                                      100,
                                                      ) } 'my own limit';

like( $stmt, qr(^yah$), 'custom LIMIT' );

$stmt = $sql->select( 'table',
                      [ 'col1', 'col2' ],
                      { this => 'that' },
                      );

like( $stmt, qr(^\QSELECT col1, col2 FROM table WHERE ( this = ? )\E$), 'SQL::Abstract - no limit' );
