#!/usr/bin/perl -w -I.

use strict ;
use Test::More tests => 7 ;

use File::Slurp ;

# try to honor possible tempdirs

my $test_dir = "read_dir_$$" ;

mkdir( $test_dir, 0700) || die "mkdir $test_dir: $!" ;

my @dir_entries = read_dir( $test_dir );

ok( @dir_entries == 0, 'empty dir' ) ;

@dir_entries = read_dir( $test_dir, keep_dot_dot => 1 ) ;

ok( @dir_entries == 2, 'empty dir with . ..' ) ;

write_file( "$test_dir/x", "foo\n" ) ;

@dir_entries = read_dir( $test_dir ) ;

ok( @dir_entries == 1, 'dir with 1 file' ) ;

ok( $dir_entries[0] eq 'x', 'dir with file x' ) ;

my $file_cnt = 23 ;

my @expected_entries = sort( 'x', 1 .. $file_cnt ) ;

for ( 1 .. $file_cnt ) {

	write_file( "$test_dir/$_", "foo\n" ) ;
}

@dir_entries = read_dir( $test_dir ) ;
@dir_entries = sort @dir_entries ;

ok( eq_array( \@dir_entries, \@expected_entries ),
	"dir with $file_cnt files" ) ;

my $dir_entries_ref = read_dir( $test_dir ) ;
@{$dir_entries_ref} = sort @{$dir_entries_ref} ;

ok( eq_array( $dir_entries_ref, \@expected_entries ),
	"dir in array ref" ) ;

# clean up

unlink map "$test_dir/$_", @dir_entries ;
rmdir( $test_dir ) || die "rmdir $test_dir: $!";
ok( 1, 'cleanup' ) ;

__END__
