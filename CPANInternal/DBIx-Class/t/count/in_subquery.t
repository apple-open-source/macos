#!/usr/bin/perl

use strict;
use warnings;

use Test::More;

plan ( tests => 1 );

use lib qw(t/lib);
use DBICTest;
use DBIC::SqlMakerTest;

my $schema = DBICTest->init_schema();

{
    my $rs = $schema->resultset("CD")->search(
        { 'artist.name' => 'Caterwauler McCrae' },
        { join => [qw/artist/]}
    );
    my $squery = $rs->get_column('cdid')->as_query;
    my $subsel_rs = $schema->resultset("CD")->search( { cdid => { IN => $squery } } );
    is($subsel_rs->count, $rs->count, 'Subselect on PK got the same row count');
}
