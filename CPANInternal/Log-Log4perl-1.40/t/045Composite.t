###########################################
# Test Suite for Composite Appenders
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

BEGIN {
    eval {
        require Storable;
    };
    if ($@) {
        plan skip_all => "only with Storable"; # Limit.pm needs it and
                                               # early Perl versions dont
                                               # have it.
    }else{
        plan tests => 20;
    }
}

use Log::Log4perl qw(get_logger :levels);
use Log::Log4perl::Level;
use Log::Log4perl::Appender::TestBuffer;

ok(1); # If we made it this far, we/re ok.

##################################################
# Limit Appender
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

my $conf = qq(
  log4perl.category = WARN, Limiter

    # Email appender
  log4perl.appender.Buffer          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout   = PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern=%d %m %n

    # Limiting appender, using the email appender above
  log4perl.appender.Limiter         = Log::Log4perl::Appender::Limit
  log4perl.appender.Limiter.appender     = Buffer
  log4perl.appender.Limiter.block_period = 3600
);

Log::Log4perl->init(\$conf);

my $logger = get_logger("");
$logger->warn("This message will be sent immediately");
$logger->warn("This message will be delayed by one hour.");

my $buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
like($buffer->buffer(), qr/immediately/);
unlike($buffer->buffer(), qr/delayed/);

    # Now flush the limiter and check again. The delayed message should now
    # be there.
my $limit = Log::Log4perl->appenders()->{Limiter};
$limit->flush();

like($buffer->buffer(), qr/immediately/);
like($buffer->buffer(), qr/delayed/);

$buffer->reset();
    # Nothing to flush
$limit->flush();
is($buffer->buffer(), "");

##################################################
# Flush method
##################################################
$conf .= <<EOT;
  log4perl.appender.Limiter.appender_method_on_flush = clear
EOT
Log::Log4perl->init(\$conf);
$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
$logger = get_logger("");
$logger->warn("This message will be queued but discarded on flush.");
$limit = Log::Log4perl->appenders()->{Limiter};
$limit->flush();

is($buffer->buffer(), "");

##################################################
# Limit Appender with max_until_discard
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = qq(
  log4perl.category = WARN, Limiter

    # Email appender
  log4perl.appender.Buffer          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout   = PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern=%d %m %n

    # Limiting appender, using the email appender above
  log4perl.appender.Limiter         = Log::Log4perl::Appender::Limit
  log4perl.appender.Limiter.appender     = Buffer
  log4perl.appender.Limiter.block_period = 3600
  log4perl.appender.Limiter.max_until_discarded = 1
);

Log::Log4perl->init(\$conf);

$logger = get_logger("");
$logger->warn("This message will be sent immediately");
for(1..10) {
    $logger->warn("This message will be discarded");
}

    # Artificially flush the limit appender
$limit = Log::Log4perl->appenders()->{Limiter};
$limit->flush();

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
like($buffer->buffer(), qr/immediately/);
unlike($buffer->buffer(), qr/discarded/);

##################################################
# Limit Appender with max_until_discard
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = qq(
  log4perl.category = WARN, Limiter

    # Email appender
  log4perl.appender.Buffer          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout   = PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern=%d %m %n

    # Limiting appender, using the email appender above
  log4perl.appender.Limiter         = Log::Log4perl::Appender::Limit
  log4perl.appender.Limiter.appender     = Buffer
  log4perl.appender.Limiter.block_period = 3600
  log4perl.appender.Limiter.max_until_discarded = 1
);

Log::Log4perl->init(\$conf);

$logger = get_logger("");
$logger->warn("This message will be sent immediately");
for(1..10) {
    $logger->warn("This message will be discarded");
}

    # Artificially flush the limit appender
$limit = Log::Log4perl->appenders()->{Limiter};
$limit->flush();

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
like($buffer->buffer(), qr/immediately/);
unlike($buffer->buffer(), qr/discarded/);

##################################################
# Limit Appender with max_until_flushed
##################################################
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

$conf = qq(
  log4perl.category = WARN, Limiter

    # Email appender
  log4perl.appender.Buffer          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout   = PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern=%d %m %n

    # Limiting appender, using the email appender above
  log4perl.appender.Limiter         = Log::Log4perl::Appender::Limit
  log4perl.appender.Limiter.appender     = Buffer
  log4perl.appender.Limiter.block_period = 3600
  log4perl.appender.Limiter.max_until_flushed = 2
);

Log::Log4perl->init(\$conf);

$logger = get_logger("");
$logger->warn("This message will be sent immediately");
$logger->warn("This message won't show right away");

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
like($buffer->buffer(), qr/immediately/);
unlike($buffer->buffer(), qr/right away/);

$logger->warn("This message will show right away");
like($buffer->buffer(), qr/right away/);


#################################
#demonstrating bug in Limiter.pm regarding $_
# Reset appender population
Log::Log4perl::Appender::TestBuffer->reset();

{package My::Test::Appender;
our @ISA = ('Log::Log4perl::Appender::TestBuffer');
sub new {
    my $self = shift;
    $_ = ''; #aye, there's the rub!
    $self->SUPER::new; 
}
}

