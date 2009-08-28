use Test;
use constant PLAN => 26;
use constant TIMES_THROUGH => $ENV{MEMORY_TIMES} || 100_000;
BEGIN { 
    plan tests => PLAN;
    if ($^O ne 'linux' ) {
        skip "linux platform only\n" for 1..PLAN;
    } elsif (not $ENV{MEMORY_TEST}) {
        skip "developers only (set MEMORY_TEST=1 to run these tests)\n" for 1..PLAN;
    }   
}
use XML::LibXML;
use XML::LibXML::SAX::Builder;
{
    if ($^O eq 'linux' && $ENV{MEMORY_TEST}) {

#        require Devel::Peek;
        my $peek = 0;
    
        ok(1);

        print("# BASELINE\n");
        check_mem(1);

        print("# MAKE DOC IN SUB\n");
        {
            my $doc = make_doc();
            ok($doc);
            ok($doc->toString);
        }
        check_mem();
        print("# MAKE DOC IN SUB II\n");
        # same test as the first one. if this still leaks, it's
        # our problem, otherwise it's perl :/
        {
            my $doc = make_doc();
            ok($doc);

            ok($doc->toString);
        }
        check_mem();

        {
            my $elem = XML::LibXML::Element->new("foo");
            my $elem2= XML::LibXML::Element->new("bar");
            $elem->appendChild($elem2);
            ok( $elem->toString );
        }
        check_mem();

        print("# SET DOCUMENT ELEMENT\n");
        {
            my $doc2 = XML::LibXML::Document->new();
            make_doc_elem( $doc2 );
            ok( $doc2 );
            ok( $doc2->documentElement );
        }
        check_mem();

        # multiple parsers:
        print("# MULTIPLE PARSERS\n");
	XML::LibXML->new(); # first parser
        check_mem(1);
	
        for (1..TIMES_THROUGH) {
            my $parser = XML::LibXML->new();
        }
        ok(1);

        check_mem();
        # multiple parses
        print("# MULTIPLE PARSES\n");
        for (1..TIMES_THROUGH) {
            my $parser = XML::LibXML->new();
            my $dom = $parser->parse_string("<sometag>foo</sometag>");
        }
        ok(1);

        check_mem();

        # multiple failing parses
        print("# MULTIPLE FAILURES\n");
        for (1..TIMES_THROUGH) {
            # warn("$_\n") unless $_ % 100;
            my $parser = XML::LibXML->new();
            eval {
                my $dom = $parser->parse_string("<sometag>foo</somtag>"); # Thats meant to be an error, btw!
            };
        }
        ok(1);
    
        check_mem();

        # building custom docs
        print("# CUSTOM DOCS\n");
        my $doc = XML::LibXML::Document->new();
        for (1..TIMES_THROUGH)        {
            my $elem = $doc->createElement('x');
            
            if($peek) {
                warn("Doc before elem\n");
                # Devel::Peek::Dump($doc);
                warn("Elem alone\n");
                # Devel::Peek::Dump($elem);
            }
            
            $doc->setDocumentElement($elem);
            
            if ($peek) {
                warn("Elem after attaching\n");
                # Devel::Peek::Dump($elem);
                warn("Doc after elem\n");
                # Devel::Peek::Dump($doc);
            }
        }
        if ($peek) {
            warn("Doc should be freed\n");
            # Devel::Peek::Dump($doc);
        }
        ok(1);
        check_mem();

        {
            my $doc = XML::LibXML->createDocument;
            for (1..TIMES_THROUGH)        {
                make_doc2( $doc );
            }
        }
        ok(1);
        check_mem();

        print("# DTD string parsing\n");

        my $dtdstr;
        {
            local $/; local *DTD;
            open(DTD, 'example/test.dtd') || die $!;
            $dtdstr = <DTD>;
            $dtdstr =~ s/\r//g;
            $dtdstr =~ s/[\r\n]*$//;
            close DTD;
        }

        ok($dtdstr);

        for ( 1..TIMES_THROUGH ) {
            my $dtd = XML::LibXML::Dtd->parse_string($dtdstr);
        }
        ok(1);
        check_mem();

        print( "# DTD URI parsing \n");
        # parse a DTD from a SYSTEM ID
        for ( 1..TIMES_THROUGH ) {
            my $dtd = XML::LibXML::Dtd->new('ignore', 'example/test.dtd');
        }
        ok(1);
        check_mem();

        print("# Document validation\n");
        {
            print "# is_valid()\n";
            my $dtd = XML::LibXML::Dtd->parse_string($dtdstr);
            my $xml;
            eval {
                local $SIG{'__WARN__'} = sub { };
                $xml = XML::LibXML->new->parse_file('example/article_bad.xml');
            };
            for ( 1..TIMES_THROUGH ) {
                my $good;
                eval {
                    local $SIG{'__WARN__'} = sub { };
                    $good = $xml->is_valid($dtd);
                };
            }
            ok(1);
            check_mem();
        
            print "# validate() \n";
            for ( 1..TIMES_THROUGH ) {
                eval {
                    local $SIG{'__WARN__'} = sub { };
                    $xml->validate($dtd);
                };
            }
            ok(1);
            check_mem();
                
        }

        print "# FIND NODES \n";
        my $xml=<<'dromeds.xml';
<?xml version="1.0" encoding="UTF-8"?>
<dromedaries>
    <species name="Camel">
      <humps>1 or 2</humps>
      <disposition>Cranky</disposition>
    </species>                         
    <species name="Llama">
      <humps>1 (sort of)</humps>
      <disposition>Aloof</disposition>
    </species>                        
    <species name="Alpaca">
      <humps>(see Llama)</humps>
      <disposition>Friendly</disposition>
    </species>                           
</dromedaries>
dromeds.xml

        {
            # my $str = "<foo><bar><foo/></bar></foo>";
            my $str = $xml;
            my $doc = XML::LibXML->new->parse_string( $str );
            for ( 1..TIMES_THROUGH ) {
                 processMessage($xml, '/dromedaries/species' );
#                my @nodes = $doc->findnodes("/foo/bar/foo");
            }
            ok(1);
            check_mem();

        }

        {
            my $str = "<foo><bar><foo/></bar></foo>";
            my $doc = XML::LibXML->new->parse_string( $str );
            for ( 1..TIMES_THROUGH ) {
                my $nodes = $doc->find("/foo/bar/foo");
            }
            ok(1);
            check_mem();

        }

#        {
#            print "# ENCODING TESTS \n";
#            my $string = "test ä ø is a test string to test iso encoding";
#            my $encstr = encodeToUTF8( "iso-8859-1" , $string );
#            for ( 1..TIMES_THROUGH ) {
#                my $str = encodeToUTF8( "iso-8859-1" , $string );
#            }
#            ok(1);
#            check_mem();

#            for ( 1..TIMES_THROUGH ) {
#                my $str = encodeToUTF8( "iso-8859-2" , "abc" );
#            }
#            ok(1);
#            check_mem();
#    
#            for ( 1..TIMES_THROUGH ) {
#                my $str = decodeFromUTF8( "iso-8859-1" , $encstr );
#            }
#            ok(1);
#            check_mem();
#        }
        {
            print "# NAMESPACE TESTS \n";

            my $string = '<foo:bar xmlns:foo="bar"><foo:a/><foo:b/></foo:bar>';

            my $doc = XML::LibXML->new()->parse_string( $string );

            for (1..TIMES_THROUGH) {
                my @ns = $doc->documentElement()->getNamespaces();
                # warn "ns : " . $_->localname . "=>" . $_->href foreach @ns;
                my $prefix = $_->localname foreach @ns;
                my $name = $doc->documentElement->nodeName;
            }  
            check_mem();
            ok(1);
        }   

        {
            print "# SAX PARSER\n";

        my %xmlStrings = (
            "SIMPLE"      => "<xml1><xml2><xml3></xml3></xml2></xml1>",
            "SIMPLE TEXT" => "<xml1> <xml2>some text some text some text </xml2> </xml1>",
            "SIMPLE COMMENT" => "<xml1> <xml2> <!-- some text --> <!-- some text --> <!--some text--> </xml2> </xml1>",
            "SIMPLE CDATA" => "<xml1> <xml2><![CDATA[some text some text some text]]></xml2> </xml1>",
            "SIMPLE ATTRIBUTE" => '<xml1  attr0="value0"> <xml2 attr1="value1"></xml2> </xml1>',
            "NAMESPACES SIMPLE" => '<xm:xml1 xmlns:xm="foo"><xm:xml2/></xm:xml1>',
            "NAMESPACES ATTRIBUTE" => '<xm:xml1 xmlns:xm="foo"><xm:xml2 xm:foo="bar"/></xm:xml1>',
        );

            my $handler = sax_null->new;
            my $parser  = XML::LibXML->new;
            $parser->set_handler( $handler );

            check_mem();
       
            foreach my $key ( keys %xmlStrings )  {
                print "# $key \n";
                for (1..TIMES_THROUGH) {
                    my $doc = $parser->parse_string( $xmlStrings{$key} );
                }

                check_mem();
            }
            ok(1);
        }

        {
            print "# PUSH PARSER\n";

        my %xmlStrings = (
            "SIMPLE"      => ["<xml1>","<xml2><xml3></xml3></xml2>","</xml1>"],
            "SIMPLE TEXT" => ["<xml1> ","<xml2>some text some text some text"," </xml2> </xml1>"],
            "SIMPLE COMMENT" => ["<xml1","> <xml2> <!","-- some text --> <!-- some text --> <!--some text-","-> </xml2> </xml1>"],
            "SIMPLE CDATA" => ["<xml1> ","<xml2><!","[CDATA[some text some text some text]","]></xml2> </xml1>"],
            "SIMPLE ATTRIBUTE" => ['<xml1 ','attr0="value0"> <xml2 attr1="value1"></xml2>',' </xml1>'],
            "NAMESPACES SIMPLE" => ['<xm:xml1 xmlns:x','m="foo"><xm:xml2','/></xm:xml1>'],
            "NAMESPACES ATTRIBUTE" => ['<xm:xml1 xmlns:xm="foo">','<xm:xml2 xm:foo="bar"/></xm',':xml1>'],
        );

            my $handler = sax_null->new;
            my $parser  = XML::LibXML->new;

            check_mem();
       if(0) {
            foreach my $key ( keys %xmlStrings )  {
                print "# $key \n";
                for (1..TIMES_THROUGH) {
                    map { $parser->push( $_ ) } @{$xmlStrings{$key}};
                    my $doc = $parser->finish_push();
                }

                check_mem();
            }
            ok(1);
        }
            my %xmlBadStrings = (
                "SIMPLE"      => ["<xml1>"],
                "SIMPLE2"      => ["<xml1>","</xml2>", "</xml1>"],
                "SIMPLE TEXT" => ["<xml1> ","some text some text some text","</xml2>"],
                "SIMPLE CDATA"=> ["<xml1> ","<!","[CDATA[some text some text some text]","</xml1>"],
                "SIMPLE JUNK" => ["<xml1/> ","junk"],
            );

            print "# BAD PUSHED DATA\n";
            foreach my $key ( "SIMPLE","SIMPLE2", "SIMPLE TEXT","SIMPLE CDATA","SIMPLE JUNK" )  {
                print "# $key \n";
                for (1..TIMES_THROUGH) {
                    eval {map { $parser->push( $_ ) } @{$xmlBadStrings{$key}};};
                    eval {my $doc = $parser->finish_push();};
                }

                check_mem();
            }            
            ok(1);
        }

        {
            print "# SAX PUSH PARSER\n";

            my $handler = sax_null->new;
            my $parser  = XML::LibXML->new;
            $parser->set_handler( $handler );
            check_mem();


        my %xmlStrings = (
            "SIMPLE"      => ["<xml1>","<xml2><xml3></xml3></xml2>","</xml1>"],
            "SIMPLE TEXT" => ["<xml1> ","<xml2>some text some text some text"," </xml2> </xml1>"],
            "SIMPLE COMMENT" => ["<xml1","> <xml2> <!","-- some text --> <!-- some text --> <!--some text-","-> </xml2> </xml1>"],
            "SIMPLE CDATA" => ["<xml1> ","<xml2><!","[CDATA[some text some text some text]","]></xml2> </xml1>"],
            "SIMPLE ATTRIBUTE" => ['<xml1 ','attr0="value0"> <xml2 attr1="value1"></xml2>',' </xml1>'],
            "NAMESPACES SIMPLE" => ['<xm:xml1 xmlns:x','m="foo"><xm:xml2','/></xm:xml1>'],
            "NAMESPACES ATTRIBUTE" => ['<xm:xml1 xmlns:xm="foo">','<xm:xml2 xm:foo="bar"/></xm',':xml1>'],
        );
       
            foreach my $key ( keys %xmlStrings )  {
                print "# $key \n";
                for (1..TIMES_THROUGH) {
                    eval {map { $parser->push( $_ ) } @{$xmlStrings{$key}};};
                    eval {my $doc = $parser->finish_push();};
                }

                check_mem();
            }
            ok(1);

            print "# BAD PUSHED DATA\n";

            my %xmlBadStrings = (
                "SIMPLE "      => ["<xml1>"],
                "SIMPLE2"      => ["<xml1>","</xml2>", "</xml1>"],
                "SIMPLE TEXT"  => ["<xml1> ","some text some text some text","</xml2>"],
                "SIMPLE CDATA" => ["<xml1> ","<!","[CDATA[some text some text some text]","</xml1>"],
                "SIMPLE JUNK"  => ["<xml1/> ","junk"],
            );

            foreach my $key ( keys %xmlBadStrings )  {
                print "# $key \n";
                for (1..TIMES_THROUGH) {
                    eval {map { $parser->push( $_ ) } @{$xmlBadStrings{$key}};};
                    eval {my $doc = $parser->finish_push();};
                }

                check_mem();
            }            
            ok(1);
        }
    }
}

