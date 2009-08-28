use Test::More;

use Bencode qw( bencode );

my @test = (
	'i4e'                      => 4,
	'i0e'                      => 0,
	'i-10e'                    => -10,
	'i12345678901234567890e'   => '12345678901234567890',
	'0:'                       => '',
	'3:abc'                    => 'abc',
	'10:1234567890'            => \'1234567890',
	'le'                       => [],
	'li1ei2ei3ee'              => [ 1, 2, 3 ],
	'll5:Alice3:Bobeli2ei3eee' => [ [ 'Alice', 'Bob' ], [ 2, 3 ] ],
	'de'                       => {},
	'd3:agei25e4:eyes4:bluee'  => { 'age' => 25, 'eyes' => 'blue' },
	'd8:spam.mp3d6:author5:Alice6:lengthi100000eee' => { 'spam.mp3' => { 'author' => 'Alice', 'length' => 100000 } },
);

plan tests => 0 + @test / 2;

while ( my ( $frozen, $thawed ) = splice @test, 0, 2 ) {
	is_deeply( bencode( $thawed ), $frozen, "encode $frozen" );
}

# vim: set ft=perl:
