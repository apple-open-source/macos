
# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

use Test;
use strict;
use Getopt::Std;

my %opts;

getopts('d:',\%opts);

my @test_table;

BEGIN {

  open TESTFILE, "test.txt";
  @test_table = grep { /\S/ and not /^\s*#/ } <TESTFILE>;
  chomp @test_table;
  close TESTFILE;

  plan tests => 1+ @test_table ;
};

use Mail::SPF::Query;

# 1: did the library load okay?
ok(1);

if ($opts{d}) {
    open(TEST, ">$opts{d}") || die "Cannot open $opts{d} for output";
}

my $testnum = 2;

#########################

foreach my $tuple (@test_table) {
  my ($num, $domain, $ipv4, $expected_result, $expected_smtp_comment, $expected_header_comment) = $tuple =~ /\t/ ? split(/\t/, $tuple) : split(' ', $tuple);

  my ($actual_result, $actual_smtp_comment, $actual_header_comment);

  my ($sender, $localpolicy) = split(':', $domain, 2);
  $sender =~ s/\\([0-7][0-7][0-7])/chr(oct($1))/ge;
  $domain = $sender;
  if ($domain =~ /\@/) { ($domain) = $domain =~ /\@(.+)/ }

  my $testcnt = 3;

  if ($expected_result =~ /=(pass|fail),/) {
      for (my $debug = 0; $debug < 2; $debug++) {
          Mail::SPF::Query->clear_cache;
          my $query = eval  { new Mail::SPF::Query (ipv4   => $ipv4,
                                                    sender => $sender,
                                                    helo   => $domain,
                                                    debug  => $debug,
                                                    local  => $localpolicy,
                                                   ); };

          my $ok = 1;
          my $header_comment;

          $actual_result = "";

          foreach my $e_result (split(/,/, $expected_result)) {
              if ($e_result !~ /=/) {
                  my ($msg_result, $smtp_comment);
                  ($msg_result, $smtp_comment, $header_comment) = eval { $query->message_result2 };

                  $actual_result .= $msg_result;

                  $ok = ok($msg_result, $e_result) if (!$debug);
                  if (!$ok) {
                      last;
                  }
              } else {
                  my ($recip, $expected_recip_result) = split(/=/, $e_result, 2);
                  my ($recip_result, $smtp_comment) = eval { $query->result2(split(';',$recip)) };

                  $actual_result .= "$recip=$recip_result,";
                  $testcnt++;

                  $ok = ok($recip_result, $expected_recip_result) if (!$debug);
                  if (!$ok) {
                      last;
                  }
              }
          }

          $header_comment =~ s/\S+: //; # strip the reporting hostname prefix

          if ($expected_header_comment) {
              $ok &= ok($header_comment, $expected_header_comment) if (!$debug);
          }
          $actual_header_comment = $header_comment;
          $actual_smtp_comment = '.';
          last if ($ok);
      }
  } else {
      my ($result, $smtp_comment, $header_comment) = eval  { new Mail::SPF::Query (ipv4   => $ipv4,
                                                                                   sender => $sender,
                                                                                   helo   => $domain,
                                                                                   local  => $localpolicy,
                                                                                   default_explanation => "explanation",
                                                                                  )->result; };
      $header_comment =~ s/^\S+: //; # strip the reporting hostname prefix

      my $ok = (! $expected_smtp_comment
                ?  ok($result, $expected_result)
                : (ok($result, $expected_result) &&
                   ok($smtp_comment, $expected_smtp_comment) &&
                   ok($header_comment, $expected_header_comment)));

      $actual_smtp_comment = $smtp_comment;
      $actual_result = $result;
      $actual_header_comment = $header_comment;
      
      if (not $ok) {
        Mail::SPF::Query->clear_cache;
        my $result = eval { scalar(new Mail::SPF::Query (ipv4   => $ipv4,
                                                         sender => $sender,
                                                         helo   => $domain,
                                                         debug  => 1,
                                                         local  => $localpolicy,
                                                        )->result) };
        if ($@) {
          print "  trapped error: $@\n";
          next;
        }
      }
  }
  if ($opts{d}) {
      $num = join(",", $testnum .. $testnum + $testcnt - 1);
      $testnum += $testcnt;
      print TEST join("\t", $num, $sender . ($localpolicy ? ":$localpolicy": ""), $ipv4, $actual_result, $actual_smtp_comment, $actual_header_comment),
            "\n";
  }
}

