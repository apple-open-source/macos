
# = HISTORY SECTION =====================================================================

# ---------------------------------------------------------------------------------------
# version | date   | author   | changes
# ---------------------------------------------------------------------------------------
# 2.03    |29.02.00| JSTENZEL | corrected perl version demand;
#         |        | JSTENZEL | slight POD and comment improvements;
# 2.02    |06.01.00| JSTENZEL | started translation of POD;
#         |        | JSTENZEL | replaced bug() by Carp::confess();
#         |        | JSTENZEL | integrated inhouse modules and funtions (like filters);
#         |15.01.00| JSTENZEL | improved source filters;
# 2.01    |        | JSTENZEL | ;
# 2.00    |25.10.99| JSTENZEL | added the delayed sending feature;
#         |26.10.99| JSTENZEL | formatted POD to be better prepared for pod2text;
#         |        | JSTENZEL | added traces;
# ---------------------------------------------------------------------------------------
# 1.05    |30.09.99| JSTENZEL | extended traces;
# 1.04    |15.09.99| JSTENZEL | documented noAssert;
# 1.03    |10.09.99| JSTENZEL | improvements to avoid warnings;
#         |        | JSTENZEL | constructor now checks if the passed handle is open;
#         |        | JSTENZEL | all methods now check if the the handle is still opened;
#         |        | JSTENZEL | improved POD formatting;
#         |        | JSTENZEL | message fix;
# 1.02    |09.09.99| JSTENZEL | comment fixes;
#         |        | JSTENZEL | better fcntl() calls (saving original flags really),
#         |        |          | inspired by code pieces in "Advanced Perl Programming";
#         |        | JSTENZEL | non blocking mode is now activated only during transfers;
#         |        | JSTENZEL | modified some traces;
# 1.01    |09.09.99| JSTENZEL | adding initial select() calls to avoid obstipation trouble;
#         |        | JSTENZEL | using IO::Select now;
#         |        | JSTENZEL | the waiting select() call to complete incomplete transfers
#         |        |          | now waits SPECIFICALLY on the socket AND with timeout -
#         |        |          | so if the socket is ready before the timeout period ends,
#         |        |          | the process is accelerated now (select() returns earlier);
#         |        | JSTENZEL | SIGPIPE is now temporarily disabled in send() and receive()
#         |        |          | to avoid trouble with sockets closing during a transfer;
# 1.00    |07.09.99| JSTENZEL | new, derived from Admin::IO::common;
# ---------------------------------------------------------------------------------------

# = POD SECTION =========================================================================

=head1 NAME

B<IPC::LDT> - implements a length based IPC protocol

=head1 SCRIPT DATA

This manual describes version B<2.03>.

=head1 DESCRIPTION

Interprocess communication often uses line (or record) oriented protocols. FTP,
for example, usually is such a protocol: a client sends a command (e.g. "LS") which is
completed by a carriage return. This carriage return is included in the command
which is sent to the server process (FTP deamon) which could implement its reading
in a way like this:

  while ($cmd=<CLIENT>)
   {
    chomp($cmd);
    performCommand($cmd);
   }

Well, such record oriented, blocked protocols are very useful and simply to implement,
but sometimes there is a need to transfer more complex data which has no trailing carriage
return, or data which may include more carriage returns inside the message which should
not cause the reciepient to think the message is already complete while it is really not.
Even if you choose to replace carriage returns by some obscure delimiters, the same could
happen again until you switch to a protocol which does not flag the end of a message by
special strings.

On the other hand, if there is no final carriage return (or whatever flag string) within
a message, the end of the message has to be marked another way to avoid blocking by endless
waiting for more message parts. A simple way to provide this is to precede a message by a
prefix which includes the length of the remaining (real) message. A reciepient reads this
prefix, decodes the length information and continues reading until the announced number of
bytes came in.

B<IPC::LDT> provides a class to build objects which transparently perform such "I<l>ength
I<d>riven I<t>ransfer". A user sends and receives messages by simple method calls, while
the LDT objects perform the complete translation into and from LDT messages (with prefix)
and all the necessary low level IO handling to transfer stream messages on non blocked handles.

B<IPC::LDT> objects can be configured to transfer simle string messages as well as complex
data structures. Additionally, they allow to delay the transfer of certain messages in a user
defined way.

=head1 SYNOPSIS

Load the module as usual:

  use IPC::LDT;

