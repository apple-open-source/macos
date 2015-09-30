use strict;

use File::Temp qw( tempfile );

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

my($tmpfh, $tempfile) = tempfile( UNLINK => 1 );

use Test::More;
BEGIN { plan tests => 1 };
use Log::Log4perl qw( :easy );

Log::Log4perl->easy_init( { level => $DEBUG, file => $tempfile } );

my $warnings = "";

$SIG{__WARN__} = sub {
   $warnings .= $_[0];
};

DEBUG "foo", undef, "bar";

like $warnings, qr/Log message argument #2/, "warning for undef element issued";
