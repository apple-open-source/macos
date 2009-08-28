package Authen::Krb5;

use strict;
use Carp;
use vars qw($VERSION @ISA @EXPORT @EXPORT_OK $AUTOLOAD);

require 5.004;

require Exporter;
require DynaLoader;
require AutoLoader;

@ISA = qw(Exporter DynaLoader);

@EXPORT = qw(
	ADDRTYPE_ADDRPORT
	ADDRTYPE_CHAOS
	ADDRTYPE_DDP
	ADDRTYPE_INET
	ADDRTYPE_IPPORT
	ADDRTYPE_ISO
	ADDRTYPE_XNS
	AP_OPTS_MUTUAL_REQUIRED
	AP_OPTS_RESERVED
	AP_OPTS_USE_SESSION_KEY
	AP_OPTS_USE_SUBKEY
	AP_OPTS_WIRE_MASK
	KDC_OPT_ALLOW_POSTDATE
	KDC_OPT_ENC_TKT_IN_SKEY
	KDC_OPT_FORWARDABLE
	KDC_OPT_FORWARDED
	KDC_OPT_POSTDATED
	KDC_OPT_PROXIABLE
	KDC_OPT_PROXY
	KDC_OPT_RENEW
	KDC_OPT_RENEWABLE
	KDC_OPT_RENEWABLE_OK
	KDC_OPT_VALIDATE
	KRB5_AUTH_CONTEXT_DO_SEQUENCE
	KRB5_AUTH_CONTEXT_DO_TIME
	KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR
	KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR
	KRB5_AUTH_CONTEXT_GENERATE_REMOTE_ADDR
	KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR
	KRB5_AUTH_CONTEXT_RET_SEQUENCE
	KRB5_AUTH_CONTEXT_RET_TIME
	KRB5_NT_PRINCIPAL
	KRB5_NT_SRV_HST
	KRB5_NT_SRV_INST
	KRB5_NT_SRV_XHST
	KRB5_NT_UID
	KRB5_NT_UNKNOWN
	KRB5_TGS_NAME
);
$VERSION = '1.6';

sub KRB5_TGS_NAME() { return "krbtgt"; }

bootstrap Authen::Krb5 $VERSION;

# Preloaded methods go here.

sub AUTOLOAD {
    # This AUTOLOAD is used to 'autoload' constants from the constant()
    # XS function.  If a constant is not found then control is passed
    # to the AUTOLOAD in AutoLoader.

    my $constname;
    ($constname = $AUTOLOAD) =~ s/.*:://;
    my $val = constant($constname, @_ ? $_[0] : 0);
    if ($! != 0) {
	if ($! =~ /Invalid/) {
	    $AutoLoader::AUTOLOAD = $AUTOLOAD;
	    goto &AutoLoader::AUTOLOAD;
	}
	else {
		croak "Your vendor has not defined Krb5 macro $constname";
	}
    }
    eval "sub $AUTOLOAD { $val }";
    goto &$AUTOLOAD;
}

# Autoload methods go after =cut, and are processed by the autosplit program.

1;
__END__

=head1 NAME

Authen::Krb5 - Perl extension for Kerberos 5

=head1 SYNOPSIS

use Authen::Krb5;

Authen::Krb5::init_context();

=head1 DESCRIPTION

Authen::Krb5 is an object oriented interface to the Kerberos 5 API.  Both the
implementation and documentation are nowhere near complete, and may require
previous experience with Kerberos 5 programming.  Most of the functions here
are documented in detail in the Kerberos 5 API documentation.

=head2 FUNCTIONS

=over 4

=item error(n)

Returns the error code from the most recent Authen::Krb5 call.  If provided
with an error code 'n', this function will return a textual description of the
error.

=item init_context()

Initializes a context for the application.  Returns a Authen::Krb5::Context
object, or undef if there was an error.

=item init_ets() (DEPRECATED)

Initializes the Kerberos error tables.  Should be called along with
init_context at the beginning of a script.

=item get_default_realm()

Returns the default realm of your host.

=item get_host_realm(host)

Returns the realm of the specified host.

=item get_krbhst(realm)

Returns a list of the Kerberos servers from the specified realm.

=item build_principal_ext(p)

Not like the actual krb5_build_principal_ext.  This is legacy code from
Malcolm's code, which I'll probably change in future releases.  In any case,
it creates a 'server' principal for use in getting a TGT.  Pass it the
principal for which you would like a TGT.

=item parse_name(name)

Converts a string representation of a principal to a principal object.  You
can use this to create a principal from your username.

=item sname_to_principal(hostname,sname,type)

Generates a server principal from the given hostname, service, and type.
Type can be one of the following: NT_UNKNOWN, NT_PRINCIPAL, NT_SRV_INST,
NT_SRV_HST, NT_SRV_XHST, NT_UID.  See the Kerberos documentation for details.

=item cc_resolve(name)

