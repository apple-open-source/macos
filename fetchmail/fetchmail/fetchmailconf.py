#!/usr/bin/env python
#
# A GUI configurator for generating fetchmail configuration files.
# by Eric S. Raymond, <esr@snark.thyrsus.com>,
# Matthias Andree <matthias.andree@gmx.de>
# Requires Python with Tkinter, and the following OS-dependent services:
#	posix, posixpath, socket
version = "1.55 $Revision: 5330 $"

from Tkinter import *
from Dialog import *
import sys, time, os, string, socket, getopt, tempfile

#
# Define the data structures the GUIs will be tossing around
#
class Configuration:
    def __init__(self):
	self.poll_interval = 0		# Normally, run in foreground
	self.logfile = None		# No logfile, initially
	self.idfile = os.environ["HOME"] + "/.fetchids"	 # Default idfile, initially
	self.postmaster = None		# No last-resort address, initially
	self.bouncemail = TRUE		# Bounce errors to users
	self.spambounce = FALSE		# Bounce spam errors
	self.softbounce = TRUE		# Treat permanent error as temporary
	self.properties = None		# No exiguous properties
	self.invisible = FALSE		# Suppress Received line & spoof?
	self.syslog = FALSE		# Use syslogd for logging?
	self.servers = []		# List of included sites
	Configuration.typemap = (
	    ('poll_interval',	'Int'),
	    ('logfile',	 'String'),
	    ('idfile',	  'String'),
	    ('postmaster',	'String'),
	    ('bouncemail',	'Boolean'),
	    ('spambounce',	'Boolean'),
	    ('softbounce',	'Boolean'),
	    ('properties',	'String'),
	    ('syslog',	  'Boolean'),
	    ('invisible',	'Boolean'))

    def __repr__(self):
	str = "";
	if self.syslog != ConfigurationDefaults.syslog:
	   str = str + ("set syslog\n")
	elif self.logfile:
	    str = str + ("set logfile \"%s\"\n" % (self.logfile,));
	if self.idfile != ConfigurationDefaults.idfile:
	    str = str + ("set idfile \"%s\"\n" % (self.idfile,));
	if self.postmaster != ConfigurationDefaults.postmaster:
	    str = str + ("set postmaster \"%s\"\n" % (self.postmaster,));
	if self.bouncemail:
	    str = str + ("set bouncemail\n")
	else:
	    str = str + ("set nobouncemail\n")
	if self.spambounce:
	    str = str + ("set spambounce\n")
	else:
	    str = str + ("set no spambounce\n")
	if self.softbounce:
	    str = str + ("set softbounce\n")
	else:
	    str = str + ("set no softbounce\n")
	if self.properties != ConfigurationDefaults.properties:
	    str = str + ("set properties \"%s\"\n" % (self.properties,));
	if self.poll_interval > 0:
	    str = str + "set daemon " + `self.poll_interval` + "\n"
	for site in self.servers:
	    str = str + repr(site)
	return str

    def __delitem__(self, name):
	for si in range(len(self.servers)):
	    if self.servers[si].pollname == name:
		del self.servers[si]
		break

    def __str__(self):
	return "[Configuration: " + repr(self) + "]"

class Server:
    def __init__(self):
	self.pollname = None		# Poll label
	self.via = None			# True name of host
	self.active = TRUE		# Poll status
	self.interval = 0		# Skip interval
	self.protocol = 'auto'		# Default to auto protocol
	self.service = None		# Service name to use
	self.uidl = FALSE		# Don't use RFC1725 UIDLs by default
	self.auth = 'any'		# Default to password authentication
	self.timeout = 300		# 5-minute timeout
	self.envelope = 'Received'	# Envelope-address header
	self.envskip = 0		# Number of envelope headers to skip
	self.qvirtual = None		# Name prefix to strip
	self.aka = []			# List of DNS aka names
	self.dns = TRUE			# Enable DNS lookup on multidrop
	self.localdomains = []		# Domains to be considered local
	self.interface = None		# IP address and range
	self.monitor = None		# IP address and range
	self.plugin = None		# Plugin command for going to server
	self.plugout = None		# Plugin command for going to listener
	self.principal = None		# Kerberos principal
	self.esmtpname = None		# ESMTP 2554 name
	self.esmtppassword = None	# ESMTP 2554 password
	self.tracepolls = FALSE		# Add trace-poll info to headers
	self.users = []			# List of user entries for site
	Server.typemap = (
	    ('pollname',  'String'),
	    ('via',	  'String'),
	    ('active',	  'Boolean'),
	    ('interval',  'Int'),
	    ('protocol',  'String'),
	    ('service',	  'String'),
	    ('uidl',	  'Boolean'),
	    ('auth',	  'String'),
	    ('timeout',   'Int'),
	    ('envelope',  'String'),
	    ('envskip',   'Int'),
	    ('qvirtual',  'String'),
	    # leave aka out
	    ('dns',	  'Boolean'),
	    # leave localdomains out
	    ('interface', 'String'),
	    ('monitor',   'String'),
	    ('plugin',	  'String'),
	    ('plugout',   'String'),
	    ('esmtpname', 'String'),
	    ('esmtppassword', 'String'),
	    ('principal', 'String'),
	    ('tracepolls','Boolean'))

    def dump(self, folded):
	res = ""
	if self.active:   res = res + "poll"
	else:	     res = res + "skip"
	res = res + (" " + self.pollname)
	if self.via:
	    res = res + (" via " + str(self.via) + "\n");
	if self.protocol != ServerDefaults.protocol:
	    res = res + " with proto " + self.protocol
	if self.service and self.protocol and self.service != defaultports[self.protocol] and defaultports[self.protocol] and self.service != ianaservices[defaultports[self.protocol]]:
	    res = res + " service " + self.service
	if self.timeout != ServerDefaults.timeout:
	    res = res + " timeout " + `self.timeout`
	if self.interval != ServerDefaults.interval:
	    res = res + " interval " + `self.interval`
	if self.envelope != ServerDefaults.envelope or self.envskip != ServerDefaults.envskip:
	    if self.envskip:
		res = res + " envelope " + `self.envskip` + " " + self.envelope
	    else:
		res = res + " envelope " + self.envelope
	if self.qvirtual:
	    res = res + (" qvirtual " + str(self.qvirtual) + "\n");
	if self.auth != ServerDefaults.auth:
	    res = res + " auth " + self.auth
	if self.dns != ServerDefaults.dns or self.uidl != ServerDefaults.uidl:
	    res = res + " and options"
	if self.dns != ServerDefaults.dns:
	    res = res + flag2str(self.dns, 'dns')
	if self.uidl != ServerDefaults.uidl:
	    res = res + flag2str(self.uidl, 'uidl')
	if folded:	res = res + "\n    "
	else:	     res = res + " "

	if self.aka:
	     res = res + "aka"
	     for x in self.aka:
		res = res + " " + x
	if self.aka and self.localdomains: res = res + " "
	if self.localdomains:
	     res = res + ("localdomains")
	     for x in self.localdomains:
		res = res + " " + x
	if (self.aka or self.localdomains):
	    if folded:
		res = res + "\n    "
	    else:
		res = res + " "

	if self.tracepolls:
	   res = res + "tracepolls\n"

	if self.interface:
	    res = res + " interface " + str(self.interface)
	if self.monitor:
	    res = res + " monitor " + str(self.monitor)
	if self.plugin:
	    res = res + " plugin " + `self.plugin`
	if self.plugout:
	    res = res + " plugout " + `self.plugout`
	if self.principal:
	    res = res + " principal " + `self.principal`
	if self.esmtpname:
	    res = res + " esmtpname " + `self.esmtpname`
	if self.esmtppassword:
	    res = res + " esmtppassword " + `self.esmtppassword`
	if self.interface or self.monitor or self.principal or self.plugin or self.plugout:
	    if folded:
		res = res + "\n"

	if res[-1] == " ": res = res[0:-1]

	for user in self.users:
	    res = res + repr(user)
	res = res + "\n"
	return res;

    def __delitem__(self, name):
	for ui in range(len(self.users)):
	    if self.users[ui].remote == name:
		del self.users[ui]
		break

    def __repr__(self):
	return self.dump(TRUE)

    def __str__(self):
	return "[Server: " + self.dump(FALSE) + "]"