sub processMessage {
      my ($msg, $xpath) = @_;
      my $parser = XML::LibXML->new();
                                      
      my $doc  = $parser->parse_string($msg);
      my $elm  = $doc->getDocumentElement;   
      my $node = $doc->findnodes($xpath);      
      my $text = $node->to_literal->value;
#      undef $doc;   # comment this line to make memory leak much worse
#      undef $parser;
}

sub make_doc {
    # code taken from an AxKit XSP generated page
    my ($r, $cgi) = @_;
    my $document = XML::LibXML::Document->createDocument("1.0", "UTF-8");
    # warn("document: $document\n");
    my ($parent);

    { 
        my $elem = $document->createElement(q(p));
        $document->setDocumentElement($elem); 
        $parent = $elem; 
    }

    $parent->setAttribute("xmlns:" . q(param), q(http://axkit.org/XSP/param));
    
    { 
        my $elem = $document->createElementNS(q(http://axkit.org/XSP/param),q(param:foo),);
        $parent->appendChild($elem);
        $parent = $elem;
    }

    $parent = $parent->parentNode;
    # warn("parent now: $parent\n");
    $parent = $parent->parentNode;
    # warn("parent now: $parent\n");

    return $document
}

sub make_doc2 {
    my $docA = shift;
    my $docB = XML::LibXML::Document->new;
    my $e1   = $docB->createElement( "A" );
    my $e2   = $docB->createElement( "B" );
    $e1->appendChild( $e2 );
    $docA->setDocumentElement( $e1 );
}

sub check_mem {
    my $initialise = shift;
    # Log Memory Usage
    local $^W;
    my %mem;
    if (open(FH, "/proc/self/status")) {
        my $units;
        while (<FH>) {
            if (/^VmSize.*?(\d+)\W*(\w+)$/) {
                $mem{Total} = $1;
                $units = $2;
            }
            if (/^VmRSS:.*?(\d+)/) {
                $mem{Resident} = $1;
            }
        }
        close FH;

        if ($LibXML::TOTALMEM != $mem{Total}) {
            warn("LEAK! : ", $mem{Total} - $LibXML::TOTALMEM, " $units\n") unless $initialise;
            $LibXML::TOTALMEM = $mem{Total};
        }

        print("# Mem Total: $mem{Total} $units, Resident: $mem{Resident} $units\n");
    }
}

# some tests for document fragments
sub make_doc_elem {
    my $doc = shift;
    my $dd = XML::LibXML::Document->new();
    my $node1 = $doc->createElement('test1');
    my $node2 = $doc->createElement('test2');
    $doc->setDocumentElement( $node1 );
}

package sax_null;

# require Devel::Peek;
# use Data::Dumper;

sub new {
    my $class = shift;
    bless {}, $class;
}

sub start_document {
    my $self = shift;
    my $dummy = shift;
}

sub xml_decl {
    my $self = shift;
    my $dummy = shift;
}

sub start_element {
    my $self = shift;
    my $dummy = shift;
    # warn Dumper( $dummy );
}

sub end_element {
    my $self = shift;
    my $dummy = shift;
}

sub start_cdata {
    my $self = shift;
    my $dummy = shift;
}

sub end_cdata {
    my $self = shift;
    my $dummy = shift;
}

sub start_prefix_mapping {
    my $self = shift;
    my $dummy = shift;
}

sub end_prefix_mapping {
    my $self = shift;
    my $dummy = shift;
}

sub characters {
    my $self = shift;
    my $dummy = shift;
}

sub comment {
    my $self = shift;
    my $dummy = shift;
}


sub end_document {
    my $self = shift;
    my $dummy = shift;
}

sub error {
    my $self = shift;
    my $msg  = shift;
    die( $msg );
}

1;
