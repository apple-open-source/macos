#!/usr/bin/perl

use strict;
use warnings;
use Test::More;

my $aspell_path = eval q{
    use Test::Spelling; 
    use File::Which;
    which('aspell') || die 'no aspell'
};
plan skip_all => 'Optional Test::Spelling, File::Which and aspell program required to spellcheck POD' if $@;
set_spell_cmd("$aspell_path list");
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
