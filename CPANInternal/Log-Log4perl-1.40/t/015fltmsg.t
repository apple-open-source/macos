###########################################
# Test Suite for Log::Log4perl
# warp_message cases
# Mike Schilli, 2003 (m@perlmeister.com)
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test::More tests => 5;

use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

######################################################################
# warp_message undef: Concatenation
######################################################################
Log::Log4perl->init( \ <<EOT );
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%m%n
EOT

my $app = Log::Log4perl::Appender::TestBuffer->by_name("A1");
my $logger = Log::Log4perl->get_logger("");
$logger->debug("Chunk1", "Chunk2", "Chunk3");

is($app->buffer(), "Chunk1Chunk2Chunk3\n", "warp_message undef"); 

######################################################################
# warp_message undef: Concatenation plus JOIN_MSG_ARRAY_CHAR
######################################################################
Log::Log4perl->init( \ <<EOT );
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=PatternLayout
    log4perl.appender.A1.layout.ConversionPattern=%m%n
EOT

$Log::Log4perl::JOIN_MSG_ARRAY_CHAR = "bang!";

$app = Log::Log4perl::Appender::TestBuffer->by_name("A1");
$logger = Log::Log4perl->get_logger("");
$logger->debug("Chunk1", "Chunk2", "Chunk3");

is($app->buffer(), "Chunk1bang!Chunk2bang!Chunk3\n", 
   "warp_message undef (JOIN_MSG_ARRAY_CHAR)"); 

$Log::Log4perl::JOIN_MSG_ARRAY_CHAR = ""; # back to default

######################################################################
# warp_message 0
######################################################################
Log::Log4perl->init( \ <<EOT );
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestArrayBuffer
    log4perl.appender.A1.layout=NoopLayout
    log4perl.appender.A1.warp_message=0
EOT

$app = Log::Log4perl::Appender::TestArrayBuffer->by_name("A1");
$logger = Log::Log4perl->get_logger("");
$logger->debug("Chunk1", "Chunk2", "Chunk3");

is($app->buffer(), "[Chunk1,Chunk2,Chunk3]", 
   "warp_message 0 (NoopLayout)"); 

######################################################################
# warp_message = code ref
######################################################################
Log::Log4perl->init( \ <<'EOT' );
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestArrayBuffer
    log4perl.appender.A1.layout=NoopLayout
    log4perl.appender.A1.warp_message = sub { $#_ = 2 if @_ > 3; \
                                           return @_; }
EOT

$app = Log::Log4perl::Appender::TestArrayBuffer->by_name("A1");
$logger = Log::Log4perl->get_logger("");
$logger->debug("Chunk1", "Chunk2", "Chunk3", "Chunk4");

is($app->buffer(), "[Chunk1,Chunk2,Chunk3]", 
   "warp_message = function (by cref)");


######################################################################
# warp_message = function
######################################################################
my $COUNTER = 0;
sub warp_my_message {
    my @chunks = @_;
    unshift @chunks, ++$COUNTER;
    return @chunks;
}

Log::Log4perl->init( \ <<'EOT' );
    log4perl.rootLogger=DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestArrayBuffer
    log4perl.appender.A1.layout=NoopLayout
    log4perl.appender.A1.warp_message = main::warp_my_message
EOT

$app = Log::Log4perl::Appender::TestArrayBuffer->by_name("A1");
$logger = Log::Log4perl->get_logger("");
$logger->debug("Chunk1", "Chunk2", "Chunk3");

is($app->buffer(), "[1,Chunk1,Chunk2,Chunk3]", 
   "warp_message = function (by name)");