Make an LDT object for every handle that should be used in an LDT communication:

  my $asciiClient=new IPC::LDT(handle=>HANDLE);
  my $objectClient=new IPC::LDT(handle=>HANDLE, objectMode=>1);

Now you can send and receive data:

  $data=$asciiClient->receive;
  @objects=$objectClient->receive;

B<>

  $asciiClient=$client->send("This is", " a message.");
  $objectClient=$client->send("These are data:", [qw(a b c)]);

=cut

# check perl version
require 5.00503;

# = PACKAGE SECTION (internal helper packages) ==========================================

# declare package
package IPC::LDT::Filter::MeTrace;

# declare package version
$VERSION=$VERSION=1.00;

# set pragmas
use strict;

# load CPAN modules
use Filter::Util::Call;

# The main function - see the Filter::Util::Call manual for details.
# I'm using the closure variant here. It's shorter.
sub import
 {
  # get parameter
  my ($self)=@_;

  # define and register the filter
  filter_add(
	     sub
	      {
	       # get parameter
	       my ($self)=@_;

	       # declare variable
	       my ($status);

	       # remove trace code ...
	       s/\$me->trace\(.+?\);//g if ($status=filter_read())>0;
	       
	       # reply state
	       $status;
	      }
	    );
 }

# reply a true value to flag successfull init
1;

# reset pragmas;
no strict;

# declare package
package IPC::LDT::Filter::Assert;

# declare package version
$VERSION=$VERSION=1.00;

# set pragmas
use strict;

# load CPAN modules
use Filter::Util::Call;

