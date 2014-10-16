#!perl -w
#!d:\perl\bin\perl.exe 

# -- SOAP::Lite -- soaplite.com -- Copyright (C) 2001 Paul Kulchenko --

use strict;
use Benchmark;
use SOAP::Lite on_fault => sub {my($soap, $res) = @_; die ref $res ? $res->faultdetail : $soap->transport->status, "\n"};
use My::Examples;

my %dests = (
  local              => ['local://localhost/cgi-bin/soap.cgi' => 'http://www.soaplite.com/My/Examples'],
  mod_perl           => ['http://localhost/soap/' => 'http://www.soaplite.com/My/Examples'],
  CGI                => ['http://localhost/cgi-bin/soap.cgi' => 'http://www.soaplite.com/My/Examples'],
  daemon             => ['http://localhost:81/' => 'http://www.soaplite.com/My/Examples'],
  'Apache::Registry' => ['http://localhost/mod_perl/soap.mod_cgi' => 'http://www.soaplite.com/My/Examples'],
  tcpip              => ['tcp://localhost:82' => 'http://www.soaplite.com/My/Examples'],
  direct             => ['' => 'My::Examples'],
);

my $s;

my %tests = (
  simple => sub {$s->getStateName(1)},
  array =>  sub {$s->getStateName((1) x 100)},
  string =>  sub {$s->getStateName(1 x 100)},
);

my $testnum = 3;
my $testtime = 5;
my %result;

print STDERR <<DISCLAIMER;

This test should be used only for comparison different Perl server
implementations running in your environment. 

DISCLAIMER

print STDERR "All tests may take up to ", keys(%dests) * keys(%tests) * $testnum * $testtime, " sec\n";

foreach my $dest (keys %dests) {
  my($proxy, $uri) = @{$dests{$dest}};
  $s = $proxy ? SOAP::Lite->proxy($proxy)->uri($uri) : $uri;
  foreach my $test (keys %tests) {
    printf STDERR "%s [%s] ", $dest, $test;
    eval {$tests{$test}->()}; warn('skipped, ', $@), next if $@;
    my($tps) = 0;
    for (1..$testnum) {
      my $r = Benchmark::countit($testtime => $tests{$test});
      my($pu, $ps, $n) = @{$r}[1,2,5];
      $tps += $n / ($pu + $ps);
      print STDERR ".";
    }
    printf STDERR " %.5s call/s\n", $result{$dest}{$test} = $tps / $testnum;
  }
}
