# $Id: 12-compression.t 704 2008-02-06 21:30:59Z olaf $   -*-perl-*-
# build DNS packet which has an endless loop in compression
# check it against XS and PP implementation of dn_expand
# both should return (undef,undef) as a sign that the packet
# is invalid
# 

use Test::More tests => 5;
use strict;
use Net::DNS;

# simple query packet
my $pkt = Net::DNS::Packet->new( 'www.example.com','a' )->data;

# replace 'com' with pointer to 'example', thus causing
# endless loop for compressed string:
# www.example.example.example.example...
my $pos = pack( 'C', index( $pkt,"\007example" ));
$pkt =~s{\003com}{\xc0$pos\001x};

# start at 'www'
my $start_offset = index( $pkt,"\003www" );

# fail in case the implementation is buggy and loops forever
$SIG{ ALRM } = sub { BAIL_OUT( "endless loop?" ) };
alarm(15);


my ($name,$offset);
# XS implementation
SKIP: {
     skip("No dn_expand_xs available",1) if ! $Net::DNS::HAVE_XS; 
     my ($name,$offset) = eval { Net::DNS::Packet::dn_expand( \$pkt,$start_offset ) };
     ok( !defined($name) && !defined($offset), 'XS detected invalid packet' );
 }
$Net::DNS::HAVE_XS = 0;
undef $name; undef $offset;
($name,$offset) = eval { Net::DNS::Packet::dn_expand( \$pkt,$start_offset ) };
ok( !defined($name) && !defined($offset), 'PP detected invalid packet' );


# rt.cpan.org 27391
my $packet = Net::DNS::Packet->new("bad..example.com");
my $corrupt = $packet->data;
my $result = Net::DNS::Packet->new(\$corrupt);

is (($result->question)[0]->qtype(),"A","Type correct");
is (($result->question)[0]->qclass(),"IN","Type correct");

#rt.cpan.org #26957
undef $packet;
$packet = Net::DNS::Packet->new();
my $input=     "123456789112345678921234567893123456789412345678951234567896123456789.example.com";
# We truncate labels:
my $compressed="123456789112345678921234567893123456789412345678951234567896123.example.com";
my $compname=$packet->dn_comp($input,0);

is((Net::DNS::Packet::dn_expand(\$compname,0))[0],$compressed,"Long labels chopped")
