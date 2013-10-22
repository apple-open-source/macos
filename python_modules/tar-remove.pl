#! /usr/bin/perl
#############################################################################
# Does an in-memory removal of tar entries matching the given regex.
#
# Usage:
# % perl tar-remove.pl regex tarball newtarball
#############################################################################

use strict;

package MyTar;

use Archive::Tar;

our @ISA = qw(Archive::Tar);

# Override the remove method.  Order of remaining items is preserved.
sub remove {
    my $self = shift;
    my %list = map { $_ => 1 } @_;
    my @data = grep { !$list{$_->full_path} } @{$self->_data};
    $self->_data( \@data );
    return @data;
}

package main;

my %suffix = (
    '\.tar\.gz$' => MyTar::COMPRESS_GZIP,
    '\.tar\.bz2$' => MyTar::COMPRESS_BZIP,
);

sub matchsuffix {
    my $arg = shift;
    local $_;
    for(keys(%suffix)) {
	return $suffix{$_} if $arg =~ /$_/;
    }
    return undef;
}

die "Usage: $0 regex tarball newtarball\n" unless scalar(@ARGV) == 3;
my $match = qr{$ARGV[0]};
my $tar = MyTar->new($ARGV[1]) || die "$0: Can't open $ARGV[1]\n";
my @list = grep { /$match/ } map { $_->full_path } $tar->get_files();
$tar->remove(@list);
my $compress = matchsuffix($ARGV[2]);
die "$0: Didn't recognize suffix of $ARGV[2]\n" unless defined($compress);
$tar->write($ARGV[2], $compress) || die "$0: Can't create $ARGV[2]\n";
