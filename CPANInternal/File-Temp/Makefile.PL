use ExtUtils::MakeMaker;

# Write the makefile
WriteMakefile(
    'NAME'      => 'File::Temp',
    'VERSION_FROM' => 'Temp.pm', # finds $VERSION

    # Part of core perl since 5.6.1
    'INSTALLDIRS' => ( $] > 5.006 ? "perl" : "site" ),

    'PREREQ_PM' => {
         File::Spec => 0.8,
         Fcntl      => 1.03,
         File::Path => 0,
	 Test::More => 0,
       },
    'dist'      => { COMPRESS => "gzip -9f" },
  ($] >= 5.005 ?    ## Add these new keywords supported since 5.005
        (ABSTRACT_FROM => 'Temp.pm',
         AUTHOR     => 'Tim Jenness <tjenness@cpan.org>') : ()),
	      ($ExtUtils::MakeMaker::VERSION gt '6.30'?
	       ('LICENSE'                => 'perl', ) : ()),
);
