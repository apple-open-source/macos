# $Id: 02parse.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $

##
# this test checks the parsing capabilities of XML::LibXML
# it relies on the success of t/01basic.t

use Test;
use IO::File;

BEGIN { use XML::LibXML;
    if ( XML::LibXML::LIBXML_VERSION >= 20600 ) {
        plan tests => 478; 
    }
    else {
        plan tests => 470;
        print "# skip NS cleaning tests\n";
    }
};

use XML::LibXML::Common qw(:libxml);
use XML::LibXML::SAX;
use XML::LibXML::SAX::Builder;

use constant XML_DECL => "<?xml version=\"1.0\"?>\n";

##
# test values
my @goodWFStrings = (
'<foobar/>',
'<foobar></foobar>',
XML_DECL . "<foobar></foobar>",
'<?xml version="1.0" encoding="UTF-8"?>'."\n<foobar></foobar>",
'<?xml version="1.0" encoding="ISO-8859-1"?>'."\n<foobar></foobar>",
XML_DECL. "<foobar> </foobar>\n",
XML_DECL. '<foobar><foo/></foobar> ',
XML_DECL. '<foobar> <foo/> </foobar> ',
XML_DECL. '<foobar><![CDATA[<>&"\']]></foobar>',
XML_DECL. '<foobar>&lt;&gt;&amp;&quot;&apos;</foobar>',
XML_DECL. '<foobar>&#x20;&#160;</foobar>',
XML_DECL. '<!--comment--><foobar>foo</foobar>',
XML_DECL. '<foobar>foo</foobar><!--comment-->',
XML_DECL. '<foobar>foo<!----></foobar>',
XML_DECL. '<foobar foo="bar"/>',
XML_DECL. '<foobar foo="\'bar>"/>',
#XML_DECL. '<bar:foobar foo="bar"><bar:foo/></bar:foobar>',
#'<bar:foobar/>'
                    );

my @goodWFNSStrings = (
XML_DECL. '<foobar xmlns:bar="xml://foo" bar:foo="bar"/>'."\n",
XML_DECL. '<foobar xmlns="xml://foo" foo="bar"><foo/></foobar>'."\n",
XML_DECL. '<bar:foobar xmlns:bar="xml://foo" foo="bar"><foo/></bar:foobar>'."\n",
XML_DECL. '<bar:foobar xmlns:bar="xml://foo" foo="bar"><bar:foo/></bar:foobar>'."\n",
XML_DECL. '<bar:foobar xmlns:bar="xml://foo" bar:foo="bar"><bar:foo/></bar:foobar>'."\n",
                      );

my @goodWFDTDStrings = (
XML_DECL. '<!DOCTYPE foobar ['."\n".'<!ENTITY foo " test ">'."\n".']>'."\n".'<foobar>&foo;</foobar>',
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar">]><foobar>&foo;</foobar>',
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar">]><foobar>&foo;&gt;</foobar>',
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar=&quot;foo&quot;">]><foobar>&foo;&gt;</foobar>',
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar">]><foobar>&foo;&gt;</foobar>',
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar">]><foobar foo="&foo;"/>',
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar">]><foobar foo="&gt;&foo;"/>',
                       );

