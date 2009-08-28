# $Id: 06-update.t 616 2006-10-18 09:15:48Z olaf $

use Test::More tests => 72;
use strict;


BEGIN { use_ok('Net::DNS'); } #1


sub is_empty {
	my ($string) = @_;
	
	return 1 if not $string;
	
	return ($string eq "; no data" || $string eq "; rdlength = 0");
}

#------------------------------------------------------------------------------
# Canned data.
#------------------------------------------------------------------------------

my $zone	= "example.com";
my $name	= "foo.example.com";
my $class	= "HS";
my $class2  = "CH";
my $type	= "A";
my $ttl	    = 43200;
my $rdata	= "10.1.2.3";
my $rr      = undef;

#------------------------------------------------------------------------------
# Packet creation.
#------------------------------------------------------------------------------

my $packet = Net::DNS::Update->new($zone, $class);
my $z = ($packet->zone)[0];

ok($packet,                                'new() returned packet');  #2
is($packet->header->opcode, 'UPDATE',      'header opcode correct');  #3 
is($z->zname,  $zone,                      'zname correct');          #4
is($z->zclass, $class,                     'zclass correct');         #5
is($z->ztype,  'SOA',                      'ztype correct');          #6       

#------------------------------------------------------------------------------
# RRset exists (value-independent).
#------------------------------------------------------------------------------

$rr = yxrrset("$name $class $type");

ok($rr,                                    'yxrrset() returned RR');  #7
is($rr->name,  $name,                      'yxrrset - right name');   #8
is($rr->ttl,   0,                          'yxrrset - right TTL');    #9
is($rr->class, 'ANY',                      'yxrrset - right class');  #10
is($rr->type,  $type,                      'yxrrset - right type');   #11
ok(is_empty($rr->rdatastr),                'yxrrset - data empty');   #12

undef $rr;

#------------------------------------------------------------------------------
# RRset exists (value-dependent).
#------------------------------------------------------------------------------

$rr = yxrrset("$name $class $type $rdata");

ok($rr,                                    'yxrrset() returned RR');  #13
is($rr->name,     $name,                   'yxrrset - right name');   #14
is($rr->ttl,      0,                       'yxrrset - right TTL');    #15
is($rr->class,    $class,                  'yxrrset - right class');  #16
is($rr->type,     $type,                   'yxrrset - right type');   #17
is($rr->rdatastr, $rdata,                  'yxrrset - right data');   #18

undef $rr;

#------------------------------------------------------------------------------
# RRset does not exist.
#------------------------------------------------------------------------------

$rr = nxrrset("$name $class $type");

ok($rr,                                    'nxrrset() returned RR');  #19
is($rr->name,  $name,                      'nxrrset - right name');   #20
is($rr->ttl,   0,                          'nxrrset - right ttl');    #21
is($rr->class, 'NONE',                     'nxrrset - right class');  #22
is($rr->type,  $type,                      'nxrrset - right type');   #23
ok(is_empty($rr->rdatastr),                'nxrrset - data empty');   #24

undef $rr;

#------------------------------------------------------------------------------
# Name is in use.
#------------------------------------------------------------------------------

$rr = yxdomain("$name $class");

ok($rr,                                    'yxdomain() returned RR'); #25
is($rr->name,  $name,                      'yxdomain - right name');  #26
is($rr->ttl,   0,                          'yxdomain - right ttl');   #27
is($rr->class, 'ANY',                      'yxdomain - right class'); #28
is($rr->type,  'ANY',                      'yxdomain - right type');  #29
ok(is_empty($rr->rdatastr),                'yxdomain - data empty');  #30

undef $rr;

#------------------------------------------------------------------------------
# Name is not in use.
#------------------------------------------------------------------------------

$rr = nxdomain("$name $class");

ok($rr,                                    'nxdomain() returned RR'); #31
is($rr->name,  $name,                      'nxdomain - right name');  #32
is($rr->ttl,   0,                          'nxdomain - right ttl');   #33
is($rr->class, 'NONE',                     'nxdomain - right class'); #34
is($rr->type,  'ANY',                      'nxdomain - right type');  #35
ok(is_empty($rr->rdatastr),                'nxdomain - data empty');  #36

