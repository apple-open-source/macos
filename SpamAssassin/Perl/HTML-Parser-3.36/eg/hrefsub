#!/usr/bin/perl

# Perform transformations on link attributes in an HTML document.
# Examples:
#
#  $ hrefsub 's/foo/bar/g' index.html
#  $ hrefsub '$_=URI->new_abs($_, "http://foo")' index.html
#
# The first argument is a perl expression that might modify $_.
# It is called for each link in the document with $_ set to
# the original value of the link URI.  The variables $tag and
# $attr can be used to access the tagname and attributename
# within the tag where the current link is found.
#
# The second argument is the name of a file to process.

use strict;
use HTML::Parser ();
use URI;

# Construct a hash of tag names that may have links.
my %link_attr;
{
    # To simplify things, reformat the %HTML::Tagset::linkElements
    # hash so that it is always a hash of hashes.
    require HTML::Tagset;
    while (my($k,$v) = each %HTML::Tagset::linkElements) {
	if (ref($v)) {
	    $v = { map {$_ => 1} @$v };
	}
	else {
	    $v = { $v => 1};
	}
	$link_attr{$k} = $v;
    }
    # Uncomment this to see what HTML::Tagset::linkElements thinks are
    # the tags with link attributes
    #use Data::Dump; Data::Dump::dump(\%link_attr); exit;
}

# Create a subroutine named 'edit' to perform the operation
# passed in from the command line.  The code should modify $_
# to change things.
my $code = shift;
my $code = 'sub edit { local $_ = shift; my($attr, $tag) = @_; no strict; ' .
           $code .
           '; $_; }';
#print $code;
eval $code;
die $@ if $@;

# Set up the parser.
my $p = HTML::Parser->new(api_version => 3);

# The default is to print everything as is.
$p->handler(default => sub { print @_ }, "text");

# All links are found in start tags.  This handler will evaluate
# &edit for each link attribute found.
$p->handler(start => sub {
		my($tagname, $pos, $text) = @_;
		if (my $link_attr = $link_attr{$tagname}) {
		    while (4 <= @$pos) {
			# use attribute sets from right to left
			# to avoid invalidating the offsets
			# when replacing the values
			my($k_offset, $k_len, $v_offset, $v_len) =
			    splice(@$pos, -4);
			my $attrname = lc(substr($text, $k_offset, $k_len));
			next unless $link_attr->{$attrname};
			next unless $v_offset; # 0 v_offset means no value
			my $v = substr($text, $v_offset, $v_len);
			$v =~ s/^([\'\"])(.*)\1$/$2/;
			my $new_v = edit($v, $attrname, $tagname);
			next if $new_v eq $v;
			$new_v =~ s/\"/&quot;/g;  # since we quote with ""
			substr($text, $v_offset, $v_len) = qq("$new_v");
		    }
		}
		print $text;
	    },
	    "tagname, tokenpos, text");

# Parse the file passed in from the command line
my $file = shift || usage();
$p->parse_file($file) || die "Can't open file $file: $!\n";

sub usage
{
    my $progname = $0;
    $progname =~ s,^.*/,,;
    die "Usage: $progname <perlexpr> <filename>\n";
}
