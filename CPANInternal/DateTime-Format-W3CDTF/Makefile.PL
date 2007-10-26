 use ExtUtils::MakeMaker;

WriteMakefile(
        'NAME'		=> 'DateTime::Format::W3CDTF',
        'VERSION_FROM' 	=> 'lib/DateTime/Format/W3CDTF.pm',
        'PREREQ_PM' 	=> { 'DateTime' => 0 },
        'ABSTRACT'	=> 'Formatting and parsing of W3CDTF dates',
        'AUTHOR' 	=> 'Kellan Elliott-McCrea <kellan@protest.net>'
 );