class User:
    def __init__(self):
	if os.environ.has_key("USER"):
	    self.remote = os.environ["USER"]	# Remote username
	elif os.environ.has_key("LOGNAME"):
	    self.remote = os.environ["LOGNAME"]
	else:
	    print "Can't get your username!"
	    sys.exit(1)
	self.localnames = [self.remote,]# Local names
	self.password = None	# Password for mail account access
	self.mailboxes = []	# Remote folders to retrieve from
	self.smtphunt = []	# Hosts to forward to
	self.fetchdomains = []	# Domains to fetch from
	self.smtpaddress = None	# Append this to MAIL FROM line
	self.smtpname = None	# Use this for RCPT TO
	self.preconnect = None	# Connection setup
	self.postconnect = None	# Connection wrapup
	self.mda = None		# Mail Delivery Agent
	self.bsmtp = None	# BSMTP output file
	self.lmtp = FALSE	# Use LMTP rather than SMTP?
	self.antispam = ""	# Listener's spam-block code
	self.keep = FALSE	# Keep messages
	self.flush = FALSE	# Flush messages
	self.limitflush = FALSE	# Flush oversized messages
	self.fetchall = FALSE	# Fetch old messages
	self.rewrite = TRUE	# Rewrite message headers
	self.forcecr = FALSE	# Force LF -> CR/LF
	self.stripcr = FALSE	# Strip CR
	self.pass8bits = FALSE	# Force BODY=7BIT
	self.mimedecode = FALSE	# Undo MIME armoring
	self.dropstatus = FALSE	# Drop incoming Status lines
	self.dropdelivered = FALSE     # Drop incoming Delivered-To lines
	self.idle = FALSE	       # IDLE after poll
	self.limit = 0		# Message size limit
	self.warnings = 3600	# Size warning interval (see tunable.h)
	self.fetchlimit = 0	# Max messages fetched per batch
	self.fetchsizelimit = 100	# Max message sizes fetched per transaction
	self.fastuidl = 4	# Do fast uidl 3 out of 4 times
	self.batchlimit = 0	# Max message forwarded per batch
	self.expunge = 0	# Interval between expunges (IMAP)
	self.ssl = 0		# Enable Seccure Socket Layer
	self.sslkey = None	# SSL key filename
	self.sslcert = None	# SSL certificate filename
	self.sslproto = None	# Force SSL?
	self.sslcertck = 0	# Enable strict SSL cert checking
	self.sslcertpath = None	# Path to trusted certificates
	self.sslcommonname = None	# SSL CommonName to expect
	self.sslfingerprint = None	# SSL key fingerprint to check
	self.properties = None	# Extension properties
	User.typemap = (
	    ('remote',	    'String'),
	    # leave out mailboxes and localnames
	    ('password',    'String'),
	    # Leave out smtphunt, fetchdomains
	    ('smtpaddress', 'String'),
	    ('smtpname', 'String'),
	    ('preconnect',  'String'),
	    ('postconnect', 'String'),
	    ('mda',	 'String'),
	    ('bsmtp',	    'String'),
	    ('lmtp',	'Boolean'),
	    ('antispam',    'String'),
	    ('keep',	'Boolean'),
	    ('flush',	    'Boolean'),
	    ('limitflush',  'Boolean'),
	    ('fetchall',    'Boolean'),
	    ('rewrite',     'Boolean'),
	    ('forcecr',     'Boolean'),
	    ('stripcr',     'Boolean'),
	    ('pass8bits',   'Boolean'),
	    ('mimedecode',  'Boolean'),
	    ('dropstatus',  'Boolean'),
	    ('dropdelivered', 'Boolean'),
	    ('idle',	'Boolean'),
	    ('limit',	    'Int'),
	    ('warnings',    'Int'),
	    ('fetchlimit',  'Int'),
	    ('fetchsizelimit',	'Int'),
	    ('fastuidl',    'Int'),
	    ('batchlimit',  'Int'),
	    ('expunge',     'Int'),
	    ('ssl',	 'Boolean'),
	    ('sslkey',	    'String'),
	    ('sslcert',     'String'),
	    ('sslcertck',   'Boolean'),
	    ('sslcertpath', 'String'),
	    ('sslcommonname', 'String'),
	    ('sslfingerprint', 'String'),
	    ('properties',  'String'))

    def __repr__(self):
	res = "    "
	res = res + "user " + `self.remote` + " there ";
	if self.password:
	    res = res + "with password " + `self.password` + " "
	if self.localnames:
	    res = res + "is"
	    for x in self.localnames:
		res = res + " " + `x`
	    res = res + " here"
	if (self.keep != UserDefaults.keep
		or self.flush != UserDefaults.flush
		or self.limitflush != UserDefaults.limitflush
		or self.fetchall != UserDefaults.fetchall
		or self.rewrite != UserDefaults.rewrite
		or self.forcecr != UserDefaults.forcecr
		or self.stripcr != UserDefaults.stripcr
		or self.pass8bits != UserDefaults.pass8bits
		or self.mimedecode != UserDefaults.mimedecode
		or self.dropstatus != UserDefaults.dropstatus
		or self.dropdelivered != UserDefaults.dropdelivered
		or self.idle != UserDefaults.idle):
	    res = res + " options"
	if self.keep != UserDefaults.keep:
	    res = res + flag2str(self.keep, 'keep')
	if self.flush != UserDefaults.flush:
	    res = res + flag2str(self.flush, 'flush')
	if self.limitflush != UserDefaults.limitflush:
	    res = res + flag2str(self.limitflush, 'limitflush')
	if self.fetchall != UserDefaults.fetchall:
	    res = res + flag2str(self.fetchall, 'fetchall')
	if self.rewrite != UserDefaults.rewrite:
	    res = res + flag2str(self.rewrite, 'rewrite')
	if self.forcecr != UserDefaults.forcecr:
	    res = res + flag2str(self.forcecr, 'forcecr')
	if self.stripcr != UserDefaults.stripcr:
	    res = res + flag2str(self.stripcr, 'stripcr')
	if self.pass8bits != UserDefaults.pass8bits:
	    res = res + flag2str(self.pass8bits, 'pass8bits')
	if self.mimedecode != UserDefaults.mimedecode:
	    res = res + flag2str(self.mimedecode, 'mimedecode')
	if self.dropstatus != UserDefaults.dropstatus:
	    res = res + flag2str(self.dropstatus, 'dropstatus')
	if self.dropdelivered != UserDefaults.dropdelivered:
	    res = res + flag2str(self.dropdelivered, 'dropdelivered')
	if self.idle != UserDefaults.idle:
	    res = res + flag2str(self.idle, 'idle')
	if self.limit != UserDefaults.limit:
	    res = res + " limit " + `self.limit`
	if self.warnings != UserDefaults.warnings:
	    res = res + " warnings " + `self.warnings`
	if self.fetchlimit != UserDefaults.fetchlimit:
	    res = res + " fetchlimit " + `self.fetchlimit`
	if self.fetchsizelimit != UserDefaults.fetchsizelimit:
	    res = res + " fetchsizelimit " + `self.fetchsizelimit`
	if self.fastuidl != UserDefaults.fastuidl:
	    res = res + " fastuidl " + `self.fastuidl`
	if self.batchlimit != UserDefaults.batchlimit:
	    res = res + " batchlimit " + `self.batchlimit`
	if self.ssl and self.ssl != UserDefaults.ssl:
	    res = res + flag2str(self.ssl, 'ssl')
	if self.sslkey and self.sslkey != UserDefaults.sslkey:
	    res = res + " sslkey " + `self.sslkey`
	if self.sslcert and self.sslcert != UserDefaults.sslcert:
	    res = res + " sslcert " + `self.sslcert`
	if self.sslproto and self.sslproto != UserDefaults.sslproto:
	    res = res + " sslproto " + `self.sslproto`
	if self.sslcertck and self.sslcertck != UserDefaults.sslcertck:
	    res = res +  flag2str(self.sslcertck, 'sslcertck')
	if self.sslcertpath and self.sslcertpath != UserDefaults.sslcertpath:
	    res = res + " sslcertpath " + `self.sslcertpath`
	if self.sslcommonname and self.sslcommonname != UserDefaults.sslcommonname:
	    res = res + " sslcommonname " + `self.sslcommonname`
	if self.sslfingerprint and self.sslfingerprint != UserDefaults.sslfingerprint:
	    res = res + " sslfingerprint " + `self.sslfingerprint`
	if self.expunge != UserDefaults.expunge:
	    res = res + " expunge " + `self.expunge`
	res = res + "\n"
	trimmed = self.smtphunt;
	if trimmed != [] and trimmed[len(trimmed) - 1] == "localhost":
	    trimmed = trimmed[0:len(trimmed) - 1]
	if trimmed != [] and trimmed[len(trimmed) - 1] == hostname:
	    trimmed = trimmed[0:len(trimmed) - 1]
	if trimmed != []:
	    res = res + "    smtphost "
	    for x in trimmed:
		res = res + " " + x
		res = res + "\n"
	trimmed = self.fetchdomains
	if trimmed != [] and trimmed[len(trimmed) - 1] == hostname:
	    trimmed = trimmed[0:len(trimmed) - 1]
	if trimmed != []:
	    res = res + "    fetchdomains "
	    for x in trimmed:
		res = res + " " + x
		res = res + "\n"
	if self.mailboxes:
	     res = res + "    folder"
	     for x in self.mailboxes:
		res = res + ' "%s"' % x
	     res = res + "\n"
	for fld in ('smtpaddress', 'preconnect', 'postconnect', 'mda', 'bsmtp', 'properties'):
	    if getattr(self, fld):
		res = res + " %s %s\n" % (fld, `getattr(self, fld)`)
	if self.lmtp != UserDefaults.lmtp:
	    res = res + flag2str(self.lmtp, 'lmtp')
	if self.antispam != UserDefaults.antispam:
	    res = res + "    antispam " + self.antispam + "\n"
	return res;

    def __str__(self):
	return "[User: " + repr(self) + "]"

#
# Helper code
#

# IANA port assignments and bogus 1109 entry
ianaservices = {"pop2":109,
		"pop3":110,
		"1109":1109,
		"imap":143,
		"smtp":25,
		"odmr":366}

# fetchmail protocol to IANA service name
defaultports = {"auto":None,
		"POP2":"pop2",
		"POP3":"pop3",
		"APOP":"pop3",
		"KPOP":"1109",
		"IMAP":"imap",
		"ETRN":"smtp",
		"ODMR":"odmr"}

authlist = ("any", "password", "gssapi", "kerberos", "ssh", "otp",
	    "msn", "ntlm")

listboxhelp = {
    'title' : 'List Selection Help',
    'banner': 'List Selection',
    'text' : """
You must select an item in the list box (by clicking on it).
"""}

def flag2str(value, string):
# make a string representation of a .fetchmailrc flag or negated flag
    str = ""
    if value != None:
	str = str + (" ")
	if value == FALSE: str = str + ("no ")
	str = str + string;
    return str

class LabeledEntry(Frame):
# widget consisting of entry field with caption to left
    def bind(self, key, action):
	self.E.bind(key, action)
    def focus_set(self):
	self.E.focus_set()
    def __init__(self, Master, text, textvar, lwidth, ewidth=12):
	Frame.__init__(self, Master)
	self.L = Label(self, {'text':text, 'width':lwidth, 'anchor':'w'})
	self.E = Entry(self, {'textvar':textvar, 'width':ewidth})
	self.L.pack({'side':'left'})
	self.E.pack({'side':'left', 'expand':'1', 'fill':'x'})

def ButtonBar(frame, legend, ref, alternatives, depth, command):
# array of radio buttons, caption to left, picking from a string list
    bar = Frame(frame)
    width = (len(alternatives)+1) / depth;
    Label(bar, text=legend).pack(side=LEFT)
    for column in range(width):
	subframe = Frame(bar)
	for row in range(depth):
	    ind = width * row + column
	    if ind < len(alternatives):
		Radiobutton(subframe,
			{'text':alternatives[ind],
			 'variable':ref,
			 'value':alternatives[ind],
			 'command':command}).pack(side=TOP, anchor=W)
	    else:
		# This is just a spacer
		Radiobutton(subframe,
			{'text':" ",'state':DISABLED}).pack(side=TOP, anchor=W)
	subframe.pack(side=LEFT)
    bar.pack(side=TOP);
    return bar

def helpwin(helpdict):
# help message window with a self-destruct button
    helpwin = Toplevel()
    helpwin.title(helpdict['title'])
    helpwin.iconname(helpdict['title'])
    Label(helpwin, text=helpdict['banner']).pack()
    textframe = Frame(helpwin)
    scroll = Scrollbar(textframe)
    helpwin.textwidget = Text(textframe, setgrid=TRUE)
    textframe.pack(side=TOP, expand=YES, fill=BOTH)
    helpwin.textwidget.config(yscrollcommand=scroll.set)
    helpwin.textwidget.pack(side=LEFT, expand=YES, fill=BOTH)
    scroll.config(command=helpwin.textwidget.yview)
    scroll.pack(side=RIGHT, fill=BOTH)
    helpwin.textwidget.insert(END, helpdict['text']);
    Button(helpwin, text='Done',
	   command=lambda x=helpwin: x.destroy(), bd=2).pack()
    textframe.pack(side=TOP)

