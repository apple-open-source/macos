#!/usr/bin/perl -w
use strict;
use warnings;
use Test::More;

use XML::LibXML;

BEGIN{
  if (1000*$] < 5008) {
     plan skip_all => "Reader interface only supported in Perl >= 5.8";
     exit;
  } elsif (XML::LibXML::LIBXML_VERSION() <= 20620) {
     plan skip_all => "Reader not supported for libxml2 <= 2.6.20";
     exit;
  } else {
     plan tests => 93;
  }

  use_ok('XML::LibXML::Reader');
};

my $file = "test/textReader/countries.xml";
{
  my $reader = new XML::LibXML::Reader(location => $file, {expand_entities => 1});
  isa_ok($reader, "XML::LibXML::Reader");
  is($reader->read, 1, "read");
  is($reader->byteConsumed, 488, "byteConsumed");
  is($reader->attributeCount, 0, "attributeCount");
  is($reader->baseURI, $file, "baseURI");
  is($reader->encoding, 'UTF-8', "encoding");
  is($reader->localName, 'countries', "localName");
  is($reader->name, 'countries', "name");
  is($reader->prefix, undef, "prefix");
  is($reader->value, undef, "value");
  is($reader->xmlLang, undef, "xmlLang");
  is($reader->xmlVersion, '1.0', "xmlVersion");
  $reader->read;
  $reader->read;
  $reader->read;		# skipping to country node
  is($reader->name, 'country', "skipping to country");
  is($reader->depth, "1", "depth");
  is($reader->getAttribute("acronym"), "AL", "getAttribute");
  is($reader->getAttributeNo(0), "AL", "getAttributeNo");
  is($reader->getAttributeNs("acronym", undef), "AL", "getAttributeNs");
  is($reader->lineNumber, "20", "lineNumber");
  is($reader->columnNumber, "1", "columnNumber");
  ok($reader->hasAttributes, "hasAttributes");
  ok(! $reader->hasValue, "hasValue");
  ok(! $reader->isDefault, "isDefault");
  ok(! $reader->isEmptyElement, "isEmptyElement");
  ok(! $reader->isNamespaceDecl, "isNamespaceDecl");
  ok(! $reader->isValid, "isValid");
  is($reader->localName, "country", "localName");
  is($reader->lookupNamespace(undef), undef, "lookupNamespace");

  ok($reader->moveToAttribute("acronym"), "moveToAttribute");
  ok($reader->moveToAttributeNo(0), "moveToAttributeNo");
  ok($reader->moveToAttributeNs("acronym", undef), "moveToAttributeNs");

  ok($reader->moveToElement, "moveToElement");
  ok($reader->moveToFirstAttribute, "moveToFirstAttribute");
  ok($reader->moveToNextAttribute, "moveToNextAttribute");
  ok($reader->readAttributeValue, "attributeValue");

  $reader->moveToElement;
  is($reader->name, "country", "name");
  is($reader->namespaceURI, undef, "namespaceURI");

  ok($reader->nextSibling, "nextSibling");
  is($reader->nodeType, XML_READER_TYPE_SIGNIFICANT_WHITESPACE, "nodeType");
  is($reader->prefix, undef, "prefix");

  is($reader->readInnerXml, "", "readInnerXml");
  is($reader->readOuterXml, "\n", "readOuterXml");
  ok($reader->readState, "readState");

  is($reader->getParserProp('expand_entities'), 1, "getParserProp");

  ok($reader->standalone, "standalone");
  is($reader->value, "\n", "value");
  is($reader->xmlLang, undef, "xmlLang");


  ok($reader->close, "close");
}


my $fd;
# FD interface
for my $how (qw(FD IO)) {
#  my $fd;
  open $fd, $file or die "cannot open $file: $!\n";
  my $reader = new XML::LibXML::Reader($how => $fd, URI => $file);
  isa_ok($reader, "XML::LibXML::Reader");
  $reader->read;
  $reader->read;
  is($reader->name, "countries","name in fd");
  $reader->read;
  $reader->read;
  $reader->read;
  close $fd;
}

# scalar interface
{
  open my $fd, $file or die "cannot open $file: $!\n";
  my $doc;
  {
    local $/;
    $doc = <$fd>;
  }
  close $fd;
  my $reader = new XML::LibXML::Reader(string => $doc, URI => $file);
  isa_ok($reader, "XML::LibXML::Reader");
  $reader->read;
  $reader->read;
  is($reader->name, "countries","name in string");
}

# DOM
{
  my $DOM = XML::LibXML->new->parse_file($file);
  my $reader = new XML::LibXML::Reader(DOM => $DOM);
  isa_ok($reader, "XML::LibXML::Reader");
  $reader->read;
  $reader->read;
  is($reader->name, "countries","name in string");
  ok($reader->document,"document");
  ok($reader->document->isSameNode($DOM),"document is DOM");
}

