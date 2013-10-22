package DBIx::Class::Schema::Loader::Optional::Dependencies;

use warnings;
use strict;

use Carp;

# Stolen from DBIx::Class

# NO EXTERNAL NON-5.8.1 CORE DEPENDENCIES EVER (e.g. C::A::G)
# This module is to be loaded by Makefile.PM on a pristine system

# POD is generated automatically by calling _gen_pod from the
# Makefile.PL in $AUTHOR mode

my $reqs = {
    dist => {
        #'Module::Install::Pod::Inherit' => '0.01',
    },

    use_moose => {
        req => {
            'Moose' => '1.12',
            'MooseX::NonMoose' => '0.16',
            'namespace::autoclean' => '0.09',
            'MooseX::MarkAsMethods' => '0.13',
        },
        pod => {
            title => 'use_moose',
            desc  => 'Modules required for the use_moose option',
        },
    },

    dbicdump_config => {
        req => {
            'Config::Any' => '0',
        },
        pod => {
            title => 'dbicdump config file',
            desc  => 'Modules required for using a config file with dbicdump',
        },
    },

    test_dbicdump_config => {
        req => {
            'Config::Any'     => '0',
            'Config::General' => '0',
        },
        pod => {
            title => 'dbicdump config file testing',
            desc  => 'Modules required for using testing using a config file with dbicdump',
        },
    },

    test_pod => {
        req => {
            'Test::Pod'   => '1.14',
            'Pod::Simple' => '3.22',
        },
        pod => {
            title => 'POD testing',
            desc  => 'Modules required for testing POD in this distribution',
        },
    },
};

sub req_list_for {
  my ($class, $group) = @_;

  croak "req_list_for() expects a requirement group name"
    unless $group;

  my $deps = $reqs->{$group}{req}
    or croak "Requirement group '$group' does not exist";

  return { %$deps };
}


our %req_availability_cache;
sub req_ok_for {
  my ($class, $group) = @_;

  croak "req_ok_for() expects a requirement group name"
    unless $group;

  $class->_check_deps ($group) unless $req_availability_cache{$group};

  return $req_availability_cache{$group}{status};
}

sub req_missing_for {
  my ($class, $group) = @_;

  croak "req_missing_for() expects a requirement group name"
    unless $group;

  $class->_check_deps ($group) unless $req_availability_cache{$group};

  return $req_availability_cache{$group}{missing};
}

sub req_errorlist_for {
  my ($class, $group) = @_;

  croak "req_errorlist_for() expects a requirement group name"
    unless $group;

  $class->_check_deps ($group) unless $req_availability_cache{$group};

  return $req_availability_cache{$group}{errorlist};
}

sub _check_deps {
  my ($class, $group) = @_;

  my $deps = $class->req_list_for ($group);

  my %errors;
  for my $mod (keys %$deps) {
    if (my $ver = $deps->{$mod}) {
      eval "use $mod $ver ()";
    }
    else {
      eval "require $mod";
    }

    $errors{$mod} = $@ if $@;
  }

  if (keys %errors) {
    my $missing = join (', ', map { $deps->{$_} ? "$_ >= $deps->{$_}" : $_ } (sort keys %errors) );
    $missing .= " (see $class for details)" if $reqs->{$group}{pod};
    $req_availability_cache{$group} = {
      status => 0,
      errorlist => { %errors },
      missing => $missing,
    };
  }
  else {
    $req_availability_cache{$group} = {
      status => 1,
      errorlist => {},
      missing => '',
    };
  }
}

sub req_group_list {
  return { map { $_ => { %{ $reqs->{$_}{req} || {} } } } (keys %$reqs) };
}

