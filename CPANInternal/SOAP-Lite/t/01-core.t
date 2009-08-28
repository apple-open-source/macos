#!/bin/env perl

BEGIN {
  unless(grep /blib/, @INC) {
    chdir 't' if -d 't';
    unshift @INC, '../lib' if -d '../lib';
  }
}

use strict;
use Test;

BEGIN { plan tests => 35; }

use SOAP::Lite;

my($a, $s, $r, $serialized, $deserialized);

{ # check 'use ...'
  print "'use SOAP::Lite ...' test(s)...\n";
  eval 'use SOAP::Lite 99.99'; # hm, definitely should fail
  ok($@ =~ /99\.99 required/);
}

# These tests are for backwards compatibility
{ # check use of use_prefix and uri together
  # test 2 - turn OFF default namespace
  $SIG{__WARN__} = sub { ; }; # turn off deprecation warnings
  $serialized = SOAP::Serializer->use_prefix(1)->uri("urn:Test")->method(
    'testMethod', SOAP::Data->name(test => 123)
  );
  ok($serialized =~ m!<soap:Body><namesp(\d):testMethod><test xsi:type="xsd:int">123</test></namesp\1:testMethod></soap:Body>!);

  # test 3 - turn ON default namespace
  $serialized = SOAP::Serializer->use_prefix(0)->uri("urn:Test")->method(
    'testMethod', SOAP::Data->name(test => 123)
  );
  ok($serialized =~ m!<soap:Envelope(?: xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"| soap:encodingStyle="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"){5}><soap:Body><testMethod xmlns="urn:Test"><test xsi:type="xsd:int">123</test></testMethod></soap:Body></soap:Envelope>!);

}

{ # check use of default_ns, ns, and use_prefix
  # test 4
  $serialized = SOAP::Serializer->ns("urn:Test")->method(
    'testMethod', SOAP::Data->name(test => 123)
  );
  ok($serialized =~ m!<namesp(\d):testMethod><test xsi:type="xsd:int">123</test></namesp\1:testMethod>!);

  # test 5
  $serialized = SOAP::Serializer->ns("urn:Test","testns")->method(
    'testMethod', SOAP::Data->name(test => 123)
  );
  ok($serialized =~ m!<testns:testMethod><test xsi:type="xsd:int">123</test></testns:testMethod>!);

  # test 6
  $serialized = SOAP::Serializer->default_ns("urn:Test")->method(
    'testMethod', SOAP::Data->name(test => 123)
  );
  ok($serialized =~ m!<soap:Body><testMethod xmlns="urn:Test"><test xsi:type="xsd:int">123</test></testMethod></soap:Body>!);
}  

{ # check serialization
  print "Arrays, structs, refs serialization test(s)...\n";
  $serialized = SOAP::Serializer->serialize(
    SOAP::Data->name(test => \SOAP::Data->value(1, [1,2], {a=>3}, \4))
  );
  ok($serialized =~ m!<test(?: xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){4}><c-gensym(\d+) xsi:type="xsd:int">1</c-gensym\1><soapenc:Array(?: xsi:type="soapenc:Array"| soapenc:arrayType="xsd:int\[2\]"){2}><item xsi:type="xsd:int">1</item><item xsi:type="xsd:int">2</item></soapenc:Array><c-gensym(\d+)><a xsi:type="xsd:int">3</a></c-gensym\2><c-gensym(\d+)><c-gensym(\d+) xsi:type="xsd:int">4</c-gensym\4></c-gensym\3></test>!);

}  

{ # check simple circular references
  print "Simple circular references (\$a=\\\$a) serialization test(s)...\n";

  $a = \$a;
  $serialized = SOAP::Serializer->namespaces({})->serialize($a);

  ok($serialized =~ m!<c-gensym(\d+) id="ref-(\w+)"><c-gensym(\d+) href="#ref-\2" /></c-gensym\1>!);

  $a = SOAP::Deserializer->deserialize($serialized)->root;
  ok(0+$a == 0+(values%$a)[0]);
}

{ # check complex circular references
  print "Complex circlular references serialization test(s)...\n";

  $a = SOAP::Deserializer->deserialize(<<'EOX')->root;
<root xmlns="urn:Foo">
  <a id="id1">
    <x>1</x>
    <next id="id2">
      <x>7</x>
      <next href="#id3" />
    </next>
  </a>
  <item id="id3">
    <x>8</x>
    <next href="#id1" />
  </item>
</root>
EOX

  ok($a->{a}->{next}->{next}->{next}->{next}->{x} == 
     $a->{a}->{next}->{x});

  $a = { a => 1 }; my $b = { b => $a }; $a->{a} = $b;
  $serialized = SOAP::Serializer->autotype(0)->namespaces({})->serialize($a);

  ok($serialized =~ m!<c-gensym(\d+) id="ref-(\w+)"><a id="ref-\w+"><b href="#ref-\2" /></a></c-gensym\1>!);
}

{ # check multirefs
  print "Multireferences serialization test(s)...\n";

  $a = 1; my $b = \$a;

  $serialized = SOAP::Serializer->new(multirefinplace=>1)->serialize(
    SOAP::Data->name(test => \SOAP::Data->value($b, $b))
  );

  ok($serialized =~ m!<test(?: xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){4}><c-gensym(\d+) id="ref-(\w+)"><c-gensym(\d+) xsi:type="xsd:int">1</c-gensym\3></c-gensym\1><c-gensym\d+ href="#ref-\2" /></test>!);

  $serialized = SOAP::Serializer->namespaces({})->serialize(
    SOAP::Data->name(test => \SOAP::Data->value($b, $b))
  );

  ok($serialized =~ m!<c-gensym\d+ href="#ref-(\w+)" /><c-gensym\d+ href="#ref-\1" /><c-gensym(\d+) id="ref-\1"><c-gensym(\d+) xsi:type="xsd:int">1</c-gensym\3></c-gensym\2>!);
}

{ # check base64, XML encoding of elements and attributes 
  print "base64, XML encoding of elements and attributes test(s)...\n";

  $serialized = SOAP::Serializer->serialize(
    SOAP::Data->name(test => \SOAP::Data->value("\0\1\2\3   \4\5\6", "<123>&amp;\015</123>"))
  );

  ok($serialized =~ m!<c-gensym(\d+) xsi:type="xsd:base64Binary">AAECAyAgIAQFBg==</c-gensym\1><c-gensym(\d+) xsi:type="xsd:string">&lt;123&gt;&amp;amp;&#xd;&lt;/123&gt;</c-gensym\2>!);

  $serialized = SOAP::Serializer->namespaces({})->serialize(
    SOAP::Data->name(name=>'value')->attr({attr => '<123>"&amp"</123>'})
  );

  ok($serialized =~ m!^<\?xml version="1.0" encoding="UTF-8"\?><name(?: xsi:type="xsd:string"| attr="&lt;123&gt;&quot;&amp;amp&quot;&lt;/123&gt;"){2}>value</name>$!);
}

{ # check objects and SOAP::Data 
  print "Blessed references and SOAP::Data encoding test(s)...\n";

  $serialized = SOAP::Serializer->serialize(SOAP::Data->uri('some_urn' => bless {a => 1} => 'ObjectType'));

  ok($serialized =~ m!<namesp(\d+):c-gensym(\d+)(:? xsi:type="namesp\d+:ObjectType"| xmlns:namesp\d+="http://namespaces.soaplite.com/perl"| xmlns:namesp\1="some_urn"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){7}><a xsi:type="xsd:int">1</a></namesp\1:c-gensym\2>!);
}

{ # check serialization/deserialization of simple types
  print "Serialization/deserialization of simple types test(s)...\n";

  $a = 'abc234xyz';

  $serialized = SOAP::Serializer->serialize(SOAP::Data->type(hex => $a));

  ok($serialized =~ m!<c-gensym(\d+)(?: xsi:type="xsd:hexBinary"| xmlns:soapenc="http://schemas.xmlsoap.org/soap/encoding/"| xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"| xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"| xmlns:xsd="http://www.w3.org/2001/XMLSchema"){5}>61626332333478797A</c-gensym(\d+)>!);
  ok(SOAP::Deserializer->deserialize($serialized)->root eq $a); 

  $a = <<"EOBASE64";
qwertyuiop[]asdfghjkl;'zxcvbnm,./QWERTYUIOP{}ASDFGHJKL:"ZXCVBNM<>?`1234567890-=\~!@#$%^&*()_+|
EOBASE64

  $serialized = SOAP::Serializer->serialize($a);

  ok(index($serialized, quotemeta(q!qwertyuiop[]asdfghjkl;'zxcvbnm,./QWERTYUIOP{}ASDFGHJKL:"ZXCVBNM&lt;>?`1234567890-=~\!@#0^&amp;*()_+|!)));

  if (UNIVERSAL::isa(SOAP::Deserializer->parser->parser => 'XML::Parser::Lite')) {
    skip(q!Entity decoding is not supported in XML::Parser::Lite! => undef);
  } else {
    ok(SOAP::Deserializer->deserialize($serialized)->root eq $a);
  }

  $a = <<"EOBASE64";

qwertyuiop[]asdfghjkl;'zxcvbnm,./
QWERTYUIOP{}ASDFGHJKL:"ZXCVBNM<>?
\x00

EOBASE64

  $serialized = SOAP::Serializer->serialize($a);

  ok($serialized =~ /base64/);
}

{ # check serialization/deserialization of blessed reference  
  print "Serialization/deserialization of blessed reference test(s)...\n";

  $serialized = SOAP::Serializer->serialize(bless {a => 1} => 'SOAP::Lite');
  $a = SOAP::Deserializer->deserialize($serialized)->root;

  ok(ref $a eq 'SOAP::Lite' && UNIVERSAL::isa($a => 'HASH'));

  $a = SOAP::Deserializer->deserialize(
    SOAP::Serializer->serialize(bless [a => 1] => 'SOAP::Lite')
  )->root;

  ok(ref $a eq 'SOAP::Lite' && UNIVERSAL::isa($a => 'ARRAY'));
}

{ # check serialization/deserialization of undef/empty elements  
  print "Serialization/deserialization of undef/empty elements test(s)...\n";

  { local $^W; # suppress warnings
    $a = undef;
    $serialized = SOAP::Serializer->serialize(
      SOAP::Data->type(negativeInteger => $a)
    );

    ok(! defined SOAP::Deserializer->deserialize($serialized)->root);

    my $type = 'nonstandardtype';
    eval {
      $serialized = SOAP::Serializer->serialize(
        SOAP::Data->type($type => $a)
      );
    };
    ok($@ =~ /for type '$type' is not specified/);

    $serialized = SOAP::Serializer->serialize(
      SOAP::Data->type($type => {})
    );

    ok(ref SOAP::Deserializer->deserialize($serialized)->root eq $type);
  }
}

{
  print "Check for unspecified Transport module test(s)...\n";

  eval { SOAP::Lite->new->abc() };
  ok($@ =~ /A service address has not been specified/);
}

{
  print "Deserialization of CDATA test(s)...\n";

  UNIVERSAL::isa(SOAP::Deserializer->parser->parser => 'XML::Parser::Lite') ?
    skip(q!CDATA decoding is not supported in XML::Parser::Lite! => undef) :
    ok(SOAP::Deserializer->deserialize('<root><![CDATA[<123>]]></root>')->root eq '<123>');
}

{
  print "Test of XML::Parser External Entity vulnerability...\n";
  UNIVERSAL::isa(SOAP::Deserializer->parser->parser => 'XML::Parser::Lite') ?
    skip(q!External entity references are not supported in XML::Parser::Lite! => undef) :
    ok(!eval { SOAP::Deserializer->deserialize('<?xml version="1.0"?><!DOCTYPE foo [ <!ENTITY ll SYSTEM "foo.txt"> ]><root>&ll;</root>')->root } and $@ =~ /^External entity/);
}

{
  print "Test SOAP:: prefix with no +autodispatch option...\n";
  eval { A->SOAP::b };
  ok($@ =~ /^SOAP:: prefix/);
}

{
  # check deserialization of an array of multiple elements
  # nested within a complex type
  print "Deserialization of doc/lit arrays nested in complex types...\n";
  my $input =  '<?xml version="1.0" encoding="utf-8"?><soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"><soap:Body><getFooResponse xmlns="http://example.com/v1"><getFooReturn><id>100</id><complexFoo><arrayFoo>one</arrayFoo><arrayFoo>two</arrayFoo></complexFoo></getFooReturn></getFooResponse></soap:Body></soap:Envelope>';
  my $deserializer = SOAP::Deserializer->new;	
  my $ret = $deserializer->deserialize($input);
  my @arr = @{$ret->result->{'complexFoo'}{'arrayFoo'}};
  ok($#arr == 1);
  ok("one" eq $arr[0]);
  ok("two" eq $arr[1]);
  
  ok(100 == $ret->result->{"id"});
  
  # If only one araryFoo tag is found, it's deserialized as a scalar.
  $input =  '<?xml version="1.0" encoding="utf-8"?><soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/" xmlns:xsd="http://www.w3.org/1999/XMLSchema" xmlns:xsi="http://www.w3.org/1999/XMLSchema-instance"><soap:Body><getFooResponse xmlns="http://example.com/v1"><getFooReturn><id>100</id><complexFoo><arrayFoo>one</arrayFoo></complexFoo></getFooReturn></getFooResponse></soap:Body></soap:Envelope>';
  $ret = $deserializer->deserialize($input);
  ok("one" eq $ret->result->{'complexFoo'}{'arrayFoo'});
}
