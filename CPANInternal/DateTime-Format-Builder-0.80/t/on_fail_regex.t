use strict;

use Test::More tests => 2;

use DateTime::Format::Builder;


{
    eval q!package DTFB::FailRegexTest;
        use DateTime::Format::Builder (
            parsers => {
                parse_datetime => [
                    {
                        regex => qr|\d{4}-\d{2}-\d{2}|,
                        params => [ qw(year month day) ]
                    }
                ]
            }
        );
    !;
    ok( !$@, "Made class" );
    diag $@ if $@;

    my $o = DTFB::FailRegexTest->new();
    my $str = eval { $o->parse_datetime("FooBlah") };
    my $e = $@;
    my $file = __FILE__;
    like($e, qr(at \Q$file\E), "Should croak from this file");
}
