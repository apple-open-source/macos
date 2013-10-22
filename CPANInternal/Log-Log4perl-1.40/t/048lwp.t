###########################################
# Test Suite for LWP debugging with Log4perl
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
        require LWP::UserAgent;
        die "Skip tests" if $LWP::UserAgent::VERSION <  2.0;
        die "Skip tests" if $LWP::UserAgent::VERSION >= 5.822;
    };

    if($@) {
        plan skip_all => "Only with 2.0 < LWP::UserAgent < 5.822 ";
    } else {
        plan tests => 3;
    }
}

use Log::Log4perl qw(:easy);
use Log::Log4perl::Util;

Log::Log4perl->easy_init(
    { level    => $DEBUG,
      category => "LWP::UserAgent",
      file     => 'lwpout.txt'
    });

Log::Log4perl->infiltrate_lwp();

my $ua = LWP::UserAgent->new();

my $tmpfile = Log::Log4perl::Util::tmpfile_name();
END { unlink $tmpfile };
$ua->get("file:$tmpfile");

open LOG, "<lwpout.txt" or die "Cannot open lwpout.txt";
my $data = join('', <LOG>);
close LOG;

like($data, qr#\QGET file:$tmpfile\E#);

END { unlink "lwpout.txt" }

####################################
# Check different category
####################################
Log::Log4perl->reset();
Log::Log4perl->easy_init(
    { level    => $DEBUG,
      category => "LWP::SchmoozeAgent",
      file     => '>lwpout.txt'
    });

Log::Log4perl->infiltrate_lwp();

$ua = LWP::UserAgent->new();
$ua->get("file:$tmpfile");

open LOG, "<lwpout.txt" or die "Cannot open lwpout.txt";
$data = join('', <LOG>);
close LOG;

is($data, '');

####################################
# Check layout
####################################
Log::Log4perl->reset();
Log::Log4perl->easy_init(
    { level    => $DEBUG,
      category => "LWP::UserAgent",
      file     => '>lwpout.txt',
      layout   => '%F-%L: %m%n',
    });

Log::Log4perl->infiltrate_lwp();

$ua = LWP::UserAgent->new();
$ua->get("file:$tmpfile");

open LOG, "<lwpout.txt" or die "Cannot open lwpout.txt";
$data = join('', <LOG>);
close LOG;

like($data, qr#LWP/UserAgent.pm-\d+#);
