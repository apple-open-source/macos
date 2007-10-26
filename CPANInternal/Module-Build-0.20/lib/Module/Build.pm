package Module::Build;

# This module doesn't do much of anything itself, it inherits from the
# modules that do the real work.  The only real thing it has to do is
# figure out which OS-specific module to pull in.  Many of the
# OS-specific modules don't do anything either - most of the work is
# done in Module::Build::Base.

use strict;
use File::Spec ();
use File::Path ();
use File::Basename ();

use vars qw($VERSION @ISA);
$VERSION = '0.20';

# Okay, this is the brute-force method of finding out what kind of
# platform we're on.  I don't know of a systematic way.  These values
# came from the latest (bleadperl) perlport.pod.

my %OSTYPES = qw(
		 aix       Unix
		 bsdos     Unix
		 dgux      Unix
		 dynixptx  Unix
		 freebsd   Unix
		 linux     Unix
		 hpux      Unix
		 irix      Unix
		 darwin    Unix
		 machten   Unix
		 next      Unix
		 openbsd   Unix
		 dec_osf   Unix
		 svr4      Unix
		 sco_sv    Unix
		 svr4      Unix
		 unicos    Unix
		 unicosmk  Unix
		 solaris   Unix
		 sunos     Unix
		 cygwin    Unix
		 
		 dos       Windows
		 MSWin32   Windows

		 os390     EBCDIC
		 os400     EBCDIC
		 posix-bc  EBCDIC
		 vmesa     EBCDIC

		 MacOS     MacOS
		 VMS       VMS
		 VOS       VOS
		 riscos    RiscOS
		 amigaos   Amiga
		 mpeix     MPEiX
		);

# We only use this once - don't waste a symbol table entry on it.
# More importantly, don't make it an inheritable method.
my $load = sub {
  my $mod = shift;
  #warn "Using $mod";
  eval "use $mod";
  die $@ if $@;
  @ISA = ($mod);
};

if (grep {-e File::Spec->catfile($_, qw(Module Build Platform), $^O) . '.pm'} @INC) {
  $load->("Module::Build::Platform::$^O");

} elsif (exists $OSTYPES{$^O}) {
  $load->("Module::Build::Platform::$OSTYPES{$^O}");

} else {
  warn "Unknown OS type '$^O' - using default settings\n";
  $load->("Module::Build::Platform::Default");
}

sub os_type { $OSTYPES{$^O} }

1;
__END__


=head1 NAME

Module::Build - Build and install Perl modules

=head1 SYNOPSIS

Standard process for building & installing modules:
 
   perl Build.PL
   ./Build
   ./Build test
   ./Build install

Or, if you're on a platform (like DOS or Windows) that doesn't like
the "./" notation, you can do this:

   perl Build.PL
   perl Build
   perl Build test
   perl Build install

=head1 DESCRIPTION

C<Module::Build> is a system for building, testing, and installing
Perl modules.  It is meant to be a replacement for
C<ExtUtils::MakeMaker>.  Developers may alter the behavior of the
module through subclassing in a much more straightforward way than
with C<MakeMaker>.  It also does not require a C<make> on your system
- most of the C<Module::Build> code is pure-perl and written in a very
cross-platform way.  In fact, you don't even need a shell, so even
platforms like MacOS (traditional) can use it fairly easily.  Its only
prerequisites are modules that are included with perl 5.6.0, and it
works fine on perl 5.005 if you can install a few additional modules.

See L<MOTIVATIONS> for more comparisons between C<ExtUtils::MakeMaker>
and C<Module::Build>.

To install C<Module::Build>, and any other module that uses
C<Module::Build> for its installation process, do the following:

  perl Build.PL       # 'Build.PL' script creates the 'Build' script
  ./Build             # Need ./ to ensure we're using this "Build" script
  ./Build test        # and not another one that happens to be in the PATH
  ./Build install

This illustrates initial configuration and the running of three
'actions'.  In this case the actions run are 'build' (the default
action), 'test', and 'install'.  Actions defined so far include:

  build                          docs        
  clean                          fakeinstall 
  code                           help        
  diff                           install     
  dist                           manifest    
  distcheck                      ppd         
  distclean                      realclean   
  distdir                        skipcheck   
  distmeta                       test        
  distsign                       testdb      
  disttest                       versioninstall

You can run the 'help' action for a complete list of actions.

When creating a C<Build.PL> script for a module, something like the
following code will typically be used:

  use Module::Build;
  my $build = Module::Build->new
    (
     module_name => 'Foo::Bar',
     license => 'perl',
     requires => {
                  'perl'           => '5.6.1',
                  'Some::Module'   => '1.23',
                  'Other::Module'  => '>= 1.2, != 1.5, < 2.0',
                 },
    );
  $build->create_build_script;

A simple module could get away with something as short as this for its
C<Build.PL> script:

  use Module::Build;
  Module::Build->new(
     module_name => 'Foo::Bar',
     license => 'perl',
  )->create_build_script;

The model used by C<Module::Build> is a lot like the C<MakeMaker>
metaphor, with the following correspondences:

   In Module::Build                 In ExtUtils::MakeMaker
  ---------------------------      ------------------------
   Build.PL (initial script)        Makefile.PL (initial script)
   Build (a short perl script)      Makefile (a long Makefile)
   _build/ (saved state info)       various config text in the Makefile

Any customization can be done simply by subclassing C<Module::Build>
and adding a method called (for example) C<ACTION_test>, overriding
the default 'test' action.  You could also add a method called
C<ACTION_whatever>, and then you could perform the action C<Build
whatever>.

For information on providing backward compatibility with
C<ExtUtils::MakeMaker>, see L<Module::Build::Compat>.

=head1 METHODS

