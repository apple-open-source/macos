package Module::Build::PPMMaker;

use strict;

# This code is mostly borrowed from ExtUtils::MM_Unix 6.10_03, with a
# few tweaks based on the PPD spec at
# http://www.xav.com/perl/site/lib/XML/PPD.html

sub new {
  my $package = shift;
  return bless {@_}, $package;
}

sub make_ppd {
  my ($self, %args) = @_;
  my $build = delete $args{build};

  die "Cannot create a PPD file unless codebase argument is given\n"
    unless exists $args{codebase};
  my @codebase = ref $args{codebase} ? @{$args{codebase}} : ($args{codebase});

  my $name         = $build->dist_name;
  my $author       = $build->dist_author;
  my $abstract     = $build->dist_abstract;
  my $version      = $self->_ppd_version($build->dist_version);

  $self->_simple_xml_escape($_) foreach ($abstract, $author);

  # could add <LICENSE HREF=...> tag if we knew what the URLs were for
  # various licenses
  my $ppd = sprintf(<<'EOF', $name, $version, $name, $abstract, $author);
<SOFTPKG NAME="%s" VERSION="%s">
    <TITLE>%s</TITLE>
    <ABSTRACT>%s</ABSTRACT>
    <AUTHOR>%s</AUTHOR>
    <IMPLEMENTATION>
EOF

  # We don't include recommended dependencies because PPD has no way
  # to distinguish them from normal dependencies.  We don't include
  # build_requires dependencies because the PPM installer doesn't
  # build or test before installing.  And obviously we don't include
  # conflicts either.
  
  foreach my $type (qw(requires)) {
    my $prereq = $build->$type();
    while (my ($modname, $spec) = each %$prereq) {
      next if $modname eq 'perl';

      my $min_version = '0.0';
      foreach my $c ($build->_parse_conditions($spec)) {
        my ($op, $version) = $c =~ /^\s*  (<=?|>=?|==|!=)  \s*  ([\w.]+)  \s*$/x;

        # This is a nasty hack because it fails if there is no >= op
        if ($op eq '>=') {
          $min_version = $version;
          last;
        }
      }

      # Another hack - dependencies are on modules, but PPD expects
      # them to be on distributions (I think).
      $modname =~ s/::/-/g;

      $ppd .= sprintf(<<'EOF', $modname, $self->_ppd_version($min_version));
        <DEPENDENCY NAME="%s" VERSION="%s" />
EOF

    }
  }

  # We only include these tags if this module involves XS, on the
  # assumption that pure Perl modules will work on any OS.  PERLCORE,
  # unfortunately, seems to indicate that a module works with _only_
  # that version of Perl, and so is only appropriate when a module
  # uses XS.
  if (keys %{$build->find_xs_files}) {
    my $perl_version = $self->_ppd_version($build->perl_version);
    $ppd .= sprintf(<<'EOF', $perl_version, $^O, $self->{archname});
        <PERLCORE VERSION="%s" />
        <OS VALUE="%s" />
        <ARCHITECTURE NAME="%s" />
EOF
  }

  foreach my $codebase (@codebase) {
    $self->_simple_xml_escape($codebase);
    $ppd .= sprintf(<<'EOF', $codebase);
        <CODEBASE HREF="%s" />
EOF
  }

  $ppd .= <<'EOF';
    </IMPLEMENTATION>
</SOFTPKG>
EOF

  my $ppd_file = "$name.ppd";
  my $fh = IO::File->new(">$ppd_file")
    or die "Cannot write to $ppd_file: $!";
  print $fh $ppd;
  close $fh;

  return $ppd_file;
}

sub _ppd_version {
  my ($self, $version) = @_;

  # generates something like "0,18,0,0"
  return join ',', (split(/\./, $version), (0)x4)[0..3];
}


{
  my %escapes = (
		 "\n" => "\\n",
		 '"' => '&quot;',
		 '&' => '&amp;',
		 '>' => '&gt;',
		 '<' => '&lt;',
		);
  my $rx = join '|', keys %escapes;
  
  sub _simple_xml_escape {
    $_[1] =~ s/($rx)/$escapes{$1}/go;
  }
}

1;
__END__

=head1 NAME

Module::Build::PPMMaker - Perl Package Manager file creation

=head1 SYNOPSIS

 On the command line, builds a .ppd file:
 % ./Build ppd

=head1 DESCRIPTION

This package contains the code that builds F<.ppd> "Perl Package
Description" files, in support of ActiveState's "Perl Package
Manager".  Details are here:
L<"http://aspn.activestate.com/ASPN/Downloads/ActivePerl/PPM/">

=head1 AUTHOR

Dave Rolsky <autarch@urth.org>, Ken Williams <ken@mathforum.org>

=head1 SEE ALSO

perl(1), Module::Build(3)

=cut
