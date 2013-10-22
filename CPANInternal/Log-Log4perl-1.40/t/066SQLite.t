###########################################
# Test DBI appender with SQLite
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

        require DBD::SQLite;
    };
    if ($@) {
        plan skip_all => 
          "DBI $minversion->{ DBI } " .
          "not installed, skipping tests\n";
    }else{
        plan tests => 3;
    }
}

my $testdir = "t/tmp";
mkdir $testdir;

my $dbfile = "$testdir/sqlite.dat";

END {
    unlink $dbfile;
    rmdir $testdir;
}

require DBI;

unlink $dbfile;
my $dbh = DBI->connect("dbi:SQLite:dbname=$dbfile","","");

  # https://rt.cpan.org/Public/Bug/Display.html?id=79960
  # undef as NULL
my $stmt = <<EOL;
    CREATE TABLE log4perltest (
      loglevel  char(9) ,   
      message   char(128),
      mdc       char(16)
  )
EOL

$dbh->do($stmt) || die "do failed on $stmt".$dbh->errstr;

my $config = <<"EOT";
log4j.category = WARN, DBAppndr
log4j.appender.DBAppndr            = Log::Log4perl::Appender::DBI
log4j.appender.DBAppndr.datasource = dbi:SQLite:dbname=$dbfile
log4j.appender.DBAppndr.sql = \\
   insert into log4perltest \\
   (loglevel, mdc, message) \\
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

my $logger = Log::Log4perl->get_logger();
$logger->warn('test message');

my $ary_ref = $dbh->selectall_arrayref( "SELECT * from log4perltest" );
is $ary_ref->[0]->[0], "WARN", "level logged in db";
is $ary_ref->[0]->[1], "test message", "msg logged in db";
is $ary_ref->[0]->[2], undef, "msg logged in db";