my @badWFStrings = (
"",                                        # totally empty document
XML_DECL,                                  # only XML Declaration
"<!--ouch-->",                             # comment only is like an empty document 
'<!DOCTYPE ouch [<!ENTITY foo "bar">]>',   # no good either ...
"<ouch>",                                  # single tag (tag mismatch)
"<ouch/>foo",                              # trailing junk
"foo<ouch/>",                              # leading junk
"<ouch foo=bar/>",                         # bad attribute
'<ouch foo="bar/>',                        # bad attribute
"<ouch>&</ouch>",                          # bad char
"<ouch>&#0x20;</ouch>",                    # bad char
"<foobär/>",                               # bad encoding
"<ouch>&foo;</ouch>",                      # undefind entity
"<ouch>&gt</ouch>",                        # unterminated entity
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar">]><foobar &foo;="ouch"/>',          # bad placed entity
XML_DECL. '<!DOCTYPE foobar [<!ENTITY foo "bar=&quot;foo&quot;">]><foobar &foo;/>', # even worse 
"<ouch><!---></ouch>",                     # bad comment
'<ouch><!-----></ouch>',                   # bad either... (is this conform with the spec????)
                    );

    my %goodPushWF = (
single1 => ['<foobar/>'],
single2 => ['<foobar>','</foobar>'],
single3 => [ XML_DECL, "<foobar>", "</foobar>" ],
single4 => ["<foo", "bar/>"],
single5 => ["<", "foo","bar", "/>"],
single6 => ['<?xml version="1.0" encoding="UTF-8"?>',"\n<foobar/>"],
single7 => ['<?xml',' version="1.0" ','encoding="UTF-8"?>',"\n<foobar/>"],
single8 => ['<foobar', ' foo=', '"bar"', '/>'],
single9 => ['<?xml',' versio','n="1.0" ','encodi','ng="U','TF8"?>',"\n<foobar/>"],
multiple1 => [ '<foobar>','<foo/>','</foobar> ', ],
multiple2 => [ '<foobar','><fo','o','/><','/foobar> ', ],
multiple3 => [ '<foobar>','<![CDATA[<>&"\']]>','</foobar>'],
multiple4 => [ '<foobar>','<![CDATA[', '<>&', ']]>', '</foobar>' ],
multiple5 => [ '<foobar>','<!','[CDA','TA[', '<>&', ']]>', '</foobar>' ],
multiple6 => ['<foobar>','&lt;&gt;&amp;&quot;&apos;','</foobar>'],
multiple6 => ['<foobar>','&lt',';&','gt;&a','mp;','&quot;&ap','os;','</foobar>'],
multiple7 => [ '<foobar>', '&#x20;&#160;','</foobar>' ],
multiple8 => [ '<foobar>', '&#x','20;&#1','60;','</foobar>' ],
multiple9 => [ '<foobar>','moo','moo','</foobar> ', ],
multiple10 => [ '<foobar>','moo','</foobar> ', ],
comment1  => [ '<!--comment-->','<foobar/>' ],
comment2  => [ '<foobar/>','<!--comment-->' ],
comment3  => [ '<!--','comment','-->','<foobar/>' ],
comment4  => [ '<!--','-->','<foobar/>' ],
comment5  => [ '<foobar>fo','o<!---','-><','/foobar>' ],
attr1     => [ '<foobar',' foo="bar"/>'],
attr2     => [ '<foobar',' foo','="','bar','"/>'],
attr3     => [ '<foobar',' fo','o="b','ar"/>'],
#prefix1   => [ '<bar:foobar/>' ],
#prefix2   => [ '<bar',':','foobar/>' ],
#prefix3   => [ '<ba','r:fo','obar/>' ],
ns1       => [ '<foobar xmlns:bar="xml://foo"/>' ],
ns2       => [ '<foobar ','xmlns:bar="xml://foo"','/>' ],
ns3       => [ '<foo','bar x','mlns:b','ar="foo"/>' ],
ns4       => [ '<bar:foobar xmlns:bar="xml://foo"/>' ],
ns5       => [ '<bar:foo','bar xm','lns:bar="fo','o"/>' ],
ns6       => [ '<bar:fooba','r xm','lns:ba','r="foo"','><bar',':foo/','></bar'.':foobar>'],
dtd1      => [XML_DECL, '<!DOCTYPE ','foobar [','<!ENT','ITY foo " test ">',']>','<foobar>&f','oo;</foobar>',],
dtd2      => [XML_DECL, '<!DOCTYPE ','foobar [','<!ENT','ITY foo " test ">',']>','<foobar>&f','oo;&gt;</foobar>',],
                    );

my $goodfile = "example/dromeds.xml";
my $badfile1 = "example/bad.xml";
my $badfile2 = "does_not_exist.xml";

my $parser = XML::LibXML->new();

print "# 1 NON VALIDATING PARSER\n";
print "# 1.1 WELL FORMED STRING PARSING\n";
print "# 1.1.1 DEFAULT VALUES\n";

