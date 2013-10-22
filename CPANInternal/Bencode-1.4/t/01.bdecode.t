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
	['d1:a0:e', 0, 1]          => { a => '' }, # Accept single dict when max_depth is 1
	['d1:a0:e', 0, 0]          => \[ qr/\Anesting depth exceeded at 1/, 'single dict when max_depth is 0' ],
	['d1:ad1:a0:ee', 0, 2]     => { a => { a => '' } }, # Accept a nested dict when max_depth is 2
	['d1:ad1:a0:ee', 0, 1]     => \[ qr/\Anesting depth exceeded at 5/, 'nested dict when max_depth is 1' ],
	['l0:e', 0, 1]             => [ '' ], # Accept single list when max_depth is 1
	['l0:e', 0, 0]             => \[ qr/\Anesting depth exceeded at 1/, 'single list when max_depth is 0' ],
	['ll0:ee', 0, 2]           => [ [ '' ] ], # Accept a nested list when max_depth is 2
	['ll0:ee', 0, 1]           => \[ qr/\Anesting depth exceeded at 2/, 'nested list when max_depth is 1' ],
	['d1:al0:ee', 0, 2]        => { a => [ '' ] }, # Accept dict containing list when max_depth is 2
	['d1:al0:ee', 0, 1]        => \[ qr/\Anesting depth exceeded at 5/, 'list in dict when max_depth is 1' ],
	['ld1:a0:ee', 0, 2]        => [ { 'a'  => '' } ], # Accept list containing dict when max_depth is 2
	['ld1:a0:ee', 0, 1]        => \[ qr/\Anesting depth exceeded at 2/, 'dict in list when max_depth is 1' ],
	['d1:a0:1:bl0:ee', 0, 2]   => { a => '', b => [ '' ] }, # Accept dict containing list when max_depth is 2
	['d1:a0:1:bl0:ee', 0, 1]   => \[ qr/\Anesting depth exceeded at 10/, 'list in dict when max_depth is 1' ],
);

plan tests => 1 + @test / 2;

while ( my ( $frozen, $thawed ) = splice @test, 0, 2 ) {
	my $result;
	my $testname;
	my $lived = eval {
		if ( ref $frozen eq 'ARRAY' ) {
			local $, = ', ';
			$testname = "decode [@$frozen]";
			$result = bdecode( @$frozen );
		}
		else {
			$testname = "decode '$frozen'";
			$result = bdecode( $frozen );
		}
		1
	};

	if ( ref $thawed ne 'REF' ) {
		is_deeply( $result, $thawed, $testname );
	}
	else {
		my ( $error_rx, $kind_of_brokenness ) = @$$thawed;
		like( $@, $error_rx, "reject $kind_of_brokenness" );
	}
}

is_deeply(
	bdecode( 'd1:b0:1:a0:e', 1 ),
	{ a => '', b => '', },
	'accept missorted keys when decoding leniently',
);

# vim: set ft=perl:
