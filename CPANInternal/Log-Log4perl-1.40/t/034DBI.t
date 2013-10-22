###########################################
# Test using Log::Dispatch::DBI
# Kevin Goess <cpan@goess.org>
###########################################

BEGIN { 
    if($ENV{INTERNAL_DEBUG}) {
        require Log::Log4perl::InternalDebug;
        Log::Log4perl::InternalDebug->enable();
    }
}

BEGIN {
    use FindBin qw($Bin);
    use lib "$Bin/lib";
    require Log4perlInternalTest;
}

use Test::More;
use Log::Log4perl;
use warnings;
use strict;

BEGIN {
    my $minversion = \%Log::Log4perl::Internal::Test::MINVERSION;
    eval {
        require DBI;
        die if $DBI::VERSION < $minversion->{ "DBI" };

        require DBD::CSV;
        die if $DBD::CSV::VERSION < $minversion->{ "DBD::CSV" };

        require SQL::Statement;
        die if $SQL::Statement::VERSION < $minversion->{ "SQL::Statement" };
    };
    if ($@) {
        plan skip_all => 
          "DBI $minversion->{ DBI } or " .
          "DBD::CSV $minversion->{'DBD::CSV'} or " .
          "SQL::Statement $minversion->{'SQL::Statement'} " .
          "not installed, skipping tests\n";
    }else{
        plan tests => 33;
    }
}

END {
    unlink "t/tmp/log4perltest";
    rmdir "t/tmp";
}

mkdir "t/tmp" unless -d "t/tmp";

require DBI;
my $dbh = DBI->connect('DBI:CSV:f_dir=t/tmp','testuser','testpw',{ RaiseError => 1, PrintError => 1 });

$dbh->do('DROP TABLE log4perltest') if -e 't/tmp/log4perltest';

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
log4j.appender.DBAppndr             = Log::Log4perl::Appender::DBI
log4j.appender.DBAppndr.datasource = DBI:CSV:f_dir=t/tmp
log4j.appender.DBAppndr.username  = bobjones
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

log4j.appender.DBAppndr.bufferSize=2
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
$logger->fatal('fatal message',1234,'foo',{aaa => 'aaa'});