def make_icon_window(base, image):
    try:
	# Some older pythons will error out on this
	icon_image = PhotoImage(data=image)
	icon_window = Toplevel()
	Label(icon_window, image=icon_image, bg='black').pack()
	base.master.iconwindow(icon_window)
	# Avoid TkInter brain death. PhotoImage objects go out of
	# scope when the enclosing function returns.  Therefore
	# we have to explicitly link them to something.
	base.keepalive.append(icon_image)
    except:
	pass

class ListEdit(Frame):
# edit a list of values (duplicates not allowed) with a supplied editor hook
    def __init__(self, newlegend, list, editor, deletor, master, helptxt):
	self.editor = editor
	self.deletor = deletor
	self.list = list

	# Set up a widget to accept new elements
	self.newval = StringVar(master)
	newwin = LabeledEntry(master, newlegend, self.newval, '12')
	newwin.bind('<Double-1>', self.handleNew)
	newwin.bind('<Return>', self.handleNew)
	newwin.pack(side=TOP, fill=X, anchor=E)

	# Edit the existing list
	listframe = Frame(master)
	scroll = Scrollbar(listframe)
	self.listwidget = Listbox(listframe, height=0, selectmode='browse')
	if self.list:
	    for x in self.list:
		self.listwidget.insert(END, x)
	listframe.pack(side=TOP, expand=YES, fill=BOTH)
	self.listwidget.config(yscrollcommand=scroll.set)
	self.listwidget.pack(side=LEFT, expand=YES, fill=BOTH)
	scroll.config(command=self.listwidget.yview)
	scroll.pack(side=RIGHT, fill=BOTH)
	self.listwidget.config(selectmode=SINGLE, setgrid=TRUE)
	self.listwidget.bind('<Double-1>', self.handleList);
	self.listwidget.bind('<Return>', self.handleList);

	bf = Frame(master);
	if self.editor:
	    Button(bf, text='Edit',   command=self.editItem).pack(side=LEFT)
	Button(bf, text='Delete', command=self.deleteItem).pack(side=LEFT)
	if helptxt:
	    self.helptxt = helptxt
	    Button(bf, text='Help', fg='blue',
		   command=self.help).pack(side=RIGHT)
	bf.pack(fill=X)

    def help(self):
	helpwin(self.helptxt)

    def handleList(self, event):
	self.editItem();

    def handleNew(self, event):
	item = self.newval.get()
	if item:
	    entire = self.listwidget.get(0, self.listwidget.index('end'));
	    if item and (not entire) or (not item in self.listwidget.get(0, self.listwidget.index('end'))):
		self.listwidget.insert('end', item)
		if self.list != None: self.list.append(item)
		if self.editor:
		    apply(self.editor, (item,))
	    self.newval.set('')

    def editItem(self):
	select = self.listwidget.curselection()
	if not select:
	    helpwin(listboxhelp)
	else:
	    index = select[0]
	    if index and self.editor:
		label = self.listwidget.get(index);
		if self.editor:
		    apply(self.editor, (label,))

    def deleteItem(self):
	select = self.listwidget.curselection()
	if not select:
	    helpwin(listboxhelp)
	else:
	    index = string.atoi(select[0])
	    label = self.listwidget.get(index);
	    self.listwidget.delete(index)
	    if self.list != None:
		del self.list[index]
	    if self.deletor != None:
		apply(self.deletor, (label,))

def ConfirmQuit(frame, context):
    ans = Dialog(frame,
		 title = 'Quit?',
		 text = 'Really quit ' + context + ' without saving?',
		 bitmap = 'question',
		 strings = ('Yes', 'No'),
		 default = 1)
    return ans.num == 0

def dispose_window(master, legend, help, savelegend='OK'):
    dispose = Frame(master, relief=RAISED, bd=5)
    Label(dispose, text=legend).pack(side=TOP,pady=10)
    Button(dispose, text=savelegend, fg='blue',
	   command=master.save).pack(side=LEFT)
    Button(dispose, text='Quit', fg='blue',
	   command=master.nosave).pack(side=LEFT)
    Button(dispose, text='Help', fg='blue',
	   command=lambda x=help: helpwin(x)).pack(side=RIGHT)
    dispose.pack(fill=X)
    return dispose

class MyWidget:
# Common methods for Tkinter widgets -- deals with Tkinter declaration
    def post(self, widgetclass, field):
	for x in widgetclass.typemap:
	    if x[1] == 'Boolean':
		setattr(self, x[0], BooleanVar(self))
	    elif x[1] == 'String':
		setattr(self, x[0], StringVar(self))
	    elif x[1] == 'Int':
		setattr(self, x[0], IntVar(self))
	    source = getattr(getattr(self, field), x[0])
	    if source:
		getattr(self, x[0]).set(source)

    def fetch(self, widgetclass, field):
	for x in widgetclass.typemap:
	    setattr(getattr(self, field), x[0], getattr(self, x[0]).get())

#
# First, code to set the global fetchmail run controls.
#

configure_novice_help = {
    'title' : 'Fetchmail novice configurator help',
    'banner': 'Novice configurator help',
    'text' : """
In the `Novice Configurator Controls' panel, you can:

Press `Save' to save the new fetchmail configuration you have created.
Press `Quit' to exit without saving.
Press `Help' to bring up this help message.

In the `Novice Configuration' panels, you will set up the basic data
needed to create a simple fetchmail setup.  These include:

1. The name of the remote site you want to query.

2. Your login name on that site.

3. Your password on that site.

4. A protocol to use (POP, IMAP, ETRN, etc.)

5. A poll interval in seconds.
   If 0, fetchmail will run in the foreground once when started.
   If > 0, fetchmail will run in the background and start a new poll
   cycle after the interval has elapsed.

6. Options to fetch old messages as well as new, or to suppress
   deletion of fetched message.

The novice-configuration code will assume that you want to forward mail
to a local sendmail listener with no special options.
"""}

configure_expert_help = {
    'title' : 'Fetchmail expert configurator help',
    'banner': 'Expert configurator help',
    'text' : """
In the `Expert Configurator Controls' panel, you can:

Press `Save' to save the new fetchmail configuration you have edited.
Press `Quit' to exit without saving.
Press `Help' to bring up this help message.

In the `Run Controls' panel, you can set the following options that
control how fetchmail runs:

Poll interval
	Number of seconds to wait between polls in the background.
	If zero, fetchmail will run in foreground.

Logfile
	If empty, emit progress and error messages to stderr.
	Otherwise this gives the name of the files to write to.
	This field is ignored if the "Log to syslog?" option is on.

Idfile
	If empty, store seen-message IDs in .fetchids under user's home
	directory.  If nonempty, use given file name.

Postmaster
	Who to send multidrop mail to as a last resort if no address can
	be matched.  Normally empty; in this case, fetchmail treats the
	invoking user as the address of last resort unless that user is
	root.  If that user is root, fetchmail sends to `postmaster'.

Bounces to sender?
	If this option is on (the default) error mail goes to the sender.
	Otherwise it goes to the postmaster.

Send spam bounces?
	If this option is on, spam bounces are sent to the sender or
	postmaster (depending on the "Bounces to sender?" option.  Otherwise,
	spam bounces are not sent (the default).

Use soft bounces?
	If this option is on, permanent delivery errors are treated as
	temporary, i. e. mail is kept on the upstream server. Useful
	during testing and after configuration changes, and on by
	default.
	  If this option is off, permanent delivery errors delete
	undeliverable mail from the upstream.

Invisible
	If false (the default) fetchmail generates a Received line into
	each message and generates a HELO from the machine it is running on.
	If true, fetchmail generates no Received line and HELOs as if it were
	the remote site.

In the `Remote Mail Configurations' panel, you can:

1. Enter the name of a new remote mail server you want fetchmail to query.

To do this, simply enter a label for the poll configuration in the
`New Server:' box.  The label should be a DNS name of the server (unless
you are using ssh or some other tunneling method and will fill in the `via'
option on the site configuration screen).

2. Change the configuration of an existing site.

To do this, find the site's label in the listbox and double-click it.
This will take you to a site configuration dialogue.
"""}


