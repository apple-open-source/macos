###########################################
# Test Suite for Log::Log4perl::Logger
# Mike Schilli, 2002 (m@perlmeister.com)
###########################################

#use Data::Dump qw(dump);

use warnings;
use strict;

#########################
# used Test::Simple to help debug the test script
use Test::More tests => 70;

use Log::Log4perl;
use Log::Log4perl::Level;
use Log::Log4perl::Util;

ok(1); # If we made it this far, we're ok.

my $log0 = Log::Log4perl->get_logger("abc.def");
my $log1 = Log::Log4perl->get_logger("abc.def");
my $log2 = Log::Log4perl->get_logger("abc.def");
my $log3 = Log::Log4perl->get_logger("abc.def.ghi");
my $log4 = Log::Log4perl->get_logger("def.abc.def");
my $log5 = Log::Log4perl->get_logger("def.abc.def");
my $log6 = Log::Log4perl->get_logger("");
my $log7 = Log::Log4perl->get_logger("");
my $log8 = Log::Log4perl->get_logger("abc.def");
my $log9 = Log::Log4perl->get_logger("abc::def::ghi");

# Loggers for the same namespace have to be identical
ok($log1 == $log2, "Log1 same as Log2");
ok($log4 == $log5, "Log4 same as Log5");
ok($log6 == $log7, "Log6 same as Log7");
ok($log1 == $log8, "Log1 same as Log8");
ok($log3 == $log9, "log3 same as Log9");

# Loggers for different namespaces have to be different
ok($log1 != $log3, "Log1 not Log3");
ok($log3 != $log4, "Log3 not Log4");
ok($log1 != $log6, "Log1 not Log6");
ok($log3 != $log6, "Log3 not Log6");
ok($log5 != $log6, "Log5 not Log6");
ok($log5 != $log7, "Log5 not Log7");
ok($log5 != $log1, "Log5 not Log1");
ok($log7 != $log8, "Log7 not Log8");
ok($log8 != $log9, "Log8 not Log9");

my $app = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");

##################################################
# Suppress debug
##################################################
$log1->add_appender($app);
$log1->level($ERROR);

# warn "level is: ", $log1->level(), "\n";

my $ret;

$ret = $log1->error("Error Message");
ok($ret == 1);

$ret = $log1->debug("Debug Message");
ok(!defined $ret);

ok($app->buffer() eq "ERROR - Error Message\n", "log1 app buffer contains ERROR - Error Message");

# warn "app buffer is: \"", $app->buffer(), "\"\n";

##################################################
# Allow debug
##################################################
$log1->level($DEBUG);
$app->buffer("");
$log1->error("Error Message");
$log1->debug("Debug Message");
ok($app->buffer() eq "ERROR - Error Message\nDEBUG - Debug Message\n",
	"app buffer contains both ERROR and DEBUG message");

# warn "app buffer is: \"", $app->buffer(), "\"\n";

##################################################
# Multiple Appenders
##################################################
my $app2 = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");
my $app3 = Log::Log4perl::Appender->new(
    "Log::Log4perl::Appender::TestBuffer");

$app->buffer("");
$app2->buffer("");
    # 2nd appender to $log1
$log1->add_appender($app2);
$log1->level($ERROR);
$log1->error("Error Message");
#TODO
ok($app->buffer() eq "ERROR - Error Message\n", "app buffer contains ERROR only");
ok($app2->buffer() eq "ERROR - Error Message\n", "app2 buffer contains ERROR only");

##################################################
# Multiple Appenders in different hierarchy levels
##################################################
$app->buffer("");
$app2->buffer("");
$app3->buffer("");

$log1 = Log::Log4perl->get_logger("xxx.yyy.zzz");
$log2 = Log::Log4perl->get_logger("xxx");
$log3 = Log::Log4perl->get_logger("");

    # Root logger
$log3->add_appender($app3);

$log3->level($ERROR);

    ##################################################
    # Log to lower level, check if gets propagated up to root
    ##################################################
$log1->error("Error Message");

    # Should be distributed to root