#since we ARE buffering, that message shouldnt be there yet
{
 local $/ = undef;
 open (F, "t/tmp/log4perltest");
 my $got = <F>;
 close F;
 my $expected = <<EOL;
LOGLEVEL,MESSAGE,SHORTCALLER,THINGID,CATEGORY,PKG,RUNTIME1,RUNTIME2
EOL
  $got =~ s/[^\w ,"()]//g;  #silly DBD_CSV uses funny EOL chars
  $expected =~ s/[^\w ,"()]//g;
  $got = lc $got;   #accounting for variations in DBD::CSV behavior
  $expected = lc $expected;
  is($got, $expected, "buffered");
}

$logger->warn('warning message',3456,'foo','bar');

#with buffersize == 2, now they should write
{
 local $/ = undef;
 open (F, "t/tmp/log4perltest");
 my $got = <F>;
 close F;
 my $expected = <<EOL;
LOGLEVEL,MESSAGE,SHORTCALLER,THINGID,CATEGORY,PKG,RUNTIME1,RUNTIME2
FATAL,"fatal message",main:,1234,groceries.beer,main,foo,HASH(0x84cfd64)
WARN,"warning message",main:,3456,groceries.beer,main,foo,bar
EOL
  $got =~ s/[^\w ,"()]//g;  #silly DBD_CSV uses funny EOL chars
  $expected =~ s/[^\w ,"()]//g;
  $got =~ s/HASH\(.+?\)//;
  $expected =~ s/HASH\(.+?\)//;
  $got = lc $got; #accounting for variations in DBD::CSV behavior
  $expected = lc $expected;
  is($got, $expected, "buffersize=2");
}


# setting is WARN so the debug message should not go through
$logger->debug('debug message',99,'foo','bar');
$logger->warn('warning message with two params',99, 'foo', 'bar');
$logger->warn('another warning to kick the buffer',99, 'foo', 'bar');

my $sth = $dbh->prepare('select * from log4perltest'); 
$sth->execute;

#first two rows are repeats from the last test
my $row = $sth->fetchrow_arrayref;
is($row->[0], 'FATAL');
is($row->[1], 'fatal message');
is($row->[3], '1234');
is($row->[4], 'groceries.beer');
is($row->[5], 'main');
is($row->[6], 'foo');
like($row->[7], qr/HASH/); #verifying param checking for "filter=>sub{...} stuff

$row = $sth->fetchrow_arrayref;
is($row->[0], 'WARN');
is($row->[1], 'warning message');
is($row->[3], '3456');
is($row->[4], 'groceries.beer');
is($row->[5], 'main');
is($row->[6], 'foo');
is($row->[7], 'bar');

#these two rows should have undef for the final two params
$row = $sth->fetchrow_arrayref;
is($row->[0], 'WARN');
is($row->[1], 'warning message with two params');
is($row->[3], '99');
is($row->[4], 'groceries.beer');
is($row->[5], 'main');
is($row->[6], 'foo');
is($row->[7], 'bar');

$row = $sth->fetchrow_arrayref;
is($row->[0], 'WARN');
is($row->[1], 'another warning to kick the buffer');
is($row->[3], '99');
is($row->[4], 'groceries.beer');
is($row->[5], 'main');
is($row->[6], 'foo');
is($row->[7], 'bar');
#that should be all
ok(!$sth->fetchrow_arrayref);

$dbh->disconnect;

# **************************************
# checking usePreparedStmt, spurious warning bug reported by Brett Rann
# might as well give it a thorough check
Log::Log4perl->reset;

unlink 't/tmp/log4perltest'
    if -e 't/tmp/log4perltest';

$dbh = DBI->connect('DBI:CSV:f_dir=t/tmp','testuser','testpw',{ PrintError => 1 });

$stmt = <<EOL;
    CREATE TABLE log4perltest (
      loglevel     char(9) ,   
      message   char(128)     
      
  )
EOL

$dbh->do($stmt) || die "do failed on $stmt".$dbh->errstr;


$config = <<'EOT';
log4j.category = WARN, DBAppndr
log4j.appender.DBAppndr             = Log::Log4perl::Appender::DBI
log4j.appender.DBAppndr.datasource = DBI:CSV:f_dir=t/tmp
log4j.appender.DBAppndr.sql = \
   insert into log4perltest \
   (loglevel, message) \
   values (?,?)
log4j.appender.DBAppndr.params.1 = %p    
#---------------------------- #2 is message

log4j.appender.DBAppndr.usePreparedStmt=2
log4j.appender.DBAppndr.warp_message=0
    
#noop layout to pass it through
log4j.appender.DBAppndr.layout    = Log::Log4perl::Layout::NoopLayout

EOT

Log::Log4perl::init(\$config);

$logger = Log::Log4perl->get_logger("groceries.beer");

$logger->fatal('warning message');

#since we're not buffering, this message should show up immediately
{
 local $/ = undef;
 open (F, "t/tmp/log4perltest");
 my $got = <F>;
 close F;
 my $expected = <<EOL;
LOGLEVEL,MESSAGE
FATAL,"warning message"
EOL
  $got =~ s/[^\w ,"()]//g;  #silly DBD_CSV uses funny EOL chars
  $expected =~ s/[^\w ,"()]//g;
  $got = lc $got; #accounting for variations in DBD::CSV behavior
  $expected = lc $expected;
  is($got, $expected);
}

$logger->fatal('warning message');

  # https://rt.cpan.org/Public/Bug/Display.html?id=79960
  # undef as NULL
$dbh->do('DROP TABLE log4perltest');
$stmt = <<EOL;
    CREATE TABLE log4perltest (
      loglevel     char(9) ,   
      message   char(128),
      mdc char(16)
      
  )
EOL

$dbh->do($stmt) || die "do failed on $stmt".$dbh->errstr;

$config = <<'EOT';
log4j.category = WARN, DBAppndr
log4j.appender.DBAppndr             = Log::Log4perl::Appender::DBI
log4j.appender.DBAppndr.datasource = DBI:CSV:f_dir=t/tmp
log4j.appender.DBAppndr.sql = \
   insert into log4perltest \
   (loglevel, mdc, message) \
   values (?, ?, ?)
log4j.appender.DBAppndr.params.1 = %p    
log4j.appender.DBAppndr.params.2 = %X{foo}
#---------------------------- #3 is message

log4j.appender.DBAppndr.usePreparedStmt=2
log4j.appender.DBAppndr.warp_message=0
    
#noop layout to pass it through
log4j.appender.DBAppndr.layout    = Log::Log4perl::Layout::NoopLayout

EOT

Log::Log4perl::init(\$config);

$logger = Log::Log4perl->get_logger();
$logger->warn('test message');

open (F, "t/tmp/log4perltest");
my $got = join '', <F>;
close F;

my $expected = <<EOT;
loglevel,message,mdc
WARN,"test message",
EOT

$got =~ s/[^\w ,"()]//g;  #silly DBD_CSV uses funny EOL chars
$expected =~ s/[^\w ,"()]//g;
is $got, $expected, "dbi insert with NULL values";
