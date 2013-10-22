# $Id$

use lib qw( ../lib ./lib );
use ExtUtils::MakeMaker;
use Template;

WriteMakefile(
    'NAME'     => 'Template::Stash::XS',
    'C'        => [ qw( Stash.c ) ],
    'XS'       => { 'Stash.xs' => 'Stash.c' },
    'OBJECT'   => 'Stash.o',
    'VERSION'  => $Template::VERSION,
    'NORECURS' => 1,
);

