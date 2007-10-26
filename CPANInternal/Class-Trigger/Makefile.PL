use ExtUtils::MakeMaker;
WriteMakefile(
    'NAME'      => 'Class::Trigger',
    'VERSION_FROM' => 'lib/Class/Trigger.pm', # finds $VERSION
    'PREREQ_PM' => {
	Test::More => 0.32,
	Class::Data::Inheritable => 0.02,
	IO::Scalar => 0,
    },
);
