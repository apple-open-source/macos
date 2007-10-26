# $Id: mergecb.t,v 1.4 2003/06/24 07:16:28 koschei Exp $
use Test::More tests => 8;
use lib 'inc';
use blib;
use strict;
use vars qw( $class );

BEGIN {
    $class = 'DateTime::Format::Builder::Parser';
    use_ok $class;
}

{
    my $new_sub = sub { my $x = shift; sub { $_[1].$x } };
    my @cbs = (
	map { $new_sub->( $_ ) } qw( a b c d e f g )
    );
    my $cb = $class->merge_callbacks( @cbs );
    is( $cb->( input => "x" ) => "xabcdefg", "Callback chaining works." );

    my $cbr = $class->merge_callbacks( \@cbs );
    is( $cbr->( input => "x" ) => "xabcdefg", "Callback chaining works on ref." );
}

{
    my $inout = sub { $_[0] . "foo" };
    my $cb = $class->merge_callbacks( $inout );
    is( $cb->("foo") => "foofoo", "Single callback works." );
}

{
    my $empty = $class->merge_callbacks( undef );
    ok( !defined $empty, "Given undef, do bugger all." );

    $empty = $class->merge_callbacks( );
    ok( !defined $empty, "Given nothing, do bugger all." );

    $empty = $class->merge_callbacks( [] );
    ok( !defined $empty, "Given empty arrayref, do bugger all." );
}

{
    my $error = eval { $class->merge_callbacks( { foo => 4 } ) };
    ok( $@, "Correctly faulted on bad arguments." );
}