class ConfigurationEdit(Frame, MyWidget):
    def __init__(self, configuration, outfile, master, onexit):
	self.subwidgets = {}
	self.configuration = configuration
	self.outfile = outfile
	self.container = master
	self.onexit = onexit
	ConfigurationEdit.mode_to_help = {
	    'novice':configure_novice_help, 'expert':configure_expert_help
	    }

    def server_edit(self, sitename):
	self.subwidgets[sitename] = ServerEdit(sitename, self).edit(self.mode, Toplevel())

    def server_delete(self, sitename):
	try:
	    for user in self.subwidgets.keys():
		user.destruct()
	    del self.configuration[sitename]
	except:
	    pass

    def edit(self, mode):
	self.mode = mode
	Frame.__init__(self, self.container)
	self.master.title('fetchmail ' + self.mode + ' configurator');
	self.master.iconname('fetchmail ' + self.mode + ' configurator');
	self.master.protocol('WM_DELETE_WINDOW', self.nosave)
	self.keepalive = []	# Use this to anchor the PhotoImage object
	make_icon_window(self, fetchmail_icon)
	Pack.config(self)
	self.post(Configuration, 'configuration')

	dispose_window(self,
		       'Configurator ' + self.mode + ' Controls',
		       ConfigurationEdit.mode_to_help[self.mode],
		       'Save')

	gf = Frame(self, relief=RAISED, bd = 5)
	Label(gf,
		text='Fetchmail Run Controls',
		bd=2).pack(side=TOP, pady=10)

	df = Frame(gf)

	ff = Frame(df)
	if self.mode != 'novice':
	    # Set the postmaster
	    log = LabeledEntry(ff, '	 Postmaster:', self.postmaster, '14')
	    log.pack(side=RIGHT, anchor=E)

	# Set the poll interval
	de = LabeledEntry(ff, '     Poll interval:', self.poll_interval, '14')
	de.pack(side=RIGHT, anchor=E)
	ff.pack()

	df.pack()

	if self.mode != 'novice':
	    pf = Frame(gf)
	    Checkbutton(pf,
		{'text':'Bounces to sender?',
		'variable':self.bouncemail,
		'relief':GROOVE}).pack(side=LEFT, anchor=W)
	    pf.pack(fill=X)

	    sb = Frame(gf)
	    Checkbutton(sb,
		{'text':'Send spam bounces?',
		'variable':self.spambounce,
		'relief':GROOVE}).pack(side=LEFT, anchor=W)
	    sb.pack(fill=X)

	    sb = Frame(gf)
	    Checkbutton(sb,
		{'text':'Treat permanent errors as temporary?',
		'variable':self.softbounce,
		'relief':GROOVE}).pack(side=LEFT, anchor=W)
	    sb.pack(fill=X)

	    sf = Frame(gf)
	    Checkbutton(sf,
		{'text':'Log to syslog?',
		'variable':self.syslog,
		'relief':GROOVE}).pack(side=LEFT, anchor=W)
	    log = LabeledEntry(sf, '	 Logfile:', self.logfile, '14')
	    log.pack(side=RIGHT, anchor=E)
	    sf.pack(fill=X)

	    Checkbutton(gf,
		{'text':'Invisible mode?',
		'variable':self.invisible,
		 'relief':GROOVE}).pack(side=LEFT, anchor=W)
	    # Set the idfile
	    log = LabeledEntry(gf, '	 Idfile:', self.idfile, '14')
	    log.pack(side=RIGHT, anchor=E)

	gf.pack(fill=X)

	# Expert mode allows us to edit multiple sites
	lf = Frame(self, relief=RAISED, bd=5)
	Label(lf,
	      text='Remote Mail Server Configurations',
	      bd=2).pack(side=TOP, pady=10)
	ListEdit('New Server:',
		map(lambda x: x.pollname, self.configuration.servers),
		lambda site, self=self: self.server_edit(site),
		lambda site, self=self: self.server_delete(site),
		lf, remotehelp)
	lf.pack(fill=X)

    def destruct(self):
	for sitename in self.subwidgets.keys():
	    self.subwidgets[sitename].destruct()
	self.master.destroy()
	self.onexit()

    def nosave(self):
	if ConfirmQuit(self, self.mode + " configuration editor"):
	    self.destruct()

    def save(self):
	for sitename in self.subwidgets.keys():
	    self.subwidgets[sitename].save()
	self.fetch(Configuration, 'configuration')
	fm = None
	if not self.outfile:
	    fm = sys.stdout
	elif not os.path.isfile(self.outfile) or Dialog(self,
		 title = 'Overwrite existing run control file?',
		 text = 'Really overwrite existing run control file?',
		 bitmap = 'question',
		 strings = ('Yes', 'No'),
		 default = 1).num == 0:
	    try:
		os.rename(self.outfile, self.outfile + "~")
	    # Pre-1.5.2 compatibility...
	    except os.error:
		pass
	    oldumask = os.umask(077)
	    fm = open(self.outfile, 'w')
	    os.umask(oldumask)
	if fm:
	    # be paranoid
	    if fm != sys.stdout:
		os.chmod(self.outfile, 0600)
	    fm.write("# Configuration created %s by fetchmailconf %s\n" % (time.ctime(time.time()), version))
	    fm.write(`self.configuration`)
	    if self.outfile:
		fm.close()
	    self.destruct()

#
# Server editing stuff.
#
remotehelp = {
    'title' : 'Remote site help',
    'banner': 'Remote sites',
    'text' : """
When you add a site name to the list here,
you initialize an entry telling fetchmail
how to poll a new site.

When you select a sitename (by double-
clicking it, or by single-clicking to
select and then clicking the Edit button),
you will open a window to configure that
site.
"""}

serverhelp = {
    'title' : 'Server options help',
    'banner': 'Server Options',
    'text' : """
The server options screen controls fetchmail
options that apply to one of your mailservers.

Once you have a mailserver configuration set
up as you like it, you can select `OK' to
store it in the server list maintained in
the main configuration window.

If you wish to discard changes to a server
configuration, select `Quit'.
"""}

controlhelp = {
    'title' : 'Run Control help',
    'banner': 'Run Controls',
    'text' : """
If the `Poll normally' checkbox is on, the host is polled as part of
the normal operation of fetchmail when it is run with no arguments.
If it is off, fetchmail will only query this host when it is given as
a command-line argument.

The `True name of server' box should specify the actual DNS name
to query. By default this is the same as the poll name.

Normally each host described in the file is queried once each
poll cycle. If `Cycles to skip between polls' is greater than 0,
that's the number of poll cycles that are skipped between the
times this post is actually polled.

The `Server timeout' is the number of seconds fetchmail will wait
for a reply from the mailserver before concluding it is hung and
giving up.
"""}

protohelp = {
    'title' : 'Protocol and Port help',
    'banner': 'Protocol and Port',
    'text' : """
These options control the remote-mail protocol
and TCP/IP service port used to query this
server.

If you click the `Probe for supported protocols'
button, fetchmail will try to find you the most
capable server on the selected host (this will
only work if you're conncted to the Internet).
The probe only checks for ordinary IMAP and POP
protocols; fortunately these are the most
frequently supported.

The `Protocol' button bar offers you a choice of
all the different protocols available.	The `auto'
protocol is the default mode; it probes the host
ports for POP3 and IMAP to see if either is
available.

Normally the TCP/IP service port to use is
dictated by the protocol choice.  The `Service'
field (only present in expert mode) lets you
set a non-standard service (port).
"""}

sechelp = {
    'title' : 'Security option help',
    'banner': 'Security',
    'text' : """
The 'authorization mode' allows you to choose the
mode that fetchmail uses to log in to your server. You
can usually leave this at 'any', but you will have to pick
'NTLM' and 'MSN' manually for the nonce.

The 'interface' option allows you to specify a range
of IP addresses to monitor for activity.  If these
addresses are not active, fetchmail will not poll.
Specifying this may protect you from a spoofing attack
if your client machine has more than one IP gateway
address and some of the gateways are to insecure nets.

The `monitor' option, if given, specifies the only
device through which fetchmail is permitted to connect
to servers.  This option may be used to prevent
fetchmail from triggering an expensive dial-out if the
interface is not already active.

The `interface' and `monitor' options are available
only for Linux and freeBSD systems.  See the fetchmail
manual page for details on these.

The ssl option enables SSL communication with a mailserver
supporting Secure Sockets Layer. The sslkey and sslcert options
declare key and certificate files for use with SSL.
The sslcertck option enables strict checking of SSL server
certificates (and sslcertpath gives the trusted certificate
directory). The sslcommonname option helps if the server is
misconfigured and returning "Server CommonName mismatch"
warnings. With sslfingerprint, you can specify a finger-
print the server's key is checked against.
"""}

multihelp = {
    'title' : 'Multidrop option help',
    'banner': 'Multidrop',
    'text' : """
These options are only useful with multidrop mode.
See the manual page for extended discussion.
"""}

suserhelp = {
    'title' : 'User list help',
    'banner': 'User list',
    'text' : """
When you add a user name to the list here,
you initialize an entry telling fetchmail
to poll the site on behalf of the new user.

When you select a username (by double-
clicking it, or by single-clicking to
select and then clicking the Edit button),
you will open a window to configure the
user's options on that site.
"""}

class ServerEdit(Frame, MyWidget):
    def __init__(self, host, parent):
	self.parent = parent
	self.server = None
	self.subwidgets = {}
	for site in parent.configuration.servers:
	    if site.pollname == host:
		self.server = site
	if (self.server == None):
		self.server = Server()
		self.server.pollname = host
		self.server.via = None
		parent.configuration.servers.append(self.server)

    def edit(self, mode, master=None):
	Frame.__init__(self, master)
	Pack.config(self)
	self.master.title('Fetchmail host ' + self.server.pollname);
	self.master.iconname('Fetchmail host ' + self.server.pollname);
	self.post(Server, 'server')
	self.makeWidgets(self.server.pollname, mode)
	self.keepalive = []	# Use this to anchor the PhotoImage object
	make_icon_window(self, fetchmail_icon)