# This is to be called by the author only (automatically in Makefile.PL)
sub _gen_pod {

  my $class = shift;
  my $modfn = __PACKAGE__ . '.pm';
  $modfn =~ s/\:\:/\//g;

  my $podfn = __FILE__;
  $podfn =~ s/\.pm$/\.pod/;

  my $distver =
    eval { require DBIx::Class::Schema::Loader; DBIx::Class::Schema::Loader->VERSION; }
      ||
    do {
      warn
"\n\n---------------------------------------------------------------------\n" .
'Unable to load the DBIx::Class::Schema::Loader module to determine current ' .
'version, possibly due to missing dependencies. Author-mode autodocumentation ' .
"halted\n\n" . $@ .
"\n\n---------------------------------------------------------------------\n"
      ;
      '*UNKNOWN*';  # rv
    }
  ;

  my @chunks = (
    <<"EOC",
#########################################################################
#####################  A U T O G E N E R A T E D ########################
#########################################################################
#
# The contents of this POD file are auto-generated.  Any changes you make
# will be lost. If you need to change the generated text edit _gen_pod()
# at the end of $modfn
#
EOC
    '=head1 NAME',
    "$class - Optional module dependency specifications (for module authors)",
    '=head1 SYNOPSIS',
    <<EOS,
Somewhere in your build-file (e.g. L<Module::Install>'s Makefile.PL):

  ...

  configure_requires 'DBIx::Class::Schema::Loader' => '$distver';

  require $class;

  my \$use_moose_deps = $class->req_list_for ('use_moose');

  for (keys %\$use_moose_deps) {
    requires \$_ => \$use_moose_deps->{\$_};
  }

  ...

Note that there are some caveats regarding C<configure_requires()>, more info
can be found at L<Module::Install/configure_requires>
EOS
    '=head1 DESCRIPTION',
    <<'EOD',
Some of the features of L<DBIx::Class::Schema::Loader> have external
module dependencies on their own. In order not to burden the average user
with modules he will never use, these optional dependencies are not included
in the base Makefile.PL. Instead an exception with a descriptive message is
thrown when a specific feature is missing one or several modules required for
its operation. This module is the central holding place for  the current list
of such dependencies.
EOD
    '=head1 CURRENT REQUIREMENT GROUPS',
    <<'EOD',
Dependencies are organized in C<groups> and each group can list one or more
required modules, with an optional minimum version (or 0 for any version).
EOD
  );

  for my $group (sort keys %$reqs) {
    my $p = $reqs->{$group}{pod}
      or next;

    my $modlist = $reqs->{$group}{req}
      or next;

    next unless keys %$modlist;

    push @chunks, (
      "=head2 $p->{title}",
      "$p->{desc}",
      '=over',
      ( map { "=item * $_" . ($modlist->{$_} ? " >= $modlist->{$_}" : '') } (sort keys %$modlist) ),
      '=back',
      "Requirement group: B<$group>",
    );
  }

  push @chunks, (
    '=head1 METHODS',
    '=head2 req_group_list',
    '=over',
    '=item Arguments: $none',
    '=item Returns: \%list_of_requirement_groups',
    '=back',
    <<EOD,
This method should be used by DBIx::Class packagers, to get a hashref of all
dependencies keyed by dependency group. Each key (group name) can be supplied
to one of the group-specific methods below.
EOD

    '=head2 req_list_for',
    '=over',
    '=item Arguments: $group_name',
    '=item Returns: \%list_of_module_version_pairs',
    '=back',
    <<EOD,
This method should be used by DBIx::Class extension authors, to determine the
version of modules a specific feature requires in the B<current> version of
L<DBIx::Class::Schema::Loader>. See the L</SYNOPSIS> for a real-world
example.
EOD

    '=head2 req_ok_for',
    '=over',
    '=item Arguments: $group_name',
    '=item Returns: 1|0',
    '=back',
    'Returns true or false depending on whether all modules required by C<$group_name> are present on the system and loadable',

    '=head2 req_missing_for',
    '=over',
    '=item Arguments: $group_name',
    '=item Returns: $error_message_string',
    '=back',
    <<EOD,
Returns a single line string suitable for inclusion in larger error messages.
This method would normally be used by L<DBIx::Class::Schema::Loader>
maintainers, to indicate to the user that he needs to install specific modules
before he will be able to use a specific feature.

For example if some of the requirements for C<use_moose> are not available,
the returned string could look like:

 Moose >= 0 (see use_moose for details)

The author is expected to prepend the necessary text to this message before
returning the actual error seen by the user.
EOD

    '=head2 req_errorlist_for',
    '=over',
    '=item Arguments: $group_name',
    '=item Returns: \%list_of_loaderrors_per_module',
    '=back',
    <<'EOD',
Returns a hashref containing the actual errors that occured while attempting
to load each module in the requirement group.
EOD
    '=head1 AUTHOR',
    'See L<DBIx::Class/CONTRIBUTORS>.',
    '=head1 LICENSE',
    'You may distribute this code under the same terms as Perl itself',
  );

  open (my $fh, '>', $podfn) or croak "Unable to write to $podfn: $!";
  print $fh join ("\n\n", @chunks);
  close ($fh);
}

1;