I list here some of the most important methods in C<Module::Build>.
Normally you won't need to deal with these methods unless you want to
subclass C<Module::Build>.  But since one of the reasons I created
this module in the first place was so that subclassing is possible
(and easy), I will certainly write more docs as the interface
stabilizes.

=over 4

=item new()

Creates a new Module::Build object.  Arguments to the new() method are
listed below.  Most arguments are optional, but you must provide
either the C<module_name> argument, or C<dist_name> and one of
C<dist_version> or C<dist_version_from>.  In other words, you must
provide enough information to determine both a distribution name and
version.

=over 4

=item module_name

The C<module_name> is a shortcut for setting default values of
C<dist_name> and C<dist_version_from>, reflecting the fact that the
majority of CPAN distributions are centered around one "main" module.
For instance, if you set C<module_name> to C<Foo::Bar>, then
C<dist_name> will default to C<Foo-Bar> and C<dist_version_from> will
default to C<lib/Foo/Bar.pm>.  C<dist_version_from> will in turn be
used to set C<dist_version>.

Setting C<module_name> won't override a C<dist_*> parameter you
specify explicitly.

=item dist_name

Specifies the name for this distribution.  Most authors won't need to
set this directly, they can use C<module_name> to set C<dist_name> to
a reasonable default.  However, some agglomerative distributions like
C<libwww-perl> or C<bioperl> have names that don't correspond directly
to a module name, so C<dist_name> can be set independently.

=item dist_version

Specifies a version number for the distribution.  See C<module_name>
or C<dist_version_from> for ways to have this set automatically from a
C<$VERSION> variable in a module.  One way or another, a version
number needs to be set.

=item dist_version_from

Specifies a file to look for the distribution version in.  Most
authors won't need to set this directly, they can use C<module_name>
to set it to a reasonable default.