ok($app3->buffer() eq "ERROR - Error Message\n", "app3 buffer contains ERROR");
    ##################################################
    # Log in lower levels and propagate to root
    ##################################################
$app->buffer("");
$app2->buffer("");
$app3->buffer("");

$log1->add_appender($app);
$log2->add_appender($app2);
# log3 already has app3 attached
$ret = $log1->error("Error Message");
ok($ret == 3);
ok($app->buffer() eq "ERROR - Error Message\n", "app buffer contains ERROR");
ok($app2->buffer() eq "ERROR - Error Message\n", "app2 buffer contains ERROR");
ok($app3->buffer() eq "ERROR - Error Message\n", "app3 buffer contains ERROR");

    ##################################################
    # Block appenders via priority 
    ##################################################
$app->buffer("");
$app2->buffer("");
$app3->buffer("");

$log1->level($ERROR);
$log2->level($DEBUG);
$log3->level($DEBUG);

$log1->debug("Debug Message");
ok($app->buffer() eq "", "app buffer is empty");
ok($app2->buffer() eq "", "app2 buffer is empty");
ok($app3->buffer() eq "", "app3 buffer is empty");

    ##################################################
    # Block via 'false' additivity
    ##################################################
$app->buffer("");
$app2->buffer("");
$app3->buffer("");

$log1->level($DEBUG);
$log2->additivity(0);
$log2->level($DEBUG);
$log3->level($DEBUG);

$log1->debug("Debug Message");
ok($app->buffer() eq "DEBUG - Debug Message\n", "app buffer contains DEBUG");
ok($app2->buffer() eq "DEBUG - Debug Message\n", "app2 buffer contains DEBUG");
ok($app3->buffer() eq "", "app3 buffer is empty");

    ##################################################
    # Check is_*() functions
    ##################################################
$log0->level($TRACE);
$log1->level($DEBUG);
$log2->level($ERROR);
$log3->level($INFO);

ok($log0->is_trace(), "log0 is_trace == 1");
ok($log0->is_error(), "log0 is_error == 1");

ok($log1->is_error(), "log1 is_error == 1");
ok($log1->is_info(), "log1 is_info == 1");
ok($log1->is_fatal(), "log1 is_fatal == 1");
ok($log1->is_debug(), "log1 is_debug == 1");

ok($log2->is_error(), "log2 is_error == 1");
ok(!$log2->is_info(), "log2 is_info == 0");
ok($log2->is_fatal(), "log2 is_fatal == 1");
ok(!$log2->is_debug(), "log2 is_debug == 0");

ok($log3->is_error(), "log3 is_error == 1");
ok($log3->is_info(), "log3 is_info == 1");
ok($log3->is_fatal(), "log3 is_fatal == 1");
ok(!$log3->is_debug(), "log3 is_debug == 0");


    ##################################################
    # Check is_*() functions with text
    ##################################################
$log3->level('DEBUG');
$log2->level('ERROR');
$log1->level('INFO');

ok($log3->is_error(), "log3 is_error == 1");
ok($log3->is_info(), "log3 is_info == 1");
ok($log3->is_fatal(), "log3 is_fatal == 1");
ok($log3->is_debug(), "log3 is_debug == 1");

ok($log2->is_error(), "log2 is_error == 1");
ok(!$log2->is_info(), "log2 is_info == 0");
ok($log2->is_fatal(), "log2 is_fatal == 1");
ok(!$log2->is_debug(), "log2 is_debug == 0");

ok($log1->is_error(), "log1 is_error == 1");
ok($log1->is_info(), "log1 is_info == 1");
ok($log1->is_fatal(), "log1 is_fatal == 1");
ok(!$log1->is_debug(), "log1 is_debug == 0");


    ##################################################
    # Check log->(<level_const>,<msg>)
    ##################################################
$app->buffer("");
$app2->buffer("");
$app3->buffer("");

$log1->level($DEBUG);
$log2->level($ERROR);
$log3->level($INFO);

$log1->log($DEBUG, "debug message");
$log1->log($INFO,  "info message ");

