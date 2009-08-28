###########################################
# Test Suite for :no_extra_logdie_message
# Mike Schilli, 2005 (m@perlmeister.com)
###########################################

use warnings;
use strict;

use Log::Log4perl qw(:easy :no_extra_logdie_message);
use Test::More;

BEGIN {
    if ($] < 5.006) {
        plan skip_all => "Only with perl >= 5.006";
    } else {
        plan tests => 9;
    }
}

use Log::Log4perl::Appender::TestBuffer;

is($Log::Log4perl::LOGDIE_MESSAGE_ON_STDERR, 0, "internal variable set");

my $conf = qq(
log4perl.category                  = DEBUG, Screen

    # Regular Screen Appender
log4perl.appender.Screen           = Log::Log4perl::Appender::TestBuffer
log4perl.appender.Screen.layout    = PatternLayout
log4perl.appender.Screen.layout.ConversionPattern = %d %p %c %m %n
);

Log::Log4perl->init(\$conf);

#########################################################################
# Capture STDERR to a temporary file and a filehandle to read from it

my $TMP_FILE = File::Spec->catfile(qw(t tmp easy));
$TMP_FILE = "tmp/easy" if ! -d "t";

open STDERR, ">$TMP_FILE";
select STDERR; $| = 1; #needed on win32
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";
sub readstderr { return join("", <IN>); }

END   { unlink $TMP_FILE;
        close IN;
      }
#########################################################################

my $buf = Log::Log4perl::Appender::TestBuffer->by_name("Screen");

$buf->buffer("");
LOGCARP("logcarp");

is(readstderr(), "", "No output to stderr");
SKIP: { use Carp;
    skip "Detected buggy Carp.pm (upgrade to perl-5.8.*)", 3 unless 
        defined $Carp::VERSION;
    like($buf->buffer(), qr/logcarp.*54/, "Appender output intact");
    $buf->buffer("");
    LOGCARP("logcarp");
    is(readstderr(), "", "No output to stderr");
    like($buf->buffer(), qr/logcarp.*62/, "Appender output intact");
}
#########################################################################
# Turn default behaviour back on
#########################################################################
$Log::Log4perl::LOGDIE_MESSAGE_ON_STDERR ^= 1;
$buf->buffer("");

package Foo;
use Log::Log4perl qw(:easy);
sub foo {
    LOGCARP("logcarp");
}
package main;

Foo::foo();

like(readstderr(), qr/logcarp.*79/, "Output to stderr");
like($buf->buffer(), qr/logcarp.*79/, "Appender output intact");

$buf->buffer("");
eval {
    LOGDIE("logdie");
};
like($@, qr/logdie.*86/, "Output to stderr");
like($buf->buffer(), qr/logdie/, "Appender output intact");