undef $rr;

#------------------------------------------------------------------------------
# Name is not in use. (No Class)
#------------------------------------------------------------------------------

$rr = nxdomain("$name");

ok($rr,                                    'nxdomain() returned RR'); #31
is($rr->name,  $name,                      'nxdomain - right name');  #32
is($rr->ttl,   0,                          'nxdomain - right ttl');   #33
is($rr->class, 'NONE',                     'nxdomain - right class'); #34
is($rr->type,  'ANY',                      'nxdomain - right type');  #35
ok(is_empty($rr->rdatastr),                'nxdomain - data empty');  #36

undef $rr;



#------------------------------------------------------------------------------
# Add to an RRset.
#------------------------------------------------------------------------------

$rr = rr_add("$name $ttl $class $type $rdata");

ok($rr,                                    'rr_add() returned RR');   #37
is($rr->name,     $name,                   'rr_add - right name');    #38
is($rr->ttl,      $ttl,                    'rr_add - right ttl');     #39
is($rr->class,    $class,                  'rr_add - right class');   #40
is($rr->type,     $type,                   'rr_add - right type');    #41
is($rr->rdatastr, $rdata,                  'rr_add - right data');    #42

undef $rr;

#------------------------------------------------------------------------------
# Delete an RRset.
#------------------------------------------------------------------------------

$rr = rr_del("$name $class $type");

ok($rr,                                    'rr_del() returned RR');   #43
is($rr->name,  $name,                      'rr_del - right name');    #44
is($rr->ttl,   0,                          'rr_del - right ttl');     #45
is($rr->class, 'ANY',                      'rr_del - right class');   #46
is($rr->type,  $type,                      'rr_del - right type');    #47
ok(is_empty($rr->rdatastr),                'rr_del - data empty');    #48

undef $rr;

#------------------------------------------------------------------------------
# Delete All RRsets From A Name.
#------------------------------------------------------------------------------

$rr = rr_del("$name $class");

ok($rr,                                    'rr_del() returned RR');   #49
is($rr->name,  $name,                      'rr_del - right name');    #50
is($rr->ttl,   0,                          'rr_del - right ttl');     #51
is($rr->class, 'ANY',                      'rr_del - right class');   #52
is($rr->type,  'ANY',                      'rr_del - right type');    #53
ok(is_empty($rr->rdatastr),                'rr_del - data empty');    #54

undef $rr;

#------------------------------------------------------------------------------
# Delete An RR From An RRset.
#------------------------------------------------------------------------------

$rr = rr_del("$name $class $type $rdata");

ok($rr,                                    'rr_del() returned RR');   #55
is($rr->name,     $name,                   'rr_del - right name');    #56
is($rr->ttl,      0,                       'rr_del - right ttl');     #57
is($rr->class,    'NONE',                  'rr_del - right class');   #58
is($rr->type,     $type,                   'rr_del - right type');    #59
is($rr->rdatastr, $rdata,                  'rr_del - right data');    #60

undef $rr;

#------------------------------------------------------------------------------
# Make sure RRs in an update packet have the same class as the zone, unless
# the class is NONE or ANY.
#------------------------------------------------------------------------------

$packet = Net::DNS::Update->new($zone, $class);
ok($packet,                               'packet created');          #61


$packet->push("pre", yxrrset("$name $class $type $rdata"));
$packet->push("pre", yxrrset("$name $class2 $type $rdata"));
$packet->push("pre", yxrrset("$name $class2 $type"));
$packet->push("pre", nxrrset("$name $class2 $type"));

my @pre = $packet->pre;

is(scalar(@pre), 4,                     'pushed inserted correctly'); #62
is($pre[0]->class, $class,              'first class right');         #63
is($pre[1]->class, $class,              'second class right');        #64
is($pre[2]->class, 'ANY',               'third class right');         #65
is($pre[3]->class, 'NONE',              'forth class right');         #66
