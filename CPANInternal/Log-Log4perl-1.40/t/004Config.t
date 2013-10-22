###########################################
# Test Suite for Log::Log4perl
# Mike Schilli, 2002 (m@perlmeister.com)
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
use Test::More;
BEGIN { plan tests => 26 };

use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;
use File::Spec;

my $EG_DIR = "eg";
$EG_DIR = "../eg" unless -d $EG_DIR;

my $TMP_FILE = File::Spec->catfile($EG_DIR, "warnings");

ok(1, "Startup"); # If we made it this far, we are ok.

######################################################################
# Test the root logger on a configuration file defining a file appender
######################################################################
Log::Log4perl->init("$EG_DIR/log4j-manual-1.conf");

my $logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");


like(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
   qr#^\d+\s+\[N/A\] DEBUG  N/A - Gurgel$#, "Root logger"); 

######################################################################
# Test the root logger via inheritance (discovered by Kevin Goess)
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init("$EG_DIR/log4j-manual-1.conf");

$logger = Log::Log4perl->get_logger("foo");
$logger->debug("Gurgel");

like(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    qr#^\d+\s+\[N/A\] DEBUG foo N/A - Gurgel$#, "Root logger inherited"); 

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

like(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    qr#^\d+\s+\[N/A\] DEBUG foo - Gurgel$#, "Init via string"); 

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

like(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
    qr#^\d+\s+\[N/A\] DEBUG foo - Gurgel$#, "Init via hashref"); 


############################################################
# testing multiple parameters, nested hashes
############################################################

our $stub_hook;

# -----------------------------------
# here is a stub
package Log::Log4perl::AppenderTester;
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
#here is an example of using Log::Dispatch::Jabber

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


is($stub_hook->{P}{login}{hostname}, 'a.jabber.server', "Config and Jabber");
is($stub_hook->{P}{login}{password}, 'bunny', "Config and Jabber");
is($stub_hook->{P}{to}[0], 'elmer@a.jabber.server', "Config and Jabber");
is($stub_hook->{P}{to}[1], 'sam@another.jabber.server', "Config and Jabber");

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
is($@, '', 'PatternLayout without ConversionPattern'); 

######################################################################
# Test with $/ set to undef
######################################################################
$/ = undef;
Log::Log4perl->init("$EG_DIR/log4j-manual-1.conf");

$logger = Log::Log4perl->get_logger("");
$logger->debug("Gurgel");

like(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
     qr#^\d+\s+\[N/A\] DEBUG  N/A - Gurgel$#, "Config in slurp mode"); 

######################################################################
# Test init with a config parser object
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

my $parser = Log::Log4perl::Config::PropertyConfigurator->new();
my @lines = split "\n", <<EOT;
log4j.rootLogger         = DEBUG, A1
log4j.appender.A1        = Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout = org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern = object%m%n
EOT
$parser->text(\@lines);

Log::Log4perl->init($parser);

$logger = Log::Log4perl->get_logger("foo");
$logger->debug("Gurgel");

is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
   "objectGurgel\n", "Init with parser object"); 

######################################################################
# Test integrity check
######################################################################
open STDERR, ">$TMP_FILE";
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";
sub readwarn { return (scalar <IN>) || ''; }
END { close IN }

Log::Log4perl->init(\ <<EOT);
    # Just an empty configuration
EOT

like(readwarn(), qr/looks suspicious: No loggers/, 
     "Test integrity check on empty conf file");

close STDERR;
close IN;
unlink $TMP_FILE;

######################################################################
# Misspelled 'rootlogger' (needs to be rootLogger)
######################################################################
open STDERR, ">$TMP_FILE";
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";

Log::Log4perl->reset();
$Log::Log4perl::Logger::LOGGERS_BY_NAME = {};

Log::Log4perl->init(\ <<EOT);
  log4perl.rootlogger=ERROR, LOGFILE

  log4perl.appender.LOGFILE=Log::Log4perl::Appender::Screen
  log4perl.appender.LOGFILE.layout=PatternLayout
  log4perl.appender.LOGFILE.layout.ConversionPattern=[%r] %F %L %c - %m %n
EOT

is(readwarn(), "", "Autocorrecting rootLogger/rootlogger typo");

close STDERR;
close IN;
unlink $TMP_FILE;

######################################################################
# Totally misspelled rootLogger
######################################################################
open STDERR, ">$TMP_FILE";
open IN, "<$TMP_FILE" or die "Cannot open $TMP_FILE";

Log::Log4perl->reset();
$Log::Log4perl::Logger::LOGGERS_BY_NAME = {};

Log::Log4perl->init(\ <<EOT);
  log4perl.schtonk=ERROR, LOGFILE

  log4perl.appender.LOGFILE=Log::Log4perl::Appender::Screen
  log4perl.appender.LOGFILE.layout=PatternLayout
  log4perl.appender.LOGFILE.layout.ConversionPattern=[%r] %F %L %c - %m %n
EOT

like(readwarn(), qr/looks suspicious: No loggers/, 
     "Test integrity check on totally misspelled rootLogger typo");

close STDERR;
close IN;
unlink $TMP_FILE;

######################################################################
# PatternLayout %m{}
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init(\ <<EOT);
log4j.logger.foo=DEBUG, A1
log4j.appender.A1=Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout=org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern=%M%m
EOT

###########################################
sub somefunc {
###########################################
    $logger = Log::Log4perl->get_logger("foo");
    $logger->debug("Gurgel");
}

package SomePackage;
###########################################
sub somepackagefunc {
###########################################
    $logger = Log::Log4perl->get_logger("foo");
    $logger->debug("Gurgel");
}
package main;

somefunc();
is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
        "main::somefuncGurgel", "%M main");

Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer("");
SomePackage::somepackagefunc();
is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
        "SomePackage::somepackagefuncGurgel", "%M in package");

######################################################################
# PatternLayout %m{1}
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init(\ <<EOT);
log4j.logger.foo=DEBUG, A1
log4j.appender.A1=Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout=org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern=%M{1}%m
EOT

somefunc();
is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
        "somefuncGurgel", "%M{1} main");

Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer("");
SomePackage::somepackagefunc();
is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
        "somepackagefuncGurgel", "%M{1} package");

