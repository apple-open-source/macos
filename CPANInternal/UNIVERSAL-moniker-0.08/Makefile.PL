#!/usr/bin/perl -w
use ExtUtils::MakeMaker;

warn <<"EOT";
Hello!

We no longer depend on Lingua::EN::Inflect.
If you care about plurals, you should install it.

EOT

WriteMakefile(
    'NAME'	=> 'UNIVERSAL::moniker',
    'VERSION_FROM'	=> 'moniker.pm',
);
