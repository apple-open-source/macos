use ExtUtils::MakeMaker;

WriteMakefile
  NAME          => "File::NFSLock",
  AUTHOR        => "Paul Seamons",
  ABSTRACT_FROM => "lib/File/NFSLock.pm",
  VERSION_FROM  => "lib/File/NFSLock.pm",
  PREREQ_PM     => { # e.g., 'Module::Name' => 1.1
  },

  dist          => {
    DIST_DEFAULT => 'all tardist',
    COMPRESS     => 'gzip -vf',
    SUFFIX       => '.gz',
  },

  clean          => {
    FILES        => '*~',
  },

  realclean         => {
    FILES        => '*~',
  },
  ;

package MY;

sub processPL {
  my $self = shift;
  my $block = $self->SUPER::processPL(@_);
  # "Version:" in spec needs to match
  # "$VERSION" from VERSION_FROM
  $block =~ s%(spec.PL\s*)$%$1 \$\(VERSION_FROM\)%m;
  $block;
}

sub libscan {
  my $self = shift;
  my $path = shift;
  ($path =~ / \bCVS\b | \~$ /x) ? undef : $path;
}

sub postamble {
  return qq^

pm_to_blib: README

README: \$(VERSION_FROM)
	pod2text \$(VERSION_FROM) > README
^;
}

1;