# Expand
{
  my ($node1,$node2, $node3);
  my $xml = <<'EOF';
<root>
  <AA foo="FOO"> text1 <inner/> </AA>
  <DD/><BB bar="BAR">text2<CC> xx </CC>foo<FF/> </BB>x
  <EE baz="BAZ"> xx <PP>preserved</PP> yy <XX>FOO</XX></EE>
  <a/>
  <b/>
  <x:ZZ xmlns:x="foo"/>
  <QQ/>
  <YY/>
</root>
EOF
  {
    my $reader = new XML::LibXML::Reader(string => $xml);
    $reader->preservePattern('//PP');
    $reader->preservePattern('//x:ZZ',{ x => "foo"});

    isa_ok($reader, "XML::LibXML::Reader");
    $reader->nextElement;
    is($reader->name, "root","root node");
    $reader->nextElement;
    $node1 = $reader->copyCurrentNode(1);
    is($node1->nodeName, "AA","deep copy node");
    $reader->next;
    ok($reader->nextElement("DD"),"next named element");
    is($reader->name, "DD","name");
    is($reader->readOuterXml, "<DD/>","readOuterXml");
    ok($reader->read,"read");
    is($reader->name, "BB","name");
    $node2 = $reader->copyCurrentNode(0);
    is($node2->nodeName, "BB","shallow copy node");
    $reader->nextElement;
    is($reader->name, "CC","nextElement");
    $reader->nextSibling;
    is( $reader->nodeType(), XML_READER_TYPE_TEXT, "text node" );
    is( $reader->value,"foo", "text content" );
    $reader->skipSiblings;
    is( $reader->nodeType(), XML_READER_TYPE_END_ELEMENT, "end element type" );
    $reader->nextElement;
    is($reader->name, "EE","name");
    ok($reader->nextSiblingElement("ZZ","foo"),"namespace");
    is($reader->namespaceURI, "foo","namespaceURI");
    $reader->nextElement;
    $node3= $reader->preserveNode;
    is( $reader->readOuterXml(), $node3->toString(),"outer xml");
    ok($node3,"preserve node");
    $reader->finish;
    my $doc = $reader->document;
    ok($doc, "document");
    ok($doc->documentElement, "doc root element");
    is($doc->documentElement->toString,q(<root><EE baz="BAZ"><PP>preserved</PP></EE><x:ZZ xmlns:x="foo"/><QQ/></root>),
       "preserved content");
  }
  ok($node1->hasChildNodes,"copy w/  child nodes");
  ok($node1->toString(),q(<AA foo="FOO"> text1 <inner/> </AA>));
  ok(!defined $node2->firstChild, "copy w/o child nodes");
  ok($node2->toString(),q(<BB bar="BAR"/>));
  ok($node3->toString(),q(<QQ/>));
}

{
  my $bad_xml = <<'EOF';
<root>
  <x>
     foo
  </u>
</root>
EOF
  my $reader = new XML::LibXML::Reader(
    string => $bad_xml,
    URI => "mystring.xml"
   );
  eval { $reader->finish };
  ok((defined $@ and $@ =~ /in mystring.xml at line 2:/), 'catchin error');
}

{
  my $rng = "test/relaxng/demo.rng";
  for my $RNG ($rng, XML::LibXML::RelaxNG->new(location => $rng)) {
    {
      my $reader = new XML::LibXML::Reader(
	location => "test/relaxng/demo.xml",
	RelaxNG => $RNG,
       );
      ok($reader->finish, "validate using ".(ref($RNG) ? 'XML::LibXML::RelaxNG' : 'RelaxNG file'));
    }
    {
      my $reader = new XML::LibXML::Reader(
	location => "test/relaxng/invaliddemo.xml",
	RelaxNG => $RNG,
       );
      eval { $reader->finish };
      ok($@, "catch validation error for ".(ref($RNG) ? 'XML::LibXML::RelaxNG' : 'RelaxNG file'));
    }

  }
}

{
  my $xsd = "test/schema/schema.xsd";
  for my $XSD ($xsd, XML::LibXML::Schema->new(location => $xsd)) {
    {
      my $reader = new XML::LibXML::Reader(
	location => "test/schema/demo.xml",
	Schema => $XSD,
       );
      ok($reader->finish, "validate using ".(ref($XSD) ? 'XML::LibXML::Schema' : 'Schema file'));
    }
    {
      my $reader = new XML::LibXML::Reader(
	location => "test/schema/invaliddemo.xml",
	Schema => $XSD,
       );
      eval { $reader->finish };
      ok($@, "catch validation error for ".(ref($XSD) ? 'XML::LibXML::Schema' : 'Schema file'));
    }

  }
}