# The main function - see the Filter::Util::Call manual for details.
# I'm using the closure variant here. It's shorter.
sub import
 {
  # get parameter
  my ($self, $noAssert)=@_;

  # define and register the filter
  filter_add(
	     sub
	      {
	       # get parameter
	       my ($self)=@_;

	       # declare variable
	       my ($status);

	       # remove trace code ...
               if (($status=filter_read())>0)
                 {
                  if ($noAssert)
                    {s/bug\(.+?\)[^;]*?;//g;}
                  else
                    {s/bug\((['"])/confess\($1\[BUG\] /g;}
                 }
	       
	       # reply state
	       $status;
	      }
	    );
 }

# reply a true value to flag successfull init
1;

# reset pragmas
no strict;

# = PACKAGE SECTION ======================================================================

# declare package
package IPC::LDT;

# filters
BEGIN
 {
  # deactivate compiler checks
  no strict 'refs';

  # trace filter (first line to avoid useless warnings)
  defined ${join('::', __PACKAGE__, 'Trace')} ? 1 : 1;
  IPC::LDT::Filter::MeTrace::import() unless ${join('::', __PACKAGE__, 'Trace')};

  # assertion filter (first line to avoid useless warnings)
  defined ${join('::', __PACKAGE__, 'noAssert')} ? 1 : 1;
  IPC::LDT::Filter::Assert::import(${join('::', __PACKAGE__, 'noAssert')});
 }

use Exporter ();
@ISA=qw(Exporter);

# declare package version
$VERSION=2.03;

# declare fields
use fields qw(
              delayFilter
              delayQueue
              fileno
              handle
              msg
              objectMode
              rc
              select
              startblockLength
              traceMode
             );

=pod

=head2 Exports

No symbol is exported by default.

You can explicitly import LDT_CLOSED, LDT_READ_INCOMPLETE, LDT_WRITE_INCOMPLETE,
LDT_OK and LDT_INFO_LENGTH which are described in section I<CONSTANTS>.

=cut

# declare exporter modules
@EXPORT=qw();
@EXPORT_OK=qw(
              LDT_CLOSED
              LDT_INFO_LENGTH
              LDT_OK
              LDT_READ_INCOMPLETE
              LDT_WRITE_INCOMPLETE
             );

# = PRAGMA SECTION =======================================================================

# set pragmas
use strict;

# = LIBRARY SECTION ======================================================================

# load modules
use Carp;                               # message handling;
use POSIX;                      
use Storable;		      	        # data serialization;
use IO::Select;                         # a select() wrapper;

# = CODE SECTION =========================================================================

# exportable constants
use constant LDT_INFO_LENGTH=>8;        # length of a handle message length string;

# internal constants
use constant HANDLE_RETRY_COUNT=>100;	# number of trials to complete a message from a handle;
use constant HANDLE_RETRY_DELAY=>0.2;	# number of seconds until a new attempt to complete a reading;

=pod

=head1 Global Variables

=head2 Settings

=over 4

=item Traces

You may set the module variable B<$IPC::LDT::Trace> I<before> the module
is loaded (that means in a I<BEGIN> block before the "use" statement) to
activate the built in trace code. If not prepared this way, all runtime
trace settings (e.g. via the constructor parameter I<traceMode>) will take
I<no effect> because the trace code will have been filtered out at compile
time for reasons of performance. (This means that no trace message will
appear.)

I<If> B<$IPC::LDT::Trace> is set before the module is loaded, te builtin
trace code is active and can be deactivated or reactivated at runtime
globally (for all objects of this class) by unsetting or resetting of
this module variable. Alternatively, you may choose to control traces
for certain objects by using the constructor parameter I<traceMode>.

So, if you want to trace every object, set B<$IPC::LDT::Trace> initially
and load the module. If you want to trace only certain objects, additionally
unset B<$IPC::LDT::Trace> after the module is loaded and construct these
certain objects with constructor flag I<traceMode>.

=item Assertions

It is a good tradition to build self checks into a code. This makes
code execution more secure and simplifies bug searching after a failure.
On the other hand, self checks decrease code performance. That's why
you can filter out the self checking code (which is built in and activated
by default) by setting the module variable B<$IPC::LDT::noAssert> I<before>
the module is loaded. The checks will be removed from the code before
they reach the compiler.

Setting or unsetting this variable after the module was loaded takes
I<no effect>.

=back

=head1 CONSTANTS

=head2 Error codes

=over 4

=item LDT_CLOSED

a handle related to an LDT object was closed when reading or writing
should be performed on it;

=item LDT_READ_INCOMPLETE

a message could not be (completely) read within the set number of
trials;

=item LDT_WRITE_INCOMPLETE

a message could not be (completely) written within the set number of
trials;

=back

=cut

# error constants - these are made public (but not exported by default)
use constant LDT_OK              =>100;	# all right;
use constant LDT_CLOSED          =>-1;	# the handle was closed while it should be read;
use constant LDT_READ_INCOMPLETE =>-2;	# a handle message could not be read completely;
use constant LDT_WRITE_INCOMPLETE=>-3;	# a handle message could not be read completely;

=pod

=head1 METHODS

=cut


# -------------------------------------------------------------------
=pod

=head2 new()

The constructor builds a new object for data transfers. All parameters
except of the class name are passed named (this means, by a hash).

B<Parameters:>

=over 4

=item Class name

the first parameter as usual - passed implicitly by Perl:

  my $asciiClient=new IPC::LDT(...);

The method form of construtor calls is not supported.

=item handle

The handle to be used to perform the communication. It has to be opened
already and will not be closed if the object will be destroyed.

 Example:
  handle => SERVER

A closed handle is I<not> accepted.

You can use whatever type of handle meets your needs. Usually it is a socket
or anything derived from a socket. For example, if you want to perform secure
IPC, the handle could be made by Net::SSL. There is only one precondition:
the handle has to provide a B<fileno()> method. (You can enorce this even for
Perls default handles by simply using B<FileHandle>.)

=item objectMode

Pass a true value if you want to transfer data structures. If this
setting is missed or a "false" value is passed, the object will transfer
strings.

Data structures will be serialized via I<Storable> for transfer. Because
of this, such a communication is usually restricted to partners which could
use I<Storable> methods as well to reconstruct the data structures (which
means that they are written in Perl).

String transfer objects, on the other hand, can be used to cimmunicate with
any partner who speaks the LDT protocol. We use Java and C clients as well
as Perl ones, for example.

 Example:
  objectMode => 1

The transfer mode may be changed while the object is alive by using the
methods I<setObjectMode()> and I<setAsciiMode()>.

=item startblockLength

sets the length of the initial info block which preceds every LDT
message coding the length of the remaining message. This setting is
done in bytes.

If no value is provided, the builtin default value I<LDT_INFO_LENGTH>
is used. (This value can be imported in your own code, see section
"I<Exports>" for details.) I<LDT_INFO_LENGTH> is designed to meet
usual needs.

 Example:
  startblockLength => 4

=item traceMode

Set this flag to a true value if you want to trace to actions of the
module. If set, messages will be displayed on STDERR reporting what
is going on.

Traces for <all> objects of this class can be activated (regardless of
this constructor parameter) via I<$IPC::LDT::Trace>. This is described
more detailed in section "I<CONSTANTS>".

 Example:
  traceMode => 1

=back

B<Return value:>

A successfull constructor call replies the new object. A failed call
replies an undefined value.

B<Examples:>

  my $asciiClient=new IPC::LDT(handle=>HANDLE);
  my $objectClient=new IPC::LDT(handle=>HANDLE, objectMode=>1);

=cut
# -------------------------------------------------------------------
sub new
 {
  # get parameters
  bug("Number of parameters should be even") unless @_ % 2;
  my ($class, %switches)=@_;

  # and check them
  bug("Missing class name parameter") unless $class;
  bug("Constructor called as method, use copy() method instead") if ref($class);
  bug("Missing handle parameter") unless exists $switches{'handle'} and $switches{'handle'};

  # declare function variables
  my ($me);

  # make new object
  {
   no strict 'refs';
   $me=bless([\%{"$class\::FIELDS"}], $class);
  }

  # check the handle for being valid and open
  if (defined $switches{'handle'}->fileno)
    {
     # build and init the object
     $me->{'handle'}=$switches{'handle'};
     $me->{'fileno'}=$me->{'handle'}->fileno;
     $me->{'msg'}=$me->{'rc'}='';
     $me->{'objectMode'}=(exists $switches{'objectMode'} and $switches{'objectMode'}) ? 1 : 0;
     $me->{'startblockLength'}=(exists $switches{'startblockLength'} and $switches{'startblockLength'}>0) ? $switches{'startblockLength'} : LDT_INFO_LENGTH;
     $me->{'traceMode'}=(exists $switches{'trace'} and $switches{'trace'}) ? 1: 0;
     $me->{'select'}=new IO::Select($me->{'handle'});

     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: object is made.");

     # reply the new object
     return $me;
    }
  else
    {
     # invalid or closed handle passed
     return undef;
    }
 }


# internal method
sub DESTROY
 {
  # get and check parameters
  my ($me)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;

  # get fileno (and handle status this way)
  my $fileno=$me->{'handle'}->fileno;

  # trace, if necessary
  $me->trace("LDT ${\($fileno?$fileno:qq(with closed handle, was $me->{'fileno'}))}: object dies. Queue is", (defined $me->{'delayQueue'} and @{$me->{'delayQueue'}}) ? 'filled.' : 'empty.');
 }


# -------------------------------------------------------------------
=pod

=head2 setObjectMode()

Switches the LDT object to "object trasnfer mode" which means that
is can send and receive Perl data structures now.

Runtime changes of the transfer mode have to be exactly synchronized
with the partner the object is talking with. See the constructor (I<new()>)
description for details.

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=back

B<Example:>

  $asciiClient->setObjectMode;

=cut
# -------------------------------------------------------------------
sub setObjectMode
 {
  # get and check parameters
  my ($me)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: object switches into object mode.");

  # modify mode
  $me->{'objectMode'}=1;
 }


# -------------------------------------------------------------------
=pod

=head2 setAsciiMode()

Switches the LDT object to "ASCII trasnfer mode" which means that
is sends and receives strings now.

Runtime changes of the transfer mode have to be exactly synchronized
with the partner the object is talking with. See the constructor (I<new()>)
description for details.

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=back

B<Example:>

  $objectClient->setAsciiMode;

=cut
# -------------------------------------------------------------------
sub setAsciiMode
 {
  # get and check parameters
  my ($me)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: objekt switches into ASCII mode.");

  # modify mode
  $me->{'objectMode'}=0;
 }


# -------------------------------------------------------------------
=pod

=head2 delay()

Sometimes you do not want to send messages immediatly but buffer
them for later delivery, e.g. to set up a certain send order. You
can use I<delay()> to install a filter which enforces the LDT object
to delay the delivery of all matching messages until the next call
of I<undelay()>.

The filter is implemented as a callback of I<send()>. As long as it
is set, I<send()> calls it to check a message for sending or buffering
it.

You can overwrite a set filter by a subsequent call of I<delay()>.
Messages already collected will remain collected.

To send delayed messages you have to call I<undelay()>.

If the object is detroyed while messages are still buffered,
they will not be delivered but lost.

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=item filter

A code reference. It should await a reference to an array which
will contain the message (possibly in parts). It should reply
a true or false value to flag if the passed message has to be delayed.

It is recommended to provide a I<fast> function because it will be
called everytime I<send()> will be invoked.

=back

B<Example:>

  $ldt->delay(\&filter);

with filter() defined as

  sub filter
   {
    # get and check parameters
    my ($msg)=@_;
    confess "Missed message parameter" unless $msg;
    confess "Message parameter is no array reference"
      unless ref($msg) and ref($msg) eq 'ARRAY';

C<>

    # check something
    $msg->[0] eq 'delay me';
   }

See I<undelay()> for a complete example.

=cut
# -------------------------------------------------------------------
sub delay
 {
  # get and check parameters
  my ($me, $filter)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;
  bug("Missed filter parameter") unless $filter;
  bug("Filter parameter is no code reference ($filter)") unless ref($filter) and ref($filter) eq 'CODE';

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: object is setting a new delay filter.");

  # store filter
  $me->{'delayFilter'}=$filter;
  $me->{'delayQueue'}=[] unless defined $me->{'delayQueue'} and @{$me->{'delayQueue'}};	# keep messages possibly delayed by another filter;
 }


# -------------------------------------------------------------------
=pod

=head2 undelay()

Sends all messages collected by a filter which was set by I<delay()>.
The filter is I<removed>, so that every message will be sent by I<send()>
immediatly afterwards again.

In case of no buffered message and no set filter, a call of this message
takes no effect.

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=back

B<Beispiel:>

  $ldt->undelay;

Here comes a complete example to illustrate how delays can be used.

filter definition:

  sub filter
   {
    # check something
    $msg->[0] eq 'delay me';
   }

usage:

  # send messages
  $ldt->send('send me', 1);    # sent;
  $ldt->send('delay me', 2);   # sent;
  # activate filter
  $ldt->delay(\&filter);
  # send messages
  $ldt->send('send me', 3);    # sent;
  $ldt->send('delay me', 4);   # delayed;
  $ldt->send('send me', 5);    # sent;
  $ldt->send('delay me', 6);   # delayed;
  # send collected messages, uninstall filter
  $ldt->undelay;               # sends messages 4 and 6;
  # send messages
  $ldt->send('send me', 7);    # sent;
  $ldt->send('delay me', 8);   # sent;
  

=cut
# -------------------------------------------------------------------
sub undelay
 {
  # get and check parameters
  my ($me)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;

  # check for a set filter
  if (defined $me->{'delayFilter'})
    {
     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: object stops delay and sends", scalar(@{$me->{'delayQueue'}}), "stored message(s).");

     # remove filter
     $me->{'delayFilter'}=undef;

     # send all delayed messages
     $me->send(@$_) foreach @{$me->{'delayQueue'}};

     # empty queue
     $me->{'delayQueue'}=undef;
    }
  else
    {
     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: object was enforced to stop delay, but there was no delay set before.");
    }
 }

# -------------------------------------------------------------------
=pod

=head2 send(<message>)

Sends the passed message via the related handle (which was passed to
I<new()>). The message, which could be passed as a list of parts, is
sent as a (concatenated) string or as serialized Perl data depending
on the settings made by the constructor flag I<objectMode> and calls
of I<setObjectMode()> or I<setAsciiMode>, respectively.

In case of an error, the method replies an undefined value and stores
both an error code and an error message inside the object which could
be accessed via the object variables "rc" and "msg". (See I<CONSTANTS>
for a list of error codes.)

An error will occur, for example, if the handle related to the LDT object
was closed (possibly outside the module).

An error is detected as well if a I<previous> call of I<send()> or
I<receive()> already detected an error. This behaviour is implemented
for reasons of security, however, if you want to try it again regardless
of the objects history, you can reset the internal error state by I<reset()>.

For reasons of efficiency, sent messages may be splitted up into parts by
the underlaying (operating or network) system. The reciepient will get the
message part by part. On the other hand, the sender might only be able to
I<send> them part by part as well. That is why this I<send()> method retries writing
attempts to the associated handle until the complete message could be sent.
Well, in fact it stops retries earlier if an inacceptable long period of time
passed by without being successfull. If that happens, the method replies I<undef>
and provides an error code in the object variable "rc". I<The caller should be>
I<prepared to handle such cases. Usually further access to the associated handle>
I<is useless or even dangerous.> 

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=item message (a list)

All list elements will be combined to the resulting message as done by I<print()>
or I<warn()> (that means, I<without> separating parts by additional whitespaces).

=back

B<Examples:>

  $asciiClient->send('Silence?', 'Maybe.')
  or die $asciiClient->{'msg'};

B<>

  $objectClient->send({oops=>1, beep=>[qw(7)]}, $scalar, \@array);

B<Note:> If the connection is closed while the message is sent, the signal
I<SIGPIPE> might arrive and terminate the complete program. To
avoid this, I<SIGPIPE> is ignored while this method is running.

The handle associated with the LDT object is made I<non blocking> during
data transmission. The original mode is restored before the method returns.

=cut
# -------------------------------------------------------------------
sub send
 {
  # get and check parameters
  my ($me, @msg)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;
  bug("Missed message parameter(s)") unless @msg;

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: starting send.");

  # check state
  if ($me->{'rc'} and $me->{'rc'}!=LDT_OK)
    {
     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: message unsent: object is in state $me->{'rc'}.");

     # flag error
     undef;
    }
  elsif (not defined $me->{'handle'}->fileno)
    {
     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: message unsent: related handle was closed.");

     # set internal flags
     $me->{'rc'}=LDT_CLOSED;
     $me->{'msg'}='Related handle was closed.';

     # flag error
     undef;
    }
  elsif (defined $me->{'delayFilter'} and &{$me->{'delayFilter'}}(\@msg))
    {
     # messages should be delayed, queue the new one
     push(@{$me->{'delayQueue'}}, \@msg);

     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: message unsent: handle was closed.");
    }
  else
    {
     # temporarily disable SIGPIPE
     local($SIG{'PIPE'})='IGNORE';

     # build the message as necessary
     my $msg=join('', @msg);
     $msg=Storable::nfreeze([@msg]) if $me->{'objectMode'};

     # store original handle access flags
     my $handleFlags=fcntl($me->{'handle'}, F_GETFL, 0);

     # activate non blocking mode
     fcntl($me->{'handle'}, F_SETFL, $handleFlags | O_NONBLOCK);

     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: new message on the way ...");

     # send
     my $rc=$me->writeHandle(\(join('', sprintf(join('', '%.', $me->{'startblockLength'}, 'd'), length($msg)), $msg)));

     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: sent message: $msg.");

     # reset file handle access flags
     fcntl($me->{'handle'}, F_SETFL, $handleFlags);

     # reply result state
     $rc;
    }
 }


# -------------------------------------------------------------------
=pod

=head2 reset

If an error occurs while data are transmitted, further usage of the
associated handle is usually critical. That is why I<send()> and
I<receive()> stop operation after a transmission error, even if you
repeat their calls. This should I<protect> your program and make it
more stable (e.g. writing to a closed handle migth cause a fatal error
and even terminate your program).

Nevertheless, if you really want to retry after an error, here is the
I<reset()> method which resets the internal error flags - unless the
associated handle was not already closed.

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=back

B<Example:>

  $ldtObject->reset;

=cut
sub reset
 {
  # get and check parameters
  my ($me)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: object resets error state.");

  # reset state buffer
  $me->{'msg'}=$me->{'rc'}='' unless $me->{'rc'}==LDT_CLOSED;
 }

# -------------------------------------------------------------------
=pod

=head2 receive()

reads a message from the associated handle and replies it.

In case of an error, the method replies an undefined value and
provides both a return code (see I<CONSTANTS>) and a complete
message in the object variables "rc" and "msg", respectively,
where you can read them.

An error will occur, for example, if the handle related to the LDT object
was closed (possibly outside the module).

An error is detected as well if a I<previous> call of I<send()> or
I<receive()> already detected an error. This behaviour is implemented
for reasons of security, however, if you want to try it again regardless
of the objects history, you can reset the internal error state by I<reset()>.

For reasons of efficiency, sent messages may be splitted up into parts by
the underlaying (operating or network) system. The reciepient will get the
message part by part. That is why this I<receive()> method retries reading
attempts to the associated handle until the complete message could be read.
Well, in fact it stops retries earlier if an inacceptable long period of time
passed by without being successfull. If that happens, the method replies I<undef>
and provides an error code in the object variable "rc". I<The caller should be>
I<prepared to handle such cases. Usually further access to the associated handle>
I<is useless or even dangerous.> 

B<Parameters:>

=over 4

=item object

An LDT object made by I<new()>.

=back

The received message is replied as a string in ASCII mode, and as
a list in object mode.

B<Example:>

  $msg=$asciiClient->receive or die $asciiClient->{'msg'};

B<>

  @objects=$objectClient->receive or die $objectClient->{'msg'};

B<Note:> If the connection is closed while the message is read, the signal
I<SIGPIPE> might arrive and terminate the complete program. To
avoid this, I<SIGPIPE> is ignored while this method is running.

The handle associated with the LDT object is made I<non blocking> during
data transmission. The original mode is restored before the method returns.

=cut
# -------------------------------------------------------------------
sub receive
 {
  # declare function variables
  my ($buffer, $mlen)=('', '');

  # get and check parameters
  my ($me)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: startet receiving.");

  # check state
  if ($me->{'rc'} and $me->{'rc'}!=LDT_OK)
    {
     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: stopped receiving: object is in state $me->{'rc'}.");

     # flag error
     undef;
    }
  elsif (not defined $me->{'handle'}->fileno)
    {
     # trace, if necessary
     $me->trace("LDT $me->{'fileno'}: stopped receiving: object is in state $me->{'rc'}.");

     # set internal flags
     $me->{'rc'}=LDT_CLOSED;
     $me->{'msg'}='Related handle was closed.';

     # flag error
     undef;
    }
  else
    {
     # temporarily disable SIGPIPE
     local($SIG{'PIPE'})='IGNORE';

     # store original handle access flags
     my $handleFlags=fcntl($me->{'handle'}, F_GETFL, 0);

     # activate non blocking mode
     fcntl($me->{'handle'}, F_SETFL, $handleFlags | O_NONBLOCK);

     # read message, start with length info
     my $rc=($me->readHandle(\$mlen) and $me->readHandle(\$buffer, $mlen));

     # reset file handle access flags
     fcntl($me->{'handle'}, F_SETFL, $handleFlags);

     # check transfer success
     unless ($rc)
       {
	# failed: reply state
	return undef;
       }
     else
       {
	# thaw result list, if necessary
	my @buffer=@{Storable::thaw($buffer)} if $buffer and $me->{'objectMode'};

	# reply result in correct form
	$me->{'objectMode'} ? @buffer : $buffer;
       }
    }
 }

# -------------------------------------------------------------------
#
# Internal method: Reads a number of bytes from the object handle.
#
# -------------------------------------------------------------------
sub readHandle
 {
  # declare function variables
  my ($readBytes, $trials);

  # get and check parameters
  my ($me, $targetBufferRef, $targetLength)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;
  bug("Missed target buffer parameter") unless $targetBufferRef;
  bug("Target buffer parameter is no scalar reference") unless ref $targetBufferRef eq 'SCALAR';

  # set default length, if necessary
  $targetLength=$me->{'startblockLength'} unless defined $targetLength;

  # read!
  my $length=$targetLength;
  while ($length)
    {
     # perform reading
     $readBytes=sysread($me->{'handle'}, $$targetBufferRef, $length, $targetLength-$length);

     # all right?
     if (defined $readBytes)
       {
	# connection closed?
	unless ($readBytes)
	  {
	   # the handle closed!
	   $me->{'msg'}="Related handle was closed (while reading was performed).";
	   $me->{'rc'}=LDT_CLOSED;
	   $me->trace("LDT $me->{'fileno'}: $me->{'msg'}");
	   return undef;
	  }

	# If here, we read a little bit more - and this bit was already added
	# to our buffer. All we still have to do is to update our length
	# counter and to reset the trial one.
	$length-=$readBytes;
	$trials=0;
	$me->trace("LDT $me->{'fileno'}: read $readBytes bytes gelesen, still waiting for $length.");
       }
     else
       {
	if ($!==EAGAIN and ++$trials<HANDLE_RETRY_COUNT)
	  {
	   # The system flagged that we should continue later to get more
	   # from our handle. Doing nothing here means we continue with
	   # the next loop - restarting select() - which will hopefully
	   # provide more bytes from the handle.
	   $me->trace("LDT $me->{'fileno'}: waitig for a new chance to read remaining $length bytes ($trials. trial).");
	   $me->{'select'}->can_read(HANDLE_RETRY_DELAY);
	  }
	else
	  {
	   # anything is wrong here
	   $me->{'msg'}="Cannot read the message completely.";
	   $me->{'rc'}=LDT_READ_INCOMPLETE;
	   $me->trace("LDT $me->{'fileno'}: $me->{'msg'}");
	   return undef;
	  }
       }
    }

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: message received: \"$$targetBufferRef\".");

  # if we are here, we were successfull
  $me->{'rc'}=LDT_OK;
  1;
 }

  

# -------------------------------------------------------------------
#
# Internal method: Writes a number of bytes to the object handle.
#
# -------------------------------------------------------------------
sub writeHandle
 {
  # declare function variables
  my ($writtenBytes, $trials, $length, $srcLength);

  # get and check parameters
  my ($me, $srcBufferRef)=@_;
  bug("Missed object parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;
  bug("Missed source buffer parameter") unless $srcBufferRef;
  bug("Source buffer parameter is no scalar reference") unless ref $srcBufferRef eq 'SCALAR';

  # write!
  $length=$srcLength=length($$srcBufferRef);
  while ($length)
    {
     # perform writing
     $writtenBytes=syswrite($me->{'handle'}, $$srcBufferRef, $length, $srcLength-$length);

     # all right?
     if (defined $writtenBytes)
       {
	# connection closed?
	unless ($writtenBytes)
	  {
	   # the handle closed!
	   $me->{'msg'}="Related handle was closed (while writing to it).";
	   $me->{'rc'}=LDT_CLOSED;
	   $me->trace("LDT $me->{'fileno'}: $me->{'msg'}");
	   return undef;
	  }

	# If here, we wrote a little bit more. All we still
	# have to do is to update our length counter and to reset the trial one.
	$length-=$writtenBytes;
	$trials=0;
	$me->trace("LDT $me->{'fileno'}: wrote $writtenBytes bytes, $length bytes still waiting.");
       }
     else
       {
	if ($!==EAGAIN and ++$trials<HANDLE_RETRY_COUNT)
	  {
	   # The sytem flagged that we should continue later to send more
	   # to our handle. Doing nothing here means we continue with
	   # the next loop - restarting select() - which will hopefully
	   # send more bytes to the handle.
	   $me->trace("LDT $me->{'fileno'}: waiting for a new chance to write remaining $length bytes ($trials. trial).");
	   $me->{'select'}->can_write(HANDLE_RETRY_DELAY);
	  }
	else
	  {
	   # anything is wrong here
	   $me->{'msg'}="Cannot write the message completely.";
	   $me->{'rc'}=LDT_WRITE_INCOMPLETE;
	   $me->trace("LDT $me->{'fileno'}: $me->{'msg'}");
	   return undef;
	  }
       }
    }

  # trace, if necessary
  $me->trace("LDT $me->{'fileno'}: message sent completely: \"$$srcBufferRef\".");

  # if we are here, we were successfull
  $me->{'rc'}=LDT_OK;
  1;
 }


# -------------------------------------------------------------------
# Internal trace method.
# -------------------------------------------------------------------
sub trace
 {
  # get and check parameters
  my ($me, @msg)=@_;
  bug("Missed object reference parameter") unless $me;
  bug("Object parameter is no ${\(__PACKAGE__)} object") unless ref($me) eq __PACKAGE__;
  bug("Missed message parameter(s)") unless @msg;

  # deactivate compiler checks
  no strict 'refs';

  # display trace (use print() instead of warn() because the message may contain freezed data)
  print STDERR "[Trace] ", time, ": @msg\n" if ${join('::', __PACKAGE__, 'Trace')} or $me->{'traceMode'};
 }


# ----------------------------------------------------------------------------------------------
=pod

=head2 version()

replies the modules version. It simply replies $IPC::LDT::VERSION and is
implemented only to provide compatibility to other object modules.

Example:

  
  # get version
  warn "[Info] IPC is performed by IPC::LDT ", IPC::LDT::version, ".\n";

=cut
# ----------------------------------------------------------------------------------------------
sub version
 {
  # reply module version
  $IPC::LDT::VERSION;
 }


# = MODULE TRAILER SECTION ===============================================================

# mark a completely read module
1;


# = POD TRAILER SECTION ==================================================================

=pod

=head1 ENVIRONMENT

=head1 FILES

=head1 SEE ALSO

=head1 NOTES

=head1 EXAMPLE

To share data between processes, you could embed a socket into an LDT object.

  my $ipc=new IO::Socket(...);
  my $ldt=new IPC::LDT(handle=>$ipc, objectMode=>1);

Now you are able to send data:

  my $dataRef=[{o=>1, lal=>2, a=>3}, [[qw(4 5 6)], [{oo=>'ps'}, 7, 8, 9]]];
  $ldt->send($dataRef) or die $ldt->{'msg'};

or receive them:

  @data=$ldt->receive or die $ldt->{'msg'};


=head1 AUTHOR

Jochen Stenzel (perl@jochen-stenzel.de)

=head1 COPYRIGHT

Copyright (c) 1998-2000 Jochen Stenzel. All rights reserved.

This program is free software, you can redistribute it and/or modify it
under the terms of the Artistic License distributed with Perl version
5.003 or (at your option) any later version. Please refer to the
Artistic License that came with your Perl distribution for more
details.

=cut
