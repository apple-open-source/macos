#line 1
# SEE DOCUMENTATION AT BOTTOM OF FILE


#------------------------------------------------------------
package IO::WrapTie;
#------------------------------------------------------------
require 5.004;              ### for tie
use strict;
use vars qw(@ISA @EXPORT $VERSION);
use Exporter;

# Inheritance, exporting, and package version:
@ISA     = qw(Exporter);
@EXPORT  = qw(wraptie);
$VERSION = "2.110";

# Function, exported.
sub wraptie {
    IO::WrapTie::Master->new(@_);
}

# Class method; BACKWARDS-COMPATIBILITY ONLY!
sub new { 
    shift; 
    IO::WrapTie::Master->new(@_);
}



#------------------------------------------------------------
package IO::WrapTie::Master;
#------------------------------------------------------------

use strict;
use vars qw(@ISA $AUTOLOAD);
use IO::Handle;

# We inherit from IO::Handle to get methods which invoke i/o operators,
# like print(), on our tied handle:
@ISA = qw(IO::Handle);

#------------------------------
# new SLAVE, TIEARGS...
#------------------------------
# Create a new subclass of IO::Handle which...
#
#   (1) Handles i/o OPERATORS because it is tied to an instance of 
#       an i/o-like class, like IO::Scalar.
#
#   (2) Handles i/o METHODS by delegating them to that same tied object!.
#
# Arguments are the slave class (e.g., IO::Scalar), followed by all 
# the arguments normally sent into that class's TIEHANDLE method.
# In other words, much like the arguments to tie().  :-)
#
# NOTE:
# The thing $x we return must be a BLESSED REF, for ($x->print()).
# The underlying symbol must be a FILEHANDLE, for (print $x "foo").
# It has to have a way of getting to the "real" back-end object...
#
sub new {
    my $master = shift;
    my $io = IO::Handle->new;   ### create a new handle
    my $slave = shift;
    tie *$io, $slave, @_;       ### tie: will invoke slave's TIEHANDLE
    bless $io, $master;         ### return a master
}

#------------------------------
# AUTOLOAD
#------------------------------
# Delegate method invocations on the master to the underlying slave.
#
sub AUTOLOAD {
    my $method = $AUTOLOAD;
    $method =~ s/.*:://;
    my $self = shift; tied(*$self)->$method(\@_);
}

#------------------------------
# PRELOAD
#------------------------------
# Utility.
#
# Most methods like print(), getline(), etc. which work on the tied object 
# via Perl's i/o operators (like 'print') are inherited from IO::Handle.
#
# Other methods, like seek() and sref(), we must delegate ourselves.
# AUTOLOAD takes care of these.
#
# However, it may be necessary to preload delegators into your
# own class.  PRELOAD will do this.
#
sub PRELOAD {
    my $class = shift;
    foreach (@_) {
	eval "sub ${class}::$_ { my \$s = shift; tied(*\$s)->$_(\@_) }";
    }    
}

# Preload delegators for some standard methods which we can't simply
# inherit from IO::Handle... for example, some IO::Handle methods 
# assume that there is an underlying file descriptor.
#
PRELOAD IO::WrapTie::Master 
    qw(open opened close read clearerr eof seek tell setpos getpos);



#------------------------------------------------------------
package IO::WrapTie::Slave;
#------------------------------------------------------------
# Teeny private class providing a new_tie constructor...
#
# HOW IT ALL WORKS:
# 
# Slaves inherit from this class.
#
# When you send a new_tie() message to a tie-slave class (like IO::Scalar),
# it first determines what class should provide its master, via TIE_MASTER.
# In this case, IO::Scalar->TIE_MASTER would return IO::Scalar::Master.
# Then, we create a new master (an IO::Scalar::Master) with the same args
# sent to new_tie.
#
# In general, the new() method of the master is inherited directly 
# from IO::WrapTie::Master.
#
sub new_tie {
    my $self = shift;
    $self->TIE_MASTER->new($self,@_);     ### e.g., IO::Scalar::Master->new(@_)
}

# Default class method for new_tie().
# All your tie-slave class (like IO::Scalar) has to do is override this 
# method with a method that returns the name of an appropriate "master"
# class for tying that slave.
#
sub TIE_MASTER { 'IO::WrapTie::Master' }

#------------------------------
1;
__END__


package IO::WrapTie;      ### for doc generator


#line 490

