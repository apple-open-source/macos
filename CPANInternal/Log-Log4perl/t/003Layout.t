###########################################
# Test Suite for Log::Log4perl
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

use warnings;
use strict;

#########################
# change 'tests => 1' to 'tests => last_test_to_print';
#########################
use Test::More;
BEGIN { plan tests => 21 };

use Log::Log4perl;
use Log::Log4perl::Layout;

use Log::Log4perl::Level;
use Log::Log4perl::Appender::TestBuffer;
use File::Spec;

my $app = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");

ok(1); # If we made it this far, we/re ok.

my $logger = Log::Log4perl->get_logger("abc.def.ghi");
$logger->add_appender($app);
my $layout = Log::Log4perl::Layout::PatternLayout->new(
    "bugo %% %c{2} %-17F{2} %L hugo");
$app->layout($layout);
$logger->debug("That's the message");

is($app->buffer(), "bugo % def.ghi " . 
                   File::Spec->catfile(qw(t 003Layout.t)) .
                   "     32 hugo"); 

############################################################
# Log the message
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new(
   "The message is here: %m");
$app->layout($layout);
$logger->debug("That's the message");

is($app->buffer(), "The message is here: That's the message"); 

############################################################
# Log the time
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("[%r] %m");
$app->layout($layout);
$logger->debug("That's the message");

like($app->buffer(), qr/^\[\d+\] That's the message$/); 

############################################################
# Log the date/time
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%d> %m");
$app->layout($layout);
$logger->debug("That's the message");

like($app->buffer(), 
   qr#^\d{4}/\d\d/\d\d \d\d:\d\d:\d\d> That\'s the message$#); 

############################################################
# Log the date/time with own timer function
############################################################
sub mytimer1 {
    # 2 days after 1/1/1970 to compensate for time zones
    return 180000;
}

$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new(
  { time_function => \&mytimer1 }, "%d{MM/yyyy}> %m");
$app->layout($layout);
$logger->debug("That's the message");
like($app->buffer(), qr{01/1970}); 

############################################################
# Check SimpleLayout
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::SimpleLayout->new();
$app->layout($layout);
$logger->debug("That's the message");

is($app->buffer(), "DEBUG - That\'s the message\n"); 

############################################################
# Check depth level of %M - with debug(...)
############################################################

sub mysubroutine {
    $app->buffer("");
    $layout = Log::Log4perl::Layout::PatternLayout->new("%M: %m");
    $app->layout($layout);
    $logger->debug("That's the message");
}

mysubroutine();
is($app->buffer(), 'main::mysubroutine: That\'s the message'); 

############################################################
# Check depth level of %M - with debug(...)
############################################################

$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%M: %m");
$app->layout($layout);
$logger->debug("That's the message");

is($app->buffer(), 'main::: That\'s the message'); 

############################################################
# Check Filename and Line #
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%F-%L %m");
$app->layout($layout);
$logger->debug("That's the message");

like($app->buffer(), qr/003Layout.t-126 That's the message/); 

############################################################
# Don't append a newline if the message already contains one
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%m%n");
$app->layout($layout);
$logger->debug("That's the message\n");

is($app->buffer(), "That\'s the message\n");

############################################################
# But don't suppress other %ns
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("a%nb%n%m%n");
$app->layout($layout);
$logger->debug("That's the message\n");

is($app->buffer(), "a\nb\nThat\'s the message\n");

############################################################
# Test if the process ID works
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%P:%m");
$app->layout($layout);
$logger->debug("That's the message\n");

like($app->buffer(), qr/^\d+:That's the message$/);

############################################################
# Test if the hostname placeholder %H works
############################################################
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%H:%m");
$app->layout($layout);
$logger->debug("That's the message\n");

like($app->buffer(), qr/^[^:]+:That's the message$/);

############################################################
# Test max width in the format specifiers
############################################################
#min width
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%5.5m");
$app->layout($layout);
$logger->debug("123");
is($app->buffer(), '  123');

#max width
$app->buffer("");
$logger->debug("1234567");
is($app->buffer(), '12345');

#left justify
$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%-5.5m");
$app->layout($layout);
$logger->debug("123");
is($app->buffer(), '123  ');

############################################################
# Check depth level of %M - with eval {...}
############################################################

$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%M: %m");
$app->layout($layout);
sub foo {
    eval {
        $logger->debug("Thats the message");
    };
}
foo();
is($app->buffer(), 'main::foo: Thats the message'); 

############################################################
# Check two levels of %M - with eval {...}
############################################################

$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%M: %m");
$app->layout($layout);
sub foo2 {
    eval {
        eval {
            $logger->debug("Thats the message");
        };
    };
}
foo2();
is($app->buffer(), 'main::foo2: Thats the message'); 

############################################################
# Check depth level of %M - with eval {...}
############################################################

$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout->new("%M: %m");
$app->layout($layout);
eval {
    $logger->debug("Thats the message");
};
is($app->buffer(), 'main::: Thats the message'); 

############################################################
# Render a multiline message
############################################################

$app->buffer("");
$layout = Log::Log4perl::Layout::PatternLayout::Multiline->new("%M: %m%n");
$app->layout($layout);
eval {
    $logger->debug("Thats the\nmultiline\nmessage");
};
is($app->buffer(), "main::: Thats the\nmain::: multiline\nmain::: message\n"); 

