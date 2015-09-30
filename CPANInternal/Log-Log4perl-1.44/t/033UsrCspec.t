#testing user-defined conversion specifiers (cspec)

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Test::More;
use Log::Log4perl;
use Log::Log4perl::Appender::TestBuffer;
use File::Spec;

Log::Log4perl::Appender::TestBuffer->reset();


my $config = <<'EOL';
log4j.category.plant    = DEBUG, appndr1
log4j.category.animal   = DEBUG, appndr2

#'U' a global user-defined cspec
log4j.PatternLayout.cspec.U =       \
        sub {                       \
            return "UID $< GID $("; \
        }                           \
    
  
# ********************
# first appender
log4j.appender.appndr1        = Log::Log4perl::Appender::TestBuffer
#log4j.appender.appndr1        = Log::Log4perl::Appender::Screen
log4j.appender.appndr1.layout = org.apache.log4j.PatternLayout
log4j.appender.appndr1.layout.ConversionPattern = %K xx %G %U

#'K' cspec local to appndr1                 (pid in hex)
log4j.appender.appndr1.layout.cspec.K = sub { return sprintf "%1x", $$}

#'G' cspec unique to appdnr1
log4j.appender.appndr1.layout.cspec.G = sub {return 'thisistheGcspec'}

    

# ********************
# second appender
log4j.appender.appndr2        = Log::Log4perl::Appender::TestBuffer
#log4j.appender.appndr2        = Log::Log4perl::Appender::Screen
log4j.appender.appndr2.layout = org.apache.log4j.PatternLayout
log4j.appender.appndr2.layout.ConversionPattern = %K %U

#'K' cspec local to appndr2
log4j.appender.appndr2.layout.cspec.K =                              \
    sub {                                                            \
        my ($self, $message, $category, $priority, $caller_level) = @_; \
        $message =~ /--- (.+) ---/;                                  \
        my $snippet = $1;                                            \
        return ucfirst(lc($priority)).'-'.$snippet.'-'.ucfirst(lc($priority));                 \
      }
      
#override global 'U' cspec
log4j.appender.appndr2.layout.cspec.U = sub {return 'foobar'}
      
EOL


Log::Log4perl::init(\$config);

my $plant = Log::Log4perl::get_logger('plant');
my $animal = Log::Log4perl::get_logger('animal');