#	self.grab_set()
#	self.focus_set()
#	self.wait_window()
	return self

    def destruct(self):
	for username in self.subwidgets.keys():
	    self.subwidgets[username].destruct()
	del self.parent.subwidgets[self.server.pollname]
	self.master.destroy()

    def nosave(self):
	if ConfirmQuit(self, 'server option editing'):
	    self.destruct()

    def save(self):
	self.fetch(Server, 'server')
	for username in self.subwidgets.keys():
	    self.subwidgets[username].save()
	self.destruct()

    def defaultPort(self):
	proto = self.protocol.get()
	# Callback to reset the port number whenever the protocol type changes.
	# We used to only reset the port if it had a default (zero) value.
	# This turns out to be a bad idea especially in Novice mode -- if
	# you set POP3 and then set IMAP, the port invisibly remained 110.
	# Now we reset unconditionally on the theory that if you're setting
	# a custom port number you should be in expert mode and playing
	# close enough attention to notice this...
	self.service.set(defaultports[proto])
	if not proto in ("POP3", "APOP", "KPOP"): self.uidl.state = DISABLED

    def user_edit(self, username, mode):
	self.subwidgets[username] = UserEdit(username, self).edit(mode, Toplevel())

    def user_delete(self, username):
	if self.subwidgets.has_key(username):
	    self.subwidgets[username].destruct()
	del self.server[username]

    def makeWidgets(self, host, mode):
	topwin = dispose_window(self, "Server options for querying " + host, serverhelp)

	leftwin = Frame(self);
	leftwidth = '25';

	if mode != 'novice':
	    ctlwin = Frame(leftwin, relief=RAISED, bd=5)
	    Label(ctlwin, text="Run Controls").pack(side=TOP)
	    Checkbutton(ctlwin, text='Poll ' + host + ' normally?', variable=self.active).pack(side=TOP)
	    LabeledEntry(ctlwin, 'True name of ' + host + ':',
		      self.via, leftwidth).pack(side=TOP, fill=X)
	    LabeledEntry(ctlwin, 'Cycles to skip between polls:',
		      self.interval, leftwidth).pack(side=TOP, fill=X)
	    LabeledEntry(ctlwin, 'Server timeout (seconds):',
		      self.timeout, leftwidth).pack(side=TOP, fill=X)
	    Button(ctlwin, text='Help', fg='blue',
	       command=lambda: helpwin(controlhelp)).pack(side=RIGHT)
	    ctlwin.pack(fill=X)

	# Compute the available protocols from the compile-time options
	protolist = ['auto']
	if 'pop2' in feature_options:
	    protolist.append("POP2")
	if 'pop3' in feature_options:
	    protolist = protolist + ["POP3", "APOP", "KPOP"]
	if 'sdps' in feature_options:
	    protolist.append("SDPS")
	if 'imap' in feature_options:
	    protolist.append("IMAP")
	if 'etrn' in feature_options:
	    protolist.append("ETRN")
	if 'odmr' in feature_options:
	    protolist.append("ODMR")

	protwin = Frame(leftwin, relief=RAISED, bd=5)
	Label(protwin, text="Protocol").pack(side=TOP)
	ButtonBar(protwin, '',
		  self.protocol, protolist, 2,
		  self.defaultPort)
	if mode != 'novice':
	    LabeledEntry(protwin, 'On server TCP/IP service:',
		      self.service, leftwidth).pack(side=TOP, fill=X)
	    self.defaultPort()
	    Checkbutton(protwin,
		text="POP3: track `seen' with client-side UIDLs?",
		variable=self.uidl).pack(side=TOP)
	Button(protwin, text='Probe for supported protocols', fg='blue',
	       command=self.autoprobe).pack(side=LEFT)
	Button(protwin, text='Help', fg='blue',
	       command=lambda: helpwin(protohelp)).pack(side=RIGHT)
	protwin.pack(fill=X)

	userwin = Frame(leftwin, relief=RAISED, bd=5)
	Label(userwin, text="User entries for " + host).pack(side=TOP)
	ListEdit("New user: ",
		 map(lambda x: x.remote, self.server.users),
		 lambda u, m=mode, s=self: s.user_edit(u, m),
		 lambda u, s=self: s.user_delete(u),
		 userwin, suserhelp)
	userwin.pack(fill=X)

	leftwin.pack(side=LEFT, anchor=N, fill=X);

	if mode != 'novice':
	    rightwin = Frame(self);

	    mdropwin = Frame(rightwin, relief=RAISED, bd=5)
	    Label(mdropwin, text="Multidrop options").pack(side=TOP)
	    LabeledEntry(mdropwin, 'Envelope address header:',
		      self.envelope, '22').pack(side=TOP, fill=X)
	    LabeledEntry(mdropwin, 'Envelope headers to skip:',
		      self.envskip, '22').pack(side=TOP, fill=X)
	    LabeledEntry(mdropwin, 'Name prefix to strip:',
		      self.qvirtual, '22').pack(side=TOP, fill=X)
	    Checkbutton(mdropwin, text="Enable multidrop DNS lookup?",
		    variable=self.dns).pack(side=TOP)
	    Label(mdropwin, text="DNS aliases").pack(side=TOP)
	    ListEdit("New alias: ", self.server.aka, None, None, mdropwin, None)
	    Label(mdropwin, text="Domains to be considered local").pack(side=TOP)
	    ListEdit("New domain: ",
		 self.server.localdomains, None, None, mdropwin, multihelp)
	    mdropwin.pack(fill=X)

	    if os_type in ('linux', 'freebsd'):
		secwin = Frame(rightwin, relief=RAISED, bd=5)
		Label(secwin, text="Security").pack(side=TOP)
		# Don't actually let users set this.  KPOP sets it implicitly
		ButtonBar(secwin, 'Authorization mode:',
			 self.auth, authlist, 2, None).pack(side=TOP)
		if os_type == 'linux' or os_type == 'freebsd'  or 'interface' in dictmembers:
		    LabeledEntry(secwin, 'IP range to check before poll:',
			 self.interface, leftwidth).pack(side=TOP, fill=X)
		if os_type == 'linux' or os_type == 'freebsd' or 'monitor' in dictmembers:
		    LabeledEntry(secwin, 'Interface to monitor:',
			 self.monitor, leftwidth).pack(side=TOP, fill=X)
		# Someday this should handle Kerberos 5 too
		if 'kerberos' in feature_options:
		    LabeledEntry(secwin, 'Principal:',
			 self.principal, '12').pack(side=TOP, fill=X)
		# ESMTP authentication
		LabeledEntry(secwin, 'ESMTP name:',
			     self.esmtpname, '12').pack(side=TOP, fill=X)
		LabeledEntry(secwin, 'ESMTP password:',
			     self.esmtppassword, '12').pack(side=TOP, fill=X)
		Button(secwin, text='Help', fg='blue',
		       command=lambda: helpwin(sechelp)).pack(side=RIGHT)
		secwin.pack(fill=X)

	    rightwin.pack(side=LEFT, anchor=N);

    def autoprobe(self):
	# Note: this only handles case (1) near fetchmail.c:1032
	# We're assuming people smart enough to set up ssh tunneling
	# won't need autoprobing.
	if self.server.via:
	    realhost = self.server.via
	else:
	    realhost = self.server.pollname
	greetline = None
	for protocol in ("IMAP","POP3","POP2"):
	    service = defaultports[protocol]
	    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
	    try:
		sock.connect((realhost, ianaservices[service]))
		greetline = sock.recv(1024)
		sock.close()
	    except:
		pass
	    else:
		break
	confwin = Toplevel()
	if greetline == None:
	    title = "Autoprobe of " + realhost + " failed"
	    confirm = """
Fetchmailconf didn't find any mailservers active.
This could mean the host doesn't support any,
or that your Internet connection is down, or
that the host is so slow that the probe timed
out before getting a response.
"""
	else:
	    warnings = ''
	    # OK, now try to recognize potential problems

	    if protocol == "POP2":
		warnings = warnings + """
It appears you have somehow found a mailserver running only POP2.
Congratulations.  Have you considered a career in archaeology?

Unfortunately, stock fetchmail binaries don't include POP2 support anymore.
Unless the first line of your fetchmail -V output includes the string "POP2",
you'll have to build it from sources yourself with the configure
switch --enable-POP2.

"""

### POP3 servers start here

	    if string.find(greetline, "1.003") > 0 or string.find(greetline, "1.004") > 0:
		warnings = warnings + """
This appears to be an old version of the UC Davis POP server.  These are
dangerously unreliable (among other problems, they may drop your mailbox
on the floor if your connection is interrupted during the session).

It is strongly recommended that you find a better POP3 server.	The fetchmail
FAQ includes pointers to good ones.

"""
	    if string.find(greetline, "comcast.net") > 0:
		warnings = warnings + """
The Comcast Maillennium POP3 server only returns the first 80K of a long
message retrieved with TOP. Its response to RETR is normal, so use the
`fetchall' option.

"""
# Steve VanDevender <stevev@efn.org> writes:
# The only system I have seen this happen with is cucipop-1.31
# under SunOS 4.1.4.  cucipop-1.31 runs fine on at least Solaris
# 2.x and probably quite a few other systems.  It appears to be a
# bug or bad interaction with the SunOS realloc() -- it turns out
# that internally cucipop does allocate a certain data structure in
# multiples of 16, using realloc() to bump it up to the next
# multiple if it needs more.
#
# The distinctive symptom is that when there are 16 messages in the
# inbox, you can RETR and DELE all 16 messages successfully, but on
# QUIT cucipop returns something like "-ERR Error locking your
# mailbox" and aborts without updating it.
#
# The cucipop banner looks like:
#
# +OK Cubic Circle's v1.31 1998/05/13 POP3 ready <6229000062f95036@wakko>
#
	    if string.find(greetline, "Cubic Circle") > 0:
		warnings = warnings + """
I see your server is running cucipop.  Better make sure the server box
isn't a SunOS 4.1.4 machine; cucipop tickles a bug in SunOS realloc()
under that version, and doesn't cope with the result gracefully.  Newer
SunOS and Solaris machines run cucipop OK.

Also, some versions of cucipop don't assert an exclusive lock on your
mailbox when it's being queried.  This means that if you have more than
one fetchmail query running against the same mailbox, bad things can happen.
"""
	    if string.find(greetline, "David POP3 Server") > 0:
		warnings = warnings + """
This POP3 server is badly broken.  You should get rid of it -- and the
brain-dead Microsoft operating system it rode in on.

"""
# The greeting line on the server known to be buggy is:
# +OK POP3 server ready (running FTGate V2, 2, 1, 0 Jun 21 1999 09:55:01)
#
	    if string.find(greetline, "FTGate") > 0:
		warnings = warnings + """
This POP server has a weird bug; it says OK twice in response to TOP.
Its response to RETR is normal, so use the `fetchall' option.

"""
	    if string.find(greetline, " geonet.de") > 0:
		warnings = warnings + """
You appear to be using geonet.	As of late 2002, the TOP command on
geonet's POP3 is broken.  Use the fetchall option.

"""
	    if string.find(greetline, "OpenMail") > 0:
		warnings = warnings + """
You appear to be using some version of HP OpenMail.  Many versions of
OpenMail do not process the "TOP" command correctly; the symptom is that
only the header and first line of each message is retrieved.  To work
around this bug, turn on `fetchall' on all user entries associated with
this server.

"""
	    if string.find(greetline, "Escape character is") > 0:
		warnings = warnings + """
Your greeting line looks like it was written by a fetid pile of
camel dung identified to me as `popa3d written by Solar Designer'.
Beware!  The UIDL support in this thing is known to be completely broken,
and other things probably are too.

"""
	    if string.find(greetline, "MercuryP/NLM v1.48") > 0:
		warnings = warnings + """
This is not a POP3 server.  It has delusions of being one, but after
RETR all messages are automatically marked to be deleted.  The only
way to prevent this is to issue an RSET before leaving the server.
Fetchmail does this, but we suspect this is probably broken in lots
of other ways, too.

"""
	    if string.find(greetline, "POP-Max") > 0:
		warnings = warnings + """
The Mail Max POP3 server screws up on mail with attachments.  It
reports the message size with attachments included, but doesn't
download them on a RETR or TOP (this violates the IMAP RFCs).  It also
doesn't implement TOP correctly.  You should get rid of it -- and the
brain-dead NT server it rode in on.

"""
	    if string.find(greetline, "POP3 Server Ready") > 0:
		warnings = warnings + """
Some server that uses this greeting line has been observed to choke on
TOP %d 99999999.  Use the fetchall option. if necessary, to force RETR.

"""
	    if string.find(greetline, "QPOP") > 0:
		warnings = warnings + """
This appears to be a version of Eudora qpopper.  That's good.  Fetchmail
knows all about qpopper.  However, be aware that the 2.53 version of
qpopper does something odd that causes fetchmail to hang with a socket
error on very large messages.  This is probably not a fetchmail bug, as
it has been observed with fetchpop.  The fix is to upgrade to qpopper
3.0beta or a more recent version.  Better yet, switch to IMAP.

"""
	    if string.find(greetline, " sprynet.com") > 0:
		warnings = warnings + """
You appear to be using a SpryNet server.  In mid-1999 it was reported that
the SpryNet TOP command marks messages seen.  Therefore, for proper error
recovery in the event of a line drop, it is strongly recommended that you
turn on `fetchall' on all user entries associated with this server.

"""
	    if string.find(greetline, "TEMS POP3") > 0:
		warnings = warnings + """
Your POP3 server has "TEMS" in its header line.  At least one such
server does not process the "TOP" command correctly; the symptom is
that fetchmail hangs when trying to retrieve mail.  To work around
this bug, turn on `fetchall' on all user entries associated with this
server.

"""
	    if string.find(greetline, " spray.se") > 0:
		warnings = warnings + """
Your POP3 server has "spray.se" in its header line.  In May 2000 at
least one such server did not process the "TOP" command correctly; the
symptom is that messages are treated as headerless.  To work around
this bug, turn on `fetchall' on all user entries associated with this
server.

"""
	    if string.find(greetline, " usa.net") > 0:
		warnings = warnings + """
You appear to be using USA.NET's free mail service.  Their POP3 servers
(at least as of the 2.2 version in use mid-1998) are quite flaky, but
fetchmail can compensate.  They seem to require that fetchall be switched on
(otherwise you won't necessarily see all your mail, not even new mail).
They also botch the TOP command the fetchmail normally uses for retrieval
(it only retrieves about 10 lines rather than the number specified).
Turning on fetchall will disable the use of TOP.

Therefore, it is strongly recommended that you turn on `fetchall' on all
user entries associated with this server.

"""
	    if string.find(greetline, " Novonyx POP3") > 0:
		warnings = warnings + """
Your mailserver is running Novonyx POP3.  This server, at least as of
version 2.17, seems to have problems handling and reporting seen bits.
You may have to use the fetchall option.

"""
	    if string.find(greetline, " IMS POP3") > 0:
		warnings = warnings + """
Some servers issuing the greeting line 'IMS POP3' have been known to
do byte-stuffing incorrectly.  This means that if a message you receive
has a . (period) at start of line, fetchmail will become confused and
probably wedge itself.	(This bug was recorded on IMS POP3 0.86.)

"""

