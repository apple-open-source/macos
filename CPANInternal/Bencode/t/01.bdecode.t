use Test::More;

use Bencode qw( bdecode );

my @test = (
	'0:0:'                     => \[ qr/\Atrailing garbage at 2\b/, 'data past end of first correct bencoded string' ],
	'i'                        => \[ qr/\Aunexpected end of data at 1\b/, 'aborted integer' ],
	'i0'                       => \[ qr/\Amalformed integer data at 1\b/, 'unterminated integer' ],
	'ie'                       => \[ qr/\Amalformed integer data at 1\b/, 'empty integer' ],
	'i341foo382e'              => \[ qr/\Amalformed integer data at 1\b/, 'malformed integer' ],
	'i4e'                      => 4,
	'i0e'                      => 0,
	'i123456789e'              => 123456789,
	'i-10e'                    => -10,
	'i-0e'                     => \[ qr/\Amalformed integer data at 1\b/, 'negative zero integer' ],
	'i123'                     => \[ qr/\Amalformed integer data at 1\b/, 'unterminated integer' ],
	''                         => \[ qr/\Aunexpected end of data at 0/, 'empty data' ],
	'1:'                       => \[ qr/\Aunexpected end of string data starting at 2\b/, 'string longer than data' ],
	'i6easd'                   => \[ qr/\Atrailing garbage at 3\b/, 'integer with trailing garbage' ],
	'35208734823ljdahflajhdf'  => \[ qr/\Agarbage at 0/, 'garbage looking vaguely like a string, with large count' ],
	'2:abfdjslhfld'            => \[ qr/\Atrailing garbage at 4\b/, 'string with trailing garbage' ],
	'0:'                       => '',
	'3:abc'                    => 'abc',
	'10:1234567890'            => '1234567890',
	'02:xy'                    => \[ qr/\Amalformed string length at 0\b/, 'string with extra leading zero in count' ],
	'l'                        => \[ qr/\Aunexpected end of data at 1\b/, 'unclosed empty list' ],
	'le'                       => [],
	'leanfdldjfh'              => \[ qr/\Atrailing garbage at 2\b/, 'empty list with trailing garbage' ],
	'l0:0:0:e'                 => [ '', '', '' ],
	'relwjhrlewjh'             => \[ qr/\Agarbage at 0/, 'complete garbage' ],
	'li1ei2ei3ee'              => [ 1, 2, 3 ],
	'l3:asd2:xye'              => [ 'asd', 'xy' ],
	'll5:Alice3:Bobeli2ei3eee' => [ [ 'Alice', 'Bob' ], [ 2, 3 ] ],
	'd'                        => \[ qr/\Aunexpected end of data at 1\b/, 'unclosed empty dict' ],
	'defoobar'                 => \[ qr/\Atrailing garbage at 2\b/, 'empty dict with trailing garbage' ],
	'de'                       => {},
	'd3:agei25e4:eyes4:bluee'  => { 'age' => 25, 'eyes' => 'blue' },
	'd8:spam.mp3d6:author5:Alice6:lengthi100000eee' => { 'spam.mp3' => { 'author' => 'Alice', 'length' => 100000 } },
	'd3:fooe'                  => \[ qr/\Adict key is missing value at 7\b/, 'dict with odd number of elements' ],
	'di1e0:e'                  => \[ qr/\Adict key is not a string at 1/, 'dict with integer key' ],
	'd1:b0:1:a0:e'             => \[ qr/\Adict key not in sort order at 9/, 'missorted keys' ],
	'd1:a0:1:a0:e'             => \[ qr/\Aduplicate dict key at 9/, 'duplicate keys' ],
	'i03e'                     => \[ qr/\Amalformed integer data at 1/, 'integer with leading zero' ],
	'l01:ae'                   => \[ qr/\Amalformed string length at 1/, 'list with string with leading zero in count' ],
	'9999:x'                   => \[ qr/\Aunexpected end of string data starting at 5/, 'string shorter than count' ],
	'l0:'                      => \[ qr/\Aunexpected end of data at 3/, 'unclosed list with content' ],
	'd0:0:'                    => \[ qr/\Aunexpected end of data at 5/, 'unclosed dict with content' ],
	'd0:'                      => \[ qr/\Aunexpected end of data at 3/, 'unclosed dict with odd number of elements' ],
	'00:'                      => \[ qr/\Amalformed string length at 0/, 'zero-length string with extra leading zero in count' ],
	'l-3:e'                    => \[ qr/\Amalformed string length at 1/, 'list with negative-length string' ],
	'i-03e'                    => \[ qr/\Amalformed integer data at 1/, 'negative integer with leading zero' ],
	"2:\x0A\x0D"               => "\x0A\x0D",
);

plan tests => 1 + @test / 2;

while ( my ( $frozen, $thawed ) = splice @test, 0, 2 ) {
	my $result;
	my $lived = eval { $result = bdecode( $frozen ); 1 };

	if( ref $thawed ne 'REF' ) {
		is_deeply( $result, $thawed, "decode '$frozen'" )
	}
	else {
		my ( $error_rx, $kind_of_brokenness ) = @$$thawed;
		like( $@, $error_rx, "reject $kind_of_brokenness" );
	}
}

is_deeply( bdecode( 'd1:b0:1:a0:e', 1 ), { a => '', b => '', }, "accept missorted keys when decoding leniently" )

# vim: set ft=perl:
