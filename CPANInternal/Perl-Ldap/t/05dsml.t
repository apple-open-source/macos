#!perl

BEGIN {
  require "t/common.pl";
}

unless (eval { require XML::SAX::Base; 1}) {
  print "1..0 # XML::SAX::Base not installed\n";
  exit;
}

unless (eval { require XML::SAX::Writer; 1}) {
  print "1..0 # XML::SAX::Writer not installed\n";
  exit;
}

require Net::LDAP::LDIF;
require Net::LDAP::DSML;

print "1..1\n";

my $infile   = "data/00-in.ldif";
my $outfile1 = "$TEMPDIR/05-out1.dsml";
my $cmpfile1 = "data/05-cmp.dsml";

my $ldif = Net::LDAP::LDIF->new($infile,"r");

@entry = $ldif->read;

open(FH,">$outfile1");

my $dsml = Net::LDAP::DSML->new(output => \*FH,pretty_print => 1);

$dsml->write_entry($_) for @entry;

$dsml->end_dsml;
close(FH);

ok(!compare($cmpfile1,$outfile1), $cmpfile1);
