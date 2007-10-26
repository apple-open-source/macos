# Before `make install' is performed this script should be runnable with
# `make test'. After `make install' it should work as `perl test.pl'

#########################

# change 'tests => 1' to 'tests => last_test_to_print';

use Test;
BEGIN { plan tests => 8 };
use XML::LibXML::Common qw( :libxml :encoding );

use constant TEST_STRING_GER => "Hänsel und Gretel";
use constant TEST_STRING_GER2 => "täst";
use constant TEST_STRING_UTF => 'test';
use constant TEST_STRING_JP  => 'À¸ÇþÀ¸ÊÆÀ¸Íñ';

ok(1); # If we made it this far, we're ok.

#########################

# Insert your test code below, the Test module is use()ed here so read
# its man page ( perldoc Test ) for help writing this test script.

# ok( ELEMENT_NODE, 1 );
ok( XML_ELEMENT_NODE, 1 );

# encoding();

ok( decodeFromUTF8('iso-8859-1',
                   encodeToUTF8('iso-8859-1',
                                TEST_STRING_GER2 ) ),
    TEST_STRING_GER2 );


ok( decodeFromUTF8( 'UTF-8' ,
                     encodeToUTF8('UTF-8', TEST_STRING_UTF ) ),
    TEST_STRING_UTF );


my $u16 = decodeFromUTF8( 'UTF-16',
                          encodeToUTF8('UTF-8', TEST_STRING_UTF ) );
ok( length($u16), 2*length(TEST_STRING_UTF));

my $u16be = decodeFromUTF8( 'UTF-16BE',
                            encodeToUTF8('UTF-8', TEST_STRING_UTF ) );
ok( length($u16be), 2*length(TEST_STRING_UTF));

my $u16le = decodeFromUTF8( 'UTF-16LE', 
                            encodeToUTF8('UTF-8', TEST_STRING_UTF ) );
ok( length($u16le), 2*length(TEST_STRING_UTF));

#bad encoding name test.
eval {
    my $str = encodeToUTF8( "foo" , TEST_STRING_GER2 );
};
ok( length( $@ ) );

# here should be a test to test badly encoded strings. but for some
# reasons i am unable to create an apropriate test :(

# uncomment these lines if your system is capable to handel not only i
# so latin 1
#ok( decodeFromUTF8('EUC-JP',
#                   encodeToUTF8('EUC-JP',
#                                TEST_STRING_JP ) ),
#    TEST_STRING_JP );