$log2->log($DEBUG, "debug message");
$log2->log($INFO,  "info message ");

$log3->log($DEBUG, "debug message");
$log3->log($INFO,  "info message ");

ok($app->buffer() eq "DEBUG - debug message\nINFO - info message \n",
	"app  buffer contains DEBUG and INFO");
ok($app2->buffer() eq "DEBUG - debug message\nINFO - info message \n",
	"app2 buffer contains DEBUG");
ok($app3->buffer() eq "INFO - info message \n",
	"app3 buffer contains INFO");

    ##################################################
    # Check several messages concatenated
    ##################################################
$app->buffer("");

$log1->level($DEBUG);

$log1->log($DEBUG, "1", " ", "2", " ");
$log1->debug("3 ", "4 ");
$log1->info("5 ", "6 ");
$log1->warn("7 ", "8 ");
$log1->error("9 ", "10 ");
$log1->fatal("11 ", "12 ", "13 ");

my $got = $app->buffer();
my $expected = <<EOT;
DEBUG - 1 2 
DEBUG - 3 4 
INFO - 5 6 
WARN - 7 8 
ERROR - 9 10 
FATAL - 11 12 13 
EOT

ok($got eq $expected) || print STDERR "got $got\n expected $expected";


#ok($app->buffer() eq <<EOT, "app buffer six lines");
#DEBUG - 1 2 
#DEBUG - 3 4 
#INFO - 5 6 
#WARN - 7 8 
#ERROR - 9 10 
#FATAL - 11 12 13 
#EOT

    ##################################################
    # Check several messages concatenated
    ##################################################
$app->buffer("");

$log1->level($DEBUG);

$log1->log($DEBUG, sub { "1" . " " . "2" } );
$log1->info(
    sub { "3 " . "4 " }, # subroutine
                         # filter (throw out blanks)
    { filter => sub { my $v = shift;
                      $v =~ s/\s+//g; 
                      return $v;
                    },
      value  => "  5   6 " },
    " 7" );

is($app->buffer(), <<EOT, "app buffer contains 2 lines");
DEBUG - 1 2
INFO - 3 4 56 7
EOT

# warn("app buffer is: ", $app->buffer(), "\n");

############################################################
# testing multiple parameters, nested hashes
############################################################

our $stub_hook;

# -----------------------------------
# here/s a stub
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

$app = Log::Log4perl::Appender->new(
    "Log::Log4perl::AppenderTester",
    name  => 'dumpy',
    login => { hostname => 'a.jabber.server',
               port     => 5222,
               username => "bugs",
               password => "bunny",
               resource => "logger" },
    to    => [ 'elmer@a.jabber.server', 'sam@another.jabber.server' ],
);

ok($stub_hook->{P}{login}{hostname}, 'a.jabber.server');
ok($stub_hook->{P}{login}{password}, 'bunny');
ok($stub_hook->{P}{to}[0], 'elmer@a.jabber.server');
ok($stub_hook->{P}{to}[1], 'sam@another.jabber.server');

# ------------------------------------
# Check if we get all appenders
    
my $href   = Log::Log4perl->appenders(); 
my $result = "";
    
for(sort keys %$href) {
    $result .= "$_ => " . ref($href->{$_}->{appender});
}

like($result, qr/(app\d+.*?Log::Log4perl::Appender::TestBuffer){3}/, 
     "all appenders");


##################################################
# Bug reported by Brian Edwards: add_appender()
# with screen/file appender fails because of missing
# base class declaration
##################################################
my $log10 = Log::Log4perl->get_logger("xxx.yyy.zzz");

use Log::Log4perl::Appender::Screen;
use Log::Log4perl::Appender::File;

my $app_screen = Log::Log4perl::Appender::Screen->new();

my $tmpfile = Log::Log4perl::Util::tmpfile_name();
END { unlink $tmpfile };

my $app_file = Log::Log4perl::Appender::File->new(
    filename => $tmpfile
);

eval { $log10->add_appender($app_file); };
is($@, "", "Adding file appender");
eval { $log10->add_appender($app_screen); };
is($@, "", "Adding screen appender");

