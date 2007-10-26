use Module::Build;

my $build = new Module::Build 
  (
   module_name => 'XSTest',
  );

$build->create_build_script;

__END__

# The EU::MM equivalent of the above:

use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'		=> 'XSTest',
    'VERSION_FROM'	=> 'XSTest.pm', # finds $VERSION
    'PREREQ_PM'		=> {}, # e.g., Module::Name => 1.1
    'LIBS'		=> [''], # e.g., '-lm'
    'DEFINE'		=> '', # e.g., '-DHAVE_SOMETHING'
    'INC'		=> '', # e.g., '-I/usr/include/other'
);
