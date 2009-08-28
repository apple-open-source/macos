##
# $Id: 19encoding.t,v 1.1.1.2 2007/10/10 23:04:15 ahuda Exp $
#
# This should test the XML::LibXML internal encoding/ decoding.
# Since most of the internal encoding code is depentend to 
# the perl version the module is build for. only the encodeToUTF8() and 
# decodeFromUTF8() functions are supposed to be general, while all the 
# magic code is only available for more recent perl version (5.6+)
#
use Test;

BEGIN {
    my $tests        = 1;
    my $basics       = 0;
    my $magic        = 6;

    $tests += $basics;
    $tests += $magic if $] >= 5.006;

    if ( defined $ENV{TEST_LANGUAGES} ) {
      if ( $ENV{TEST_LANGUAGES} eq "all" ) {
	$tests += 2*$basics;
	$tests += 2*$magic if $] >= 5.006;
      } elsif ( $ENV{TEST_LANGUAGES} eq "EUC-JP"
		or $ENV{TEST_LANGUAGES} eq "KOI8-R" ) {
	$tests += $basics;
	$tests += $magic if $] >= 5.006;
      }
    }
    plan tests => $tests;
}

use XML::LibXML::Common;
use XML::LibXML;
ok(1);


my $p = XML::LibXML->new();

# encoding tests
# ok there is the UTF16 test still missing

my $tstr_utf8       = 'test';
my $tstr_iso_latin1 = "täst";

my $domstrlat1 = q{<?xml version="1.0" encoding="iso-8859-1"?>
<täst>täst</täst>
};

if ( $] < 5.006 ) {
    warn "\nskip magic encoding tests on this platform\n";
    exit(0);
}
else {
    print "# magic encoding tests\n";

    my $dom_latin1 = XML::LibXML::Document->new('1.0', 'iso-8859-1');
    my $elemlat1   = $dom_latin1->createElement( $tstr_iso_latin1 );

    $dom_latin1->setDocumentElement( $elemlat1 );

    ok( decodeFromUTF8( 'iso-8859-1' ,$elemlat1->toString()),
        "<$tstr_iso_latin1/>");
    ok( $elemlat1->toString(0,1), "<$tstr_iso_latin1/>");

    my $elemlat2   = $dom_latin1->createElement( "Öl" );
    ok( $elemlat2->toString(0,1), "<Öl/>");

    $elemlat1->appendText( $tstr_iso_latin1 );

    ok( decodeFromUTF8( 'iso-8859-1' ,$elemlat1->string_value()),
        $tstr_iso_latin1);
    ok( $elemlat1->string_value(1), $tstr_iso_latin1);

    ok( $dom_latin1->toString(), $domstrlat1 );

}

exit(0) unless defined $ENV{TEST_LANGUAGES};

if ( $ENV{TEST_LANGUAGES} eq 'all' or $ENV{TEST_LANGUAGES} eq "EUC-JP" ) {
    print "# japanese encoding (EUC-JP)\n";

    my $tstr_euc_jp     = 'À¸ÇþÀ¸ÊÆÀ¸Íñ';
    my $domstrjp = q{<?xml version="1.0" encoding="EUC-JP"?>
<À¸ÇþÀ¸ÊÆÀ¸Íñ>À¸ÇþÀ¸ÊÆÀ¸Íñ</À¸ÇþÀ¸ÊÆÀ¸Íñ>
};
    

    if ( $] >= 5.006 ) {
        my $dom_euc_jp = XML::LibXML::Document->new('1.0', 'EUC-JP');
        $elemjp = $dom_euc_jp->createElement( $tstr_euc_jp );


        ok( decodeFromUTF8( 'EUC-JP' , $elemjp->nodeName()),
            $tstr_euc_jp );
        ok( decodeFromUTF8( 'EUC-JP' ,$elemjp->toString()),
            "<$tstr_euc_jp/>");
        ok( $elemjp->toString(0,1), "<$tstr_euc_jp/>");

        $dom_euc_jp->setDocumentElement( $elemjp );
        $elemjp->appendText( $tstr_euc_jp );

        ok( decodeFromUTF8( 'EUC-JP' ,$elemjp->string_value()),
            $tstr_euc_jp);
        ok( $elemjp->string_value(1), $tstr_euc_jp);

        ok( $dom_euc_jp->toString(), $domstrjp );
    }   

}

if ( $ENV{TEST_LANGUAGES} eq 'all' or $ENV{TEST_LANGUAGES} eq "KOI8-R" ) {
    print "# cyrillic encoding (KOI8-R)\n";

    my $tstr_koi8r       = 'ÐÒÏÂÁ';
    my $domstrkoi = q{<?xml version="1.0" encoding="KOI8-R"?>
<ÐÒÏÂÁ>ÐÒÏÂÁ</ÐÒÏÂÁ>
};
    

    if ( $] >= 5.006 ) {
        my ($dom_koi8, $elemkoi8);

        $dom_koi8 = XML::LibXML::Document->new('1.0', 'KOI8-R');
        $elemkoi8 = $dom_koi8->createElement( $tstr_koi8r );

        ok( decodeFromUTF8( 'KOI8-R' ,$elemkoi8->nodeName()), 
            $tstr_koi8r );

        ok( decodeFromUTF8( 'KOI8-R' ,$elemkoi8->toString()), 
            "<$tstr_koi8r/>");
        ok( $elemkoi8->toString(0,1), "<$tstr_koi8r/>");

        $elemkoi8->appendText( $tstr_koi8r );

        ok( decodeFromUTF8( 'KOI8-R' ,$elemkoi8->string_value()),
            $tstr_koi8r);
        ok( $elemkoi8->string_value(1),
            $tstr_koi8r);
        $dom_koi8->setDocumentElement( $elemkoi8 );

        ok( $dom_koi8->toString(),
            $domstrkoi );
        
    }
}