### IMAP servers start here

	    if string.find(greetline, "GroupWise") > 0:
		warnings = warnings + """
The Novell GroupWise IMAP server would be better named GroupFoolish;
it is (according to the designer of IMAP) unusably broken.  Among
other things, it doesn't include a required content length in its
BODY[TEXT] response.<p>

Fetchmail works around this problem, but we strongly recommend voting
with your dollars for a server that isn't brain-dead.  If you stick
with code as shoddy as GroupWise seems to be, you will probably pay
for it with other problems.<p>

"""
	    if string.find(greetline, "InterChange") > 0:
		warnings = warnings + """

The InterChange IMAP server at release levels below 3.61.08 screws up
on mail with attachments.  It doesn't fetch them if you give it a
BODY[TEXT] request, though it does if you request RFC822.TEXT.
According to the IMAP RFCs and their maintainer these should be
equivalent -- and we can't drop the BODY[TEXT] form because M$
Exchange (quite legally under RFC2062) rejectsit.  The InterChange
folks claim to have fixed this bug in 3.61.08.

"""
	    if string.find(greetline, "Imail") > 0:
		warnings = warnings + """
We've seen a bug report indicating that this IMAP server (at least as of
version 5.0.7) returns an invalid body size for messages with MIME
attachments; the effect is to drop the attachments on the floor.  We
recommend you upgrade to a non-broken IMAP server.

"""
	    if string.find(greetline, "Domino IMAP4") > 0:
		warnings = warnings + """
Your IMAP server appears to be Lotus Domino.  This server, at least up
to version 4.6.2a, has a bug in its generation of MIME boundaries (see
the details in the fetchmail FAQ).  As a result, even MIME aware MUAs
will see attachments as part of the message text.  If your Domino server's
POP3 facility is enabled, we recommend you fall back on it.

"""

### Checks for protocol variants start here

	    closebrak = string.find(greetline, ">")
	    if	closebrak > 0 and greetline[closebrak+1] == "\r":
		warnings = warnings + """
It looks like you could use APOP on this server and avoid sending it your
password in clear.  You should talk to the mailserver administrator about
this.

"""
	    if string.find(greetline, "IMAP2bis") > 0:
		warnings = warnings + """
IMAP2bis servers have a minor problem; they can't peek at messages without
marking them seen.  If you take a line hit during the retrieval, the
interrupted message may get left on the server, marked seen.

To work around this, it is recommended that you set the `fetchall'
option on all user entries associated with this server, so any stuck
mail will be retrieved next time around.

To fix this bug, upgrade to an IMAP4 server.  The fetchmail FAQ includes
a pointer to an open-source implementation.

"""
	    if string.find(greetline, "IMAP4rev1") > 0:
		warnings = warnings + """
I see an IMAP4rev1 server.  Excellent.	This is (a) the best kind of
remote-mail server, and (b) the one the fetchmail author uses.	Fetchmail
has therefore been extremely well tested with this class of server.

"""
	    if warnings == '':
		warnings = warnings + """
Fetchmail doesn't know anything special about this server type.

"""

	    # Display success window with warnings
	    title = "Autoprobe of " + realhost + " succeeded"
	    confirm = "The " + protocol + " server said:\n\n" + greetline + warnings
	    self.protocol.set(protocol)
	    self.service.set(defaultports[protocol])
	confwin.title(title)
	confwin.iconname(title)
	Label(confwin, text=title).pack()
	Message(confwin, text=confirm, width=600).pack()
	Button(confwin, text='Done',
		   command=lambda x=confwin: x.destroy(), bd=2).pack()

#
# User editing stuff
#

userhelp = {
    'title' : 'User option help',
    'banner': 'User options',
    'text' : """
You may use this panel to set options
that may differ between individual
users on your site.

Once you have a user configuration set
up as you like it, you can select `OK' to
store it in the user list maintained in
the site configuration window.

If you wish to discard the changes you have
made to user options, select `Quit'.
"""}

localhelp = {
    'title' : 'Local name help',
    'banner': 'Local names',
    'text' : """
The local name(s) in a user entry are the
people on the client machine who should
receive mail from the poll described.

Note: if a user entry has more than one
local name, messages will be retrieved
in multidrop mode.  This complicates
the configuration issues; see the manual
page section on multidrop mode.

Warning: Be careful with local names
such as foo@bar.com, as that can cause
the mail to be sent to foo@bar.com instead
of sending it to your local system.
"""}

class UserEdit(Frame, MyWidget):
    def __init__(self, username, parent):
	self.parent = parent
	self.user = None
	for user in parent.server.users:
	    if user.remote == username:
		self.user = user
	if self.user == None:
	    self.user = User()
	    self.user.remote = username
	    self.user.localnames = [username]
	    parent.server.users.append(self.user)

    def edit(self, mode, master=None):
	Frame.__init__(self, master)
	Pack.config(self)
	self.master.title('Fetchmail user ' + self.user.remote
			  + ' querying ' + self.parent.server.pollname);
	self.master.iconname('Fetchmail user ' + self.user.remote);
	self.post(User, 'user')
	self.makeWidgets(mode, self.parent.server.pollname)
	self.keepalive = []	# Use this to anchor the PhotoImage object
	make_icon_window(self, fetchmail_icon)
#	self.grab_set()
#	self.focus_set()
#	self.wait_window()
	return self

    def destruct(self):
	# Yes, this test can fail -- if you delete the parent window.
	if self.parent.subwidgets.has_key(self.user.remote):
	    del self.parent.subwidgets[self.user.remote]
	self.master.destroy()

    def nosave(self):
	if ConfirmQuit(self, 'user option editing'):
	    self.destruct()

    def save(self):
	ok = 0
	for x in self.user.localnames: ok = ok + (string.find(x, '@') != -1)
	if ok == 0 or  Dialog(self,
	    title = "Really accept an embedded '@' ?",
	    text = "Local names with an embedded '@', such as in foo@bar "
		   "might result in your mail being sent to foo@bar.com "
		   "instead of your local system.\n Are you sure you want "
		   "a local user name with an '@' in it?",
	    bitmap = 'question',
	    strings = ('Yes', 'No'),
	    default = 1).num == 0:
		self.fetch(User, 'user')
		self.destruct()

    def makeWidgets(self, mode, servername):
	dispose_window(self,
			"User options for " + self.user.remote + " querying " + servername,
			userhelp)

	if mode != 'novice':
	    leftwin = Frame(self);
	else:
	    leftwin = self

	secwin = Frame(leftwin, relief=RAISED, bd=5)
	Label(secwin, text="Authentication").pack(side=TOP)
	LabeledEntry(secwin, 'Password:',
		      self.password, '12').pack(side=TOP, fill=X)
	secwin.pack(fill=X, anchor=N)

	if 'ssl' in feature_options or 'ssl' in dictmembers:
	    sslwin = Frame(leftwin, relief=RAISED, bd=5)
	    Checkbutton(sslwin, text="Use SSL?",
			variable=self.ssl).pack(side=TOP, fill=X)
	    LabeledEntry(sslwin, 'SSL key:',
			 self.sslkey, '14').pack(side=TOP, fill=X)
	    LabeledEntry(sslwin, 'SSL certificate:',
			 self.sslcert, '14').pack(side=TOP, fill=X)
	    Checkbutton(sslwin, text="Check server SSL certificate?",
			variable=self.sslcertck).pack(side=TOP, fill=X)
	    LabeledEntry(sslwin, 'SSL trusted certificate directory:',
			 self.sslcertpath, '14').pack(side=TOP, fill=X)
	    LabeledEntry(sslwin, 'SSL CommonName:',
			 self.sslcommonname, '14').pack(side=TOP, fill=X)
	    LabeledEntry(sslwin, 'SSL key fingerprint:',
			 self.sslfingerprint, '14').pack(side=TOP, fill=X)
	    sslwin.pack(fill=X, anchor=N)

	names = Frame(leftwin, relief=RAISED, bd=5)
	Label(names, text="Local names").pack(side=TOP)
	ListEdit("New name: ",
		     self.user.localnames, None, None, names, localhelp)
	names.pack(fill=X, anchor=N)

	if mode != 'novice':
	    targwin = Frame(leftwin, relief=RAISED, bd=5)
	    Label(targwin, text="Forwarding Options").pack(side=TOP)
	    Label(targwin, text="Listeners to forward to").pack(side=TOP)
	    ListEdit("New listener:",
		     self.user.smtphunt, None, None, targwin, None)
	    Label(targwin, text="Domains to fetch from (ODMR/ETRN only)").pack(side=TOP)
	    ListEdit("Domains:",
		     self.user.fetchdomains, None, None, targwin, None)
	    LabeledEntry(targwin, 'Append to MAIL FROM line:',
		     self.smtpaddress, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'Set RCPT To address:',
		     self.smtpname, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'Connection setup command:',
		     self.preconnect, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'Connection wrapup command:',
		     self.postconnect, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'Local delivery agent:',
		     self.mda, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'BSMTP output file:',
		     self.bsmtp, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'Listener spam-block codes:',
		     self.antispam, '26').pack(side=TOP, fill=X)
	    LabeledEntry(targwin, 'Pass-through properties:',
		     self.properties, '26').pack(side=TOP, fill=X)
	    Checkbutton(targwin, text="Use LMTP?",
			variable=self.lmtp).pack(side=TOP, fill=X)
	    targwin.pack(fill=X, anchor=N)

	if mode != 'novice':
	    leftwin.pack(side=LEFT, fill=X, anchor=N)
	    rightwin = Frame(self)
	else:
	    rightwin = self

	optwin = Frame(rightwin, relief=RAISED, bd=5)
	Label(optwin, text="Processing Options").pack(side=TOP)
	Checkbutton(optwin, text="Suppress deletion of messages after reading",
		    variable=self.keep).pack(side=TOP, anchor=W)
	Checkbutton(optwin, text="Fetch old messages as well as new",
		    variable=self.fetchall).pack(side=TOP, anchor=W)
	if mode != 'novice':
	    Checkbutton(optwin, text="Flush seen messages before retrieval",
		    variable=self.flush).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Flush oversized messages before retrieval",
		    variable=self.limitflush).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Rewrite To/Cc/Bcc messages to enable reply",
		    variable=self.rewrite).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Force CR/LF at end of each line",
		    variable=self.forcecr).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Strip CR from end of each line",
		    variable=self.stripcr).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Pass 8 bits even though SMTP says 7BIT",
		    variable=self.pass8bits).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Undo MIME armoring on header and body",
		    variable=self.mimedecode).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Drop Status lines from forwarded messages",
		    variable=self.dropstatus).pack(side=TOP, anchor=W)
	    Checkbutton(optwin, text="Drop Delivered-To lines from forwarded messages",
		    variable=self.dropdelivered).pack(side=TOP, anchor=W)
	optwin.pack(fill=X)

	if mode != 'novice':
	    limwin = Frame(rightwin, relief=RAISED, bd=5)
	    Label(limwin, text="Resource Limits").pack(side=TOP)
	    LabeledEntry(limwin, 'Message size limit:',
		      self.limit, '30').pack(side=TOP, fill=X)
	    LabeledEntry(limwin, 'Size warning interval:',
		      self.warnings, '30').pack(side=TOP, fill=X)
	    LabeledEntry(limwin, 'Max messages to fetch per poll:',
		      self.fetchlimit, '30').pack(side=TOP, fill=X)
	    LabeledEntry(limwin, 'Max message sizes to fetch per transaction:',
		      self.fetchsizelimit, '30').pack(side=TOP, fill=X)
	    if self.parent.server.protocol not in ('ETRN', 'ODMR'):
		LabeledEntry(limwin, 'Use fast UIDL:',
			self.fastuidl, '30').pack(side=TOP, fill=X)
	    LabeledEntry(limwin, 'Max messages to forward per poll:',
		      self.batchlimit, '30').pack(side=TOP, fill=X)
	    if self.parent.server.protocol not in ('ETRN', 'ODMR'):
		LabeledEntry(limwin, 'Interval between expunges:',
			     self.expunge, '30').pack(side=TOP, fill=X)
	    Checkbutton(limwin, text="Idle after each poll (IMAP only)",
		    variable=self.idle).pack(side=TOP, anchor=W)
	    limwin.pack(fill=X)

	    if self.parent.server.protocol == 'IMAP':
		foldwin = Frame(rightwin, relief=RAISED, bd=5)
		Label(foldwin, text="Remote folders (IMAP only)").pack(side=TOP)
		ListEdit("New folder:", self.user.mailboxes,
			 None, None, foldwin, None)
		foldwin.pack(fill=X, anchor=N)

	if mode != 'novice':
	    rightwin.pack(side=LEFT)
	else:
	    self.pack()