{
    foreach my $str ( @goodWFStrings,@goodWFNSStrings,@goodWFDTDStrings ) {
        my $doc = $parser->parse_string($str);
        ok($doc);
    }
}

eval { my $fail = $parser->parse_string(undef); };
ok($@);

foreach my $str ( @badWFStrings ) {
    eval { my $fail = $parser->parse_string($str); };
    ok($@);
}


print "# 1.1.2 NO KEEP BLANKS\n";

$parser->keep_blanks(0);

{
    foreach my $str ( @goodWFStrings,@goodWFNSStrings,@goodWFDTDStrings ) {
	my $doc = $parser->parse_string($str);
        ok($doc);
    }
}

eval { my $fail = $parser->parse_string(undef); };
ok($@);

foreach my $str ( @badWFStrings ) {
    eval { my $fail = $parser->parse_string($str); };
    ok($@);
}

$parser->keep_blanks(1);

print "# 1.1.3 EXPAND ENTITIES\n";

$parser->expand_entities(0);

{
    foreach my $str ( @goodWFStrings,@goodWFNSStrings,@goodWFDTDStrings ) {
        my $doc = $parser->parse_string($str);
        ok($doc);
    }
}

eval { my $fail = $parser->parse_string(undef); };
ok($@);

foreach my $str ( @badWFStrings ) {
    eval { my $fail = $parser->parse_string($str); };  
    ok($@);
}

$parser->expand_entities(1);

print "# 1.1.4 PEDANTIC\n";

$parser->pedantic_parser(1);

{
    foreach my $str ( @goodWFStrings,@goodWFNSStrings,@goodWFDTDStrings ) {
        my $doc = $parser->parse_string($str);
        ok($doc);
    }
}

eval { my $fail = $parser->parse_string(undef); };
ok($@);

foreach my $str ( @badWFStrings ) {
    eval { my $fail = $parser->parse_string($str); };  
    ok($@);
}

$parser->pedantic_parser(0);

print "# 1.2 PARSE A FILE\n";

{
    my $doc = $parser->parse_file($goodfile);
    ok($doc);
}
 
eval {my $fail = $parser->parse_file($badfile1);};
ok($@);

eval { $parser->parse_file($badfile2); };
ok($@);

{
    my $str = "<a>    <b/> </a>";
    my $tstr= "<a><b/></a>";
    $parser->keep_blanks(0);
    my $docA = $parser->parse_string($str);
    my $docB = $parser->parse_file("example/test3.xml");
    $XML::LibXML::skipXMLDeclaration = 1;
    ok( $docA->toString, $tstr );
    ok( $docB->toString, $tstr );
    $XML::LibXML::skipXMLDeclaration = 0;
}

print "# 1.3 PARSE A HANDLE\n";

my $fh = IO::File->new($goodfile);
ok($fh);

my $doc = $parser->parse_fh($fh);
ok($doc);

$fh = IO::File->new($badfile1);
ok($fh);

eval { my $doc = $parser->parse_fh($fh); };
ok($@);

$fh = IO::File->new($badfile2);

eval { my $doc = $parser->parse_fh($fh); };
ok($@);

{
    $parser->expand_entities(1);
    my $doc = $parser->parse_file( "example/dtd.xml" );
    my @cn = $doc->documentElement->childNodes;
    ok( scalar @cn, 1 );
    
    $doc = $parser->parse_file( "example/complex/complex2.xml" );
    @cn = $doc->documentElement->childNodes;
    ok( scalar @cn, 1 );

    $parser->expand_entities(0);
    $doc = $parser->parse_file( "example/dtd.xml" );
    @cn = $doc->documentElement->childNodes;
    ok( scalar @cn, 3 );
}

print "# 1.4 x-include processing\n";

my $goodXInclude = q{
<x>
<xinclude:include 
 xmlns:xinclude="http://www.w3.org/2001/XInclude"
 href="test2.xml"/>
</x>
};


my $badXInclude = q{
<x xmlns:xinclude="http://www.w3.org/2001/XInclude">
<xinclude:include href="bad.xml"/>
</x>
};

