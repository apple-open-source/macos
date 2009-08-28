use Test;
use Config;
use constant MAX_THREADS => 10;
use constant MAX_LOOP => 50;
use constant PLAN => 14;
BEGIN {
  plan tests => PLAN;
  if( $Config{useithreads} ) {
    if ($ENV{THREAD_TEST}) {
      require threads;;
    } else {
      skip("optional (set THREAD_TEST=1 to run these tests)\n") for (1..PLAN);
      exit;
    }
  } else {
    skip("no ithreads in this Perl\n") for (1..PLAN);
    exit;
  }
}
use XML::LibXML;
ok(1);

my $p = XML::LibXML->new();
ok($p);

# Simple spawn threads with $p in scope
{
for(1..MAX_THREADS)
{
	threads->new(sub {});
}
$_->join for(threads->list);
ok(1);
}

my $xml = <<EOF;
<?xml version="1.0" encoding="utf-8"?>
<root><node><leaf/></node></root>
EOF

# Parse a correct XML document
{
for(1..MAX_THREADS)
{
	threads->new(sub { $p->parse_string($xml) for 1..MAX_LOOP; 1; });
}
$_->join for(threads->list);
ok(1);
}

my $xml_bad = <<EOF;
<?xml version="1.0" encoding="utf-8"?>
<root><node><leaf/></root>
EOF

# Parse a bad XML document
{
for(1..MAX_THREADS)
{
	threads->new(sub { eval { my $x = $p->parse_string($xml_bad)} for(1..MAX_LOOP); 1; });
}
$_->join for(threads->list);
ok(1);
}


my $xml_invalid = <<EOF;
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE root [
<!ELEMENT root EMPTY>
]>
<root><something/></root>
EOF

# Parse an invalid XML document
{
for(1..MAX_THREADS)
{
  threads->new(sub {
		 for (1..MAX_LOOP) {
		   my $x = $p->parse_string($xml_invalid); 
		   die if $x->is_valid;
		   eval { $x->validate };
		   die unless $@;
		 }
               1;
	       });
}
$_->join for(threads->list);
ok(1);
}

my $rngschema = <<EOF;
<?xml version="1.0"?>
<r:grammar xmlns:r="http://relaxng.org/ns/structure/1.0">
  <r:start>
    <r:element name="root">
      <r:attribute name="id"/>
    </r:element>
  </r:start>
</r:grammar>
EOF

# test RNG validation errors are thread safe
{
for(1..MAX_THREADS)
{
  threads->new(
    sub {
      for (1..MAX_LOOP) {
	my $x = $p->parse_string($xml);
	eval { XML::LibXML::RelaxNG->new( string => $rngschema )->validate( $x ) };
	die unless $@;
      }; 1;
    });
}
$_->join for(threads->list);
ok(1);
}

my $xsdschema = <<EOF;
<?xml version="1.0"?>
<xsd:schema xmlns:xsd="http://www.w3.org/2001/XMLSchema">
  <xsd:element name="root">
    <xsd:attribute name="partNum" type="SKU" use="required"/>
  </xsd:element>
</xsd:schema>
EOF

# test Schema validation errors are thread safe
{
for(1..MAX_THREADS)
{
  threads->new(
    sub {
      for (1..MAX_LOOP) {
 	my $x = $p->parse_string($xml);
 	eval { XML::LibXML::Schema->new( string => $xsdschema )->validate( $x ) };
 	die unless $@;
      }; 1;
    });
}
$_->join for(threads->list);
ok(1);
}

my $bigfile = "docs/libxml.dbk";
open my $fh, "<:utf8", $bigfile or die $!;
$xml = join '', <$fh>;
close $fh;
ok($xml);

sub use_dom
{
	my $d = shift;
	my @nodes = $d->getElementsByTagName("title",1);
	for(@nodes)
	{
		my $title = $_->toString;
	}
	die unless $nodes[0]->toString eq '<title>XML::LibXML</title>';
}

{
for(1..MAX_THREADS) {
	threads->new(sub { use_dom($p->parse_string($xml)) for 1..5; 1; });
}
$_->join for(threads->list);
ok(1);
}

{
package MyHandler;

use base XML::SAX::Base;

sub AUTOLOAD
{
}
}

use XML::LibXML::SAX;
$p = XML::LibXML::SAX->new(
	Handler=>MyHandler->new(),
);
ok($p);

{
for(1..MAX_THREADS)
{
	threads->new(sub { $p->parse_string($xml) for 1..10; 1; });
}
$_->join for(threads->list);
ok(1);
}

$p = XML::LibXML->new(
	Handler=>MyHandler->new(),
);
$p->parse_chunk($xml);
$p->parse_chunk("",1);

{
for(1..MAX_THREADS)
{
	threads->new(sub {
$p = XML::LibXML->new();
$p->parse_chunk($xml);
use_dom($p->parse_chunk("",1));
1;
});
}
$_->join for(threads->list);
ok(1);
}

$p = XML::LibXML->new();

{
for(1..MAX_THREADS)
{
	threads->new(sub {
open my $fh, "<$bigfile";
$p->parse_fh($fh);
close $fh;
1;
});
}
$_->join for(threads->list);
ok(1);
}