my $hexpid = sprintf "%1x", $$;
my $uid = $<;
my $gid = $(;


my $plantbuffer = Log::Log4perl::Appender::TestBuffer->by_name("appndr1");
my $animalbuffer = Log::Log4perl::Appender::TestBuffer->by_name("appndr2");

$plant->fatal('blah blah blah --- plant --- yadda yadda');
is($plantbuffer->buffer(), "$hexpid xx thisistheGcspec UID $uid GID $gid");
$plantbuffer->reset;

$animal->fatal('blah blah blah --- animal --- yadda yadda');
is($animalbuffer->buffer(), "Fatal-animal-Fatal foobar");
$animalbuffer->reset;

$plant->error('blah blah blah --- plant --- yadda yadda');
is($plantbuffer->buffer(), "$hexpid xx thisistheGcspec UID $uid GID $gid");
$plantbuffer->reset;

$animal->error('blah blah blah --- animal --- yadda yadda');
is($animalbuffer->buffer(), "Error-animal-Error foobar");
$animalbuffer->reset;

$plant->warn('blah blah blah --- plant --- yadda yadda');
is($plantbuffer->buffer(), "$hexpid xx thisistheGcspec UID $uid GID $gid");
$plantbuffer->reset;

$animal->warn('blah blah blah --- animal --- yadda yadda');
is($animalbuffer->buffer(), "Warn-animal-Warn foobar");
$animalbuffer->reset;

$plant->info('blah blah blah --- plant --- yadda yadda');
is($plantbuffer->buffer(), "$hexpid xx thisistheGcspec UID $uid GID $gid");
$plantbuffer->reset;

$animal->info('blah blah blah --- animal --- yadda yadda');
is($animalbuffer->buffer(), "Info-animal-Info foobar");
$animalbuffer->reset;

$plant->debug('blah blah blah --- plant --- yadda yadda'); 
is($plantbuffer->buffer(), "$hexpid xx thisistheGcspec UID $uid GID $gid");
$plantbuffer->reset;

$animal->debug('blah blah blah --- animal --- yadda yadda'); 
is($animalbuffer->buffer(), "Debug-animal-Debug foobar");
$animalbuffer->reset;


#now test the api call we're adding

Log::Log4perl::Layout::PatternLayout::add_global_cspec('Z', sub {'zzzzzzzz'}); #snooze?


my $app = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");

my $logger = Log::Log4perl->get_logger("plant");
$logger->add_appender($app);
my $layout = Log::Log4perl::Layout::PatternLayout->new(
    "%m %Z");
$app->layout($layout);
$logger->debug("That's the message");

is($app->buffer(), "That's the message zzzzzzzz");

###########################################################
#testing perl code snippets in Log4perl configuration files
###########################################################

Log::Log4perl::Appender::TestBuffer->reset();

$config = <<'EOL';
log4perl.category.some = DEBUG, appndr

    # This should be evaluated at config parse time
log4perl.appender.appndr = sub { \
    return "Log::Log4perl::Appender::TestBuffer" }
log4perl.appender.appndr.layout = Log::Log4perl::Layout::PatternLayout
    # This should be evaluated at config parse time ("%m %K%n")
log4perl.appender.appndr.layout.ConversionPattern = sub{ "%" . \
    chr(109) . " %K%n"; }

    # This should be evaluated at run time ('K' cspec)
log4perl.appender.appndr.layout.cspec.K = sub { $ENV{TEST_VALUE} }
EOL

Log::Log4perl::init(\$config);

$ENV{TEST_VALUE} = "env_value";

$logger = Log::Log4perl::get_logger('some');
$logger->debug("log_message");

$ENV{TEST_VALUE} = "env_value2";
$logger->info("log_message2");

my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("appndr");

#print "Testbuffer: ", $buffer->buffer(), "\n";

is($buffer->buffer(), "log_message env_value\nlog_message2 env_value2\n");

###########################################################
#testing perl code snippets with ALLOW_CODE_IN_CONFIG_FILE 
#disabled
###########################################################

Log::Log4perl::Appender::TestBuffer->reset();

$config = <<'EOL';
log4perl.category.some = DEBUG, appndr

    # This should be evaluated at config parse time
log4perl.appender.appndr = Log::Log4perl::Appender::TestBuffer
log4perl.appender.appndr.layout = Log::Log4perl::Layout::PatternLayout
    # This should be evaluated at config parse time ("%m %K%n")
log4perl.appender.appndr.layout.ConversionPattern = sub{ "%m" . \
    chr(109) . " %n"; }
EOL

Log::Log4perl::Config::allow_code(0);

eval {
    Log::Log4perl::init(\$config);
};

print "ERR is $@\n";

if($@ and $@ =~ /prohibits/) {
    ok(1);
} else {
    ok(0);
}

# Test if cspecs are denied
Log::Log4perl::Appender::TestBuffer->reset();

$config = <<'EOL';
log4perl.category.some = DEBUG, appndr

    # This should be evaluated at config parse time
log4perl.appender.appndr = Log::Log4perl::Appender::TestBuffer
log4perl.appender.appndr.layout = Log::Log4perl::Layout::PatternLayout
log4perl.appender.appndr.layout.ConversionPattern = %m %n
log4perl.appender.appndr.layout.cspec.K = sub { $ENV{TEST_VALUE} }
EOL

Log::Log4perl::Config->allow_code(0);

eval {
    Log::Log4perl::init(\$config);
};

print "ERR is $@\n";

if($@ and $@ =~ /prohibits/) {
    ok(1);
} else {
    ok(0);
}

################################################################
# Test if cspecs are passing the correct caller level
################################################################
Log::Log4perl::Config::allow_code(1);
Log::Log4perl::Appender::TestBuffer->reset();

$config = <<'EOL';
log4perl.category.some = DEBUG, appndr

    # This should be evaluated at config parse time
log4perl.appender.appndr = Log::Log4perl::Appender::TestBuffer
log4perl.appender.appndr.layout = Log::Log4perl::Layout::PatternLayout
log4perl.appender.appndr.layout.ConversionPattern = %K %m %n
log4perl.appender.appndr.layout.cspec.K = sub { return (caller($_[4]))[1] }
EOL

Log::Log4perl::init(\$config);

my $some = Log::Log4perl::get_logger('some');
$some->debug("blah");

my $somebuffer = Log::Log4perl::Appender::TestBuffer->by_name("appndr");

like($somebuffer->buffer(), qr/033UsrCspec.t blah/);

################################################################
# cspecs with parameters in curlies
################################################################
Log::Log4perl::Config::allow_code(1);
Log::Log4perl::Appender::TestBuffer->reset();

our %hash = (foo => "bar", quack => "schmack");
$hash{hollerin} = "hootin"; # shut up perl warnings

use Data::Dumper;
$config = <<'EOL';
log4perl.category.some = DEBUG, appndr

    # This should be evaluated at config parse time
log4perl.appender.appndr = Log::Log4perl::Appender::TestBuffer
log4perl.appender.appndr.layout = Log::Log4perl::Layout::PatternLayout
log4perl.appender.appndr.layout.ConversionPattern = %K{foo} %m %K{quack}%n
log4perl.appender.appndr.layout.cspec.K = sub { $main::hash{$_[0]->{curlies} } }
EOL

Log::Log4perl::init(\$config);

$some = Log::Log4perl::get_logger('some');
$some->debug("blah");

$somebuffer = Log::Log4perl::Appender::TestBuffer->by_name("appndr");

is($somebuffer->buffer(), "bar blah schmack\n");

################################################################
# Get the calling package from a cspec
################################################################
Log::Log4perl::Config::allow_code(1);
Log::Log4perl::Appender::TestBuffer->reset();

$config = <<'EOL';
log4perl.category.some = DEBUG, appndr

    # This should be evaluated at config parse time
log4perl.appender.appndr = Log::Log4perl::Appender::TestBuffer
log4perl.appender.appndr.layout = Log::Log4perl::Layout::PatternLayout
log4perl.appender.appndr.layout.ConversionPattern = %K %m%n
log4perl.appender.appndr.layout.cspec.K = \
    sub { scalar caller( $_[4] )}
EOL

Log::Log4perl::init(\$config);

$some = Log::Log4perl::get_logger('some');
$some->debug("blah");

$somebuffer = Log::Log4perl::Appender::TestBuffer->by_name("appndr");

is($somebuffer->buffer(), "main blah\n");

BEGIN { plan tests => 17, }
