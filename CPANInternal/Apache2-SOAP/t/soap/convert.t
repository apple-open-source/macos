#!/usr/bin/perl
use strict;
use warnings;
use Apache::Test;
use Apache::TestUtil qw(t_cmp);
use Apache::TestRequest qw(GET);
use FindBin;
use lib "$FindBin::Bin/../lib";
use TestSOAP qw(make_soap);

my $config   = Apache::Test::config();
my $hostport = Apache::TestRequest::hostport($config) || '';

plan tests => 11;

my $soap_uri = "http://$hostport/TestSOAP/convert";
my $soap_proxy = "http://$hostport/TestSOAP__convert";
my $soap = make_soap($soap_uri, $soap_proxy) or die "SOAP::Lite setup failed";
ok t_cmp(defined $soap, 1, "$soap");
my $string = 'AbCdEfG';

my $out = $soap->convert(mode => 'lc', string => $string);
eval{$out->fault};
ok t_cmp($@, "");
ok t_cmp($out->fault, undef);
my $results = $out->result();
ok t_cmp(defined $results, 1);
ok t_cmp(ref($results), 'HASH');
ok t_cmp($results->{results}, lc($string));

$out = $soap->convert(mode => 'uc', string => $string);
eval{$out->fault};
ok t_cmp($@, "");
ok t_cmp($out->fault, undef);
$results = $out->result();
ok t_cmp(defined $results, 1);
ok t_cmp(ref($results), 'HASH');
ok t_cmp($results->{results}, uc($string));