#
# Top-level window that offers either novice or expert mode
# (but not both at once; it disappears when one is selected).
#

class Configurator(Frame):
    def __init__(self, outfile, master, onexit, parent):
	Frame.__init__(self, master)
	self.outfile = outfile
	self.onexit = onexit
	self.parent = parent
	self.master.title('fetchmail configurator');
	self.master.iconname('fetchmail configurator');
	Pack.config(self)
	self.keepalive = []	# Use this to anchor the PhotoImage object
	make_icon_window(self, fetchmail_icon)

	Message(self, text="""
Use `Novice Configuration' for basic fetchmail setup;
with this, you can easily set up a single-drop connection
to one remote mail server.
""", width=600).pack(side=TOP)
	Button(self, text='Novice Configuration',
				fg='blue', command=self.novice).pack()

	Message(self, text="""
Use `Expert Configuration' for advanced fetchmail setup,
including multiple-site or multidrop connections.
""", width=600).pack(side=TOP)
	Button(self, text='Expert Configuration',
				fg='blue', command=self.expert).pack()

	Message(self, text="""
Or you can just select `Quit' to leave the configurator now and
return to the main panel.
""", width=600).pack(side=TOP)
	Button(self, text='Quit', fg='blue', command=self.leave).pack()
	master.protocol("WM_DELETE_WINDOW", self.leave)

    def novice(self):
	self.master.destroy()
	ConfigurationEdit(Fetchmailrc, self.outfile, Toplevel(), self.onexit).edit('novice')

    def expert(self):
	self.master.destroy()
	ConfigurationEdit(Fetchmailrc, self.outfile, Toplevel(), self.onexit).edit('expert')

    def leave(self):
	self.master.destroy()
	self.onexit()

# Run a command in a scrolling text widget, displaying its output

class RunWindow(Frame):
    def __init__(self, command, master, parent):
	Frame.__init__(self, master)
	self.master = master
	self.master.title('fetchmail run window');
	self.master.iconname('fetchmail run window');
	Pack.config(self)
	Label(self,
		text="Running "+command,
		bd=2).pack(side=TOP, pady=10)
	self.keepalive = []	# Use this to anchor the PhotoImage object
	make_icon_window(self, fetchmail_icon)

	# This is a scrolling text window
	textframe = Frame(self)
	scroll = Scrollbar(textframe)
	self.textwidget = Text(textframe, setgrid=TRUE)
	textframe.pack(side=TOP, expand=YES, fill=BOTH)
	self.textwidget.config(yscrollcommand=scroll.set)
	self.textwidget.pack(side=LEFT, expand=YES, fill=BOTH)
	scroll.config(command=self.textwidget.yview)
	scroll.pack(side=RIGHT, fill=BOTH)
	textframe.pack(side=TOP)

	Button(self, text='Quit', fg='blue', command=self.leave).pack()

	self.update()	# Draw widget before executing fetchmail

	# Always look for a runnable command in the directory we're running in
	# first. This avoids some obscure version-skew errors that can occur
	# if you pick up an old fetchmail from the standard system locations.
	os.environ["PATH"] = os.path.dirname(sys.argv[0]) + ":" + os.environ["PATH"]
	child_stdout = os.popen(command + " 2>&1 </dev/null", "r")
	while 1:
	    ch = child_stdout.read(1)
	    if not ch:
		break
	    self.textwidget.insert(END, ch)
	self.textwidget.insert(END, "Done.")
	self.textwidget.see(END);

    def leave(self):
	self.master.destroy()

# Here's where we choose either configuration or launching

class MainWindow(Frame):
    def __init__(self, outfile, master=None):
	Frame.__init__(self, master)
	self.outfile = outfile
	self.master.title('fetchmail launcher');
	self.master.iconname('fetchmail launcher');
	Pack.config(self)
	Label(self,
		text='Fetchmailconf ' + version,
		bd=2).pack(side=TOP, pady=10)
	self.keepalive = []	# Use this to anchor the PhotoImage object
	make_icon_window(self, fetchmail_icon)
	self.debug = 0

	## Test icon display with the following:
	# icon_image = PhotoImage(data=fetchmail_icon)
	# Label(self, image=icon_image).pack(side=TOP, pady=10)
	# self.keepalive.append(icon_image)

	Message(self, text="""
Use `Configure fetchmail' to tell fetchmail about the remote
servers it should poll (the host name, your username there,
whether to use POP or IMAP, and so forth).
""", width=600).pack(side=TOP)
	self.configbutton = Button(self, text='Configure fetchmail',
				fg='blue', command=self.configure)
	self.configbutton.pack()

	Message(self, text="""
Use `Run fetchmail' to run fetchmail with debugging enabled.
This is a good way to test out a new configuration.
""", width=600).pack(side=TOP)
	Button(self, text='Run fetchmail',fg='blue', command=self.test).pack()

	Message(self, text="""
Use `Run fetchmail' to run fetchmail in foreground.
Progress  messages will be shown, but not debug messages.
""", width=600).pack(side=TOP)
	Button(self, text='Run fetchmail', fg='blue', command=self.run).pack()

	Message(self, text="""
Or you can just select `Quit' to exit the launcher now.
""", width=600).pack(side=TOP)
	Button(self, text='Quit', fg='blue', command=self.leave).pack()

    def configure(self):
	self.configbutton.configure(state=DISABLED)
	Configurator(self.outfile, Toplevel(),
		     lambda self=self: self.configbutton.configure(state=NORMAL),
		     self)
    def test(self):
	cmd = "fetchmail -N -d0 --nosyslog -v"
	if rcfile:
	    cmd = cmd + " -f " + rcfile
	RunWindow(cmd, Toplevel(), self)

    def run(self):
	cmd = "fetchmail -N -d0"
	if rcfile:
	    cmd = cmd + " -f " + rcfile
	RunWindow(cmd, Toplevel(), self)

    def leave(self):
	self.quit()

# Functions for turning a dictionary into an instantiated object tree.

def intersect(list1, list2):
# Compute set intersection of lists
    res = []
    for x in list1:
	if x in list2:
	    res.append(x)
    return res

def setdiff(list1, list2):
# Compute set difference of lists
    res = []
    for x in list1:
	if not x in list2:
	    res.append(x)
    return res

def copy_instance(toclass, fromdict):
# Initialize a class object of given type from a conformant dictionary.
    for fld in fromdict.keys():
	if not fld in dictmembers:
	    dictmembers.append(fld)
# The `optional' fields are the ones we can ignore for purposes of
# conformability checking; they'll still get copied if they are
# present in the dictionary.
    optional = ('interface', 'monitor',
		'esmtpname', 'esmtppassword',
		'ssl', 'sslkey', 'sslcert', 'sslproto', 'sslcertck',
		'sslcertpath', 'sslcommonname', 'sslfingerprint', 'showdots')
    class_sig = setdiff(toclass.__dict__.keys(), optional)
    class_sig.sort()
    dict_keys = setdiff(fromdict.keys(), optional)
    dict_keys.sort()
    common = intersect(class_sig, dict_keys)
    if 'typemap' in class_sig:
	class_sig.remove('typemap')
    if tuple(class_sig) != tuple(dict_keys):
	print "Fields don't match what fetchmailconf expected:"
#	print "Class signature: " + `class_sig`
#	print "Dictionary keys: " + `dict_keys`
	diff = setdiff(class_sig, common)
	if diff:
	    print "Not matched in class `" + toclass.__class__.__name__ + "' signature: " + `diff`
	diff = setdiff(dict_keys, common)
	if diff:
	    print "Not matched in dictionary keys: " + `diff`
	sys.exit(1)
    else:
	for x in fromdict.keys():
	    setattr(toclass, x, fromdict[x])

