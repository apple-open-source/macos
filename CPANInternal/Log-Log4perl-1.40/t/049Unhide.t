###########################################
# Test Suite for ':resurrect' tag
# Mike Schilli, 2004 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use warnings;
use strict;

use Test::More;
use Log::Log4perl::Appender::TestBuffer;

BEGIN {
    eval {
        require Filter::Util::Call;
    };

    if($@) {
        plan skip_all => "Filter::Util::Call not available";
    } else {
        plan tests => 1;
    }
}

use Log::Log4perl qw(:easy :resurrect);

Log::Log4perl->easy_init($DEBUG);

Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init(\ <<EOT);
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=org.apache.log4j.PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%m %n
EOT

    # All of these should be activated
###l4p DEBUG "first";
   ###l4p DEBUG "second";
DEBUG "third";

is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    "first \nsecond \nthird \n", "Hidden statements via ###l4p"); 
