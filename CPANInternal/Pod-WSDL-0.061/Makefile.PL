
use strict;
use warnings;



use ExtUtils::MakeMaker 6.31;



my %WriteMakefileArgs = (
  'ABSTRACT' => 'Creates WSDL documents from (extended) pod',
  'AUTHOR' => 'Tarek Ahmed <bloerch -at- oelbsk.org>',
  'BUILD_REQUIRES' => {},
  'CONFIGURE_REQUIRES' => {
    'ExtUtils::MakeMaker' => '6.31'
  },
  'DISTNAME' => 'Pod-WSDL',
  'EXE_FILES' => [],
  'LICENSE' => 'perl',
  'NAME' => 'Pod::WSDL',
  'PREREQ_PM' => {
    'Carp' => '1.03',
    'IO::Scalar' => '2.110',
    'Pod::Text' => '2.21',
    'Test::More' => '0.47',
    'XML::Writer' => '0.531',
    'XML::XPath' => '1.13'
  },
  'VERSION' => '0.061',
  'test' => {
    'TESTS' => 't/*.t'
  }
);


unless ( eval { ExtUtils::MakeMaker->VERSION(6.56) } ) {
  my $br = delete $WriteMakefileArgs{BUILD_REQUIRES};
  my $pp = $WriteMakefileArgs{PREREQ_PM};
  for my $mod ( keys %$br ) {
    if ( exists $pp->{$mod} ) {
      $pp->{$mod} = $br->{$mod} if $br->{$mod} > $pp->{$mod};
    }
    else {
      $pp->{$mod} = $br->{$mod};
    }
  }
}

delete $WriteMakefileArgs{CONFIGURE_REQUIRES}
  unless eval { ExtUtils::MakeMaker->VERSION(6.52) };

WriteMakefile(%WriteMakefileArgs);



