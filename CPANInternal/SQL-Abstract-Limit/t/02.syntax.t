#!/usr/bin/perl

use strict;
use warnings;

use Test::More tests => 7;
use Test::Exception;

use SQL::Abstract::Limit;
use Cwd;

SKIP: {
    eval { require DBD::AnyData; require Class::DBI; };

    skip( "need DBD::AnyData and Class::DBI to test syntax auto-detection", 7 ) if $@;

=for notes

    LimitOffset     PostgreSQL, MySQL (recent), SQLite
    LimitXY         MySQL (older)
    LimitYX         SQLite (optional)
    RowsTo          InterBase/FireBird

    Top             SQL/Server, MS Access
    RowNum          Oracle
    FetchFirst      DB2         # not implemented yet
    First           Informix    # not implemented yet
    GenericSubQ     Sybase, plus any databases not recognised by this module

    $dbh            a DBI database handle

    CDBI subclass
    CDBI object


    %SQL::Abstract::Limit::Syntax = ( mssql    => 'Top',
                                      access   => 'Top',
                                      sybase   => 'GenericSubQ',
                                      oracle   => 'RowNum',
                                      ...

=cut

my $cwd = getcwd;

{
    package TestApp;
    #use base 'Class::DBI'; # don't attempt to load if not installed
    our @ISA = ('Class::DBI');
    my $dsn = 'dbi:AnyData(RaiseError=>1):';

    __PACKAGE__->set_db( 'Main', $dsn, '', '' );

    __PACKAGE__->db_Main->func( 'county', 'CSV', "$cwd/t/test_data.csv", 'ad_catalog');

    __PACKAGE__->table( 'county' );

    __PACKAGE__->columns( All => qw/ code county / );
}

my $sql_ab = SQL::Abstract::Limit->new;

my $inv = TestApp->retrieve( 'INV' );

like( $inv->county, qr(^Inverness), 'retrieved record' );

my ( $syntax, $db );

lives_ok { $syntax = $sql_ab->_find_syntax( 'TestApp' ) } '_find_syntax CDBI class';
like( $syntax, qr(^LimitXY$), 'CSV syntax from CDBI class' );

lives_ok { $syntax = $sql_ab->_find_syntax( $inv ) } '_find_syntax CDBI object';
like( $syntax, qr(^LimitXY$), 'CSV syntax from CDBI object' );

my ( $dbh ) = TestApp->db_handles;
$dbh || die 'no dbh';
lives_ok { $syntax = $sql_ab->_find_syntax( $dbh ) } '_find_syntax $dbh';
like( $syntax, qr(^LimitXY$), 'CSV syntax from $dbh' );


# warn "syntax: $syntax";

}
