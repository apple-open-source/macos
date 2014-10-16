use strict;
use warnings;

use Test::More;

use DateTime::Format::Builder::Parser;

{
    my $new_sub = sub {
        my $x = shift;
        sub { $_[1] . $x }
    };
    my @cbs = ( map { $new_sub->($_) } qw( a b c d e f g ) );
    my $cb = DateTime::Format::Builder::Parser->merge_callbacks(@cbs);
    is( $cb->( input => "x" ) => "xabcdefg", "Callback chaining works." );

    my $cbr = DateTime::Format::Builder::Parser->merge_callbacks( \@cbs );
    is(
        $cbr->( input => "x" ) => "xabcdefg",
        "Callback chaining works on ref."
    );
}

{
    my $inout = sub { $_[0] . "foo" };
    my $cb = DateTime::Format::Builder::Parser->merge_callbacks($inout);
    is( $cb->("foo") => "foofoo", "Single callback works." );
}

{
    my $empty = DateTime::Format::Builder::Parser->merge_callbacks(undef);
    ok( !defined $empty, "Given undef, do bugger all." );

    $empty = DateTime::Format::Builder::Parser->merge_callbacks();
    ok( !defined $empty, "Given nothing, do bugger all." );

    $empty = DateTime::Format::Builder::Parser->merge_callbacks( [] );
    ok( !defined $empty, "Given empty arrayref, do bugger all." );
}

{
    my $error = eval {
        DateTime::Format::Builder::Parser->merge_callbacks( { foo => 4 } );
    };
    ok( $@, "Correctly faulted on bad arguments." );
}

done_testing();
