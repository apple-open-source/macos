use Test;
BEGIN { plan tests => 14 }
use XML::SAX::Exception;
eval {
    throw XML::SAX::Exception ( Message => "Test" );
};
ok($@); # test died
ok($@, "Test\n"); # test stringification
ok($@->isa('XML::SAX::Exception')); # test isa

eval {
    throw XML::SAX::Exception::Parse (
            Message => "Parse",
            LineNumber => 12,
            ColumnNumber => 2,
            SystemId => "throw.xml",
            PublicId => "Some // Public // Identifier",
            );
};
ok($@);
ok($@->{Message}, "Parse");
ok($@, qr/Parse/);
ok($@->{LineNumber}, 12);
ok($@->isa('XML::SAX::Exception::Parse'));

eval {
    throw XML::SAX::Exception::ThisOneDoesNotExist (
            Message => "Fubar",
            );
};
ok($@);
ok(!UNIVERSAL::isa($@, 'XML::SAX::Exception'));

eval {
    throw XML::SAX::Exception::NotRecognized (
            Message => "Not Recognized",
            );
};
ok($@);
ok($@->isa('XML::SAX::Exception'));

eval {
    throw XML::SAX::Exception::NotSupported (
            Message => "Not Supported",
            );
};
ok($@);
ok($@->isa('XML::SAX::Exception'));