$conf = qq(
  log4perl.category = WARN, Limiter

  log4perl.appender.Buffer          = My::Test::Appender
  log4perl.appender.Buffer.layout   = SimpleLayout
  log4perl.appender.Buffer.layout.ConversionPattern=%d %m %n

  log4perl.appender.Limiter         = Log::Log4perl::Appender::Limit
  log4perl.appender.Limiter.appender     = Buffer
  log4perl.appender.Limiter.block_period = 3600
);

Log::Log4perl->init(\$conf);
ok(1);

### API initialization
#
Log::Log4perl->reset();
my $bufApp = Log::Log4perl::Appender->new(
		'Log::Log4perl::Appender::TestBuffer',
		name     => 'MyBuffer',
		);
$bufApp->layout(
		Log::Log4perl::Layout::PatternLayout::Multiline->new(
			'%m%n')
		);
# Make the appender known to the system (without assigning it to
# any logger
Log::Log4perl->add_appender( $bufApp );

my $limitApp = Log::Log4perl::Appender->new(
	'Log::Log4perl::Appender::Limit',
	name       => 'MyLimit',
	appender   => 'MyBuffer',
	key        => 'nem',
	);
$limitApp->post_init();
$limitApp->composite(1);

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("MyBuffer");
get_logger("")->add_appender($limitApp);
get_logger("")->level($DEBUG);
get_logger("wonk")->debug("waah!");
is($buffer->buffer(), "waah!\n", "composite api init");

### Wrong %M with caching appender
#
Log::Log4perl->reset();
Log::Log4perl::Appender::TestBuffer->reset();

$conf = qq(
  log4perl.category = WARN, Limiter

    # TestBuffer appender
  log4perl.appender.Buffer          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout   = PatternLayout
  log4perl.appender.Buffer.layout.ConversionPattern=%d cat=%c meth=%M %m %n

    # Limiting appender, using the email appender above
  log4perl.appender.Limiter         = Log::Log4perl::Appender::Limit
  log4perl.appender.Limiter.appender     = Buffer
  log4perl.appender.Limiter.block_period = 3600
  log4perl.appender.Limiter.max_until_flushed = 2
);

Log::Log4perl->init(\$conf);

$logger = get_logger();

$logger->warn("Sent from main");

package Willy::Wonka;
sub func {
    use Log::Log4perl qw(get_logger);
    my $logger = get_logger();
    $logger->warn("Sent from func");
}
package main;

Willy::Wonka::func();
$logger->warn("Sent from main");

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
like($buffer->buffer(), 
     qr/cat=main meth=main::.*cat=Willy.Wonka meth=Willy::Wonka::func/s,
     "%M/%c with composite appender");

### Different caller stacks with normal vs. composite appenders
Log::Log4perl->reset();

$conf = qq(
  log4perl.category = WARN, Buffer1, Composite

    # 1st TestBuffer appender
  log4perl.appender.Buffer1          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer1.layout   = PatternLayout
  log4perl.appender.Buffer1.layout.ConversionPattern=meth=%M %m %n

    # 2nd TestBuffer appender
  log4perl.appender.Buffer2          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer2.layout   = PatternLayout
  log4perl.appender.Buffer2.layout.ConversionPattern=meth=%M %m %n

    # Composite Appender
  log4perl.appender.Composite         = Log::Log4perl::Appender::Buffer
  log4perl.appender.Composite.appender     = Buffer2
  log4perl.appender.Composite.trigger = sub { 1 }
);

Log::Log4perl->init(\$conf);

my $buffer1 = Log::Log4perl::Appender::TestBuffer->by_name("Buffer1");
my $buffer2 = Log::Log4perl::Appender::TestBuffer->by_name("Buffer2");

$logger = get_logger();

$logger->warn("Sent from main");

Willy::Wonka::func();

like $buffer1->buffer(), 
    qr/meth=main:: Sent from main.*meth=Willy::Wonka::func Sent from func/s,
    "caller stack from direct appender";
like $buffer2->buffer(),
    qr/meth=main:: Sent from main.*meth=Willy::Wonka::func Sent from func/s,
    "caller stack from composite appender";

# [RT 72056] Appender Threshold blocks composite appender

$conf = qq(
  log4perl.category = DEBUG, Composite

  log4perl.appender.Buffer          = Log::Log4perl::Appender::TestBuffer
  log4perl.appender.Buffer.layout   = PatternLayout
  log4perl.appender.Buffer.Threshold=INFO
  log4perl.appender.Buffer.layout.ConversionPattern=%M %m %n

    # Composite Appender
  log4perl.appender.Composite         = Log::Log4perl::Appender::Buffer
  log4perl.appender.Composite.appender = Buffer
  log4perl.appender.Composite.trigger = sub { 0 }

);

Log::Log4perl->init(\$conf);

$buffer = Log::Log4perl::Appender::TestBuffer->by_name("Buffer");
$logger = get_logger();
$logger->debug("this will be blocked by the appender threshold");

my $composite = Log::Log4perl->appender_by_name("Composite");
$composite->flush();

is $buffer->buffer(), "", 
   "appender threshold blocks message in composite appender";