{
    $parser->base_uri( "example/" );
    $parser->keep_blanks(0);
    my $doc = $parser->parse_string( $goodXInclude );
    ok($doc);

    my $i;
    eval { $i = $parser->processXIncludes($doc); };
    ok( $i );

    $doc = $parser->parse_string( $badXInclude );
    $i= undef;
    eval { $i = $parser->processXIncludes($doc); };
    ok($@);
    
    # auto expand
    $parser->expand_xinclude(1);
    $doc = $parser->parse_string( $goodXInclude );
    ok($doc);

    $doc = undef;
    eval { $doc = $parser->parse_string( $badXInclude ); };
    ok($@);
    ok(!$doc);

    # some bad stuff 
    eval{ $parser->processXIncludes(undef); };
    ok($@);
    eval{ $parser->processXIncludes("blahblah"); };
    ok($@);
}

print "# 2 PUSH PARSER\n";

{
    my $pparser = XML::LibXML->new();
    print "# 2.1 PARSING WELLFORMED DOCUMENTS\n";
    foreach my $key ( qw(single1 single2 single3 single4 single5 single6 
                         single7 single8 single9 multiple1 multiple2 multiple3
                         multiple4 multiple5 multiple6 multiple7 multiple8 
                         multiple9 multiple10 comment1 comment2 comment3
                         comment4 comment5 attr1 attr2 attr3
			 ns1 ns2 ns3 ns4 ns5 ns6 dtd1 dtd2) ) {
        print "# key is $key\n";
        foreach ( @{$goodPushWF{$key}} ) {
            $pparser->parse_chunk( $_ );
        }

        my $doc;
        eval {$doc = $pparser->parse_chunk("",1); };
        print "# caught an error $@ \n" if $@; 
        print "# document seems to be ok \n" if $doc;
        ok($doc && !$@);      
    }

    my @good_strings = ("<foo>", "bar", "</foo>" );
    my %bad_strings  = ( 
                            predocend1   => ["<A>" ],
                            predocend2   => ["<A>", "B"],
                            predocend3   => ["<A>", "<C>"],
                            predocend4   => ["<A>", "<C/>"],
                            postdocend1  => ["<A/>", "<C/>"],
# use with libxml2 2.4.26:  postdocend2  => ["<A/>", "B"],    # libxml2 < 2.4.26 bug
                            postdocend3  => ["<A/>", "BB"],
                            badcdata     => ["<A> ","<!","[CDATA[B]","</A>"],
                            badending1   => ["<A> ","B","</C>"],
                            badending2   => ["<A> ","</C>","</A>"],
                       );

    my $parser = XML::LibXML->new;
    {
        for ( @good_strings ) {        
            $parser->parse_chunk( $_ );
        }
        my $doc = $parser->parse_chunk("",1);
        ok($doc);
    }

    {
        print "# 2.2 PARSING BROKEN DOCUMENTS\n";
        my $doc;
        foreach my $key ( keys %bad_strings ) {
            print "# $key\n";
            $doc = undef;
            foreach ( @{$bad_strings{$key}} ) {
               eval { $parser->parse_chunk( $_ );};
               if ( $@ ) {
                   # if we won't stop here, we will loose the error :|
                   last;
               }
            }
            if ( $@ ) {
                ok(1);
#                $parser->parse_chunk("",1); # will cause no harm anymore, but is still needed
                next;
            }
           
            eval {    
                $doc = $parser->parse_chunk("",1);
            };
            ok($@);
        }

    }

    {
        print "# 2.3 RECOVERING PUSH PARSER\n";
        $parser->init_push;

        foreach ( "<A>", "B" ) {
            $parser->push( $_);
        }

        my $doc;
        eval {
	       local $SIG{'__WARN__'} = sub { };
	       $doc = $parser->finish_push(1);
	     };
        ok( $doc );
    }
}

print "# 3 SAX PARSER\n";

{
    my $handler = XML::LibXML::SAX::Builder->new();
    my $generator = XML::LibXML::SAX->new( Handler=>$handler );

    my $string  = q{<bar foo="bar">foo</bar>};

    $doc = $generator->parse_string( $string );
    ok( $doc );

    print "# 3.1 GENERAL TESTS \n";
    foreach my $str ( @goodWFStrings ) {
        my $doc = $generator->parse_string( $str );
        ok( $doc );
    }

    print "# CDATA Sections\n";

    $string = q{<foo><![CDATA[&foo<bar]]></foo>};
    $doc = $generator->parse_string( $string );
    my @cn = $doc->documentElement->childNodes();
    ok( scalar @cn );
    ok( $cn[0]->nodeType, XML_CDATA_SECTION_NODE );
    ok( $cn[0]->textContent, "&foo<bar" );
    ok( $cn[0]->toString, '<![CDATA[&foo<bar]]>');

    print "# 3.2 NAMESPACE TESTS\n";

    my $i = 0;
    foreach my $str ( @goodWFNSStrings ) {
        my $doc = $generator->parse_string( $str );
        ok( $doc );

        # skip the nested node tests until there is a xmlNormalizeNs().
        #ok(1),next if $i > 2;

        ok( $doc->toString(), $str );
        $i++
    }

    print "# DATA CONSISTENCE\n";    
    # find out if namespaces are there
    my $string2 = q{<foo xmlns:bar="http://foo.bar">bar<bar:bi/></foo>};

    $doc = $generator->parse_string( $string2 );

    my @attrs = $doc->documentElement->attributes;

    ok( scalar @attrs );
    if ( scalar @attrs ) {
        ok( $attrs[0]->nodeType, XML_NAMESPACE_DECL );
    }
    else {
        ok(0);
    }

    my $root = $doc->documentElement;

    # bad thing: i have to do some NS normalizing.
    # libxml2 will only do some fixing. this will lead to multiple 
    # declarations, if a node with a new namespace is added.

    my $vstring = q{<foo xmlns:bar="http://foo.bar">bar<bar:bi/></foo>};
    # my $vstring = q{<foo xmlns:bar="http://foo.bar">bar<bar:bi xmlns:bar="http://foo.bar"/></foo>};
    ok($root->toString, $vstring );

    print "# 3.3 INTERNAL SUBSETS\n";

    foreach my $str ( @goodWFDTDStrings ) {
        my $doc = $generator->parse_string( $str );
        ok( $doc );
    }

    print "# 3.5 PARSE URI\n"; 
    $doc = $generator->parse_uri( "example/test.xml" );
    ok($doc);

    print "# 3.6 PARSE CHUNK\n";

        
}

print "# 4 SAXY PUSHER\n";

{
    my $handler = XML::LibXML::SAX::Builder->new();
    my $parser = XML::LibXML->new;

    $parser->set_handler( $handler );
    $parser->push( '<foo/>' );
    my $doc = $parser->finish_push;
    ok($doc);

    foreach my $key ( keys %goodPushWF ) {
        foreach ( @{$goodPushWF{$key}} ) {
            $parser->push( $_);
        }

        my $doc;
        eval {$doc = $parser->finish_push; };
        ok($doc);                    
    }
}

