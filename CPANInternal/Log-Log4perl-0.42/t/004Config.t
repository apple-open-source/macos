###########################################
# Test Suite for Log::Log4perl
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test;
BEGIN { plan tests => 10 };

use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

ok(1); # If we made it this far, we're ok.

######################################################################
# Test the root logger on a configuration file defining a file appender
######################################################################
Log::Log4perl->init("$EG_DIR/log4j-manual-1.conf");

my $logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");


ok(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
   'm#^\d+\s+\[N/A\] DEBUG  N/A - Gurgel$#'); 

######################################################################
# Test the root logger via inheritance (discovered by Kevin Goess)
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init("$EG_DIR/log4j-manual-1.conf");

$logger = Log::Log4perl->get_logger("foo");
$logger->debug("Gurgel");

ok(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    'm#^\d+\s+\[N/A\] DEBUG foo N/A - Gurgel$#'); 

######################################################################
# Test init with a string
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init(\ <<EOT);
log4j.rootLogger=DEBUG, A1
log4j.appender.A1=Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout=org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern=%-4r [%t] %-5p %c - %m%n
EOT

$logger = Log::Log4perl->get_logger("foo");
$logger->debug("Gurgel");

ok(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    'm#^\d+\s+\[N/A\] DEBUG foo - Gurgel$#'); 

######################################################################
# Test init with a hashref
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

my %hash = (
    "log4j.rootLogger"         => "DEBUG, A1",
    "log4j.appender.A1"        => "Log::Log4perl::Appender::TestBuffer",
    "log4j.appender.A1.layout" => "org.apache.log4j.PatternLayout",
    "log4j.appender.A1.layout.ConversionPattern" => 
                                  "%-4r [%t] %-5p %c - %m%n"
    );

Log::Log4perl->init(\%hash);

$logger = Log::Log4perl->get_logger("foo");
$logger->debug("Gurgel");

ok(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    'm#^\d+\s+\[N/A\] DEBUG foo - Gurgel$#'); 


############################################################
# testing multiple parameters, nested hashes
############################################################

our $stub_hook;

# -----------------------------------
# here's a stub
package Log::Log4perl::AppenderTester;
use vars qw($IS_LOADED);
$IS_LOADED = 1; 
sub new {
    my($class, %params) = @_;
    my $self = {};
    bless $self, $class;

    $self->{P} = \%params;

    $main::stub_hook = $self;
    
    return $self;
}
package main;
# -----------------------------------

Log::Log4perl->init(\ <<'EOT');
#here's an example of using Log::Dispatch::Jabber

log4j.category.animal.dog   = INFO, jabbender

log4j.appender.jabbender          = Log::Log4perl::AppenderTester
log4j.appender.jabbender.layout   = Log::Log4perl::Layout::SimpleLayout
log4j.appender.jabbender.login.hostname = a.jabber.server
log4j.appender.jabbender.login.port = 5222
log4j.appender.jabbender.login.username =  bugs
log4j.appender.jabbender.login.password = bunny
log4j.appender.jabbender.login.resource = logger
log4j.appender.jabbender.to = elmer@a.jabber.server
log4j.appender.jabbender.to = sam@another.jabber.server

EOT

#should produce this:
#{
#    login => {
#          hostname => "a.jabber.server",
#          password => "bunny",
#          port     => 5222,
#          resource => "logger",
#          username => "bugs",
#        },
#    to => ["elmer\@a.jabber.server", "sam\@another.jabber.server"],
#  },


ok($stub_hook->{P}{login}{hostname}, 'a.jabber.server');
ok($stub_hook->{P}{login}{password}, 'bunny');
ok($stub_hook->{P}{to}[0], 'elmer@a.jabber.server');
ok($stub_hook->{P}{to}[1], 'sam@another.jabber.server');

##########################################################################
# Test what happens if we define a PatternLayout without ConversionPattern
##########################################################################
Log::Log4perl::Appender::TestBuffer->reset();

$conf = <<EOT;
    log4perl.logger.Twix.Bar = DEBUG, A1
    log4perl.appender.A1=Log::Log4perl::Appender::TestBuffer
    log4perl.appender.A1.layout=PatternLayout
    #log4perl.appender.A1.layout.ConversionPattern=%d-%c %m%n
EOT

eval { Log::Log4perl->init(\$conf); };


#actually, it turns out that log4j handles this, if no ConversionPattern
#specified is uses DEFAULT_LAYOUT_PATTERN, %m%n
#ok($@, '/No ConversionPattern given for PatternLayout/'); 
ok($@, ''); 
