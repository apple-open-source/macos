use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'      => 'Class::DBI::AbstractSearch',
    'VERSION_FROM' => 'lib/Class/DBI/AbstractSearch.pm', # finds $VERSION
    'PREREQ_PM' => {
	Test::More => 0.32,
	Class::DBI => 0.90,
	SQL::Abstract::Limit => 0.1,
    },
);
