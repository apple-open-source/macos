use Test::More tests => 2 + 2 ;

use strict;

BEGIN {
    require_ok( 'File::Slurp' ) ;
    use_ok('File::Slurp', qw(write_file read_file) ) ;
}


my $data = "\r\n\r\n\r\n" ;
my $file_name = 'newline.txt' ;

stdio_write_file( $file_name, $data ) ;
my $slurped_data = read_file( $file_name ) ; 

my $stdio_slurped_data = stdio_read_file( $file_name ) ; 


print 'data ', unpack( 'H*', $data), "\n",
'slurp ', unpack('H*',  $slurped_data), "\n",
'stdio slurp ', unpack('H*',  $stdio_slurped_data), "\n";

is( $data, $slurped_data, 'slurp' ) ;

write_file( $file_name, $data ) ;
$slurped_data = stdio_read_file( $file_name ) ; 

is( $data, $slurped_data, 'spew' ) ;

unlink $file_name ;

sub stdio_write_file {

	my( $file_name, $data ) = @_ ;

	local( *FH ) ;

	open( FH, ">$file_name" ) || die "Couldn't create $file_name: $!";

	print FH $data ;
}

sub stdio_read_file {

	my( $file_name ) = @_ ;

	open( FH, $file_name ) || die "Couldn't open $file_name: $!";

	local( $/ ) ;

	my $data = <FH> ;

	return $data ;
}