print "# 5 PARSE WELL BALANCED CHUNKS\n";
{
    my $MAX_WF_C = 11;
    my $MAX_WB_C = 16;

    my %chunks = ( 
                    wellformed1  => '<A/>',
                    wellformed2  => '<A></A>',
                    wellformed3  => '<A B="C"/>',
                    wellformed4  => '<A>D</A>',
                    wellformed5  => '<A><![CDATA[D]]></A>',
                    wellformed6  => '<A><!--D--></A>',
                    wellformed7  => '<A><K/></A>',
                    wellformed8  => '<A xmlns="xml://E"/>',
                    wellformed9  => '<F:A xmlns:F="xml://G" F:A="B">D</F:A>',
                    wellformed10 => '<!--D-->',      
                    wellformed11  => '<A xmlns:F="xml://E"/>',              
                    wellbalance1 => '<A/><A/>',
                    wellbalance2 => '<A></A><A></A>',
                    wellbalance3 => '<A B="C"/><A B="H"/>',
                    wellbalance4 => '<A>D</A><A>I</A>',
                    wellbalance5 => '<A><K/></A><A><L/></A>',
                    wellbalance6 => '<A><![CDATA[D]]></A><A><![CDATA[I]]></A>',
                    wellbalance7 => '<A><!--D--></A><A><!--I--></A>',
                    wellbalance8 => '<F:A xmlns:F="xml://G" F:A="B">D</F:A><J:A xmlns:J="xml://G" J:A="M">D</J:A>',
                    wellbalance9 => 'D<A/>',                    
                    wellbalance10=> 'D<A/>D',
                    wellbalance11=> 'D<A/><!--D-->',
                    wellbalance12=> 'D<A/><![CDATA[D]]>',
                    wellbalance13=> '<![CDATA[D]]><A/>D',
                    wellbalance14=> '<!--D--><A/>',
                    wellbalance15=> '<![CDATA[D]]>',
                    wellbalance16=> 'D',
                 );

    my @badWBStrings = (
        "",
        "<ouch>",
        "<ouch>bar",
        "bar</ouch>",
        "<ouch/>&foo;", # undefined entity
        "&",            # bad char
        "häh?",         # bad encoding
        "<!--->",       # bad stays bad ;)
        "<!----->",     # bad stays bad ;)
    );


    my $pparser = XML::LibXML->new;
    
    print "# 5.1 DOM CHUNK PARSER\n";

    for ( 1..$MAX_WF_C ) {
        my $frag = $pparser->parse_xml_chunk($chunks{'wellformed'.$_});
        ok($frag);
        if ( $frag->nodeType == XML_DOCUMENT_FRAG_NODE
             && $frag->hasChildNodes ) {
            if ( $frag->firstChild->isSameNode( $frag->lastChild ) ) {
                print "# well formness test $_\n";
                if ( $chunks{'wellformed'.$_} =~ /\<A\>\<\/A\>/ ) {
                    $_--; # because we cannot distinguish between <a/> and <a></a>
                }
  
                ok($frag->toString,$chunks{'wellformed'.$_});                
                next;
            }
        }
        ok(0);
    }

    for ( 1..$MAX_WB_C ) {
        my $frag = $pparser->parse_xml_chunk($chunks{'wellbalance'.$_});
        ok($frag);
        if ( $frag->nodeType == XML_DOCUMENT_FRAG_NODE
             && $frag->hasChildNodes ) {
            if ( $chunks{'wellbalance'.$_} =~ /<A><\/A>/ ) {
                $_--;
            }
            ok($frag->toString,$chunks{'wellbalance'.$_});                
            next;
        }
        ok(0);
    }

    eval { my $fail = $pparser->parse_xml_chunk(undef); };
    ok($@);

    eval { my $fail = $pparser->parse_xml_chunk(""); };
    ok($@);

    foreach my $str ( @badWBStrings ) {
        eval { my $fail = $pparser->parse_xml_chunk($str); };  
        ok($@);
    }

    {
        print "# 5.1.1 Segmenation fault tests\n";

        my $sDoc   = '<C/><D/>';
        my $sChunk = '<A/><B/>';

        my $parser = XML::LibXML->new();
        my $doc = $parser->parse_xml_chunk( $sDoc,  undef );
        my $chk = $parser->parse_xml_chunk( $sChunk,undef );

        my $fc = $doc->firstChild;

        $doc->appendChild( $chk );

        ok( $doc->toString(), '<C/><D/><A/><B/>' );
    }

    {
        print "# 5.1.2 Segmenation fault tests\n";

        my $sDoc   = '<C/><D/>';
        my $sChunk = '<A/><B/>';

        my $parser = XML::LibXML->new();
        my $doc = $parser->parse_xml_chunk( $sDoc,  undef );
        my $chk = $parser->parse_xml_chunk( $sChunk,undef );

        my $fc = $doc->firstChild;

        $doc->insertAfter( $chk, $fc );

        ok( $doc->toString(), '<C/><A/><B/><D/>' );
    }

    {
        print "# 5.1.3 Segmenation fault tests\n";

        my $sDoc   = '<C/><D/>';
        my $sChunk = '<A/><B/>';

        my $parser = XML::LibXML->new();
        my $doc = $parser->parse_xml_chunk( $sDoc,  undef );
        my $chk = $parser->parse_xml_chunk( $sChunk,undef );

        my $fc = $doc->firstChild;

        $doc->insertBefore( $chk, $fc );

        ok( $doc->toString(), '<A/><B/><C/><D/>' );
    }

    ok(1);

    print "# 5.2 SAX CHUNK PARSER\n";

    my $handler = XML::LibXML::SAX::Builder->new();
    my $parser = XML::LibXML->new;
    $parser->set_handler( $handler );
    for ( 1..$MAX_WF_C ) {
        my $frag = $parser->parse_xml_chunk($chunks{'wellformed'.$_});
        ok($frag);
        if ( $frag->nodeType == XML_DOCUMENT_FRAG_NODE
             && $frag->hasChildNodes ) {
            if ( $frag->firstChild->isSameNode( $frag->lastChild ) ) {
                if ( $chunks{'wellformed'.$_} =~ /\<A\>\<\/A\>/ ) {
                    $_--;
                }
                ok($frag->toString,$chunks{'wellformed'.$_});                
                next;
            }
        }
        ok(0);
    }

    for ( 1..$MAX_WB_C ) {
        my $frag = $parser->parse_xml_chunk($chunks{'wellbalance'.$_});
        ok($frag);
        if ( $frag->nodeType == XML_DOCUMENT_FRAG_NODE
             && $frag->hasChildNodes ) {
            if ( $chunks{'wellbalance'.$_} =~ /<A><\/A>/ ) {
                $_--;
            }
            ok($frag->toString,$chunks{'wellbalance'.$_});                
            next;
        }
        ok(0);
    }
}

