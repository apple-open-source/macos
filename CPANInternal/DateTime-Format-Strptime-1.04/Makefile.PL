use ExtUtils::MakeMaker;

WriteMakefile(
    NAME         => 'DateTime::Format::Strptime',
    VERSION_FROM => 'lib/DateTime/Format/Strptime.pm', # finds $VERSION
    AUTHOR       => 'Rick Measham (rickm@cpan.org)',
    ABSTRACT     => 'Parse and format strp and strf time patterns',
    PREREQ_PM    => {
       'DateTime'           => '0.1402',
       'DateTime::Locale'   => '0.02',
       'DateTime::TimeZone' => '0.25',
       'Params::Validate'   => '0.64',
    },
);
