use Module::Build;

my @compiler_flags;
if ($] < 5.006 and Module::Build->y_n("Does your system define the getcwd() C library function?", 'y')) {
  @compiler_flags = ('-DHAS_GETCWD');
}

my $class = Module::Build->subclass(code => <<'EOF');

  # See http://rt.cpan.org/NoAuth/Bug.html?id=4681 for why we add
  # this to the Makefile.PL:
  sub do_create_makefile_pl {
    my $self = shift;
    my $fh = IO::File->new('> Makefile.PL') or die $!;
    print $fh "\nBEGIN { \@INC = grep {!/blib/} \@INC }\n\n";
    $self->SUPER::do_create_makefile_pl(fh => $fh);
  }

  # To keep MakeMaker happy, we have .pm files in the top-level
  # directory as well as in lib/.
  sub find_pm_files {
    my $self = shift;
    my $files = $self->SUPER::find_pm_files(@_);  # Finds in lib/
    $files->{'Cwd.pm'} = 'lib/Cwd.pm';
    return $files;
  }

  # If we're building the Makefile.PL, don't note the dependence on Module::Build.
  sub build_requires {
    my $self = shift;
    my $in_create_makefile_pl = 0;
    my $i = 0;
    while (defined( my $subr = (caller $i++)[3] )) {
      $in_create_makefile_pl = 1 if $subr =~ /::create_makefile_pl$/;
    }
    my $prereqs = $self->SUPER::build_requires(@_);
    return $prereqs unless $in_create_makefile_pl;
    return { map {$_, $prereqs->{$_}} grep !/^Module::Build$/, keys %$prereqs };
  }

  # Copy the ppport.h file to where it will be expected under Module::Build
  sub ACTION_code {
    my $self = shift;
    my $ppp = 'ppport.h';
    my $lib_ppp = File::Spec->catfile('lib', $ppp);
    $self->copy_if_modified(from => $ppp, to => $lib_ppp);
    $self->add_to_cleanup($lib_ppp);
    $self->SUPER::ACTION_code(@_);
  }

  # Make sure the two main $VERSION numbers agree.  This is just a
  # simple way of making sure both VERSIONs get updated with every release.
  sub ACTION_distdir {
    my $self = shift;
    my $v1 = $self->version_from_file('Cwd.pm');
    my $v2 = $self->version_from_file('lib/File/Spec.pm');
    die "Version $v1 from Cwd.pm doesn't match version $v2 from lib/File/Spec.pm"
      unless $v1 eq $v2;

    return $self->SUPER::ACTION_distdir(@_);
  }

EOF

my @linker_flags;
if ($ENV{RC_CFLAGS}) {
    my @extra_cflags = split " ", $ENV{RC_CFLAGS};
    push @compiler_flags, @extra_cflags;
    push @linker_flags, @extra_cflags;
}

my $build = $class->new
  (
   dist_name => 'PathTools',
   dist_abstract => 'Tools for working with paths and file specs across platforms',
   dist_version_from => 'Cwd.pm',
   license => 'perl',
   installdirs => 'core',
 #  create_makefile_pl => 'traditional',
   requires       => {
		      'perl' => 5.005,
		      'Carp' => 0,
		      'File::Basename' => 0,
		     },
   build_requires => {
		      'File::Path' => 0,
		      'Test' => 0,
		      'Scalar::Util' => 0,
		      'Module::Build' => '0.19',
		      'ExtUtils::CBuilder' => 0,
		      ($^O =~ /^(dos|MSWin32)$/ ? ('ExtUtils::Install' => 1.39) : ()),
		     },
   xs_files       => { 'Cwd.xs' => 'lib/Cwd.xs' },
   extra_compiler_flags => \@compiler_flags,
   extra_linker_flags => \@linker_flags,
   dynamic_config => 1,  # Because of the $^O checking above
   sign => 1,
  );

$build->create_build_script;
