use inc::Module::Install;

name           'Pod-ProjectDocs';
all_from       'lib/Pod/ProjectDocs.pm';
install_script 'bin/pod2projdocs';
requires 'Pod::Parser'              => 1.32;
requires 'Pod::ParseUtils'          => 0;
requires 'Class::Accessor::Fast'    => 0;
requires 'Class::Data::Inheritable' => 0;
requires 'File::Spec'               => 0;
requires 'File::Basename'           => 0;
requires 'File::Find'               => 0;
requires 'File::Copy'               => 0;
requires 'MIME::Base64'             => 0;
requires 'Template'                 => 0;
requires 'Readonly'                 => 0;
requires 'URI::Escape'              => 0;
requires 'JSON'                     => 2.04;
features(
  'Syntax Highlighten' => [
    -default => 1,
    recommends('Syntax::Highlight::Universal'),
  ]
);
WriteAll;

