#!/usr/local/bin/perl -w

use strict ;

use Carp ;
use Socket ;
use Symbol ;
use Test::More ;

BEGIN{ 

	if( $^O =~ '32' ) {
		plan skip_all => 'skip inode test on windows';
		exit ;
	}
	else {
		plan tests => 3 ;
	}

	use_ok( 'File::Slurp', ) ;
}

my $data = <<TEXT ;
line 1
more text
TEXT

my $file = 'inode' ;

write_file( $file, $data ) ;
my $inode_num = (stat $file)[1] ;
write_file( $file, $data ) ;
my $inode_num2 = (stat $file)[1] ;

#print "I1 $inode_num I2 $inode_num2\n" ;

ok( $inode_num == $inode_num2, 'same inode' ) ;

write_file( $file, {atomic => 1}, $data ) ;
$inode_num2 = (stat $file)[1] ;

#print "I1 $inode_num I2 $inode_num2\n" ;

ok( $inode_num != $inode_num2, 'different inode' ) ;

unlink $file ;
