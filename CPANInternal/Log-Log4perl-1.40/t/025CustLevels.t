###########################################
# Test Suite for Log::Log4perl::Config
# Erik Selberg, (c) 2002 erik@selberg.com
# clone of 025CustLevels.t but uses nicer method (?) we hope
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
use Test;

#create a custom level "LITEWARN"
use Log::Log4perl;
use Log::Log4perl::Level;
use Log::Log4perl::Appender::TestBuffer;
# use strict;


ok(1); # If we made it this far, we're ok.

Log::Log4perl::Logger::create_custom_level("LITEWARN", "WARN");
#testing for bugfix of 9/19/03 before which custom levels beneath DEBUG didn't work
Log::Log4perl::Logger::create_custom_level("DEBUG2", "DEBUG");

# test insane creation of levels

foreach (1 .. 14) {
  ok(Log::Log4perl::Logger::create_custom_level("TEST$_", "INFO"), 0);
}

# 15th should fail.. this assumes that each level is 10000 apart from
# the other.

ok(!defined eval { Log::Log4perl::Logger::create_custom_level("TEST15", "INFO") });

# now, by re-arranging (as we whine about in create_custom_levels), we
# should be able to get 15.

my %btree = (
             8 => "DEBUG",
	     4 => 8,
	     2 => 4,
	     1 => 2,
	     3 => 4,
	     6 => 8,
	     5 => 6,
	     7 => 8,
	     12 => "DEBUG",
	     10 => 12,
	     9 => 10,
	     11 => 12,
	     14 => "DEBUG",
	     13 => 14,
	     15 => "DEBUG",
	     );

foreach (8, 4, 2, 1, 3, 6, 5, 7, 12, 10, 9, 11, 14, 13, 15) {
  my $level = $btree{$_} eq "DEBUG" ? "DEBUG" : "BTREE$btree{$_}";
#  warn("Creating BTREE$_ after $level");
  ok(Log::Log4perl::Logger::create_custom_level("BTREE$_", $level), 0);
#  warn("BTREE$_ is ", ${Log::Log4perl::Level::PRIORITY{"BTREE$_"}});
}

# foreach (1 .. 15) {
#    warn("BTREE$_ is: ", ${Log::Log4perl::Level::PRIORITY{"BTREE$_"}});
# }


my $LOGFILE = "example.log";
unlink $LOGFILE;

my $config = <<EOT;
log4j.category = LITEWARN, FileAppndr
log4j.appender.FileAppndr          = Log::Log4perl::Appender::File
log4j.appender.FileAppndr.filename = $LOGFILE
log4j.appender.FileAppndr.layout   = Log::Log4perl::Layout::SimpleLayout

log4j.category.debug2test = DEBUG2, FileAppndr
log4j.additivity.debug2test= 0
EOT


Log::Log4perl::init(\$config);


# can't create a custom level after init... let's test that. Just look
# for an undef (i.e. failure) from the eval

ok(!defined eval { Log::Log4perl::Logger::create_custom_level("NOTIFY", "WARN"); });


# *********************
# check a category logger

my $logger = Log::Log4perl->get_logger("groceries.beer");
$logger->warn("this is a warning message");
$logger->litewarn("this is a LITE warning message (2/3 the calories)");
$logger->info("this info message should not log");


open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
$/ = undef;
my $data = <FILE>;
close FILE;
my $result1 = "WARN - this is a warning message\nLITEWARN - this is a LITE warning message (2/3 the calories)\n";
ok($data, $result1);

# *********************
# check the root logger
my $rootlogger = Log::Log4perl->get_logger("");
$logger->warn("this is a rootlevel warning message");
$logger->litewarn("this is a rootlevel  LITE warning message (2/3 the calories)");
$logger->info("this rootlevel  info message should not log");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
$/ = undef;
$data = <FILE>;
close FILE;
my $result2 = "WARN - this is a rootlevel warning message\nLITEWARN - this is a rootlevel  LITE warning message (2/3 the calories)\n";
ok($data, "$result1$result2");

$logger->log($WARN, "a warning message");
$logger->log($LITEWARN, "a LITE warning message");
die("lame hack to suppress warning") if ($LITEWARN != $LITEWARN);
$logger->log($DEBUG, "an info message, should not log");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
$/ = undef;
$data = <FILE>;
close FILE;
my $result3 = "WARN - a warning message\nLITEWARN - a LITE warning message\n";
ok($data, "$result1$result2$result3");

# *********************
# check debug2 level
my $debug2 = Log::Log4perl->get_logger("debug2test");
$debug2->debug2("this is a debug2 message");

open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
$/ = undef;
$data = <FILE>;
close FILE;
my $result4 = "DEBUG2 - this is a debug2 message\n";
ok($data, "$result1$result2$result3$result4");

#*********************
#check the is_* methods
ok($logger->is_warn);
ok($logger->is_litewarn);
ok(! $logger->is_info);


# warn("Testing inc_level()");

#***************************
#increase/decrease leves
$logger->inc_level(1);  #bump up from litewarn to warn
# warn("level is now: ", $logger->level());
ok($logger->is_warn);
ok(!$logger->is_litewarn);
ok(!$logger->is_info);
$logger->warn("after bumping, warning message");
$logger->litewarn("after bumping, lite warning message, should not log");
open FILE, "<$LOGFILE" or die "Cannot open $LOGFILE";
$/ = undef;
$data = <FILE>;
close FILE;
my $result5 = "WARN - after bumping, warning message\n";
ok($data, "$result1$result2$result3$result4$result5");

$logger->dec_level(2); #bump down from warn to litewarn to info

ok($logger->is_warn);
ok($logger->is_litewarn);
ok($logger->is_info);

ok(! $logger->is_debug) ;

$logger->level($FATAL);

ok($logger->is_fatal() && !($logger->is_error() || $logger->is_warn() ||
	$logger->is_info() || $logger->is_debug()));

$logger->more_logging(); # should inc one level

ok($logger->is_fatal() && $logger->is_error() && !( $logger->is_warn() ||
	$logger->is_info() || $logger->is_debug()));

$logger->more_logging(100); # should be debug now

ok($logger->is_fatal() && $logger->is_error() && $logger->is_warn() &&
	$logger->is_info() && $logger->is_debug());

$logger->less_logging(150); # should be OFF now

ok(!($logger->is_fatal() || $logger->is_error() || $logger->is_warn() ||
	$logger->is_info() || $logger->is_debug()));

BEGIN { plan tests => 51 };

unlink $LOGFILE;