Returns a credentials cache identifier which corresponds to the given name.
'name' must be in the form TYPE:RESIDUAL.  See the Kerberos documentation
for more information.

=item cc_default_name()

Returns the name of the default credentials cache, which may be equivalent
to KRB5CCACHE.

=item cc_default()

Returns a Authen::Krb5::Ccache object representing the default credentials
cache.

=item kt_resolve(name)

Returns a Authen::Krb5::Keytab object representing the specified keytab name.

=item kt_default_name()

Returns a sting containing the default keytab name.

=item kt_default()

Returns an Authen::Krb5::Keytab object representing the default keytab.

=item kt_read_service_key(name, principal[, kvno, enctype])

Searches the keytab specified by I<name> (the default keytab if
I<name> is undef) for a key matching I<principal> (and optionally
I<kvno> and I<enctype>) and returns the key in the form of an
Authen::Krb5::Keyblock object.

=item get_in_tkt_with_password(client,server,password,cc)

Attempt to get an initial ticket for the client.  'client' is a principal
object for which you want an initial ticket.  'server' is a principal object
for the service (usually krbtgt/REALM@REALM).  'password' is the password for
the client, and 'cc' is a Authen::Krb5::Ccache object representing the current
credentials cache.  Returns a Kerberos error code.

=item get_in_tkt_with_keytab(client,server,keytab,cc)

Obtain an initial ticket for the client using a keytab.  'client' is a
principal object for which you want an initial ticket.  'server' is a principal
object for the service (usually krbtgt/REALM@REALM).  'keytab' is a keytab
object createed with kt_resolve.  'cc' is a Authen::Krb5::Ccache object
representing the current credentials cache.  Returns a Kerberos error code.

=item mk_req(auth_context,ap_req_options,service,hostname,in,cc)

Obtains a ticket for a specified service and returns a KRB_AP_REQ message
suitable for passing to rd_req.  'auth_context' is the Authen::Krb5::AuthContext
object you want to use for this connection, 'ap_req_options' is an OR'ed
representation of the possible options (see Kerberos docs), 'service' is
the name of the service for which you want a ticket (like 'host'), hostname
is the hostname of the server, 'in' can be any user-specified data that can
be verified at the server end, and 'cc' is your credentials cache object.

=item rd_req(auth_context,in,server,keytab)

Parses a KRB_AP_REQ message and returns its contents in a Authen::Krb5::Ticket
object.  'auth_context' is the connection's Authen::Krb5::AuthContext object,
'in' is the KRB_AP_REQ message (usually from mk_req), and server is the
expected server's name for the ticket.  'keytab' is a Authen::Krb5::Keytab
object for the keytab you want to use.  Specify 'undef' or leave off to use
the default keytab.

=item mk_priv(auth_context,in)

Encrypts 'in' using parameters specified in auth_context, and returns the
encrypted data.  Requires use of a replay cache.

=item rd_priv(auth_context,in)

Decrypts 'in' using parameters specified in auth_context, and returns the
decrypted data.

=item sendauth(auth_context,fh,version,client,server,options,in,in_creds,cc)

Obtains and sends an authenticated ticket from a client program to a server
program using the filehandle 'fh'.  'version' is an application-defined
version string that recvauth compares to its own version string.  'client'
is the client principal, e.g. username@REALM.  'server' is the service
principal to which you are authenticating, e.g. service.hostname@REALM.
The only useful option right now is AP_OPTS_MUTUAL_REQUIRED, which forces
sendauth to perform mutual authentication with the server.  'in' is a string
that will be received by recvauth and verified by the server--it's up to the
application.  'in_creds' is not yet supported, so just use 'undef' here.  'cc'
should be set to the current credentials cache.  sendauth returns true
on success and undefined on failure.

=item recvauth(auth_context,fh,version,server,keytab)

Receives authentication data from a client using the sendauth function through
the filehandle 'fh'.  'version' is as described in the sendauth section.
'server' is the server principal to which the client will be authenticating.
'keytab' is a Authen::Krb5::Keytab object specifying the keytab to use for this
service.  recvauth returns a Authen::Krb5::Ticket object on success or
undefined on failure.

=item genaddrs(auth_context,fh,flags)