######################################################################
# PatternLayout %p{1}
######################################################################
Log::Log4perl::Appender::TestBuffer->reset();

Log::Log4perl->init(\ <<EOT);
log4j.logger.foo=DEBUG, A1
log4j.appender.A1=Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout=org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern=-%p{1}- %m
EOT

somefunc();
is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(),
        "-D- Gurgel", "%p{1} main");

Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer("");
SomePackage::somepackagefunc();
is(Log::Log4perl::Appender::TestBuffer->by_name("A1")->buffer(), 
        "-D- Gurgel", "%p{1} package");

######################################################################
# Test accessors
######################################################################
$parser = Log::Log4perl::Config::PropertyConfigurator->new();
@lines = split "\n", <<EOT;
log4j.rootLogger         = DEBUG, A1
log4j.appender.A1        = Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout = org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern = object%m%n
EOT
$parser->text(\@lines);
$parser->parse();
is($parser->value("log4j.rootLogger"), "DEBUG, A1", "value() accessor");
is($parser->value("log4j.foobar"), undef, "value() accessor undef");

is($parser->value("log4j.appender.A1"), 
   "Log::Log4perl::Appender::TestBuffer", "value() accessor");

is($parser->value("log4perl.appender.A1.layout.ConversionPattern"), 
   "object%m%n", "value() accessor log4perl");

######################################################################
# Test accessors
######################################################################
my $conf = q{
log4perl.category.pf.trigger = DEBUG
log4j.appender.A1        = Log::Log4perl::Appender::TestBuffer
log4j.appender.A1.layout = org.apache.log4j.PatternLayout
log4j.appender.A1.layout.ConversionPattern = object%m%n
};

eval { Log::Log4perl->init( \$conf ); };

is $@, "", "'trigger' category [rt.cpan.org #50495]";
