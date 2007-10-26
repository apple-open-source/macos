use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'Text::LevenshteinXS',
    'VERSION_FROM'	=> 'LevenshteinXS.pm',
    'PREREQ_PM'         => { Test },
    ($] >= 5.005 ? () : (
       ABSTRACT_FROM 	=> 'LevenshteinXS.pm', 
       AUTHOR     	=> 'Dree Mistrut <dree@friul.it> and Josh Goldberg <josh@3io.com>' 
    )),
);
