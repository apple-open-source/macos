use ExtUtils::MakeMaker;

my $brick_over = {
  # Hack to avoid rpm conflicts with old man page.
  MAN3EXT => 3,
  # Hack to force this version to override.
  INSTALLSITELIB => "\$(INSTALLARCHLIB)",
};
WriteMakefile
  NAME => "Net::Ping",
  VERSION_FROM => "lib/Net/Ping.pm",
  ABSTRACT_FROM => "lib/Net/Ping.pm",
  AUTHOR => 'Rob Brown (bbb@cpan.org)',
  PREREQ_PM    => { # e.g., Module::Name => 1.1
    'Socket' => 0,
  },
  dist => {
    COMPRESS => "gzip -9 -vf",
  },
  ;

package MY;

sub constants {
  my $self = shift;
  my $block = $self->SUPER::constants();
  foreach my $field (keys %{ $brick_over }) {
    $block =~ s%^($field\s*=).*%$1 $brick_over->{$field}%m;
  }
  $block;
}

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