{
    print "# 6 VALIDATING PARSER\n";

    my %badstrings = (
                    SIMPLE => '<?xml version="1.0"?>'."\n<A/>\n",
                  );
    my $parser = XML::LibXML->new;

    $parser->validation(1);
    my $doc;
    eval { $doc = $parser->parse_string($badstrings{SIMPLE}); };
    ok( $@ );
    my $ql;
}

{
    print "# 7 LINE NUMBERS\n";

    my $goodxml = <<EOXML;
<?xml version="1.0"?>
<foo>
    <bar/>
</foo>
EOXML

    my $badxml = <<EOXML;
<?xml version="1.0"?>
<!DOCTYPE foo [<!ELEMENT foo EMPTY>]>
<bar/>
EOXML

    my $parser = XML::LibXML->new;
    $parser->validation(1);

    eval { $parser->parse_string( $badxml ); };
    # correct line number may or may not be present
    # depending on libxml2 version
    ok( $@ =~ /^:[03]:/ );

    $parser->line_numbers(1);
    eval { $parser->parse_string( $badxml ); };
    ok( $@ =~ /^:3:/ );

    # switch off validation for the following tests
    $parser->validation(0);

    my $doc;
    eval { $doc = $parser->parse_string( $goodxml ); };

    my $root = $doc->documentElement();
    ok( $root->line_number(), 2);

    my @kids = $root->childNodes();
    ok( $kids[1]->line_number(),3 );

    my $newkid = $root->appendChild( $doc->createElement( "bar" ) );
    ok( $newkid->line_number(), 0 );

    $parser->line_numbers(0);
    eval { $doc = $parser->parse_string( $goodxml ); };

    $root = $doc->documentElement();
    ok( $root->line_number(), 0);

    @kids = $root->childNodes();
    ok( $kids[1]->line_number(), 0 );


}

