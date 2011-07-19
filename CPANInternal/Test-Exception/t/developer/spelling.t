#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

my $ispell_path = eval q{
    use Test::Spelling; 
    use File::Which;
    which('ispell') || die 'no ispell'
};
plan skip_all => 'Optional Test::Spelling, File::Which and ispell program required to spellcheck POD' if $@;
set_spell_cmd("$ispell_path -l");
add_stopwords( <DATA> );
all_pod_files_spelling_ok();

__DATA__
AnnoCPAN
CPAN
perlmonks
RSS
Boumans
Cees
Godin
Harkins
Hek
Purkis
Schleicher
Muhlestein
Perrin
Prew
Krieger
LICENCE
McCann
Jos
Jost
qa
Adrian
Cantrell
Janek
Jore
ben
Khemir
Nadim
Pagaltzis
Dolan
RT
Ricardo
Signes
