#============================================================= -*-perl-*-
#
# t/unicode.t
#
# Test the handling of Unicode text in templates.
#
# Written by Mark Fowler <mark@twoshortplanks.com>
#
# This is free software; you can redistribute it and/or modify it
# under the same terms as Perl itself.
#
# $Id$
# 
#========================================================================

use strict;
use warnings;
use lib qw( ./lib ../lib );
use Template::Provider;
#use Template::Test;
#ntests(20);

BEGIN {
    unless ($] > 5.007) {
        print "1..0 # Skip perl < 5.8 can't do unicode well enough\n";
        exit;
    }
}

use Template;

use File::Temp qw(tempfile tempdir);
use File::Spec::Functions;
use Cwd;

use Test::More tests => 20;


# This is 'moose...' (with slashes in the 'o's them, and the '...' as one char).
my $moose = "m\x{f8}\x{f8}se\x{2026}";

# right, create some templates in various encodings by hand
# (it's the only way to be 100% sure they contain the right text)
my %encoded_text = (
 'UTF-8'    => "\x{ef}\x{bb}\x{bf}m\x{c3}\x{b8}\x{c3}\x{b8}se\x{e2}\x{80}\x{a6}",
 'UTF-16BE' => "\x{fe}\x{ff}\x{0}m\x{0}\x{f8}\x{0}\x{f8}\x{0}s\x{0}e &",
 'UTF-16LE' => "\x{ff}\x{fe}m\x{0}\x{f8}\x{0}\x{f8}\x{0}s\x{0}e\x{0}& ",
 'UTF-32BE' => "\x{0}\x{0}\x{fe}\x{ff}\x{0}\x{0}\x{0}m\x{0}\x{0}\x{0}\x{f8}\x{0}\x{0}\x{0}\x{f8}\x{0}\x{0}\x{0}s\x{0}\x{0}\x{0}e\x{0}\x{0} &",
 'UTF-32LE' => "\x{ff}\x{fe}\x{0}\x{0}m\x{0}\x{0}\x{0}\x{f8}\x{0}\x{0}\x{0}\x{f8}\x{0}\x{0}\x{0}s\x{0}\x{0}\x{0}e\x{0}\x{0}\x{0}& \x{0}\x{0}",
);

# write those variables to temp files in a temp directory
my %filenames = (
  map { $_ => write_to_temp_file(
                filename => $_,
                text     => $encoded_text{ $_ },
                # uncomment to create files in cwd
                # dir      => cwd,
              )
   } keys %encoded_text
);

my $tempdir = create_cache_dir();

# setup template toolkit and test all the encodings
my $tt = setup_tt( tempdir => $tempdir );
test_it("first try", $tt, \%filenames, $moose);
test_it("in memory", $tt, \%filenames, $moose);

# okay, now we test everything again to see if the cache file
# was written in a consisant state
$tt = setup_tt( tempdir => $tempdir );
test_it("from cache", $tt, \%filenames, $moose);
test_it("in cache, in memory", $tt, \%filenames, $moose);


#########################################################################

sub create_cache_dir { 
    return tempdir( CLEANUP => 1 ); 
}

sub setup_tt {
    my %args = @_;
    return Template->new( ABSOLUTE => 1,
                          COMPILE_DIR => $args{tempdir},
                          COMPILE_EXT => ".ttcache");
}

sub test_it {
    local $Test::Builder::Level = $Test::Builder::Level + 1;

    my $name      = shift;
    my $tt        = shift;
    my $filenames = shift;
    my $string    = shift;

    foreach my $encoding (keys %{ $filenames })
    {
        my $output;
        $tt->process($filenames->{ $encoding },{},\$output)
            or $output = $tt->error;
        is(reasciify($output), reasciify($string), "$name - $encoding");
    }
}


#------------------------------------------------------------------------
# reascify($string)
#
# escape all the high and low chars to \x{..} sequences
#------------------------------------------------------------------------

sub reasciify {
    my $string = shift;
    $string = join '', map {
        my $ord = ord($_);
        ($ord > 127 || ($ord < 32 && $ord != 10))
            ? sprintf '\x{%x}', $ord
            : $_
        } split //, $string;
    return $string;
}


#------------------------------------------------------------------------
# write_to_temp_file( dir => $dir, filename => $file, text => $text)
#
# escape all the high and low chars to \x{..} sequences
#------------------------------------------------------------------------

sub write_to_temp_file {
    my %args = @_;

    # use a temp dir unless one was specified.  We automatically
    # delete the contents when we're done with the tempdir, where
    # otherwise we just leave the files lying around.
    unless (exists $args{dir}) { 
        $args{dir} = tempdir( CLEANUP => 1 );
    }
    
    # work out where we're going to store it
    my $temp_filename = catfile($args{dir}, $args{filename});
    
    # open a filehandle with some PerlIO magic to convert data into
    # the correct encoding with the correct BOM on the front
    open my $temp_fh, ">:raw", $temp_filename
        or die "Can't write to '$temp_filename': $!";

    # write the data out
    print $temp_fh $args{text};
    close $temp_fh;
    
    # return where we've created it
    return $temp_filename;
}