#
# And this is the main sequence.  How it works:
#
# First, call `fetchmail --configdump' and trap the output in a tempfile.
# This should fill it with a Python initializer for a variable `fetchmailrc'.
# Run execfile on the file to pull fetchmailrc into Python global space.
# You don't want static data, though; you want, instead, a tree of objects
# with the same data members and added appropriate methods.
#
# This is what the copy_instance function() is for.  It tries to copy a
# dictionary field by field into a class, aborting if the class and dictionary
# have different data members (except for any typemap member in the class;
# that one is strictly for use by the MyWidget supperclass).
#
# Once the object tree is set up, require user to choose novice or expert
# mode and instantiate an edit object for the configuration.  Class methods
# will take it all from there.
#
# Options (not documented because they're for fetchmailconf debuggers only):
# -d: Read the configuration and dump it to stdout before editing.  Dump
#     the edited result to stdout as well.
# -f: specify the run control file to read.

if __name__ == '__main__':

    if not os.environ.has_key("DISPLAY"):
	print "fetchmailconf must be run under X"
	sys.exit(1)

    fetchmail_icon = """
R0lGODdhPAAoAPcAAP///wgICBAQEISEhIyMjJSUlKWlpa2trbW1tcbGxs7Ozufn5+/v7//39yEY
GNa9tUoxKZyEe1o5KTEQAN7OxpyMhIRjUvfn3pxSKYQ5EO/Wxv/WvWtSQrVzSmtCKWspAMatnP/e
xu+1jIxSKaV7Wt6ca5xSGK2EY8aUa72MY86UY617UsaMWrV7SpRjOaVrOZRaKYxSIXNCGGs5EIRC
CJR7Y/+UMdbOxnNrY97Ove/Wvd7GrZyEa961jL2Ua9alc86ca7WEUntSKcaMSqVjGNZ7GGM5CNa1
jPfOnN6tc3taMffeve/WtWtaQv/OjGtSMYRzWv/erda1hM6te7WUY62MWs61jP/vzv/ntda9jL2l
czEhAO/n1oyEc//elDEpGEo5EOfexpyUe+/epefevffvxnNrQpyUStbWzsbGvZyclN7ezmNjWv//
5/f33qWllNbWve/vzv//1ufnve/vvf//xvf3vefnrf//taWlc0pKMf//pbW1Y///jKWlWq2tWsbG
Y///c97eUvf3Ut7nc+/3a87We8bOjOfv1u/37/f//621tb3Gxtbn52Nra87n53uUlJTv/6W9xuf3
/8bW3iExOXu11tbv/5TW/4TO/63e/zmt/1KUxlK1/2u9/wCM/73GzrXG1gBKjACE/87e72NzhCkx
OaXO92OMtUql/xCE/wApUtbe57W9xnN7hHut52Ot/xBSnABKnABavQB7/2ul7zF71gBr77XO73Oc
1lqc9yFSlBApSimE/wAYOQApY0J7zlKM5wAxhABS1gBj/6W95wAhWgA5nAAYSgBS7wBS/wBK9wAp
jABC5wBK/wApnABC/wApxgAhtYSMtQAQYwAp/3OE74SMxgAYxlpjvWNr70pS/wgQ3sbGzs7O1qWl
3qWl70pKe0JC/yEhlCkp/wgI/wAAEAAAIQAAKQAAOQAASgAAUgAAYwAAawAAlAAAnAAApQAArQAA
zgAA1gAA5wAA9wAA/0pC/xgQ52Na9ykhe4R7zikhYxgQSjEpQgAAACwAAAAAPAAoAAAI/wABCBxI
sKDBgwgTKiRIYKHDhxARIvgXsaLFhGgEUBSYoKPHjyBDihxJkuS/kwNLqlzJcuTJjQBaypxpEiVH
mjhxvkyZs2fLnTd9ehxAtKjRo0ZrwhTasUsENhYHKOUpk1E3j11mxCBiQVLEBlJd2owp9iVRjwUs
zMCQ5IcLD4saPVxjIKxIoGTvvqSoyFEFGTBeqEhyxAoSFR/USGKVcEGBAwDshsSr1OYTEyhQpJiS
ZcoUKWOQtJDRJFSaggzUGBgoGSTlsjahlPCRIkWVKT16THHRIoqIISBIEUgAYIGBhgRbf3ytFygU
FZp9UDmxQkkMCRwyZKDBQy4aApABhP8XqNwj88l7BVpQYZtF5iArWgwAgGZBq24HU7OeGhQ90PVA
aKZZCiiUMJ9ArSTEwGqR8ZeXfzbV0MIIMQTBwoUdxDDfAm8sZFyDZVEF4UYSKBEBD0+k6IEFPMxH
3FzldXSea+kBgANJSOWIlIMhXZXAXv+c1WM3PuJEpH8iuhbAkv+MdENPRHaTRkdF/jiWSKCAwlKW
VbbkY5Q0LgUSKExgoYBKCjCxARpdltQNKHaUoYAddnR53lVRnJLKBWh4RIEGCZx5FSOv1OLNDUVe
deZHaWiZAB35fIOGNtbEUeV5oGAByzPOrBPFGt3kwEgxITACSg5oLGGLMg60oQAjaNz/oAAcN4Ai
a0c3kHFDK3jYsw4g9sRzBgPLXdkRrBrQ8gsWQUxCCRZX9IJNBQ1s8IgCdeBCzBYN6IBIN2TUsQYd
dXhDBxdzlAHOHHKEcocZdWwDjx8MTCmjsR2FMAstw1RyiSzHqPLALaOwk8QmzCzDCSi0xJKMMk4E
Yw8389iTDT32GAKOPf7YY0Aa9tATyD3w/EGsefgmgEYUtPiChLKWQDMBJtEUgYkzH2RiTgGfTMCI
Mlu0Yc85hNiDziH2tMqOGL72QY47gshLb7Fi4roELcjoQIsxWpDwQyfS2OCJMkLI4YUmyhgxSTVg
CP2FHPZ80UDcieBjStNPD5LPOyZT/y0iHGiMwswexDSzRiRq6KIMJBc4M8skwKAyChia2KPH3P24
YU8/lFhOTj152OPOHuXMU4g48vCRiN/9rZGLMdS4csUu1JzDgxuipOMDHMKsAwEnq/ByzTrrZMNO
OtO0k84+7KjzBjzplMJOOOOoo8846/ATxqJWinkkGUyEkMAaIezABQM3bMAEK1xEsUMDGjARRxhY
xEGGHfPjEcccca6BRxhyuEMY7FCHMNDhf9140r2qRiVvdENQ3liUArzREW/0qRsRVIAGFfBADnLw
gUSiYASJpMEHhilJTEnhAlGoQqYAZQ1AiqEMZ0jDGtqQImhwwA13yMMevoQAGvGhEAWHGMOAAAA7
"""
# The base64 data in the string above was generated by the following procedure:
#
# import base64
# print base64.encodestring(open("fetchmail.gif", "rb").read())
#

    # Process options
    (options, arguments) = getopt.getopt(sys.argv[1:], "df:hV", ["help",
	    "version"])
    dump = rcfile = None;
    for (switch, val) in options:
	if (switch == '-d'):
	    dump = TRUE
	elif (switch == '-f'):
	    rcfile = val
	elif (switch == '-h' or switch == '--help'):
	    print """
Usage: fetchmailconf {[-d] [-f fetchmailrc]|-h|--help|-V|--version}
           -d      - dump configuration (for debugging)
           -f fmrc - read alternate fetchmailrc file
--help,    -h      - print this help text and quit
--version, -V      - print fetchmailconf version and quit
"""
	    sys.exit(0)
	elif (switch == '-V' or switch == '--version'):
	    print "fetchmailconf %s" % version
	    print """
Copyright (C) 1997 - 2003 Eric S. Raymond
Copyright (C) 2005, 2006, 2008, 2009 Matthias Andree
fetchmailconf comes with ABSOLUTELY NO WARRANTY.  This is free software, you are
welcome to redistribute it under certain conditions.  Please see the file
COPYING in the source or documentation directory for details."""
	    sys.exit(0)

    # Get client host's FQDN
    hostname = socket.gethostbyaddr(socket.gethostname())[0]

    # Compute defaults
    ConfigurationDefaults = Configuration()
    ServerDefaults = Server()
    UserDefaults = User()

    # Read the existing configuration.  We set the umask to 077 to make sure
    # that group & other read/write permissions are shut off -- we wouldn't
    # want crackers to snoop password information out of the tempfile.
    tmpfile = tempfile.mktemp()
    if rcfile:
	cmd = "umask 077 && fetchmail </dev/null -f " + rcfile + " --configdump --nosyslog >" + tmpfile
    else:
	cmd = "umask 077 && fetchmail </dev/null --configdump --nosyslog >" + tmpfile

    try:
	s = os.system(cmd)
	if s != 0:
	    print "`" + cmd + "' run failure, status " + `s`
	    raise SystemExit
    except:
	print "Unknown error while running fetchmail --configdump"
	os.remove(tmpfile)
	sys.exit(1)

    try:
	execfile(tmpfile)
    except:
	print "Can't read configuration output of fetchmail --configdump."
	os.remove(tmpfile)
	sys.exit(1)

    os.remove(tmpfile)

    # The tricky part -- initializing objects from the configuration global
    # `Configuration' is the top level of the object tree we're going to mung.
    # The dictmembers list is used to track the set of fields the dictionary
    # contains; in particular, we can use it to tell whether things like the
    # monitor, interface, ssl, sslkey, or sslcert fields are present.
    dictmembers = []
    Fetchmailrc = Configuration()
    copy_instance(Fetchmailrc, fetchmailrc)
    Fetchmailrc.servers = [];
    for server in fetchmailrc['servers']:
	Newsite = Server()
	copy_instance(Newsite, server)
	Fetchmailrc.servers.append(Newsite)
	Newsite.users = [];
	for user in server['users']:
	    Newuser = User()
	    copy_instance(Newuser, user)
	    Newsite.users.append(Newuser)

    # We may want to display the configuration and quit
    if dump:
	print "This is a dump of the configuration we read:\n"+`Fetchmailrc`

    # The theory here is that -f alone sets the rcfile location,
    # but -d and -f together mean the new configuration should go to stdout.
    if not rcfile and not dump:
	rcfile = os.environ["HOME"] + "/.fetchmailrc"

    # OK, now run the configuration edit
    root = MainWindow(rcfile)
    root.mainloop()

# The following sets edit modes for GNU EMACS
# Local Variables:
# mode:python
# End:
