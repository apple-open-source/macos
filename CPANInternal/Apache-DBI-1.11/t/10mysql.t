use strict;
use Test::More tests => 10;

BEGIN { 
  # trick DBI.pm into thinking we are running under mod_perl
  # set both %ENV keys for old and new DBI versions

  $ENV{MOD_PERL} = 'CGI-Perl';          # for $DBI::VERSION > 1.33
  $ENV{GATEWAY_INTERFACE} = 'CGI-Perl'; # for older DBI.pm  
  
  use_ok('Apache::DBI');
  use_ok('DBI');
};

my $dbd_mysql = eval { require DBD::mysql };

#$Apache::DBI::DEBUG = 10;
#DBI->trace(2");

SKIP: {
  skip "Could not load DBD::mysql", 8 unless $dbd_mysql;

  ok($dbd_mysql, "DBD::mysql loaded");

  SKIP: {
    skip 'Can only check "connect_via" in DBI >= 1.38', 1 unless $DBI::VERSION >= 1.38;

    # checking private DBI data here is probably bad...
    is($DBI::connect_via, 'Apache::DBI::connect', 'DBI is using Apache::DBI');
  }


  my $dbh_1 = DBI->connect('dbi:mysql:test', undef, undef, { RaiseError => 0, PrintError => 0 });

 SKIP: {
    skip "Could not connect to test database: $DBI::errstr", 6 unless $dbh_1;

    isa_ok($dbh_1, 'Apache::DBI::db');
	
    ok(my $thread_1 = $dbh_1->{'mysql_thread_id'}, "Connected 1");

    my $dbh_2 = DBI->connect('dbi:mysql:test', undef, undef, { RaiseError => 0, PrintError => 0 });
    ok(my $thread_2 = $dbh_2->{'mysql_thread_id'}, "Connected 2");

    is($thread_1, $thread_2, "got the same connection both times");

    my $dbh_3 = DBI->connect('dbi:mysql:test', undef, undef, { RaiseError => 0, PrintError => 1 });
    ok(my $thread_3 = $dbh_3->{'mysql_thread_id'}, "Connected 3");

    isnt($thread_1, $thread_3, "got different connection from different attributes");

  }

} 

1;
