###########################################
# Test Suite for RRDs appenders
# Mike Schilli, 2004 (m@perlmeister.com)
###########################################

use warnings;
use strict;

use Test::More;

use Log::Log4perl qw(get_logger);
    
my $DB = "myrrddb.dat";
    
BEGIN { eval 'require RRDs';
        if($@) {
            plan skip_all => "(RRDs not installed)";
            exit 0;
        } else {
            plan tests => 1;
        }
      };
END { unlink $DB };

use RRDs;

RRDs::create(
  $DB, "--step=1",
  "DS:myvalue:GAUGE:2:U:U",
  "RRA:MAX:0.5:1:120");
    
Log::Log4perl->init(\qq{
  log4perl.category = INFO, RRDapp
  log4perl.appender.RRDapp = Log::Log4perl::Appender::RRDs
  log4perl.appender.RRDapp.dbname = $DB
  log4perl.appender.RRDapp.layout = Log::Log4perl::Layout::PatternLayout
  log4perl.appender.RRDapp.layout.ConversionPattern = N:%m
});
    
my $logger = get_logger();
    
for(10, 15, 20) {
    $logger->info($_);
    sleep 1;
}

my ($start,$step,$names,$data) = 
    RRDs::fetch($DB, "MAX", 
                "--start" => time() - 20);
$data = join ' - ', map { "@$_" } grep { defined $_->[0] } @$data;
#print $data;

like($data, qr/\d\d/); 
