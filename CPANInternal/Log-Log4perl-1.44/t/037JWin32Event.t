BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Log::Log4perl;
use Test::More;


#skipping on non-win32 systems
BEGIN {
    eval {
	    require Log::Dispatch::Win32EventLog;
    };
    if ($@){
       plan skip_all => "only with Log::Dispatch::Win32EventLog";
    }
};

print <<EOL;

Since EventLog doesn't return any value that indicates sucess or failure,
I'm just going to send messages to the EventLog.  You can see these
messages using the event viewer:

INFO - info message 1
WARN - warning message 1

(Probably prefaced with something like "The description for Event ID ( 0 ) 
in Source ( t/037JWinEvent.t ) cannot be found... ")


EOL


my $conf = <<CONF;
log4j.category.cat1      = INFO, myAppender

log4j.appender.myAppender=org.apache.log4j.NTEventLogAppender
log4j.appender.myAppender.source=$0
log4j.appender.myAppender.layout=org.apache.log4j.SimpleLayout
CONF

Log::Log4perl->init(\$conf);

my $logger = Log::Log4perl->get_logger('cat1');


$logger->debug("debugging message 1 ");
$logger->info("info message 1 ");      
$logger->warn("warning message 1 ");   


BEGIN {plan tests => 1}

#if we didn't die, we got here
ok(1);
