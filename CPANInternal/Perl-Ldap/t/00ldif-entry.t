#!perl

BEGIN {
  require "t/common.pl";
}


print "1..11\n";

use Net::LDAP::LDIF;

my $infile   = "data/00-in.ldif";
my $outfile1 = "$TEMPDIR/00-out1.ldif";
my $outfile2 = "$TEMPDIR/00-out2.ldif";
my $cmpfile1 = "data/00-cmp.ldif";
my $cmpfile2 = $infile;

my $ldif = Net::LDAP::LDIF->new($infile,"r");

@entry = $ldif->read;

ok($ldif->version == 1, "version == 1");

Net::LDAP::LDIF->new($outfile1,"w")->write(@entry);
Net::LDAP::LDIF->new($outfile2,"w", version => 1)->write(@entry);

ok(!compare($cmpfile1,$outfile1), $cmpfile1);

ok(!compare($cmpfile2,$outfile2), $cmpfile2);

$e = $entry[0];

$e->changetype('modify');
$e->delete('objectclass');
$e->delete('o',['UM']);
$e->add('counting',[qw(one two three)]);
$e->add('first',[qw(1 2 3)], 'second',[qw(a b c)]);
$e->replace('telephonenumber' => ['911']);

$outfile = "$TEMPDIR/00-out3.ldif";
$cmpfile = "data/00-cmp2.ldif";

$ldif = Net::LDAP::LDIF->new($outfile,"w");
$ldif->write($e);
$ldif->write_cmd($e);
$ldif->done;
ok(!compare($cmpfile,$outfile), $cmpfile);

$e->add('name' => 'Graham Barr');
$e->add('name;en-us' => 'Bob');

print "not " unless 
ok(
  join(":",sort $e->attributes)
    eq
  "associateddomain:counting:description:first:l:lastmodifiedby:lastmodifiedtime:name:name;en-us:o:postaladdress:second:st:streetaddress:telephonenumber",
  "attributes");

print "not " unless 
ok(
  join(":",sort $e->attributes(nooptions => 1))
    eq
  "associateddomain:counting:description:first:l:lastmodifiedby:lastmodifiedtime:name:o:postaladdress:second:st:streetaddress:telephonenumber",
  "attributes - nooptions");

$r = $e->get_value('name', asref => 1);
ok(($r and @$r == 1 and $r->[0] eq 'Graham Barr'), "name eq Graham Barr");

$r = $e->get_value('name;en-us', asref => 1);
ok(($r and @$r == 1 and $r->[0] eq 'Bob'), "name;en-us eq Bob");

$r = $e->get_value('name', alloptions => 1, asref => 1);
ok(($r and  join("*", sort keys %$r) eq "*;en-us"), "name keys");

ok(($r and $r->{''} and @{$r->{''}} == 1 and $r->{''}[0] eq 'Graham Barr'), "name alloptions");

ok(($r and $r->{';en-us'} and @{$r->{';en-us'}} == 1 and $r->{';en-us'}[0] eq 'Bob'), "name alloptions Bob");

