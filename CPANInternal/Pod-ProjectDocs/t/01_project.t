use strict;
use warnings;
use FindBin;
use Test::More 'no_plan';
use File::Path qw(  remove_tree );

use lib '../lib';
use Pod::ProjectDocs;

Pod::ProjectDocs->new(
    outroot => "$FindBin::Bin/output",
    libroot => "$FindBin::Bin/sample/lib",
    forcegen => 1,
)->gen;

# using XML::XPath might be better
open my $fh, "<:encoding(UTF-8)", "$FindBin::Bin/output/Sample/Project.pm.html";
my $html = join '', <$fh>;
close $fh;

like $html, qr!See <a href="#SYNOPSIS">SYNOPSIS</a> for its usage!;
like $html, qr!<a href="http://www.perl.org/">http://www.perl.org/</a>!;
like $html, qr!<a href="http://search.cpan.org/perldoc\?perlpod">Perl POD Syntax</a>!;
like $html, qr!href="../podstyle.css"!;
like $html, qr!href="../index.html"!;
like $html, qr!href="../src/Sample/Project.pm"!;
like $html, qr!src="../up.gif"!;
like $html, qr!mäh!;

open my $i_fh, "<:encoding(UTF-8)", "$FindBin::Bin/output/index.html";
my $index_html = join '', <$i_fh>;
close $i_fh;
like $index_html, qr!Sample/Module.pm.html!;
like $index_html, qr!Sample/Project.pm.html!;

remove_tree( "$FindBin::Bin/output" );
