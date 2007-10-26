use Test;
BEGIN { plan tests => 19 }
use XML::LibXML;
ok(1);

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

{
    # parse a DTD from a SYSTEM ID
    my $dtd = XML::LibXML::Dtd->new('ignore', 'example/test.dtd');
    ok($dtd);
    my $newstr = $dtd->toString();
    $newstr =~ s/\r//g;
    $newstr =~ s/^.*?\n//;
    $newstr =~ s/\n^.*\Z//m;
    ok($newstr, $dtdstr);
}

{
    # parse a DTD from a string
    my $dtd = XML::LibXML::Dtd->parse_string($dtdstr);
    ok($dtd);
}

{
# parse a DTD with a different encoding
# my $dtd = XML::LibXML::Dtd->parse_string($dtdstr, "ISO-8859-1");
# ok($dtd);
1;
}

{
    # validate with the DTD
    my $dtd = XML::LibXML::Dtd->parse_string($dtdstr);
    ok($dtd);
    my $xml = XML::LibXML->new->parse_file('example/article.xml');
    ok($xml);
    ok($xml->is_valid($dtd));
    eval { $xml->validate($dtd) }; # throws exception
    ok( !$@ );
}

{
    # validate a bad document
    my $dtd = XML::LibXML::Dtd->parse_string($dtdstr);
    ok($dtd);
    my $xml = XML::LibXML->new->parse_file('example/article_bad.xml');
    ok(!$xml->is_valid($dtd));
    eval {
        $xml->validate($dtd);
    };
    print $@, "\n";
    ok($@);

    my $parser = XML::LibXML->new();
    ok($parser->validation(1));
    # this one is OK as it's well formed (no DTD)

    eval{
        $parser->parse_file('example/article_bad.xml');
    };
    ok($@);
    eval {
        $parser->parse_file('example/article_internal_bad.xml');
    };
    print $@, "\n";
    ok($@);
}

# this test fails under XML-LibXML-1.00 with a segfault because the
# underlying DTD element in the C libxml library was freed twice

my $parser = XML::LibXML->new();
my $doc = $parser->parse_file('example/dtd.xml');
my @a = $doc->getChildnodes;
ok(scalar(@a),2);
undef @a;
undef $doc;
 
ok(1);

##
# Tests for ticket 2021
{
    my $dtd = XML::LibXML::Dtd->new("","");
    ok( $dtd, undef );
}

{
    my $dtd = XML::LibXML::Dtd->new('', 'example/test.dtd');
    ok($dtd);
}
