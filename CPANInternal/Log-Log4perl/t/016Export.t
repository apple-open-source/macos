###########################################
# Test Suite for Log::Log4perl
# Test all shortcuts (exported symbols)
#
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

use warnings;
use strict;
use Log::Log4perl::Appender::TestBuffer;

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test;
BEGIN { plan tests => 16 };

use Log::Log4perl qw(get_logger :levels);

ok(1);

ok(Log::Log4perl::Level::isGreaterOrEqual($DEBUG, $ERROR));
ok(Log::Log4perl::Level::isGreaterOrEqual($INFO, $WARN));
ok(Log::Log4perl::Level::isGreaterOrEqual($WARN, $ERROR));
ok(Log::Log4perl::Level::isGreaterOrEqual($ERROR, $FATAL));

##################################################
# Init logger
##################################################
my $app = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer",
    name => "A1");
my $logger = get_logger("abc.def");
$logger->add_appender($app);
$logger->level($DEBUG);

    # Let the next logger assume the default category,
    # which defaults to the current package, which
    # is 'main' in this case.
my $logger_main = get_logger();
$logger_main->add_appender($app);
$logger_main->level($DEBUG);
ok(2);

##################################################
# Use logger
##################################################
my $log2 = get_logger("abc.def");
$log2->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Use other logger
##################################################
my $log3 = get_logger("main");
$log3->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Use main logger
##################################################
my $log4 = get_logger("main");
$log4->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Use other logger
##################################################
my $log5 = get_logger("main");
$log5->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Use default-main logger
##################################################
my $log6 = get_logger();
$log6->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Use default-main logger
##################################################
my $log7 = Log::Log4perl->get_logger();
$log7->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Use default-main logger
##################################################
my $log8 = Log::Log4perl::get_logger();
$log8->debug("Is this it?");

ok($app->buffer(), "DEBUG - Is this it?\n");
$app->buffer("");

##################################################
# Remove appender
##################################################
$logger->remove_appender("A1");
$logger_main->remove_appender("A1");
$log8->debug("Is this it?");

$app = Log::Log4perl->appenders()->{"A1"};

ok($app->buffer(), "");
$app->buffer("");

##################################################
# Eradicate appender
##################################################
$Log::Log4perl::Appender::TestBuffer::DESTROY_MESSAGE = "";
Log::Log4perl->eradicate_appender("A1");
ok($Log::Log4perl::Appender::TestBuffer::DESTROY_MESSAGE, "", 
   "destroy message before");

undef $app;
   # Special for TestBuffer: remove circ ref
delete ${Log::Log4perl::Appender::TestBuffer::POPULATION}{A1};

ok($Log::Log4perl::Appender::TestBuffer::DESTROY_MESSAGES, 
   "Log::Log4perl::Appender::TestBuffer destroyed", 
   "destroy message after destruction");
