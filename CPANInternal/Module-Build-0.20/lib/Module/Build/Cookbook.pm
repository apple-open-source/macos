package Module::Build::Cookbook;

=head1 NAME

Module::Build::Cookbook - Examples of Module::Build Usage

=head1 DESCRIPTION

C<Module::Build> isn't conceptually very complicated, but examples are
always helpful.  I got the idea for writing this cookbook when
attending Brian Ingerson's "Extreme Programming Tools for Module
Authors" presentation at YAPC 2003, when he said, straightforwardly,
"Write A Cookbook."

The definitional of how stuff works is in the main C<Module::Build>
documentation.  It's best to get familiar with that too.

=head1 BASIC RECIPES

=head2 The basic installation recipe for modules that use Module::Build

In most cases, you can just issue the following commands from your
shell:

 perl Build.PL
 Build
 Build test
 Build install

There's nothing complicated here - first you're running a script
called F<Build.PL>, then you're running a (newly-generated) script
called F<Build> and passing it various arguments.  If you know how to
do that on your system, you can get installation working.

The exact commands may vary a bit depending on how you invoke perl
scripts on your system.  For instance, if you have multiple versions
of perl installed, you can install to one particular perl's library
directories like so:

 /usr/bin/perl5.8.1 Build.PL
 Build
 Build test
 Build install

The F<Build> script knows what perl was used to run C<Build.PL>, so
you don't need to reinvoke the F<Build> script with the complete perl
path each time.  If you invoke it with the I<wrong> perl path, you'll
get a warning.

If the current directory (usually called '.') isn't in your path, you
can do C<./Build> or C<perl Build> to run the script:

 /usr/bin/perl Build.PL
 ./Build
 ./Build test
 ./Build install


=head2 Installing modules using the programmatic interface

If you need to build, test, and/or install modules from within some
other perl code (as opposed to having the user type installation
commands at the shell), you can use the programmatic interface.
Create a Module::Build object (or an object of a custom Module::Build
subclass) and then invoke its C<dispatch()> method to run various
actions.

 my $b = Module::Build->new(
   module_name => 'Foo::Bar',
   license => 'perl',
   requires => { 'Some::Module'   => '1.23' },
 );
 $b->dispatch('build');
 $b->dispatch('test', verbose => 1);
 $b->dispatch('install);

The first argument to C<dispatch()> is the name of the action, and any
following arguments are named parameters.

This is the interface we use to test Module::Build itself in the
regression tests.

=head2 Installing to a temporary directory

To create packages for package managers like RedHat's C<rpm> or
Debian's C<deb>, you may need to install to a temporary directory
first and then create the package from that temporary installation.
To do this, specify the C<destdir> parameter to the C<install> action:

 Build install destdir=/tmp/my-package-1.003


=head1 AUTHOR

Ken Williams, ken@mathforum.org

=head1 SEE ALSO

perl(1), Module::Build(3)

=cut
