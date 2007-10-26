.\" $Xorg: design.ms,v 1.3 2000/08/17 19:42:51 cpqbld Exp $
.\" roff -ms
.de Ip
.IP \(bu 3
..
.de Qp
.nr PS -2
.nr VS -2
.QP
..
.\"	These macros should select a typewriter font if you have one.
.de LS
.KS
.LD
.ft CB
.ta .6i 1.2i 1.8i 2.4i 3i 3.6i 4.2i
..
.de LE
.ft P
.DE
.KE
..
.de Ls
.nr PS -2
.nr VS -3
.sp
.LS
..
.de Le
.LE
.nr PS +2
.nr VS +3
.LP
..
.TL
Font server implementation overview
.AU
Dave Lemke
.AI
Network Computing Devices, Inc.
Copyright \(co 1991 Network Computing Devices, Inc.
.NH
Introduction
.PP
The font server uses the same client/server model as X.  The basic structure
is that of the X Consortium X11R5 X server, and those
who know that code should find the 
.I os
and 
.I difs
(device independent font server) layers familiar.
.nf
.Ls

                        +-----------------+
                  +-----|      difs       |------+
                  |     +-----------------+      |
                  |                              |
                +----+                  +------------+
                | os |                  | renderers  |
                +----+                  +------------+
.Le
.fi
\fBDefinitions\fR
.Ip
Renderer.  Code that knows how to take font data in its raw format and
convert it to the font server's format.
.Ip
Font Path Element (FPE).  An instance of a renderer, associated with a
specific font source, (ie a directory of PCF bitmaps).
.PP
The 
.I difs
layer interprets the requests, and handles the renderer
independent work.  This includes error checking of requests, and the
top level font database.  It also contains various utility functionality
such as caching and byte swapping.
.PP
The 
.I os
layer sets up the communications channel, reads requests and
sends the raw data of replies and events.  It also handles font server
configuration issues, controlled by command line arguments and
a configuration file.
.PP
The renderer layer contains all font-specific code, 
and is responsible for rendering a font (which may mean
just reading a bitmap from disk, or may include scaling of outline
data), computing a fonts properties and header information.
.NH
Startup
.PP
At startup, the font server handles any command line arguments,
initializes any OS-specific data, and then sets up the communications.
Various internal databases are then initialized (extensions, the font
catalogue, etc).
.PP
The config file, an ordered list of font sources, cache size hints,
default resolutions, and security information, is then read in.  Each
of these source names could be a directory name, the name of another
font server, or some other string that a particular renderer can
recognize.
.PP
The default font catalogue is then built up by taking each of the font
source names and comparing it with the names a renderer recognizes.
The one that matches this name will become attached to this
source.  A renderer will ``understand'' a name if it can parse the data
in that directory, or recognize that it is a valid font server address,
or recognizes a special string.  Thus a collection of valid font path
elements is built up.  Each
.B FPE
has a set of functions to support opening a font and accessing its
data.
.PP
Font information is accessed via method functions in the 
.B Font.
When a font is
first loaded, the header information and properties are
loaded/computed.  The font also initializes its function pointers to do
the proper work.  When specific metrics or bitmaps are required, they
are access via the font's functions.  A disk-based bitmap font will
probably want to load all data when first accessed.  A scaled font or
FS font may want to do more selective caching.  In both cases, the
renderer can use the utility functions to keep track of this data.
Changing values of bitmap formats could result in the font having
multiple copies of data in different formats, which the renderer may
use the utility functions to manage.
.NH
Per client processing
.PP
Each entity attaching to the server is a client.  Each client has
its own authorization and resolution information, and its own view
of the font database.  A font open to one client may not be open to 
another, though the font server may have it loaded.
.PP
After initialization, new clients can attach to the font server and
have their requests processed.  For each request that is searching for
a font
.B (OpenBitmapFont)
or listing font names
.B (ListFonts,
.B ListFontsWithXInfo),
the pattern is given to each 
.B FPE.
.PP
.B OpenBitmapFont 
will take the supplied name and pass it to each 
.B FPE.
The 
.B FPE
will return one of three things:  
.I Success,
and the font object;
.I BadFont,
because it doesn't know the font; or 
.I BadFont
and an alias
name, when it has an alias for the font.  If 
.I Success
is returned, the
server goes on to create an ID (or find an existing one) and return a
reply.  If 
.I BadFont 
is returned, it goes on to the next 
.B FPE.
If it
reaches the end without finding a font, an error is returned to the
client.  If an alias is returned, the search resets to the first 
.B FPE
and starts again, using the alias as the new font name.  This allows
aliases to work across different 
.B FPEs,
without any ordering
restrictions.
.PP
When each 
.B FPE
receives a font name to open, it searches for the font's
existence.  If it can't find, or can only find an alias, it returns
.I BadFont
and any alias.  If it finds the font, it checks the
authorization and license status of the font to that of the client.  If
it passes, it then creates a new font object, and reads and/or computes
at least the font's header information and properties.  (It may also
want to produce the bitmaps and extents, but that choice is left to the
renderer.)
.PP
When a font's information is accessed, the interpreter routine looks up 
the font ID to find the font object, and then uses the font's access
functions to get the data.  These functions will return the data in
the format expected by the client.

.NH
Client shutdown
.PP
When a client disconnects, all its references to any fonts it
still has opened are removed.  If no other clients reference these fonts, they
may be freed, though the server may choose to cache them.

.NH
Server reset and cleanup
.PP
A server may be reset to flush the caches, re-read the configuration file,
and a new list of 
.B FPEs
to be built, via an OS-specific outside
action.  In UNIX, this will be handled via signals; in VMS it could be
handled via an async trap or event flag.

.NH
Server offloading
.PP
In order to deal with numerous clients without major performance
degradation, the server must be able to clone itself, or provide the
client with a substitute server via the alternate server mechanism.
Since both strategies have their uses, both will be supported.  For a
server that has plenty of host memory or CPU, but insufficient sockets,
cloning may be a good choice.  For a host with limited memory,
assigning an alternate server on a different host may be a good
choice.  The server will make this decision based on configuration
options.

.NH
Font server data structures
.LP
.IP
The 
.B Client
handles per-client information and interpreter status.
.Ls
typedef struct _Client {
    int         index;
    pointer     osPrivate;
    int         noClientException;
    int         (**requestVector) ();
    pointer     requestBuffer;
    int         clientGone;
    int         sequence;
    Bool        swapped;
    long        last_request_time;
    void        (*pSwapReplyFunc) ();
    AuthContextPtr auth;
    char       *catalogues;
    int         num_catalogues;
    Mask        eventmask;
    fsResolution *resolutions;
    int         num_resolutions;
}           ClientRec, *ClientPtr;
.Le
.IP
The
.B Font
contains basic font information, including header information and properties.
.Ls
typedef struct _font	{
	int	refcount;
	fsHeader	header;
	fsBitmapFormat	format;
	int	(*get_glyphs)();
	int	(*get_metrics)();
	int	(*get_extents)();
	int	(*get_bitmaps)();
	int	(*unload_font)();
	FontPathElementPtr	fpe;
	int	*client_ids;
	Bool	restricted_font;
}	FontRec *FontPtr;
.Le
.IP
The
.B ClientFont
is a wrapper on top of 
.B Font,
handling client specific font information.
.Ls
typedef struct _clientfont {
	FontPtr	font;
	int	clientindex;
}	ClientFontRec, *ClientFontRec;
.Le
.IP
The
.B AuthContext
contains authorization information.
.IP
.Ls
typedef 	struct _authcontext	{
	char	*authname;
	char	*authdata;
	FSID	acid;
}	AuthContextRec *AuthContextPtr;
.Le

.NH
Font Path Element functions
.PP
These functions are associated with each renderer, and handle
all aspects of font access.  Font data access is controlled via
another set of functions described later.  These  functions are 
intended to support the R5 X server as well as the font server.
As a result, some design decisions were made to support both
models.  When the 
.I difs
layer needs to access a font, it uses these functions.
.IP
.Ls
typedef unsigned long	Mask;
.sp
typedef unsigned char	*pointer;
.sp
typedef struct _FontPathElement {
    int         name_length;
    char       *name;
    int         type;
    int         refcount;
    pointer     private;
}           FontPathElementRec, *FontPathElementPtr;
.Le
.PP
The FPE's reference count is incremented when it is added to the
current list of FPEs and when it opens a font.  It is decremented
when it is no longer in the current list and when it closes a font.
All reference changes are handled by the
.I difs
layer.  The count is required to support font catalogue changes
that may occur while the fontserver has fonts open, and keeps FPEs
from being lost.
.IP
.Ls
.sp
typedef struct FontNames {
    int	nnames;
    int	size;
    int	*length;
    char	**names;
}	    FontNamesRec, *FontNamesPtr;
.sp
typedef struct {
	Bool	(*name_check)();
	int	(*init_fpe)();
	int	(*reset_fpe)();
	int	(*free_fpe)();
	int	(*open_font)();
	int	(*close_font)();
	int	(*list_fonts)();
	int	(*start_list_fonts_with_info)();
	int	(*list_next_font_with_info)();
	int	(*wakeup_fpe)();
	int	(*client_died);
	FontNamesPtr	renderer_names;
} FPEFunctions;
.sp
int	init_fpe_type(Bool (name_func)(), 
		int (init_func)(), int (free_func)(), int (reset_func), 
		int (open_func)(), int (close_func)(),
		int (list_func)(), 
		int (start_lfwi_func)(), int (next_lfwi_func)(),
		int (wakeup_func)(), 
		int (client_died_func)()
		)
.Le
.sp
.LP
This is called by the renderer when it is initialized at the beginning
of time, and sets up
an FPEFunctions entry for the renderer.
.LP
The
.B FPEFunctions
have the following parameters:
.IP
.Ls
Bool	name_check(char *name);
.Le
.LP
If 
.I name
is something the renderer recognizes as a valid font
source name, it return True, otherwise False.  ie, if 
.I name 
is a directory name, or is prefixed by the renderer's prefix, and the
directory contains font data the renderer can interpret, it would return
True.
.IP
.Ls
int	init_fpe(FontPathElementPtr fpe);
.Le
.LP
Does any initialization work for the renderer.  The name in 
.I fpe
will be one whose prefix matches the list returned when the renderer
was initialized.
.IP
.Ls
int	reset_fpe(FontPathElementPtr fpe);
.Le
.LP
Tells
.I fpe
to reset any internal state about what fonts it has available.
This will typically be called because the font server's 
.B FPE
search list has been changed.  The
.I fpe
should reset any cached state of available fonts (ie, re-read
.I fonts.dir) when this function is called.
.IP
.Ls
int	free_fpe(FontPathElementPtr fpe);
.Le
.LP
Frees any renderer-specific data and closes any files or sockets.
.IP
.Ls
int	open_font(pointer client, FontPathElementPtr fpe, Mask flags, 
		char *fontname, int namelength, 
		fsBitmapFormat format_hint, fsBitmapFormatMask format_mask,
		XID fontid, FontPtr *ppfont, char **alias);
.Le
.LP
Opens the font.  
The bits marked by
.I format_mask in the
.I format_hint
are used where applicable.  
The resulting FontPtr is returned in 
.I ppfont.
The 
.I client 
is optional state
information for use with blocking renderers.  If the 
.I fontname
 resolves to an alias, it is returned in
.I alias
with a 
.I FontNameAlias
error.  This tells the
calling code to start searching again, using 
.I alias
as the font name.
The renderer is expected to fill in any information
specified by the 
.I flags.
.IP
Possible flags values are:
.Ls
#define FontLoadInfo    0x0001		/* font header info */
#define FontLoadProps   0x0002		/* font properties */
#define FontLoadMetrics 0x0004		/* font extents */
#define FontLoadBitmaps 0x0008		/* glyph bitmaps */
#define FontLoadAll     0x000f
#define FontOpenSync    0x0010		/* force synchronous loading */
.Le
.LP
Once a font has been opened, the server may place it and the pattern
it matched into a name cache, to avoid lengthy searching if the font
is reopened.  If the renderer does not wish the font to be in this
cache (for licensing reasons), it should set the font's
.I restricted_access
flag.
.IP
.Ls
int	close_font(FontPtr pfont);
.Le
.LP
Frees up all the data associated with the font.
.IP
.Ls
int	list_fonts(pointer client, FontPathElementPtr fpe,
		char *pattern, int pattern_length, int maxnames,
		FontNamesPtr *paths);
.Le
.LP
Returns in 
.I paths
up to 
.I maxnames 
font names the fpe recognizes as matching the given pattern.
.IP
.Ls
int	start_list_fonts_with_info(pointer client,
		FontPathElementPtr fpe, char *pattern, int pattern_length,
		int maxnames, pointer fpe_data);
.Le
.LP
Initiates a 
.B ListFontsWithXInfo.
Typically, a disk-based renderer
will do the equivalent of ListFonts to gather all the font names 
matching the pattern.  A font server renderer will send the request.
.I fpe_data
provides a handle for any FPE-private data that needs
to be passed in later via 
.B list_next_font_with_info(),
eg, the list of font names for a disk-based renderer.
.IP
.Ls
int	list_next_font_with_info(pointer client, FontPathElementPtr fpe, 
		char **name, int *namelen, FontInfoPtr &pinfo,
		int &num_fonts, pointer fpe_data);
.Le
.LP
Returns the next font's information.  The renderer should keep any state
it requires in the 
.I fpe_data 
field.  
.I num_fonts
contains the number
of replies remaining.
.LP
These two routines are split for because of the way both disk-based
renderers and font server renderers handle this request.
The first function initiates the action, the second is used to gather
the results.  For a
disk-based renderer, a list of font names matching the pattern is first
built up when 
.B start_list_fonts_with_info()
is called, and the results are gathered with each call to
.B list_next_font_with_info.
In a font server renderer, the first function sends the 
.B ListFontsWithXInfo
request, and
the second processes the replies.
.IP
.Ls
int	wakeup_fpe(FontPathElementPtr fpe, unsigned long *mask)
.Le
.LP
Optional function which can be used for blocking renderers.  Typical
usage is for a font server renderer, where it is called when a reply is
received, allowing the data to be read and the client to be signaled
and unblocked.
.IP
.Ls
int	client_died(pointer client, FontPathElementPtr fpe)
.Le
.LP
This function is called when a client dies in the middle of a blocked
request, allowing the renderer to clean up.

.NH
Font specific functions
.LP
These functions are contained in each
.B Font.
For many renderers, every font will
use the same functions, but some renderers may wish to use different interfaces
for different fonts.
.IP
.Ls
typedef struct {
	INT16	left B16,
		right B16;
	INT16	width B16;
	INT16	ascent B16,
		descent B16;
	CARD16	attributes B16;
}	fsCharInfo;

typedef struct {
    CARD8	low,
		high;
}           fsChar2b;

typedef struct {
    fsChar2b	min_char,
		max_char;
}           fsRange;

int	get_extents(pointer client,
		FontPtr pfont, Mask flags, int num_ranges, fsRange *ranges,
		int *num_extents, fsCharInfo **extents);
.Le
.LP
Possible flags:
.IP
.Ls
LoadAll		/* ignore the ranges and get everything */
FinishRange	/* magic for range completion as specified by protocol */
.Le
.LP
Builds up the requested array of extents.  The extent data (which
the renderer allocates) is returned, as well as the number of extents.
.I closure
contains any blocking state information.
.IP
.Ls
int	get_bitmaps(pointer client,
		FontPtr pfont, fsBitmapFormat format, Mask flags,
		int num_ranges, fsRange *ranges,
		unsigned long *size, unsigned long *num_glyphs,
		unsigned long **offsets, pointer *glyph_data);
.Le
.LP
Possible flags:
.IP
.Ls
LoadAll
FinishRange	/* magic for range completion as specified by protocol */
.Le
.LP
Builds up the requested array of bitmaps.  The glyph and offset data
(which the renderer allocates) is returned, as well as the number of
glyphs.  The 
.I closure
contains any blocking state information.  This function will build up the 
bitmap data in the format specified by 
.I format
so that the interpreter can return it without any additional
modification.  This should minimize data massaging, since outline
renderers will hopefully be able to produce the bitmaps in the proper 
format.
.IP
.Ls
void	unload_font(FontPtr pfont)
.Le
.LP
The render will free any allocated data.  Note that the 
.B FPE
function
.B close_font()
will also be called, and should handle any 
.B FPE
data allocated for the font.
.IP
.Ls
int	get_glyphs()
int	get_metrics()
.Le
.LP
These two functions are used by the X server for loading glyphs and 
metrics.  They expect the results in a considerably different
form.  The 
.I get_bitmaps()
and
.I get_extents()
routines both allow for better cache control by the renderer.

.NH
Font directories and aliases
.PP
Existing bitmap renderers already have their own concept of font
organization.  In the X sample server, the files
.B fonts.dir
and 
.B fonts.alias
are used to list the known fonts.
.B fonts.dir
maps file names to font names, while
.B fonts.alias
maps font names to other font names.
.PP
These concepts will also be needed by other forms of fonts
which the sample X server does not currently use, but the font server
will, like Bitstream outlines.

.NH
Handling scalable fonts
.PP
For those renderers that support scalable fonts, several issues
must be addressed:
.br
.Ip
Name Parsing.  An XLFD name must be parsed to determine the requested
resolutions and/or sizes.
.Ip
Property scaling.  Many of the standard font properties have values
that depend on scaling (eg, 
.I RESOLUTION_X.
.I POINT_SIZE)
.Ip
Default values.  If resolution information is wildcarded, the proper
default resolution should be supplied.
.LP
Name Parsing
.PP
The font name pattern supplied to 
.B OpenBitmapFont
or
.B ListFonts
may require some parsing to be recognized as a scalable font known
to the renderer.  The
.B PIXEL_SIZE,
.B POINT_SIZE,
.B RESOLUTION_X,
.B RESOLUTION_Y
and
.B AVERAGE_WIDTH
all need to determined from the font name pattern.  The master font
must then be found, and scaled appropriately.  Any unspecified values
that cannot be determined should be replaced by the proper defaults.
For size fields, this is whatever the configuration specifies.  For
resolution fields, these should be taken from the client's resolution
list, if set, or from the server's configuration.
.LP
Property scaling
.PP
Part of scaling a font is scaling its properties.  Many scalable fonts
will have a very large number of scalable properties.  One way
to deal with these is for the ``master'' outline to keep track of the 
property names, and supply new values for each instance of the font.
If the property names are stored as Atoms, memory usage is kept to
a minimum.
.LP
Using defaults
.PP
Using default values as substitutions for missing values was covered above.
These defaults will also be useful in handling 
.B ListFonts
requests.  Returning a scalable font with an instance using the
default values will provide the most user-friendly environment.

.NH
Access control
.PP
The font server will also support large grain security.  It will have
both a limit of the number of users, and on the hosts which it will
support.
.PP
Limiting the number of users is as much a server loading issue as
a security issue.  The limitation will be typically be set via
configuration options or OS limitations.  To change it, use:
.IP
.Ls
void	AccessSetConnectionLimit(int limit)
.Le
.LP
A
.I limit
of 0 will set it to a compiled constant based on OS resources
(eg, number of file descriptors).
.PP
Client-host based access control can be used to supplement licensing,
and support font server load balancing by restricting access.
As with licensing, this is OS-specific code.
To manipulate these functions, use:
.IP
.Ls
typedef struct _host_address {
	int	type;
	pointer	address;
	struct _host_address *next;
} HostAddress;
.sp
typedef HostAddress	*HostList;
.sp
int	AddHost(HostList list, HostAddress *address)
int	RemoveHost(HostList list, HostAddress *address)
Bool	ValidHost(HostList list, HostAddress *address)
.Le
.LP
.B AddHost()
adds a host to the
.I list.
.B RemoveHost()
removes it, and
.B ValidHost()
checks to see if its on the
.I list.
In all functions, the
.I address
has will ignore any value in the
.I next
field.
.PP
Network addresses are used here to avoid issues with host name aliases.
The caller fills in the desired type, and an address of that form is 
returned.  This is highly OS-specific, but values for the
.I type
and
.I address
fields could include:
.IP
.Ls
#define	HOST_AF_INET	1
struct in_addr	*address;
.sp
#define	HOST_AF_DECnet	2
struct	dn_addr	*address;
.Le
.LP
The server will use a global host list, but having the list
as an argument will allow licensing schemes to have their
own host lists.

.NH
Licensing
.PP
Licensing is a tricky issue, which each renderer will support in a
different way.  The sample font server will attempt to provide some
guidelines, and present a possible implementation of some simple
licensing schemes.
.LP
\fBHost Address licensing\fR
.LP
This is simplistic licensing based on the client's host.  With
this form of licensing, a font may be accessible to some host but not
others.  To get the current client's host, the following is used:
.IP
.Ls
void	GetHostAddress(HostAddress *address);
.Le
.LP
A renderer can also use the host access functions to keep a list
of the licensed hosts, and
.B ValidHost()
to check a client.
.LP
\fBSimultaneous use license\fR
.PP
This licensing allows for a limited number of copies of the font to
be open at once.  Since this should be a simple per-font counter,
no support should be required outside of the renderer.

.NH
DIFS contents
.PP
This contains the protocol dispatcher, interpreter and reply encoding
routines.
.PP
The interpreter is table driven off the request code.  The dispatcher
gets a request from the os layer from 
.B WaitForSomething(),
and uses
the request code to determine which function to call.  eg, a
.I CloseFont
request would call 
.B ProcCloseFont().
.PP
Each request's routine handles any applicable error checking, and then
does as much work as it can.  For font related requests, this means
converting the request to the proper arguments for the renderers.
.PP
If any replies are generated, the reply data is gathered into the
bytestream format, and sent via 
.I os
write functions to the client.
.PP
If the byte order of the client and server differ, the above is
modified by having the dispatcher call an intermediate function which
re-orders the request to the proper byte order.  Replies go through
similar swapping.
.LP
\fBClient blocking\fR
.PP
To minimize delay caused by font server request, clients can
be blocked while they wait for data to be produced.  This is primarily
intended for 
.B FPEs
using a remote font server,
but can be used anywhere where the font server can pause to handle
other client requests while data needed to satisfy another is produced
(possibly via multiple processes).
.IP
.Ls
Bool	ClientSleep(ClientPtr client, Bool (*function)(), pointer closure)
.Le
.LP
Puts a client to 'sleep'.  This means the client will no longer be
considered while the server is dispatching requests.
.I function
will be called when the client is signaled, with the
.I client
and
.I closure
as its arguments.
.Ls
Bool	ClientSignal(ClientPtr client)
.Le
.LP
This should be called when the client is ready to do more work.
At this point, the function given to
.B ClientSleep()
will be called.
.Ls
void ClientWakeup(ClientPtr client)
.Le
.LP
Puts the client back to its normal state processing requests.
.Ls
Bool ClientIsAsleep(ClientPtr client)
.Le
.LP
Can be used to check if a client is asleep.  This is useful for handling
client termination, so that any requests the client is waiting upon can be
properply cleaned up.
.LP
\fBSample Usage\fR
.PP
For handling a font server renderer request for 
.B OpenBitmapFont
the renderer will send the request to the remote font server, and
the call 
.B ClientSleep().
The font server will then continue processing requests from other clients,
while the one making the request is blocked.
When the reply returns, the renderer will notice when its
.B wakeup_fpe()
function is called.  At this point the font server renderer will
read and process the reply.  
.B ClientSignal()
will be called, and the
.I closure
function will be called.  It will request the data from the renderer,
completing the request, and call
.B ClientWakeup()
to return the client to normal status.
.sp
.PP
This layer also contains the resource database, which associates fonts
with IDs, extension interface functions and the server initialization
and reset control.
.NH
OS contents
.PP
This layer contains OS specific routines for configuration, command
line parsing, client/server communications, and various OS-dependent
utilities such as memory management and error handling.
.PP
.B ReadRequestFromClient() 
returns a full request to the dispatcher.
.B WaitForSomething() 
is where the server spends its idle time, waiting
for any action from a client or processing any work left from a blocked
client.
.PP
When a client attempts to connect, the server will call
.IP
.Ls
int	CheckClientAuthorization(ClientPtr client, AuthPtr client_auth,
		int *accept, int *index, int *size, char **authdata)
.Le
.LP
to see if the server is set to allow the client to connect.  It may
use licensing or configuration information to determine if the client
can connect.
.PP
When then connection is established, the server will use the
.IP
.Ls
typedef struct _alt_server {
    char        subset;
    char        namelen;
    char       *name;
}           AlternateServerRec, *AlternateServerPtr;
.sp
int ListAlternateServers(AlternateServerPtr *servers)
.Le
.LP
to return any alternate server information it may have.
.LP
When the client limit is reached, the font server may attempt to
copy itself, by calling
.IP
.Ls
int CloneMyself()
.Le
.LP
This function will (if the configuartion options allow) start a new
font server process.  This is done in such a way that no pending 
connections should be lost, and that the original server will accept
no new connections.  Once the original server has no more clients, it will
exit.

Catalogue manipulation
.PP
Catalogues are configuration dependent, and hence sent by OS-dependent
methods.  In order for the
.I difs
layer to get them, it uses
.IP
.Ls
int	ListCatalogues(char *pattern, int pattern_length,
        	int maxnames, char **catalogues, int *len)
.Le
.LP
which returns the list of all catalogues it supports which match the pattern.
This function 
will be used by the catalogue manipulation requests, as well as by renderers
when they give their 
.B ListFonts
results.
.LP
.Ls
int ValidateCatalogues(int number, char *catalogues)
.Le
.LP
Can be used to validate a list of catalogues, returning True if the
list is acceptable.

.NH
Utility functions
.LP
Client data functions
.PP
These provide access to the current client's resolution and
authorization data.  This form of interface is supplied rather than
passing it to all renderers in the 
.B FPE
functions because the data may
be complex and/or uninteresting to all renderers.
.IP
.Ls
AuthContextPtr	GetClientAuthorization()
.Le
.LP
Returns the authorization data for the current client.
.IP
.Ls
fsResolution	*GetClientResolutions(int  *num_resolutions)
.Le
.LP
Returns the list of resolutions that the current client has set.
.sp 2
.LP
\fBCaching functions\fR
.PP
These are functions that simplify caching of renderer data.  These are 
for use by
renderers that take significant resources to produce data.  The data
must be re-creatable -- the cache is not meant for general storage.
The data may also be moved by the cache, so it should only be accessed
by CacheID.
.IP
.Ls
typedef void (*CacheFree)();
typedef unsigned long	CacheID;
typedef unsigned long	Cache;
.sp 2
Cache CacheInit(int renderer_id)
.Le
.LP
Initializes a cache object for the renderer.  the returned ID should be
passed to 
.B CacheStoreMemory()
when adding an object to the cache.
.IP
.Ls
void CacheStats(Cache cid, unsigned long *num_entries, 
	unsigned long *max_storage, unsigned long *current_storage,
	unsigned long *num_lookups, unsigned long *hit_ratio)
.Le
.LP
Returns statistics on the cache.  Useful if the renderer wants some
hints about whether to place an object in the cache.  If the cache is
nearly full, and the priority low, it may want to take different
action.
.IP
.Ls
CacheID	CacheStoreMemory(Cache cacheid, pointer data, unsigned long size, 
			CacheFree free_func)
.Le
.LP
The renderer hands the cache some chunk of contiguous memory, which the
cache timestamps and stores.  When it needs to remove them, it calls
the
.I free_func,
which must take responsibility for properly freeing the data.
.I size
is primarily a hint to the cache, so that cache limits can be properly
calculated.  A return value of zero means the store failed, probably
because the given size was over the cache limit.  If the given data is
too large for the current cache, it will attempt to free old data to
make room.  The returned ID is a unique value that refers both to the
object and the cache in which it was placed.
.IP
.Ls
pointer CacheFetchMemory(CacheID cid, Bool update)
.Le
.LP
Returns the memory attached to the id.  If 
.I update
is set, the timestamp is updated.  (some accesses may wish to be 'silent',
which allows some control over the freeing scheduling.)  If the cid is invalid,
.I NULL
is returned.
.IP
.Ls
int	CacheFreeMemory(CacheID cid, Bool notify)
.Le
.LP
Allows the cache to flush the data.  If 
.I notify
is set, the CacheFree
function passed in when the data was cached will also be called.
.IP
.Ls
void	MemoryFreed(CacheID cid, pointer data, int reason)
.Le
.LP
Callback function from the cache to the renderer notifying it that its
data has been flushed.  This function then has the responsibility to
free that data.  
.I reason
may be one of:
.IP
.Ls
CacheReset	/* all cache freed because of server reset */
CacheEntryFreed	/* explicit request via free_memory() */
CacheEntryOld	/* cache hit limit, and memory being freed because its old */
.Le
.LP
and is supplied so that the renderer may choose how to deal with the
free request.  (It will probably be ignored by most, but some may want to
keep the memory around by bypassing the cache, or re-inserting it.)
Note that the cache will consider the data gone, so it 
.B must
be re-inserted to keep it alive.
.IP
.Ls
void	CacheSimpleFree(CacheID cid, pointer data, int reason)
.Le
.LP
Just calls 
.B free()
on the data.  Simple CacheFree defined here to
prevent it being redefined in each renderer.
.PP
Typical usage of the cache is for the renderer to store a CacheID
rather than a pointer to the cacheable data.  The renderer is
responsible for both allocating and freeing the data, as well as
keeping track of just what it is.  When the renderer needs the cached
data, it will request it from the cache.  If it fails, it must rebuild
it.
.PP
A possible configuration parameter is the size of the cache.  when the
cache is filled (with the calculation based on the given size), it
sweeps the cache and frees old data.  The amount of memory actually
freed may wish to be tunable:  some systems may want to keep the cache
as full as possible, others may want to free some percentage such that
sweeps occur less frequently.
.PP
Cache statistics may want to be available for administrators.  They
could be dumped to a file when a signal is received.  (SNMP seems like
a perfect match, but apparently the technology isn't there yet.
.PP
Cached data could also be compressed, if the memory/CPU tradeoffs
make it worthwhile.
.PP
ISSUE:  Is a time-based freeing schedule sufficient?  Should priorities
or size also be taken into account?  [ No.  Anything that the renderer
thinks should have a higher priority should probably not be placed into
the cache. ]
.sp 2
.LP
\fBByte swapping\fR
.LP
Functions for swapping a 4-byte quantity, a 2-byte quantity and inverting
a byte.
.IP
.Ls
void	BitOrderInvert(pointer buffer, unsigned long num_bytes)
void	TwoByteSwap(pointer buffer, unsigned long num_shorts)
void	FourByteSwap(pointer buffer, unsigned long num_longs)
.Le
.LP
\fBBitmap padding\fR
.LP
Functions taking a desired extents and a bitmap that will return the
bitmap properly padded.
.Ls
int	RepadBitmap(pointer src, pointer dst, fsFormat src_format,
		 fsFormat dst_format, int width, int height)
.Le
.LP
Takes a bitmap in 
.I src_format 
and converts it to one in
.I dst_format.
.LP
\fBAtoms\fR
.PP
Existing bitmap-based renderers use atoms to store strings for property
information.  Rather than duplicate this code in each renderer, it
lives in the 
.I util
directory.
.PP
Atoms will be especially useful for property information, to prevent
many copies of the same strings from being saved.  Using atoms for
comparison when modifying properties after scaling is also more
efficient.  Since
.I atoms
will will exist until the server is reset, they may want to be used
sparingly for property values to avoid extraneous string data.
.IP
.Ls
typedef unsigned long	Atom;
.sp
Atom	MakeAtom(char *string, unsigned int length, Bool create)
.Le
.LP
Returns the atom associated with 
.I string.
If
.I create
is true, a new atom will be created.
.IP
.Ls
char	*NameForAtom(Atom atom)
.Le
.LP
Returns the string associated with
.I atom.

.NH
Server request details
.PP
This section describes in-depth the action of each protocol request.
In all cases, the request is first error checked for simple length
or value errors, with the server
immediately returning an error if one is encountered.
.NH 2
Connection
.PP
When a new client attempts to connect, the server first checks 
its initial authorization information to see if the server is willing
to talk to it.  This will be handled in some OS-specific form
using
.B CheckClientAuthorization().
If it passes
this test, and the server has sufficient to resources to talk to it, the
server sends accepts the connection and returns its connection block.
If the connection fails, the server returns the proper status and
a list of any alternate servers it may know of (gathered from 
.B ListAlternateServers().)
.NH 2
ListExtension
.PP
Returns the list of extensions the server knows about.
Any extensions will be initialized when the server is first started.
.NH 2
QueryExtension
.PP
Returns the information about the requested extension, which was set
when the extension was initialized.
.NH 2
ListCatalogues
.PP
Returns the catalogues the server recognizes (the results of
.B ListCatalogues().)
.NH 2
SetCatalogues
.PP
Sets the requesting client's catalogues after verifying them with the
supported catalogues.
.NH 2
GetCatalogues
.PP
Returns the requesting client's catalogues.
.NH 2
CreateAC
.PP
Creates a new authorization context and fills it in.  The list of
authorization protocols is then checked by the server with
.B CheckClientAuthorization().
If any are accepted,
the 
.B AC
is placed in the resource database and
.I Success
is returned with the name of the accepted protocol.  If more than one is 
accepted, 
.I Continue
is returned with each of the accepted protocols, until the last one
which has status
.I Success
Otherwise
.I Denied
is returned.
.NH 2
FreeAC
.PP
Looks up the
.B AC
in the resource database, and frees it if it finds it.  Otherwise an
.I Access
error is returned.
.NH 2
SetAuthorization
.PP
Looks up the
.B AC
in the resource database, and set the client's AuthContextPtr
to its value if it is found.  Otherwise it sends an 
.I Access
error.
.NH 2
SetResolution
.PP
Sets the requesting client's resolution list to the supplied list.
.NH 2
GetResolution
.PP
Returns the requesting client's list of resolutions.
.NH 2
ListFonts
.PP
Iterates over each open FPE, calling the FPE's
.B list_fonts()
routine passing it the pattern.
When all FPE's have been processed, the list that has been built up
is returned.  Note that the same
.B FontNamesPtr
is sent to each FPE in turn, so that one list is built up.
An FPE may restrict the fonts it returns based on the client's
catalogue.
.NH 2
ListFontsWithXInfo
.PP
Iterates over each FPE, calling its
.B start_list_fonts_with_info()
function to prime the FPE's renderer.  It then calls the FPE's
.B list_next_font_with_info(),
sending each font's data to the client until no more fonts remain.
When all FPEs have been processed, the final reply with a zero-length
name is then sent to mark the end of the replies.
An FPE may restrict the fonts it returns based
on the client's catalogue.
Note: an issue
exists with font aliases which may require this to change, since an FPE
may contain an alias pointing to another FPE, and cannot therefore
return the font's info.
.NH 2
OpenBitmapFont
.PP
The pattern is first searched for in the font server's name cache.
If it doesn't find it, the server iterates over each FPE, calling its
.B open_font
function with the supplied pattern.  This will return one of the following
values:
.Ip
an
.B Access
error, which means the renderer has the font but the client does not
have access to it because of some form of licensing restriction
.Ip
a
.B Font
error and a NULL
.I alias
parameter, which will cause the next FPE to be tried
.Ip
a
.B Font 
error but a non-NULL
.I alias,
which will cause the search to start over with the first FPE using
.I alias
as the new font pattern
.Ip
.B Success,
in which case a valid font has been found.
.PP
If the end of the FPE list is reached without having found the font,
an error is returned to the client.  If an
.B Access
error was encountered, it is returned, otherwise a
.B Font
error is returned.
If a valid font is found, its reference count will be incremented and
it will be checked to see if the client has
already opened it before.  If so, the previous ID will be returned.
Otherwise the font will be placed in the resource database.
.PP
The renderer will fill in the font's header and property information,
and may also choose to load or create the font's metrics or glyphs.
If the glyphs are built, they will use any supplied \fIformat hint\fR.
.PP
Whenever a new font is successfuly opened, the font and its name pattern
will be placed in a name cache.  This cache exists to minimize the amount 
of work spent searching for a font.  It will be flushed when the
font catalogue is modified.  Client's with private font catalogues
will require private name caches.
.NH 2
QueryXInfo
.PP
The
.I fontid
is looked up in the resource database, and the font's header and 
property info is returned.
.NH 2
QueryXExtents8 QueryXExtents16
.PP
The
.I fontid
is looked up in the resource database.  The supplied list of
characters (interpreted according to request type) is then translated 
into a list of ranges.  The font's 
.B get_extents()
function is then called.  It builds the requested list of extents,
and returns them along with the number of extents.
The results are properly swapped and sent to the client.
.NH 2
QueryXBitmaps8 QueryXBitmaps16
.PP
The
.I fontid
is looked up in the resource database.  The supplied list of
characters (interpreted according to request type) is then translated 
into a list of ranges.  The font's 
.B get_bitmaps()
function is called, and the renderer will build up the requested
bitmaps, using the specified 
.I format,
and returns the bitmaps, the number of glyphs and the offsets.
The offsets are properly swapped and the offsets and bitmaps are
sent to the clients.
.NH 2
CloseFont
.PP
The font's reference count is decremented.  If this was the last reference,
the font's
.B unload_font()
function is called to free the renderer's data, and the font's
FPE
.B close_font()
function is called to free up any FPE specific data.

.NH
Configuration
.PP
The configuration mechanism is a simple keyword-value pair, separated 
by an '='.  
.LP
Configuration types:
.ta .6i 2.1i
.nf
.sp
	cardinal	non-negative number
.sp
	boolean		"[Yy]es", "[Yy]" "on", "1", "[Nn]o", "[Nn]", "off", "0"
.sp
	resolution	\fIcardinal,cardinal\fR
.sp
	list of foo	1 or more of foo, separated by commas
.sp
.fi
.LP
Here is an incomplete list of the supported keywords:
.sp
.ta .6i 1.5i
.nf
#		in the first column, a comment character
.\".sp
.\"cache-size (cardinal) 
.\"		Size in bytes of the FS cache.
.sp
catalogue (list of string)
		Ordered list of font path element names.
.sp
alternate-servers (list of string)
		List of alternate servers for this FS.
.sp
client-limit (cardinal)
		Number of clients this FS will support before refusing
		service.
.sp
clone-self (boolean)
		Whether this FS should attempt to clone itself or
		use delegates when it reachs the client-limit.
.sp
default-point-size (cardinal)
		The default pointsize (in decipoints) for fonts that 
		don't specify.
.sp
default-resolutions (list of resolutions)
		Resolutions the server supports by default.
		This information may be used as a hint for pre-rendering.
.sp
error-file (string)
		Filename of the error file.  All warnings and errors
		will be logged here.
.sp
port (cardinal)
		The TCP port on which the server will listen for connections.
.sp
use-syslog (boolean)
		Whether syslog(3) is to be used for errors.
.\".sp
.\"trusted-clients (list of string)
.\"		Those clients the fontserver will talk to.  Others
.\"		will be refused for the initial connection.  An empty
.\"		list means the server will talk to any client.
.fi
.IP
Each renderer may also want private configuration options.  The names
should be prefixed by the renderer name, ie 
.I pcf-,
.I atm-.
.LP
Examples:
.sp
# allow a ~a megabyte of memory to be reserved for cache data
.br
cache-size = 1000000
.sp
catalogue = pcf:/usr/lib/X11/fonts/misc,speedo:/usr/lib/fonts/speedo