Uses the open socket filehandle 'fh' to generate local and remote addresses
for auth_context.  Flags should be one of the following, depending on the
type of address you want to generate (flags can be OR'ed):

KRB5_AUTH_CONTEXT_GENERATE_LOCAL_ADDR
KRB5_AUTH_CONTEXT_GENERATE_LOCAL_FULL_ADDR
KRB5_AUTH_CONTEXT_GENERATE_REMOTE_ADDR
KRB5_AUTH_CONTEXT_GENERATE_REMOTE_FULL_ADDR

=item gen_portaddr(addr,port)

Generates a local port address that can be used to name a replay cache.  'addr' is a Authen::Krb5::Address object, and port is a port number in network byte
order.  For generateing a replay cache name, you should supply the local
address of the client and the socket's local port number.  Returns a
Authen::Krb5::Address object containing the address.

=item gen_replay_name(addr,string)

Generate a unique replay cache name.  'addr' is a Authen::Krb5::Address object
created by gen_portaddr.  'string' is used as a unique identifier for the
replay cache.  Returns the replay cache name.

=item get_server_rcache(name)

Returns a Authen::Krb5::Rcache object using the replay cache name 'name.'

=back

=head2 CLASSES & METHODS

=over 4

=item Authen::Krb5::Principal

Kerberos 5 princpal object.

=over 4

=item o realm

Returns the realm of the principal.

=item o type

Returns the type of the principal.

=item o data

Returns a list containing the components of the principal (everything before
the realm).

=back

=item Authen::Krb5::Ccache

Kerberos 5 credentials cache object.

=over 4

=item o initialize(p)

Creates/refreshes a credentials cache for the primary principal 'p'.  If the
cache already exists, its contents are destroyed.

=item o get_name

Returns the name of the credentials cache.

=item o get_principal

Returns the primary principal of the credentials cache.

=item o destroy

Destroys the credentials cache and releases all resources it used.

=item o start_seq_get()

Returns a cursor that can be passed to I<next_cred()> to read in turn
every credential in the cache.

=item o next_cred(cursor)

Returns the next credential in the cache as an Authen::Krb5::Creds
object.

=item o end_seq_get(cursor)

Perform cleanup opreations after I<next_cred()> and invalidates
I<cursor>.

=back

=item Authen::Krb5::KeyBlock

Kerberos 5 keyblock object.

=over 4

=item o enctype()

Returns the encryption type ID.

=item o enctype_string()

Returns a text description of the encryption type.

=item o length()

Returns the length of the session key.

=item o contents()

Returns the actual contents of the keyblock (the session key).

=back

=item Authen::Krb5::AuthContext

Kerberos 5 auth_context object.

=over 4

=item o new

Allocates memory for a new Authen::Krb5::AuthContext object and returns it.

=item o setaddrs(localaddr,remoteaddr)

Sets the local and remote addresses for the AuthContext object.  'localaddr'
and 'remoteaddr' are Authen::Krb5::Address objects, usually of type
ADDRTYPE_INET.

=item o getaddrs()

Returns a list containing the local and the remote address of the
AuthContext object.

=item o setrcache(rc)

Sets the replay cache for auth_context.  'rc' is a Authen::Krb5::Rcache object
generated by get_server_rcache.

=item o getkey()

Retrieves the session key as an Authen::Krb5::KeyBlock object.

=back

=item Authen::Krb5::Ticket

Kerberos 5 ticket object.

=over 4

=item o server

Returns the server stored in the ticket.

=item o enc_part2

Returns a Authen::Krb5::EncTktPart object representation of the ticket data.
See below.

=back

=item Authen::Krb5::EncTktPart

Object representation of the krb5_enc_tkt_part structure.

=over 4

=item o client

The client principal contained in the ticket.

=back

=item Authen::Krb5::Keyblock

Object representation of the krb5_keyblock structure.

=over 4

=item o enctype

The integral enctype of the key.

=item o length

Length of the key.

=item o contents

Contents of the key itself, as a string.

=back

=item Authen::Krb5::Keytab

=over 4

=item o add_entry(entry)

Adds I<entry> to the keytab.

=item o remove_entry(entry)

Removes I<entry> from the keytab.

=item o get_name()

Returns the name of the keytab.

=item o get_entry(principal[, kvno, enctype])

Returns an Authen::Krb5::KeytabEntry object representing an entry in
the keytab matching I<principal> and optionally I<kvno> and
I<enctype>.

=item o start_seq_get()

Returns a cursor that can be passed to I<next_entry()> to read in turn
every key in the keytab.

=item o next_entry(cursor)

Returns the next entry in the keytab as an Authen::Krb5::KeytabEntry
object.

=item o end_seq_get(cursor)

Perform cleanup opreations after I<next_entry()> and invalidates
I<cursor>.

=back

=item Authen::Krb5::KeytabEntry

=over 4

=item o new(principal, kvno, keyblock)

Create a new Authen::Krb5::KeytabEntry object from an
Authen::Krb5::Principal object, a key version number, and an
Authen::Krb5::Keyblock object.

=item o principal

An Authen::Krb5::Principal object representing the principal contained
in the entry.

=item o timestamp

The timestamp of the entry.

=item o kvno

The key version number of the key contained in the entry.

=item o key

An Authen::Krb5::Keyblock object representing a copy of the keyblock
contained in the entry.

=back

=head1 AUTHOR

Jeff Horwitz (jeff@laserlink.net)

=head1 ACKNOWLEDGEMENTS

Based on the original work by Doug MacEachern and Malcolm Beattie.  Code
contributions from Scott Hutton (shutton@indiana.edu).

=head1 SEE ALSO

perl(1), kerberos(1).

=cut