The version is extracted from the specified file according to the same
rules as C<ExtUtils::MakeMaker> and C<CPAN.pm>.  It involves finding
the first line that matches the regular expression

   /([\$*])(([\w\:\']*)\bVERSION)\b.*\=/

, eval()-ing that line, then checking the value of the C<$VERSION>
variable.  Quite ugly, really, but all the modules on CPAN depend on
this process, so there's no real opportunity to change to something
better.

=item license

Specifies the licensing terms of your distribution.  Valid options include:

=over 4

=item perl

The distribution may be copied and redistributed under the same terms
as perl itself (this is by far the most common licensing option for
modules on CPAN).  This is a dual license, in which the user may
choose between either the GPL or the Artistic license.

=item gpl

The distribution is distributed under the terms of the Gnu General
Public License (http://www.opensource.org/licenses/gpl-license.php).

=item lgpl

The distribution is distributed under the terms of the Gnu Lesser
General Public License
(http://www.opensource.org/licenses/lgpl-license.php).

=item artistic

The distribution is licensed under the Artistic License, as specified
by the F<Artistic> file in the standard perl distribution.

=item bsd

The distribution is licensed under the BSD License
(http://www.opensource.org/licenses/bsd-license.php).

=item open_source

The distribution is licensed under some other Open Source
Initiative-approved license listed at
http://www.opensource.org/licenses/ .

=item unrestricted

The distribution is licensed under a license that is B<not> approved
by www.opensource.org but that allows distribution without
restrictions.

=item restrictive

The distribution may not be redistributed without special permission
from the author and/or copyright holder.

=back

Note that you must still include the terms of your license in your
documentation - this field only lets automated tools figure out your
licensing restrictions.  Humans still need something to read.

It is a fatal error to use a license other than the ones mentioned
above.  This is not because I wish to impose licensing terms on you -
please let me know if you would like another license option to be
added to the list.  You may also use a license type of C<unknown> if
you don't wish to specify your terms (but this is usually not a good
idea for you to do!).

I just started out with a small set of licenses to keep things simple,
figuring I'd let people with actual working knowledge in this area
tell me what to do.  So if that's you, drop me a line.

=item requires

An optional C<requires> argument specifies any module prerequisites that
the current module depends on.  The prerequisites are given in a hash
reference, where the keys are the module names and the values are
version specifiers:

 requires => {Foo::Module => '2.4',
              Bar::Module => 0,
              Ken::Module => '>= 1.2, != 1.5, < 2.0',
              perl => '5.6.0'},

These four version specifiers have different effects.  The value
C<'2.4'> means that B<at least> version 2.4 of C<Foo::Module> must be
installed.  The value C<0> means that B<any> version of C<Bar::Module>
is acceptable, even if C<Bar::Module> doesn't define a version.  The
more verbose value C<'E<gt>= 1.2, != 1.5, E<lt> 2.0'> means that
C<Ken::Module>'s version must be B<at least> 1.2, B<less than> 2.0,
and B<not equal to> 1.5.  The list of criteria is separated by commas,
and all criteria must be satisfied.

A special C<perl> entry lets you specify the versions of the Perl
interpreter that are supported by your module.  The same version
dependency-checking semantics are available, except that we also
understand perl's new double-dotted version numbers.

One note: currently C<Module::Build> doesn't actually I<require> the
user to have dependencies installed, it just strongly urges.  In the
future we may require it.  There's now a C<recommends> section for
things that aren't absolutely required.

Automated tools like CPAN.pm should refuse to install a module if one
of its dependencies isn't satisfied, unless a "force" command is given
by the user.  If the tools are helpful, they should also offer to
install the dependencies.

A sysnonym for C<requires> is C<prereq>, to help succour people
transitioning from C<ExtUtils::MakeMaker>.  The C<requires> term is
preferred, but the C<prereq> term will remain valid in future
distributions.

=item recommends

This is just like the C<requires> argument, except that modules listed
in this section aren't essential, just a good idea.  We'll just print
a friendly warning if one of these modules aren't found, but we'll
continue running.

If a module is recommended but not required, all tests should still
pass if the module isn't installed.  This may mean that some tests
will be skipped if recommended dependencies aren't present.

Automated tools like CPAN.pm should inform the user when recommended
modules aren't installed, and it should offer to install them if it
wants to be helpful.

=item build_requires

Modules listed in this section are necessary to build and install the
given module, but are not necessary for regular usage of it.  This is
actually an important distinction - it allows for tighter control over
the body of installed modules, and facilitates correct dependency
checking on binary/packaged distributions of the module.

=item conflicts

Modules listed in this section conflict in some serious way with the
given module.  C<Module::Build> will refuse to install the given
module if

=item create_makefile_pl

This parameter lets you use Module::Build::Compat during the
C<distdir> (or C<dist>) action to automatically create a Makefile.PL
for compatibility with ExtUtils::MakeMaker.  The parameter's value
should be one of the styles named in the Module::Build::Compat
documentation.

=item c_source

An optional C<c_source> argument specifies a directory which contains
C source files that the rest of the build may depend on.  Any C<.c>
files in the directory will be compiled to object files.  The
directory will be added to the search path during the compilation and
linking phases of any C or XS files.

=item pm_files

An optional parameter specifying the set of C<.pm> files in this
distribution, specified as a hash reference whose keys are the files'
locations in the distributions, and whose values are their logical
locations based on their package name, i.e. where they would be found
in a "normal" Module::Build-style distribution.  This parameter is
mainly intended to support alternative layouts of files.

For instance, if you have an old-style MakeMaker distribution for a
module called C<Foo::Bar> and a F<Bar.pm> file at the top level of the
distribution, you could specify your layout in your C<Build.PL> like
this:

 my $build = Module::Build->new
   ( module_name => 'Foo::Bar',
     ...
     pm_files => { 'Bar.pm' => 'lib/Foo/Bar.pm' },
   );

Note that the values should include C<lib/>, because this is where
they would be found in a "normal" Module::Build-style distribution.

Note also that the path specifications are I<always> given in
Unix-like format, not in the style of the local system.

=item pod_files

Just like C<pm_files>, but used for specifying the set of C<.pod>
files in your distribution.

=item xs_files

Just like C<pm_files>, but used for specifying the set of C<.xs>
files in your distribution.

=item PL_files

An optional parameter specifying a set of C<.PL> files in your
distribution.  These will be run as Perl scripts prior to processing
the rest of the files in your distribution.  They are usually used as
templates for creating other files dynamically, so that a file like
C<lib/Foo/Bar.pm.PL> might create the file C<lib/Foo/Bar.pm>.

The files are specified with the C<.PL> files as hash keys, and the
file(s) they generate as hash values, like so:

 my $build = Module::Build->new
   ( module_name => 'Foo::Bar',
     ...
     PL_files => { 'lib/Bar.pm.PL' => 'lib/Bar.pm',
                   'lib/Foo.PL' => [ 'lib/Foo1.pm', 'lib/Foo2.pm' ],
                 },
   );

Note that the path specifications are I<always> given in Unix-like
format, not in the style of the local system.

=item script_files

An optional parameter specifying a set of files that should be
installed as executable perl scripts when the module is installed.
May be given as an array reference of the files, or as a hash
reference whose keys are the files (and whose values will currently be
ignored).

The default is to install no script files - in other words, there is
no default location where Module::Build will look for script files to
install.

For backward compatibility, you may use the parameter C<scripts>
instead of C<script_files>.  Please consider this usage deprecated,
though it will continue to exist for several version releases.

=item autosplit

An optional C<autosplit> argument specifies a file which should be run
through the C<Autosplit::autosplit()> function.  In general I don't
consider this a great idea, because it's not always clear that
autosplitting achieves its intended performance benefits.  It may even
harm performance in environments like mod_perl, where as much as
possible of a module's code should be loaded during startup.

=item dynamic_config

A boolean flag indicating whether the F<Build.PL> file must be
executed, or whether this module can be built, tested and installed
solely from consulting its metadata file.  The default value is 0,
reflecting the fact that "most" of the modules on CPAN just need to be
copied from one place to another.  The main reason to set this to a
true value is that your module performs some dynamic configuration as
part of its build/install process.

Currently C<Module::Build> doesn't actually do anything with this flag
- it's probably going to be up to tools like C<CPAN.pm> to do
something useful with it.  It can potentially bring lots of security,
packaging, and convenience improvements.

=item add_to_cleanup

An array reference of files to be cleaned up when the C<clean> action
is performed.  See also the add_to_cleanup() method.

=item sign

If a true value is specified for this parameter, C<Module::Signature>
will be used (via the 'distsign' action) to create a SIGNATURE file
for your distribution during the 'distdir' action.  The default is
false.  In the future, the default may change to true if you have
C<Module::Signature> installed on your system.

=item extra_compiler_flags

=item extra_linker_flags

These parameters can contain array references (or strings, in which
case they will be split into arrays) to pass through to the compiler
and linker phases when compiling/linking C code.  For example, to tell
the compiler that your code is C++, you might do:

 my build = Module::Build->new(
     module_name          => 'Spangly',
     extra_compiler_flags => ['-x', 'c++'],
 );

To link your XS code against glib you might write something like:

 my build = Module::Build->new(
     module_name          => 'Spangly',
     dynamic_config       => 1,
     extra_compiler_flags => `glib-config --cflags`,
     extra_linker_flags   => `glib-config --libs`,
 );

=item dist_author

This should be something like "John Doe <jdoe@example.com>".  This is
used when creating PPD files.  If this is not specified, then
C<Module::Build> looks at the module from which it gets the
distribution's version.  If it finds a POD section marked "=head1
AUTHOR", then it uses the contents of this section.

=item dist_abstract

This should be a short description of the distribution.  This is used
when creating PPD files.  If it is not given then C<Module::Build>
looks in the POD of the module from which it gets the distribution's
version.  It looks for the first line matching C<$package\s-\s(.+)>,
and uses the captured text as the abstract.

=item codebase

This can be either a single scalar string, or an array reference of
strings.  It is required when creating PPD files.  It should be a URL,
or URLs, to be used as the value for the C<< <CODEBASE> >> tag in the
generated PPD.

=back

=item create_build_script()

Creates an executable script called C<Build> in the current directory
that will be used to execute further user actions.  This script is
roughly analogous (in function, not in form) to the Makefile created
by C<ExtUtils::MakeMaker>.  This method also creates some temporary
data in a directory called C<_build/>.  Both of these will be removed
when the C<realclean> action is performed.

=item add_to_cleanup(@files)

You may call C<< $self->add_to_cleanup(@files) >>
to tell C<Module::Build> that certain files should be removed when the
user performs the C<Build clean> action.  I decided to provide a
dynamic method, rather than just use a static list of files, because these
static lists can get difficult to manage.  I usually prefer to keep
the responsibility for registering temporary files close to the code
that creates them.

=item resume()

You'll probably never call this method directly, it's only called from
the auto-generated C<Build> script.  The C<new()> method is only
called once, when the user runs C<perl Build.PL>.  Thereafter, when
the user runs C<Build test> or another action, the C<Module::Build>
object is created using the C<resume()> method to reinstantiate with
the settings given earlier to C<new()>.

=item current()

This method returns a reasonable faxsimile of the currently-executing
C<Module::Build> object representing the current build.  You can use
this object to query its C<notes()> method, inquire about installed
modules, and so on.  This is a great way to share information between
different parts of your building process.  For instance, you can ask
the user a question during C<perl Build.PL>, then use their answer
during a regression test:

 # In Build.PL:
 my $color = $build->prompt("What is your favorite color?");
 $build->notes(color => $color);
 
 # In t/colortest.t:
 use Module::Build;
 my $build = Module::Build->current;
 my $color = $build->notes('color');
 ...

The way the C<current()> method is currently implemented, there may be
slight differences between the C<$build> object in Build.PL and the
one in C<t/colortest.t>.  It is our goal to minimize these differences
in future releases of Module::Build, so please report any anomalies
you find.

=item notes()

=item notes($key)

=item notes($key => $value)

The C<notes()> value allows you to store your own persistent
information about the build, and to share that information among
different entities involved in the build.  See the example in the
C<current()> method.

The C<notes()> method is essentally a glorified hash access.  With no
arguments, C<notes()> returns a reference to the entire hash of notes.
With one argument, C<notes($key)> returns the value associated with
the given key.  With two arguments, C<notes($key, $value)> sets the
value associated with the given key to C<$value>.

=item dispatch($action, %args)

This method is also called from the auto-generated C<Build> script.
It parses the command-line arguments into an action and an argument
list, then calls the appropriate routine to handle the action.
Currently (though this may change), an action C<foo> will invoke the
C<ACTION_foo> method.  All arguments (including everything mentioned
in L<ACTIONS> below) are contained in the C<< $self->{args} >> hash
reference.

=item os_type()

If you're subclassing Module::Build and some code needs to alter its
behavior based on the current platform, you may only need to know
whether you're running on Windows, Unix, MacOS, VMS, etc. and not the
fine-grained value of Perl's C<$^O> variable.  The C<os_type()> method
will return a string like C<Windows>, C<Unix>, C<MacOS>, C<VMS>, or
whatever is appropriate.  If you're running on an unknown platform, it
will return C<undef> - there shouldn't be many unknown platforms
though.

=item prereq_failures()

Returns a data structure containing information about any failed
prerequisites (of any of the types described above), or C<undef> if
all prerequisites are met.

The data structure returned is a hash reference.  The top level keys
are the type of prerequisite failed, one of "requires",
"build_requires", "conflicts", or "recommends".  The associated values
are hash references whose keys are the names of required (or
conflicting) modules.  The associated values of those are hash
references indicating some information about the failure.  For example:

 {
  have => '0.42',
  need => '0.59',
  message => 'Version 0.42 is installed, but we need version 0.59',
 }

or

 {
  have => '<none>',
  need => '0.59',
  message => 'Prerequisite Foo isn't installed',
 }

This hash has the same structure as the hash returned by the
C<check_installed_status()> method, except that in the case of
"conflicts" dependencies we change the "need" key to "conflicts" and
construct a proper message.

Examples:

  # Check a required dependency on Foo::Bar
  if ( $m->prereq_failures->{requires}{Foo::Bar} ) { ...

  # Check whether there were any failures
  if ( $m->prereq_failures ) { ...
  
  # Show messages for all failures
  my $failures = $m->prereq_failures;
  while (my ($type, $list) = each %$failures) {
    while (my ($name, $hash) = each %$list) {
      print "Failure for $name: $hash->{message}\n";
    }
  }

=item check_installed_status($module, $version)

This method returns a hash reference indicating whether a version
dependency on a certain module is satisfied.  The C<$module> argument
is given as a string like C<"Data::Dumper"> or C<"perl">, and the
C<$version> argument can take any of the forms described in L<requires>
above.  This allows very fine-grained version checking.

The returned hash reference has the following structure:

 {
  ok => $whether_the_dependency_is_satisfied,
  have => $version_already_installed,
  need => $version_requested, # Same as incoming $version argument
  message => $informative_error_message,
 }

If no version of C<$module> is currently installed, the C<have> value
will be the string C<< "<none>" >>.  Otherwise the C<have> value will
simply be the version of the installed module.  Note that this means
that if C<$module> is installed but doesn't define a version number,
the C<have> value will be C<undef> - this is why we don't use C<undef>
for the case when C<$module> isn't installed at all.

This method may be called either as an object method
(C<< $build->check_installed_status($module, $version) >>)
or as a class method 
(C<< Module::Build->check_installed_status($module, $version) >>).

=item check_installed_version($module, $version)

Like C<check_installed_status()>, but simply returns true or false
depending on whether module C<$module> statisfies the dependency
C<$version>.

If the check succeeds, the return value is the actual version of
C<$module> installed on the system.  This allows you to do the
following:

 my $installed = $m->check_installed_version('DBI', '1.15');
 if ($installed) {
   print "Congratulations, version $installed of DBI is installed.\n";
 } else {
   die "Sorry, you must install DBI.\n";
 }

If the check fails, we return false and set C<$@> to an informative
error message.

If C<$version> is any nontrue value (notably zero) and any version of
C<$module> is installed, we return true.  In this case, if C<$module>
doesn't define a version, or if its version is zero, we return the
special value "0 but true", which is numerically zero, but logically
true.

In general you might prefer to use C<check_installed_status> if you
need detailed information, or this method if you just need a yes/no
answer.

=item prompt($message, $default)

Asks the user a question and returns their response as a string.  The
first argument specifies the message to display to the user (for
example, C<"Where do you keep your money?">).  The second argument,
which is optional, specifies a default answer (for example,
C<"wallet">).  The user will be asked the question once.

If the current session doesn't seem to be interactive (i.e. if
C<STDIN> and C<STDOUT> look like they're attached to files or
something, not terminals), we'll just use the default without
letting the user provide an answer.

This method may be called as a class or object method.

=item y_n($message, $default)

Asks the user a yes/no question using C<prompt()> and returns true or
false accordingly.  The user will be asked the question repeatedly
until they give an answer that looks like "yes" or "no".

The first argument specifies the message to display to the user (for
example, C<"Shall I invest your money for you?">), and the second
argument specifies the default answer (for example, C<"y">).

Note that the default is specified as a string like C<"y"> or C<"n">,
and the return value is a Perl boolean value like 1 or 0.  I thought
about this for a while and this seemed like the most useful way to do
it.

This method may be called as a class or object method.

=item script_files()

Returns an array reference specifying the perl script files to be
installed.  This corresponds to the C<script_files> parameter to the
C<new()> method.  With an optional argument, this parameter may be set
dynamically.

For backward compatibility, the C<scripts()> method does exactly the
same thing as C<script_files()>.  C<scripts()> is deprecated, but it
will stay around for several versions to give people time to
transition.

=item copy_if_modified(%parameters)

Takes the file in the C<from> parameter and copies it to the file in
the C<to> parameter, or the directory in the C<to_dir> parameter, if
the file has changed since it was last copied (or if it doesn't exist
in the new location).  By default the entire directory structure of
C<from> will be copied into C<to_dir>; an optional C<flatten>
parameter will copy into C<to_dir> without doing so.

Returns the path to the destination file, or C<undef> if nothing
needed to be copied.

Any directories that need to be created in order to perform the
copying will be automatically created.

=item base_dir()

Returns a string containing the root-level directory of this build,
i.e. where the C<Build.PL> script and the C<lib> directory can be
found.  This is usually the same as the current working directory,
because the C<Build> script will C<chdir()> into this directory as
soon as it begins execution.

=item up_to_date($source_file, $derived_file)

=item up_to_date(\@source_files, \@derived_files)

This method can be used to compare a set of source files to a set of
derived files.  If any of the source files are newer than any of the
derived files, it returns false.  Additionally, if any of the derived
files do not exist, it returns false.  Otherwise it returns true.

The arguments may be either a scalar or an array reference of file
names.

=item contains_pod($file)

Returns true if the given file appears to contain POD documentation.
Currently this checks whether the file has a line beginning with
'=pod', '=head', or '=item', but the exact semantics may change in the
future.

=back

=head1 ACTIONS

There are some general principles at work here.  First, each task when
building a module is called an "action".  These actions are listed
above; they correspond to the building, testing, installing,
packaging, etc. tasks.

Second, arguments are processed in a very systematic way.  Arguments
are always key=value pairs.  They may be specified at C<perl Build.PL>
time (i.e.  C<perl Build.PL destdir=/my/secret/place>), in which case
their values last for the lifetime of the C<Build> script.  They may
also be specified when executing a particular action (i.e.
C<Build test verbose=1>), in which case their values last only for the
lifetime of that command.  Per-action command-line parameters take
precedence over parameters specified at C<perl Build.PL> time.

The build process also relies heavily on the C<Config.pm> module, and
all the key=value pairs in C<Config.pm> are available in 

C<< $self->{config} >>.  If the user wishes to override any of the
values in C<Config.pm>, she may specify them like so:

  perl Build.PL config='cc=gcc ld=gcc'

Not the greatest interface, I'm looking for alternatives.  Speak now!
Maybe:

  perl Build.PL config=cc:gcc config=ld:gcc

or something.

The following build actions are provided by default.

=over 4

=item help

This action will simply print out a message that is meant to help you
use the build process.  It will show you a list of available build
actions too.

With an optional argument specifying an action name (e.g. C<Build help
test>), the 'help' action will show you any POD documentation it can
find for that action.

=item build

If you run the C<Build> script without any arguments, it runs the
C<build> action.

This is analogous to the MakeMaker 'make all' target.
By default it just creates a C<blib/> directory and copies any C<.pm>
and C<.pod> files from your C<lib/> directory into the C<blib/>
directory.  It also compiles any C<.xs> files from C<lib/> and places
them in C<blib/>.  Of course, you need a working C compiler
(probably the same one that built perl itself) for this to work
properly.

The C<build> action also runs any C<.PL> files in your F<lib/>
directory.  Typically these create other files, named the same but
without the C<.PL> ending.  For example, a file F<lib/Foo/Bar.pm.PL>
could create the file F<lib/Foo/Bar.pm>.  The C<.PL> files are
processed first, so any C<.pm> files (or other kinds that we deal
with) will get copied correctly.

If your C<.PL> scripts don't create any files, or if they create files
with unexpected names, or even if they create multiple files, you
should tell us that so that we can clean up properly after these
created files.  Use the C<PL_files> parameter to C<new()>:

 PL_files => { 'lib/Foo/Bar_pm.PL' => 'lib/Foo/Bar.pm',
               'lib/something.PL'  => ['/lib/something', '/lib/else'],
               'lib/funny.PL'      => [] }

Note that in contrast to MakeMaker, the C<build> action only
(currently) handles C<.pm>, C<.pod>, C<.PL>, and C<.xs> files.  They
must all be in the C<lib/> directory, in the directory structure that
they should have when installed.  We also handle C<.c> files that can
be in the place of your choosing - see the C<c_source> argument to
C<new()>.

The C<.xs> support is currently in alpha.  Please let me know whether
it works for you.

=item test

This will use C<Test::Harness> to run any regression tests and report
their results.  Tests can be defined in the standard places: a file
called C<test.pl> in the top-level directory, or several files ending
with C<.t> in a C<t/> directory.

If you want tests to be 'verbose', i.e. show details of test execution
rather than just summary information, pass the argument C<verbose=1>.

If you want to run tests under the perl debugger, pass the argument
C<debugger=1>.

In addition, if a file called C<visual.pl> exists in the top-level
directory, this file will be executed as a Perl script and its output
will be shown to the user.  This is a good place to put speed tests or
other tests that don't use the C<Test::Harness> format for output.

To override the choice of tests to run, you may pass a C<test_files>
argument whose value is a whitespace-separated list of test scripts to
run.  This is especially useful in development, when you only want to
run a single test to see whether you've squashed a certain bug yet:

 ./Build test --test_files t/something_failing.t

You may also pass several C<test_files> arguments separately:

 ./Build test --test_files t/one.t --test_files t/two.t

or use a C<glob()>-style pattern:

 ./Build test --test_files 't/01-*.t'


=item testdb

This is a synonym for the 'test' action with the C<debugger=1>
argument.

=item clean

This action will clean up any files that the build process may have
created, including the C<blib/> directory (but not including the
C<_build/> directory and the C<Build> script itself).

=item realclean

This action is just like the C<clean> action, but also removes the
C<_build> directory and the C<Build> script.  If you run the
C<realclean> action, you are essentially starting over, so you will
have to re-create the C<Build> script again.

=item diff

This action will compare the files about to be installed with their
installed counterparts.  For .pm and .pod files, a diff will be shown
(this currently requires a 'diff' program to be in your PATH).  For
other files like compiled binary files, we simply report whether they
differ.

A C<flags> parameter may be passed to the action, which will be passed
to the 'diff' program.  Consult your 'diff' documentation for the
parameters it will accept - a good one is C<-u>:

 ./Build diff flags=-u

=item install

This action will use C<ExtUtils::Install> to install the files from
C<blib/> into the system.  See L<How Installation Paths are Determined> for details
about how Module::Build determines where to install things, and how to
influence this process.

If you want the installation process to look around in C<@INC> for
other versions of the stuff you're installing and try to delete it,
you can use the C<uninst> parameter, which tells C<Module::Install> to
do so:

 Build install uninst=1

This can be a good idea, as it helps prevent multiple versions of a
module from being present on your system, which can be a confusing
situation indeed.



=item fakeinstall

This is just like the C<install> action, but it won't actually do
anything, it will just report what it I<would> have done if you had
actually run the C<install> action.

=item versioninstall

** Note: since C<only.pm> is so new, and since we just recently added
support for it here too, this feature is to be considered
experimental. **

If you have the C<only.pm> module installed on your system, you can
use this action to install a module into the version-specific library
trees. This means that you can have several versions of the same
module installed and C<use> a specific one like this:

 use only MyModule => 0.55;

To override the default installation libraries in C<only::config>,
specify the C<versionlib> parameter when you run the C<Build.PL> script:

 perl Build.PL versionlib=/my/version/place/

To override which version the module is installed as, specify the
C<versionlib> parameter when you run the C<Build.PL> script:

 perl Build.PL version=0.50

See the C<only.pm> documentation for more information on
version-specific installs.

=item manifest

This is an action intended for use by module authors, not people
installing modules.  It will bring the F<MANIFEST> up to date with the
files currently present in the distribution.  You may use a
F<MANIFEST.SKIP> file to exclude certain files or directories from
inclusion in the F<MANIFEST>.  F<MANIFEST.SKIP> should contain a bunch
of regular expressions, one per line.  If a file in the distribution
directory matches any of the regular expressions, it won't be included
in the F<MANIFEST>.

The following is a reasonable F<MANIFEST.SKIP> starting point, you can
add your own stuff to it:

   ^_build
   ^Build$
   ^blib
   ~$
   \.bak$
   ^MANIFEST\.SKIP$
   CVS

See the L<distcheck> and L<skipcheck> actions if you want to find out
what the C<manifest> action would do, without actually doing anything.

=item dist

This action is helpful for module authors who want to package up their
module for distribution through a medium like CPAN.  It will create a
tarball of the files listed in F<MANIFEST> and compress the tarball using
GZIP compression.

=item distsign

Uses C<Module::Signature> to create a SIGNATURE file for your
distribution.

=item distmeta

Creates the F<META.yml> file for your distribution.

=item distcheck

Reports which files are in the build directory but not in the
F<MANIFEST> file, and vice versa. (See L<manifest> for details)

=item skipcheck

Reports which files are skipped due to the entries in the
F<MANIFEST.SKIP> file (See L<manifest> for details)

=item distclean

Performs the 'realclean' action and then the 'distcheck' action.

=item distdir

Creates a directory called C<$(DISTNAME)-$(VERSION)> (if that
directory already exists, it will be removed first).  Then copies all
the files listed in the F<MANIFEST> file to that directory.  This
directory is what people will see when they download your distribution
and unpack it.

While performing the 'distdir' action, a file containing various bits
of "metadata" will be created.  The metadata includes the module's
name, version, dependencies, license, and the C<dynamic_config>
flag.  This file is created as F<META.yml> in YAML format, so you
must have the C<YAML> module installed in order to create it.  You
should also ensure that the F<META.yml> file is listed in your
F<MANIFEST> - if it's not, a warning will be issued.

=item disttest

Performs the 'distdir' action, then switches into that directory and
runs a C<perl Build.PL>, followed by the 'build' and 'test' actions in
that directory.

=item ppd

Build a PPD file for your distribution.

=back

=head2 How Installation Paths are Determined

When you invoke Module::Build's C<build> action, it needs to figure
out where to install things.  The nutshell version of how this works
is that default installation locations are determined from
F<Config.pm>, and they may be overridden by using the C<install_path>
parameter.  An C<install_base> parameter lets you specify an
alternative installation root like F</home/foo>, and a C<destdir> lets
you specify a temporary installation directory like F</tmp/install> in
case you want to create bundled-up installable packages.

Natively, Module::Build provides default installation locations for
the following types of installable items:

=over 4

=item lib

Usually pure-Perl module files ending in F<.pm>.

=item arch

"Architecture-dependent" module files, usually produced by compiling
XS, Inline, or similar code.

=item script

Programs written in pure Perl.  In order to improve reuse, try to make
these as small as possible - put the code into modules whenever
possible.

=item bin

"Architecture-dependent" executable programs, i.e. compiled C code or
something.  Pretty rare to see this in a perl distribution, but it
happens.

=item libdoc

Documentation for the stuff in C<lib> and C<arch>.  This is usually
generated from the POD in F<.pm> files.  Under Unix, these are manual
pages belonging to the 'man3' category.

=item bindoc

Documentation for the stuff in C<script> and C<bin>.  Usually
generated from the POD in those files.  Under Unix, these are manual
pages belonging to the 'man1' category.

=back

=head3 installdirs

The default destinations for these installable things come from
entries in your system's C<Config.pm>.  You can select from three
different sets of default locations by setting the C<installdirs>
parameter as follows:

                          'installdirs' set to:
                   core          site                vendor
 
              uses the following defaults from Config.pm:
 
 lib     => installprivlib  installsitelib      installvendorlib
 arch    => installarchlib  installsitearch     installvendorarch
 script  => installscript   installsitebin      installvendorbin
 bin     => installbin      installsitebin      installvendorbin
 libdoc  => installman3dir  installsiteman3dir  installvendorman3dir
 bindoc  => installman1dir  installsiteman1dir  installvendorman1dir

The default value of C<installdirs> is "site".  If you're creating
vendor distributions of module packages, you may want to do something
like this:

 perl Build.PL installdirs=vendor

or

 Build install installdirs=vendor

If you're installing an updated version of a module that was included
with perl itself (i.e. a "core module"), then you may set
C<installdirs> to "core" to overwrite the module in its present
location.

(Note that the 'script' line is different from MakeMaker -
unfortunately there's no such thing as "installsitescript" or
"installvendorscript" entry in C<Config.pm>, so we use the
"installsitebin" and "installvendorbin" entries to at least get the
general location right.  In the future, if C<Config.pm> adds some more
appropriate entries, we'll start using those.)

=head3 install_path

Once the defaults have been set, you can override them.  You can set
individual entries by using the C<install_path> parameter:

 my $m = Module::Build->new
  (...other options...,
   install_path => {lib  => '/foo/lib',
                    arch => '/foo/lib/arch'});

On the command line, that would look like this:

 perl Build.PL install_path=lib=/foo/lib install_path=arch=/foo/lib/arch

or this:

 Build install install_path=lib=/foo/lib install_path=arch=/foo/lib/arch

=head3 install_base

You can also set the whole bunch of installation paths by supplying the
C<install_base> parameter to point to a directory on your system.  For
instance, if you set C<install_base> to "/home/ken" on a Linux
system, you'll install as follows:

 lib     => /home/ken/lib
 arch    => /home/ken/lib/i386-linux
 script  => /home/ken/scripts
 bin     => /home/ken/bin
 libdoc  => /home/ken/man/man1
 bindoc  => /home/ken/man/man3

Note that this is I<different> from how MakeMaker's C<PREFIX>
parameter works.  C<PREFIX> tries to create a mini-replica of a
C<site>-style installation under the directory you specify, which is
not always possible (and the results are not always pretty in this
case).  C<install_base> just gives you a default layout under the
directory you specify, which may have little to do with the
C<installdirs=site> layout.

The exact layout under the directory you specify may vary by system -
we try to do the "sensible" thing on each platform.

=head3 destdir

If you want to install everything into a temporary directory first
(for instance, if you want to create a directory tree that a package
manager like C<rpm> or C<dpkg> could create a package from), you can
use the C<destdir> parameter:

 perl Build.PL destdir=/tmp/foo

or

 Build install destdir=/tmp/foo

This will effectively install to "/tmp/foo/$sitelib",
"/tmp/foo/$sitearch", and the like, except that it will use
C<File::Spec> to make the pathnames work correctly on whatever
platform you're installing on.


=head1 AUTOMATION

One advantage of Module::Build is that since it's implemented as Perl
methods, you can invoke these methods directly if you want to install
a module non-interactively.  For instance, the following Perl script
will invoke the entire build/install procedure:

 my $m = Module::Build->new(module_name => 'MyModule');
 $m->dispatch('build');
 $m->dispatch('test');
 $m->dispatch('install');

If any of these steps encounters an error, it will throw a fatal
exception.

You can also pass arguments as part of the build process:

 my $m = Module::Build->new(module_name => 'MyModule');
 $m->dispatch('build');
 $m->dispatch('test', verbose => 1);
 $m->dispatch('install', sitelib => '/my/secret/place/');

Building and installing modules in this way skips creating the
C<Build> script.

=head1 STRUCTURE

Module::Build creates a class hierarchy conducive to customization.
Here is the parent-child class hierarchy in classy ASCII art:

   /--------------------\
   |   Your::Parent     |  (If you subclass Module::Build)
   \--------------------/
            |
            |
   /--------------------\  (Doesn't define any functionality
   |   Module::Build    |   of its own - just figures out what
   \--------------------/   other modules to load.)
            |
            |
   /-----------------------------------\  (Some values of $^O may
   |   Module::Build::Platform::$^O    |   define specialized functionality.
   \-----------------------------------/   Otherwise it's ...::Default, a
            |                              pass-through class.)
            |
   /--------------------------\
   |   Module::Build::Base    |  (Most of the functionality of 
   \--------------------------/   Module::Build is defined here.)

=head1 SUBCLASSING

Right now, there are two ways to subclass Module::Build.  The first
way is to create a regular module (in a C<.pm> file) that inherits
from Module::Build, and use that module's class instead of using
Module::Build directly:

  ------ in Build.PL: ----------
  #!/usr/bin/perl
  
  use lib qw(/nonstandard/library/path);
  use My::Builder;  # Or whatever you want to call it
  
  my $m = My::Builder->new(module_name => 'Next::Big::Thing');
  $m->create_build_script;

This is relatively straightforward, and is the best way to do things
if your My::Builder class contains lots of code.  The
C<create_build_script()> method will ensure that the current value of
C<@INC> (including the C</nonstandard/library/path>) is propogated to
the Build script, so that My::Builder can be found when running build
actions.

For very small additions, Module::Build provides a C<subclass()>
method that lets you subclass Module::Build more conveniently, without
creating a separate file for your module:

  ------ in Build.PL: ----------
  #!/usr/bin/perl
  
  my $class = Module::Build->subclass
    (
     class => 'My::Builder',
     code => q{
      sub ACTION_foo {
        print "I'm fooing to death!\n";
      }
     },
    );
  
  my $m = $class->new(module_name => 'Module::Build');
  $m->create_build_script;

Behind the scenes, this actually does create a C<.pm> file, since the
code you provide must persist after Build.PL is run if it is to be
very useful.


=head1 MOTIVATIONS

There are several reasons I wanted to start over, and not just fix
what I didn't like about MakeMaker:

=over 4

=item *

I don't like the core idea of MakeMaker, namely that C<make> should be
involved in the build process.  Here are my reasons:

=over 4

=item +

When a person is installing a Perl module, what can you assume about
their environment?  Can you assume they have C<make>?  No, but you can
assume they have some version of Perl.

=item +

When a person is writing a Perl module for intended distribution, can
you assume that they know how to build a Makefile, so they can
customize their build process?  No, but you can assume they know Perl,
and could customize that way.

=back

For years, these things have been a barrier to people getting the
build/install process to do what they want.

=item *

There are several architectural decisions in MakeMaker that make it
very difficult to customize its behavior.  For instance, when using
MakeMaker you do C<use MakeMaker>, but the object created in
C<WriteMakefile()> is actually blessed into a package name that's
created on the fly, so you can't simply subclass
C<ExtUtils::MakeMaker>.  There is a workaround C<MY> package that lets
you override certain MakeMaker methods, but only certain explicitly
predefined (by MakeMaker) methods can be overridden.  Also, the method
of customization is very crude: you have to modify a string containing
the Makefile text for the particular target.  Since these strings
aren't documented, and I<can't> be documented (they take on different
values depending on the platform, version of perl, version of
MakeMaker, etc.), you have no guarantee that your modifications will
work on someone else's machine or after an upgrade of MakeMaker or
perl.

=item *

It is risky to make major changes to MakeMaker, since it does so many
things, is so important, and generally works.  C<Module::Build> is an
entirely seperate package so that I can work on it all I want, without
worrying about backward compatibility.

=item *

Finally, Perl is said to be a language for system administration.
Could it really be the case that Perl isn't up to the task of building
and installing software?  Even if that software is a bunch of stupid
little C<.pm> files that just need to be copied from one place to
another?  Are you getting riled up yet??

=back


=head1 MIGRATION

Note that if you want to provide both a F<Makefile.PL> and a
F<Build.PL> for your distribution, you probably want to add the
following to C<WriteMakefile> in your F<Makefile.PL> so that MakeMaker
doesn't try to run your F<Build.PL> as a normal F<.PL> file:

 PL_FILES => {},

You may also be interested in looking at the C<Module::Build::Compat>
module, which can automatically create various kinds of F<Makefile.PL>
compatibility layers.

=head1 TO DO

The current method of relying on time stamps to determine whether a
derived file is out of date isn't likely to scale well, since it
requires tracing all dependencies backward, it runs into problems on
NFS, and it's just generally flimsy.  It would be better to use an MD5
signature or the like, if available.  See C<cons> for an example.

- append to perllocal.pod
- write .packlist in appropriate location (needed for un-install)

=head1 AUTHOR

Ken Williams, ken@mathforum.org

Development questions, bug reports, and patches should be sent to the
Module-Build mailing list at module-build-general@lists.sourceforge.net .
Bug reports are also welcome at
http://rt.cpan.org/NoAuth/Bugs.html?Dist=Module-Build .

An anonymous CVS repository containing the latest development version
is available; see http://sourceforge.net/cvs/?group_id=45731 for the
details of how to access it.

=head1 SEE ALSO

perl(1), ExtUtils::MakeMaker(3), YAML(3)

http://www.dsmit.com/cons/

=cut
