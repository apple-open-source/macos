###########################################
# Test using Log::Dispatch::DBI
# Kevin Goess <cpan@goess.org>
###########################################

use strict;
use warnings;

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

use Test::More;

use Log::Log4perl;

BEGIN {
    use FindBin qw($Bin);
    use lib "$Bin/lib";
    require Log4perlInternalTest;
}

BEGIN {
    my $minversion = \%Log::Log4perl::Internal::Test::MINVERSION;
    eval {
        require DBD::CSV;
        die if $DBD::CSV::VERSION < $minversion->{ "DBD::CSV" };

	require Log::Dispatch;
    };
    if ($@) {
        plan skip_all => 
          "only with Log::Dispatch and DBD::CSV $minversion->{'DBD::CSV'}";
    }else{
        plan tests => 14;
    }
}

END {
    unlink "t/tmp/log4perltest";
    rmdir "t/tmp";
}

mkdir "t/tmp" unless -d "t/tmp";

require DBI;
my $dbh = DBI->connect('DBI:CSV:f_dir=t/tmp','testuser','testpw',{ PrintError => 1 });

-e "t/tmp/log4perltest" && $dbh->do('DROP TABLE log4perltest');

my $stmt = <<EOL;
    CREATE TABLE log4perltest (
      loglevel     char(9) ,   
      message   char(128),     
      shortcaller   char(5),  
      thingid    char(6),       
      category  char(16),      
      pkg    char(16),
      runtime1 char(16),
      runtime2 char(16)
      
  )
EOL

$dbh->do($stmt);

#creating a log statement where bind values 1,3,5 and 6 are 
#calculated from conversion specifiers and 2,4,7,8 are 
#calculated at runtime and fed to the $logger->whatever(...)
#statement

my $config = <<'EOT';
#log4j.category = WARN, DBAppndr, console
log4j.category = WARN, DBAppndr
log4j.appender.DBAppndr             = org.apache.log4j.jdbc.JDBCAppender
log4j.appender.DBAppndr.URL = jdbc:CSV:testdb://localhost:9999;f_dir=t/tmp
log4j.appender.DBAppndr.user  = bobjones
log4j.appender.DBAppndr.password = 12345
log4j.appender.DBAppndr.sql = \
   insert into log4perltest \
   (loglevel, message, shortcaller, thingid, category, pkg, runtime1, runtime2) \
   values (?,?,?,?,?,?,?,?)
log4j.appender.DBAppndr.params.1 = %p    
#---------------------------- #2 is message
log4j.appender.DBAppndr.params.3 = %5.5l
#---------------------------- #4 is thingid
log4j.appender.DBAppndr.params.5 = %c
log4j.appender.DBAppndr.params.6 = %C
#-----------------------------#7,8 are also runtime

log4j.appender.DBAppndr.bufferSize=3
log4j.appender.DBAppndr.warp_message=0
    
#noop layout to pass it through
log4j.appender.DBAppndr.layout    = Log::Log4perl::Layout::NoopLayout

#a console appender for debugging
log4j.appender.console = Log::Log4perl::Appender::Screen
log4j.appender.console.layout = Log::Log4perl::Layout::SimpleLayout

EOT

Log::Log4perl::init(\$config);


# *********************
# check a category logger

my $logger = Log::Log4perl->get_logger("groceries.beer");

#$logger->fatal('fatal message',1234,'foo','bar');
$logger->fatal('fatal message',1234,'foo', 'bar');
$logger->warn('warning message',3456,'foo','bar');
$logger->debug('debug message',99,'foo','bar');

my $sth = $dbh->prepare('select * from log4perltest');
$sth->execute;

my $row = $sth->fetchrow_arrayref;
is($row->[0], 'FATAL');
is($row->[1], 'fatal message');
is($row->[3], '1234');
is($row->[4], 'groceries.beer');
is($row->[5], 'main');
is($row->[6], 'foo');
is($row->[7], 'bar');

$row = $sth->fetchrow_arrayref;
is($row->[0], 'WARN');
is($row->[1], 'warning message');
is($row->[3], '3456');
is($row->[4], 'groceries.beer');
is($row->[5], 'main');
is($row->[6], 'foo');
is($row->[7], 'bar');

$dbh->do('DROP TABLE log4perltest');

1;
