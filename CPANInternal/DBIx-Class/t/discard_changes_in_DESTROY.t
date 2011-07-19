#!/usr/bin/perl -w

use strict;
use warnings;  

use Test::More;
use lib qw(t/lib);
use DBICTest;

my $schema = DBICTest->init_schema();

plan tests => 1;

{
    my @warnings;
    local $SIG{__WARN__} = sub { push @warnings, @_; };
    {
        # Test that this doesn't cause infinite recursion.
        local *DBICTest::Artist::DESTROY;
        local *DBICTest::Artist::DESTROY = sub { $_[0]->discard_changes };
        
        my $artist = $schema->resultset("Artist")->create( { 
            artistid    => 10,
            name        => "artist number 10",
        });
        
        $artist->name("Wibble");
        
        print "# About to call DESTROY\n";
    }
    is_deeply \@warnings, [];
}