print "# " . XML::LibXML::LIBXML_VERSION . "\n";
if ( XML::LibXML::LIBXML_VERSION >= 20600 )
{
    print "# 8 Clean Namespaces\n";

    my ( $xsDoc1, $xsDoc2 );
    $xsDoc1 = q{<A:B xmlns:A="http://D"><A:C xmlns:A="http://D"></A:C></A:B>};
    $xsDoc2 = q{<A:B xmlns:A="http://D"><A:C xmlns:A="http://E"/></A:B>};

    my $parser = XML::LibXML->new();
    $parser->clean_namespaces(1);

    my $fn1 = "example/xmlns/goodguy.xml";
    my $fn2 = "example/xmlns/badguy.xml";

    ok( $parser->parse_string( $xsDoc1 )->documentElement->toString(),
        q{<A:B xmlns:A="http://D"><A:C/></A:B>} );
    ok( $parser->parse_string( $xsDoc2 )->documentElement->toString(), 
        $xsDoc2 );

    ok( $parser->parse_file( $fn1  )->documentElement->toString(), 
        q{<A:B xmlns:A="http://D"><A:C/></A:B>} );
    ok( $parser->parse_file( $fn2 )->documentElement->toString() , 
        $xsDoc2 );
    
    my $fh1 = IO::File->new($fn1);  
    my $fh2 = IO::File->new($fn2);  

    ok( $parser->parse_fh( $fh1  )->documentElement->toString(), 
        q{<A:B xmlns:A="http://D"><A:C/></A:B>} );
    ok( $parser->parse_fh( $fh2 )->documentElement->toString() , 
        $xsDoc2 );

    my @xaDoc1 = ('<A:B xmlns:A="http://D">','<A:C xmlns:A="h','ttp://D"/>' ,'</A:B>');
    my @xaDoc2 = ('<A:B xmlns:A="http://D">','<A:C xmlns:A="h','ttp://E"/>' , '</A:B>');

    my $doc;

    foreach ( @xaDoc1 ) {
        $parser->parse_chunk( $_ );
    }
    $doc = $parser->parse_chunk( "", 1 );
    ok( $doc->documentElement->toString(), 
        q{<A:B xmlns:A="http://D"><A:C/></A:B>} );


    foreach ( @xaDoc2 ) {
        $parser->parse_chunk( $_ );
    }
    $doc = $parser->parse_chunk( "", 1 );
    ok( $doc->documentElement->toString() , 
        $xsDoc2 );
}


##
# test if external subsets are loaded correctly

{
        my $xmldoc = <<EOXML;
<!DOCTYPE X SYSTEM "example/ext_ent.dtd">
<X>&foo;</X>
EOXML
        my $parser = XML::LibXML->new();
        
        $parser->load_ext_dtd(1);

        # first time it should work
        my $doc    = $parser->parse_string( $xmldoc );
        ok( $doc->documentElement()->string_value(), " test " );

        # second time it must not fail.        
        my $doc2   = $parser->parse_string( $xmldoc );
        ok( $doc2->documentElement()->string_value(), " test " );
}

##
# Test ticket #7668 xinclude breaks entity expansion 
# [CG] removed again, since #7668 claims the spec is incorrect

##
# Test ticket #7913
{
        my $xmldoc = <<EOXML;
<!DOCTYPE X SYSTEM "example/ext_ent.dtd">
<X>&foo;</X>
EOXML
        my $parser = XML::LibXML->new();
        
        $parser->load_ext_dtd(1);

        # first time it should work
        my $doc    = $parser->parse_string( $xmldoc );
        ok( $doc->documentElement()->string_value(), " test " );

        # lets see if load_ext_dtd(0) works
        $parser->load_ext_dtd(0);
        my $doc2;
        eval {
           $doc2    = $parser->parse_string( $xmldoc );
        };
        ok($@);

        $parser->validation(1);

        $parser->load_ext_dtd(0);
        my $doc3;
        eval {
           $doc3 = $parser->parse_file( "example/article_external_bad.xml" );
        };
        
        ok( $doc3 );

        $parser->load_ext_dtd(1);
        eval {
           $doc3 = $parser->parse_file( "example/article_external_bad.xml" );
        };
        
        ok( $@);
}


sub tsub {
    my $doc = shift;

    my $th = {};
    $th->{d} = XML::LibXML::Document->createDocument;
    my $e1  = $th->{d}->createElementNS("x","X:foo");

    $th->{d}->setDocumentElement( $e1 );
    my $e2 = $th->{d}->createElementNS( "x","X:bar" );

    $e1->appendChild( $e2 );

    $e2->appendChild( $th->{d}->importNode( $doc->documentElement() ) );

    return $th->{d};
}

sub tsub2 {
    my ($doc,$query)=($_[0],@{$_[1]});
#    return [ $doc->findnodes($query) ];
    return [ $doc->findnodes(encodeToUTF8('iso-8859-1',$query)) ];
}
